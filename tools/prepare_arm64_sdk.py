"""Download a pinned, link-only Linux ARM64 SDL2 SDK into build/."""
import hashlib
import io
import json
from pathlib import Path
import tarfile
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
PACKAGES={
    'libsdl2-2.0-0_2.0.14+dfsg2-3+deb11u1_arm64.deb':'8a8b0ef43a4d7825a23ea817e7dd656dde7fe3fe58e87d20c7fdfd4fc72f373c',
    'libsdl2-dev_2.0.14+dfsg2-3+deb11u1_arm64.deb':'77c694ed46f03290c61349261b03bf3765ec626454fcde22185326e939344845',
}
destination=ROOT/'build/cross-sdl';destination.mkdir(parents=True,exist_ok=True)
manifest={}
for name,expected in PACKAGES.items():
    url='https://deb.debian.org/debian/pool/main/libs/libsdl2/'+name
    path=destination/name
    if not path.is_file():urllib.request.urlretrieve(url,path)
    data=path.read_bytes()
    if hashlib.sha256(data).hexdigest()!=expected:raise ValueError('SDK checksum mismatch: '+name)
    if data[:8]!=b'!<arch>\n':raise ValueError('Invalid Debian SDK archive')
    position=8
    while position+60<=len(data):
        header=data[position:position+60];size=int(header[48:58]);member=header[:16].decode().strip().rstrip('/')
        payload=data[position+60:position+60+size];position+=60+size+size%2
        if member.startswith('data.tar'):
            with tarfile.open(fileobj=io.BytesIO(payload),mode='r:*') as archive:
                archive.extractall(destination/'sysroot',filter='data')
    manifest[name]={'url':url,'sha256':expected}
(destination/'sources.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print(destination/'sysroot')
