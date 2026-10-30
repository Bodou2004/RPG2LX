"""Exercise the actual .sh with a mock PortMaster contract, never firmware scripts."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def posix(path):
    value=path.resolve().as_posix()
    return '/'+value[0].lower()+value[2:] if os.name=='nt' else value


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bash',required=True)
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='launcher-',dir=ROOT/'build') as directory:
        base=Path(directory)
        ports=base/'Second SD with spaces/ports';ports.mkdir(parents=True)
        runtime=ports/'rpg2lx';runtime.mkdir()
        control=base/'xdg/PortMaster';control.mkdir(parents=True)
        control.joinpath('control.txt').write_text('''
CFW_NAME=MockPortableFirmware
DEVICE_NAME=MockHandheld
DEVICE_ARCH=${TEST_ARCH:-aarch64}
directory=deliberately_wrong_mount
get_controls() { sdl_controllerconfig="test-mapping"; }
pm_platform_helper() { printf '%s' "$1" > "$TEST_OUTPUT/helper"; }
pm_finish() { printf 'finished' > "$TEST_OUTPUT/cleanup"; }
''',encoding='utf-8')
        script=ports/'RPG2LX.sh';shutil.copy2(ROOT/'portmaster/RPG2LX.sh',script)
        binary=runtime/'rpg2lx.aarch64'
        binary.write_text('''#!/bin/bash
printf '%s\\n' "$@" > "$TEST_OUTPUT/args"
exit 23
''',encoding='utf-8')
        binary.chmod(0o755)
        env=os.environ.copy();env.update(XDG_DATA_HOME=posix(base/'xdg'),TEST_OUTPUT=posix(base),TEST_ARCH='aarch64')
        result=subprocess.run([args.bash,posix(script)],env=env,capture_output=True,text=True)
        if result.returncode!=23:raise RuntimeError('Port exit status not preserved: '+result.stdout+result.stderr)
        expected=['launcher','--home',posix(runtime)]
        if (base/'args').read_text().splitlines()!=expected:raise RuntimeError('Incorrect data path or arguments')
        if (base/'cleanup').read_text()!='finished':raise RuntimeError('PortMaster cleanup was not called')
        if (base/'helper').read_text()!=posix(binary):raise RuntimeError('Incorrect platform helper binary')
        for name in ('games','saves','config','logs'):
            if not (runtime/name).is_dir():raise RuntimeError('Missing runtime directory '+name)
        (base/'args').unlink();(base/'cleanup').unlink()
        env['TEST_ARCH']='armhf'
        wrong=subprocess.run([args.bash,posix(script)],env=env,capture_output=True,text=True)
        if wrong.returncode!=1 or (base/'args').exists() or not (base/'cleanup').exists():
            raise RuntimeError('Wrong architecture must fail without launching and still clean up')
    print('PortMaster launcher: portable paths, SD2/spaces, cleanup, exit status and architecture checks passed')
    return 0


if __name__=='__main__':raise SystemExit(main())
