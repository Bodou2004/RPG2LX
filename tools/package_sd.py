"""Produce a manual-copy PortMaster SD package; never write to a card or firmware."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import stat
import zipfile
from verify_arm64 import verify

ROOT = Path(__file__).resolve().parents[1]
README = '''RPG2LX 0.5.0 - Runtime 1:1 de RPG Maker MV para PortMaster (Linux ARM64)

INSTALAR
1. Extrae RPG2LX.sh y la carpeta rpg2lx dentro de la carpeta ports de la SD
   donde tienes tus otros ports. Ambos deben quedar al mismo nivel.
2. Copia tus proyectos completos de RPG Maker MV dentro de rpg2lx/games/Nombre/.
   Se detectan tanto carpetas directas como exportaciones con subcarpeta www/.
3. Expulsa la SD correctamente, colócala en la consola y abre RPG2LX desde Ports.

CONTROLES EN CONSOLA (R36S / PortMaster)
- Biblioteca: Cruceta/Stick elegir | B o A iniciar juego | R1 actualizar | Start salir.
- En el juego (1:1 RPG Maker MV):
  * B (botón Sur): Aceptar / Hablar / Confirmar
  * A (botón Este) / X (Norte) / Start: Cancelar / Abrir y cerrar menú
  * Y (botón Oeste): Correr (mantener)
  * L1 / R1: Cambiar personaje en menús (PageUp / PageDown)
  * Select + Start: Cerrar partida y volver a la biblioteca RPG2LX

CARACTERÍSTICAS 1:1
- Ejecuta directamente los scripts originales del proyecto (rpg_core, rpg_managers,
  rpg_objects, rpg_scenes, rpg_sprites, rpg_windows y plugins como CustomLogo.js).
- Pantallas de presentación/logos, pantalla de título original, animaciones de ventanas,
  cursores, diálogos con códigos de escape, seguidores, animaciones de base de datos,
  batallas completas, audio Ogg/.rpgmvo con LOOPSTART/LOOPLENGTH y tipografía GameFont
  adaptada con escalado nítido a pantallas 640x480.
- Las partidas guardadas (.rpgsave) y opciones se guardan aisladas en rpg2lx/saves/<juego>/
  sin modificar jamás los archivos originales del proyecto.

SI NO ARRANCA
Verifica que PortMaster esté instalado y que RPG2LX.sh esté junto a rpg2lx/.
Revisa rpg2lx/logs/portmaster.log y rpg2lx/logs/launcher.log.
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/aarch64/rpg2lx.aarch64')
    parser.add_argument('--game', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / 'dist') or output.suffix != '.zip':
        parser.error('Output must be a new .zip inside this project dist/ directory')
    folder = output.with_suffix('')
    if folder.exists() or output.exists():
        parser.error('Output exists; choose a new package name')
    report = verify(args.binary)
    binary_data = args.binary.read_bytes()
    if b'NATIVO EXPERIMENTAL | A: hablar' in binary_data:
        parser.error('Refusing to package a build containing the removed gameplay HUD')
    toolchain = ROOT / 'build/tooling-stable/ziglang'
    licenses = {
        'MV-CORESCRIPT-LICENSE.txt': ROOT / 'docs/legal/MV-CORESCRIPT-LICENSE.txt',
        'SDL2-LICENSE.txt': ROOT / 'docs/legal/SDL2-LICENSE.txt',
        'LLVM-libcxx-LICENSE.txt': toolchain / 'lib/libcxx/LICENSE.TXT',
        'LLVM-libcxxabi-LICENSE.txt': toolchain / 'lib/libcxxabi/LICENSE.TXT',
        'LLVM-libunwind-LICENSE.txt': toolchain / 'lib/libunwind/LICENSE.TXT',
        'Zig-LICENSE.txt': toolchain / 'LICENSE',
        'font8x8-README.txt': ROOT / 'third_party/font8x8/README',
        'QuickJS-LICENSE.txt': ROOT / 'third_party/quickjs/LICENSE',
    }
    for source in licenses.values():
        if not source.is_file():
            parser.error('Missing dependency license: ' + str(source))
    game = args.game.resolve() if args.game else None
    if game and not ((game / 'data/System.json').is_file() or (game / 'www/data/System.json').is_file()):
        parser.error('Game folder must contain an existing MV project or desktop export')
    runtime = folder / 'rpg2lx'
    runtime.mkdir(parents=True)
    for directory in ['games', 'saves', 'config', 'logs', 'licenses']:
        (runtime / directory).mkdir()
    script = (ROOT / 'portmaster/RPG2LX.sh').read_text(encoding='utf-8').replace('\r\n', '\n')
    (folder / 'RPG2LX.sh').write_bytes(script.encode('utf-8'))
    shutil.copy2(args.binary, runtime / 'rpg2lx.aarch64')
    for name, source in licenses.items():
        shutil.copy2(source, runtime / 'licenses' / name)
    stb = (ROOT / 'third_party/stb/stb_image.h').read_text(encoding='utf-8')
    (runtime / 'licenses/stb-LICENSE.txt').write_text(
        stb[stb.rfind('ALTERNATIVE A - MIT License'):].rstrip('*/\n ') + '\n', encoding='utf-8')
    shutil.copy2(ROOT / 'portmaster/port.json', runtime / 'port.json')
    (runtime / 'LEEME-SD.txt').write_text(README, encoding='utf-8')
    if game:
        if game.name in ('', '.', '..'):
            parser.error('Invalid game folder name')
        shutil.copytree(game, runtime / 'games' / game.name)
    (runtime / 'build-info.json').write_text(json.dumps({
        'runtime_version': '0.5.0',
        'platform': 'PortMaster Linux aarch64',
        'firmware_specific_code': False,
        'in_game_runtime_hud': False,
        'tested_on_hardware': False,
        'elf': report,
        'includes_user_game': bool(game),
        'game_folder': game.name if game else None,
    }, indent=2) + '\n', encoding='utf-8')
    manifest = []
    for path in sorted(folder.rglob('*')):
        if path.is_file():
            manifest.append(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + path.relative_to(folder).as_posix())
    (runtime / 'SHA256SUMS.txt').write_text('\n'.join(manifest) + '\n', encoding='utf-8')
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(folder.rglob('*')):
            relative = path.relative_to(folder).as_posix()
            if path.is_dir():
                info = zipfile.ZipInfo(relative + '/')
                info.create_system = 3
                info.external_attr = (stat.S_IFDIR | 0o755) << 16
                archive.writestr(info, b'')
            else:
                info = zipfile.ZipInfo(relative)
                info.create_system = 3
                mode = 0o755 if relative in ('RPG2LX.sh', 'rpg2lx/rpg2lx.aarch64') else 0o644
                info.external_attr = (stat.S_IFREG | mode) << 16
                info.compress_type = (
                    zipfile.ZIP_STORED
                    if path.suffix.lower() in ('.ogg', '.m4a', '.mp3', '.png', '.mp4')
                    else zipfile.ZIP_DEFLATED
                )
                archive.writestr(info, path.read_bytes())
    with zipfile.ZipFile(output) as archive:
        corrupt = archive.testzip()
        if corrupt:
            raise RuntimeError('Corrupt archive member: ' + corrupt)
    checksum = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_suffix('.zip.sha256').write_text(checksum + '  ' + output.name + '\n', encoding='utf-8')
    print(output)
    print('Copy contents into the SD ports/ directory. ZIP integrity verified.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
