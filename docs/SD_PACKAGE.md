# Paquete para SD — PortMaster ARM64

Esta entrega usa el contrato estándar de PortMaster. El motor no consulta el firmware ni tiene reglas propias de Aurknix. SDL2, glibc, vídeo y controles los proporciona el dispositivo a través de PortMaster.

Estado vigente: runtime 0.3.1-alpha y Project1 de pruebas instalados en H:/ports. Se actualizaron siete JSON del juego, con respaldo y manifiesto de 1116 entradas verificado. El recorrido de 95 pasos pasó también leyendo el proyecto desde la SD mediante una prueba oculta del motor; la ejecución física de esta versión en consola sigue pendiente. Guía y alcance: TEST_PROJECT.md. Paquete de runtime más reciente: dist/RPG2LX-0.3.1-PortMaster-update.zip; no incluye los datos nuevos del juego, instalados por separado con tools/sync_game_data.py.

## Instalación

Paquete inicial con Project1: `dist/RPG2LX-PortMaster-aarch64-Project1.zip` (0.2). Actualización del runtime 0.3: `dist/RPG2LX-0.3.0-PortMaster-update.zip`, sin juegos. Extraer la actualización sobre la instalación existente conserva games/ y saves/.

Extraer el contenido directamente en la carpeta **ports** de la SD donde están los otros ports:

```text
ports/
├── RPG2LX.sh
└── rpg2lx/
    ├── rpg2lx.aarch64
    ├── games/Project1/   (1102 archivos del proyecto del usuario)
    ├── saves/
    ├── config/
    ├── logs/
    ├── licenses/
    └── LEEME-SD.txt
```

Abrir RPG2LX desde Ports, elegir Project1 y pulsar X. Se abre su título con Nueva partida/Continuar. Cruceta/stick caminan, A interactúa, B/Start abre o cancela el menú y Y corre. Biblioteca: Menú → Salir → Biblioteca RPG2LX. No copiar SDKs, DLLs ni ejecutables Windows. La instalación inicial en H:/ports fue realizada y verificada por Codex con autorización del usuario.

## Sin HUD añadido

Se quitaron la banda NATIVO EXPERIMENTAL, los controles flotantes y el texto añadido de continuar. Los errores van al log y devuelven a la biblioteca, sin diagnósticos sobre el mapa. Se mantiene el contenido del juego dentro de las funciones MV que este motor parcial ya implementa; no se afirma fidelidad completa de plugins, ventanas o menús MV.

## Verificación

- ELF64 AArch64 little-endian, loader `/lib/ld-linux-aarch64.so.1`.
- Dependencias: libSDL2-2.0.so.0, libc.so.6, libpthread.so.0 y libdl.so.2; versiones de símbolos requeridas: GLIBC_2.17.
- Sin bibliotecas Windows ni rutas de compilación embebidas. Runtime C++ enlazado estáticamente, con sus licencias incluidas.
- SDK SDL2 ARM64 2.0.14 usado únicamente para enlace; no se entrega esa biblioteca para conservar la SDL2 del firmware.
- ZIP, permisos ejecutables, LF del launcher y manifiesto SHA-256 de cada archivo comprobados.
- Launcher probado con contrato PortMaster simulado: segunda SD/rutas con espacios, salida, limpieza y rechazo de arquitectura incorrecta.
- Pruebas automáticas de desarrollo verifican carga y ausencia del HUD. No se solicita al usuario jugar en PC.

**El usuario probó la versión inicial en consola y señaló las limitaciones que motivan 0.3.** Esta actualización necesita una nueva prueba física. Incluye título, seguidores, menús y saves propios. Siguen pendientes batalla, audio y JavaScript/plugins, además de numerosos comandos y efectos. Los logs están en rpg2lx/logs/portmaster.log y launcher.log.

## Portabilidad y fuentes

[PortMaster packaging](https://portmaster.games/packaging.html) y [build environments](https://portmaster.games/build-environments.html) definen la integración general.

El entorno que indicó el usuario se consultó solo como referencia: [Aurknix](https://github.com/lcdyk0517/distribution_aurknix), commit `e4cc43eb8eb56d94ff4054884554f22bac4d4f0a`. Su [paquete SDL2](https://github.com/lcdyk0517/distribution_aurknix/blob/e4cc43eb8eb56d94ff4054884554f22bac4d4f0a/projects/ROCKNIX/packages/graphics/SDL2/package.mk) declara 2.32.10 y [glibc](https://github.com/lcdyk0517/distribution_aurknix/blob/e4cc43eb8eb56d94ff4054884554f22bac4d4f0a/projects/ROCKNIX/packages/devel/glibc/package.mk) 2.40. No se incorporó código de esa distribución al runtime ni se condiciona su ejecución al nombre del sistema.

Paquete local con archivos del usuario; no se publicó en Internet ni se envió a un catálogo.

Actualización 0.3 instalada en H:/ports: 14 archivos de runtime/metadatos/manifiesto, con respaldo local previo y verificación SHA-256 de las 1115 entradas instaladas. Los 1104 archivos de games/saves/config/logs conservaron tamaño/fecha. Reporte local build/sd-update-report.json. No se expulsó la tarjeta.

## Reconstrucción

Zig 0.13.0 y Python >=3.12:

```text
python -m pip install --target build/elf-tools pyelftools==0.32
python tools/prepare_arm64_sdk.py
python tools/build_arm64.py --zig build/tooling-stable/ziglang/zig.exe --sdl-root build/cross-sdl/sysroot
python tools/verify_arm64.py build/aarch64/rpg2lx.aarch64 --output build/aarch64/elf-report.json
python tools/package_sd.py --game games/Project1 --output dist/nuevo-paquete.zip
```

Descargas del SDK fijadas por SHA-256. La herramienta no sobrescribe paquetes existentes. Los juegos y los artefactos de dist/build quedan fuera de Git.
