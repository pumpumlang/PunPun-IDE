#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QLineEdit;
class QPlainTextEdit;
class QProcess;
class ToolchainManager;

class PackageManagerDialog : public QDialog {
    Q_OBJECT

public:
    PackageManagerDialog(ToolchainManager *toolchain,
                         const QString &projectRoot,
                         QWidget *parent = nullptr);

private:
    ToolchainManager *toolchain_ = nullptr;
    QString root_;
    QLineEdit *query_ = nullptr;
    QPlainTextEdit *output_ = nullptr;
    QProcess *process_ = nullptr;

    void run(const QStringList &args);
};
