"""Build actual C++ targets with an explicitly supplied compiler (no Python surrogate).

Example: python tools/build_native.py --zig build/tooling-stable/ziglang/zig.exe
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zig", type=Path, required=True)
    parser.add_argument("--release", action="store_true")
    parser.add_argument("--win-sdl", type=Path, default=ROOT / "build/win-sdl/SDL2-2.30.9/x86_64-w64-mingw32")
    args = parser.parse_args()
    compiler = args.zig.resolve()
    out = ROOT / "build" / ("native-release" if args.release else "native-debug")
    out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["ZIG_GLOBAL_CACHE_DIR"] = str(ROOT / "build" / "zig-cache")
    env["ZIG_LOCAL_CACHE_DIR"] = str(ROOT / "build" / "zig-local")
    suffix = ".exe" if os.name == "nt" else ""

    print("Pre-compiling C components (QuickJS + stb_vorbis)...", flush=True)
    qjs_dir = ROOT / "third_party" / "quickjs"
    c_units = [
        (qjs_dir / "dtoa.c", "dtoa.o"),
        (qjs_dir / "libregexp.c", "libregexp.o"),
        (qjs_dir / "libunicode.c", "libunicode.o"),
        (qjs_dir / "quickjs.c", "quickjs.o"),
        (ROOT / "third_party/stb/stb_vorbis.c", "stb_vorbis.o"),
    ]
    c_objs = []
    c_flags = ["-O2", "-DNDEBUG", "-w"] if args.release else ["-O1", "-g", "-w"]
    for src_file, obj_name in c_units:
        obj_file = out / obj_name
        c_objs.append(str(obj_file))
        if not obj_file.exists() or obj_file.stat().st_mtime < src_file.stat().st_mtime:
            cmd = [str(compiler), "cc", *c_flags, "-I", str(ROOT / "include"),
                   "-I", str(qjs_dir), "-c", str(src_file), "-o", str(obj_file)]
            res = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            if res.returncode != 0:
                print(f"Error compiling {src_file.name}:\n{res.stdout}")
                return res.returncode

    flags = [
        "-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Wno-nullability-completeness",
        "-I", str(ROOT / "include"),
        "-I", str(ROOT / "third_party/quickjs"),
        "-I", str(ROOT / "third_party/font8x8"),
        "-isystem", str(ROOT / "third_party/stb"),
    ]
    flags += ["-O2", "-DNDEBUG"] if args.release else ["-O0", "-g"]

    core = [
        "src/core/inspector.cpp",
        "src/mv/map.cpp",
        "src/mv/session.cpp",
        "src/mv/game_state.cpp",
        "src/mv/project_source.cpp",
        "src/mv/tile_layout.cpp",
        "src/mv/mv_audio.cpp",
        "src/mv/mv_engine.cpp",
        "src/launcher/library.cpp",
        "src/launcher/model.cpp",
        "src/js/js_runtime.cpp",
        "src/render/font_renderer.cpp",
        *c_objs,
    ]

    targets = {
        "rpg2lx": [*core, "src/cli/main.cpp", "src/launcher/frontend.cpp"],
        "test_inspector": [*core, "tests/test_inspector.cpp"],
        "test_json": ["tests/test_json.cpp"],
        "test_map": ["src/mv/map.cpp", "tests/test_map.cpp"],
        "test_library": [*core, "tests/test_library.cpp"],
        "test_session": [*core, "tests/test_session.cpp"],
        "test_game_state": [*core, "tests/test_game_state.cpp"],
        "test_js_runtime": ["src/js/js_runtime.cpp", "src/render/font_renderer.cpp", *c_objs, "tests/test_js_runtime.cpp"],
    }

    for name, sources in targets.items():
        print(f"Building {name}", flush=True)
        with (out / f"{name}.log").open("w", encoding="utf-8") as log:
            libraries = ["-lshell32"] if name == "rpg2lx" and os.name == "nt" else []
            result = subprocess.run([str(compiler), "c++", *flags, *sources,
                                     *libraries, "-o", str(out / (name + suffix))],
                                    cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            print((out / f"{name}.log").read_text(encoding="utf-8", errors="replace")[-12000:])
            return result.returncode

    for name, arguments in {
        "test_json": [],
        "test_map": [str(ROOT / "fixtures/valid_project/data/Map001.json")],
        "test_inspector": [str(ROOT / "fixtures")],
        "test_library": [str(ROOT / "fixtures")],
        "test_session": [str(ROOT / "fixtures/playable_project")],
        "test_game_state": [str(ROOT / "fixtures/playable_project")],
        "test_js_runtime": [],
    }.items():
        subprocess.run([str(out / (name + suffix)), *arguments], cwd=out, check=True)

    binary = str(out / ("rpg2lx" + suffix))
    for arguments, expected in [
        (["--version"], 0),
        (["--help"], 0),
        (["inspect", str(ROOT / "fixtures/valid_project")], 0),
        (["inspect", str(ROOT / "fixtures/incomplete_project")], 2),
        (["inspect-map", str(ROOT / "fixtures/valid_project/data/Map001.json")], 0),
        (["inspect-map"], 1),
        (["inspect-map", "missing-map.json"], 2),
    ]:
        result = subprocess.run([binary, *arguments], cwd=out)
        if result.returncode != expected:
            raise RuntimeError(f"{arguments}: expected {expected}, got {result.returncode}")

    subprocess.run([sys.executable, str(ROOT / "tests/test_cli_native.py"), binary], cwd=ROOT, check=True)

    # Build and run SDL2 targets when Windows SDL2 SDK is available
    sdl_root = args.win_sdl.resolve()
    sdl_dll_a = sdl_root / "lib/libSDL2.dll.a"
    sdl_dll = sdl_root / "bin/SDL2.dll"
    if os.name == "nt" and sdl_dll_a.is_file() and sdl_dll.is_file():
        shutil.copy2(sdl_dll, out / "SDL2.dll")
        sdl_flags = [
            *flags,
            "-DRPG2LX_WITH_SDL",
            "-isystem", str(sdl_root / "include/SDL2"),
        ]
        sdl_targets = {
            "rpg2lx-sdl": [
                *core, "src/render/map_view.cpp", "src/launcher/library_view.cpp",
                "src/cli/main.cpp", "src/launcher/frontend.cpp",
                str(sdl_dll_a), "-lshell32",
            ],
            "test_game_view": [
                *core, "src/render/map_view.cpp", "src/launcher/library_view.cpp",
                "tests/test_game_view.cpp", str(sdl_dll_a),
            ],
            "test_mv_engine": [
                *core, "tests/test_mv_engine.cpp", str(sdl_dll_a),
            ],
        }
        for name, sources in sdl_targets.items():
            print(f"Building {name} (SDL2)", flush=True)
            with (out / f"{name}.log").open("w", encoding="utf-8") as log:
                result = subprocess.run([str(compiler), "c++", *sdl_flags, *sources,
                                         "-o", str(out / (name + suffix))],
                                        cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
            if result.returncode:
                print((out / f"{name}.log").read_text(encoding="utf-8", errors="replace")[-12000:])
                return result.returncode

        sdl_env = env.copy()
        sdl_env["SDL_VIDEODRIVER"] = "dummy"
        sdl_env["SDL_AUDIODRIVER"] = "dummy"
        subprocess.run([str(out / ("test_game_view" + suffix)), str(ROOT / "fixtures/playable_project")],
                       cwd=out, env=sdl_env, check=True)
        subprocess.run([str(out / ("test_mv_engine" + suffix))],
                       cwd=out, env=sdl_env, check=True)
        subprocess.run([sys.executable, str(ROOT / "tests/test_game_smoke.py"),
                        str(out / ("rpg2lx-sdl" + suffix))],
                       cwd=ROOT, env=sdl_env, check=True)

    print("Native C++ build and tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
