#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd "$(dirname "$0")/../.."
isolated="$(mktemp -d)"
trap 'rm -rf "$isolated"' EXIT
# A fresh bus and config root ensure neither reads nor writes touch user preferences.
XDG_CONFIG_HOME="$isolated" dbus-run-session -- bash -euo pipefail -c '
 dconf write /com/tlolabs/ewaf/default-weekday 2
 dconf write /com/tlolabs/ewaf/range-start "\"09-03-2026\""
 dconf write /com/tlolabs/ewaf/automatic-updates false
 dconf write /com/tlolabs/ewaf/update-last-success "int64 123"
 dotnet tests/avalonia/bin/Release/net10.0/EWAF.Tests.dll --preferences-migration
'
