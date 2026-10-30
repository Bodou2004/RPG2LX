#!/bin/bash

# PortMaster Launcher Script for RPG2LX
# Standard Lifecycle Reference: https://portmaster.games/port-scripting.html

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

# 1. Detect PortMaster control folder across supported custom firmwares (CFWs)
if [ -d "/opt/system/Tools/PortMaster/" ]; then
    controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
    controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
    controlfolder="$XDG_DATA_HOME/PortMaster"
else
    controlfolder="/roms/ports/PortMaster"
fi

if [ ! -f "$controlfolder/control.txt" ]; then
    echo "ERROR: PortMaster control.txt not found in $controlfolder"
    exit 1
fi

source "$controlfolder/control.txt"

# 2. Source CFW specific modifications if present
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"

# 3. Initialize controller environment and device variables
get_controls

GAMEDIR="/$directory/ports/rpg2lx"
CONFDIR="$GAMEDIR/conf"
GAMEDATA="$GAMEDIR/gamedata"

mkdir -p "$CONFDIR"
mkdir -p "$GAMEDATA"
cd "$GAMEDIR" || exit 1

# 4. Redirect logs with timestamping for debugging support
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1
echo "=== Starting RPG2LX PortMaster Session: $(date) ==="
echo "CFW: $CFW_NAME | Device: $DEVICE_NAME | Arch: $DEVICE_ARCH"

# 5. Environment configuration and isolation
export XDG_DATA_HOME="$CONFDIR"
export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

# 6. Check binary existence for target architecture
BIN="./rpg2lx.${DEVICE_ARCH}"
if [ ! -f "$BIN" ]; then
    echo "ERROR: Binary $BIN not found for architecture $DEVICE_ARCH"
    pm_finish
    exit 1
fi
chmod +x "$BIN"

# 7. Launch gptokeyb with appropriate layout (analog stick fallback)
GPTOKEYB_CONFIG="./rpg2lx.gptk"
if [ -f "./rpg2lx.gptk.$ANALOGSTICKS" ]; then
    GPTOKEYB_CONFIG="./rpg2lx.gptk.$ANALOGSTICKS"
fi

if [ -n "$GPTOKEYB" ] && [ -f "$GPTOKEYB_CONFIG" ]; then
    $GPTOKEYB "rpg2lx.${DEVICE_ARCH}" -c "$GPTOKEYB_CONFIG" &
fi

# 8. Platform helper hook
pm_platform_helper "$GAMEDIR/rpg2lx.${DEVICE_ARCH}"

# 9. Launch target binary
# Note: In Phase 0/1 milestone, operates in inspect mode.
# In Phase 2+ with renderer/event loop ready, this transitions to the game loop:
# $BIN run "$GAMEDATA"
$BIN inspect "$GAMEDATA"

# 10. Clean up background tasks and restore terminal state
pm_finish
echo "=== RPG2LX Session Ended: $(date) ==="
