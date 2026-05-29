/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace Ekos { class Manager; }
namespace MCP  { class ToolRegistry; }

namespace MCP::Tools
{

/**
 * @brief initEkosTools Register Ekos status tools into the tool registry.
 *
 * Registers:
 *  - ekos_status       Report INDI/Ekos connection status + connected devices.
 *  - ekos_get_logs     Return the last 50 lines from a named module log.
 *  - ekos_list_profiles List all equipment profiles.
 */
void initEkosTools(MCP::ToolRegistry *registry, Ekos::Manager *manager);

} // namespace MCP::Tools
