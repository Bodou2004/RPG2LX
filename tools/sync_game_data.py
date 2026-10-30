"""Install explicitly selected MV test-project data, with backup and verification.

Unlike runtime-only updates, this command intentionally replaces game data.
It only copies data/*.json; artwork, JavaScript, saves and settings are preserved.
"""
import argparse
import json
from pathlib import Path
import uuid
from update_sd import ROOT, inside, digest, manifest, atomic_write


def protected_state(runtime,game):
    files=[p for folder in ('saves','config','logs') for p in (runtime/folder).rglob('*') if p.is_file()]
    files.extend(p for p in game.rglob('*') if p.is_file() and not p.is_relative_to(game/'data'))
    return {str(p.relative_to(runtime)):(p.stat().st_size,p.stat().st_mtime_ns) for p in files}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ports',type=Path,required=True)
    parser.add_argument('--project',type=Path,required=True)
    parser.add_argument('--game-id',required=True)
    args=parser.parse_args();ports=args.ports.resolve();project=args.project.resolve();name=args.game_id
    if not name or name in ('.','..') or any(c in name for c in '/\\:'):parser.error('Invalid game folder')
    runtime=inside(ports,'rpg2lx');game=inside(ports,'rpg2lx/games/'+name)
    if not (game/'data/System.json').is_file():parser.error('Existing game installation required')
    if not (project/'Game.rpgproject').is_file():parser.error('Pass the explicitly chosen MV test project')
    changed={}
    for source in sorted((project/'data').glob('*.json')):
        if not source.resolve().is_relative_to(project):parser.error('Data file leaves source project')
        data=json.loads(source.read_text(encoding='utf-8-sig'))
        if source.name.startswith('Map') and source.name[3:-5].isdigit():
            if len(data['data'])!=data['width']*data['height']*6:parser.error('Invalid MV map planes')
        relative='rpg2lx/games/'+name+'/data/'+source.name;target=inside(ports,relative)
        if not target.is_file() or digest(source)!=digest(target):changed[relative]=source
    entries=manifest(inside(ports,'rpg2lx/SHA256SUMS.txt'))
    before=protected_state(runtime,game);backup=ROOT/'build'/('sd-game-backup-'+uuid.uuid4().hex);backup.mkdir(parents=True)
    paths=[*changed,'rpg2lx/SHA256SUMS.txt'];applied=[]
    for relative in paths:
        target=inside(ports,relative)
        if target.is_file():
            copy=inside(backup,relative);copy.parent.mkdir(parents=True,exist_ok=True);copy.write_bytes(target.read_bytes())
    try:
        for relative,source in changed.items():
            atomic_write(inside(ports,relative),source.read_bytes());applied.append(relative);entries[relative]=digest(source)
        for relative,sha in entries.items():
            if digest(inside(ports,relative))!=sha:raise IOError('Installed checksum mismatch: '+relative)
        sums=''.join(sha+'  '+relative+'\n' for relative,sha in sorted(entries.items()))
        atomic_write(inside(ports,paths[-1]),sums.encode('utf-8'));applied.append(paths[-1])
        if protected_state(runtime,game)!=before:raise IOError('Protected game/state files changed')
    except BaseException:
        for relative in reversed(applied):
            previous=inside(backup,relative);target=inside(ports,relative)
            if previous.is_file():atomic_write(target,previous.read_bytes())
            elif target.is_file():target.unlink() # Only files created by this transaction.
        raise
    report={'destination':str(game),'changed_data_files':[Path(p).name for p in changed],
            'verified_manifest_entries':len(entries),'preserved_asset_and_state_files':len(before),'backup':str(backup)}
    (ROOT/'build/sd-game-data-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2));return 0


if __name__=='__main__':raise SystemExit(main())
