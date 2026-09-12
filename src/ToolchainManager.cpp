#include "ToolchainManager.h"
#include "Settings.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QIODevice>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

#include <archive.h>
#include <archive_entry.h>

namespace {
const QUrl kLatest("https://api.github.com/repos/pumpumlang/punpun/releases/latest");

QString normalizedVersion(QString value) {
    value.remove(QRegularExpression("^[vV]"));
    return value.trimmed();
}
}

ToolchainManager::ToolchainManager(IdeSettings *settings, QObject *parent)
    : QObject(parent), settings_(settings) {
    connect(&timer_, &QTimer::timeout, this, [this] { checkForUpdates(false); });
}

QString ToolchainManager::appDataDir() const {
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(path);
    return path;
}

QString ToolchainManager::privateBinDir() const {
    QFile marker(appDataDir() + "/toolchain/bin-path.txt");
    if (marker.open(QIODevice::ReadOnly)) {
        const QString path = QString::fromUtf8(marker.readAll()).trimmed();
        if (QDir(path).exists()) return path;
    }
    return appDataDir() + "/toolchain/current/bin";
}

void ToolchainManager::setProjectRoot(const QString &path) {
    projectRoot_ = path;
}

QStringList ToolchainManager::searchPaths() const {
    // PunPun's installer puts `ppc` in ~/.local/bin and makes it reachable by
    // appending to the *shell* rc files. A desktop launcher, a .desktop file
    // and an AppImage never read those, so PATH alone finds nothing and Run
    // appears to do nothing at all. Look where the installer actually writes.
    QStringList paths;
    auto add = [&paths](const QString &path) {
        if (path.isEmpty()) return;
        const QString absolute = QDir(path).absolutePath();
        if (!paths.contains(absolute) && QDir(absolute).exists()) paths << absolute;
    };

    if (settings_) add(settings_->toolchainDir());
    add(privateBinDir());

    const auto env = QProcessEnvironment::systemEnvironment();
    if (env.contains("PUNPUN_PREFIX")) add(env.value("PUNPUN_PREFIX") + "/bin");
    if (env.contains("PUNPUN_HOME")) add(env.value("PUNPUN_HOME") + "/bin");

    const QString home = QDir::homePath();
    add(home + "/.local/bin");            // install.sh default
    add(home + "/.punpun/bin");
    add(home + "/bin");
#ifdef Q_OS_WIN
    add(env.value("LOCALAPPDATA") + "/Programs/PunPun/bin");
    add(env.value("ProgramFiles") + "/PunPun/bin");
#else
    add("/usr/local/bin");
    add("/opt/punpun/bin");
    add("/usr/bin");
#endif

    // A source checkout builds the compiler in place; running from one should
    // use the compiler that checkout just built, not an older installed copy.
    if (!projectRoot_.isEmpty()) {
        add(projectRoot_ + "/build");
        add(projectRoot_);
    }
    return paths;
}

QString ToolchainManager::findExecutable(const QStringList &names) const {
    for (const auto &directory : searchPaths()) {
        for (const auto &name : names) {
            const QFileInfo info(directory + "/" + name);
            if (info.exists() && info.isFile() && info.isExecutable())
                return info.absoluteFilePath();
        }
    }
    for (const auto &name : names) {
        const QString path = QStandardPaths::findExecutable(name);
        if (!path.isEmpty()) return path;
    }
    return {};
}

QString ToolchainManager::discoveryReport() const {
    QString text = "Looked for ppc in:\n";
    for (const auto &directory : searchPaths()) text += "  " + QDir::toNativeSeparators(directory) + "\n";
    text += "  and every directory on PATH\n";
    return text;
}

QString ToolchainManager::ppcPath() const { return findExecutable({"ppc", "ppc.exe"}); }
QString ToolchainManager::ppPath() const { return findExecutable({"pp", "punpun", "pp.exe", "punpun.exe"}); }
QString ToolchainManager::ppxPath() const { return findExecutable({"ppx", "ppx.exe"}); }

QString ToolchainManager::versionFromText(const QString &text) {
    const auto match = QRegularExpression(
        R"((?:^|\s|v)(\d+\.\d+(?:\.\d+)?(?:[-+][A-Za-z0-9.-]+)?))").match(text);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString ToolchainManager::installedVersion() const {
    const QString executable = ppcPath();
    if (executable.isEmpty()) return {};
    QProcess process;
    process.start(executable, {"--version"});
    if (!process.waitForFinished(1500)) return {};
    return versionFromText(QString::fromLocal8Bit(
        process.readAllStandardOutput() + process.readAllStandardError()));
}

QString ToolchainManager::installedTag() const {
    QFile file(appDataDir() + "/toolchain/release-tag.txt");
    if (file.open(QIODevice::ReadOnly)) return QString::fromUtf8(file.readAll()).trimmed();
    return installedVersion();
}

int ToolchainManager::compareVersions(const QString &left, const QString &right) {
    const auto a = normalizedVersion(left)
                       .split(QRegularExpression("[-+]"), Qt::SkipEmptyParts)
                       .value(0)
                       .split('.');
    const auto b = normalizedVersion(right)
                       .split(QRegularExpression("[-+]"), Qt::SkipEmptyParts)
                       .value(0)
                       .split('.');
    for (int i = 0; i < 3; ++i) {
        const int av = a.value(i).toInt();
        const int bv = b.value(i).toInt();
        if (av < bv) return -1;
        if (av > bv) return 1;
    }
    return 0;
}

void ToolchainManager::startAutomaticChecks() {
    const int minutes = qMax(5, settings_->updateIntervalMinutes());
    timer_.setInterval(minutes * 60 * 1000);
    timer_.start();
    QTimer::singleShot(250, this, [this] { checkForUpdates(false); });
}

void ToolchainManager::checkForUpdates(bool userInitiated) {
    if (checking_ || downloading_) return;
    checking_ = true;
    Q_EMIT status("Checking latest stable PunPun release…");

    QNetworkRequest request{QUrl(kLatest)};
    request.setRawHeader("User-Agent", QByteArray("PunPun-IDE/") + PPIDE_VERSION);
    request.setRawHeader("Accept", "application/vnd.github+json");
    auto *reply = network_.get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, userInitiated] { handleRelease(reply, userInitiated); });
}

ReleaseAssetInfo ToolchainManager::chooseAsset(const QJsonObject &release) const {
    const QJsonArray assets = release.value("assets").toArray();
    QStringList preferred;
#ifdef Q_OS_WIN
    preferred = {"-win-x64.msi", "-windows-x64.msi"};
#elif defined(Q_OS_LINUX)
    preferred = {"-linux-x86_64-SDK.zip", "-linux-x86_64.tar.zst"};
#endif

    for (const auto &suffix : preferred) {
        for (const auto &value : assets) {
            const QJsonObject object = value.toObject();
            const QString name = object.value("name").toString();
            if (!name.endsWith(suffix, Qt::CaseInsensitive)) continue;
            QString digest = object.value("digest").toString();
            if (digest.startsWith("sha256:")) digest = digest.mid(7);
            QString kind = "archive";
            if (name.endsWith(".msi", Qt::CaseInsensitive)) kind = "msi";
            return {name,
                    object.value("browser_download_url").toString(),
                    digest,
                    kind,
                    qint64(object.value("size").toDouble())};
        }
    }
    return {};
}

void ToolchainManager::handleRelease(QNetworkReply *reply, bool userInitiated) {
    checking_ = false;
    const QByteArray payload = reply->readAll();
    const auto networkError = reply->error();
    const QString networkMessage = reply->errorString();
    reply->deleteLater();

    if (networkError != QNetworkReply::NoError) {
        // A background poll that cannot reach the network is ordinary: an
        // offline or proxied machine must not be greeted by a red error on
        // every launch. Only a check the user actually asked for reports.
        if (userInitiated) Q_EMIT error("PunPun update check failed: " + networkMessage);
        else Q_EMIT status("Update check skipped; the release feed is unreachable.");
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        if (userInitiated) Q_EMIT error("GitHub returned invalid release metadata");
        else Q_EMIT status("Update check skipped; the release feed returned no metadata.");
        return;
    }

    latestRelease_ = document.object();
    latestTag_ = latestRelease_.value("tag_name").toString();
    latestAsset_ = chooseAsset(latestRelease_);
    if (latestTag_.isEmpty()) {
        if (userInitiated) Q_EMIT error("Latest PunPun release has no version tag");
        return;
    }

    const QString installed = installedVersion();
    const bool needsUpdate = installed.isEmpty() ||
                             compareVersions(installed, latestTag_) < 0 ||
                             normalizedVersion(installedTag()) != normalizedVersion(latestTag_);
    if (!needsUpdate) {
        Q_EMIT status("PunPun " + installed + " is current");
        if (userInitiated)
            Q_EMIT status("Already on the latest stable PunPun release (" + latestTag_ + ")");
        return;
    }

    Q_EMIT updateAvailable(installed.isEmpty() ? "not installed" : installed, latestTag_);
    Q_EMIT status("PunPun " + latestTag_ + " is available");
    if (!settings_->autoUpdatePunPun()) return;
    if (!latestAsset_.valid()) {
        Q_EMIT error("No private-install asset exists for this platform");
        return;
    }
    installLatest();
}

void ToolchainManager::installLatest() {
    if (downloading_ || !latestAsset_.valid()) return;
    downloading_ = true;
    downloadAsset(latestAsset_);
}

void ToolchainManager::downloadAsset(const ReleaseAssetInfo &asset) {
    const QString downloads = appDataDir() + "/downloads";
    QDir().mkpath(downloads);
    const QString target = downloads + "/" + asset.name;
    auto *file = new QSaveFile(target, this);
    if (!file->open(QIODevice::WriteOnly)) {
        downloading_ = false;
        Q_EMIT error("Cannot create toolchain download file");
        file->deleteLater();
        return;
    }

    QNetworkRequest request{QUrl(asset.url)};
    request.setRawHeader("User-Agent", QByteArray("PunPun-IDE/") + PPIDE_VERSION);
    auto *reply = network_.get(request);
    connect(reply, &QNetworkReply::downloadProgress, this, &ToolchainManager::progress);
    connect(reply, &QNetworkReply::readyRead, this,
            [reply, file] { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, file, target, asset] {
                file->write(reply->readAll());
                const auto networkError = reply->error();
                const QString networkMessage = reply->errorString();
                reply->deleteLater();

                if (networkError != QNetworkReply::NoError) {
                    file->cancelWriting();
                    file->deleteLater();
                    downloading_ = false;
                    Q_EMIT error("Download failed: " + networkMessage);
                    return;
                }
                if (!file->commit()) {
                    file->deleteLater();
                    downloading_ = false;
                    Q_EMIT error("Could not commit downloaded toolchain");
                    return;
                }
                file->deleteLater();

                if (!asset.sha256.isEmpty()) {
                    QFile downloaded(target);
                    if (!downloaded.open(QIODevice::ReadOnly)) {
                        downloading_ = false;
                        Q_EMIT error("Could not verify download");
                        return;
                    }
                    QCryptographicHash hash(QCryptographicHash::Sha256);
                    while (!downloaded.atEnd()) hash.addData(downloaded.read(1024 * 1024));
                    if (QString::fromLatin1(hash.result().toHex())
                            .compare(asset.sha256, Qt::CaseInsensitive) != 0) {
                        QFile::remove(target);
                        downloading_ = false;
                        Q_EMIT error("PunPun download failed SHA-256 verification");
                        return;
                    }
                }

                QString installError;
                const bool ok = asset.kind == "msi"
                    ? installMsi(target, &installError)
                    : installArchive(target, asset, &installError);
                downloading_ = false;
                if (!ok) {
                    Q_EMIT error(installError);
                    return;
                }
                Q_EMIT installed(latestTag_);
                Q_EMIT status("PunPun " + latestTag_ + " installed and verified");
            });
}

bool ToolchainManager::extractWithLibarchive(const QString &archivePath,
                                             const QString &destination,
                                             QString *error) {
    archive *input = archive_read_new();
    archive_read_support_filter_all(input);
    archive_read_support_format_all(input);
    if (archive_read_open_filename(input, archivePath.toUtf8().constData(), 10240) != ARCHIVE_OK) {
        *error = QString::fromLocal8Bit(archive_error_string(input));
        archive_read_free(input);
        return false;
    }

    archive *disk = archive_write_disk_new();
    archive_write_disk_set_options(
        disk, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM |
                  ARCHIVE_EXTRACT_SECURE_NODOTDOT | ARCHIVE_EXTRACT_SECURE_SYMLINKS);
    archive_entry *entry = nullptr;
    const QString base = QDir(destination).absolutePath();

    while (archive_read_next_header(input, &entry) == ARCHIVE_OK) {
        const QString relative = QString::fromUtf8(archive_entry_pathname(entry));
        const QString clean = QDir::cleanPath(QDir(base).absoluteFilePath(relative));
        if (!clean.startsWith(base + QDir::separator()) && clean != base) {
            *error = "Unsafe path in PunPun archive";
            archive_write_free(disk);
            archive_read_free(input);
            return false;
        }

        const QByteArray encoded = clean.toUtf8();
        archive_entry_set_pathname(entry, encoded.constData());
        int result = archive_write_header(disk, entry);
        if (result < ARCHIVE_WARN) {
            *error = QString::fromLocal8Bit(archive_error_string(disk));
            archive_write_free(disk);
            archive_read_free(input);
            return false;
        }

        const void *buffer = nullptr;
        size_t size = 0;
        la_int64_t offset = 0;
        while (true) {
            result = archive_read_data_block(input, &buffer, &size, &offset);
            if (result == ARCHIVE_EOF) break;
            if (result < ARCHIVE_OK) {
                *error = QString::fromLocal8Bit(archive_error_string(input));
                archive_write_free(disk);
                archive_read_free(input);
                return false;
            }
            if (archive_write_data_block(disk, buffer, size, offset) < ARCHIVE_OK) {
                *error = QString::fromLocal8Bit(archive_error_string(disk));
                archive_write_free(disk);
                archive_read_free(input);
                return false;
            }
        }
        archive_write_finish_entry(disk);
    }

    archive_write_free(disk);
    archive_read_free(input);
    return true;
}

bool ToolchainManager::smokeTest(const QString &root, QString *error) const {
    QDirIterator iterator(root, {"ppc", "ppc.exe"}, QDir::Files, QDirIterator::Subdirectories);
    if (!iterator.hasNext()) {
        *error = "Downloaded PunPun SDK does not contain ppc";
        return false;
    }
    const QString ppc = iterator.next();
    QProcess process;
    process.start(ppc, {"--version"});
    if (!process.waitForFinished(4000) || process.exitCode() != 0) {
        *error = "Downloaded PPC failed its version smoke test";
        return false;
    }
    const QString version = versionFromText(QString::fromLocal8Bit(
        process.readAllStandardOutput() + process.readAllStandardError()));
    if (version.isEmpty()) {
        *error = "Downloaded PPC returned an unrecognized version";
        return false;
    }
    return true;
}

void ToolchainManager::finishStagedInstall(const QString &staging, const QString &tag) {
    const QString toolchain = appDataDir() + "/toolchain";
    const QString current = toolchain + "/current";
    if (QDir(current).exists()) QDir(current).removeRecursively();
    QDir().mkpath(toolchain);
    QDir().rename(staging, current);

    QDirIterator iterator(current, {"ppc", "ppc.exe"}, QDir::Files, QDirIterator::Subdirectories);
    QString bin = current + "/bin";
    if (iterator.hasNext()) bin = QFileInfo(iterator.next()).absolutePath();

    QFile binMarker(toolchain + "/bin-path.txt");
    if (binMarker.open(QIODevice::WriteOnly | QIODevice::Truncate))
        binMarker.write(bin.toUtf8());
    QFile tagMarker(toolchain + "/release-tag.txt");
    if (tagMarker.open(QIODevice::WriteOnly | QIODevice::Truncate))
        tagMarker.write(tag.toUtf8());
}

bool ToolchainManager::installArchive(const QString &archivePath,
                                      const ReleaseAssetInfo &asset,
                                      QString *error) {
    Q_UNUSED(asset);
    const QString toolchain = appDataDir() + "/toolchain";
    const QString staging = toolchain + "/staging";
    QDir(staging).removeRecursively();
    QDir().mkpath(staging);
    if (!extractWithLibarchive(archivePath, staging, error)) return false;
    if (!smokeTest(staging, error)) {
        QDir(staging).removeRecursively();
        return false;
    }
    finishStagedInstall(staging, latestTag_);
    return true;
}

bool ToolchainManager::installMsi(const QString &msiPath, QString *error) {
#ifdef Q_OS_WIN
    const QString toolchain = appDataDir() + "/toolchain";
    const QString staging = toolchain + "/staging";
    QDir(staging).removeRecursively();
    QDir().mkpath(staging);
    QProcess process;
    const QString target = QDir::toNativeSeparators(staging);
    process.start("msiexec.exe",
                  {"/a", QDir::toNativeSeparators(msiPath), "/qn", "TARGETDIR=" + target});
    if (!process.waitForFinished(120000) || process.exitCode() != 0) {
        *error = "Windows MSI administrative extraction failed";
        return false;
    }
    if (!smokeTest(staging, error)) return false;
    finishStagedInstall(staging, latestTag_);
    return true;
#else
    Q_UNUSED(msiPath);
    *error = "MSI assets are only supported on Windows";
    return false;
#endif
}
