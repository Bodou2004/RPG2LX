# Dependencias de terceros

Todo lo que se compila dentro del binario o se distribuye en el paquete de la SD. `tools/package_sd.py` copia cada licencia a `rpg2lx/licenses/` y falla si falta alguna.

| Componente | Origen | Licencia | Uso en RPG2LX | En el paquete |
|---|---|---|---|---|
| QuickJS | `third_party/quickjs`, de Fabrice Bellard y Charlie Gordon | MIT | Motor JavaScript que ejecuta los corescripts y plugins. Modificación local marcada `RPG2LX` en `quickjs.c`: `RegExp.$1`–`$9` y el resto de estáticos antiguos de `RegExp`, también como `rpg2lx-regexp-legacy.patch`. | `QuickJS-LICENSE.txt` |
| canvas_ity | `third_party/canvas_ity` | ISC | Rasterizador de Canvas 2D para trazados, curvas, degradados, patrones, transformaciones y recortes. | `canvas_ity-LICENSE.txt` |
| stb_image 2.30 | `third_party/stb`, de [nothings/stb](https://github.com/nothings/stb) | MIT o dominio público, a elegir | Decodificación de PNG. No se usan sus funciones de apertura de archivos: RPG2LX lee con rutas de filesystem para soportar Unicode. | `stb-LICENSE.txt` |
| stb_vorbis | `third_party/stb` | MIT o dominio público, a elegir | Decodificación de audio OGG. | `stb-LICENSE.txt` (misma licencia) |
| stb_truetype | `third_party/stb` | MIT o dominio público, a elegir | Rasterizado de fuentes TrueType. | `stb-LICENSE.txt` (misma licencia) |
| font8x8 | `third_party/font8x8`, de Daniel Hepper y colaboradores ([repositorio](https://github.com/dhepper/font8x8)) | Dominio público | Fuente de mapa de bits de respaldo (ASCII y Latin-1). Modificación local: los arrays pasan de `char` a `static const unsigned char`. | No hace falta |
| NW.js v0.119.2 (Chromium 156, Node) | [dl.nwjs.io](https://dl.nwjs.io), compilación oficial linux-arm64, comprobada con su `SHASUMS256.txt` | MIT (NW.js); BSD y otras de Chromium y Node | Motor navegador. Se distribuye tal cual en `rpg2lx/nwjs/`. | `nwjs/credits.html` |
| NSS 3.87.1 y NSPR 4.35 | Paquetes arm64 de Debian 12 (`libnss3`, `libnspr4`), comprobados con el SHA-256 del índice de Debian | MPL 2.0 | Librerías que NW.js necesita y ROCKNIX no trae, en `rpg2lx/nwjs/syslibs/`. | `libnss3-copyright.txt`, `libnspr4-copyright.txt` |
| SDL2 | Biblioteca del sistema en la consola; el SDK solo se usa para compilar | zlib | Ventana, renderizado, entrada y audio. Se enlaza dinámicamente y no se distribuye. | `SDL2-LICENSE.txt` |
| libc++, libc++abi, libunwind (LLVM) | Toolchain Zig | Apache 2.0 con excepción LLVM | Biblioteca estándar de C++ enlazada estáticamente en el binario ARM64. | `LLVM-*-LICENSE.txt` |
| Runtime de Zig | Toolchain Zig | MIT | Código de arranque enlazado por el compilador. | `Zig-LICENSE.txt` |
| Corescript de RPG Maker MV | KADOKAWA CORPORATION / YOJI OJIMA | MIT | Tablas de autotiles y fórmulas de layout adaptadas de `Tilemap.js` para el renderizador nativo. Los corescripts completos los aporta cada juego. | `MV-CORESCRIPT-LICENSE.txt` |

RPG2LX en sí todavía no tiene una licencia general elegida. Hay que decidirla antes de publicarlo en PortMaster.
