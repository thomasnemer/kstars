/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "paatools.h"
#include "../mcpserver.h"
#include "../mcptoolregistry.h"

#include "ekos/manager.h"
#include "ekos/align/align.h"
#include "ekos/align/polaralignmentassistant.h"

#include <QJsonObject>
#include <QJsonValue>

namespace MCP::Tools
{

static QString pahStageString(Ekos::PolarAlignmentAssistant::Stage s)
{
    switch (s)
    {
        case Ekos::PolarAlignmentAssistant::PAH_IDLE:           return QStringLiteral("idle");
        case Ekos::PolarAlignmentAssistant::PAH_FIRST_CAPTURE:  return QStringLiteral("first_capture");
        case Ekos::PolarAlignmentAssistant::PAH_FIRST_SOLVE:    return QStringLiteral("first_solve");
        case Ekos::PolarAlignmentAssistant::PAH_FIND_CP:        return QStringLiteral("find_celestial_pole");
        case Ekos::PolarAlignmentAssistant::PAH_FIRST_ROTATE:   return QStringLiteral("first_rotate");
        case Ekos::PolarAlignmentAssistant::PAH_FIRST_SETTLE:   return QStringLiteral("first_settle");
        case Ekos::PolarAlignmentAssistant::PAH_SECOND_CAPTURE: return QStringLiteral("second_capture");
        case Ekos::PolarAlignmentAssistant::PAH_SECOND_SOLVE:   return QStringLiteral("second_solve");
        case Ekos::PolarAlignmentAssistant::PAH_SECOND_ROTATE:  return QStringLiteral("second_rotate");
        case Ekos::PolarAlignmentAssistant::PAH_SECOND_SETTLE:  return QStringLiteral("second_settle");
        case Ekos::PolarAlignmentAssistant::PAH_THIRD_CAPTURE:  return QStringLiteral("third_capture");
        case Ekos::PolarAlignmentAssistant::PAH_THIRD_SOLVE:    return QStringLiteral("third_solve");
        case Ekos::PolarAlignmentAssistant::PAH_STAR_SELECT:    return QStringLiteral("star_select");
        case Ekos::PolarAlignmentAssistant::PAH_REFRESH:        return QStringLiteral("refresh");
        case Ekos::PolarAlignmentAssistant::PAH_POST_REFRESH:   return QStringLiteral("post_refresh");
    }
    return QStringLiteral("unknown");
}

void initPAATools(ToolRegistry *registry, Ekos::Manager *manager, Server *server)
{
    // -----------------------------------------------------------------------
    // paa_start
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("paa_start"),
        QStringLiteral("Begin the Polar Alignment Assistant workflow. Captures, slews, solves, and walks "
                       "the operator through azimuth/altitude corrections. The mount should be roughly pointing "
                       "near the celestial pole and tracking enabled before calling this."),
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align) { error = "Align module not available"; return {}; }
            auto *paa = align->polarAlignmentAssistant();
            if (!paa) { error = "Polar alignment assistant not available"; return {}; }
            paa->startPAHProcess();
            return QJsonObject { { QStringLiteral("started"), true } };
        }
    });

    // -----------------------------------------------------------------------
    // paa_status
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("paa_status"),
        QStringLiteral("Returns the current PAA workflow stage and the latest polar-alignment error values "
                       "(total / azimuth / altitude in degrees AND arcseconds). errorArcsec is what the operator "
                       "reads while turning the mount bolts."),
        {},
        [manager, server](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align) { error = "Align module not available"; return {}; }
            auto *paa = align->polarAlignmentAssistant();
            if (!paa) { error = "Polar alignment assistant not available"; return {}; }

            QJsonObject result;
            result[QStringLiteral("stage")]   = pahStageString(paa->getPAHStage());
            result[QStringLiteral("message")] = paa->getPAHMessage();

            const auto &s = server->polarAlignState();
            if (s.hasError)
            {
                QJsonObject deg;
                deg[QStringLiteral("total")]    = s.totalDeg;
                deg[QStringLiteral("azimuth")]  = s.azDeg;
                deg[QStringLiteral("altitude")] = s.altDeg;
                QJsonObject arcsec;
                arcsec[QStringLiteral("total")]    = s.totalDeg * 3600.0;
                arcsec[QStringLiteral("azimuth")]  = s.azDeg    * 3600.0;
                arcsec[QStringLiteral("altitude")] = s.altDeg   * 3600.0;
                result[QStringLiteral("errorDegrees")] = deg;
                result[QStringLiteral("errorArcsec")]  = arcsec;
            }
            return result;
        }
    });

    // -----------------------------------------------------------------------
    // paa_abort
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("paa_abort"),
        QStringLiteral("Cancel the in-flight PAA workflow."),
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align) { error = "Align module not available"; return {}; }
            auto *paa = align->polarAlignmentAssistant();
            if (!paa) { error = "Polar alignment assistant not available"; return {}; }
            paa->stopPAHProcess();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // -----------------------------------------------------------------------
    // paa_start_refresh
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("paa_start_refresh"),
        QStringLiteral("After paa_select_star, advance the PAA workflow to the `refresh` stage so the operator "
                       "can iteratively turn the mount's altitude/azimuth bolts while paa_status streams "
                       "errorArcsec feedback. Wraps the UI Refresh button. Call paa_stop_refresh when the "
                       "alignment error is acceptable."),
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align) { error = "Align module not available"; return {}; }
            auto *paa = align->polarAlignmentAssistant();
            if (!paa) { error = "Polar alignment assistant not available"; return {}; }
            paa->startPAHRefreshProcess();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // -----------------------------------------------------------------------
    // paa_stop_refresh
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("paa_stop_refresh"),
        QStringLiteral("Exit the PAA `refresh` stage cleanly: transitions to `post_refresh`, releases the "
                       "mount, and returns the workflow to idle. Equivalent to the UI Stop button while in "
                       "refresh — and to paa_abort, which uses the same underlying call regardless of stage."),
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align) { error = "Align module not available"; return {}; }
            auto *paa = align->polarAlignmentAssistant();
            if (!paa) { error = "Polar alignment assistant not available"; return {}; }
            paa->stopPAHProcess();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // -----------------------------------------------------------------------
    // paa_select_star
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("paa_select_star"),
        QStringLiteral("During the STAR_SELECT phase, pick the alignment reference star by its pixel "
                       "coordinates in the last captured frame. The PAA converts to a relative offset; this "
                       "tool wraps that conversion using the latest image dimensions cached by the server."),
        {
            { QStringLiteral("x"), QStringLiteral("integer"), QStringLiteral("Pixel x in the latest captured frame."), true },
            { QStringLiteral("y"), QStringLiteral("integer"), QStringLiteral("Pixel y in the latest captured frame."), true }
        },
        [manager, server](const QJsonObject &args, QString &error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align) { error = "Align module not available"; return {}; }
            auto *paa = align->polarAlignmentAssistant();
            if (!paa) { error = "Polar alignment assistant not available"; return {}; }

            // PAA captures through its own camera (the one Align is using).
            // Look up that camera's last frame specifically rather than the
            // most-recent-across-cameras default, so a stray frame from
            // another scope on a multi-train rig can't be picked up here.
            const QString paaCamera = align->camera();
            const auto &img = server->lastImageFor(paaCamera);
            if (!img.available || img.width == 0 || img.height == 0)
            { error = QStringLiteral("No image available for PAA camera '%1' — cannot resolve pixel to percentage").arg(paaCamera); return {}; }

            const double x = args[QStringLiteral("x")].toDouble();
            const double y = args[QStringLiteral("y")].toDouble();
            const double dx = (x / static_cast<double>(img.width))  * 100.0;
            const double dy = (y / static_cast<double>(img.height)) * 100.0;
            paa->setPAHCorrectionOffsetPercentage(dx, dy);
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    registry->classify(QStringLiteral("paa_start"),         /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("paa_status"),        /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("paa_abort"),         /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("paa_select_star"),   /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("paa_start_refresh"), /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("paa_stop_refresh"),  /*ro*/false, /*destr*/false, /*idemp*/true);
}

} // namespace MCP::Tools
