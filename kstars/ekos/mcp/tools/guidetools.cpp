/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guidetools.h"

#include "../mcpserver.h"
#include "../mcpguidehistory.h"
#include "../mcptoolregistry.h"
#include "ekos/manager.h"
#include "ekos/guide/guide.h"
#include "ekos/ekos.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace MCP::Tools
{

void initGuideTools(MCP::ToolRegistry *registry, Ekos::Manager *manager)
{
    // guide_status — returns current guiding state and statistics
    registry->registerTool({
        "guide_status",
        "Returns the current guide module status, camera, axis deltas, sigma values, and exposure.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *guide = manager->guideModule();
            if (!guide)
            {
                error = "Guide module not available";
                return {};
            }
            const QList<double> delta = guide->axisDelta();
            const QList<double> sigma = guide->axisSigma();

            QJsonObject result;
            result["status"]   = Ekos::getGuideStatusString(guide->status(), false);
            result["camera"]   = guide->camera();
            result["raDelta"]  = delta.size() > 0 ? delta[0] : 0.0;
            result["decDelta"] = delta.size() > 1 ? delta[1] : 0.0;
            result["raSigma"]  = sigma.size() > 0 ? sigma[0] : 0.0;
            result["decSigma"] = sigma.size() > 1 ? sigma[1] : 0.0;
            result["exposure"] = guide->exposure();
            return result;
        }
    });

    // guide_calibrate — starts the calibration process
    registry->registerTool({
        "guide_calibrate",
        "Starts the guider calibration process.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *guide = manager->guideModule();
            if (!guide)
            {
                error = "Guide module not available";
                return {};
            }
            guide->calibrate();
            return QJsonObject { { "success", true } };
        }
    });

    // guide_start — starts autoguiding (requires calibration)
    registry->registerTool({
        "guide_start",
        "Starts autoguiding. The guider must be calibrated first.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *guide = manager->guideModule();
            if (!guide)
            {
                error = "Guide module not available";
                return {};
            }
            guide->guide();
            return QJsonObject { { "success", true } };
        }
    });

    // guide_stop — aborts any active guiding or calibration
    registry->registerTool({
        "guide_stop",
        "Aborts any active guiding, calibration, or dithering operation.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *guide = manager->guideModule();
            if (!guide)
            {
                error = "Guide module not available";
                return {};
            }
            guide->abort();
            return QJsonObject { { "success", true } };
        }
    });

    // guide_suspend — suspends guiding (mount corrections paused)
    registry->registerTool({
        "guide_suspend",
        "Suspends guiding so that corrections are paused without fully stopping.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *guide = manager->guideModule();
            if (!guide)
            {
                error = "Guide module not available";
                return {};
            }
            guide->suspend();
            return QJsonObject { { "success", true } };
        }
    });

    // guide_resume — resumes previously suspended guiding
    registry->registerTool({
        "guide_resume",
        "Resumes guiding after it has been suspended.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *guide = manager->guideModule();
            if (!guide)
            {
                error = "Guide module not available";
                return {};
            }
            guide->resume();
            return QJsonObject { { "success", true } };
        }
    });

    // guide_dither — triggers a random dither move
    registry->registerTool({
        "guide_dither",
        "Triggers a dither move in a random direction within the configured pixel limit.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *guide = manager->guideModule();
            if (!guide)
            {
                error = "Guide module not available";
                return {};
            }
            guide->dither();
            return QJsonObject { { "success", true } };
        }
    });

    // guide_clear_calibration — clears stored calibration data
    registry->registerTool({
        "guide_clear_calibration",
        "Clears the stored guider calibration data so calibration must be re-run.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *guide = manager->guideModule();
            if (!guide)
            {
                error = "Guide module not available";
                return {};
            }
            guide->clearCalibration();
            return QJsonObject { { "success", true } };
        }
    });

    // guide_set_exposure — sets the guider exposure time
    registry->registerTool({
        "guide_set_exposure",
        "Sets the guide camera exposure time in seconds.",
        {
            { "seconds", "number", "Exposure duration in seconds (e.g. 2.0).", true }
        },
        [manager](const QJsonObject &args, QString &error) -> QJsonValue
        {
            auto *guide = manager->guideModule();
            if (!guide)
            {
                error = "Guide module not available";
                return {};
            }
            if (!args.contains("seconds"))
            {
                error = "Parameter 'seconds' is required";
                return {};
            }
            const double seconds = args["seconds"].toDouble();
            if (seconds <= 0.0)
            {
                error = "Parameter 'seconds' must be greater than zero";
                return {};
            }
            guide->setExposure(seconds);
            return QJsonObject { { "success", true } };
        }
    });

    registry->classify(QStringLiteral("guide_status"),            /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("guide_calibrate"),         /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("guide_start"),             /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("guide_stop"),              /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("guide_suspend"),           /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("guide_resume"),            /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("guide_dither"),            /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("guide_clear_calibration"), /*ro*/false, /*destr*/true,  /*idemp*/true);
    registry->classify(QStringLiteral("guide_set_exposure"),      /*ro*/false, /*destr*/false, /*idemp*/true);
}

void initGuideHistoryTools(MCP::ToolRegistry *registry, MCP::Server *server)
{
    registry->registerTool({
        QStringLiteral("guide_history"),
        QStringLiteral("Returns recent guide samples as a time series. Useful for detecting oscillation, drift, "
                       "or settling problems that a single guide_status snapshot can't show. Each sample reports "
                       "timestampMs (epoch ms), raDelta/decDelta in arcsec, raSigma/decSigma in arcsec RMS, "
                       "sampled at the guide cadence (typically ~1 Hz). Capacity ~10 minutes; older samples are "
                       "evicted FIFO. Buffer clears when Ekos guide module disconnects (typically on Ekos stop)."),
        {
            { QStringLiteral("sinceMs"),    QStringLiteral("integer"),
              QStringLiteral("Return only samples newer than this epoch ms. Default 0 = entire buffer."), false },
            { QStringLiteral("maxSamples"), QStringLiteral("integer"),
              QStringLiteral("Cap on returned sample count (newest kept). Defaults to buffer capacity."), false }
        },
        [server](const QJsonObject &args, QString &error) -> QJsonValue
        {
            auto *hist = server->guideHistory();
            if (!hist) { error = "Guide history not available"; return {}; }

            const qint64 sinceMs = args.value(QStringLiteral("sinceMs")).toVariant().toLongLong();
            QVector<MCP::GuideSample> samples = sinceMs > 0 ? hist->samplesSince(sinceMs) : hist->samples();

            const int maxN = args.value(QStringLiteral("maxSamples")).toInt(hist->capacity());
            if (samples.size() > maxN)
                samples = samples.mid(samples.size() - maxN);

            QJsonArray arr;
            for (const auto &s : samples)
            {
                QJsonObject pt;
                pt[QStringLiteral("timestampMs")] = s.timestampMs;
                pt[QStringLiteral("raDelta")]     = s.raDelta;
                pt[QStringLiteral("decDelta")]    = s.decDelta;
                pt[QStringLiteral("raSigma")]     = s.raSigma;
                pt[QStringLiteral("decSigma")]    = s.decSigma;
                arr.append(pt);
            }
            return QJsonObject {
                { QStringLiteral("samples"),  arr },
                { QStringLiteral("capacity"), hist->capacity() }
            };
        }
    });

    registry->classify(QStringLiteral("guide_history"), /*ro*/true, /*destr*/false, /*idemp*/true);
}

} // namespace MCP::Tools
