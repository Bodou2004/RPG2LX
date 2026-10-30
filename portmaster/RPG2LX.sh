#!/bin/bash

# PortMaster Launcher Script for RPG2LX
# Lifecycle reference: https://portmaster.games/packaging.html

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

# 1. Detect PortMaster control folder across supported custom firmwares (CFWs)
controlfolder=""
for candidate in "/opt/system/Tools/PortMaster" "/opt/tools/PortMaster" \
    "$XDG_DATA_HOME/PortMaster" "/storage/.config/PortMaster" \
    "/roms/ports/PortMaster" "/roms2/ports/PortMaster" \
    "/roms/tools/PortMaster" "/roms2/tools/PortMaster"; do
    if [ -f "$candidate/control.txt" ]; then
        controlfolder="$candidate"
        break
    fi
done

if [ ! -f "$controlfolder/control.txt" ]; then
    echo "ERROR: PortMaster control.txt not found in $controlfolder"
    exit 1
fi

source "$controlfolder/control.txt" || exit 1

# 2. Source CFW specific modifications if present
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"

# 3. Initialize controller environment and device variables
get_controls || exit 1

# Locate our data beside this script, independent of SD number and mount path.
RPG2LX_PORTS_DIR="$(cd -- "$(dirname -- "$0")" && pwd -P)" || exit 1
GAMEDIR="$RPG2LX_PORTS_DIR/rpg2lx"
CONFDIR="$GAMEDIR/config"

mkdir -p "$CONFDIR" "$GAMEDIR/games" "$GAMEDIR/saves" "$GAMEDIR/logs" || exit 1
cd "$GAMEDIR" || exit 1
trap 'pm_finish' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# 4. Redirect logs with timestamping for debugging support
exec > >(tee -a "$GAMEDIR/logs/portmaster.log") 2>&1
echo "=== Starting RPG2LX PortMaster Session: $(date) ==="
echo "CFW: $CFW_NAME | Device: $DEVICE_NAME | Arch: $DEVICE_ARCH"

# 5. Environment configuration and isolation
export XDG_DATA_HOME="$CONFDIR"
export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
if [ -z "${SDL_GAMECONTROLLERCONFIG_FILE:-}" ] && [ -f "$controlfolder/gamecontrollerdb.txt" ]; then
    export SDL_GAMECONTROLLERCONFIG_FILE="$controlfolder/gamecontrollerdb.txt"
fi

# 6. Check binary existence for target architecture
BIN="./rpg2lx.${DEVICE_ARCH}"
if [ "${DEVICE_ARCH:-}" != "aarch64" ]; then
    echo "ERROR: This package contains an aarch64 binary; detected architecture: ${DEVICE_ARCH:-unknown}"
    exit 1
fi
if [ ! -f "$BIN" ]; then
    echo "ERROR: Binary $BIN not found for architecture $DEVICE_ARCH"
    exit 1
fi
chmod +x "$BIN"

# 7. The menu reads SDL GameController directly; do not duplicate input with gptokeyb.

# 8. Platform helper hook
pm_platform_helper "$GAMEDIR/rpg2lx.${DEVICE_ARCH}"

# 9. Open the multi-game library. The binary creates its own app directories.
"$BIN" launcher --home "$GAMEDIR"
RPG2LX_STATUS=$?

# 10. Clean up background tasks and restore terminal state
echo "=== RPG2LX Session Ended: $(date) ==="
exit "$RPG2LX_STATUS"
