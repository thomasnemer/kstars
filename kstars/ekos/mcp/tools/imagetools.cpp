/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "imagetools.h"
#include "../mcpserver.h"
#include "../mcptoolregistry.h"

#include "fitsviewer/fitsdata.h"

#include <QBuffer>
#include <QByteArray>
#include <QElapsedTimer>
#include <QImage>
#include <QJsonObject>
#include <QJsonValue>

namespace MCP::Tools
{

void initImageTools(ToolRegistry *registry, Server *server)
{
    // -----------------------------------------------------------------------
    // capture_last_image_info
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("capture_last_image_info"),
        QStringLiteral("Returns metadata for the most recently captured frame: path, exposure, filter, target, "
                       "date, CCD temperature, HFR (arcsec), star count, and dimensions. "
                       "Returns {\"available\": false} if no image has been captured this session."),
        {},
        [server](const QJsonObject &, QString &) -> QJsonValue
        {
            const auto &li = server->lastImage();
            if (!li.available)
                return QJsonObject { { QStringLiteral("available"), false } };

            QJsonObject result;
            result[QStringLiteral("available")] = true;
            result[QStringLiteral("path")]      = li.path;
            result[QStringLiteral("width")]     = li.width;
            result[QStringLiteral("height")]    = li.height;
            result[QStringLiteral("hfr")]       = li.hfr;
            result[QStringLiteral("starCount")] = li.starCount;
            if (!li.filter.isEmpty())  result[QStringLiteral("filter")]   = li.filter;
            if (!li.target.isEmpty())  result[QStringLiteral("target")]   = li.target;
            if (!li.dateObs.isEmpty()) result[QStringLiteral("dateObs")]  = li.dateObs;
            if (li.exposure > 0)       result[QStringLiteral("exposure")] = li.exposure;
            result[QStringLiteral("ccdTemp")]   = li.ccdTemp;
            return result;
        }
    });

    // -----------------------------------------------------------------------
    // capture_last_image_thumbnail
    // -----------------------------------------------------------------------
    registry->registerTool({
        QStringLiteral("capture_last_image_thumbnail"),
        QStringLiteral("Returns a base64-encoded JPEG preview of the latest captured frame, useful for vision-capable LLMs. "
                       "Set maxSize to control the long-edge pixel count (default 512, capped at 1024 to keep response under ~1 MB). "
                       "Returns {\"available\": false} if no image is available."),
        {
            { QStringLiteral("maxSize"), QStringLiteral("integer"),
              QStringLiteral("Long-edge pixel cap for the thumbnail. Default 512, max 1024."), false }
        },
        [server](const QJsonObject &args, QString &error) -> QJsonValue
        {
            const auto &li = server->lastImage();
            if (!li.available || li.path.isEmpty())
                return QJsonObject { { QStringLiteral("available"), false } };

            int maxSize = args.value(QStringLiteral("maxSize")).toInt(512);
            if (maxSize <= 0)   maxSize = 512;
            if (maxSize > 1024) maxSize = 1024;

            QElapsedTimer timer;
            timer.start();
            QImage img = FITSData::FITSToImage(li.path);
            if (img.isNull()) { error = "Failed to render FITS to image"; return {}; }
            if (timer.elapsed() > 3000) { error = "FITS rendering exceeded 3 seconds"; return {}; }

            const QImage scaled = (img.width() > maxSize || img.height() > maxSize)
                                  ? img.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                                  : img;

            QByteArray jpg;
            QBuffer buf(&jpg);
            buf.open(QIODevice::WriteOnly);
            if (!scaled.save(&buf, "JPEG", 85))
            { error = "JPEG encoding failed"; return {}; }

            QJsonObject result;
            result[QStringLiteral("available")] = true;
            result[QStringLiteral("mediaType")] = QStringLiteral("image/jpeg");
            result[QStringLiteral("encoding")]  = QStringLiteral("base64");
            result[QStringLiteral("width")]     = scaled.width();
            result[QStringLiteral("height")]    = scaled.height();
            result[QStringLiteral("data")]      = QString::fromLatin1(jpg.toBase64());
            return result;
        }
    });

    registry->classify(QStringLiteral("capture_last_image_info"),      /*ro*/true, /*destr*/false, /*idemp*/true);
    registry->classify(QStringLiteral("capture_last_image_thumbnail"), /*ro*/true, /*destr*/false, /*idemp*/true);
}

} // namespace MCP::Tools
