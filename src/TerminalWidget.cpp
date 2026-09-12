#include "TerminalWidget.h"

#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
const QString kBeginMarker = "__PPIDE_BEGIN__";
const QString kEndMarker = "__PPIDE_END__";
const QString kCwdMarker = "__PPIDE_CWD__";

bool couldBeProtocolPrefix(const QString &text) {
    return kBeginMarker.startsWith(text) ||
           kEndMarker.startsWith(text) ||
           kCwdMarker.startsWith(text);
}
}

TerminalWidget::TerminalWidget(QWidget *parent) : QWidget(parent) {
    setObjectName("terminalWidget");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget;
    toolbar->setObjectName("terminalToolbar");
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(10, 4, 6, 4);
    toolbarLayout->setSpacing(8);

    shellLabel_ = new QLabel("TERMINAL");
    shellLabel_->setObjectName("terminalTitle");
    stateLabel_ = new QLabel("READY");
    stateLabel_->setObjectName("terminalState");
    cwdLabel_ = new QLabel;
    cwdLabel_->setObjectName("terminalCwd");
    cwdLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *clear = new QToolButton;
    clear->setText("Clear");
    clear->setToolTip("Clear terminal output (Ctrl+L)");
    auto *stop = new QToolButton;
    stop->setText("Stop");
    stop->setToolTip("Stop the current program and restart the shell");
    auto *restart = new QToolButton;
    restart->setText("Restart");
    restart->setToolTip("Restart terminal shell");

    toolbarLayout->addWidget(shellLabel_);
    toolbarLayout->addWidget(stateLabel_);
    toolbarLayout->addWidget(cwdLabel_, 1);
    toolbarLayout->addWidget(clear);
    toolbarLayout->addWidget(stop);
    toolbarLayout->addWidget(restart);
    layout->addWidget(toolbar);

    output_ = new QPlainTextEdit;
    output_->setReadOnly(true);
    output_->setMaximumBlockCount(8000);
    output_->setObjectName("terminalOutput");
    output_->setUndoRedoEnabled(false);
    output_->setLineWrapMode(QPlainTextEdit::NoWrap);
    output_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    layout->addWidget(output_, 1);

    auto *inputRow = new QWidget;
    inputRow->setObjectName("terminalInputRow");
    auto *inputLayout = new QHBoxLayout(inputRow);
    inputLayout->setContentsMargins(10, 4, 10, 6);
    inputLayout->setSpacing(7);
    auto *prompt = new QLabel(QString::fromUtf8("❯"));
    prompt->setObjectName("terminalPrompt");
    input_ = new QLineEdit;
    input_->setObjectName("terminalInput");
    input_->setPlaceholderText("Type a command…");
    input_->installEventFilter(this);
    inputLayout->addWidget(prompt);
    inputLayout->addWidget(input_, 1);
    layout->addWidget(inputRow);

    process_.setProcessChannelMode(QProcess::MergedChannels);
    connect(&process_, &QProcess::readyReadStandardOutput, this, [this] {
        appendBytes(process_.readAllStandardOutput());
    });
    connect(&process_, &QProcess::readyReadStandardError, this, [this] {
        appendBytes(process_.readAllStandardError());
    });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        Q_UNUSED(error);
        appendText(QString("\n[terminal] %1\n").arg(process_.errorString()));
        setForegroundBusy(false);
    });
    connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
        setForegroundBusy(false);
        if (!restarting_)
            appendText(QString("\n[terminal exited %1] Press Restart to reopen it.\n").arg(code));
        updateHeader();
    });
    connect(input_, &QLineEdit::returnPressed, this, &TerminalWidget::submitInput);
    connect(clear, &QToolButton::clicked, this, &TerminalWidget::clearTerminal);
    connect(stop, &QToolButton::clicked, this, &TerminalWidget::stopForeground);
    connect(restart, &QToolButton::clicked, this, &TerminalWidget::restartShell);
}

void TerminalWidget::start(const QString &cwd, const QString &extraBin) {
    cwd_ = cwd.isEmpty() ? QDir::homePath() : QDir(cwd).absolutePath();
    extraBin_ = extraBin;
    restartShell();
}

void TerminalWidget::restartShell() {
    restarting_ = true;
    setForegroundBusy(false);
    receiveBuffer_.clear();
    if (process_.state() != QProcess::NotRunning) {
        process_.terminate();
        if (!process_.waitForFinished(500)) process_.kill();
        process_.waitForFinished(500);
    }

    process_.setWorkingDirectory(cwd_.isEmpty() ? QDir::homePath() : cwd_);
    auto env = QProcessEnvironment::systemEnvironment();
    if (!extraBin_.isEmpty())
        env.insert("PATH", extraBin_ + QDir::listSeparator() + env.value("PATH"));
    env.insert("TERM", "xterm-256color");
    env.insert("COLORTERM", "truecolor");
    process_.setProcessEnvironment(env);

#ifdef Q_OS_WIN
    const QString shell = qEnvironmentVariable("COMSPEC", "cmd.exe");
    shellFlavor_ = "cmd";
    process_.start(shell, {"/Q", "/D", "/V:ON"});
    shellLabel_->setText("TERMINAL · CMD");
#else
    QString shell = qEnvironmentVariable("SHELL");
    if (shell.isEmpty() || !QFileInfo::exists(shell)) shell = QStandardPaths::findExecutable("fish");
    if (shell.isEmpty()) shell = QStandardPaths::findExecutable("bash");
    if (shell.isEmpty()) shell = "/bin/sh";

    const QString base = QFileInfo(shell).fileName().toLower();
    shellFlavor_ = base.contains("fish") ? "fish" : "posix";
    // Non-interactive on purpose. The IDE owns the prompt while the shell keeps
    // cwd/export state across commands without drawing a second shell prompt.
    process_.start(shell, {});
    shellLabel_->setText("TERMINAL · " + QFileInfo(shell).fileName().toUpper());
#endif

    if (!process_.waitForStarted(1200)) {
        appendText(QString("[terminal] Could not start shell: %1\n").arg(process_.errorString()));
    }
    restarting_ = false;
    updateHeader();
    input_->setFocus();
}

void TerminalWidget::stopForeground() {
    if (process_.state() == QProcess::NotRunning) return;
    appendText("\n[terminal] stopped current process and restarted the shell\n");
    restartShell();
}

void TerminalWidget::setWorkingDirectory(const QString &cwd) {
    cwd_ = cwd.isEmpty() ? QDir::homePath() : QDir(cwd).absolutePath();
    restartShell();
}

void TerminalWidget::clearTerminal() {
    output_->clear();
}

void TerminalWidget::appendText(const QString &text) {
    if (text.isEmpty()) return;
    output_->moveCursor(QTextCursor::End);
    output_->insertPlainText(text);
    output_->moveCursor(QTextCursor::End);
    output_->ensureCursorVisible();
    Q_EMIT processText(text);
}

void TerminalWidget::setForegroundBusy(bool busy) {
    foregroundBusy_ = busy;
    if (stateLabel_) stateLabel_->setText(busy ? "RUNNING" : "READY");
    if (input_) input_->setPlaceholderText(busy ? "Program input…" : "Type a command…");
}

void TerminalWidget::appendBytes(const QByteArray &bytes) {
    QString text = QString::fromLocal8Bit(bytes);
    static const QRegularExpression ansi("\\x1B(?:\\[[0-?]*[ -/]*[@-~]|\\][^\\x07]*(?:\\x07|\\x1B\\\\))");
    text.remove(ansi);

    receiveBuffer_ += text;
    int newline = -1;
    while ((newline = receiveBuffer_.indexOf('\n')) >= 0) {
        QString line = receiveBuffer_.left(newline);
        receiveBuffer_.remove(0, newline + 1);
        if (line.endsWith('\r')) line.chop(1);

        if (line == kBeginMarker) {
            setForegroundBusy(true);
            continue;
        }
        if (line.startsWith(kEndMarker)) {
            bool ok = false;
            const int code = line.mid(kEndMarker.size()).trimmed().toInt(&ok);
            setForegroundBusy(false);
            if (ok && code != 0) appendText(QString("[exit %1]\n").arg(code));
            continue;
        }
        if (line.startsWith(kCwdMarker)) {
            const QString next = line.mid(kCwdMarker.size()).trimmed();
            if (!next.isEmpty()) {
                cwd_ = next;
                updateHeader();
                Q_EMIT workingDirectoryChanged(cwd_);
            }
            continue;
        }
        appendText(line + "\n");
    }

    // Show prompts/output even when programs do not print a trailing newline.
    // Hold only a possible protocol marker until the rest arrives.
    if (!receiveBuffer_.isEmpty() && !couldBeProtocolPrefix(receiveBuffer_) &&
        !receiveBuffer_.startsWith(kBeginMarker) &&
        !receiveBuffer_.startsWith(kEndMarker) &&
        !receiveBuffer_.startsWith(kCwdMarker)) {
        appendText(receiveBuffer_);
        receiveBuffer_.clear();
    }
}

QString TerminalWidget::wrapShellCommand(const QString &command) const {
#ifdef Q_OS_WIN
    return QString("echo %1 & %2 & set __ppide_code=!errorlevel! & echo %3!__ppide_code! & echo %4!CD!")
        .arg(kBeginMarker, command, kEndMarker, kCwdMarker);
#else
    if (shellFlavor_ == "fish") {
        // QString::arg() substitutes %1..%99 only; it does not collapse "%" +
        // "%" the way printf does. Doubling the sign here shipped it verbatim
        // to the shell, so the cwd report came back as literal text instead of
        // the directory. A bare conversion is not a Qt placeholder and passes
        // through untouched.
        return QString("printf '%1\\n'; %2; set __ppide_code $status; printf '%3%s\\n' \"$__ppide_code\"; printf '%4%s\\n' \"$PWD\"")
            .arg(kBeginMarker, command, kEndMarker, kCwdMarker);
    }
    return QString("printf '%1\\n'; { %2; }; __ppide_code=$?; printf '%3%s\\n' \"$__ppide_code\"; printf '%4%s\\n' \"$PWD\"")
        .arg(kBeginMarker, command, kEndMarker, kCwdMarker);
#endif
}

void TerminalWidget::submitInput() {
    const QString command = input_->text();
    if (command.isEmpty()) return;
    if (process_.state() != QProcess::Running) restartShell();
    if (process_.state() != QProcess::Running) return;

    if (foregroundBusy_) {
        // A child program owns stdin. Send exactly what the user typed instead
        // of accidentally queueing IDE shell bookkeeping behind it.
        appendText(command + "\n");
        input_->clear();
        process_.write((command + "\n").toLocal8Bit());
        return;
    }

    if (history_.isEmpty() || history_.constLast() != command) history_.push_back(command);
    historyIndex_ = int(history_.size());
    appendText(QString::fromUtf8("❯ ") + command + "\n");
    input_->clear();
    process_.write((wrapShellCommand(command) + "\n").toLocal8Bit());
}

void TerminalWidget::showNotice(const QString &text) {
    QString body = text;
    if (!body.endsWith('\n')) body += '\n';
    appendText("\n" + body);
}

void TerminalWidget::sendCommand(const QString &command) {
    if (command.trimmed().isEmpty()) return;
    if (foregroundBusy_) {
        appendText("[terminal] A program is already running. Stop it before launching another command.\n");
        return;
    }
    input_->setText(command);
    submitInput();
}

QString TerminalWidget::shellQuote(const QString &value) {
#ifdef Q_OS_WIN
    QString escaped = value;
    escaped.replace("\"", "\"\"");
    return "\"" + escaped + "\"";
#else
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    escaped.replace("$", "\\$");
    escaped.replace("`", "\\`");
    return "\"" + escaped + "\"";
#endif
}

void TerminalWidget::runCommand(const QString &program, const QStringList &arguments,
                                const QString &workingDirectory) {
    QStringList parts;
    parts << shellQuote(program);
    for (const auto &argument : arguments) parts << shellQuote(argument);
    QString command = parts.join(' ');
    if (!workingDirectory.isEmpty() &&
        QDir(workingDirectory) != QDir(cwd_)) {
        command = "cd " + shellQuote(QDir::toNativeSeparators(workingDirectory)) +
                  " && " + command;
    }
    sendCommand(command);
}

void TerminalWidget::setFontSize(int points) {
    QFont font = output_->font();
    font.setPointSize(qBound(8, points, 32));
    output_->setFont(font);
    input_->setFont(font);
}

void TerminalWidget::updateHeader() {
    cwdLabel_->setText(QDir::toNativeSeparators(cwd_));
    cwdLabel_->setToolTip(cwd_);
}

bool TerminalWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == input_ && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Up && !foregroundBusy_ && !history_.isEmpty()) {
            historyIndex_ = qMax(0, historyIndex_ - 1);
            input_->setText(history_.value(historyIndex_));
            return true;
        }
        if (key->key() == Qt::Key_Down && !foregroundBusy_ && !history_.isEmpty()) {
            historyIndex_ = qMin(int(history_.size()), historyIndex_ + 1);
            input_->setText(historyIndex_ < history_.size() ? history_.value(historyIndex_) : QString());
            return true;
        }
        if ((key->modifiers() & Qt::ControlModifier) && key->key() == Qt::Key_L) {
            clearTerminal();
            return true;
        }
        if ((key->modifiers() & Qt::ControlModifier) && key->key() == Qt::Key_C &&
            foregroundBusy_ && !input_->hasSelectedText()) {
            stopForeground();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
