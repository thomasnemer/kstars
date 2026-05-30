/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mcpserver.h"
#include "mcptransport.h"
#include "mcptoolregistry.h"
#include "mcplogbridge.h"
#include "mcpeventbridge.h"
#include "ekos_mcp_debug.h"
#include "Options.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulerprocess.h"
#include "ekos/mount/mount.h"
#include "ekos/capture/capture.h"
#include "ekos/capture/sequencejob.h"
#include "ekos/guide/guide.h"
#include "ekos/focus/focusmodule.h"
#include "ekos/align/align.h"
#include "fitsviewer/fitsdata.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTcpSocket>
#include <QUuid>

namespace MCP
{

Server::Server(QObject *parent) : QObject(parent)
{
    m_transport   = new Transport(this);
    m_registry    = new ToolRegistry(this);
    m_logBridge   = new LogBridge(m_transport, this);
    m_eventBridge = new EventBridge(m_transport, this);
    connect(m_transport, &Transport::requestReceived, this, &Server::handleRequest);
}

bool Server::start(quint16 port)
{
    QString token = Options::mCPToken();
    if (token.isEmpty())
    {
        token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        Options::setMCPToken(token);
    }
    m_transport->setToken(token);
    const QString roToken = Options::mCPReadOnlyToken();
    if (!roToken.isEmpty())
        m_transport->setReadOnlyToken(roToken);
    return m_transport->start(port);
}

bool Server::restart(quint16 port)
{
    m_transport->stop();
    return start(port);
}

void Server::regenerateToken()
{
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    Options::setMCPToken(token);
    m_transport->setToken(token);
}

void Server::regenerateReadOnlyToken()
{
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    Options::setMCPReadOnlyToken(token);
    m_transport->setReadOnlyToken(token);
}

void Server::stop()
{
    m_transport->stop();
}

ToolRegistry *Server::registry()
{
    return m_registry;
}

bool Server::isListening() const
{
    return m_transport->isListening();
}

quint16 Server::port() const
{
    return m_transport->serverPort();
}

void Server::setMount(Ekos::Mount *mount)
{
    m_mount = mount;
    if (mount)
    {
        m_logBridge->connectModule(QStringLiteral("mount"), mount);
        m_eventBridge->connectMount(mount);
    }
}

void Server::setCapture(Ekos::Capture *capture)
{
    m_capture = capture;
    if (capture)
    {
        m_logBridge->connectModule(QStringLiteral("capture"), capture);
        m_eventBridge->connectCapture(capture);
        connect(capture, &Ekos::Capture::newImage, this,
                [this](const QSharedPointer<Ekos::SequenceJob> &job,
                       const QSharedPointer<FITSData> &data,
                       const QString &)
                {
                    if (!data) return;
                    m_lastImage.available = true;
                    m_lastImage.path      = data->filename();
                    m_lastImage.hfr       = data->getHFR();
                    m_lastImage.starCount = data->getStarCenters().size();
                    m_lastImage.width     = data->width();
                    m_lastImage.height    = data->height();
                    m_lastImage.data      = data;

                    QVariant v;
                    if (data->getRecordValue(QStringLiteral("EXPTIME"), v))   m_lastImage.exposure = v.toDouble();
                    if (data->getRecordValue(QStringLiteral("OBJECT"),  v))   m_lastImage.target   = v.toString();
                    if (data->getRecordValue(QStringLiteral("DATE-OBS"), v))  m_lastImage.dateObs  = v.toString();
                    if (data->getRecordValue(QStringLiteral("CCD-TEMP"), v))  m_lastImage.ccdTemp  = v.toDouble();
                    if (data->getRecordValue(QStringLiteral("FILTER"),  v))   m_lastImage.filter   = v.toString();
                    Q_UNUSED(job)
                });
    }
}

void Server::setGuide(Ekos::Guide *guide)
{
    m_guide = guide;
    if (guide)
    {
        m_logBridge->connectModule(QStringLiteral("guide"), guide);
        m_eventBridge->connectGuide(guide);
    }
}

void Server::setFocus(Ekos::FocusModule *focus)
{
    m_focus = focus;
    if (focus)
    {
        m_logBridge->connectModule(QStringLiteral("focus"), focus);
        m_eventBridge->connectFocus(focus);
    }
}

void Server::setAlign(Ekos::Align *align)
{
    m_align = align;
    if (align)
    {
        m_logBridge->connectModule(QStringLiteral("align"), align);
        m_eventBridge->connectAlign(align);
    }
}

void Server::setScheduler(Ekos::Scheduler *sched)
{
    m_scheduler = sched;
    if (sched)
    {
        auto process = sched->process();
        if (process)
        {
            m_logBridge->connectModule(QStringLiteral("scheduler"), process.data());
            m_eventBridge->connectScheduler(process.data());
        }
    }
}

void Server::handleRequest(QTcpSocket *socket, const QByteArray &body)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        qCWarning(KSTARS_EKOS_MCP) << "JSON parse error:" << parseError.errorString();
        m_transport->sendResponse(socket,
            QJsonDocument(makeError(QJsonValue(), -32700, "Parse error"))
                .toJson(QJsonDocument::Compact));
        return;
    }

    QJsonObject req     = doc.object();
    bool isNotification = !req.contains("id");
    QJsonValue id       = isNotification ? QJsonValue() : req["id"];

    // JSON-RPC 2.0 envelope validation: must be version "2.0" with a non-empty method
    const QString version = req["jsonrpc"].toString();
    const QString method  = req["method"].toString();
    if (version != QLatin1String("2.0") || !req.contains("method") || method.isEmpty())
    {
        if (!isNotification)
            m_transport->sendResponse(socket,
                QJsonDocument(makeError(id, -32600, "Invalid Request"))
                    .toJson(QJsonDocument::Compact));
        else
            m_transport->sendNoContent(socket);
        return;
    }

    if (method == "initialize")
    {
        if (isNotification) { m_transport->sendNoContent(socket); return; }

        QJsonObject serverInfo;
        serverInfo["name"]    = "kstars-mcp";
        serverInfo["version"] = "1.0.0";

        QJsonObject capabilities;
        capabilities["tools"] = QJsonObject {};

        QJsonObject result;
        result["protocolVersion"] = "2025-03-26";
        result["serverInfo"]      = serverInfo;
        result["capabilities"]    = capabilities;

        m_transport->sendResponse(socket,
            QJsonDocument(makeResponse(id, result)).toJson(QJsonDocument::Compact));
    }
    else if (method == "notifications/initialized")
    {
        // Client acknowledgement — close connection without a JSON-RPC body.
        m_transport->sendNoContent(socket);
    }
    else if (method == "tools/list")
    {
        if (isNotification) { m_transport->sendNoContent(socket); return; }

        QJsonObject result;
        result["tools"] = m_registry->toolsList();
        m_transport->sendResponse(socket,
            QJsonDocument(makeResponse(id, result)).toJson(QJsonDocument::Compact));
    }
    else if (method == "tools/call")
    {
        if (isNotification) { m_transport->sendNoContent(socket); return; }

        QJsonObject params = req["params"].toObject();
        QString toolName   = params["name"].toString();
        QJsonObject args   = params["arguments"].toObject();

        // Read-only enforcement: block mutating tools for read-only sessions or server-wide RO mode
        const MCP::ToolDefinition *def = m_registry->find(toolName);
        if (!def)
        {
            m_transport->sendResponse(socket,
                QJsonDocument(makeError(id, -32603, QString("Tool not found: %1").arg(toolName)))
                    .toJson(QJsonDocument::Compact));
            return;
        }

        const bool readOnlySession =
            Options::mCPReadOnlyMode() || m_transport->isReadOnlySession(socket);

        if (readOnlySession && !def->readOnly)
        {
            m_transport->sendResponse(socket,
                QJsonDocument(makeError(id, -32601,
                    QStringLiteral("Tool '%1' is not available in read-only mode").arg(toolName)))
                    .toJson(QJsonDocument::Compact));
            return;
        }

        QString error;
        QJsonValue toolResult = m_registry->dispatch(toolName, args, error);

        if (!error.isEmpty())
        {
            m_transport->sendResponse(socket,
                QJsonDocument(makeError(id, -32603, error)).toJson(QJsonDocument::Compact));
            return;
        }

        QString resultText;
        if (toolResult.isObject())
            resultText = QString::fromUtf8(
                QJsonDocument(toolResult.toObject()).toJson(QJsonDocument::Compact));
        else if (toolResult.isArray())
            resultText = QString::fromUtf8(
                QJsonDocument(toolResult.toArray()).toJson(QJsonDocument::Compact));
        else
            resultText = toolResult.toVariant().toString();

        QJsonObject contentItem;
        contentItem["type"] = "text";
        contentItem["text"] = resultText;

        QJsonObject result;
        result["content"] = QJsonArray { contentItem };

        m_transport->sendResponse(socket,
            QJsonDocument(makeResponse(id, result)).toJson(QJsonDocument::Compact));
    }
    else
    {
        if (isNotification)
            m_transport->sendNoContent(socket);
        else
            m_transport->sendResponse(socket,
                QJsonDocument(makeError(id, -32601, "Method not found"))
                    .toJson(QJsonDocument::Compact));
    }
}

QJsonObject Server::makeResponse(const QJsonValue &id, const QJsonValue &result) const
{
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"]      = id;
    response["result"]  = result;
    return response;
}

QJsonObject Server::makeError(const QJsonValue &id, int code, const QString &message) const
{
    QJsonObject errorObj;
    errorObj["code"]    = code;
    errorObj["message"] = message;

    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"]      = id;
    response["error"]   = errorObj;
    return response;
}

} // namespace MCP
