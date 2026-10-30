"""Produce a manual-copy PortMaster SD package; never write to a card or firmware."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import stat
import zipfile
from verify_arm64 import verify

ROOT=Path(__file__).resolve().parents[1]
README='''RPG2LX - Prueba en consola con PortMaster (Linux ARM64)

INSTALAR
1. Extrae RPG2LX.sh y la carpeta rpg2lx dentro de la carpeta ports de la SD
   donde tienes tus otros ports. Ambos deben quedar al mismo nivel.
2. Expulsa la SD correctamente, colócala en la consola y abre RPG2LX desde Ports.
3. Selecciona Project1 y entra a sus detalles; X inicia el modo nativo.
   Si este paquete no incluye juegos, copia uno completo a rpg2lx/games/Nombre/.
4. Dentro del mapa: cruceta/stick para caminar, A para interactuar,
   B para volver a la biblioteca en esta versión experimental.

PARTIDA
No hay barra, contador, indicador ni instrucciones añadidas por RPG2LX.
Los textos de eventos pertenecen al juego. Los errores se registran en logs/;
la aplicación vuelve a la biblioteca en vez de superponer diagnósticos al mapa.

COMPATIBILIDAD
Port estándar PortMaster aarch64; el núcleo no tiene reglas de un firmware.
Usa SDL2 y glibc del dispositivo. No reemplaza sus bibliotecas ni archivos.
Es una build experimental, todavía sin prueba física de esta entrega.
Implementa exploración nativa parcial, no todos los juegos/plugins de MV.
Faltan batalla, audio, guardado, menús MV y ejecución de JavaScript/plugins.

SI NO ARRANCA
Verifica que PortMaster esté instalado y actualizado y que hayas extraído
RPG2LX.sh junto a rpg2lx/, sin una carpeta extra entre ambos.
Revisa rpg2lx/logs/portmaster.log y rpg2lx/logs/launcher.log.
No hace falta formatear la tarjeta ni cambiar el sistema de la consola.

Este paquete local puede contener archivos de tu juego. No se publicó en Internet.
'''


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,default=ROOT/'build/aarch64/rpg2lx.aarch64')
    parser.add_argument('--game',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    output=args.output.resolve()
    if not output.is_relative_to(ROOT/'dist') or output.suffix!='.zip':
        parser.error('Output must be a new .zip inside this project dist/ directory')
    folder=output.with_suffix('')
    if folder.exists() or output.exists():parser.error('Output exists; choose a new package name')
    report=verify(args.binary)
    binary_data=args.binary.read_bytes()
    if b'NATIVO EXPERIMENTAL | A: hablar' in binary_data:
        parser.error('Refusing to package a build containing the removed gameplay HUD')
    toolchain=ROOT/'build/tooling-stable/ziglang'
    licenses={'MV-CORESCRIPT-LICENSE.txt':ROOT/'docs/legal/MV-CORESCRIPT-LICENSE.txt',
              'SDL2-LICENSE.txt':ROOT/'docs/legal/SDL2-LICENSE.txt',
              'LLVM-libcxx-LICENSE.txt':toolchain/'lib/libcxx/LICENSE.TXT',
              'LLVM-libcxxabi-LICENSE.txt':toolchain/'lib/libcxxabi/LICENSE.TXT',
              'LLVM-libunwind-LICENSE.txt':toolchain/'lib/libunwind/LICENSE.TXT',
              'Zig-LICENSE.txt':toolchain/'LICENSE',
              'font8x8-README.txt':ROOT/'third_party/font8x8/README'}
    for source in licenses.values():
        if not source.is_file():parser.error('Missing dependency license: '+str(source))
    game=args.game.resolve() if args.game else None
    if game and not ((game/'data/System.json').is_file() or (game/'www/data/System.json').is_file()):
        parser.error('Game folder must contain an existing MV project or desktop export')
    runtime=folder/'rpg2lx'
    runtime.mkdir(parents=True)
    for directory in ['games','saves','config','logs','licenses']:(runtime/directory).mkdir()
    script=(ROOT/'portmaster/RPG2LX.sh').read_text(encoding='utf-8').replace('\r\n','\n')
    (folder/'RPG2LX.sh').write_bytes(script.encode('utf-8'))
    shutil.copy2(args.binary,runtime/'rpg2lx.aarch64')
    for name,source in licenses.items():shutil.copy2(source,runtime/'licenses'/name)
    stb=(ROOT/'third_party/stb/stb_image.h').read_text(encoding='utf-8')
    (runtime/'licenses/stb-image-LICENSE.txt').write_text(stb[stb.rfind('ALTERNATIVE A - MIT License'):].rstrip('*/\n ')+'\n',encoding='utf-8')
    shutil.copy2(ROOT/'portmaster/port.json',runtime/'port.json')
    (runtime/'LEEME-SD.txt').write_text(README,encoding='utf-8')
    if game:
        if game.name in ('','.','..'):parser.error('Invalid game folder name')
        shutil.copytree(game,runtime/'games'/game.name)
    (runtime/'build-info.json').write_text(json.dumps({
        'platform':'PortMaster Linux aarch64','firmware_specific_code':False,
        'in_game_runtime_hud':False,'tested_on_hardware':False,'elf':report,
        'includes_user_game':bool(game),'game_folder':game.name if game else None
    },indent=2)+'\n',encoding='utf-8')
    manifest=[]
    for path in sorted(folder.rglob('*')):
        if path.is_file():manifest.append(hashlib.sha256(path.read_bytes()).hexdigest()+'  '+path.relative_to(folder).as_posix())
    (runtime/'SHA256SUMS.txt').write_text('\n'.join(manifest)+'\n',encoding='utf-8')
    with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
        for path in sorted(folder.rglob('*')):
            relative=path.relative_to(folder).as_posix()
            if path.is_dir():
                info=zipfile.ZipInfo(relative+'/');info.create_system=3;info.external_attr=(stat.S_IFDIR|0o755)<<16
                archive.writestr(info,b'')
            else:
                info=zipfile.ZipInfo(relative);info.create_system=3
                mode=0o755 if relative in ('RPG2LX.sh','rpg2lx/rpg2lx.aarch64') else 0o644
                info.external_attr=(stat.S_IFREG|mode)<<16
                info.compress_type=zipfile.ZIP_STORED if path.suffix.lower() in ('.ogg','.m4a','.mp3','.png','.mp4') else zipfile.ZIP_DEFLATED
                archive.writestr(info,path.read_bytes())
    with zipfile.ZipFile(output) as archive:
        corrupt=archive.testzip()
        if corrupt:raise RuntimeError('Corrupt archive member: '+corrupt)
    checksum=hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix('.zip.sha256').write_text(checksum+'  '+output.name+'\n',encoding='utf-8')
    print(output)
    print('Copy contents into the SD ports/ directory. ZIP integrity verified.')
    return 0


if __name__=='__main__':raise SystemExit(main())
