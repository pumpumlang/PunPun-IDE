#pragma once
#include <QHash>
#include <QJSEngine>
#include <QStringList>

class ScriptHost {
public:
    ScriptHost();
    void reload();
    QStringList keywords(const QString &language) const;
    QHash<QString,QString> snippets(const QString &language) const;
    QStringList loadedExtensions() const { return loaded_; }
private:
    void loadScript(const QString &source, const QString &name);
    QHash<QString,QStringList> keywords_;
    QHash<QString,QHash<QString,QString>> snippets_;
    QStringList loaded_;
};
