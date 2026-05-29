/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guidetools.h"

#include "../mcptoolregistry.h"
#include "ekos/manager.h"
#include "ekos/guide/guide.h"
#include "ekos/ekos.h"

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
}

} // namespace MCP::Tools
