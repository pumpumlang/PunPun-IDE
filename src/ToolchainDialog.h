#pragma once

#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;
class ToolchainManager;

class ToolchainDialog : public QDialog {
    Q_OBJECT
public:
    explicit ToolchainDialog(ToolchainManager *toolchain, QWidget *parent = nullptr);

private:
    ToolchainManager *toolchain_ = nullptr;
    QLabel *installed_ = nullptr;
    QLabel *latest_ = nullptr;
    QLabel *status_ = nullptr;
    QProgressBar *progress_ = nullptr;
    QPushButton *install_ = nullptr;

    void refresh();
};
