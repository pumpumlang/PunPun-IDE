#pragma once

#include <QRegularExpression>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

#include "punpun_plugin_api.h"

class SyntaxHighlighter : public QSyntaxHighlighter {
public:
    SyntaxHighlighter(QTextDocument *doc, ppide_language language,
                      const QStringList &extraKeywords = {});
    void setLanguage(ppide_language language,
                     const QStringList &extraKeywords = {});

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression rx;
        QTextCharFormat fmt;
        int captureGroup = 0;
    };

    ppide_language language_;
    QVector<Rule> rules_;
    QTextCharFormat commentFmt_;
    QTextCharFormat stringFmt_;
    QTextCharFormat numberFmt_;
    QTextCharFormat keywordFmt_;
    QTextCharFormat typeFmt_;
    QTextCharFormat preprocessorFmt_;
    QTextCharFormat headingFmt_;
    QTextCharFormat fenceFmt_;
    QTextCharFormat escapeFmt_;
    QTextCharFormat propertyFmt_;

    void rebuild(const QStringList &extraKeywords);
    void addWords(const QStringList &words, const QTextCharFormat &format);
};
