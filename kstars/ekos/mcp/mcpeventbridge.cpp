/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mcpeventbridge.h"
#include "mcptransport.h"
#include "ekos_mcp_debug.h"

#include "ekos/mount/mount.h"
#include "ekos/capture/capture.h"
#include "ekos/capture/sequencejob.h"
#include "ekos/guide/guide.h"
#include "ekos/focus/focusmodule.h"
#include "ekos/align/align.h"
#include "ekos/scheduler/schedulerprocess.h"
#include "indi/indimount.h"
#include "fitsviewer/fitsdata.h"
#include "skyobjects/skypoint.h"

#include <cmath>

#include <QDateTime>
#include <QJsonValue>

namespace MCP
{

EventBridge::EventBridge(Transport *transport, QObject *parent)
    : QObject(parent), m_transport(transport)
{
    // Mount coords are batched at 1 Hz to avoid flooding clients during slews.
    // The timer also suppresses idle no-ops: at a parked or idle pointing the
    // mount isn't moving, but LST-driven JNow RA recomputation still drifts at
    // the sidereal rate (~15″/s) regardless of whether the mount is moving.
    // Suppression keys on Alt/Az/Dec only — those are stationary at idle and
    // change continuously during tracking or slewing, so they are the right
    // truth source for "is the mount actually moving?". RA is intentionally
    // excluded; including it would re-emit every tick at idle as RA drifts
    // past any reasonable threshold.
    m_mountCoordsTimer.setInterval(1000);
    m_mountCoordsTimer.setSingleShot(false);
    connect(&m_mountCoordsTimer, &QTimer::timeout, this, [this]()
    {
        if (!m_mountCoordsHasPending) return;
        const double dec = m_mountCoordsPending.value(QStringLiteral("dec")).toDouble();
        const double alt = m_mountCoordsPending.value(QStringLiteral("alt")).toDouble();
        const double az  = m_mountCoordsPending.value(QStringLiteral("az")).toDouble();
        constexpr double EPS_DEG = 1.0 / 3600.0; // 1″ in degrees
        const bool unchanged = m_mountCoordsHasLast
                               && std::abs(dec - m_mountCoordsLastDec) < EPS_DEG
                               && std::abs(alt - m_mountCoordsLastAlt) < EPS_DEG
                               && std::abs(az  - m_mountCoordsLastAz)  < EPS_DEG;
        m_mountCoordsHasPending = false;
        if (unchanged) return;
        m_transport->broadcastSSEEvent(QStringLiteral("mount_coords_update"), m_mountCoordsPending);
        m_mountCoordsLastDec    = dec;
        m_mountCoordsLastAlt    = alt;
        m_mountCoordsLastAz     = az;
        m_mountCoordsHasLast    = true;
    });
    m_mountCoordsTimer.start();

    // Guide deviations at ~2 Hz.
    m_guideDeviationTimer.setInterval(500);
    m_guideDeviationTimer.setSingleShot(false);
    connect(&m_guideDeviationTimer, &QTimer::timeout, this, [this]()
    {
        if (!m_guideDeviationHasPending) return;
        m_transport->broadcastSSEEvent(QStringLiteral("guide_deviation"), m_guideDeviationPending);
        m_guideDeviationHasPending = false;
    });
    m_guideDeviationTimer.start();
}

void EventBridge::emitStatusChange(const QString &module, const QString &to)
{
    const QString from = m_lastStatus.value(module);
    // Suppress no-op events where the module re-asserts its current status.
    // Modules can re-fire newStatus mid-state (e.g. Capture pulses every job
    // sub-step) which previously inflated status_change to ~75% noise.
    if (from == to) return;
    m_lastStatus[module] = to;
    QJsonObject p;
    p[QStringLiteral("module")] = module;
    p[QStringLiteral("from")]   = from.isEmpty() ? QJsonValue() : QJsonValue(from);
    p[QStringLiteral("to")]     = to;
    p[QStringLiteral("ts")]     = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    m_transport->broadcastSSEEvent(QStringLiteral("status_change"), p);
}

// Free-function mirror of the ISD::Mount::statusString member so we can map an
// enum without needing a live ISD::Mount pointer in the SSE event lambda.
static QString mountStatusString(ISD::Mount::Status s)
{
    switch (s)
    {
        case ISD::Mount::MOUNT_IDLE:     return QStringLiteral("Idle");
        case ISD::Mount::MOUNT_MOVING:   return QStringLiteral("Moving");
        case ISD::Mount::MOUNT_SLEWING:  return QStringLiteral("Slewing");
        case ISD::Mount::MOUNT_TRACKING: return QStringLiteral("Tracking");
        case ISD::Mount::MOUNT_PARKING:  return QStringLiteral("Parking");
        case ISD::Mount::MOUNT_PARKED:   return QStringLiteral("Parked");
        case ISD::Mount::MOUNT_ERROR:    return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}

void EventBridge::connectMount(Ekos::Mount *m)
{
    if (!m) return;
    // Manager re-fires newModule for every module creation, which re-runs all
    // setX() handlers and therefore reaches every connect* method multiple
    // times per session. Lambda slots can't be deduplicated with
    // Qt::UniqueConnection, so drop any prior wiring from this sender to us
    // before re-establishing it — otherwise each Ekos signal fires N stacked
    // SSE broadcasts.
    disconnect(m, nullptr, this, nullptr);
    connect(m, &Ekos::Mount::newStatus, this, [this](ISD::Mount::Status status)
    {
        emitStatusChange(QStringLiteral("mount"), mountStatusString(status));
    });
    connect(m, &Ekos::Mount::newCoords, this,
            [this](const SkyPoint &pos, ISD::Mount::PierSide, const dms &)
    {
        QJsonObject p;
        p[QStringLiteral("module")] = QStringLiteral("mount");
        p[QStringLiteral("ra")]     = pos.ra().Hours();
        p[QStringLiteral("dec")]    = pos.dec().Degrees();
        p[QStringLiteral("alt")]    = pos.alt().Degrees();
        p[QStringLiteral("az")]     = pos.az().Degrees();
        m_mountCoordsPending = p;
        m_mountCoordsHasPending = true;
    });
}

void EventBridge::connectCapture(Ekos::Capture *c)
{
    if (!c) return;
    disconnect(c, nullptr, this, nullptr);
    connect(c, &Ekos::Capture::newStatus, this,
            [this](Ekos::CaptureState s, const QString &, int)
    {
        emitStatusChange(QStringLiteral("capture"), Ekos::getCaptureStatusString(s, false));
    });
    connect(c, &Ekos::Capture::newImage, this,
            [this](const QSharedPointer<Ekos::SequenceJob> &,
                   const QSharedPointer<FITSData> &data, const QString &)
    {
        if (!data) return;
        // Capture::newImage relays from both the per-train Camera signal and
        // explicit Q_EMIT newImage sites in cameraprocess.cpp, so the same
        // FITSData reaches us twice for non-final frames. Skip the duplicate.
        if (data == m_lastImageCapturedData) return;
        m_lastImageCapturedData = data;
        QJsonObject p;
        p[QStringLiteral("module")] = QStringLiteral("capture");
        p[QStringLiteral("path")]   = data->filename();
        p[QStringLiteral("hfr")]    = data->getHFR();
        QVariant v;
        if (data->getRecordValue(QStringLiteral("EXPTIME"), v)) p[QStringLiteral("exposure")] = v.toDouble();
        if (data->getRecordValue(QStringLiteral("FILTER"),  v)) p[QStringLiteral("filter")]   = v.toString();
        m_transport->broadcastSSEEvent(QStringLiteral("image_captured"), p);
    });
    connect(c, &Ekos::Capture::captureComplete, this,
            [this](const QVariantMap &metadata, const QString &trainname)
    {
        QJsonObject p;
        p[QStringLiteral("module")]    = QStringLiteral("capture");
        p[QStringLiteral("trainname")] = trainname;
        if (metadata.contains(QStringLiteral("exposure")))
            p[QStringLiteral("exposure")] = metadata[QStringLiteral("exposure")].toDouble();
        if (metadata.contains(QStringLiteral("filename")))
            p[QStringLiteral("path")]     = metadata[QStringLiteral("filename")].toString();
        m_transport->broadcastSSEEvent(QStringLiteral("capture_progress"), p);
    });
}

void EventBridge::connectGuide(Ekos::Guide *g)
{
    if (!g) return;
    disconnect(g, nullptr, this, nullptr);
    connect(g, &Ekos::Guide::newStatus, this, [this](Ekos::GuideState s)
    {
        emitStatusChange(QStringLiteral("guide"), Ekos::getGuideStatusString(s, false));
    });
    connect(g, &Ekos::Guide::newAxisDelta, this, [this](double ra, double de)
    {
        QJsonObject p;
        p[QStringLiteral("module")]   = QStringLiteral("guide");
        p[QStringLiteral("raDelta")]  = ra;
        p[QStringLiteral("decDelta")] = de;
        p[QStringLiteral("raSigma")]  = m_lastGuideSigmaRa;
        p[QStringLiteral("decSigma")] = m_lastGuideSigmaDec;
        m_guideDeviationPending = p;
        m_guideDeviationHasPending = true;
    });
    connect(g, &Ekos::Guide::newAxisSigma, this, [this](double ra, double de)
    {
        m_lastGuideSigmaRa  = ra;
        m_lastGuideSigmaDec = de;
    });
}

void EventBridge::connectFocus(Ekos::FocusModule *f)
{
    if (!f) return;
    disconnect(f, nullptr, this, nullptr);
    connect(f, &Ekos::FocusModule::newStatus, this,
            [this](Ekos::FocusState s, const QString &)
    {
        emitStatusChange(QStringLiteral("focus"), Ekos::getFocusStatusString(s, false));
    });
}

void EventBridge::connectAlign(Ekos::Align *a)
{
    if (!a) return;
    disconnect(a, nullptr, this, nullptr);
    connect(a, &Ekos::Align::newStatus, this, [this](Ekos::AlignState s)
    {
        emitStatusChange(QStringLiteral("align"), Ekos::getAlignStatusString(s, false));
    });
    connect(a, &Ekos::Align::newSolution, this, [this](const QVariantMap &solution)
    {
        QJsonObject p;
        p[QStringLiteral("module")] = QStringLiteral("align");
        for (auto it = solution.cbegin(); it != solution.cend(); ++it)
            p[it.key()] = QJsonValue::fromVariant(it.value());
        m_transport->broadcastSSEEvent(QStringLiteral("align_solution"), p);
    });
}

void EventBridge::connectScheduler(Ekos::SchedulerProcess *s)
{
    if (!s) return;
    disconnect(s, nullptr, this, nullptr);
    connect(s, &Ekos::SchedulerProcess::newStatus, this, [this](Ekos::SchedulerState st)
    {
        emitStatusChange(QStringLiteral("scheduler"), Ekos::getSchedulerStatusString(st, false));
    });
    connect(s, &Ekos::SchedulerProcess::jobStarted, this, [this](const QString &name)
    {
        QJsonObject p;
        p[QStringLiteral("module")]  = QStringLiteral("scheduler");
        p[QStringLiteral("event")]   = QStringLiteral("job_started");
        p[QStringLiteral("jobName")] = name;
        m_transport->broadcastSSEEvent(QStringLiteral("scheduler_event"), p);
    });
    connect(s, &Ekos::SchedulerProcess::jobEnded, this,
            [this](const QString &name, const QString &reason)
    {
        QJsonObject p;
        p[QStringLiteral("module")]  = QStringLiteral("scheduler");
        p[QStringLiteral("event")]   = QStringLiteral("job_ended");
        p[QStringLiteral("jobName")] = name;
        p[QStringLiteral("reason")]  = reason;
        m_transport->broadcastSSEEvent(QStringLiteral("scheduler_event"), p);
    });
}

} // namespace MCP
