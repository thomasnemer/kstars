/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace MCP
{
class ToolRegistry;

namespace Tools
{

// Registers curated filter-wheel tools on the given registry:
//   filter_list, filter_change
void initFilterTools(ToolRegistry *registry);

} // namespace Tools
} // namespace MCP
