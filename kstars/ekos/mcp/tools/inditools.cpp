/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inditools.h"
#include "../mcptoolregistry.h"

#include "indi/indilistener.h"
#include "indi/indistd.h"

#include <indidevapi.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace MCP::Tools
{

// ---------------------------------------------------------------------------
// Helper: find a device by name, return nullptr + set error on failure
// ---------------------------------------------------------------------------
static QSharedPointer<ISD::GenericDevice> findDevice(const QString &name, QString &error)
{
    const auto &devices = INDIListener::Instance()->getDevices();
    auto it = std::find_if(devices.begin(), devices.end(),
        [&name](const QSharedPointer<ISD::GenericDevice> &d) {
            return d->getDeviceName() == name;
        });
    if (it == devices.end())
    {
        error = QString("Device not found: %1").arg(name);
        return {};
    }
    return *it;
}

// ---------------------------------------------------------------------------
// Helper: map INDI_PROPERTY_TYPE to a human-readable string
// ---------------------------------------------------------------------------
static QString propTypeString(INDI_PROPERTY_TYPE t)
{
    switch (t)
    {
        case INDI_NUMBER: return QStringLiteral("number");
        case INDI_SWITCH: return QStringLiteral("switch");
        case INDI_TEXT:   return QStringLiteral("text");
        case INDI_LIGHT:  return QStringLiteral("light");
        default:          return QStringLiteral("unknown");
    }
}

// ---------------------------------------------------------------------------
// Helper: map IPState to a human-readable string
// ---------------------------------------------------------------------------
static QString stateString(IPState s)
{
    switch (s)
    {
        case IPS_IDLE:  return QStringLiteral("Idle");
        case IPS_OK:    return QStringLiteral("Ok");
        case IPS_BUSY:  return QStringLiteral("Busy");
        case IPS_ALERT: return QStringLiteral("Alert");
        default:        return QStringLiteral("Unknown");
    }
}

// ---------------------------------------------------------------------------
// Helper: serialise a single INDI::Property into the MCP JSON format
//         { "name":…, "type":…, "state":…, "elements":[…] }
// ---------------------------------------------------------------------------
static QJsonObject serialiseProperty(INDI::Property prop)
{
    QJsonObject obj;
    obj["name"]  = prop.getName();
    obj["type"]  = propTypeString(prop.getType());
    obj["state"] = stateString(prop.getState());

    QJsonArray elements;

    switch (prop.getType())
    {
        case INDI_SWITCH:
        {
            INDI::PropertySwitch svp(prop);
            for (size_t i = 0; i < svp.count(); ++i)
            {
                QJsonObject e;
                e["name"]  = svp[i].getName();
                e["value"] = (svp[i].getState() == ISS_ON)
                             ? QStringLiteral("On") : QStringLiteral("Off");
                elements.append(e);
            }
            break;
        }
        case INDI_NUMBER:
        {
            INDI::PropertyNumber nvp(prop);
            for (size_t i = 0; i < nvp.count(); ++i)
            {
                QJsonObject e;
                e["name"]  = nvp[i].getName();
                e["value"] = nvp[i].getValue();
                elements.append(e);
            }
            break;
        }
        case INDI_TEXT:
        {
            INDI::PropertyText tvp(prop);
            for (size_t i = 0; i < tvp.count(); ++i)
            {
                QJsonObject e;
                e["name"]  = tvp[i].getName();
                e["value"] = tvp[i].getText();
                elements.append(e);
            }
            break;
        }
        case INDI_LIGHT:
        {
            INDI::PropertyLight lvp(prop);
            for (size_t i = 0; i < lvp.count(); ++i)
            {
                QJsonObject e;
                e["name"]  = lvp[i].getName();
                e["value"] = stateString(lvp[i].getState());
                elements.append(e);
            }
            break;
        }
        default:
            break;
    }

    obj["elements"] = elements;
    return obj;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
void initIndiTools(MCP::ToolRegistry *registry, Ekos::Manager * /*manager*/)
{
    // ------------------------------------------------------------------
    // indi_get_devices
    // ------------------------------------------------------------------
    registry->registerTool({
        "indi_get_devices",
        "Lists all connected INDI devices with their name and driver interface flags.",
        {},
        [](const QJsonObject & /*args*/, QString & /*error*/) -> QJsonValue
        {
            QJsonArray arr;
            for (const auto &dev : INDIListener::Instance()->getDevices())
            {
                QJsonObject obj;
                obj["name"]      = dev->getDeviceName();
                obj["interface"] = static_cast<int>(dev->getDriverInterface());
                arr.append(obj);
            }
            return QJsonObject{{ "devices", arr }};
        }
    });

    // ------------------------------------------------------------------
    // indi_get_properties
    // ------------------------------------------------------------------
    registry->registerTool({
        "indi_get_properties",
        "Returns all properties of a named INDI device.",
        {
            { "device", "string", "Name of the INDI device", true }
        },
        [](const QJsonObject &args, QString &error) -> QJsonValue
        {
            const QString name = args["device"].toString();
            auto device = findDevice(name, error);
            if (!device)
                return {};

            QJsonArray props;
            for (const INDI::Property &prop : device->getProperties())
                props.append(serialiseProperty(prop));

            return QJsonObject{{ "properties", props }};
        }
    });

    // ------------------------------------------------------------------
    // indi_get_property
    // ------------------------------------------------------------------
    registry->registerTool({
        "indi_get_property",
        "Returns a single named property of an INDI device.",
        {
            { "device",   "string", "Name of the INDI device",   true },
            { "property", "string", "Name of the INDI property", true }
        },
        [](const QJsonObject &args, QString &error) -> QJsonValue
        {
            const QString devName  = args["device"].toString();
            const QString propName = args["property"].toString();

            auto device = findDevice(devName, error);
            if (!device)
                return {};

            INDI::Property prop = device->getProperty(propName);
            if (!prop.isValid())
            {
                error = QString("Property not found: %1 on device %2")
                            .arg(propName, devName);
                return {};
            }

            return serialiseProperty(prop);
        }
    });

    // ------------------------------------------------------------------
    // indi_set_switch
    // ------------------------------------------------------------------
    registry->registerTool({
        "indi_set_switch",
        "Directly sets an INDI switch element to On or Off. "
        "Use with caution — incorrect values can misconfigure hardware.",
        {
            { "device",   "string", "Name of the INDI device",                        true },
            { "property", "string", "Name of the INDI switch property",               true },
            { "element",  "string", "Name of the switch element to set",              true },
            { "state",    "string", "Desired state: \"On\" or \"Off\"",               true }
        },
        [](const QJsonObject &args, QString &error) -> QJsonValue
        {
            const QString devName   = args["device"].toString();
            const QString propName  = args["property"].toString();
            const QString elemName  = args["element"].toString();
            const QString stateStr  = args["state"].toString();

            if (stateStr != "On" && stateStr != "Off")
            {
                error = "state must be \"On\" or \"Off\"";
                return {};
            }

            auto device = findDevice(devName, error);
            if (!device)
                return {};

            INDI::Property prop = device->getProperty(propName);
            if (!prop.isValid() || prop.getType() != INDI_SWITCH)
            {
                error = QString("Switch property not found: %1 on device %2")
                            .arg(propName, devName);
                return {};
            }

            INDI::PropertySwitch svp(prop);
            auto elem = svp.findWidgetByName(elemName.toLatin1().constData());
            if (!elem)
            {
                error = QString("Switch element not found: %1").arg(elemName);
                return {};
            }

            // Respect the property's switch rule. Setting a single element on a
            // 1-of-many group without turning the previously-on element off
            // gets silently rejected by the driver, so the rule needs to be
            // applied client-side — same pattern as ISD::Camera::setCoolerControl
            // and ISD::Focuser::focusIn.
            const bool turningOn = (stateStr == "On");
            switch (svp.getRule())
            {
                case ISR_1OFMANY:
                    if (!turningOn)
                    {
                        error = QString("Switch '%1' on '%2' is 1-of-many; turn another element On instead of turning '%3' Off.")
                                    .arg(propName, devName, elemName);
                        return {};
                    }
                    svp.reset();
                    elem->setState(ISS_ON);
                    break;
                case ISR_ATMOST1:
                    if (turningOn) svp.reset();
                    elem->setState(turningOn ? ISS_ON : ISS_OFF);
                    break;
                case ISR_NOFMANY:
                default:
                    elem->setState(turningOn ? ISS_ON : ISS_OFF);
                    break;
            }
            device->sendNewProperty(prop);

            return QJsonObject{{ "success", true }};
        }
    });

    // ------------------------------------------------------------------
    // indi_set_number
    // ------------------------------------------------------------------
    registry->registerTool({
        "indi_set_number",
        "Directly sets an INDI number element to the given value. "
        "Use with caution — incorrect values can misconfigure hardware.",
        {
            { "device",   "string", "Name of the INDI device",           true },
            { "property", "string", "Name of the INDI number property",  true },
            { "element",  "string", "Name of the number element to set", true },
            { "value",    "number", "Numeric value to assign",           true }
        },
        [](const QJsonObject &args, QString &error) -> QJsonValue
        {
            const QString devName  = args["device"].toString();
            const QString propName = args["property"].toString();
            const QString elemName = args["element"].toString();
            const double  value    = args["value"].toDouble();

            auto device = findDevice(devName, error);
            if (!device)
                return {};

            INDI::Property prop = device->getProperty(propName);
            if (!prop.isValid() || prop.getType() != INDI_NUMBER)
            {
                error = QString("Number property not found: %1 on device %2")
                            .arg(propName, devName);
                return {};
            }

            INDI::PropertyNumber nvp(prop);
            auto elem = nvp.findWidgetByName(elemName.toLatin1().constData());
            if (!elem)
            {
                error = QString("Number element not found: %1").arg(elemName);
                return {};
            }

            elem->setValue(value);
            device->sendNewProperty(prop);

            return QJsonObject{{ "success", true }};
        }
    });

    // ------------------------------------------------------------------
    // indi_set_text
    // ------------------------------------------------------------------
    registry->registerTool({
        "indi_set_text",
        "Directly sets an INDI text element to the given string value. "
        "Use with caution — incorrect values can misconfigure hardware.",
        {
            { "device",   "string", "Name of the INDI device",         true },
            { "property", "string", "Name of the INDI text property",  true },
            { "element",  "string", "Name of the text element to set", true },
            { "value",    "string", "String value to assign",          true }
        },
        [](const QJsonObject &args, QString &error) -> QJsonValue
        {
            const QString devName  = args["device"].toString();
            const QString propName = args["property"].toString();
            const QString elemName = args["element"].toString();
            const QString value    = args["value"].toString();

            auto device = findDevice(devName, error);
            if (!device)
                return {};

            INDI::Property prop = device->getProperty(propName);
            if (!prop.isValid() || prop.getType() != INDI_TEXT)
            {
                error = QString("Text property not found: %1 on device %2")
                            .arg(propName, devName);
                return {};
            }

            INDI::PropertyText tvp(prop);
            auto elem = tvp.findWidgetByName(elemName.toLatin1().constData());
            if (!elem)
            {
                error = QString("Text element not found: %1").arg(elemName);
                return {};
            }

            elem->setText(value.toLatin1().constData());
            device->sendNewProperty(prop);

            return QJsonObject{{ "success", true }};
        }
    });
}

} // namespace MCP::Tools
