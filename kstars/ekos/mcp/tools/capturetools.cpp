/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capturetools.h"

#include "../mcptoolregistry.h"
#include "ekos/manager.h"
#include "ekos/capture/capture.h"
#include "ekos/ekos.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace MCP::Tools
{

void initCaptureTools(MCP::ToolRegistry *registry, Ekos::Manager *manager)
{
    // capture_status — returns current capture module status
    registry->registerTool({
        "capture_status",
        "Returns the current capture module status, job counts, active camera and filter.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            QJsonObject result;
            result["status"]      = Ekos::getCaptureStatusString(capture->status(), false);
            result["jobCount"]    = capture->getJobCount();
            result["pendingJobs"] = capture->getPendingJobCount();
            result["camera"]      = capture->camera();
            result["filter"]      = capture->filter();
            return result;
        }
    });

    // capture_get_jobs — returns details for every job in the sequence queue
    registry->registerTool({
        "capture_get_jobs",
        "Returns the list of sequence jobs with their state, filter, and image progress.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            QJsonArray jobs;
            const int count = capture->getJobCount();
            for (int i = 0; i < count; ++i)
            {
                QJsonObject job;
                job["index"]    = i;
                job["state"]    = capture->getJobState(i);
                job["filter"]   = capture->getJobFilterName(i);
                job["progress"] = capture->getJobImageProgress(i);
                job["count"]    = capture->getJobImageCount(i);
                jobs.append(job);
            }
            QJsonObject result;
            result["jobs"] = jobs;
            return result;
        }
    });

    // capture_start — starts or resumes the capture sequence
    registry->registerTool({
        "capture_start",
        "Starts the capture sequence.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            capture->start();
            return QJsonObject { { "success", true } };
        }
    });

    // capture_stop — stops the capture sequence (returns to idle)
    registry->registerTool({
        "capture_stop",
        "Stops the capture sequence and returns the module to idle.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            capture->stop();
            return QJsonObject { { "success", true } };
        }
    });

    // capture_abort — aborts the capture sequence immediately
    registry->registerTool({
        "capture_abort",
        "Aborts the capture sequence immediately.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            capture->abort();
            return QJsonObject { { "success", true } };
        }
    });

    // capture_suspend — suspends the capture sequence
    registry->registerTool({
        "capture_suspend",
        "Suspends the capture sequence; it can be resumed later.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            capture->suspend();
            return QJsonObject { { "success", true } };
        }
    });

    // capture_load_sequence — loads an ESQ sequence file
    registry->registerTool({
        "capture_load_sequence",
        "Loads an Ekos Sequence Queue (.esq) file from the given path.",
        {
            { "path", "string", "Full path to the .esq sequence file to load.", true }
        },
        [manager](const QJsonObject &args, QString &error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            const QString path = args["path"].toString();
            if (path.isEmpty())
            {
                error = "Parameter 'path' is required";
                return {};
            }
            const bool ok = capture->loadSequenceQueue(path);
            QJsonObject result;
            result["success"] = ok;
            return result;
        }
    });

    // capture_set_target — sets the target name for the sequence
    registry->registerTool({
        "capture_set_target",
        "Sets the target object name used when saving captured images.",
        {
            { "name", "string", "Target object name (e.g. 'M42').", true }
        },
        [manager](const QJsonObject &args, QString &error) -> QJsonValue
        {
            auto *capture = manager->captureModule();
            if (!capture)
            {
                error = "Capture module not available";
                return {};
            }
            const QString name = args["name"].toString();
            if (name.isEmpty())
            {
                error = "Parameter 'name' is required";
                return {};
            }
            capture->setTargetName(name);
            return QJsonObject { { "success", true } };
        }
    });

    registry->classify(QStringLiteral("capture_status"),        /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("capture_get_jobs"),      /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("capture_start"),         /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("capture_stop"),          /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("capture_abort"),         /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("capture_suspend"),       /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("capture_load_sequence"), /*ro*/false, /*destr*/true,  /*idemp*/false);
    registry->classify(QStringLiteral("capture_set_target"),    /*ro*/false, /*destr*/false, /*idemp*/true);
}

} // namespace MCP::Tools
