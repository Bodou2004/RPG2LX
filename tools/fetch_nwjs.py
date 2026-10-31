"""Download the official NW.js linux-arm64 build used by RPG2LX's browser engine into build/.

The archive is checked against the SHASUMS256.txt published next to it on dl.nwjs.io.
Unused locales are dropped to save SD space; nothing outside build/ is touched.
Example: python tools/fetch_nwjs.py --version v0.119.2
"""
import argparse
import hashlib
from pathlib import Path
import shutil
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
KEEP_LOCALES = {'en-US.pak', 'es.pak', 'es-419.pak'}


def download(url, destination):
    # dl.nwjs.io answers 403 to urllib's default User-Agent.
    request = urllib.request.Request(url, headers={'User-Agent': 'rpg2lx-fetch-nwjs/1.0'})
    with urllib.request.urlopen(request) as response, destination.open('wb') as out:
        shutil.copyfileobj(response, out, length=1 << 20)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default='v0.119.2')
    args = parser.parse_args()
    version = args.version if args.version.startswith('v') else 'v' + args.version
    name = f'nwjs-{version}-linux-arm64'
    base = f'https://dl.nwjs.io/{version}'
    cache = ROOT / 'build' / 'nwjs-download'
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / f'{name}.tar.gz'
    sums = cache / f'SHASUMS256-{version}.txt'

    if not sums.is_file():
        download(f'{base}/SHASUMS256.txt', sums)
    expected = None
    for line in sums.read_text(encoding='utf-8').splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[1].lstrip('*') == archive.name:
            expected = parts[0].lower()
    if not expected:
        parser.error(f'{archive.name} is not listed in {base}/SHASUMS256.txt')

    if not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
        print(f'Downloading {base}/{archive.name} (~200 MB)...', flush=True)
        download(f'{base}/{archive.name}', archive)
    actual = hashlib.sha256(archive.read_bytes()).hexdigest()
    if actual != expected:
        archive.unlink()
        raise RuntimeError(f'Checksum mismatch for {archive.name}')

    target = ROOT / 'build' / 'nwjs-arm64'
    if target.exists():
        shutil.rmtree(target)
    staging = cache / 'extract'
    if staging.exists():
        shutil.rmtree(staging)
    prefix = name + '/'
    with tarfile.open(archive) as tar:
        # Windows drops Unix exec bits on extraction; remember them for tools/package_sd.py.
        executables = sorted(m.name[len(prefix):] for m in tar.getmembers()
                             if m.isfile() and m.mode & 0o111 and m.name.startswith(prefix))
        tar.extractall(staging, filter='data')
    shutil.move(str(staging / name), target)
    shutil.rmtree(staging)

    locales = target / 'locales'
    if locales.is_dir():
        for pak in locales.iterdir():
            if pak.name not in KEEP_LOCALES:
                pak.unlink()
    if not (target / 'nw').is_file():
        raise RuntimeError('Extracted NW.js has no nw executable')
    (target / 'VERSION.txt').write_text(version + '\n', encoding='utf-8')
    (target / 'EXECUTABLES.txt').write_text('\n'.join(executables) + '\n', encoding='utf-8')
    size = sum(p.stat().st_size for p in target.rglob('*') if p.is_file())
    print(f'{target} ready ({size / 1e6:.0f} MB). Package it with tools/package_sd.py --nwjs {target}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
