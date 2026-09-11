#pragma once

#include "CodeEditor.h"

class SmartAnalyzer {
public:
    static QVector<EditorDiagnostic> analyze(ppide_language language,
                                             const QString &text,
                                             const QString &path = {});
    static QString suggestionFor(ppide_language language, const QString &message);
};
