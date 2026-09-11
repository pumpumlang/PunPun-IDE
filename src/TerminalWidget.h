#pragma once

#include <QByteArray>
#include <QEvent>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QToolButton;

class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);

    void start(const QString &cwd, const QString &extraBin = {});
    void setWorkingDirectory(const QString &cwd);
    void clearTerminal();
    void sendCommand(const QString &command);
    /// Run a program, optionally from `workingDirectory`. Without it the
    /// command inherits whatever directory the shell happens to sit in,
    /// which breaks programs that open files by relative path.
    void runCommand(const QString &program, const QStringList &arguments = {},
                    const QString &workingDirectory = {});
    void setFontSize(int points);
    bool isRunning() const { return process_.state() == QProcess::Running; }

Q_SIGNALS:
    void processText(const QString &text);
    void workingDirectoryChanged(const QString &cwd);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPlainTextEdit *output_ = nullptr;
    QLineEdit *input_ = nullptr;
    QLabel *cwdLabel_ = nullptr;
    QLabel *shellLabel_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QProcess process_;
    QString cwd_;
    QString extraBin_;
    QString receiveBuffer_;
    QStringList history_;
    int historyIndex_ = 0;
    bool restarting_ = false;
    bool foregroundBusy_ = false;
    QString shellFlavor_;

    void appendBytes(const QByteArray &bytes);
    void appendText(const QString &text);
    void submitInput();
    void restartShell();
    void stopForeground();
    void updateHeader();
    void setForegroundBusy(bool busy);
    QString wrapShellCommand(const QString &command) const;
    static QString shellQuote(const QString &value);
};
