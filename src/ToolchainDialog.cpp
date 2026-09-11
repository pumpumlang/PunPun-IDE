#include "ToolchainDialog.h"
#include "ToolchainManager.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

ToolchainDialog::ToolchainDialog(ToolchainManager *toolchain, QWidget *parent)
    : QDialog(parent), toolchain_(toolchain) {
    setWindowTitle("PunPun Toolchain");
    resize(580, 300);

    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel("PunPun Toolchain");
    title->setObjectName("brandText");

    auto *help = new QLabel(
        "PunPun IDE follows GitHub's latest stable release and keeps a private, "
        "verified toolchain. Your system compiler is left alone.");
    help->setWordWrap(true);
    help->setObjectName("muted");

    installed_ = new QLabel;
    latest_ = new QLabel;
    status_ = new QLabel;
    status_->setWordWrap(true);
    progress_ = new QProgressBar;
    progress_->setRange(0, 100);
    progress_->setValue(0);

    layout->addWidget(title);
    layout->addWidget(help);
    layout->addSpacing(8);
    layout->addWidget(installed_);
    layout->addWidget(latest_);
    layout->addWidget(progress_);
    layout->addWidget(status_);
    layout->addStretch();

    auto *buttons = new QHBoxLayout;
    auto *check = new QPushButton("Check now");
    install_ = new QPushButton("Install latest");
    install_->setObjectName("primary");
    auto *close = new QPushButton("Close");
    buttons->addWidget(check);
    buttons->addWidget(install_);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);

    connect(check, &QPushButton::clicked, toolchain_,
            [this] { toolchain_->checkForUpdates(true); });
    connect(install_, &QPushButton::clicked,
            toolchain_, &ToolchainManager::installLatest);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    connect(toolchain_, &ToolchainManager::status, this,
            [this](const QString &status) {
                status_->setText(status);
                refresh();
            });
    connect(toolchain_, &ToolchainManager::updateAvailable, this,
            [this](const QString &, const QString &latest) {
                latest_->setText("Latest stable: " + latest);
                install_->setEnabled(true);
            });
    connect(toolchain_, &ToolchainManager::progress, this,
            [this](qint64 done, qint64 total) {
                if (total > 0) {
                    progress_->setRange(0, 100);
                    progress_->setValue(int(done * 100 / total));
                } else {
                    progress_->setRange(0, 0);
                }
            });
    connect(toolchain_, &ToolchainManager::installed, this,
            [this](const QString &) {
                progress_->setRange(0, 100);
                progress_->setValue(100);
                refresh();
            });
    connect(toolchain_, &ToolchainManager::error, this,
            [this](const QString &error) { status_->setText(error); });

    refresh();
    toolchain_->checkForUpdates(true);
}

void ToolchainDialog::refresh() {
    const QString installed = toolchain_->installedVersion();
    installed_->setText("Installed: " +
                        (installed.isEmpty() ? QString("not found") : installed));

    const QString latest = toolchain_->latestTag();
    latest_->setText("Latest stable: " +
                     (latest.isEmpty() ? QString("checking…") : latest));
}
