"""Apply a runtime-only package to an existing SD installation, preserving user data.

Validates source hashes, backs up every replaced file in local build/, verifies
the merged installed manifest, and rolls back runtime files on an error.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import uuid

ROOT=Path(__file__).resolve().parents[1]
PROTECTED={'games','saves','config','logs'}


def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
    return h.hexdigest()


def inside(root,relative):
    p=PurePosixPath(relative)
    if p.is_absolute() or '..' in p.parts or '\\' in relative or ':' in relative:
        raise ValueError('Unsafe manifest path')
    target=root.joinpath(*p.parts).resolve()
    if not target.is_relative_to(root):raise ValueError('Path leaves the ports directory')
    return target


def manifest(path):
    entries={}
    for line in path.read_text(encoding='utf-8').splitlines():
        sha,name=line.split('  ',1)
        if len(sha)!=64 or any(c not in '0123456789abcdef' for c in sha):raise ValueError('Invalid checksum')
        if name in entries:raise ValueError('Duplicate manifest entry')
        entries[name]=sha
    return entries


def snapshot(runtime):
    return {str(p.relative_to(runtime)):(p.stat().st_size,p.stat().st_mtime_ns)
            for folder in PROTECTED for p in (runtime/folder).rglob('*') if p.is_file()}


def atomic_write(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    temporary=path.with_name(path.name+'.update-'+uuid.uuid4().hex)
    try:
        with temporary.open('xb') as out:
            out.write(data);out.flush();os.fsync(out.fileno())
        if digest(temporary)!=hashlib.sha256(data).hexdigest():raise IOError('SD write verification failed')
        os.replace(temporary,path)
    finally:
        if temporary.exists():temporary.unlink()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ports',type=Path,required=True)
    parser.add_argument('--package',type=Path,required=True)
    args=parser.parse_args();ports=args.ports.resolve();source=args.package.resolve()
    runtime=inside(ports,'rpg2lx');installed_manifest=inside(ports,'rpg2lx/SHA256SUMS.txt')
    if not (ports/'RPG2LX.sh').is_file() or not installed_manifest.is_file():parser.error('Existing RPG2LX installation required')
    previous=json.loads((runtime/'build-info.json').read_text(encoding='utf-8'))
    if previous.get('platform')!='PortMaster Linux aarch64':parser.error('Unexpected installation')
    incoming=manifest(source/'rpg2lx/SHA256SUMS.txt');merged=manifest(installed_manifest)
    for name,sha in incoming.items():
        parts=PurePosixPath(name).parts
        if name!='RPG2LX.sh' and (len(parts)<2 or parts[0]!='rpg2lx' or parts[1] in PROTECTED):
            parser.error('Only a runtime-only update may be installed')
        if digest(inside(source,name))!=sha:parser.error('Package checksum mismatch: '+name)
        inside(ports,name)
    for name in merged:inside(ports,name)
    from verify_arm64 import verify
    verify(source/'rpg2lx/rpg2lx.aarch64')
    before=snapshot(runtime)
    backup=ROOT/'build'/('sd-backup-'+uuid.uuid4().hex);backup.mkdir(parents=True)
    changes=sorted(incoming,key=lambda s:s=='RPG2LX.sh')+['rpg2lx/SHA256SUMS.txt']
    for name in changes:
        old=inside(ports,name)
        if old.is_file():
            target=inside(backup,name);target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(old.read_bytes())
    applied=[]
    try:
        for name in changes[:-1]:
            atomic_write(inside(ports,name),inside(source,name).read_bytes());applied.append(name)
            if digest(inside(ports,name))!=incoming[name]:raise IOError('Installed checksum mismatch: '+name)
        merged.update(incoming)
        for name,sha in merged.items():
            if digest(inside(ports,name))!=sha:raise IOError('Installed manifest mismatch: '+name)
        content=''.join(sha+'  '+name+'\n' for name,sha in sorted(merged.items()))
        atomic_write(installed_manifest,content.encode('utf-8'));applied.append(changes[-1])
        if snapshot(runtime)!=before:raise IOError('User data changed during update')
    except BaseException:
        for name in reversed(applied):
            old=inside(backup,name)
            if old.is_file():atomic_write(inside(ports,name),old.read_bytes())
        raise
    report={'destination':str(ports),'version':json.loads((runtime/'build-info.json').read_text(encoding='utf-8'))['runtime_version'],
            'updated_files':len(changes),'preserved_user_files':len(before),'verified_manifest_entries':len(merged),'backup':str(backup)}
    (ROOT/'build/sd-update-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))
    return 0


if __name__=='__main__':raise SystemExit(main())
