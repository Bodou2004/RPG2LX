"""Validate the Linux ARM64 ELF and report its real device dependencies."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'build/elf-tools'))
from elftools.elf.elffile import ELFFile


def verify(path):
    with Path(path).open('rb') as stream:
        elf=ELFFile(stream)
        if elf.elfclass!=64 or not elf.little_endian or elf.header['e_machine']!='EM_AARCH64':
            raise ValueError('Expected a 64-bit little-endian Linux AArch64 ELF')
        interpreters=[p.get_interp_name() for p in elf.iter_segments() if p.header.p_type=='PT_INTERP']
        if interpreters!=['/lib/ld-linux-aarch64.so.1']:
            raise ValueError('Unexpected ELF loader: '+str(interpreters))
        dynamic=elf.get_section_by_name('.dynamic')
        needed=[t.needed for t in dynamic.iter_tags() if t.entry.d_tag=='DT_NEEDED']
        allowed={'libSDL2-2.0.so.0','libc.so.6','libpthread.so.0','libdl.so.2','libm.so.6','librt.so.1'}
        if set(needed)-allowed or 'libSDL2-2.0.so.0' not in needed:
            raise ValueError('Unexpected dependencies: '+str(needed))
        if any(t.entry.d_tag in ('DT_RPATH','DT_RUNPATH') for t in dynamic.iter_tags()):
            raise ValueError('Build paths must not be embedded in the runtime search path')
        versions=[]
        section=elf.get_section_by_name('.gnu.version_r')
        if section:
            for _,auxiliary in section.iter_versions():
                versions.extend(a.name for a in auxiliary)
        glibc=[tuple(map(int,v.split('_',1)[1].split('.'))) for v in versions
               if re.fullmatch(r'GLIBC_\d+(?:\.\d+)+',v)]
        if any(v>(2,28) for v in glibc):raise ValueError('glibc requirement exceeds the portable 2.28 baseline')
        imports=[symbol.name for symbol in elf.get_section_by_name('.dynsym').iter_symbols()
                 if symbol.entry.st_shndx=='SHN_UNDEF' and symbol.name.startswith('SDL_')]
        return {'machine':'AArch64','bits':64,'interpreter':interpreters[0],
                'needed':needed,'symbol_versions':sorted(set(versions)),
                'sdl_imports':sorted(imports),'sha256':hashlib.sha256(Path(path).read_bytes()).hexdigest()}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    result=verify(args.binary)
    text=json.dumps(result,indent=2)+'\n'
    if args.output:args.output.write_text(text,encoding='utf-8')
    print(text)
