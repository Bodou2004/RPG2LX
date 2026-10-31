# Estado de RPG2LX

2026-10-31 — versión 0.7.0.

## Qué hay

- HUB para PortMaster: biblioteca, menú por juego y editor de controles, global o por juego, editable antes de entrar.
- HUB y juego en procesos separados: `RPG2LX.sh` abre el HUB, lanza el juego elegido y vuelve al HUB.
- Un solo motor, nativo (QuickJS + SDL2), que ejecuta los corescripts y plugins originales del juego. El motor navegador (NW.js) se quitó en 0.7.0.
- Los juegos corren como exportados: `Utils.isOptionValid` siempre es falso. No hay playtest, ni F8/F9, ni editores de plugins que dependan de `test`.

## Compatibilidad con plugins (0.7.0)

- **Canvas 2D completo** con `canvas_ity` (`third_party/canvas_ity`, ISC): trazados, arcos, curvas, elipses, `roundRect`, trazos con grosor, terminaciones, uniones y guiones, degradados lineales y radiales, patrones, transformaciones, `clip`, sombras, `Path2D` e `isPointInPath`. Rectángulos, imágenes y texto sin transformación siguen por la ruta rápida nativa.
- **DOM:** elementos con `classList`, atributos, `querySelector*`, `appendChild`/`insertBefore`/`remove`, eventos reales, `document.hasFocus`, `readyState`, constructores `HTML*Element` para `instanceof`.
- **APIs de navegador:**
  - Eventos: `Event`/`CustomEvent`/`KeyboardEvent`…
  - Codificación y binarios: `atob`/`btoa`, `TextEncoder`/`TextDecoder`, `Blob`/`File`/`FileReader`, `fetch` (archivos del juego).
  - Almacenamiento: `sessionStorage`.
  - Ventana y pantalla: `screen`, `matchMedia`, `getComputedStyle`.
  - Utilidades: `crypto`, `structuredClone`, `queueMicrotask`.
  - Stubs de observadores y `Worker`.
- **Node/NW.js:**
  - Módulos: `fs` (síncrono, con callbacks y promesas; lee del juego y escribe en `saves/<juego>`), `path` completo, `os`, `events`, `util`, `buffer`, `Buffer`. `child_process` devuelve error en vez de romper el juego.
  - `nw.Window`/`nw.App`/`nw.Shell` ampliados.
  - Rutas `/game/...` y `file://` resueltas al juego.
- **Carga de plugins como en un navegador:** si un plugin lanza un error al cargarse, se registra y el resto sigue.
- **Archivos sin distinguir mayúsculas y minúsculas**, como en Windows (`img/Pictures/Hero.PNG` = `img/pictures/hero.png`).
- **Errores completos en el log:** mensaje y pila.

## Controles

Por defecto cada botón envía una acción de MV ([Aceptar], [Cancelar], [Menú], [Correr], páginas, direcciones). El motor pulsa la tecla que el `Input.keyMapper` del juego tenga asignada a esa acción. También se puede asignar cualquier tecla concreta: letras, números, F1–F12, Enter, flechas…

## Qué está comprobado (PC, Windows)

- Toda la suite de `tools/build_native.py --release`. Incluye la prueba completa contra la plantilla `NewData` de Steam (logo, título, mapa, mensaje, menú, guardado, batalla), el teclado vía `keyMapper` y una prueba de píxeles del Canvas 2D.
- SAOR DOMINA (`Games/Project1/SAOR-DOMINA-PLUGIN-TEST`, 21 plugins activos) sin modificar: arranca, título MOG, nueva partida, mapa con su HUD y movimiento. Corrió 1037 frames sin errores, a una media de 7,7 ms por frame en PC (`tests/mv_probe.cpp`).

## Qué no está comprobado

- Rendimiento y controles en la consola.
- En la prueba de Domina aparecen un cuadrado magenta y rectángulos negros sin comparar contra NW.js: pueden ser del juego o del motor.
- Filtros de PIXI (se ignoran), vídeo (`.webm` se salta) y las APIs de PIXI más allá de las que usa MV.
