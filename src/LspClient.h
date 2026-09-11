#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QPair>
#include <QProcess>
#include <QString>
#include <QStringList>

class LspClient : public QObject {
    Q_OBJECT
public:
    explicit LspClient(QObject *parent = nullptr);

    bool start(const QString &ppcPath, const QString &rootPath);
    void stop();
    bool running() const { return process_.state() == QProcess::Running; }

    void openDocument(const QString &path, const QString &text);
    void changeDocument(const QString &path, const QString &text);
    quint64 requestHover(const QString &path, int line, int column);
    quint64 requestCompletion(const QString &path, int line, int column);

Q_SIGNALS:
    void diagnostics(const QString &path, const QJsonArray &items);
    void hoverResult(quint64 token, const QString &markdown);
    void completionResult(quint64 token, const QStringList &items);
    void log(const QString &line);
    void stopped();

private:
    QProcess process_;
    QByteArray buffer_;
    qint64 nextId_ = 1;
    QHash<qint64, QPair<QString, quint64>> pending_;
    QHash<QString, int> versions_;
    quint64 nextToken_ = 1;
    QString root_;

    void send(const QJsonObject &object);
    qint64 request(const QString &method, const QJsonObject &params,
                   const QString &kind, quint64 token);
    void notification(const QString &method, const QJsonObject &params);
    void consume();
    void handle(const QJsonObject &object);

    static QString hoverText(const QJsonValue &value);
    static QString uriForPath(const QString &path);
    static QString pathForUri(const QString &uri);
};
