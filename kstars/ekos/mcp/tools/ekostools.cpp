/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekostools.h"

#include "../mcptoolregistry.h"
#include "ekos/manager.h"
#include "ekos/mount/mount.h"
#include "ekos/capture/capture.h"
#include "ekos/guide/guide.h"
#include "ekos/focus/focusmodule.h"
#include "ekos/align/align.h"
#include "ekos/scheduler/scheduler.h"
#include "ekos/scheduler/schedulerprocess.h"
#include "ekos/ekos.h"
#include "indi/indilistener.h"
#include "Options.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace MCP::Tools
{

// ---------------------------------------------------------------------------
// Helper: Ekos::CommunicationStatus  →  string
// ---------------------------------------------------------------------------
static QString commStatusString(Ekos::CommunicationStatus s)
{
    switch (s)
    {
        case Ekos::Idle:    return QStringLiteral("Idle");
        case Ekos::Pending: return QStringLiteral("Pending");
        case Ekos::Success: return QStringLiteral("Connected");
        case Ekos::Error:   return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}

// ---------------------------------------------------------------------------
// Helper: ekos status string (reuses same enum, different labels for ekos)
// ---------------------------------------------------------------------------
static QString ekosStatusString(Ekos::CommunicationStatus s)
{
    switch (s)
    {
        case Ekos::Idle:    return QStringLiteral("Idle");
        case Ekos::Pending: return QStringLiteral("Pending");
        case Ekos::Success: return QStringLiteral("Success");
        case Ekos::Error:   return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}

// ---------------------------------------------------------------------------
void initEkosTools(MCP::ToolRegistry *registry, Ekos::Manager *manager)
{
    // -----------------------------------------------------------------------
    // ekos_status
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("ekos_status"),
        QStringLiteral("Return the current INDI/Ekos connection status, active profile, and connected device names."),
        {},
        [manager](const QJsonObject &, QString &) -> QJsonValue
        {
            QJsonArray devices;
            if (INDIListener::Instance())
            {
                for (const auto &dev : INDIListener::Instance()->getDevices())
                    devices.append(dev->getDeviceName());
            }

            return QJsonObject {
                { QStringLiteral("indi"),    commStatusString(manager->indiStatus()) },
                { QStringLiteral("ekos"),    ekosStatusString(manager->ekosStatus()) },
                { QStringLiteral("profile"), manager->getCurrentProfile() },
                { QStringLiteral("devices"), devices }
            };
        }
    });

    // -----------------------------------------------------------------------
    // ekos_get_logs
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("ekos_get_logs"),
        QStringLiteral("Return the last 50 log lines from the specified Ekos module. "
                       "Valid values for 'module': ekos, mount, capture, guide, focus, align, scheduler."),
        {
            { QStringLiteral("module"), QStringLiteral("string"),
              QStringLiteral("Module name: ekos | mount | capture | guide | focus | align | scheduler"),
              true }
        },
        [manager](const QJsonObject &args, QString &error) -> QJsonValue
        {
            const QString mod = args[QStringLiteral("module")].toString().toLower();

            QStringList rawLog;

            if (mod == QLatin1String("ekos"))
            {
                rawLog = manager->logText();
            }
            else if (mod == QLatin1String("mount"))
            {
                auto *m = manager->mountModule();
                if (!m) { error = "Mount module not available"; return {}; }
                rawLog = m->logText();
            }
            else if (mod == QLatin1String("capture"))
            {
                auto *c = manager->captureModule();
                if (!c) { error = "Capture module not available"; return {}; }
                rawLog = c->logText();
            }
            else if (mod == QLatin1String("guide"))
            {
                auto *g = manager->guideModule();
                if (!g) { error = "Guide module not available"; return {}; }
                rawLog = g->logText();
            }
            else if (mod == QLatin1String("focus"))
            {
                auto *f = manager->focusModule();
                if (!f) { error = "Focus module not available"; return {}; }
                rawLog = f->logText();
            }
            else if (mod == QLatin1String("align"))
            {
                auto *a = manager->alignModule();
                if (!a) { error = "Align module not available"; return {}; }
                rawLog = a->logText();
            }
            else if (mod == QLatin1String("scheduler"))
            {
                auto *s = manager->schedulerModule();
                if (!s) { error = "Scheduler module not available"; return {}; }
                rawLog = s->process()->logText();
            }
            else
            {
                error = QStringLiteral("Unknown module '%1'. Use: ekos, mount, capture, guide, focus, align, scheduler").arg(mod);
                return {};
            }

            // Keep last 50 lines
            if (rawLog.size() > 50)
                rawLog = rawLog.mid(rawLog.size() - 50);

            QJsonArray lines;
            for (const auto &line : rawLog)
                lines.append(line);

            return QJsonObject { { QStringLiteral("lines"), lines } };
        }
    });

    // -----------------------------------------------------------------------
    // ekos_set_profile
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("ekos_set_profile"),
        QStringLiteral("Select the active Ekos profile by name. Must be called before ekos_start. "
                       "Fails if Ekos is already running — call ekos_stop first."),
        {
            { QStringLiteral("name"), QStringLiteral("string"),
              QStringLiteral("Profile name as returned by ekos_list_profiles."), true }
        },
        [manager](const QJsonObject &args, QString &error) -> QJsonValue
        {
            if (manager->getEkosStartingStatus() != Ekos::Idle)
            {
                error = "Ekos is currently running. Call ekos_stop first.";
                return {};
            }
            const QString name = args[QStringLiteral("name")].toString();
            if (name.isEmpty())
            {
                error = "name must not be empty";
                return {};
            }
            const bool ok = manager->setProfile(name);
            return QJsonObject { { QStringLiteral("success"), ok } };
        }
    });

    // -----------------------------------------------------------------------
    // ekos_start
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("ekos_start"),
        QStringLiteral("Start the active Ekos profile: boots its INDI server and brings Ekos online. "
                       "Returns immediately; poll ekos_status to observe the transition from Pending to Success."),
        {},
        [manager](const QJsonObject &, QString &) -> QJsonValue
        {
            manager->start();
            return QJsonObject { { QStringLiteral("started"), true } };
        }
    });

    // -----------------------------------------------------------------------
    // ekos_stop
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("ekos_stop"),
        QStringLiteral("Stop Ekos and disconnect from the INDI server. "
                       "Returns immediately; poll ekos_status to observe the transition back to Idle."),
        {},
        [manager](const QJsonObject &, QString &) -> QJsonValue
        {
            manager->stop();
            return QJsonObject{};
        }
    });

    // -----------------------------------------------------------------------
    // ekos_get_profile
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("ekos_get_profile"),
        QStringLiteral("Return the full definition of a named Ekos profile (drivers, host, port, etc.) as a "
                       "structured object. Returns {\"found\": false} if no profile by that name exists."),
        {
            { QStringLiteral("name"), QStringLiteral("string"),
              QStringLiteral("Profile name as returned by ekos_list_profiles."), true }
        },
        [manager](const QJsonObject &args, QString &error) -> QJsonValue
        {
            const QString name = args[QStringLiteral("name")].toString();
            if (name.isEmpty())
            {
                error = "name must not be empty";
                return {};
            }
            const QString raw = manager->getProfile(name);
            if (raw.isEmpty())
                return QJsonObject { { QStringLiteral("found"), false } };

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            {
                error = QStringLiteral("Failed to parse profile JSON: %1").arg(parseError.errorString());
                return {};
            }
            QJsonObject result = doc.object();
            result[QStringLiteral("found")] = true;
            return result;
        }
    });

    // -----------------------------------------------------------------------
    // ekos_list_profiles
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("ekos_list_profiles"),
        QStringLiteral("Return the list of all equipment profiles defined in Ekos."),
        {},
        [manager](const QJsonObject &, QString &) -> QJsonValue
        {
            QJsonArray profiles;
            for (const auto &p : manager->getProfiles())
                profiles.append(p);

            return QJsonObject { { QStringLiteral("profiles"), profiles } };
        }
    });

    // -----------------------------------------------------------------------
    // log_stream_info
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("log_stream_info"),
        QStringLiteral("Return the URL and instructions for connecting to the live log SSE stream."),
        {},
        [](const QJsonObject &, QString &) -> QJsonValue
        {
            return QJsonObject {
                { QStringLiteral("url"),
                  QStringLiteral("http://localhost:%1/mcp/stream").arg(Options::mCPPort()) },
                { QStringLiteral("description"),
                  QStringLiteral("Connect with Accept: text/event-stream to receive live log lines from all Ekos modules.") }
            };
        }
    });

    // -----------------------------------------------------------------------
    // ekos_get_mcp_config
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("ekos_get_mcp_config"),
        QStringLiteral("Return the MCP server URL, auth token, and a ready-to-paste Claude Desktop config block."),
        {},
        [](const QJsonObject &, QString &) -> QJsonValue
        {
            const QString url     = QStringLiteral("http://localhost:%1/mcp").arg(Options::mCPPort());
            const QString token   = Options::mCPToken();
            const QString roToken = Options::mCPReadOnlyToken();

            auto buildConfig = [&](const QString &t) -> QJsonObject {
                QJsonObject headers;
                headers[QStringLiteral("Authorization")] = QString(QStringLiteral("Bearer ") + t);
                QJsonObject srv;
                srv[QStringLiteral("url")]     = url;
                srv[QStringLiteral("headers")] = headers;
                QJsonObject servers;
                servers[QStringLiteral("kstars")] = srv;
                QJsonObject cfg;
                cfg[QStringLiteral("mcpServers")] = servers;
                return cfg;
            };

            QJsonObject fullAccess;
            fullAccess[QStringLiteral("token")]               = token;
            fullAccess[QStringLiteral("claudeDesktopConfig")] = buildConfig(token);

            QJsonObject result;
            result[QStringLiteral("url")]        = url;
            result[QStringLiteral("fullAccess")] = fullAccess;

            if (!roToken.isEmpty())
            {
                QJsonObject roAccess;
                roAccess[QStringLiteral("token")]               = roToken;
                roAccess[QStringLiteral("claudeDesktopConfig")] = buildConfig(roToken);
                roAccess[QStringLiteral("description")] =
                    QStringLiteral("Read-only access — observation tools only, no rig control");
                result[QStringLiteral("readOnly")] = roAccess;
            }

            return result;
        }
    });

    // Apply read-only classification to all Ekos info tools
    registry->classify(QStringLiteral("ekos_status"),         /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("ekos_get_logs"),       /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("ekos_list_profiles"),  /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("log_stream_info"),     /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("ekos_get_mcp_config"), /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("ekos_set_profile"),    /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("ekos_start"),          /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("ekos_stop"),           /*ro*/false, /*destr*/true,  /*idemp*/true);
    registry->classify(QStringLiteral("ekos_get_profile"),    /*ro*/true,  /*destr*/false, /*idemp*/true);
}

} // namespace MCP::Tools
