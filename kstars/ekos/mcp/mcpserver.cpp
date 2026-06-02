/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mcpserver.h"
#include "mcptransport.h"
#include "mcptoolregistry.h"
#include "mcplogbridge.h"
#include "mcpeventbridge.h"
#include "mcpguidehistory.h"
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
#include "ekos/align/polaralignmentassistant.h"
#include "fitsviewer/fitsdata.h"
#include "indi/indicamera.h"
#include "indi/indilistener.h"
#include "indi/indistd.h"

#include <basedevice.h>

#include <QDateTime>
#include <QDir>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTcpSocket>
#include <QUuid>

namespace MCP
{

Server::Server(QObject *parent) : QObject(parent)
{
    m_transport    = new Transport(this);
    m_registry     = new ToolRegistry(this);
    m_logBridge    = new LogBridge(m_transport, this);
    m_eventBridge  = new EventBridge(m_transport, this);
    m_guideHistory = new GuideHistory(this);
    connect(m_transport, &Transport::requestReceived, this, &Server::handleRequest);

    // Hook every camera's newImage signal so the image cache sees frames
    // from every workflow (Capture queue, PAA, Focus, Align, ad-hoc
    // camera_capture, raw INDI control). Existing cameras need an initial
    // sweep; future cameras come in via INDIListener::newDevice.
    if (auto *listener = INDIListener::Instance())
    {
        connect(listener, &INDIListener::newDevice, this, &Server::hookCamera);
        for (const auto &dev : INDIListener::devicesByInterface(INDI::BaseDevice::CCD_INTERFACE))
            hookCamera(dev);
    }
}

const Server::LastImage &Server::lastImage() const
{
    if (m_mostRecentCamera.isEmpty()) return m_emptyImage;
    auto it = m_imagesByCamera.constFind(m_mostRecentCamera);
    return it == m_imagesByCamera.constEnd() ? m_emptyImage : it.value();
}

const Server::LastImage &Server::lastImageFor(const QString &cameraName) const
{
    if (cameraName.isEmpty()) return m_emptyImage;
    auto it = m_imagesByCamera.constFind(cameraName);
    return it == m_imagesByCamera.constEnd() ? m_emptyImage : it.value();
}

void Server::hookCamera(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (!device) return;
    // INDIListener::newDevice fires when the GenericDevice is created — before
    // DRIVER_INFO arrives and the concrete ISD::Camera is constructed and added
    // to m_ConcreteDevices. If getCamera() returns null here, install the hook
    // via GenericDevice::newCamera so it lands the moment the camera is
    // registered. Otherwise the cache stays empty for the entire session.
    if (auto *cam = device->getCamera())
    {
        installImageHook(cam);
        return;
    }
    connect(device.data(), &ISD::GenericDevice::newCamera, this,
            [this](ISD::Camera *cam) { installImageHook(cam); });
}

void Server::installImageHook(ISD::Camera *camera)
{
    if (!camera) return;
    const QString cameraName = camera->getDeviceName();
    // installImageHook may be reached more than once for the same camera (e.g.
    // initial sweep + GenericDevice::newCamera). The set deduplicates so we
    // don't stack newImage listeners, which would write the same frame into
    // the cache twice and double the workload of every image_last_* call.
    if (m_hookedCameras.contains(cameraName)) return;
    m_hookedCameras.insert(cameraName);

    connect(camera, &ISD::Camera::newImage, this,
            [this, camera](const QSharedPointer<FITSData> &data, const QString &)
            {
                if (!data) return;
                const QString cameraName = camera->getDeviceName();

                // Ensure the FITSData has an on-disk path. Capture's post-save
                // pipeline (cameraprocess.cpp setFilename) eventually overwrites
                // this with the real sequence path, but ad-hoc captures and PAA
                // frames never run that pipeline and otherwise leave filename
                // empty — which would suppress image_last_info.path and the
                // thumbnail's disk-render fast path. Persist to a per-camera
                // temp file once so every producer has a stable path.
                if (data->filename().isEmpty())
                {
                    QString safeName = cameraName;
                    safeName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")),
                                     QStringLiteral("_"));
                    QString tempPath = QDir::temp().filePath(
                                           QStringLiteral("mcp_cache_%1.fits").arg(safeName));
                    if (data->saveImage(tempPath))
                        data->setFilename(tempPath);
                }

                LastImage img;
                img.available  = true;
                img.cameraName = cameraName;
                img.receivedAt = QDateTime::currentDateTimeUtc();
                img.hfr        = data->getHFR();
                img.starCount  = data->getStarCenters().size();
                img.width      = data->width();
                img.height     = data->height();
                img.data       = data;

                QVariant v;
                if (data->getRecordValue(QStringLiteral("EXPTIME"),  v)) img.exposure = v.toDouble();
                if (data->getRecordValue(QStringLiteral("OBJECT"),   v)) img.target   = v.toString();
                if (data->getRecordValue(QStringLiteral("DATE-OBS"), v)) img.dateObs  = v.toString();
                if (data->getRecordValue(QStringLiteral("CCD-TEMP"), v)) img.ccdTemp  = v.toDouble();
                if (data->getRecordValue(QStringLiteral("FILTER"),   v)) img.filter   = v.toString();

                m_imagesByCamera.insert(cameraName, img);
                m_mostRecentCamera = cameraName;
            });
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
        // Image-cache population is handled per-camera via hookCamera(); the
        // ISD::Camera::newImage signal fires for Capture-queue frames too, so
        // we don't need a separate Capture::newImage subscriber here.
    }
}

void Server::setGuide(Ekos::Guide *guide)
{
    m_guide = guide;
    if (guide)
    {
        m_logBridge->connectModule(QStringLiteral("guide"), guide);
        m_eventBridge->connectGuide(guide);
        m_guideHistory->clear();
        m_guideHistory->attach(guide);
    }
    else
    {
        m_guideHistory->clear();
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

        if (auto *paa = align->polarAlignmentAssistant())
        {
            connect(paa, &Ekos::PolarAlignmentAssistant::updatedErrorsChanged, this,
                    [this](double total, double az, double alt)
            {
                m_polarAlignState.hasError = true;
                m_polarAlignState.totalDeg = total;
                m_polarAlignState.azDeg    = az;
                m_polarAlignState.altDeg   = alt;
            });
        }
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
