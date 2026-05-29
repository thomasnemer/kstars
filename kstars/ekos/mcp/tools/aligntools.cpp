/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "aligntools.h"
#include "../mcptoolregistry.h"

#include "ekos/manager.h"
#include "ekos/align/align.h"
#include "ekos/ekos.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QList>

namespace MCP
{
namespace Tools
{

void initAlignTools(ToolRegistry *registry, Ekos::Manager *manager)
{
    // align_status — returns current align state, camera, and FOV
    registry->registerTool({
        "align_status",
        "Returns the current alignment module status, active camera, and field of view dimensions.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            QList<double> fovData = align->fov();
            QJsonObject fovObj;
            if (fovData.size() >= 2)
            {
                fovObj["width"]  = fovData[0];
                fovObj["height"] = fovData[1];
            }
            QJsonObject result;
            result["status"] = Ekos::getAlignStatusString(align->status(), false);
            result["camera"] = align->camera();
            result["fov"]    = fovObj;
            return result;
        }
    });

    // align_solve — captures a frame and runs plate-solving asynchronously
    registry->registerTool({
        "align_solve",
        "Captures a frame and starts plate-solving asynchronously. Poll align_result for the solution.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            align->captureAndSolve();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    // align_result — returns the last plate-solve solution
    registry->registerTool({
        "align_result",
        "Returns the last plate-solve solution (RA in hours, Dec in degrees, orientation in degrees). Returns {\"available\": false} if no solution is available yet.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            // getSolutionResult() returns [orientation, RA, DEC]
            QList<double> sol = align->getSolutionResult();
            if (sol.size() < 3 || sol[1] <= Ekos::INVALID_VALUE)
            {
                QJsonObject result;
                result["available"] = false;
                return result;
            }
            // fov() returns [width, height, pixelScale]
            QList<double> fovData = align->fov();
            QJsonObject result;
            result["orientation"] = sol[0];
            result["ra"]          = sol[1];
            result["dec"]         = sol[2];
            if (fovData.size() >= 3)
                result["pixscale"] = fovData[2];
            result["available"]   = true;
            return result;
        }
    });

    // align_load_and_slew — loads a FITS file and slews to its coordinates
    registry->registerTool({
        "align_load_and_slew",
        "Loads a FITS file, plate-solves it, and slews the mount to the solved coordinates.",
        {
            { "path", "string", "Absolute path to the FITS file to load and solve.", true }
        },
        [manager](const QJsonObject &args, QString &error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            QString path = args["path"].toString();
            if (path.isEmpty())
            {
                error = "path must not be empty";
                return {};
            }
            align->loadAndSlew(path);
            return QJsonObject{{"success", true}};
        }
    });

    // align_abort — aborts the current alignment operation
    registry->registerTool({
        "align_abort",
        "Aborts the current alignment or plate-solving operation.",
        {},
        [manager](const QJsonObject &, QString &error) -> QJsonValue
        {
            auto *align = manager->alignModule();
            if (!align)
            {
                error = "Align module not available";
                return {};
            }
            align->abort();
            return QJsonObject { { QStringLiteral("success"), true } };
        }
    });

    registry->classify(QStringLiteral("align_status"),        /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("align_solve"),         /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("align_result"),        /*ro*/true,  /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("align_load_and_slew"), /*ro*/false, /*destr*/false, /*idemp*/false);
    registry->classify(QStringLiteral("align_abort"),         /*ro*/false, /*destr*/false, /*idemp*/true);
}

} // namespace Tools
} // namespace MCP
