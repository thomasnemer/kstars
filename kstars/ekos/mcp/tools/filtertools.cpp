/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "filtertools.h"
#include "devicelookup.h"
#include "../mcptoolregistry.h"

#include "indi/indifilterwheel.h"
#include "indi/indilistener.h"
#include "indi/indistd.h"

#include <basedevice.h>
#include <indiproperty.h>
#include <indipropertynumber.h>
#include <indipropertytext.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace MCP::Tools
{

static ISD::FilterWheel *resolveFilterWheel(const QString &name, QString &error)
{
    if (!name.isEmpty())
    {
        auto dev = findDeviceByName(name, error);
        if (!dev) return nullptr;
        // ISD::FilterWheel is owned by GenericDevice via m_ConcreteDevices, not a
        // subclass of it — use the typed accessor instead of a dynamic_cast.
        auto *fw = dev->getFilterWheel();
        if (!fw) { error = QStringLiteral("Device '%1' is not a filter wheel").arg(name); return nullptr; }
        return fw;
    }
    auto dev = findFirstDeviceByInterface(INDI::BaseDevice::FILTER_INTERFACE, error);
    if (!dev) return nullptr;
    return dev->getFilterWheel();
}

void initFilterTools(ToolRegistry *registry)
{
    // -----------------------------------------------------------------------
    // filter_list
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("filter_list"),
        QStringLiteral("Return the connected filter wheel, the slot/name mapping, and the current slot."),
        {
            { QStringLiteral("device"), QStringLiteral("string"),
              QStringLiteral("Filter wheel device name. Defaults to first connected wheel."), false }
        },
        [](const QJsonObject &args, QString &error) -> QJsonValue
        {
            auto fw = resolveFilterWheel(args[QStringLiteral("device")].toString(), error);
            if (!fw) return {};

            QJsonObject result;
            result[QStringLiteral("device")] = fw->getDeviceName();

            // Current slot from FILTER_SLOT (1-based per INDI convention).
            INDI::Property slotProp = fw->getProperty(QStringLiteral("FILTER_SLOT"));
            if (slotProp.isValid() && slotProp.getType() == INDI_NUMBER)
            {
                auto nvp = INDI::PropertyNumber(slotProp);
                if (nvp.count() >= 1)
                    result[QStringLiteral("current")] = static_cast<int>(nvp[0].getValue());
            }

            // Filter labels from FILTER_NAME (one text element per slot).
            QJsonArray filters;
            INDI::Property nameProp = fw->getProperty(QStringLiteral("FILTER_NAME"));
            if (nameProp.isValid() && nameProp.getType() == INDI_TEXT)
            {
                auto tvp = INDI::PropertyText(nameProp);
                for (size_t i = 0; i < tvp.count(); ++i)
                {
                    QJsonObject entry;
                    entry[QStringLiteral("slot")] = static_cast<int>(i + 1);
                    entry[QStringLiteral("name")] = QString::fromUtf8(tvp[i].getText());
                    filters.append(entry);
                }
            }
            result[QStringLiteral("filters")] = filters;
            return result;
        }
    });

    // -----------------------------------------------------------------------
    // filter_change
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("filter_change"),
        QStringLiteral("Move the filter wheel to the requested 1-based slot. Returns immediately; poll filter_list "
                       "to confirm completion."),
        {
            { QStringLiteral("device"), QStringLiteral("string"),  QStringLiteral("Filter wheel device name. Defaults to first connected wheel."), false },
            { QStringLiteral("slot"),   QStringLiteral("integer"), QStringLiteral("1-based slot index."), true }
        },
        [](const QJsonObject &args, QString &error) -> QJsonValue
        {
            const int slot = args[QStringLiteral("slot")].toInt();
            if (slot < 1) { error = "slot must be >= 1"; return {}; }

            auto fw = resolveFilterWheel(args[QStringLiteral("device")].toString(), error);
            if (!fw) return {};

            if (!fw->setPosition(static_cast<uint8_t>(slot))) { error = "Filter change rejected by driver"; return {}; }
            return QJsonObject { { QStringLiteral("started"), true } };
        }
    });

    registry->classify(QStringLiteral("filter_list"),   /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("filter_change"), /*ro*/false, /*destr*/false, /*idemp*/true);
}

} // namespace MCP::Tools
