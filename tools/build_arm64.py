"""Cross-build the PortMaster ARM64 executable; never ship the link-only SDL SDK.

Use Zig 0.13.0 and a Linux ARM64 SDL2 SDK. Device SDL2 is linked dynamically.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zig',type=Path,required=True)
    parser.add_argument('--sdl-root',type=Path,required=True)
    args=parser.parse_args()
    compiler=args.zig.resolve(); sdk=args.sdl_root.resolve()
    library=sdk/'usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0.14.0'
    if not library.is_file(): parser.error('Expected the ARM64 SDL2 2.0.14 link SDK')
    out=ROOT/'build/aarch64';out.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy()
    env['ZIG_GLOBAL_CACHE_DIR']=str(ROOT/'build/zig-cache')
    env['ZIG_LOCAL_CACHE_DIR']=str(ROOT/'build/zig-arm-local')
    sources=['src/core/inspector.cpp','src/mv/map.cpp','src/mv/project_source.cpp',
             'src/mv/session.cpp','src/mv/game_state.cpp','src/mv/tile_layout.cpp','src/launcher/library.cpp',
             'src/launcher/model.cpp','src/launcher/frontend.cpp','src/launcher/library_view.cpp','src/render/map_view.cpp',
             'src/cli/main.cpp']
    binary=out/'rpg2lx.aarch64'
    command=[str(compiler),'c++','-target','aarch64-linux-gnu.2.28','-mcpu=cortex_a35',
             '-std=c++17','-O2','-DNDEBUG','-DRPG2LX_WITH_SDL','-Wall','-Wextra','-Wpedantic',
             '-I',str(ROOT/'include'),'-I',str(ROOT/'third_party/font8x8'),
             '-isystem',str(ROOT/'third_party/stb'),
             '-isystem',str(sdk/'usr/include/SDL2'),
             '-isystem',str(sdk/'usr/include/aarch64-linux-gnu'),
             *sources,str(library),'-Wl,--allow-shlib-undefined','-ldl','-lpthread','-s','-o',str(binary)]
    print('Building Linux ARM64 / Cortex-A35 / glibc 2.28 baseline',flush=True)
    with (out/'build.log').open('w',encoding='utf-8') as log:
        result=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT)
    if result.returncode:
        print((out/'build.log').read_text(encoding='utf-8',errors='replace')[-16000:])
        return result.returncode
    (out/'build-info.json').write_text(json.dumps({
        'target':'aarch64-linux-gnu.2.28','cpu':'cortex_a35',
        'sdl_link_sdk':'Debian SDL2 2.0.14 arm64 (link-only, not bundled)',
        'device_dependency':'libSDL2-2.0.so.0 from firmware',
        'tested_on_hardware':False,'in_game_runtime_hud':False,
        'compiler':subprocess.check_output([str(compiler),'version'],text=True).strip()
    },indent=2)+'\n',encoding='utf-8')
    print(binary)
    return 0


if __name__=='__main__':sys.exit(main())
