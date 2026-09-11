#include "PackageManagerDialog.h"
#include "ToolchainManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QPair>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>

PackageManagerDialog::PackageManagerDialog(
    ToolchainManager *toolchain,
    const QString &projectRoot,
    QWidget *parent)
    : QDialog(parent), toolchain_(toolchain), root_(projectRoot) {
    setWindowTitle("PPX Package Manager");
    resize(760, 520);

    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel("PPX Packages");
    title->setObjectName("brandText");
    auto *hint = new QLabel("Search, install, remove and audit packages in the active PunPun project.");
    hint->setObjectName("muted");
    layout->addWidget(title);
    layout->addWidget(hint);

    auto *row = new QHBoxLayout;
    query_ = new QLineEdit;
    query_->setPlaceholderText("Package name or search query");
    row->addWidget(query_, 1);

    auto addCommandButton = [this, row](const QString &text, const QStringList &prefix) {
        auto *button = new QPushButton(text);
        row->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, prefix] {
            QStringList args = prefix;
            const QString query = query_->text().trimmed();
            if (!query.isEmpty()) {
                args << query;
            }
            run(args);
        });
        return button;
    };

    addCommandButton("Search", {"search"});
    auto *install = addCommandButton("Install", {"add"});
    install->setObjectName("primary");
    addCommandButton("Remove", {"remove"});
    layout->addLayout(row);

    auto *tools = new QHBoxLayout;
    const QList<QPair<QString, QStringList>> toolCommands = {
        {"Update", {"update"}},
        {"Tree", {"tree"}},
        {"Audit", {"audit"}},
        {"Doctor", {"doctor"}},
    };
    for (const auto &spec : toolCommands) {
        auto *button = new QPushButton(spec.first);
        tools->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, args = spec.second] {
            run(args);
        });
    }
    tools->addStretch();
    layout->addLayout(tools);

    output_ = new QPlainTextEdit;
    output_->setReadOnly(true);
    layout->addWidget(output_, 1);

    connect(query_, &QLineEdit::returnPressed, this, [this] {
        run({"search", query_->text().trimmed()});
    });
}

void PackageManagerDialog::run(const QStringList &args) {
    if (root_.isEmpty()) {
        output_->appendPlainText("Open a project folder first.");
        return;
    }

    QString executable = toolchain_->ppxPath();
    QStringList actualArgs = args;
    if (executable.isEmpty()) {
        executable = toolchain_->ppPath();
        if (!executable.isEmpty()) {
            actualArgs.prepend("x");
        }
    }
    if (executable.isEmpty()) {
        output_->appendPlainText("PPX was not found. The IDE updater will install the current PunPun toolchain automatically.");
        return;
    }

    if (process_ && process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->deleteLater();
    }

    process_ = new QProcess(this);
    process_->setWorkingDirectory(root_);
    process_->setProcessChannelMode(QProcess::MergedChannels);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    QString bridge = QCoreApplication::applicationDirPath() + QDir::separator() + "ppide-pp-bridge";
#ifdef Q_OS_WIN
    bridge += ".exe";
#endif
    if (QFileInfo::exists(bridge)) {
        environment.insert("PUNPUN_PP", bridge);
    }

    const QString privateBin = toolchain_->privateBinDir();
    if (!privateBin.isEmpty()) {
        environment.insert("PATH", privateBin + QDir::listSeparator() + environment.value("PATH"));
    }
    process_->setProcessEnvironment(environment);

    output_->appendPlainText("\n❯ " + executable + " " + actualArgs.join(' '));
    connect(process_, &QProcess::readyReadStandardOutput, this, [this] {
        output_->moveCursor(QTextCursor::End);
        output_->insertPlainText(QString::fromLocal8Bit(process_->readAllStandardOutput()));
    });
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        output_->appendPlainText("\n[process error] " + process_->errorString());
    });
    connect(process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        output_->appendPlainText(QString("\n[finished: %1]").arg(code));
    });
    process_->start(executable, actualArgs);
}
