#pragma once

#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QMainWindow>
#include <QModelIndex>
#include <QPointer>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>

#include "CodeEditor.h"
#include "ScriptHost.h"
#include "Settings.h"
#include "ToolchainManager.h"

class QAction;
class QCloseEvent;
class AssistantPanel;
class FileIconProvider;
class QFileSystemModel;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QStackedWidget;
class QTabWidget;
class QTextBrowser;
class QTimer;
class QToolButton;
class QTreeView;
class TerminalWidget;
class LspClient;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &startPath = {}, QWidget *parent = nullptr);
    ~MainWindow() override;

    /// Open an additional file in a new tab. Used for the extra paths the
    /// desktop entry's %F can deliver alongside the first one.
    void openFile(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    IdeSettings settings_;
    ScriptHost scripts_;
    ToolchainManager toolchain_;
    LspClient *lsp_ = nullptr;
    QString projectRoot_;

    QFileSystemModel *fsModel_ = nullptr;
    std::unique_ptr<FileIconProvider> fileIconProvider_;
    QTreeView *tree_ = nullptr;
    QLabel *projectLabel_ = nullptr;
    QStackedWidget *sideStack_ = nullptr;
    QStackedWidget *centerStack_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    QTabWidget *bottomTabs_ = nullptr;
    QListWidget *searchResults_ = nullptr;
    QListWidget *extensionList_ = nullptr;
    QListWidget *problemList_ = nullptr;
    QLineEdit *searchBox_ = nullptr;
    QPlainTextEdit *output_ = nullptr;
    TerminalWidget *terminal_ = nullptr;
    AssistantPanel *assistant_ = nullptr;
    QFrame *bottomPanel_ = nullptr;
    QFrame *hoverCard_ = nullptr;
    QTextBrowser *hoverText_ = nullptr;
    QLabel *statusProject_ = nullptr;
    QLabel *statusLanguage_ = nullptr;
    QLabel *statusCursor_ = nullptr;
    QLabel *statusPunPun_ = nullptr;

    QHash<QWidget *, CodeEditor *> pageEditors_;
    QHash<CodeEditor *, QTimer *> autoSaveTimers_;
    QHash<CodeEditor *, QTimer *> smartCheckTimers_;
    QHash<QString, QAction *> actions_;
    QHash<QString, QToolButton *> activityButtons_;
    QList<QToolButton *> tintedButtons_;
    QHash<QString, QVector<EditorDiagnostic>> compilerDiagnostics_;
    QHash<QString, QVector<EditorDiagnostic>> smartDiagnostics_;
    QHash<QString, quint64> nativeCheckGeneration_;
    quint64 diagnosticGeneration_ = 0;

    QPointer<CodeEditor> pendingHoverEditor_;
    QPoint pendingHoverPosition_;
    quint64 pendingHoverToken_ = 0;
    quint64 pendingCompletionToken_ = 0;
    QPointer<CodeEditor> pendingCompletionEditor_;

    void buildUi();
    QWidget *buildActivityBar();
    QWidget *buildExplorerPage();
    QWidget *buildSearchPage();
    QWidget *buildExtensionsPage();
    QWidget *buildRunPage();
    QWidget *buildWelcomePage();
    QWidget *buildEditorRegion();
    QWidget *buildBottomPanel();
    void buildMenus();
    void createActions();
    void applySettings();
    void configureEditor(CodeEditor *editor);
    void refreshActivityIcons();
    void refreshTabCloseButtons();
    void setSidebarPage(int index, const QString &id);

    void openProject(const QString &path, bool remember = true);
    void chooseFolder();
    void chooseFile();
    CodeEditor *editorAt(int index) const;
    CodeEditor *currentEditor() const;
    int tabForEditor(CodeEditor *editor) const;
    bool saveEditor(CodeEditor *editor, bool forceDialog = false);
    void saveCurrent();
    void saveAll();
    void closeTab(int index);
    bool canCloseEditor(CodeEditor *editor);
    void updateTabTitle(CodeEditor *editor);
    void updateStatusForEditor(CodeEditor *editor);
    void showWelcomeIfNeeded();

    void showExplorerMenu(const QPoint &pos);
    QString contextDirectory(const QModelIndex &index) const;
    void createFileAt(const QString &directory, const QString &suggestion = {});
    void createFolderAt(const QString &directory);
    void renameIndex(const QModelIndex &index);
    void deleteIndex(const QModelIndex &index);
    void runSearch();
    void refreshExtensions();

    void toggleBottomPanel(int tab = -1);
    void toggleAssistant();
    void appendOutput(const QString &text);
    void rebuildProblems();
    void appendProblem(const QString &path, const EditorDiagnostic &diagnostic);
    void handleProblemsActivated();

    void reportRunProblem(const QString &message);
    void runCurrent();
    void checkCurrent();
    void debugCurrent();
    void formatCurrent();
    void runProcess(const QString &program, const QStringList &args,
                    const QString &cwd, const QString &title,
                    std::function<void(int, const QString &)> finished = {},
                    bool revealOutput = true);
    QString compilerFor(int language, bool cpp) const;

    void scheduleSmartCheck(CodeEditor *editor);
    void runSmartCheck(CodeEditor *editor);
    void runNativeSyntaxCheck(CodeEditor *editor, bool revealOutput = false);
    QVector<EditorDiagnostic> parseNativeDiagnostics(const QString &text,
                                                     const QString &fallbackPath) const;
    QVector<EditorDiagnostic> combinedDiagnostics(const QString &path) const;
    void refreshDiagnostics(const QString &path);
    void refreshAssistantForCurrent();

    void startLanguageService();
    void reopenPunPunDocumentsInLsp();
    void handleLspDiagnostics(const QString &path, const QJsonArray &items);
    void requestHover(const QString &path, int line, int column,
                      const QString &word, const QPoint &globalPos);
    QString localHover(CodeEditor *editor, const QString &word) const;
    void showHover(const QString &markdown, const QPoint &globalPos);
    void requestCompletion();

    void runEnvironmentDoctor();
    QString doctorResourcePath() const;
    QString environmentSummaryFromDoctor(const QString &text) const;

    void openSettings();
    void openToolchain();
    void openPackages();
    void configureUpdateSignals();
    QString readText(const QString &path, QString *error) const;
    static QString languageKey(int language);
    static QString normalizedPath(const QString &path);
};
