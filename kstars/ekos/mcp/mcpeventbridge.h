/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QTimer>

class FITSData;

namespace Ekos
{
class Mount;
class Capture;
class Guide;
class FocusModule;
class Align;
class SchedulerProcess;
}

namespace MCP
{

class Transport;

// Bridges Ekos module signals to typed SSE event broadcasts on the
// /mcp/stream channel. Each connect* method wires the relevant signal(s)
// for one module class. Designed as a sibling of LogBridge: owned by
// MCP::Server, fed module pointers from Server::setMount/setCapture/...
//
// Some event streams are inherently chatty (mount coords during a slew,
// guide deltas during guiding). Those broadcasts pass through internal
// throttle QTimers so a single event is emitted at most ~1 Hz / ~2 Hz.
class EventBridge : public QObject
{
        Q_OBJECT
    public:
        explicit EventBridge(Transport *transport, QObject *parent = nullptr);

        void connectMount(Ekos::Mount *m);
        void connectCapture(Ekos::Capture *c);
        void connectGuide(Ekos::Guide *g);
        void connectFocus(Ekos::FocusModule *f);
        void connectAlign(Ekos::Align *a);
        void connectScheduler(Ekos::SchedulerProcess *s);

    private:
        void emitStatusChange(const QString &module, const QString &to);

        Transport *m_transport { nullptr };

        // Last-known status string per module — drives the "from" field in
        // status_change events. Empty if the module has not reported yet.
        QHash<QString, QString> m_lastStatus;

        // Throttle latches: collapse rapid signals into one emission per tick.
        QTimer m_mountCoordsTimer;
        QJsonObject m_mountCoordsPending;
        bool m_mountCoordsHasPending { false };
        // Last broadcast Dec/Alt/Az (degrees) — used by the timer to suppress
        // mount_coords_update events when none of these axes have moved by
        // more than 1″ since the last broadcast. RA is intentionally not
        // tracked: it drifts at the sidereal rate via LST-driven JNow
        // recomputation regardless of whether the mount is moving, so it
        // can't be used as a motion signal. Alt/Az/Dec are the operator-
        // visible axes — stationary at parked/idle, moving during tracking
        // (Alt/Az) or slewing (all three).
        double m_mountCoordsLastDec { 0.0 };
        double m_mountCoordsLastAlt { 0.0 };
        double m_mountCoordsLastAz  { 0.0 };
        bool   m_mountCoordsHasLast { false };

        // Pointer of the last FITSData broadcast via image_captured. Capture's
        // pipeline emits Capture::newImage twice for non-final frames (relay
        // from per-train Camera + an explicit cameraprocess emit), so the same
        // FITSData reaches our subscriber more than once. Compare pointers to
        // dedupe; holding the shared_ptr keeps the data alive across the gap,
        // which is fine — it's replaced on the next genuine frame.
        QSharedPointer<FITSData> m_lastImageCapturedData;

        QTimer m_guideDeviationTimer;
        QJsonObject m_guideDeviationPending;
        bool m_guideDeviationHasPending { false };

        // Latest guide sigma values — newAxisSigma fires on its own signal,
        // so we latch it here and include it when the throttled delta event fires.
        double m_lastGuideSigmaRa  { 0.0 };
        double m_lastGuideSigmaDec { 0.0 };
};

} // namespace MCP
