#include "ScriptHost.h"

#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QJSEngine>
#include <QJSValue>
#include <QJSValueIterator>
#include <QStandardPaths>

ScriptHost::ScriptHost() { reload(); }

void ScriptHost::reload() {
    keywords_.clear();
    snippets_.clear();
    loaded_.clear();

    QFile builtin(":/js/builtin-extension.js");
    if (builtin.open(QIODevice::ReadOnly)) {
        loadScript(QString::fromUtf8(builtin.readAll()), "builtin-extension.js");
    }

    const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/extensions";
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    const QStringList scripts = dir.entryList({"*.js"}, QDir::Files, QDir::Name);
    for (const QString &name : scripts) {
        QFile file(dir.filePath(name));
        if (file.open(QIODevice::ReadOnly)) {
            loadScript(QString::fromUtf8(file.readAll()), name);
        }
    }
}

void ScriptHost::loadScript(const QString &source, const QString &name) {
    QJSEngine engine;
    const QJSValue result = engine.evaluate(source, name);
    if (result.isError()) {
        return;
    }

    const QJSValue extension = engine.globalObject().property("punpunIdeExtension");
    if (!extension.isObject()) {
        return;
    }

    const QString extensionName = extension.property("name").toString().trimmed();
    if (!extensionName.isEmpty()) {
        loaded_ << extensionName;
    }

    const QJSValue languages = extension.property("languages");
    static const QStringList supportedLanguages = {
        "punpun", "c", "cpp", "header", "markdown", "json", "javascript", "python"
    };

    for (const QString &language : supportedLanguages) {
        const QJSValue spec = languages.property(language);
        if (!spec.isObject()) {
            continue;
        }

        const QJSValue keywords = spec.property("keywords");
        if (keywords.isArray()) {
            const int length = keywords.property("length").toInt();
            for (int i = 0; i < length; ++i) {
                const QString word = keywords.property(i).toString().trimmed();
                if (!word.isEmpty() && !keywords_[language].contains(word)) {
                    keywords_[language] << word;
                }
            }
        }

        const QJSValue snippets = spec.property("snippets");
        if (snippets.isObject()) {
            QJSValueIterator it(snippets);
            while (it.hasNext()) {
                it.next();
                const QString key = it.name().trimmed();
                if (!key.isEmpty()) {
                    snippets_[language].insert(key, it.value().toString());
                }
            }
        }
    }
}

QStringList ScriptHost::keywords(const QString &language) const {
    return keywords_.value(language.toLower());
}

QHash<QString, QString> ScriptHost::snippets(const QString &language) const {
    return snippets_.value(language.toLower());
}
