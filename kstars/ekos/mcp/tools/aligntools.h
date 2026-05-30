/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QJsonObject>
#include <QList>

namespace Ekos
{
class Manager;
}

namespace MCP
{

class ToolRegistry;

namespace Tools
{

void initAlignTools(ToolRegistry *registry, Ekos::Manager *manager);

// Build the align_result JSON payload from raw solver outputs. Exposed for
// regression testing of the unit (degrees → hours) and epoch (J2000 → JNow)
// conversions.
//
// solution: [orientation_deg, ra_deg_j2000, dec_deg_j2000] as returned by
//           Ekos::Align::getSolutionResult().
// fov:      [width, height, pixscale_arcsec_per_pixel] as returned by
//           Ekos::Align::fov(); pixscale is read from index 2 if present.
//
// Returns {"available": false} when the solution is missing or invalid.
// Otherwise returns a payload with ra/dec in JNow (hours/degrees) and
// ra_j2000/dec_j2000 in J2000 (hours/degrees). The JNow conversion uses
// the current KStarsData clock, so callers must ensure KStarsData is
// initialised before invoking this in production code.
QJsonObject makeAlignResultPayload(const QList<double> &solution, const QList<double> &fov);

} // namespace Tools
} // namespace MCP
