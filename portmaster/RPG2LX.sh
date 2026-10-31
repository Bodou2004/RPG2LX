#!/bin/bash

# PortMaster launcher for RPG2LX.
# The HUB and every game run as separate processes: the HUB writes a launch request and
# exits, this script runs the chosen engine, then reopens the HUB. A crashing game can
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

weston_dir=/tmp/weston
weston_runtime="weston_pkg_0.2"
weston_mounted=0

cleanup() {
    if [ "$weston_mounted" = 1 ]; then
        $ESUDO "$weston_dir/westonwrap.sh" cleanup
        [ "${PM_CAN_MOUNT:-Y}" != "N" ] && $ESUDO umount "$weston_dir"
    fi
    pm_finish
}
trap cleanup EXIT
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

# Reads one "key=value" line from the launch request.
request_value() {
    sed -n "s/^$1=//p" "$REQUEST" | head -n 1
}

# Mounts PortMaster's Westonpack runtime, which provides X11/Wayland on every CFW.
mount_weston() {
    [ "$weston_mounted" = 1 ] && return 0
    if [ ! -f "$controlfolder/libs/${weston_runtime}.squashfs" ]; then
        if [ ! -f "$controlfolder/harbourmaster" ]; then
            pm_message "Instala la ultima version de PortMaster para usar el modo navegador."
            sleep 5
            return 1
        fi
        $ESUDO "$controlfolder/harbourmaster" --quiet --no-check runtime_check "${weston_runtime}.squashfs"
    fi
    $ESUDO mkdir -p "$weston_dir"
    [ "${PM_CAN_MOUNT:-Y}" != "N" ] && $ESUDO umount "$weston_dir" 2>/dev/null
    $ESUDO mount "$controlfolder/libs/${weston_runtime}.squashfs" "$weston_dir" || return 1
    weston_mounted=1
}

# Browser engine: real Chromium (NW.js) for games whose plugins the native engine can't run.
run_browser() {
    local project="$1" game_id="$2"
    local nw="$GAMEDIR/nwjs/nw"
    if [ ! -x "$nw" ]; then
        echo "ERROR: NW.js not installed in $GAMEDIR/nwjs (see docs/ENGINES.md)"
        return 2
    fi
    local app="$CONFDIR/games/$game_id/nwapp"
    mkdir -p "$app" "$CONFDIR/games/$game_id/chromium"
    # Wrapper app: the game folder is never modified (an empty package.json "name" breaks NW.js).
    cat > "$app/package.json" <<EOF
{
  "name": "rpg2lx-game",
  "main": "file://$project/index.html",
  "window": { "fullscreen": true, "frame": false, "kiosk": true },
  "chromium-args": "--disable-background-timer-throttling --disable-renderer-backgrounding --ignore-gpu-blocklist --enable-gpu-rasterization --disk-cache-size=8388608 --js-flags=--max-old-space-size=384 --user-data-dir=$CONFDIR/games/$game_id/chromium"
}
EOF
    local gptk="$CONFDIR/games/$game_id/browser.gptk"
    "$BIN" export-gptk --home "$GAMEDIR" --game-id "$game_id" --output "$gptk" || return 2

    mount_weston || return 2
    $GPTOKEYB "nw" -c "$gptk" &
    local mode="${RPG2LX_WESTON_MODE:-headless noop kiosk crusty_x11egl}"
    # shellcheck disable=SC2086
    $ESUDO env "$weston_dir/westonwrap.sh" $mode \
        XDG_DATA_HOME="$CONFDIR" "$nw" "$app"
    local status=$?
    pm_gptokeyb_finish 2>/dev/null || $ESUDO kill -9 "$(pidof gptokeyb)" 2>/dev/null
    return $status
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
    engine="$(request_value engine)"
    project="$(request_value project)"
    game_id="$(request_value id)"
    echo "--- Launch: $game_id ($engine) $(date) ---"
    if [ "$engine" = "browser" ]; then
        run_browser "$project" "$game_id"
    else
        "$BIN" run "$project" --home "$GAMEDIR" --game-id "$game_id"
    fi
    game_status=$?
    echo "--- Game ended: $game_id status $game_status ---"
    # Lets the HUB offer the browser engine next time a native run crashed.
    "$BIN" record-result --home "$GAMEDIR" --game-id "$game_id" --engine "$engine" --status "$game_status"
done

echo "=== RPG2LX session ended: $(date) ==="
[ "$STATUS" -eq 10 ] && STATUS=0
exit "$STATUS"
