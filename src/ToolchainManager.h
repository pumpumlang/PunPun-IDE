#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

class IdeSettings;
class QNetworkReply;

struct ReleaseAssetInfo {
    QString name;
    QString url;
    QString sha256;
    QString kind;
    qint64 size = 0;

    bool valid() const {
        return !name.isEmpty() && !url.isEmpty();
    }
};

class ToolchainManager : public QObject {
    Q_OBJECT

public:
    explicit ToolchainManager(IdeSettings *settings, QObject *parent = nullptr);

    QString ppcPath() const;
    QString ppPath() const;
    QString ppxPath() const;
    QString privateBinDir() const;

    /// Every directory the toolchain is looked for in, most specific first.
    /// The terminal prepends these to PATH so a command typed there resolves
    /// the same compiler the Run button does.
    QStringList searchPaths() const;

    /// A source checkout builds its compiler into the project, so the open
    /// project participates in discovery. Empty clears it.
    void setProjectRoot(const QString &path);

    /// Human-readable account of where the toolchain was looked for, shown
    /// when it could not be found.
    QString discoveryReport() const;
    QString installedVersion() const;
    QString installedTag() const;
    QString latestTag() const { return latestTag_; }

    void startAutomaticChecks();
    void checkForUpdates(bool userInitiated = false);
    void installLatest();

Q_SIGNALS:
    void status(const QString &text);
    void updateAvailable(const QString &installed, const QString &latest);
    void progress(qint64 done, qint64 total);
    void installed(const QString &tag);
    void error(const QString &message);

private:
    IdeSettings *settings_ = nullptr;
    QNetworkAccessManager network_;
    QTimer timer_;
    QJsonObject latestRelease_;
    ReleaseAssetInfo latestAsset_;
    QString latestTag_;
    QString projectRoot_;
    bool checking_ = false;
    bool downloading_ = false;

    QString appDataDir() const;
    QString findExecutable(const QStringList &names) const;
    static QString versionFromText(const QString &text);
    static int compareVersions(const QString &a, const QString &b);
    ReleaseAssetInfo chooseAsset(const QJsonObject &release) const;
    void handleRelease(QNetworkReply *reply, bool userInitiated);
    void downloadAsset(const ReleaseAssetInfo &asset);
    bool installArchive(const QString &archivePath,
                        const ReleaseAssetInfo &asset,
                        QString *error);
    bool installMsi(const QString &msiPath, QString *error);
    bool smokeTest(const QString &root, QString *error) const;
    static bool extractWithLibarchive(const QString &archivePath,
                                      const QString &destination,
                                      QString *error);
    void finishStagedInstall(const QString &staging, const QString &tag);
};
