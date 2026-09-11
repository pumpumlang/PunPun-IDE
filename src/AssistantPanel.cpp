#include "AssistantPanel.h"

#include <QBrush>
#include <QColor>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QVariantMap>

namespace {
QString severityName(int severity) {
    if (severity <= 1) return "Error";
    if (severity == 2) return "Warning";
    return "Hint";
}

QString severityGlyph(int severity) {
    if (severity <= 1) return QString::fromUtf8("●");
    if (severity == 2) return QString::fromUtf8("▲");
    return QString::fromUtf8("◆");
}
}

AssistantPanel::AssistantPanel(QWidget *parent) : QWidget(parent) {
    setObjectName("assistantPanel");
    setMinimumWidth(270);
    setMaximumWidth(410);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *header = new QWidget;
    header->setObjectName("assistantHeader");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 8, 8);
    auto *title = new QLabel("CODE ASSISTANT");
    title->setObjectName("sectionTitle");
    title->setToolTip("Live compiler diagnostics + local smart review");
    checkButton_ = new QPushButton("Check");
    checkButton_->setObjectName("compactButton");
    headerLayout->addWidget(title, 1);
    headerLayout->addWidget(checkButton_);
    layout->addWidget(header);

    summary_ = new QLabel("Open a source file to review it.");
    summary_->setObjectName("assistantSummary");
    summary_->setWordWrap(true);
    summary_->setContentsMargins(12, 8, 12, 8);
    layout->addWidget(summary_);

    auto *counts = new QWidget;
    counts->setObjectName("assistantCounts");
    auto *countsLayout = new QHBoxLayout(counts);
    countsLayout->setContentsMargins(10, 5, 10, 6);
    countsLayout->setSpacing(6);
    errorCount_ = new QLabel("● 0 Errors");
    errorCount_->setObjectName("assistantErrors");
    warningCount_ = new QLabel("▲ 0 Warnings");
    warningCount_->setObjectName("assistantWarnings");
    hintCount_ = new QLabel("◆ 0 Hints");
    hintCount_->setObjectName("assistantHints");
    countsLayout->addWidget(errorCount_);
    countsLayout->addWidget(warningCount_);
    countsLayout->addWidget(hintCount_);
    countsLayout->addStretch();
    layout->addWidget(counts);

    list_ = new QListWidget;
    list_->setObjectName("assistantList");
    list_->setMinimumHeight(170);
    layout->addWidget(list_, 1);

    details_ = new QTextBrowser;
    details_->setObjectName("assistantDetails");
    details_->setOpenExternalLinks(true);
    details_->setMinimumHeight(150);
    details_->setMaximumHeight(260);
    details_->setHtml("<p>Select a diagnostic to see an explanation and likely next step.</p>");
    layout->addWidget(details_);

    environment_ = new QLabel("Environment: waiting for toolchain check");
    environment_->setObjectName("assistantEnvironment");
    environment_->setWordWrap(true);
    environment_->setContentsMargins(12, 8, 12, 10);
    layout->addWidget(environment_);

    connect(checkButton_, &QPushButton::clicked, this, &AssistantPanel::checkRequested);
    connect(list_, &QListWidget::itemClicked, this, &AssistantPanel::showDetails);
    connect(list_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        const auto data = item->data(Qt::UserRole).toMap();
        Q_EMIT navigateRequested(data.value("path").toString(),
                                 data.value("line").toInt(),
                                 data.value("column").toInt());
    });
}

void AssistantPanel::setDiagnostics(const QString &path, const QVector<EditorDiagnostic> &items) {
    currentPath_ = path;
    diagnostics_ = items;
    rebuild();
}

void AssistantPanel::setEnvironmentSummary(const QString &summary) {
    environment_->setText(summary.isEmpty() ? "Environment: unavailable" : summary);
    environment_->setToolTip(summary);
}

void AssistantPanel::setBusy(bool busy) {
    checkButton_->setEnabled(!busy);
    checkButton_->setText(busy ? "Checking…" : "Check");
}

void AssistantPanel::rebuild() {
    list_->clear();
    int errors = 0;
    int warnings = 0;
    int hints = 0;
    for (const auto &d : diagnostics_) {
        if (d.severity <= 1) ++errors;
        else if (d.severity == 2) ++warnings;
        else ++hints;

        const QString source = d.source.isEmpty() ? "Analyzer" : d.source;
        auto *item = new QListWidgetItem(
            QString("%1  L%2  %3\n%4")
                .arg(severityGlyph(d.severity))
                .arg(d.line + 1)
                .arg(source)
                .arg(d.message));
        QVariantMap data{{"path", currentPath_},
                         {"line", d.line},
                         {"column", d.column},
                         {"severity", d.severity},
                         {"message", d.message},
                         {"code", d.code},
                         {"source", source},
                         {"suggestion", d.suggestion}};
        item->setData(Qt::UserRole, data);
        if (d.severity <= 1) item->setForeground(QBrush(QColor("#f14c4c")));
        else if (d.severity == 2) item->setForeground(QBrush(QColor("#cca700")));
        else item->setForeground(QBrush(QColor("#75beff")));
        list_->addItem(item);
    }

    errorCount_->setText(QString("● %1 Errors").arg(errors));
    warningCount_->setText(QString("▲ %1 Warnings").arg(warnings));
    hintCount_->setText(QString("◆ %1 Hints").arg(hints));

    if (currentPath_.isEmpty()) {
        summary_->setText("Open a source file to review it.");
    } else if (diagnostics_.isEmpty()) {
        summary_->setText(QString("No issues found in %1.").arg(QFileInfo(currentPath_).fileName()));
    } else {
        summary_->setText(QString("%1 error(s)  •  %2 warning(s)  •  %3 hint(s)")
                              .arg(errors).arg(warnings).arg(hints));
    }

    if (!diagnostics_.isEmpty()) {
        list_->setCurrentRow(0);
        showDetails(list_->item(0));
    } else {
        details_->setHtml("<p><b>No problems in this file.</b></p><p>PPC/LSP or the host C/C++ compiler remains the source of truth. Local Review adds lightweight explanations and suspicious-pattern hints.</p>");
    }
}

void AssistantPanel::showDetails(QListWidgetItem *item) {
    if (!item) return;
    const auto data = item->data(Qt::UserRole).toMap();
    const QString message = data.value("message").toString().toHtmlEscaped();
    const QString code = data.value("code").toString().toHtmlEscaped();
    const QString source = data.value("source").toString().toHtmlEscaped();
    const QString suggestion = data.value("suggestion").toString().toHtmlEscaped();
    const QString severity = severityName(data.value("severity").toInt());

    QString html = QString("<h3>%1</h3><p><b>%2</b>%3</p><p>%4</p>")
                       .arg(severity, source,
                            code.isEmpty() ? QString() : QString(" · <code>%1</code>").arg(code),
                            message);
    if (!suggestion.isEmpty())
        html += QString("<hr><p><b>Suggested next step</b></p><p>%1</p>").arg(suggestion);
    html += "<p><small>Double-click the issue to jump to its location. Compiler diagnostics override Local Review when they disagree.</small></p>";
    details_->setHtml(html);
}
