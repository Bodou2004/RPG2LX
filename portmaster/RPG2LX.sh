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

# GPU kernel driver: ROCKNIX loads libmali (mali_kbase, its default on RK3326) or Mesa's panfrost, after its
# gpu.driver setting.
gpu_driver() {
    grep -o -m 1 -E '^(mali_kbase|bifrost_kbase|midgard_kbase|panfrost|lima)' /proc/modules 2>/dev/null || echo unknown
}

# Prints the Chromium GPU switches that give WebGL a real GPU on this device, or nothing when none does. Each
# candidate runs a tiny hidden NW.js page that reports its WebGL renderer; the first real GPU wins. The answer
# is kept in config/browser-gpu.txt (switches or "none", the environment, the renderer) and probed again when
# the GPU driver, the kernel or NW.js change.
# Chromium normally draws in a separate GPU process, which needs a DRM render node and GBM: Mesa has them,
# libmali does not, and ANGLE then gets no EGL config at all. --in-process-gpu draws in the main process through
# the Wayland window instead, as SDL does, so it goes first.
browser_gpu_flags() {
    local nwdir="$1" platform="$2" sandbox="$3"
    local cache="$CONFDIR/browser-gpu.txt" blog="$GAMEDIR/logs/browser.log"
    local key
    key="driver=$(gpu_driver) kernel=$(uname -r) nw=$(stat -c %s "$nwdir/lib/libnw.so" 2>/dev/null)"
    if [ -f "$cache" ] && [ "$(sed -n 2p "$cache")" = "$key" ]; then
        local saved
        saved="$(head -n 1 "$cache")"
        [ "$saved" != "none" ] && printf '%s' "$saved"
        return
    fi
    echo "=== GPU probe | $key" >> "$blog"
    local probe="/tmp/rpg2lx-nw/probe" out="/tmp/rpg2lx-nw/probe.txt"
    mkdir -p "$probe/app"
    cat > "$probe/app/package.json" <<'EOF'
{ "name": "rpg2lx-gpu-probe", "main": "probe.html", "window": { "show": false } }
EOF
    cat > "$probe/app/probe.html" <<'EOF'
<!doctype html><script>
var r = "no-webgl";
try {
  var gl = document.createElement("canvas").getContext("webgl");
  if (gl) {
    var e = gl.getExtension("WEBGL_debug_renderer_info");
    r = String(gl.getParameter(e ? e.UNMASKED_RENDERER_WEBGL : gl.RENDERER));
  }
} catch (err) { r = "error " + err; }
require("fs").writeFileSync(process.env.RPG2LX_GPU_PROBE, r);
nw.App.quit();
</script>
EOF
    local candidate renderer
    for candidate in "--in-process-gpu" "--in-process-gpu --use-angle=gles" "--use-angle=default" "--use-angle=gles"; do
        rm -rf "$out" "$probe/profile"
        RPG2LX_GPU_PROBE="$out" LD_LIBRARY_PATH="$nwdir/lib:$nwdir/syslibs:$LD_LIBRARY_PATH" \
            timeout 15 "$nwdir/nw" $platform $sandbox --disable-gpu-sandbox --ignore-gpu-blocklist $candidate \
            "--user-data-dir=$probe/profile" "$probe/app" > "$probe/stderr.txt" 2>&1
        renderer="$(cat "$out" 2>/dev/null)"
        echo "=== GPU probe [$candidate]: ${renderer:-no answer}" >> "$blog"
        # Chromium's own GPU messages, to see why a candidate fails.
        grep -i -E 'egl|gl_|gpu|angle|wayland|config' "$probe/stderr.txt" | grep -v 'dbus/' | tail -n 4 | sed 's/^/      /' >> "$blog"
        case "$renderer" in
            ""|no-webgl|error*|*SwiftShader*|*llvmpipe*|*softpipe*) ;;
            *)
                printf '%s\n%s\n%s\n' "$candidate" "$key" "$renderer" > "$cache"
                rm -rf "$probe"
                printf '%s' "$candidate"
                return
                ;;
        esac
    done
    printf 'none\n%s\n' "$key" > "$cache"
    rm -rf "$probe"
    case "$key" in
        *mali_kbase*) echo "=== No GPU for Chromium with libmali. ROCKNIX can load Mesa's panfrost instead (its GPU driver setting); RPG2LX probes again by itself after that." >> "$blog" ;;
    esac
}

# Browser engine: the game runs in NW.js (Chromium + Node, what MV exports ship with). It draws straight into the
# firmware's Wayland compositor (sway on ROCKNIX) or X11; no extra runtime is mounted. mode "auto" only runs it
# when Chromium gets the GPU; "browser" runs it anyway. Returns 3 when NW.js was not used or could not start at
# all, so the caller runs the native engine.
run_browser() {
    local project="$1" game_id="$2" mode="$3"
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
    unset DBUS_SESSION_BUS_ADDRESS

    local gpu_flags
    gpu_flags="$(browser_gpu_flags "$nwdir" "$platform" "$sandbox")"
    if [ -z "$gpu_flags" ] && [ "$mode" = "auto" ]; then
        echo "=== $(date) | $game_id | Auto: Chromium has no GPU here (config/browser-gpu.txt), native engine" >> "$blog"
        return 3
    fi

    local gptk="$CONFDIR/games/$game_id/browser.gptk"
    local app
    app="$("$BIN" browser-app --home "$GAMEDIR" --game-id "$game_id" --project "$project" --gptk "$gptk")" || return 3

    echo "=== $(date) | $game_id | $platform WAYLAND_DISPLAY=$WAYLAND_DISPLAY XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR | GPU: ${gpu_flags:-none, CPU drawing}" >> "$blog"
    $GPTOKEYB "nw" -c "$gptk" &
    command -v portmaster_sway_fullscreen.sh >/dev/null 2>&1 && portmaster_sway_fullscreen.sh nw >/dev/null 2>&1 &

    # Chromium's profile goes to tmpfs: the SD's FAT/exFAT cannot hold its SingletonLock symlink. tmpfs is RAM, so
    # it starts empty and is deleted afterwards; saves do not live there (they go to saves/<id>/).
    local profile="/tmp/rpg2lx-nw/$(printf '%s' "$game_id" | tr -c 'A-Za-z0-9._-' '_')"
    rm -rf "$profile"
    mkdir -p "$profile"

    local started=$SECONDS
    # nwjs/syslibs carries the NSS libraries the firmware lacks; everything else comes from the system.
    RPG2LX_BROWSER_LOG="$blog" RPG2LX_SAVE_DIR="$GAMEDIR/saves/$game_id" \
    LD_LIBRARY_PATH="$nwdir/lib:$nwdir/syslibs:$LD_LIBRARY_PATH" \
        "$nwdir/nw" $platform $sandbox --disable-gpu-sandbox $gpu_flags "--user-data-dir=$profile" "$app" >> "$blog" 2>&1
    local status=$?
    pm_gptokeyb_finish 2>/dev/null || $ESUDO kill -9 "$(pidof gptokeyb)" 2>/dev/null
    rm -rf "$profile"
    echo "=== NW.js exited with $status after $((SECONDS - started)) s" >> "$blog"
    # A crash within a few seconds is a start failure (missing library, no display), not the player quitting.
    # Select+Start kills NW.js (137, SIGKILL) and is the player's choice, as is a SIGTERM (143).
    if [ "$status" -ne 0 ] && [ "$status" -ne 137 ] && [ "$status" -ne 143 ] && [ $((SECONDS - started)) -lt 8 ]; then
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
    if [ "$engine" = "browser" ] || [ "$engine" = "auto" ]; then
        run_browser "$project" "$game_id" "$engine"
        GAME_STATUS=$?
        [ "$GAME_STATUS" -eq 3 ] && echo "Browser engine not used (see logs/browser.log); using the native engine"
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
