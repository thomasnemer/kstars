/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace MCP
{
class Server;
class ToolRegistry;

namespace Tools
{

// Registers image-access tools:
//   capture_last_image_info       — metadata + HFR + star count + dimensions
//   capture_last_image_thumbnail  — base64 JPEG preview (long edge ≤ 1024 px)
//
// Both read from MCP::Server::lastImage(), which is populated by the
// Capture::newImage hook installed in Server::setCapture().
void initImageTools(ToolRegistry *registry, Server *server);

} // namespace Tools
} // namespace MCP
