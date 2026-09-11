#include "LspClient.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QUrl>

LspClient::LspClient(QObject *parent) : QObject(parent) {
    process_.setProcessChannelMode(QProcess::SeparateChannels);

    connect(&process_, &QProcess::readyReadStandardOutput, this, [this] {
        buffer_ += process_.readAllStandardOutput();
        consume();
    });
    connect(&process_, &QProcess::readyReadStandardError, this, [this] {
        const QString text = QString::fromLocal8Bit(process_.readAllStandardError()).trimmed();
        if (!text.isEmpty()) Q_EMIT log(text);
    });
    connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                pending_.clear();
                versions_.clear();
                Q_EMIT stopped();
            });
}

bool LspClient::start(const QString &ppcPath, const QString &rootPath) {
    stop();
    if (ppcPath.isEmpty()) return false;

    root_ = rootPath;
    process_.setWorkingDirectory(rootPath.isEmpty()
                                     ? QFileInfo(ppcPath).absolutePath()
                                     : rootPath);
    process_.start(ppcPath, {"serve", "--stdio"});
    if (!process_.waitForStarted(1500)) return false;

    const QString rootUri = rootPath.isEmpty() ? QString() : uriForPath(rootPath);
    const QJsonObject textDocumentCapabilities{
        {"hover", QJsonObject{}},
        {"completion", QJsonObject{}},
        {"publishDiagnostics", QJsonObject{}}
    };
    const QJsonObject params{
        {"processId", QCoreApplication::applicationPid()},
        {"rootUri", rootUri},
        {"capabilities", QJsonObject{{"textDocument", textDocumentCapabilities}}},
        {"clientInfo", QJsonObject{{"name", "PunPun IDE"},
                                    {"version", QString(PPIDE_VERSION)}}}
    };
    request("initialize", params, "initialize", 0);
    return true;
}

void LspClient::stop() {
    if (process_.state() != QProcess::NotRunning) {
        // LSP shutdown is intentionally lightweight here. A graceful shutdown
        // request would require waiting for its response before `exit`; when
        // the IDE closes we prefer bounded teardown over hanging the window.
        notification("exit", {});
        process_.terminate();
        if (!process_.waitForFinished(300)) {
            process_.kill();
            process_.waitForFinished(300);
        }
    }
    buffer_.clear();
    pending_.clear();
    versions_.clear();
}

QString LspClient::uriForPath(const QString &path) {
    return QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toString();
}

QString LspClient::pathForUri(const QString &uri) {
    return QUrl(uri).toLocalFile();
}

void LspClient::send(const QJsonObject &object) {
    if (!running()) return;
    const QByteArray body = QJsonDocument(object).toJson(QJsonDocument::Compact);
    const QByteArray frame = "Content-Length: " + QByteArray::number(body.size()) +
                             "\r\n\r\n" + body;
    process_.write(frame);
}

void LspClient::notification(const QString &method, const QJsonObject &params) {
    send(QJsonObject{{"jsonrpc", "2.0"}, {"method", method}, {"params", params}});
}

qint64 LspClient::request(const QString &method, const QJsonObject &params,
                          const QString &kind, quint64 token) {
    const qint64 id = nextId_++;
    pending_.insert(id, {kind, token});
    send(QJsonObject{{"jsonrpc", "2.0"},
                     {"id", id},
                     {"method", method},
                     {"params", params}});
    return id;
}

void LspClient::openDocument(const QString &path, const QString &text) {
    if (!running()) return;
    versions_[path] = 1;
    notification("textDocument/didOpen",
                 QJsonObject{{"textDocument",
                              QJsonObject{{"uri", uriForPath(path)},
                                          {"languageId", "punpun"},
                                          {"version", 1},
                                          {"text", text}}}});
}

void LspClient::changeDocument(const QString &path, const QString &text) {
    if (!running()) return;
    const int version = versions_.value(path, 0) + 1;
    if (version == 1) {
        openDocument(path, text);
        return;
    }

    versions_[path] = version;
    notification("textDocument/didChange",
                 QJsonObject{{"textDocument",
                              QJsonObject{{"uri", uriForPath(path)},
                                          {"version", version}}},
                             {"contentChanges",
                              QJsonArray{QJsonObject{{"text", text}}}}});
}

quint64 LspClient::requestHover(const QString &path, int line, int column) {
    const quint64 token = nextToken_++;
    request("textDocument/hover",
            QJsonObject{{"textDocument", QJsonObject{{"uri", uriForPath(path)}}},
                        {"position", QJsonObject{{"line", line}, {"character", column}}}},
            "hover", token);
    return token;
}

quint64 LspClient::requestCompletion(const QString &path, int line, int column) {
    const quint64 token = nextToken_++;
    request("textDocument/completion",
            QJsonObject{{"textDocument", QJsonObject{{"uri", uriForPath(path)}}},
                        {"position", QJsonObject{{"line", line}, {"character", column}}}},
            "completion", token);
    return token;
}

void LspClient::consume() {
    while (true) {
        const int headerEnd = buffer_.indexOf("\r\n\r\n");
        if (headerEnd < 0) return;

        const QByteArray headers = buffer_.left(headerEnd);
        int contentLength = -1;
        for (const QByteArray &rawLine : headers.split('\n')) {
            const QByteArray line = rawLine.trimmed();
            if (line.toLower().startsWith("content-length:")) {
                contentLength = line.mid(15).trimmed().toInt();
                break;
            }
        }

        if (contentLength < 0) {
            buffer_.remove(0, headerEnd + 4);
            Q_EMIT log("PunPun language service sent a frame without Content-Length");
            continue;
        }

        const int bodyStart = headerEnd + 4;
        if (buffer_.size() < bodyStart + contentLength) return;

        const QByteArray body = buffer_.mid(bodyStart, contentLength);
        buffer_.remove(0, bodyStart + contentLength);
        const QJsonDocument document = QJsonDocument::fromJson(body);
        if (document.isObject()) handle(document.object());
    }
}

QString LspClient::hoverText(const QJsonValue &value) {
    if (value.isString()) return value.toString();
    if (!value.isObject()) return {};

    const QJsonValue contents = value.toObject().value("contents");
    if (contents.isString()) return contents.toString();
    if (contents.isObject()) return contents.toObject().value("value").toString();
    if (!contents.isArray()) return {};

    QStringList parts;
    for (const QJsonValue &entry : contents.toArray()) {
        if (entry.isString()) {
            parts << entry.toString();
        } else if (entry.isObject()) {
            const QString text = entry.toObject().value("value").toString();
            if (!text.isEmpty()) parts << text;
        }
    }
    return parts.join("\n\n");
}

void LspClient::handle(const QJsonObject &object) {
    if (object.value("method").toString() == "textDocument/publishDiagnostics") {
        const QJsonObject params = object.value("params").toObject();
        Q_EMIT diagnostics(pathForUri(params.value("uri").toString()),
                           params.value("diagnostics").toArray());
        return;
    }

    if (!object.contains("id")) return;

    const qint64 id = object.value("id").toInteger();
    if (!pending_.contains(id)) return;
    const auto pending = pending_.take(id);
    const QString kind = pending.first;
    const quint64 token = pending.second;

    if (kind == "initialize") {
        notification("initialized", {});
        Q_EMIT log("PunPun language service ready");
        return;
    }

    if (kind == "hover") {
        Q_EMIT hoverResult(token, hoverText(object.value("result")));
        return;
    }

    if (kind != "completion") return;

    QStringList completions;
    const QJsonValue value = object.value("result");
    const QJsonArray items = value.isArray()
        ? value.toArray()
        : value.toObject().value("items").toArray();

    for (const QJsonValue &item : items) {
        if (item.isString()) {
            completions << item.toString();
            continue;
        }
        if (!item.isObject()) continue;

        const QJsonObject completion = item.toObject();
        QString text = completion.value("insertText").toString();
        if (text.isEmpty()) text = completion.value("label").toString();
        if (!text.isEmpty()) completions << text;
    }

    completions.removeDuplicates();
    Q_EMIT completionResult(token, completions);
}
