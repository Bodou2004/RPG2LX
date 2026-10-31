#!/bin/bash

# PortMaster launcher for RPG2LX.
# The HUB and every game run as separate processes: the HUB writes a launch request and
# exits, this script runs the game, then reopens the HUB. A crashing game can
# never take the HUB down, and each game starts with all memory free.
# Lifecycle reference: https://portmaster.games/packaging.html

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

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

if [ -z "$controlfolder" ]; then
    echo "ERROR: PortMaster control.txt not found in any known location"
    exit 1
fi

source "$controlfolder/control.txt" || exit 1
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls || exit 1

RPG2LX_PORTS_DIR="$(cd -- "$(dirname -- "$0")" && pwd -P)" || exit 1
GAMEDIR="$RPG2LX_PORTS_DIR/rpg2lx"
CONFDIR="$GAMEDIR/config"
REQUEST="$CONFDIR/launch.request"

mkdir -p "$CONFDIR" "$GAMEDIR/games" "$GAMEDIR/saves" "$GAMEDIR/logs" || exit 1
cd "$GAMEDIR" || exit 1

trap 'restore_governor; pm_finish' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

exec > >(tee -a "$GAMEDIR/logs/portmaster.log") 2>&1
echo "=== RPG2LX session: $(date) ==="
echo "CFW: $CFW_NAME | Device: $DEVICE_NAME | Arch: $DEVICE_ARCH | Res: ${DISPLAY_WIDTH}x${DISPLAY_HEIGHT}"

export XDG_DATA_HOME="$CONFDIR"
export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
if [ -z "${SDL_GAMECONTROLLERCONFIG_FILE:-}" ] && [ -f "$controlfolder/gamecontrollerdb.txt" ]; then
    export SDL_GAMECONTROLLERCONFIG_FILE="$controlfolder/gamecontrollerdb.txt"
fi

BIN="./rpg2lx.${DEVICE_ARCH}"
if [ "${DEVICE_ARCH:-}" != "aarch64" ]; then
    echo "ERROR: This package contains an aarch64 binary; detected architecture: ${DEVICE_ARCH:-unknown}"
    exit 1
fi
if [ ! -f "$BIN" ]; then
    echo "ERROR: Binary $BIN not found"
    exit 1
fi
chmod +x "$BIN"
pm_platform_helper "$GAMEDIR/rpg2lx.${DEVICE_ARCH}"

# The game's scripts run on one core in an interpreter; a CPU that an on-demand governor keeps clocked down makes
# the game itself run slow. While a game runs the governor is "performance"; afterwards it goes back to what it was.
GOVERNORS=()
set_performance_governor() {
    local files gov
    files=$(ls /sys/devices/system/cpu/cpufreq/policy*/scaling_governor 2>/dev/null)
    [ -n "$files" ] || files=$(ls /sys/devices/system/cpu/cpu[0-9]*/cpufreq/scaling_governor 2>/dev/null)
    for gov in $files; do
        GOVERNORS+=("$gov=$(cat "$gov" 2>/dev/null)")
        echo performance | $ESUDO tee "$gov" >/dev/null 2>&1
    done
}
restore_governor() {
    local entry
    for entry in "${GOVERNORS[@]}"; do
        [ -n "${entry#*=}" ] && echo "${entry#*=}" | $ESUDO tee "${entry%%=*}" >/dev/null 2>&1
    done
    GOVERNORS=()
}

# Reads one "key=value" line from the launch request.
request_value() {
    sed -n "s/^$1=//p" "$REQUEST" | head -n 1
}

STATUS=0
while true; do
    rm -f "$REQUEST"
    "$BIN" hub --home "$GAMEDIR" --request "$REQUEST"
    STATUS=$?
    # 10 = the HUB asked to launch a game; anything else means quit.
    if [ "$STATUS" -ne 10 ] || [ ! -f "$REQUEST" ]; then
        break
    fi
    project="$(request_value project)"
    game_id="$(request_value id)"
    echo "--- Launch: $game_id $(date) ---"
    set_performance_governor
    "$BIN" run "$project" --home "$GAMEDIR" --game-id "$game_id"
    GAME_STATUS=$?
    restore_governor
    echo "--- Game ended: $game_id status $GAME_STATUS ---"
done

echo "=== RPG2LX session ended: $(date) ==="
[ "$STATUS" -eq 10 ] && STATUS=0
exit "$STATUS"
