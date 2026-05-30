/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace MCP { class ToolRegistry; class Server; }
namespace Ekos { class Manager; }

namespace MCP::Tools
{

/**
 * @brief Register all guiding-related MCP tools into the given registry.
 *
 * Tools registered:
 *  - guide_status
 *  - guide_calibrate
 *  - guide_start
 *  - guide_stop
 *  - guide_suspend
 *  - guide_resume
 *  - guide_dither
 *  - guide_clear_calibration
 *  - guide_set_exposure
 */
void initGuideTools(MCP::ToolRegistry *registry, Ekos::Manager *manager);

// Registers guide_history. Kept separate from initGuideTools so the latter
// can stay manager-only — the history buffer is server-side state populated
// from Server's GuideHistory member, not pulled from Ekos::Guide on demand.
void initGuideHistoryTools(MCP::ToolRegistry *registry, MCP::Server *server);

} // namespace MCP::Tools
