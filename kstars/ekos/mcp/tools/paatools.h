/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace Ekos
{
class Manager;
}

namespace MCP
{
class Server;
class ToolRegistry;

namespace Tools
{

// Registers Polar Alignment Assistant tools:
//   paa_start, paa_status, paa_abort, paa_select_star
void initPAATools(ToolRegistry *registry, Ekos::Manager *manager, Server *server);

} // namespace Tools
} // namespace MCP
