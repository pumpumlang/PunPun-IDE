#pragma once

#include "CodeEditor.h"

#include <QWidget>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QTextBrowser;
class QPushButton;

class AssistantPanel : public QWidget {
    Q_OBJECT
public:
    explicit AssistantPanel(QWidget *parent = nullptr);
    void setDiagnostics(const QString &path, const QVector<EditorDiagnostic> &items);
    void setEnvironmentSummary(const QString &summary);
    void setBusy(bool busy);

Q_SIGNALS:
    void navigateRequested(const QString &path, int line, int column);
    void checkRequested();

private:
    QString currentPath_;
    QVector<EditorDiagnostic> diagnostics_;
    QLabel *summary_ = nullptr;
    QLabel *errorCount_ = nullptr;
    QLabel *warningCount_ = nullptr;
    QLabel *hintCount_ = nullptr;
    QLabel *environment_ = nullptr;
    QListWidget *list_ = nullptr;
    QTextBrowser *details_ = nullptr;
    QPushButton *checkButton_ = nullptr;

    void rebuild();
    void showDetails(QListWidgetItem *item);
};
