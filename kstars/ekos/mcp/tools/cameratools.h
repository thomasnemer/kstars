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

// Registers curated camera tools on the given registry:
//   camera_list, camera_status, camera_set_cooling, camera_capture,
//   camera_abort_exposure
//
// These talk directly to the ISD::Camera device — bypassing the Ekos
// Capture sequence queue — and are intended for ad-hoc inspection and
// single-frame exposures. For full imaging runs use capture_load_sequence
// + capture_start from capturetools.
void initCameraTools(ToolRegistry *registry);

} // namespace Tools
} // namespace MCP
