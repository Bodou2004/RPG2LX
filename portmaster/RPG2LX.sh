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

# Browser engine: the game runs in NW.js (Chromium + Node, what MV exports ship with). It draws straight into the
# firmware's Wayland compositor (sway on ROCKNIX) or X11; no extra runtime is mounted. Returns 3 when NW.js could
# not start at all, so the caller can fall back to the native engine.
run_browser() {
    local project="$1" game_id="$2"
    local nwdir="$GAMEDIR/nwjs"
    local blog="$GAMEDIR/logs/browser.log"
    if [ ! -f "$nwdir/nw" ]; then
        echo "Browser engine: NW.js is not installed in $nwdir"
        return 3
    fi
    # Copying to the SD from Windows drops exec bits.
    while read -r exe; do
        [ -n "$exe" ] && chmod +x "$nwdir/$exe" 2>/dev/null
    done < "$nwdir/EXECUTABLES.txt"

    local gptk="$CONFDIR/games/$game_id/browser.gptk"
    local app
    app="$("$BIN" browser-app --home "$GAMEDIR" --game-id "$game_id" --project "$project" --gptk "$gptk")" || return 3

    # The compositor's variables, when the port was started without them.
    if [ -z "$WAYLAND_DISPLAY" ] && [ -z "$DISPLAY" ] && [ -f /etc/profile ]; then
        . /etc/profile >/dev/null 2>&1
    fi
    local runtime="${XDG_RUNTIME_DIR:-/var/run/0-runtime-dir}"
    local platform
    if [ -n "$WAYLAND_DISPLAY" ] || [ -S "$runtime/wayland-1" ] || [ -S "$runtime/wayland-0" ]; then
        export XDG_RUNTIME_DIR="$runtime"
        if [ -z "$WAYLAND_DISPLAY" ]; then
            [ -S "$runtime/wayland-1" ] && export WAYLAND_DISPLAY=wayland-1 || export WAYLAND_DISPLAY=wayland-0
        fi
        platform="--ozone-platform=wayland"
    elif [ -n "$DISPLAY" ]; then
        platform="--ozone-platform=x11"
    else
        echo "Browser engine: no Wayland or X11 display found"
        return 3
    fi
    # ROCKNIX runs ports as root, where Chromium refuses to start with its sandbox on.
    local sandbox=""
    [ "$(id -u)" = "0" ] && sandbox="--no-sandbox"

    echo "=== $(date) | $game_id | $platform WAYLAND_DISPLAY=$WAYLAND_DISPLAY DISPLAY=$DISPLAY XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR" >> "$blog"
    $GPTOKEYB "nw" -c "$gptk" &
    command -v portmaster_sway_fullscreen.sh >/dev/null 2>&1 && portmaster_sway_fullscreen.sh nw >/dev/null 2>&1 &

    # Chromium's profile goes to tmpfs: the SD's FAT/exFAT cannot hold its SingletonLock symlink. Saves do not
    # live there (they go to saves/<id>/), so losing it on reboot costs nothing.
    local profile="/tmp/rpg2lx-nw/$(printf '%s' "$game_id" | tr -c 'A-Za-z0-9._-' '_')"
    mkdir -p "$profile"
    rm -f "$profile/SingletonLock" "$profile/SingletonSocket" "$profile/SingletonCookie"

    local started=$SECONDS
    # nwjs/syslibs carries the NSS libraries the firmware lacks; everything else comes from the system.
    RPG2LX_BROWSER_LOG="$blog" RPG2LX_SAVE_DIR="$GAMEDIR/saves/$game_id" \
    LD_LIBRARY_PATH="$nwdir/lib:$nwdir/syslibs:$LD_LIBRARY_PATH" \
        "$nwdir/nw" $platform $sandbox --disable-gpu-sandbox "--user-data-dir=$profile" "$app" >> "$blog" 2>&1
    local status=$?
    pm_gptokeyb_finish 2>/dev/null || $ESUDO kill -9 "$(pidof gptokeyb)" 2>/dev/null
    echo "=== NW.js exited with $status after $((SECONDS - started)) s" >> "$blog"
    # A crash within a few seconds is a start failure (missing library, no display), not the player quitting.
    if [ "$status" -ne 0 ] && [ $((SECONDS - started)) -lt 8 ]; then
        return 3
    fi
    return "$status"
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
    engine="$(request_value engine)"
    echo "--- Launch: $game_id (${engine:-native}) $(date) ---"
    set_performance_governor
    GAME_STATUS=3
    if [ "$engine" = "browser" ]; then
        run_browser "$project" "$game_id"
        GAME_STATUS=$?
        [ "$GAME_STATUS" -eq 3 ] && echo "Browser engine could not start (see logs/browser.log); using the native engine"
    fi
    if [ "$GAME_STATUS" -eq 3 ]; then
        "$BIN" run "$project" --home "$GAMEDIR" --game-id "$game_id"
        GAME_STATUS=$?
    fi
    restore_governor
    echo "--- Game ended: $game_id status $GAME_STATUS ---"
done

echo "=== RPG2LX session ended: $(date) ==="
[ "$STATUS" -eq 10 ] && STATUS=0
exit "$STATUS"
