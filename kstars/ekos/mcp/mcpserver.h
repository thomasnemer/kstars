/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QSharedPointer>
#include <QString>

class QTcpSocket;
class FITSData;

namespace ISD
{
class GenericDevice;
class Camera;
}

namespace Ekos
{
class Mount;
class Capture;
class Guide;
class FocusModule;
class Align;
class Scheduler;
class SequenceJob;
}

namespace MCP
{

class Transport;
class ToolRegistry;
class LogBridge;
class EventBridge;
class GuideHistory;

class Server : public QObject
{
    Q_OBJECT

public:
    explicit Server(QObject *parent = nullptr);

    bool start(quint16 port);
    bool restart(quint16 port);
    void stop();
    bool isListening() const;
    quint16 port() const;

    ToolRegistry *registry();

    // Called by Manager whenever module pointers change (INDI connect/disconnect)
    void setMount(Ekos::Mount *mount);
    void setCapture(Ekos::Capture *capture);
    void setGuide(Ekos::Guide *guide);
    void setFocus(Ekos::FocusModule *focus);
    void setAlign(Ekos::Align *align);
    void setScheduler(Ekos::Scheduler *scheduler);

    void regenerateToken();
    void regenerateReadOnlyToken();

    // Latest captured image — populated by a per-camera hook on
    // ISD::Camera::newImage installed in hookCamera(). Covers every frame
    // producer (Capture queue, PAA, Focus, Align, ad-hoc camera_capture, raw
    // INDI) without requiring per-module wiring.
    struct LastImage
    {
        bool                       available = false;
        QString                    cameraName;
        QDateTime                  receivedAt;
        QString                    filter;
        QString                    target;
        QString                    dateObs;
        double                     exposure = 0.0;
        double                     ccdTemp  = 0.0;
        double                     hfr      = 0.0;
        int                        starCount = 0;
        int                        width    = 0;
        int                        height   = 0;
        // The on-disk path is not cached: ISD::Camera::newImage fires before
        // the file is written, so the FITSData's m_Filename is empty at hook
        // time. Read data->filename() at query time instead — by then the
        // consumer (Capture::cameraprocess for queue, etc.) has set it. For
        // ad-hoc / preview captures there's no disk save at all, and the
        // thumbnail tool round-trips via FITSData::saveImage() to a temp file.
        QSharedPointer<FITSData>   data;
    };
    // Most-recent frame across all cameras. Sentinel (.available == false) if
    // no frame has been received this session.
    const LastImage &lastImage() const;
    // Most-recent frame from a specific camera (matched by device name).
    // Sentinel if no frame from that camera. Empty cameraName returns the
    // same sentinel.
    const LastImage &lastImageFor(const QString &cameraName) const;

    // Latest polar-alignment error values, populated by Server's hook on
    // PolarAlignmentAssistant::updatedErrorsChanged.
    struct PolarAlignState
    {
        bool   hasError = false;
        double totalDeg = 0.0;
        double azDeg    = 0.0;
        double altDeg   = 0.0;
    };
    const PolarAlignState &polarAlignState() const { return m_polarAlignState; }

    GuideHistory *guideHistory() const { return m_guideHistory; }

private slots:
    void handleRequest(QTcpSocket *socket, const QByteArray &body);

private:
    QJsonObject makeResponse(const QJsonValue &id, const QJsonValue &result) const;
    QJsonObject makeError(const QJsonValue &id, int code, const QString &message) const;

    // Hook a device's camera-frame signal. The concrete ISD::Camera object
    // may not exist yet at INDIListener::newDevice time — it's created
    // asynchronously when DRIVER_INFO arrives — so we either install the
    // hook immediately if the camera is already there, or wire up
    // GenericDevice::newCamera to install it when ready.
    void hookCamera(const QSharedPointer<ISD::GenericDevice> &device);
    // Connect ISD::Camera::newImage → image-cache update. Tracked via
    // m_hookedCameras so duplicate calls (initial sweep + signal, or two
    // newCamera deliveries) don't stack listeners.
    void installImageHook(ISD::Camera *camera);

    Transport        *m_transport    { nullptr };
    ToolRegistry     *m_registry     { nullptr };
    LogBridge        *m_logBridge    { nullptr };
    EventBridge      *m_eventBridge  { nullptr };
    GuideHistory     *m_guideHistory { nullptr };

    Ekos::Mount      *m_mount     { nullptr };
    Ekos::Capture    *m_capture   { nullptr };
    Ekos::Guide      *m_guide     { nullptr };
    Ekos::FocusModule *m_focus    { nullptr };
    Ekos::Align      *m_align     { nullptr };
    Ekos::Scheduler  *m_scheduler { nullptr };

    QHash<QString, LastImage> m_imagesByCamera;
    QString                   m_mostRecentCamera;
    QSet<QString>             m_hookedCameras;
    LastImage                 m_emptyImage; // sentinel returned when nothing cached
    PolarAlignState           m_polarAlignState;
};

} // namespace MCP
