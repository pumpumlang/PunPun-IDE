#include "MainWindow.h"

#include "AssistantPanel.h"
#include "FileIconProvider.h"
#include "IconUtils.h"
#include <QToolButton>
#include <QTabBar>
#include "LspClient.h"
#include "PackageManagerDialog.h"
#include "SettingsDialog.h"
#include "SmartAnalyzer.h"
#include "TerminalWidget.h"
#include "Theme.h"
#include "ToolchainDialog.h"

#include <QAction>
#include <QByteArray>
#include <QColor>
#include <QIcon>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFrame>
#include <QHBoxLayout>
#include <QIODevice>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSet>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVersionNumber>
#include <QVariantMap>

#include <algorithm>
#include <memory>

namespace {
QToolButton *smallButton(const QString &tooltip, const QIcon &icon = {}) {
    auto *button = new QToolButton;
    button->setToolTip(tooltip);
    button->setAutoRaise(true);
    button->setIcon(icon);
    button->setIconSize({18, 18});
    return button;
}

QString cleanName(const QString &name) {
    const QString trimmed = name.trimmed();
    if (trimmed == "." || trimmed == ".." || trimmed.contains('/') || trimmed.contains('\\'))
        return {};
    return trimmed;
}

int severityFromString(const QString &value) {
    const QString s = value.toLower();
    if (s == "warning") return 2;
    if (s == "note" || s == "information" || s == "hint") return 3;
    return 1;
}
}

MainWindow::MainWindow(const QString &startPath, QWidget *parent)
    : QMainWindow(parent), toolchain_(&settings_, this) {
    setWindowTitle(QString("PunPun IDE %1").arg(PPIDE_VERSION));
    resize(1540, 940);
    setMinimumSize(1000, 650);

    buildUi();
    createActions();
    buildMenus();
    applySettings();
    configureUpdateSignals();

    QString requested = startPath;
    if (requested.isEmpty()) {
        const QString last = settings_.lastProject();
        if (QDir(last).exists()) requested = last;
    }

    if (!requested.isEmpty()) {
        const QFileInfo info(requested);
        if (info.isFile()) {
            openProject(info.absolutePath(), false);
            openFile(info.absoluteFilePath());
        } else if (info.isDir()) {
            openProject(info.absoluteFilePath());
        }
    }

    showWelcomeIfNeeded();
    toolchain_.startAutomaticChecks();
    QTimer::singleShot(900, this, &MainWindow::startLanguageService);
    QTimer::singleShot(1400, this, &MainWindow::runEnvironmentDoctor);
}

MainWindow::~MainWindow() {
    if (lsp_) lsp_->stop();
}

void MainWindow::buildUi() {
    auto *root = new QWidget;
    root->setObjectName("root");
    auto *outer = new QVBoxLayout(root);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *body = new QSplitter(Qt::Horizontal);
    body->setObjectName("workbench");
    body->setChildrenCollapsible(false);
    body->addWidget(buildActivityBar());

    sideStack_ = new QStackedWidget;
    sideStack_->setObjectName("sideBarStack");
    sideStack_->setMinimumWidth(210);
    sideStack_->setMaximumWidth(430);
    sideStack_->addWidget(buildExplorerPage());
    sideStack_->addWidget(buildSearchPage());
    sideStack_->addWidget(buildRunPage());
    sideStack_->addWidget(buildExtensionsPage());
    body->addWidget(sideStack_);

    auto *mainVertical = new QSplitter(Qt::Vertical);
    mainVertical->setObjectName("editorSplitter");
    mainVertical->setChildrenCollapsible(true);
    mainVertical->addWidget(buildEditorRegion());
    mainVertical->addWidget(buildBottomPanel());
    mainVertical->setStretchFactor(0, 1);
    mainVertical->setStretchFactor(1, 0);
    mainVertical->setSizes({760, 180});
    body->addWidget(mainVertical);

    assistant_ = new AssistantPanel;
    body->addWidget(assistant_);
    body->setStretchFactor(2, 1);
    body->setSizes({48, 260, 900, 330});
    outer->addWidget(body, 1);
    setCentralWidget(root);

    connect(assistant_, &AssistantPanel::checkRequested, this, &MainWindow::checkCurrent);
    connect(assistant_, &AssistantPanel::navigateRequested, this,
            [this](const QString &path, int line, int column) {
                openFile(path);
                if (auto *editor = currentEditor()) editor->gotoLine(line, column);
            });

    statusProject_ = new QLabel("No folder");
    statusLanguage_ = new QLabel("Plain Text");
    statusCursor_ = new QLabel("Ln 1, Col 1");
    statusPunPun_ = new QLabel("PunPun: checking…");
    statusBar()->addWidget(statusProject_);
    statusBar()->addPermanentWidget(statusPunPun_);
    statusBar()->addPermanentWidget(statusLanguage_);
    statusBar()->addPermanentWidget(statusCursor_);

    hoverCard_ = new QFrame(nullptr, Qt::ToolTip);
    hoverCard_->setObjectName("hoverCard");
    auto *hoverLayout = new QVBoxLayout(hoverCard_);
    hoverLayout->setContentsMargins(12, 9, 12, 9);
    hoverText_ = new QTextBrowser;
    hoverText_->setObjectName("hoverText");
    hoverText_->setOpenExternalLinks(true);
    hoverText_->setMaximumWidth(560);
    hoverText_->setMinimumWidth(280);
    hoverText_->setMaximumHeight(280);
    hoverLayout->addWidget(hoverText_);
    hoverCard_->hide();
}

QWidget *MainWindow::buildActivityBar() {
    auto *bar = new QFrame;
    bar->setObjectName("activityBar");
    bar->setFixedWidth(50);
    auto *layout = new QVBoxLayout(bar);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(2);

    auto *brand = new QLabel;
    brand->setPixmap(QIcon(":/branding/punpun-mark.svg").pixmap(29, 29));
    brand->setAlignment(Qt::AlignCenter);
    brand->setToolTip("PunPun IDE");
    layout->addWidget(brand);
    layout->addSpacing(7);

    auto add = [&](const QString &id, const QString &icon, const QString &tip, int page) {
        auto *button = new QToolButton;
        button->setObjectName("activityButton");
        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setToolTip(tip);
        button->setProperty("iconPath", icon);
        button->setIconSize({23, 23});
        connect(button, &QToolButton::clicked, this,
                [this, page, id] { setSidebarPage(page, id); });
        layout->addWidget(button);
        activityButtons_[id] = button;
        return button;
    };

    add("explorer", ":/fluent/folder.svg", "Explorer", 0)->setChecked(true);
    add("search", ":/fluent/search.svg", "Search", 1);
    add("run", ":/fluent/play.svg", "Run and Debug", 2);
    add("extensions", ":/icons/add.svg", "Extensions", 3);
    layout->addStretch();

    auto *assistant = smallButton("Toggle Code Assistant");
    assistant->setObjectName("activityButton");
    assistant->setProperty("iconPath", ":/icons/search.svg");
    assistant->setIconSize({22, 22});
    tintedButtons_ << assistant;
    connect(assistant, &QToolButton::clicked, this, &MainWindow::toggleAssistant);
    layout->addWidget(assistant);

    auto *settings = add("settings", ":/icons/settings.svg", "Settings", 0);
    settings->setAutoExclusive(false);
    disconnect(settings, nullptr, this, nullptr);
    connect(settings, &QToolButton::clicked, this, &MainWindow::openSettings);
    return bar;
}

QWidget *MainWindow::buildExplorerPage() {
    auto *page = new QFrame;
    page->setObjectName("sideBar");
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *head = new QHBoxLayout;
    auto *title = new QLabel("EXPLORER");
    title->setObjectName("sectionTitle");
    projectLabel_ = new QLabel("NO FOLDER OPEN");
    projectLabel_->setObjectName("muted");
    projectLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *newFile = smallButton("New File");
    newFile->setText("+");
    auto *newFolder = smallButton("New Folder");
    newFolder->setText("▣+");
    connect(newFile, &QToolButton::clicked, this, [this] { createFileAt(projectRoot_); });
    connect(newFolder, &QToolButton::clicked, this, [this] { createFolderAt(projectRoot_); });
    head->addWidget(title, 1);
    head->addWidget(newFile);
    head->addWidget(newFolder);
    layout->addLayout(head);
    projectLabel_->setContentsMargins(10, 2, 8, 7);
    layout->addWidget(projectLabel_);

    fsModel_ = new QFileSystemModel(this);
    fsModel_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    fsModel_->setReadOnly(false);
    fileIconProvider_ = std::make_unique<FileIconProvider>();
    fsModel_->setIconProvider(fileIconProvider_.get());

    tree_ = new QTreeView;
    tree_->setModel(fsModel_);
    tree_->setHeaderHidden(true);
    tree_->setAnimated(true);
    tree_->setUniformRowHeights(true);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    for (int column = 1; column < 4; ++column) tree_->hideColumn(column);
    connect(tree_, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        const QString path = fsModel_->filePath(index);
        if (QFileInfo(path).isFile()) openFile(path);
    });
    connect(tree_, &QTreeView::customContextMenuRequested,
            this, &MainWindow::showExplorerMenu);
    layout->addWidget(tree_, 1);
    return page;
}

QWidget *MainWindow::buildSearchPage() {
    auto *page = new QFrame;
    page->setObjectName("sideBar");
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 0, 8, 8);
    auto *title = new QLabel("SEARCH");
    title->setObjectName("sectionTitle");
    layout->addWidget(title);
    searchBox_ = new QLineEdit;
    searchBox_->setPlaceholderText("Search files in project");
    searchResults_ = new QListWidget;
    layout->addWidget(searchBox_);
    layout->addWidget(searchResults_, 1);
    connect(searchBox_, &QLineEdit::returnPressed, this, &MainWindow::runSearch);
    connect(searchResults_, &QListWidget::itemActivated, this,
            [this](QListWidgetItem *item) { openFile(item->data(Qt::UserRole).toString()); });
    return page;
}

QWidget *MainWindow::buildExtensionsPage() {
    auto *page = new QFrame;
    page->setObjectName("sideBar");
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 0, 8, 8);
    auto *title = new QLabel("EXTENSIONS");
    title->setObjectName("sectionTitle");
    auto *hint = new QLabel("Sandboxed JavaScript adds language keywords and editor metadata. Native execution is not exposed.");
    hint->setWordWrap(true);
    hint->setObjectName("muted");
    extensionList_ = new QListWidget;
    auto *reload = new QPushButton("Reload extensions");
    connect(reload, &QPushButton::clicked, this, [this] {
        scripts_.reload();
        refreshExtensions();
        for (int i = 0; i < tabs_->count(); ++i) {
            if (auto *editor = editorAt(i)) {
                configureEditor(editor);
            }
        }
    });
    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addWidget(extensionList_, 1);
    layout->addWidget(reload);
    QTimer::singleShot(0, this, &MainWindow::refreshExtensions);
    return page;
}

QWidget *MainWindow::buildRunPage() {
    auto *page = new QFrame;
    page->setObjectName("sideBar");
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 0, 10, 10);
    auto *title = new QLabel("RUN AND DEBUG");
    title->setObjectName("sectionTitle");
    auto *run = new QPushButton("▶  Run current file");
    run->setObjectName("primary");
    auto *check = new QPushButton("✓  Check current file");
    auto *format = new QPushButton("↹  Format current file");
    auto *debug = new QPushButton("◉  Debug current file");
    auto *doctor = new QPushButton("Environment Doctor");
    connect(run, &QPushButton::clicked, this, &MainWindow::runCurrent);
    connect(check, &QPushButton::clicked, this, &MainWindow::checkCurrent);
    connect(format, &QPushButton::clicked, this, &MainWindow::formatCurrent);
    connect(debug, &QPushButton::clicked, this, &MainWindow::debugCurrent);
    connect(doctor, &QPushButton::clicked, this, &MainWindow::runEnvironmentDoctor);
    layout->addWidget(title);
    layout->addWidget(run);
    layout->addWidget(check);
    layout->addWidget(format);
    layout->addWidget(debug);
    layout->addSpacing(8);
    layout->addWidget(doctor);
    layout->addStretch();
    return page;
}

QWidget *MainWindow::buildWelcomePage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    auto *mark = new QLabel;
    mark->setPixmap(QIcon(":/branding/punpun-mark.svg").pixmap(78, 78));
    mark->setAlignment(Qt::AlignCenter);
    auto *title = new QLabel("PunPun IDE");
    title->setObjectName("brandText");
    title->setAlignment(Qt::AlignCenter);
    auto *sub = new QLabel("Native editing, compiler diagnostics, and a quiet workspace for PunPun and C/C++.");
    sub->setObjectName("muted");
    sub->setAlignment(Qt::AlignCenter);
    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    auto *folder = new QPushButton("Open Folder");
    folder->setObjectName("primary");
    folder->setMinimumWidth(150);
    auto *file = new QPushButton("Open File");
    file->setMinimumWidth(150);
    auto *newFile = new QPushButton("New File");
    newFile->setMinimumWidth(150);
    row->addStretch(1);
    row->addWidget(folder);
    row->addWidget(file);
    row->addWidget(newFile);
    row->addStretch(1);
    connect(folder, &QPushButton::clicked, this, &MainWindow::chooseFolder);
    connect(file, &QPushButton::clicked, this, &MainWindow::chooseFile);
    connect(newFile, &QPushButton::clicked, this, [this] { createFileAt(projectRoot_); });

    // Offer the last folder directly; reopening it is the common first action.
    auto *recent = new QLabel;
    recent->setObjectName("muted");
    recent->setAlignment(Qt::AlignCenter);
    recent->setTextFormat(Qt::RichText);
    recent->setOpenExternalLinks(false);
    const QString last = settings_.lastProject();
    if (!last.isEmpty() && QFileInfo(last).isDir()) {
        recent->setText(QString("Recent: <a href=\"%1\" style=\"color:%2;\">%3</a>")
                            .arg(last.toHtmlEscaped(),
                                 Theme::palette(settings_.theme()).accent.name(),
                                 QFileInfo(last).fileName().toHtmlEscaped()));
        connect(recent, &QLabel::linkActivated, this,
                [this](const QString &path) { openProject(path); });
    } else {
        recent->setText(QString());
    }

    auto *hints = new QLabel(
        "F5 run   ·   Ctrl+Shift+B check   ·   Ctrl+`  terminal   ·   Ctrl+/ comment");
    hints->setObjectName("muted");
    hints->setAlignment(Qt::AlignCenter);

    layout->addWidget(mark);
    layout->addSpacing(10);
    layout->addWidget(title);
    layout->addWidget(sub);
    layout->addSpacing(24);
    layout->addLayout(row);
    layout->addSpacing(14);
    layout->addWidget(recent);
    layout->addSpacing(28);
    layout->addWidget(hints);
    return page;
}

QWidget *MainWindow::buildEditorRegion() {
    auto *region = new QWidget;
    region->setObjectName("editorRegion");
    auto *layout = new QVBoxLayout(region);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *top = new QFrame;
    top->setObjectName("editorToolbar");
    top->setFixedHeight(36);
    auto *toolbar = new QHBoxLayout(top);
    toolbar->setContentsMargins(10, 3, 8, 3);
    auto *crumb = new QLabel("PunPun IDE");
    crumb->setObjectName("muted");
    toolbar->addWidget(crumb, 1);
    auto *run = smallButton("Run (F5)");
    run->setProperty("iconPath", ":/fluent/play.svg");
    auto *check = smallButton("Check");
    check->setProperty("iconPath", ":/icons/search.svg");
    auto *debug = smallButton("Debug");
    debug->setProperty("iconPath", ":/icons/debug-alt.svg");
    tintedButtons_ << run << check << debug;
    connect(run, &QToolButton::clicked, this, &MainWindow::runCurrent);
    connect(check, &QToolButton::clicked, this, &MainWindow::checkCurrent);
    connect(debug, &QToolButton::clicked, this, &MainWindow::debugCurrent);
    toolbar->addWidget(run);
    toolbar->addWidget(check);
    toolbar->addWidget(debug);
    layout->addWidget(top);

    centerStack_ = new QStackedWidget;
    centerStack_->addWidget(buildWelcomePage());
    tabs_ = new QTabWidget;
    tabs_->setTabsClosable(true);
    tabs_->setMovable(true);
    tabs_->setDocumentMode(true);
    tabs_->tabBar()->setElideMode(Qt::ElideRight);
    connect(tabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int) {
        if (auto *editor = currentEditor()) updateStatusForEditor(editor);
        showWelcomeIfNeeded();
        refreshAssistantForCurrent();
    });
    connect(tabs_->tabBar(), &QTabBar::tabBarClicked, this, [this](int) { refreshTabCloseButtons(); });
    centerStack_->addWidget(tabs_);
    layout->addWidget(centerStack_, 1);
    return region;
}

QWidget *MainWindow::buildBottomPanel() {
    bottomPanel_ = new QFrame;
    bottomPanel_->setObjectName("bottomPanel");
    auto *layout = new QVBoxLayout(bottomPanel_);
    layout->setContentsMargins(0, 0, 0, 0);
    bottomTabs_ = new QTabWidget;
    problemList_ = new QListWidget;
    problemList_->setObjectName("problemList");
    output_ = new QPlainTextEdit;
    output_->setObjectName("outputPanel");
    output_->setReadOnly(true);
    output_->setMaximumBlockCount(5000);
    terminal_ = new TerminalWidget;
    bottomTabs_->addTab(problemList_, "Problems");
    bottomTabs_->addTab(output_, "Output");
    bottomTabs_->addTab(terminal_, "Terminal");
    connect(problemList_, &QListWidget::itemActivated, this,
            [this](QListWidgetItem *) { handleProblemsActivated(); });
    layout->addWidget(bottomTabs_);
    return bottomPanel_;
}

void MainWindow::createActions() {
    const auto bindings = settings_.keybindings();
    auto add = [this, &bindings](const QString &id, const QString &text,
                                 std::function<void()> fn) {
        auto *action = new QAction(text, this);
        if (bindings.contains(id) && !bindings[id].isEmpty())
            action->setShortcut(QKeySequence(bindings[id]));
        connect(action, &QAction::triggered, this, [fn] { fn(); });
        addAction(action);
        actions_[id] = action;
        return action;
    };

    add("file.new", "New File", [this] { createFileAt(projectRoot_); });
    add("file.open", "Open File…", [this] { chooseFile(); });
    add("file.openFolder", "Open Folder…", [this] { chooseFolder(); });
    add("file.save", "Save", [this] { saveCurrent(); });
    add("file.saveAll", "Save All", [this] { saveAll(); });
    add("editor.find", "Find", [this] { setSidebarPage(1, "search"); searchBox_->setFocus(); });
    add("editor.complete", "Trigger Completion", [this] { requestCompletion(); });
    add("editor.format", "Format Document", [this] { formatCurrent(); });
    add("editor.comment", "Toggle Line Comment",
        [this] { if (auto *e = currentEditor()) e->toggleLineComment(); });
    add("editor.indent", "Indent Lines",
        [this] { if (auto *e = currentEditor()) e->indentSelection(); });
    add("editor.dedent", "Outdent Lines",
        [this] { if (auto *e = currentEditor()) e->dedentSelection(); });
    add("editor.duplicate", "Duplicate Selection",
        [this] { if (auto *e = currentEditor()) e->duplicateSelection(); });
    add("editor.moveUp", "Move Lines Up",
        [this] { if (auto *e = currentEditor()) e->moveSelectedLines(-1); });
    add("editor.moveDown", "Move Lines Down",
        [this] { if (auto *e = currentEditor()) e->moveSelectedLines(1); });
    add("run.run", "Run", [this] { runCurrent(); });
    add("run.debug", "Debug", [this] { debugCurrent(); });
    add("run.check", "Check", [this] { checkCurrent(); });
    add("view.terminal", "Toggle Terminal", [this] { toggleBottomPanel(2); });
    add("view.assistant", "Toggle Code Assistant", [this] { toggleAssistant(); });
}

void MainWindow::buildMenus() {
    auto *file = menuBar()->addMenu("File");
    file->addAction(actions_["file.new"]);
    file->addAction(actions_["file.open"]);
    file->addAction(actions_["file.openFolder"]);
    file->addSeparator();
    file->addAction(actions_["file.save"]);
    file->addAction(actions_["file.saveAll"]);
    file->addSeparator();
    auto *quit = file->addAction("Exit");
    connect(quit, &QAction::triggered, this, &QWidget::close);

    auto *edit = menuBar()->addMenu("Edit");
    edit->addAction(actions_["editor.find"]);
    edit->addAction(actions_["editor.complete"]);
    edit->addAction(actions_["editor.format"]);
    edit->addSeparator();
    edit->addAction(actions_["editor.comment"]);
    edit->addAction(actions_["editor.indent"]);
    edit->addAction(actions_["editor.dedent"]);
    edit->addSeparator();
    edit->addAction(actions_["editor.duplicate"]);
    edit->addAction(actions_["editor.moveUp"]);
    edit->addAction(actions_["editor.moveDown"]);

    auto *run = menuBar()->addMenu("Run");
    run->addAction(actions_["run.run"]);
    run->addAction(actions_["run.debug"]);
    run->addAction(actions_["run.check"]);

    auto *view = menuBar()->addMenu("View");
    view->addAction(actions_["view.terminal"]);
    view->addAction(actions_["view.assistant"]);
    auto *explorer = view->addAction("Explorer");
    connect(explorer, &QAction::triggered, this, [this] { setSidebarPage(0, "explorer"); });
    auto *search = view->addAction("Search");
    connect(search, &QAction::triggered, this, [this] { setSidebarPage(1, "search"); });

    auto *tools = menuBar()->addMenu("Tools");
    auto *packages = tools->addAction("PPX Package Manager…");
    connect(packages, &QAction::triggered, this, &MainWindow::openPackages);
    auto *doctor = tools->addAction("Environment Doctor");
    connect(doctor, &QAction::triggered, this, &MainWindow::runEnvironmentDoctor);
    auto *toolchain = tools->addAction("PunPun Toolchain…");
    connect(toolchain, &QAction::triggered, this, &MainWindow::openToolchain);
    auto *settings = tools->addAction("Settings…");
    connect(settings, &QAction::triggered, this, &MainWindow::openSettings);
}

void MainWindow::applySettings() {
    qApp->setStyleSheet(Theme::stylesheet(settings_.theme()));
    refreshActivityIcons();
    for (int i = 0; i < tabs_->count(); ++i) {
        if (auto *editor = editorAt(i)) configureEditor(editor);
    }
    if (terminal_) terminal_->setFontSize(settings_.terminalFontSize());
    refreshTabCloseButtons();
    const auto bindings = settings_.keybindings();
    for (auto it = actions_.begin(); it != actions_.end(); ++it) {
        if (bindings.contains(it.key()))
            it.value()->setShortcut(QKeySequence(bindings[it.key()]));
    }
}

void MainWindow::configureEditor(CodeEditor *editor) {
    if (!editor) return;
    editor->configure(settings_.fontSize(), settings_.fontFamily(),
                      settings_.tabWidth(), settings_.wordWrap(), &scripts_);
    editor->setEditingBehaviour(settings_.autoIndent(), settings_.autoCloseBrackets(),
                                settings_.highlightCurrentLine(), settings_.showWhitespace(),
                                settings_.trimTrailingWhitespace(),
                                settings_.insertFinalNewline());
    const auto palette = Theme::palette(settings_.theme());
    editor->setEditorPalette(palette.lineHighlight, palette.selection,
                             palette.muted, palette.accent);
}

void MainWindow::refreshTabCloseButtons() {
    // Qt's stock close button renders as a stark red cross under this
    // stylesheet, which reads as an error badge on every open tab. Supply a
    // muted glyph from the active theme that only brightens on hover.
    const auto palette = Theme::palette(settings_.theme());
    auto *bar = tabs_->tabBar();
    for (int index = 0; index < bar->count(); ++index) {
        auto *existing = qobject_cast<QToolButton *>(
            bar->tabButton(index, QTabBar::RightSide));
        if (!existing) {
            auto *button = new QToolButton(bar);
            button->setObjectName("tabCloseButton");
            button->setAutoRaise(true);
            button->setCursor(Qt::ArrowCursor);
            button->setToolTip("Close");
            button->setFixedSize(18, 18);
            button->setIconSize(QSize(11, 11));
            connect(button, &QToolButton::clicked, this, [this, bar, button] {
                for (int i = 0; i < bar->count(); ++i) {
                    if (bar->tabButton(i, QTabBar::RightSide) == button) {
                        closeTab(i);
                        return;
                    }
                }
            });
            bar->setTabButton(index, QTabBar::RightSide, button);
            existing = button;
        }
        existing->setIcon(IconUtils::tinted(":/icons/close.svg", palette.muted, 16));
    }
}

void MainWindow::refreshActivityIcons() {
    const auto palette = Theme::palette(settings_.theme());
    for (auto it = activityButtons_.begin(); it != activityButtons_.end(); ++it) {
        const QString path = it.value()->property("iconPath").toString();
        if (!path.isEmpty()) {
            const QColor color = it.value()->isChecked() ? palette.accent : palette.muted;
            it.value()->setIcon(IconUtils::tinted(path, color, 24));
        }
    }
    for (auto *button : tintedButtons_) {
        const QString path = button->property("iconPath").toString();
        if (!path.isEmpty()) button->setIcon(IconUtils::tinted(path, palette.muted, 20));
    }
}

void MainWindow::setSidebarPage(int index, const QString &id) {
    sideStack_->setCurrentIndex(index);
    for (auto it = activityButtons_.begin(); it != activityButtons_.end(); ++it) {
        if (it.key() != "settings") it.value()->setChecked(it.key() == id);
    }
    sideStack_->show();
    refreshActivityIcons();
}

void MainWindow::openProject(const QString &path, bool remember) {
    const QFileInfo info(path);
    if (!info.isDir()) return;
    projectRoot_ = info.absoluteFilePath();
    if (remember) settings_.setLastProject(projectRoot_);
    projectLabel_->setText(info.fileName().isEmpty() ? projectRoot_ : info.fileName().toUpper());
    projectLabel_->setToolTip(projectRoot_);
    fsModel_->setRootPath(projectRoot_);
    tree_->setRootIndex(fsModel_->index(projectRoot_));
    statusProject_->setText(info.fileName());
    terminal_->start(projectRoot_, toolchain_.privateBinDir());
    scripts_.reload();
    refreshExtensions();
    startLanguageService();
    runEnvironmentDoctor();
}

void MainWindow::chooseFolder() {
    const QString path = QFileDialog::getExistingDirectory(
        this, "Open Folder", projectRoot_.isEmpty() ? QDir::homePath() : projectRoot_);
    if (!path.isEmpty()) openProject(path);
}

void MainWindow::chooseFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open File", projectRoot_.isEmpty() ? QDir::homePath() : projectRoot_,
        "Source files (*.pp *.c *.cc *.cpp *.cxx *.h *.hpp *.md *.markdown *.json *.js *.py);;All files (*)");
    if (path.isEmpty()) return;
    if (projectRoot_.isEmpty()) openProject(QFileInfo(path).absolutePath(), false);
    openFile(path);
}

QString MainWindow::readText(const QString &path, QString *error) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return {};
    }
    if (file.size() > 20 * 1024 * 1024) {
        if (error) *error = "File is larger than the 20 MiB editor safety limit";
        return {};
    }
    const QByteArray bytes = file.readAll();
    if (bytes.startsWith("\xEF\xBB\xBF")) return QString::fromUtf8(bytes.mid(3));
    if (bytes.size() >= 2 && uchar(bytes[0]) == 0xFF && uchar(bytes[1]) == 0xFE) {
        return QString::fromUtf16(reinterpret_cast<const char16_t *>(bytes.constData() + 2),
                                  qsizetype((bytes.size() - 2) / 2));
    }
    if (bytes.size() >= 2 && uchar(bytes[0]) == 0xFE && uchar(bytes[1]) == 0xFF) {
        QByteArray littleEndian;
        littleEndian.resize(bytes.size() - 2);
        for (int i = 2; i + 1 < bytes.size(); i += 2) {
            littleEndian[i - 2] = bytes[i + 1];
            littleEndian[i - 1] = bytes[i];
        }
        return QString::fromUtf16(reinterpret_cast<const char16_t *>(littleEndian.constData()),
                                  qsizetype(littleEndian.size() / 2));
    }
    return QString::fromUtf8(bytes);
}

void MainWindow::openFile(const QString &path) {
    const QFileInfo info(path);
    if (!info.isFile()) return;
    const QString absolute = info.absoluteFilePath();
    for (int i = 0; i < tabs_->count(); ++i) {
        auto *editor = editorAt(i);
        if (editor && normalizedPath(editor->filePath()) == normalizedPath(absolute)) {
            tabs_->setCurrentIndex(i);
            centerStack_->setCurrentIndex(1);
            return;
        }
    }

    QString error;
    const QString text = readText(absolute, &error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, "Open File", error);
        return;
    }

    auto *editor = new CodeEditor;
    editor->setFilePath(absolute);
    editor->setPlainText(text);
    editor->document()->setModified(false);
    configureEditor(editor);

    QWidget *page = editor;
    if (editor->language() == PPIDE_LANG_MARKDOWN) {
        auto *split = new QSplitter(Qt::Horizontal);
        split->addWidget(editor);
        auto *preview = new QTextBrowser;
        preview->setObjectName("markdownPreview");
        preview->setOpenExternalLinks(true);
        preview->document()->setMarkdown(text);
        split->addWidget(preview);
        split->setSizes({720, 500});
        connect(editor, &QPlainTextEdit::textChanged, preview,
                [editor, preview] { preview->document()->setMarkdown(editor->toPlainText()); });
        page = split;
    }

    pageEditors_[page] = editor;
    const QIcon tabIcon = fileIconProvider_ ? fileIconProvider_->icon(info) : QIcon();
    const int index = tabs_->addTab(page, tabIcon, info.fileName());
    tabs_->setTabToolTip(index, absolute);
    refreshTabCloseButtons();
    tabs_->setCurrentIndex(index);
    centerStack_->setCurrentIndex(1);

    connect(editor->document(), &QTextDocument::modificationChanged,
            this, [this, editor] { updateTabTitle(editor); });
    connect(editor, &CodeEditor::cursorLocationChanged, this,
            [this, editor](int line, int column) {
                if (editor == currentEditor())
                    statusCursor_->setText(QString("Ln %1, Col %2").arg(line).arg(column));
            });
    connect(editor, &CodeEditor::hoverRequested, this, &MainWindow::requestHover);
    connect(editor, &CodeEditor::hoverDismissRequested, hoverCard_, &QWidget::hide);
    connect(editor, &CodeEditor::contentChangedForLanguageService, this,
            [this, editor](const QString &documentPath, const QString &contents) {
                if (lsp_ && lsp_->running() && editor->language() == PPIDE_LANG_PUNPUN)
                    lsp_->changeDocument(documentPath, contents);
                scheduleSmartCheck(editor);
                if (settings_.autoSave()) {
                    auto *timer = autoSaveTimers_.value(editor);
                    if (!timer) {
                        timer = new QTimer(editor);
                        timer->setSingleShot(true);
                        timer->setInterval(1200);
                        connect(timer, &QTimer::timeout, this,
                                [this, editor] { if (editor) saveEditor(editor); });
                        autoSaveTimers_[editor] = timer;
                    }
                    timer->start();
                }
            });

    if (editor->language() == PPIDE_LANG_PUNPUN && lsp_ && lsp_->running())
        lsp_->openDocument(absolute, text);

    updateStatusForEditor(editor);
    runSmartCheck(editor);
    editor->setFocus();
}

CodeEditor *MainWindow::editorAt(int index) const {
    if (index < 0 || index >= tabs_->count()) return nullptr;
    QWidget *page = tabs_->widget(index);
    if (auto *editor = qobject_cast<CodeEditor *>(page)) return editor;
    return pageEditors_.value(page, nullptr);
}

CodeEditor *MainWindow::currentEditor() const {
    return editorAt(tabs_->currentIndex());
}

int MainWindow::tabForEditor(CodeEditor *editor) const {
    for (int i = 0; i < tabs_->count(); ++i)
        if (editorAt(i) == editor) return i;
    return -1;
}

bool MainWindow::saveEditor(CodeEditor *editor, bool forceDialog) {
    if (!editor) return false;
    QString path = editor->filePath();
    if (forceDialog || path.isEmpty()) {
        path = QFileDialog::getSaveFileName(
            this, "Save File",
            projectRoot_.isEmpty() ? QDir::homePath() + "/untitled.pp"
                                   : projectRoot_ + "/untitled.pp");
        if (path.isEmpty()) return false;
        editor->setFilePath(path);
        configureEditor(editor);
    }

    // Normalise the buffer before it reaches disk, so what is saved is exactly
    // what the editor shows afterwards.
    editor->prepareForSave();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Save File", file.errorString());
        return false;
    }
    const QByteArray data = editor->toPlainText().toUtf8();
    if (file.write(data) != data.size()) {
        QMessageBox::warning(this, "Save File", file.errorString());
        return false;
    }
    editor->document()->setModified(false);
    updateTabTitle(editor);
    return true;
}

void MainWindow::saveCurrent() { saveEditor(currentEditor()); }

void MainWindow::saveAll() {
    for (int i = 0; i < tabs_->count(); ++i) {
        if (auto *editor = editorAt(i); editor && editor->document()->isModified())
            saveEditor(editor);
    }
}

bool MainWindow::canCloseEditor(CodeEditor *editor) {
    if (!editor || !editor->document()->isModified()) return true;
    const auto answer = QMessageBox::question(
        this, "Unsaved changes",
        "Save changes to " + QFileInfo(editor->filePath()).fileName() + "?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) return saveEditor(editor);
    return true;
}

void MainWindow::closeTab(int index) {
    auto *editor = editorAt(index);
    if (!canCloseEditor(editor)) return;
    QWidget *page = tabs_->widget(index);
    if (editor) {
        smartDiagnostics_.remove(normalizedPath(editor->filePath()));
        compilerDiagnostics_.remove(normalizedPath(editor->filePath()));
    }
    pageEditors_.remove(page);
    tabs_->removeTab(index);
    page->deleteLater();
    rebuildProblems();
    showWelcomeIfNeeded();
    refreshAssistantForCurrent();
}

void MainWindow::updateTabTitle(CodeEditor *editor) {
    const int index = tabForEditor(editor);
    if (index < 0) return;
    QString title = QFileInfo(editor->filePath()).fileName();
    if (editor->document()->isModified()) title = "● " + title;
    tabs_->setTabText(index, title);
}

void MainWindow::updateStatusForEditor(CodeEditor *editor) {
    if (!editor) return;
    statusLanguage_->setText(QString::fromUtf8(ppide_language_name(editor->language())));
    const auto cursor = editor->textCursor();
    statusCursor_->setText(QString("Ln %1, Col %2")
                               .arg(cursor.blockNumber() + 1)
                               .arg(cursor.positionInBlock() + 1));
}

void MainWindow::showWelcomeIfNeeded() {
    centerStack_->setCurrentIndex(tabs_->count() ? 1 : 0);
}

QString MainWindow::contextDirectory(const QModelIndex &index) const {
    if (!index.isValid()) return projectRoot_;
    const QFileInfo info(fsModel_->filePath(index));
    return info.isDir() ? info.absoluteFilePath() : info.absolutePath();
}

void MainWindow::showExplorerMenu(const QPoint &pos) {
    if (projectRoot_.isEmpty()) return;
    const QModelIndex index = tree_->indexAt(pos);
    const QString directory = contextDirectory(index);
    QMenu menu;
    auto *newPunPun = menu.addAction("New PunPun File");
    auto *newFile = menu.addAction("New File…");
    auto *newFolder = menu.addAction("New Folder…");
    QAction *rename = nullptr;
    QAction *remove = nullptr;
    if (index.isValid()) {
        menu.addSeparator();
        rename = menu.addAction("Rename…");
        remove = menu.addAction("Delete");
    }
    QAction *chosen = menu.exec(tree_->viewport()->mapToGlobal(pos));
    if (chosen == newPunPun) createFileAt(directory, "main.pp");
    else if (chosen == newFile) createFileAt(directory);
    else if (chosen == newFolder) createFolderAt(directory);
    else if (chosen == rename) renameIndex(index);
    else if (chosen == remove) deleteIndex(index);
}

void MainWindow::createFileAt(const QString &directory, const QString &suggestion) {
    if (directory.isEmpty()) return;
    bool ok = false;
    QString name = QInputDialog::getText(this, "New File", "File name", QLineEdit::Normal,
                                         suggestion.isEmpty() ? "untitled.pp" : suggestion, &ok);
    name = cleanName(name);
    if (!ok || name.isEmpty()) return;
    const QString path = QDir(directory).filePath(name);
    if (QFileInfo::exists(path)) {
        QMessageBox::warning(this, "New File", "That name already exists.");
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "New File", file.errorString());
        return;
    }
    file.close();
    openFile(path);
}

void MainWindow::createFolderAt(const QString &directory) {
    if (directory.isEmpty()) return;
    bool ok = false;
    const QString name = cleanName(QInputDialog::getText(
        this, "New Folder", "Folder name", QLineEdit::Normal, "new-folder", &ok));
    if (!ok || name.isEmpty()) return;
    if (!QDir(directory).mkdir(name))
        QMessageBox::warning(this, "New Folder", "Could not create the folder.");
}

void MainWindow::renameIndex(const QModelIndex &index) {
    if (!index.isValid()) return;
    const QString oldPath = fsModel_->filePath(index);
    const QFileInfo info(oldPath);
    bool ok = false;
    const QString name = cleanName(QInputDialog::getText(
        this, "Rename", "New name", QLineEdit::Normal, info.fileName(), &ok));
    if (!ok || name.isEmpty() || name == info.fileName()) return;
    const QString newPath = QDir(info.absolutePath()).filePath(name);
    if (QFileInfo::exists(newPath)) {
        QMessageBox::warning(this, "Rename", "That name already exists.");
        return;
    }
    const bool success = info.isDir()
        ? QDir(info.absolutePath()).rename(info.fileName(), name)
        : QFile::rename(oldPath, newPath);
    if (!success) QMessageBox::warning(this, "Rename", "Could not rename the item.");
}

void MainWindow::deleteIndex(const QModelIndex &index) {
    if (!index.isValid()) return;
    const QString path = fsModel_->filePath(index);
    const QFileInfo info(path);
    if (QMessageBox::warning(this, "Delete",
                             QString("Delete %1?\n\nThis cannot be undone by PunPun IDE.")
                                 .arg(info.fileName()),
                             QMessageBox::Yes | QMessageBox::No,
                             QMessageBox::No) != QMessageBox::Yes)
        return;
    const bool ok = info.isDir() ? QDir(path).removeRecursively() : QFile::remove(path);
    if (!ok) QMessageBox::warning(this, "Delete", "Could not delete the item.");
}

void MainWindow::runSearch() {
    searchResults_->clear();
    const QString needle = searchBox_->text().trimmed();
    if (needle.isEmpty() || projectRoot_.isEmpty()) return;
    QDirIterator iterator(projectRoot_, QDir::Files, QDirIterator::Subdirectories);
    int hits = 0;
    while (iterator.hasNext() && hits < 250) {
        const QString path = iterator.next();
        const QString relative = QDir(projectRoot_).relativeFilePath(path);
        if (relative.startsWith(".git/") || relative.contains("/.git/") ||
            relative.startsWith(".punpun-ide/") || relative.contains("/node_modules/"))
            continue;
        bool found = relative.contains(needle, Qt::CaseInsensitive);
        if (!found && QFileInfo(path).size() < 2 * 1024 * 1024) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly))
                found = QString::fromUtf8(file.readAll()).contains(needle, Qt::CaseInsensitive);
        }
        if (found) {
            auto *item = new QListWidgetItem(relative);
            item->setData(Qt::UserRole, path);
            searchResults_->addItem(item);
            ++hits;
        }
    }
}

void MainWindow::refreshExtensions() {
    extensionList_->clear();
    for (const auto &name : scripts_.loadedExtensions()) extensionList_->addItem("✓  " + name);
    if (extensionList_->count() == 0) extensionList_->addItem("No extensions loaded");
}

void MainWindow::toggleBottomPanel(int tab) {
    if (tab >= 0) {
        bottomPanel_->show();
        bottomTabs_->setCurrentIndex(tab);
        return;
    }
    bottomPanel_->setVisible(!bottomPanel_->isVisible());
}

void MainWindow::toggleAssistant() {
    assistant_->setVisible(!assistant_->isVisible());
}

void MainWindow::appendOutput(const QString &text) {
    if (text.isEmpty()) return;
    output_->moveCursor(QTextCursor::End);
    output_->insertPlainText(text.endsWith('\n') ? text : text + '\n');
    output_->moveCursor(QTextCursor::End);
}

void MainWindow::appendProblem(const QString &path, const EditorDiagnostic &diagnostic) {
    QString glyph = diagnostic.severity <= 1 ? "●" : diagnostic.severity == 2 ? "▲" : "◆";
    QString source = diagnostic.source.isEmpty() ? "Analyzer" : diagnostic.source;
    QString code = diagnostic.code.isEmpty() ? QString() : diagnostic.code + " ";
    auto *item = new QListWidgetItem(
        QString("%1  %2:%3  %4%5 · %6")
            .arg(glyph)
            .arg(QFileInfo(path).fileName())
            .arg(diagnostic.line + 1)
            .arg(code, diagnostic.message, source));
    QVariantMap data{{"path", path}, {"line", diagnostic.line}, {"column", diagnostic.column}};
    item->setData(Qt::UserRole, data);
    problemList_->addItem(item);
}

void MainWindow::rebuildProblems() {
    problemList_->clear();
    QSet<QString> paths;
    for (auto it = smartDiagnostics_.cbegin(); it != smartDiagnostics_.cend(); ++it) paths.insert(it.key());
    for (auto it = compilerDiagnostics_.cbegin(); it != compilerDiagnostics_.cend(); ++it) paths.insert(it.key());
    for (const auto &path : paths) {
        for (const auto &diagnostic : combinedDiagnostics(path)) appendProblem(path, diagnostic);
    }
    bottomTabs_->setTabText(0, problemList_->count() ? QString("Problems (%1)").arg(problemList_->count()) : "Problems");
}

void MainWindow::handleProblemsActivated() {
    auto *item = problemList_->currentItem();
    if (!item) return;
    const auto data = item->data(Qt::UserRole).toMap();
    openFile(data.value("path").toString());
    if (auto *editor = currentEditor())
        editor->gotoLine(data.value("line").toInt(), data.value("column").toInt());
}

QString MainWindow::compilerFor(int language, bool cpp) const {
    Q_UNUSED(language);
    const QStringList names = cpp ? QStringList{"clang++", "g++", "c++"}
                                  : QStringList{"clang", "gcc", "cc"};
    for (const auto &name : names) {
        const QString path = QStandardPaths::findExecutable(name);
        if (!path.isEmpty()) return path;
    }
    return {};
}

void MainWindow::runProcess(const QString &program, const QStringList &args,
                            const QString &cwd, const QString &title,
                            std::function<void(int, const QString &)> finished,
                            bool revealOutput) {
    if (program.isEmpty()) {
        if (revealOutput) {
            appendOutput(title + ": executable not found");
            toggleBottomPanel(1);
        }
        if (finished) finished(-1, title + ": executable not found");
        return;
    }

    auto *process = new QProcess(this);
    process->setWorkingDirectory(cwd);
    process->setProcessChannelMode(QProcess::MergedChannels);
    auto buffer = std::make_shared<QString>();
    if (revealOutput) {
        appendOutput(QString("\n[%1]\n❯ %2 %3\n").arg(title, program, args.join(' ')));
        toggleBottomPanel(1);
    }
    connect(process, &QProcess::readyReadStandardOutput, this,
            [this, process, buffer, revealOutput] {
                const QString text = QString::fromLocal8Bit(process->readAllStandardOutput());
                *buffer += text;
                if (revealOutput) appendOutput(text);
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process, buffer, title, revealOutput](QProcess::ProcessError) {
                const QString text = QString("%1: %2").arg(title, process->errorString());
                *buffer += text;
                if (revealOutput) appendOutput(text);
            });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process, buffer, finished, title, revealOutput](int code, QProcess::ExitStatus) {
                const QString tail = QString::fromLocal8Bit(process->readAllStandardOutput());
                *buffer += tail;
                if (revealOutput) {
                    if (!tail.isEmpty()) appendOutput(tail);
                    appendOutput(QString("[%1 exited %2]").arg(title).arg(code));
                }
                if (finished) finished(code, *buffer);
                process->deleteLater();
            });
    process->start(program, args);
}

void MainWindow::runCurrent() {
    auto *editor = currentEditor();
    if (!editor) return;
    if (editor->document()->isModified() && !saveEditor(editor)) return;

    const QString path = editor->filePath();
    const QString cwd = QFileInfo(path).absolutePath();
    if (editor->language() == PPIDE_LANG_PUNPUN) {
        const QString ppc = toolchain_.ppcPath();
        if (ppc.isEmpty()) {
            assistant_->setEnvironmentSummary("PunPun: compiler unavailable. Checking the latest stable toolchain now…");
            toolchain_.checkForUpdates(true);
            return;
        }
        bottomPanel_->show();
        bottomTabs_->setCurrentIndex(2);
        terminal_->runCommand(ppc, {"go", path}, cwd);
        return;
    }

    if (editor->language() == PPIDE_LANG_HEADER) {
        appendOutput("Header files are checked, not executed. Use Check or open a C/C++ translation unit.");
        toggleBottomPanel(1);
        return;
    }

    const bool cpp = editor->language() == PPIDE_LANG_CPP;
    if (editor->language() != PPIDE_LANG_C && !cpp) {
        appendOutput("This file type is not directly runnable.");
        toggleBottomPanel(1);
        return;
    }

    const QString compiler = compilerFor(editor->language(), cpp);
    const QString build = QDir(cwd).filePath(".punpun-ide/build");
    QDir().mkpath(build);
    QString outputPath = QDir(build).filePath(QFileInfo(path).completeBaseName());
#ifdef Q_OS_WIN
    outputPath += ".exe";
#endif
    runProcess(compiler, {path, "-O0", "-g", "-o", outputPath}, cwd, "Build",
               [this, outputPath, cwd](int code, const QString &) {
                   if (code == 0) {
                       bottomPanel_->show();
                       bottomTabs_->setCurrentIndex(2);
                       terminal_->runCommand(outputPath, {}, cwd);
                   }
               });
}

void MainWindow::checkCurrent() {
    auto *editor = currentEditor();
    if (!editor) return;
    runSmartCheck(editor);
    assistant_->setBusy(true);

    if (editor->language() == PPIDE_LANG_PUNPUN) {
        if (editor->document()->isModified() && !saveEditor(editor)) {
            assistant_->setBusy(false);
            return;
        }
        const QString ppc = toolchain_.ppcPath();
        const QString path = editor->filePath();
        runProcess(ppc, {"check", "--json", path}, QFileInfo(path).absolutePath(),
                   "Check PunPun",
                   [this, path](int, const QString &text) {
                       QVector<EditorDiagnostic> parsed;
                       const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8());
                       if (document.isArray()) {
                           for (const auto &entry : document.array()) {
                               const QJsonObject object = entry.toObject();
                               const QJsonArray labels = object.value("labels").toArray();
                               QJsonObject primary;
                               for (const auto &labelValue : labels) {
                                   const auto label = labelValue.toObject();
                                   if (label.value("primary").toBool()) { primary = label; break; }
                               }
                               if (primary.isEmpty() && !labels.isEmpty()) primary = labels.first().toObject();
                               EditorDiagnostic diagnostic;
                               diagnostic.line = qMax(0, primary.value("line").toInt(1) - 1);
                               diagnostic.column = qMax(0, primary.value("column").toInt(1) - 1);
                               const int endColumn = primary.value("end_column").toInt(diagnostic.column + 2) - 1;
                               diagnostic.length = qMax(1, endColumn - diagnostic.column);
                               diagnostic.severity = severityFromString(object.value("severity").toString());
                               diagnostic.message = object.value("message").toString();
                               diagnostic.code = object.value("code").toString();
                               diagnostic.source = "PPC";
                               diagnostic.suggestion = object.value("help").toString();
                               if (diagnostic.suggestion.isEmpty())
                                   diagnostic.suggestion = SmartAnalyzer::suggestionFor(PPIDE_LANG_PUNPUN, diagnostic.message);
                               parsed.push_back(diagnostic);
                           }
                           compilerDiagnostics_[normalizedPath(path)] = parsed;
                           refreshDiagnostics(path);
                       }
                       assistant_->setBusy(false);
                   });
        return;
    }

    if (editor->language() == PPIDE_LANG_C || editor->language() == PPIDE_LANG_CPP ||
        editor->language() == PPIDE_LANG_HEADER) {
        runNativeSyntaxCheck(editor, true);
        return;
    }

    assistant_->setBusy(false);
    appendOutput("No checker configured for this file type.");
}

void MainWindow::debugCurrent() {
    auto *editor = currentEditor();
    if (!editor) return;
    if (editor->document()->isModified() && !saveEditor(editor)) return;

    if (editor->language() == PPIDE_LANG_PUNPUN) {
        const QString pp = toolchain_.ppPath();
        if (pp.isEmpty()) {
            appendOutput("PunPun debug wrapper `pp` is unavailable in this toolchain.");
            return;
        }
        bottomPanel_->show();
        bottomTabs_->setCurrentIndex(2);
        terminal_->runCommand(pp, {"debug", editor->filePath()});
        return;
    }

    if (editor->language() == PPIDE_LANG_HEADER) {
        appendOutput("A header cannot be debugged by itself. Debug a C/C++ translation unit that includes it.");
        return;
    }

    const bool cpp = editor->language() == PPIDE_LANG_CPP;
    if (editor->language() != PPIDE_LANG_C && !cpp) return;
    const QString compiler = compilerFor(editor->language(), cpp);
    const QString cwd = QFileInfo(editor->filePath()).absolutePath();
    const QString build = QDir(cwd).filePath(".punpun-ide/build");
    QDir().mkpath(build);
    QString outputPath = QDir(build).filePath("debug-" + QFileInfo(editor->filePath()).completeBaseName());
#ifdef Q_OS_WIN
    outputPath += ".exe";
#endif
    runProcess(compiler, {editor->filePath(), "-g", "-O0", "-o", outputPath}, cwd,
               "Debug build", [this, outputPath](int code, const QString &) {
                   if (code != 0) return;
                   const QString gdb = QStandardPaths::findExecutable("gdb");
                   const QString lldb = QStandardPaths::findExecutable("lldb");
                   const QString debugger = !gdb.isEmpty() ? gdb : lldb;
                   if (debugger.isEmpty()) {
                       appendOutput("No gdb/lldb debugger found.");
                       return;
                   }
                   bottomPanel_->show();
                   bottomTabs_->setCurrentIndex(2);
                   terminal_->runCommand(debugger, {outputPath});
               });
}

void MainWindow::formatCurrent() {
    auto *editor = currentEditor();
    if (!editor) return;
    if (editor->document()->isModified() && !saveEditor(editor)) return;
    const QString path = editor->filePath();
    const QString cwd = QFileInfo(path).absolutePath();

    QString program;
    QStringList args;
    if (editor->language() == PPIDE_LANG_PUNPUN) {
        program = toolchain_.ppcPath();
        args = {"fmt", path};
    } else if (editor->language() == PPIDE_LANG_C || editor->language() == PPIDE_LANG_CPP ||
               editor->language() == PPIDE_LANG_HEADER) {
        program = QStandardPaths::findExecutable("clang-format");
        args = {"-i", path};
    } else {
        statusBar()->showMessage("No formatter configured for this file type", 3000);
        return;
    }

    if (program.isEmpty()) {
        statusBar()->showMessage("Formatter executable is not installed", 4000);
        return;
    }
    runProcess(program, args, cwd, "Format", [this, editor, path](int code, const QString &) {
        if (code != 0 || !editor) return;
        QString error;
        const QString formatted = readText(path, &error);
        if (!error.isEmpty()) return;
        const int oldPosition = editor->textCursor().position();
        editor->setPlainText(formatted);
        QTextCursor cursor = editor->textCursor();
        cursor.setPosition(qMin(oldPosition, int(formatted.size())));
        editor->setTextCursor(cursor);
        editor->document()->setModified(false);
        runSmartCheck(editor);
        statusBar()->showMessage("Formatted " + QFileInfo(path).fileName(), 2500);
    }, false);
}

void MainWindow::scheduleSmartCheck(CodeEditor *editor) {
    if (!editor) return;
    auto *timer = smartCheckTimers_.value(editor);
    if (!timer) {
        timer = new QTimer(editor);
        timer->setSingleShot(true);
        timer->setInterval(450);
        connect(timer, &QTimer::timeout, this, [this, editor] {
            if (editor) runSmartCheck(editor);
        });
        smartCheckTimers_[editor] = timer;
    }
    timer->start();
}

void MainWindow::runSmartCheck(CodeEditor *editor) {
    if (!editor || editor->filePath().isEmpty()) return;
    const QString key = normalizedPath(editor->filePath());
    smartDiagnostics_[key] = SmartAnalyzer::analyze(editor->language(), editor->toPlainText(), key);
    refreshDiagnostics(key);

    if (editor->language() == PPIDE_LANG_C || editor->language() == PPIDE_LANG_CPP ||
        editor->language() == PPIDE_LANG_HEADER)
        runNativeSyntaxCheck(editor, false);
}

void MainWindow::runNativeSyntaxCheck(CodeEditor *editor, bool revealOutput) {
    if (!editor) return;
    const bool cpp = editor->language() != PPIDE_LANG_C;
    const QString compiler = compilerFor(editor->language(), cpp);
    const QString path = normalizedPath(editor->filePath());
    if (compiler.isEmpty()) {
        if (revealOutput) appendOutput("No C/C++ compiler found on PATH.");
        assistant_->setBusy(false);
        return;
    }

    const quint64 generation = ++diagnosticGeneration_;
    nativeCheckGeneration_[path] = generation;
    auto *process = new QProcess(this);
    process->setWorkingDirectory(QFileInfo(path).absolutePath());
    process->setProcessChannelMode(QProcess::MergedChannels);
    auto buffer = std::make_shared<QByteArray>();
    QString language = cpp ? "c++" : "c";
    QStringList args{"-x", language, "-fsyntax-only", "-Wall", "-Wextra", "-Wpedantic",
                     "-fdiagnostics-color=never", "-"};
    if (cpp) args.insert(2, "-std=c++20");

    if (revealOutput) {
        appendOutput(QString("\n[Check %1]\n❯ %2 %3\n")
                         .arg(QFileInfo(path).fileName(), compiler, args.join(' ')));
        toggleBottomPanel(1);
    }
    connect(process, &QProcess::readyReadStandardOutput, this,
            [this, process, buffer, revealOutput] {
                const QByteArray bytes = process->readAllStandardOutput();
                *buffer += bytes;
                if (revealOutput) appendOutput(QString::fromLocal8Bit(bytes));
            });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process, buffer, path, generation, revealOutput](int code, QProcess::ExitStatus) {
                *buffer += process->readAllStandardOutput();
                if (nativeCheckGeneration_.value(path) == generation) {
                    compilerDiagnostics_[path] = parseNativeDiagnostics(QString::fromLocal8Bit(*buffer), path);
                    refreshDiagnostics(path);
                }
                if (revealOutput) appendOutput(QString("[syntax check exited %1]").arg(code));
                assistant_->setBusy(false);
                process->deleteLater();
            });
    const QByteArray source = editor->toPlainText().toUtf8();
    connect(process, &QProcess::started, process, [process, source] {
        process->write(source);
        process->closeWriteChannel();
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, process, revealOutput](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart) {
                    if (revealOutput) appendOutput("Could not start compiler: " + process->errorString());
                    assistant_->setBusy(false);
                }
            });
    process->start(compiler, args);
}

QVector<EditorDiagnostic> MainWindow::parseNativeDiagnostics(
    const QString &text, const QString &fallbackPath) const {
    QVector<EditorDiagnostic> result;
    const QRegularExpression rx(
        R"(^(.+?):(\d+):(\d+):\s*(fatal error|error|warning|note):\s*(.+)$)",
        QRegularExpression::MultilineOption);
    auto matches = rx.globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        QString compilerPath = match.captured(1);
        if (compilerPath == "<stdin>" || compilerPath == "-") compilerPath = fallbackPath;
        if (normalizedPath(compilerPath) != normalizedPath(fallbackPath) && QFileInfo(compilerPath).isAbsolute())
            continue;
        EditorDiagnostic diagnostic;
        diagnostic.line = qMax(0, match.captured(2).toInt() - 1);
        diagnostic.column = qMax(0, match.captured(3).toInt() - 1);
        diagnostic.length = 1;
        diagnostic.severity = match.captured(4).contains("warning") ? 2
                              : match.captured(4).contains("note") ? 3 : 1;
        diagnostic.message = match.captured(5).trimmed();
        const QRegularExpression warningCode(R"(\s*\[(-W[^\]]+)\]\s*$)");
        const auto warningMatch = warningCode.match(diagnostic.message);
        if (warningMatch.hasMatch()) {
            diagnostic.code = warningMatch.captured(1);
            diagnostic.message.remove(warningMatch.capturedStart(), warningMatch.capturedLength());
            diagnostic.message = diagnostic.message.trimmed();
        }
        diagnostic.source = "C/C++ compiler";
        diagnostic.suggestion = SmartAnalyzer::suggestionFor(PPIDE_LANG_CPP, diagnostic.message);
        result.push_back(diagnostic);
    }
    return result;
}

QVector<EditorDiagnostic> MainWindow::combinedDiagnostics(const QString &path) const {
    const QString key = normalizedPath(path);
    QVector<EditorDiagnostic> result = compilerDiagnostics_.value(key);
    result += smartDiagnostics_.value(key);
    std::stable_sort(result.begin(), result.end(), [](const EditorDiagnostic &a, const EditorDiagnostic &b) {
        if (a.severity != b.severity) return a.severity < b.severity;
        if (a.line != b.line) return a.line < b.line;
        return a.column < b.column;
    });
    return result;
}

void MainWindow::refreshDiagnostics(const QString &path) {
    const QString key = normalizedPath(path);
    const auto combined = combinedDiagnostics(key);
    for (int i = 0; i < tabs_->count(); ++i) {
        auto *editor = editorAt(i);
        if (editor && normalizedPath(editor->filePath()) == key) editor->applyDiagnostics(combined);
    }
    rebuildProblems();
    if (auto *editor = currentEditor(); editor && normalizedPath(editor->filePath()) == key)
        assistant_->setDiagnostics(key, combined);
}

void MainWindow::refreshAssistantForCurrent() {
    auto *editor = currentEditor();
    if (!editor) {
        assistant_->setDiagnostics(QString(), QVector<EditorDiagnostic>());
        return;
    }
    assistant_->setDiagnostics(normalizedPath(editor->filePath()), combinedDiagnostics(editor->filePath()));
}

void MainWindow::startLanguageService() {
    if (projectRoot_.isEmpty()) return;
    const QString ppc = toolchain_.ppcPath();
    const QString version = toolchain_.installedVersion();
    const QVersionNumber parsed = QVersionNumber::fromString(version);
    if (ppc.isEmpty() || parsed < QVersionNumber(1, 3, 0)) {
        statusPunPun_->setText(version.isEmpty() ? "PunPun: installing…"
                                                 : "PunPun: " + version + " · update needed");
        return;
    }

    if (lsp_) {
        lsp_->stop();
        lsp_->deleteLater();
    }
    lsp_ = new LspClient(this);
    connect(lsp_, &LspClient::log, this,
            [this](const QString &text) { appendOutput("[LSP] " + text); });
    connect(lsp_, &LspClient::diagnostics, this, &MainWindow::handleLspDiagnostics);
    connect(lsp_, &LspClient::hoverResult, this, [this](quint64 token, const QString &text) {
        if (token == pendingHoverToken_ && pendingHoverEditor_ &&
            !pendingHoverEditor_->textCursor().hasSelection() && !text.isEmpty())
            showHover(text, pendingHoverPosition_);
    });
    connect(lsp_, &LspClient::completionResult, this,
            [this](quint64 token, const QStringList &items) {
                if (token != pendingCompletionToken_ || !pendingCompletionEditor_ || items.isEmpty()) return;
                pendingCompletionEditor_->setCompletionItems(items);
                pendingCompletionEditor_->showCompletion();
            });
    connect(lsp_, &LspClient::stopped, this,
            [this] { statusPunPun_->setText("PunPun LSP: stopped"); });

    if (lsp_->start(ppc, projectRoot_)) {
        statusPunPun_->setText("PunPun " + version);
        reopenPunPunDocumentsInLsp();
    } else {
        statusPunPun_->setText("PunPun LSP unavailable");
    }
}

void MainWindow::reopenPunPunDocumentsInLsp() {
    if (!lsp_ || !lsp_->running()) return;
    for (int i = 0; i < tabs_->count(); ++i) {
        auto *editor = editorAt(i);
        if (editor && editor->language() == PPIDE_LANG_PUNPUN)
            lsp_->openDocument(editor->filePath(), editor->toPlainText());
    }
}

void MainWindow::handleLspDiagnostics(const QString &path, const QJsonArray &items) {
    QVector<EditorDiagnostic> diagnostics;
    for (const auto &value : items) {
        const QJsonObject object = value.toObject();
        const QJsonObject range = object.value("range").toObject();
        const QJsonObject start = range.value("start").toObject();
        const QJsonObject end = range.value("end").toObject();
        EditorDiagnostic diagnostic;
        diagnostic.line = start.value("line").toInt();
        diagnostic.column = start.value("character").toInt();
        diagnostic.length = qMax(1, end.value("character").toInt() - diagnostic.column);
        diagnostic.severity = object.value("severity").toInt(1);
        diagnostic.message = object.value("message").toString();
        diagnostic.code = object.value("code").toVariant().toString();
        diagnostic.source = "PPC";
        diagnostic.suggestion = SmartAnalyzer::suggestionFor(PPIDE_LANG_PUNPUN, diagnostic.message);
        diagnostics.push_back(diagnostic);
    }
    compilerDiagnostics_[normalizedPath(path)] = diagnostics;
    refreshDiagnostics(path);
}

QString MainWindow::localHover(CodeEditor *editor, const QString &word) const {
    if (!editor || word.isEmpty()) return {};
    const QString escaped = QRegularExpression::escape(word);
    const QRegularExpression functionPattern(
        QString(R"((?:fn|craft)\s+%1\s*\(([^)]*)\)(?:\s*->\s*([^\s{]+)|\s+gives\s+([^:\s]+))?)")
            .arg(escaped));
    const auto match = functionPattern.match(editor->toPlainText());
    if (match.hasMatch())
        return QString("```punpun\n%1\n```\n\nFunction declared in this file.").arg(match.captured(0));

    static const QHash<QString, QString> help{
        {"launch", "Program entry block."},
        {"craft", "Function declaration in PunPun's migration syntax."},
        {"fn", "Function declaration."},
        {"match", "Pattern-match a value."},
        {"await", "Wait for an async task result."},
        {"say", "Write a value to standard output."},
        {"str", "UTF-8 text string type. `str` is the stable API spelling."},
        {"String", "Source-level string type used by the PunPun standard-library wrappers."},
        {"Result", "A success/error value. Handle both outcomes instead of hiding failures."},
        {"Option", "An optional value that may be present or absent."},
        {"bring", "Import a PunPun module into the current file."}
    };
    return help.value(word);
}

void MainWindow::requestHover(const QString &path, int line, int column,
                              const QString &word, const QPoint &globalPos) {
    auto *editor = qobject_cast<CodeEditor *>(sender());
    if (!editor || editor->textCursor().hasSelection()) return;
    pendingHoverEditor_ = editor;
    pendingHoverPosition_ = globalPos;

    // Diagnostics win over symbol hover. When the pointer is on a squiggle,
    // explain the problem right there instead of forcing a trip to Problems.
    for (const auto &diagnostic : editor->diagnostics()) {
        const int start = diagnostic.column;
        const int end = start + qMax(1, diagnostic.length);
        const bool overlapsWord = diagnostic.line == line &&
                                  column <= end &&
                                  column + qMax(1, word.size()) >= start;
        if (!overlapsWord) continue;

        const QString severity = diagnostic.severity <= 1 ? "Error"
                               : diagnostic.severity == 2 ? "Warning" : "Hint";
        QString markdown = QString("**%1%2** · %3\n\n%4")
                               .arg(severity,
                                    diagnostic.code.isEmpty() ? QString()
                                                              : QString(" · `%1`").arg(diagnostic.code),
                                    diagnostic.source.isEmpty() ? "Analyzer" : diagnostic.source,
                                    diagnostic.message);
        if (!diagnostic.suggestion.isEmpty())
            markdown += "\n\n---\n\n**Suggested next step**\n\n" + diagnostic.suggestion;
        showHover(markdown, globalPos);
        return;
    }

    const QString local = localHover(editor, word);
    if (!local.isEmpty()) showHover(local, globalPos);
    if (editor->language() == PPIDE_LANG_PUNPUN && lsp_ && lsp_->running())
        pendingHoverToken_ = lsp_->requestHover(path, line, column);
}

void MainWindow::showHover(const QString &markdown, const QPoint &globalPos) {
    if (!pendingHoverEditor_ || pendingHoverEditor_->textCursor().hasSelection()) return;
    hoverText_->document()->setMarkdown(markdown);
    hoverCard_->adjustSize();
    QPoint position = globalPos + QPoint(12, 18);
    QScreen *screenAtPoint = QApplication::screenAt(globalPos);
    const QRect screen = screenAtPoint ? screenAtPoint->availableGeometry() : QRect();
    if (!screen.isNull()) {
        if (position.x() + hoverCard_->width() > screen.right())
            position.setX(screen.right() - hoverCard_->width() - 8);
        if (position.y() + hoverCard_->height() > screen.bottom())
            position.setY(globalPos.y() - hoverCard_->height() - 8);
    }
    hoverCard_->move(position);
    hoverCard_->show();
}

void MainWindow::requestCompletion() {
    auto *editor = currentEditor();
    if (!editor) return;
    if (editor->language() == PPIDE_LANG_PUNPUN && lsp_ && lsp_->running()) {
        const auto cursor = editor->textCursor();
        pendingCompletionEditor_ = editor;
        pendingCompletionToken_ = lsp_->requestCompletion(
            editor->filePath(), cursor.blockNumber(), cursor.positionInBlock());
    } else {
        editor->showCompletion();
    }
}

QString MainWindow::doctorResourcePath() const {
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/doctor";
    QDir().mkpath(directory);
    const QString destination = directory + "/doctor.pp";
    QFile source(":/punpun/doctor.pp");
    if (!source.open(QIODevice::ReadOnly)) return {};
    QFile output(destination);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};
    output.write(source.readAll());
    output.close();
    return destination;
}

QString MainWindow::environmentSummaryFromDoctor(const QString &text) const {
    QStringList lines = text.split('\n', Qt::KeepEmptyParts);
    for (auto &line : lines) line = line.trimmed();
    const int marker = lines.indexOf("PUNPUN_IDE_DOCTOR_V1");
    if (marker < 0 || lines.size() < marker + 8) return {};
    const QString platform = lines.value(marker + 1);
    const QString cwd = lines.value(marker + 2);
    const QString runtime = lines.value(marker + 4);
    const QString stdlib = lines.value(marker + 5);
    const QString cc = lines.value(marker + 6);
    const QString cxx = lines.value(marker + 7);
    QStringList pieces;
    pieces << QString("Environment · %1").arg(platform.isEmpty() ? "unknown platform" : platform);
    pieces << QString("PPC %1").arg(toolchain_.installedVersion().isEmpty() ? "unknown" : toolchain_.installedVersion());
    pieces << QString("cwd: %1").arg(cwd);
    pieces << QString("runtime: %1").arg(runtime.isEmpty() ? "auto" : runtime);
    pieces << QString("stdlib: %1").arg(stdlib.isEmpty() ? "auto" : stdlib);
    pieces << QString("CC: %1 · CXX: %2")
                  .arg(cc.isEmpty() ? "auto" : cc, cxx.isEmpty() ? "auto" : cxx);
    return pieces.join("\n");
}

void MainWindow::runEnvironmentDoctor() {
    if (!assistant_) return;
    const QString ppc = toolchain_.ppcPath();
    if (ppc.isEmpty()) {
        assistant_->setEnvironmentSummary("Environment · PunPun compiler not installed\nUpdater is checking the latest stable release.");
        toolchain_.checkForUpdates(false);
        return;
    }
    const QString doctor = doctorResourcePath();
    if (doctor.isEmpty()) {
        assistant_->setEnvironmentSummary("Environment · doctor resource unavailable");
        return;
    }
    const QString cwd = projectRoot_.isEmpty() ? QDir::homePath() : projectRoot_;
    runProcess(ppc, {"run", doctor}, cwd, "PunPun environment doctor",
               [this](int code, const QString &text) {
                   const QString summary = environmentSummaryFromDoctor(text);
                   if (code == 0 && !summary.isEmpty()) assistant_->setEnvironmentSummary(summary);
                   else assistant_->setEnvironmentSummary(
                       "Environment · PunPun doctor could not run\nOpen Output or Toolchain to inspect the compiler installation.");
               }, false);
}

void MainWindow::openSettings() {
    SettingsDialog dialog(&settings_, this);
    connect(&dialog, &SettingsDialog::settingsChanged, this, [this] {
        applySettings();
        toolchain_.startAutomaticChecks();
    });
    dialog.exec();
}

void MainWindow::openToolchain() {
    ToolchainDialog dialog(&toolchain_, this);
    dialog.exec();
}

void MainWindow::openPackages() {
    PackageManagerDialog dialog(&toolchain_, projectRoot_, this);
    dialog.exec();
}

void MainWindow::configureUpdateSignals() {
    connect(&toolchain_, &ToolchainManager::status, this,
            [this](const QString &text) { statusPunPun_->setText(text); });
    connect(&toolchain_, &ToolchainManager::updateAvailable, this,
            [this](const QString &, const QString &latest) {
                statusPunPun_->setText("PunPun: updating " + latest);
                assistant_->setEnvironmentSummary("Environment · installing PunPun " + latest + "…");
            });
    connect(&toolchain_, &ToolchainManager::installed, this,
            [this](const QString &tag) {
                statusPunPun_->setText("PunPun " + tag);
                terminal_->start(projectRoot_, toolchain_.privateBinDir());
                startLanguageService();
                runEnvironmentDoctor();
            });
    connect(&toolchain_, &ToolchainManager::error, this,
            [this](const QString &error) {
                appendOutput("[Updater] " + error);
                statusPunPun_->setText("PunPun update error");
                assistant_->setEnvironmentSummary("Environment · updater error\n" + error);
            });
}

QString MainWindow::languageKey(int language) {
    switch (language) {
        case PPIDE_LANG_PUNPUN: return "punpun";
        case PPIDE_LANG_C: return "c";
        case PPIDE_LANG_CPP: return "cpp";
        case PPIDE_LANG_HEADER: return "header";
        case PPIDE_LANG_MARKDOWN: return "markdown";
        case PPIDE_LANG_JSON: return "json";
        case PPIDE_LANG_JAVASCRIPT: return "javascript";
        case PPIDE_LANG_PYTHON: return "python";
        default: return "text";
    }
}

QString MainWindow::normalizedPath(const QString &path) {
    if (path.isEmpty()) return {};
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    for (int i = tabs_->count() - 1; i >= 0; --i) {
        if (!canCloseEditor(editorAt(i))) {
            event->ignore();
            return;
        }
    }
    event->accept();
}
