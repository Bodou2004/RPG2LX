"""Build actual C++ targets with an explicitly supplied compiler (no Python surrogate).

Example: python tools/build_native.py --zig build/tooling/ziglang/zig.exe
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zig", type=Path, required=True)
    parser.add_argument("--release", action="store_true")
    args = parser.parse_args()
    compiler = args.zig.resolve()
    out = ROOT / "build" / ("native-release" if args.release else "native-debug")
    out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["ZIG_GLOBAL_CACHE_DIR"] = str(ROOT / "build" / "zig-cache")
    env["ZIG_LOCAL_CACHE_DIR"] = str(ROOT / "build" / "zig-local")
    suffix = ".exe" if os.name == "nt" else ""
    flags = ["-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Wno-nullability-completeness",
             "-I", str(ROOT / "include")]
    flags += ["-O2", "-DNDEBUG"] if args.release else ["-O0", "-g"]
    core = ["src/core/inspector.cpp", "src/mv/map.cpp", "src/mv/session.cpp", "src/mv/tile_layout.cpp",
            "src/launcher/library.cpp", "src/launcher/model.cpp"]
    targets = {"rpg2lx": [*core, "src/cli/main.cpp", "src/launcher/frontend.cpp"],
               "test_inspector": [*core, "tests/test_inspector.cpp"],
               "test_json": ["tests/test_json.cpp"],
               "test_map": ["src/mv/map.cpp", "tests/test_map.cpp"],
               "test_library": [*core, "tests/test_library.cpp"],
               "test_session": [*core, "tests/test_session.cpp"]}
    for name, sources in targets.items():
        print(f"Building {name}", flush=True)
        with (out / f"{name}.log").open("w", encoding="utf-8") as log:
            libraries = ["-lshell32"] if name == "rpg2lx" and os.name == "nt" else []
            result = subprocess.run([str(compiler), "c++", *flags, *sources,
                                     *libraries, "-o", str(out / (name + suffix))], cwd=ROOT,
                                    env=env, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            print((out / f"{name}.log").read_text(encoding="utf-8", errors="replace")[-12000:])
            return result.returncode
    for name, arguments in {"test_json": [], "test_map": [str(ROOT / "fixtures/valid_project/data/Map001.json")],
                            "test_inspector": [str(ROOT / "fixtures")],
                            "test_library": [str(ROOT / "fixtures")],
                            "test_session": [str(ROOT / "fixtures/playable_project")]}.items():
        subprocess.run([str(out / (name + suffix)), *arguments], cwd=out, check=True)
    binary = str(out / ("rpg2lx" + suffix))
    for arguments, expected in [(["--version"], 0), (["--help"], 0),
                                (["inspect", str(ROOT / "fixtures/valid_project")], 0),
                                (["inspect", str(ROOT / "fixtures/incomplete_project")], 2),
                                (["inspect-map", str(ROOT / "fixtures/valid_project/data/Map001.json")], 0),
                                (["inspect-map"], 1),
                                (["inspect-map", "missing-map.json"], 2)]:
        result = subprocess.run([binary, *arguments], cwd=out)
        if result.returncode != expected:
            raise RuntimeError(f"{arguments}: expected {expected}, got {result.returncode}")
    subprocess.run([sys.executable, str(ROOT / "tests/test_cli_native.py"), binary],
                   cwd=ROOT, check=True)
    print("Native C++ build and tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
