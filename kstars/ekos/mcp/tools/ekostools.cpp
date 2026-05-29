/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekostools.h"

#include "../mcptoolregistry.h"
#include "ekos/manager.h"
#include "ekos/ekos.h"
#include "indi/indilistener.h"

#include <QJsonArray>
#include <QJsonObject>
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
}

} // namespace MCP::Tools
