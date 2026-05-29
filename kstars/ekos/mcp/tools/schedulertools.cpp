/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "schedulertools.h"
#include "../mcptoolregistry.h"

#include "ekos/manager.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulerprocess.h"
#include "ekos/ekos.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

namespace MCP
{
namespace Tools
{

void initSchedulerTools(ToolRegistry *registry, Ekos::Manager *manager)
{
    // scheduler_status — returns scheduler state, current job name, and profile
    registry->registerTool({
        "scheduler_status",
        "Returns the current scheduler state, the name of the currently executing job, and the active equipment profile.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *scheduler = manager->schedulerModule();
            if (!scheduler)
            {
                error = "Scheduler module not available";
                return {};
            }
            auto process = scheduler->process();
            if (!process)
            {
                error = "Scheduler process not available";
                return {};
            }
            QJsonObject result;
            result["status"]     = Ekos::getSchedulerStatusString(process->status(), false);
            result["currentJob"] = process->currentJobName();
            result["profile"]    = process->profile();
            return result;
        }
    });

    // scheduler_jobs — returns all scheduler jobs as a JSON array
    registry->registerTool({
        "scheduler_jobs",
        "Returns all scheduler jobs as a JSON array.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *scheduler = manager->schedulerModule();
            if (!scheduler)
            {
                error = "Scheduler module not available";
                return {};
            }
            auto process = scheduler->process();
            if (!process)
            {
                error = "Scheduler process not available";
                return {};
            }
            QString jsonStr = process->jsonJobs();
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);
            QJsonArray jobsArray;
            if (parseError.error == QJsonParseError::NoError)
            {
                if (doc.isArray())
                    jobsArray = doc.array();
                else if (doc.isObject())
                    jobsArray.append(doc.object());
            }
            QJsonObject result;
            result["jobs"] = jobsArray;
            return result;
        }
    });

    // scheduler_current_job — returns name and details of the currently executing job
    registry->registerTool({
        "scheduler_current_job",
        "Returns the name and full details of the currently executing scheduler job.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *scheduler = manager->schedulerModule();
            if (!scheduler)
            {
                error = "Scheduler module not available";
                return {};
            }
            auto process = scheduler->process();
            if (!process)
            {
                error = "Scheduler process not available";
                return {};
            }
            QString name    = process->currentJobName();
            QString jsonStr = process->currentJobJson();
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);
            QJsonObject details;
            if (parseError.error == QJsonParseError::NoError && doc.isObject())
                details = doc.object();
            QJsonObject result;
            result["name"]    = name;
            result["details"] = details;
            return result;
        }
    });

    // scheduler_start — starts the scheduler
    registry->registerTool({
        "scheduler_start",
        "Starts the scheduler to begin executing queued observation jobs.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *scheduler = manager->schedulerModule();
            if (!scheduler)
            {
                error = "Scheduler module not available";
                return {};
            }
            auto process = scheduler->process();
            if (!process)
            {
                error = "Scheduler process not available";
                return {};
            }
            process->start();
            return QJsonObject{};
        }
    });

    // scheduler_stop — stops the scheduler
    registry->registerTool({
        "scheduler_stop",
        "Stops the scheduler, halting execution of the current and queued observation jobs.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *scheduler = manager->schedulerModule();
            if (!scheduler)
            {
                error = "Scheduler module not available";
                return {};
            }
            auto process = scheduler->process();
            if (!process)
            {
                error = "Scheduler process not available";
                return {};
            }
            process->stop();
            return QJsonObject{};
        }
    });

    // scheduler_load — loads a scheduler file from disk
    registry->registerTool({
        "scheduler_load",
        "Loads a scheduler list file (.esl) from the specified path.",
        {
            { "path", "string", "Absolute path to the scheduler list file (.esl) to load.", true }
        },
        [manager](const QJsonObject &args, QString &error) -> QJsonValue
        {
            auto *scheduler = manager->schedulerModule();
            if (!scheduler)
            {
                error = "Scheduler module not available";
                return {};
            }
            auto process = scheduler->process();
            if (!process)
            {
                error = "Scheduler process not available";
                return {};
            }
            QString path = args["path"].toString();
            if (path.isEmpty())
            {
                error = "path must not be empty";
                return {};
            }
            process->loadScheduler(path);
            return QJsonObject{};
        }
    });

    registry->classify(QStringLiteral("scheduler_status"),      /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("scheduler_jobs"),        /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("scheduler_current_job"), /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("scheduler_start"),       /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("scheduler_stop"),        /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("scheduler_load"),        /*ro*/false, /*destr*/true,  /*idemp*/false);
}

} // namespace Tools
} // namespace MCP
