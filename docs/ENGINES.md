# Motores de juego

RPG2LX tiene dos motores. El HUB elige uno por juego; ninguno modifica los archivos del juego.

| | Nativo | Navegador |
|---|---|---|
| Qué es | QuickJS + renderizador SDL2 propio que ejecuta los `js/rpg_*.js` y plugins originales | NW.js (Chromium real) oficial `linux-arm64` |
| Compatibilidad | Corescripts de MV y plugins que usan las APIs que el puente imita | Cualquier plugin: DOM, WebGL, Node, vídeo |
| Rendimiento | Ligero, pensado para 60 FPS en RK3326 | Pesado: en 1 GB de RAM otros ports reportan ~20 FPS |
| Requisitos | Solo el binario | Carpeta `rpg2lx/nwjs/` y runtime Westonpack de PortMaster |

## Auto

`Auto` (por defecto) usa el nativo. Si un juego sale con error en el nativo, RPG2LX lo anota en
`config/games/<juego>/native_result.txt` y la próxima vez lo abre en el navegador, si está instalado.
Se puede fijar el motor a mano desde el menú del juego en el HUB (`config/games/<juego>/engine.txt`).

## Instalar el motor navegador

```sh
python tools/fetch_nwjs.py --version v0.119.2
python tools/package_sd.py --binary build/aarch64/rpg2lx.aarch64 --nwjs build/nwjs-arm64 --output dist/RPG2LX-0.6.0.zip
```

`fetch_nwjs.py` descarga de dl.nwjs.io, comprueba el SHA-256 publicado y deja solo los idiomas en/es.

En la consola, `RPG2LX.sh` monta `weston_pkg_0.2` (lo descarga `harbourmaster` si falta), lanza
gptokeyb con los controles del juego y abre NW.js dentro de Westonpack. Así funciona en cualquier
CFW con PortMaster, no solo en los que traen Wayland. El modo por defecto es
`headless noop kiosk crusty_x11egl`; si un dispositivo muestra pantalla negra se puede probar otro
exportando `RPG2LX_WESTON_MODE` antes de lanzar (ver la wiki de Westonpack).

NW.js recibe una app envoltorio en `config/games/<juego>/nwapp/package.json` que apunta al
`index.html` del juego, con su perfil de Chromium en `config/games/<juego>/chromium/`.

## Controles

Los dos motores reciben teclas de teclado, no botones:

- Nativo: cada botón se convierte en un `keydown`/`keyup` DOM real, que pasa por `Input.keyMapper`
  (incluidos los cambios que hagan los plugins) y llega a los listeners de teclado de los plugins.
- Navegador: `rpg2lx export-gptk` genera el `.gptk` de gptokeyb con el mismo perfil.

Perfil global: `config/controls.ini`. Perfil de un juego: `config/games/<juego>/controls.ini`.

## Pendiente de comprobar en hardware

El modo navegador (Westonpack + NW.js + gptokeyb) está escrito siguiendo las plantillas de PortMaster
pero no se ha ejecutado en una consola. Tampoco se ha medido el rendimiento del nativo en RK3326.
