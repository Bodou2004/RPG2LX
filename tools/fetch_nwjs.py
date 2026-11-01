"""Prepare build/nwjs-arm64 for RPG2LX's browser engine.

1. The official NW.js linux-arm64 build, checked against the SHASUMS256.txt published next to it on dl.nwjs.io.
   Unused locales are dropped to save SD space.
2. nwjs/syslibs: the NSS and NSPR libraries NW.js links against that ROCKNIX does not ship. They come from the
   Debian 12 (bookworm) arm64 packages, checked against the SHA-256 in Debian's package index; Debian 12's glibc
   (2.36) is older than any current firmware's, so they load there.

Nothing outside build/ is touched.
Example: python tools/fetch_nwjs.py --version v0.119.2
"""
import argparse
import hashlib
import io
import lzma
from pathlib import Path
import shutil
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
KEEP_LOCALES = {'en-US.pak', 'es.pak', 'es-419.pak'}
DEBIAN = 'https://deb.debian.org/debian'
DEBIAN_INDEX = DEBIAN + '/dists/bookworm/main/binary-arm64/Packages.xz'
NSS_PACKAGES = ('libnss3', 'libnspr4')


def download(url, destination):
    # dl.nwjs.io answers 403 to urllib's default User-Agent.
    request = urllib.request.Request(url, headers={'User-Agent': 'rpg2lx-fetch-nwjs/1.0'})
    with urllib.request.urlopen(request) as response, destination.open('wb') as out:
        shutil.copyfileobj(response, out, length=1 << 20)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fetch_checked(url, destination, expected):
    if not destination.is_file() or sha256(destination) != expected:
        print(f'Downloading {url}...', flush=True)
        download(url, destination)
    if sha256(destination) != expected:
        destination.unlink()
        raise RuntimeError(f'Checksum mismatch for {destination.name}')


def debian_packages(cache):
    """Filename and SHA256 of each wanted package, from Debian's index."""
    index = cache / 'debian-bookworm-arm64-Packages.xz'
    download(DEBIAN_INDEX, index)
    found = {}
    for block in lzma.decompress(index.read_bytes()).decode('utf-8').split('\n\n'):
        fields = dict(line.split(': ', 1) for line in block.splitlines() if ': ' in line and not line.startswith(' '))
        if fields.get('Package') in NSS_PACKAGES:
            found[fields['Package']] = (fields['Filename'], fields['SHA256'])
    missing = set(NSS_PACKAGES) - set(found)
    if missing:
        raise RuntimeError('Not in the Debian index: ' + ', '.join(sorted(missing)))
    return found


def deb_data(deb):
    """The data.tar member of a .deb (an ar archive) as an open tarfile."""
    blob = deb.read_bytes()
    if not blob.startswith(b'!<arch>\n'):
        raise RuntimeError(f'{deb.name} is not a .deb')
    pos = 8
    while pos < len(blob):
        name = blob[pos:pos + 16].decode().strip().rstrip('/')
        size = int(blob[pos + 48:pos + 58].decode().strip())
        body = blob[pos + 60:pos + 60 + size]
        if name.startswith('data.tar'):
            return tarfile.open(fileobj=io.BytesIO(body))
        pos += 60 + size + (size & 1)
    raise RuntimeError(f'{deb.name} has no data.tar')


def fetch_syslibs(cache, target):
    syslibs = target / 'syslibs'
    syslibs.mkdir()
    for package, (filename, expected) in debian_packages(cache).items():
        deb = cache / Path(filename).name
        fetch_checked(f'{DEBIAN}/{filename}', deb, expected)
        with deb_data(deb) as tar:
            for member in tar.getmembers():
                # Libraries only (the nss/ subfolder holds softokn3 and freebl3, which libnss3 loads at run time).
                if member.isfile() and '/aarch64-linux-gnu/' in member.name and '.so' in Path(member.name).name:
                    data = tar.extractfile(member).read()
                    (syslibs / Path(member.name).name).write_bytes(data)
                elif member.isfile() and member.name.endswith(f'/doc/{package}/copyright'):
                    (target / f'{package}-copyright.txt').write_bytes(tar.extractfile(member).read())
        print(f'{package}: {Path(filename).name}', flush=True)
    # Sonames the loader asks for, where the package only has the versioned file.
    for lib in list(syslibs.iterdir()):
        stem = lib.name.split('.so')[0] + '.so'
        if not (syslibs / stem).exists():
            shutil.copy2(lib, syslibs / stem)
    return sorted(p.name for p in syslibs.iterdir())


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
    fetch_checked(f'{base}/{archive.name}', archive, expected)

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
    libs = fetch_syslibs(cache, target)
    (target / 'VERSION.txt').write_text(version + '\n', encoding='utf-8')
    (target / 'EXECUTABLES.txt').write_text('\n'.join(executables) + '\n', encoding='utf-8')
    size = sum(p.stat().st_size for p in target.rglob('*') if p.is_file())
    print(f'syslibs: {", ".join(libs)}')
    print(f'{target} ready ({size / 1e6:.0f} MB). Package it with tools/package_sd.py --nwjs {target}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
