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
 * @brief initIndiTools Registers generic INDI device read/write tools with the MCP tool registry.
 *
 * Tools registered:
 *   - indi_get_devices    : list all connected INDI devices
 *   - indi_get_properties : list all properties on a named device
 *   - indi_get_property   : fetch a single named property on a named device
 *   - indi_set_switch     : set a switch element on a device property
 *   - indi_set_number     : set a number element on a device property
 *   - indi_set_text       : set a text element on a device property
 */
void initIndiTools(MCP::ToolRegistry *registry, Ekos::Manager *manager);

} // namespace MCP::Tools
