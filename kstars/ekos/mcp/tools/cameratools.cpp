/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cameratools.h"
#include "devicelookup.h"
#include "../mcptoolregistry.h"

#include "indi/indicamera.h"
#include "indi/indicamerachip.h"
#include "indi/indilistener.h"
#include "indi/indistd.h"

#include <basedevice.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace MCP::Tools
{

static ISD::Camera *resolveCamera(const QString &name, QString &error)
{
    if (!name.isEmpty())
    {
        auto dev = findDeviceByName(name, error);
        if (!dev) return nullptr;
        // ISD::Camera is not a subclass of GenericDevice — it's owned by it via
        // m_ConcreteDevices. Use the typed accessor instead of a dynamic_cast.
        auto *cam = dev->getCamera();
        if (!cam) { error = QStringLiteral("Device '%1' is not a camera").arg(name); return nullptr; }
        return cam;
    }
    auto dev = findFirstDeviceByInterface(INDI::BaseDevice::CCD_INTERFACE, error);
    if (!dev) return nullptr;
    return dev->getCamera();
}

void initCameraTools(ToolRegistry *registry)
{
    // -----------------------------------------------------------------------
    // camera_list
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("camera_list"),
        QStringLiteral("List all connected INDI camera devices."),
        {},
        [](const QJsonObject &, QString &) -> QJsonValue
        {
            QJsonArray cameras;
            if (INDIListener::Instance())
            {
                for (const auto &dev : INDIListener::devicesByInterface(INDI::BaseDevice::CCD_INTERFACE))
                {
                    QJsonObject o;
                    o[QStringLiteral("name")] = dev->getDeviceName();
                    cameras.append(o);
                }
            }
            return QJsonObject { { QStringLiteral("cameras"), cameras } };
        }
    });

    // -----------------------------------------------------------------------
    // camera_status
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("camera_status"),
        QStringLiteral("Return camera status: temperature, cooler state, gain (if supported), binning, and whether an exposure is in progress."),
        {
            { QStringLiteral("name"), QStringLiteral("string"),
              QStringLiteral("Device name. Defaults to the first connected camera."), false }
        },
        [](const QJsonObject &args, QString &error) -> QJsonValue
        {
            auto cam = resolveCamera(args[QStringLiteral("name")].toString(), error);
            if (!cam) return {};

            QJsonObject result;
            result[QStringLiteral("name")] = cam->getDeviceName();

            if (cam->hasCooler())
            {
                double t = 0.0;
                if (cam->getTemperature(&t))
                    result[QStringLiteral("temperature")] = t;
                result[QStringLiteral("coolerOn")] = cam->isCoolerOn();
                result[QStringLiteral("canCool")]  = cam->canCool();
            }

            if (cam->hasGain())
            {
                double g = 0.0;
                if (cam->getGain(&g))
                    result[QStringLiteral("gain")] = g;
            }

            if (auto *chip = cam->getChip(ISD::CameraChip::PRIMARY_CCD))
            {
                int bx = 1, by = 1;
                if (chip->getBinning(&bx, &by))
                {
                    result[QStringLiteral("binningX")] = bx;
                    result[QStringLiteral("binningY")] = by;
                }
                result[QStringLiteral("capturing")] = chip->isCapturing();
            }

            return result;
        }
    });

    // -----------------------------------------------------------------------
    // camera_set_cooling
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("camera_set_cooling"),
        QStringLiteral("Enable/disable the camera cooler and optionally set a target temperature in °C."),
        {
            { QStringLiteral("name"),    QStringLiteral("string"),  QStringLiteral("Device name. Defaults to first connected camera."), false },
            { QStringLiteral("enable"),  QStringLiteral("boolean"), QStringLiteral("Toggle the cooler on/off."), true },
            { QStringLiteral("targetC"), QStringLiteral("number"),  QStringLiteral("Target temperature in degrees Celsius. Optional."), false }
        },
        [](const QJsonObject &args, QString &error) -> QJsonValue
        {
            auto cam = resolveCamera(args[QStringLiteral("name")].toString(), error);
            if (!cam) return {};
            if (!cam->hasCoolerControl()) { error = "Camera does not expose cooler control"; return {}; }

            // Write setpoint first, then the cooler switch. Some INDI drivers (incl. the CCD
            // Simulator) react to a CCD_TEMPERATURE write by toggling CCD_COOLER off, so the
            // explicit cooler-control write has to be the last property update we send.
            if (args.contains(QStringLiteral("targetC")))
                cam->setTemperature(args[QStringLiteral("targetC")].toDouble());

            const bool enable = args[QStringLiteral("enable")].toBool();
            cam->setCoolerControl(enable);

            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // -----------------------------------------------------------------------
    // camera_capture
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("camera_capture"),
        QStringLiteral("Start a single ad-hoc exposure on the primary chip. For sequenced imaging use the capture_* tools instead, "
                       "which cooperate with the Ekos Capture queue, dithering, and meridian flips."),
        {
            { QStringLiteral("name"),     QStringLiteral("string"),  QStringLiteral("Device name. Defaults to first connected camera."), false },
            { QStringLiteral("exposure"), QStringLiteral("number"),  QStringLiteral("Exposure time in seconds. Must be > 0."), true },
            { QStringLiteral("binning"),  QStringLiteral("integer"), QStringLiteral("Symmetric binning factor (1-4). Defaults to 1."), false }
        },
        [](const QJsonObject &args, QString &error) -> QJsonValue
        {
            const double exposure = args[QStringLiteral("exposure")].toDouble();
            if (exposure <= 0.0) { error = "exposure must be > 0"; return {}; }

            auto cam = resolveCamera(args[QStringLiteral("name")].toString(), error);
            if (!cam) return {};
            auto *chip = cam->getChip(ISD::CameraChip::PRIMARY_CCD);
            if (!chip) { error = "Camera has no primary chip"; return {}; }

            const int bin = args.value(QStringLiteral("binning")).toInt(1);
            if (bin < 1 || bin > 4) { error = "binning must be in [1, 4]"; return {}; }
            chip->setBinning(bin, bin);

            if (!chip->capture(exposure)) { error = "Capture request rejected by driver"; return {}; }
            return QJsonObject { { QStringLiteral("started"), true } };
        }
    });

    // -----------------------------------------------------------------------
    // camera_abort_exposure
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("camera_abort_exposure"),
        QStringLiteral("Abort the current exposure on the primary chip."),
        {
            { QStringLiteral("name"), QStringLiteral("string"),
              QStringLiteral("Device name. Defaults to first connected camera."), false }
        },
        [](const QJsonObject &args, QString &error) -> QJsonValue
        {
            auto cam = resolveCamera(args[QStringLiteral("name")].toString(), error);
            if (!cam) return {};
            auto *chip = cam->getChip(ISD::CameraChip::PRIMARY_CCD);
            if (!chip) { error = "Camera has no primary chip"; return {}; }
            chip->abortExposure();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    registry->classify(QStringLiteral("camera_list"),           /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("camera_status"),         /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("camera_set_cooling"),    /*ro*/false, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("camera_capture"),        /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("camera_abort_exposure"), /*ro*/false, /*destr*/false, /*idemp*/true);
}

} // namespace MCP::Tools
