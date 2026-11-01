# Estado de RPG2LX

2026-11-01 — versión 0.8.3.

## GPU del navegador, motor Auto y medición en la consola (0.8.1–0.8.3)

**Medido en la K36 (ROCKNIX 20260809):**

- 0.8.0 no llegó a abrir NW.js. Chromium no podía crear su perfil en la SD: FAT/exFAT no admite el enlace `SingletonLock`, y una ruta con espacios se partía. Todos los juegos acababan en el motor nativo. Desde 0.8.1 el perfil va a tmpfs (`/tmp/rpg2lx-nw/<id>`); desde 0.8.3 se borra antes y después de cada juego, porque tmpfs ocupa RAM.
- Con NW.js ya arrancando, Chromium dibujaba con la CPU: 1–3 fps en los mapas de Domina y F&H. En los ajustes probados ANGLE responde `eglInitialize: No configs were generated` y Chromium cierra su proceso de GPU.
- Causa: ROCKNIX usa en RK3326 el driver cerrado **libmali** por defecto. Su `gpudriver`, con el ajuste `gpu.driver`, monta `/usr/lib/mali/*` encima de `libEGL`. El log lo confirma con `arm_release_ver: g24p0`. El proceso de GPU de Chromium necesita un nodo render DRM y GBM, y libmali no los tiene. SDL sí funciona porque dibuja en la ventana de Wayland.
- Motor nativo con F&H en la consola: 80–600 ms por frame (8–48 % de velocidad de juego).
- Las líneas de "270 fps, 450 %" eran un error de medida. Se medía antes de la espera que limita a 60 FPS, así que contaba el trabajo y no el tiempo real.

**Cambios:**

- Prueba de GPU (`RPG2LX.sh`, `browser_gpu_flags`):
  - Prueba primero `--in-process-gpu`, que dibuja en el proceso principal a través de la ventana de Wayland, como SDL.
  - Después prueba los ajustes de ANGLE con proceso de GPU, que sirven con panfrost (Mesa).
  - El resultado se guarda en `config/browser-gpu.txt` junto con el driver, el kernel y NW.js, y se repite si alguno cambia.
  - Por cada intento se escriben en `browser.log` las líneas de GPU de Chromium.
  - Si nada funciona con libmali, el log sugiere probar panfrost en ROCKNIX.
- Motor **Auto**, nuevo por defecto: el navegador si la prueba encontró GPU, el nativo si no. "Navegador" lo fuerza aunque dibuje con la CPU, y el HUB lo marca como "Navegador sin GPU". Lo ya elegido en `engine.txt` se respeta.
- Ya no se pasa al motor nativo cuando el jugador cierra NW.js en los primeros segundos (Select+Start, 137) ni por un SIGTERM (143). gptokeyb arranca después de la prueba, así que Select+Start no puede matarla.
- Memoria, como pidieron las revisiones externas:
  - Chromium con 1 MB de caché de disco y de medios, sin caché de shaders en disco y con un solo proceso de renderizado.
  - Sin `--max-old-space-size`: V8 ya calcula su heap a partir de la RAM, y un límite menor tiraría un juego grande en vez de dejarle usar la memoria que hay.
- Las líneas `perf:` llevan la memoria:
  - Motor nativo: `rss` del proceso, `free` (MemAvailable) y `swap`.
  - Navegador: `nw` (RssAnon + RssShmem de todos sus procesos, leído sin bloquear el juego), `free` y `swap`.
- Motor nativo:
  - La línea `perf:` usa tiempo real, sale cada 10 s aunque el juego vaya lento y separa el tiempo de JS del de dibujo.
  - **Perfilador por muestreo** de JavaScript: cada 30 s, si el juego no llega a 60 FPS, escribe en `launcher.log` las 12 funciones más pesadas y los pares "función < quien la llama", como `nombre (archivo:línea donde está definida)`. El tiempo de las llamadas nativas cuenta para la función JS que las hace; el dibujo de la escena va aparte.
  - Para el perfilador, QuickJS mira las interrupciones también al entrar en cada función y cada 1000 pasos en vez de 10000. Está en `rpg2lx-profiler.patch`.
  - `mv_probe` tiene el comando `profile N`.
- `ctx.save()`/`restore()` del shim de Canvas sin `for…in` ni objetos de 21 claves; TerraxLighting los llama en cada luz. En PC, selección de personaje de F&H: `restore` pasa del 9 % del JS a no aparecer, y el frame de 4,74 a 4,57 ms.
- HUB: la fila Motor se veía `< < Navegador > >`.

**Perfil en PC** (F&H, selección de personaje, 4,6 ms por frame, casi todo JS):

- Lo más pesado es el código estándar de MV para las 100 imágenes de pantalla: `Game_Screen.realPictureId`, `Array.prototype.equals` desde `Sprite.setColorTone`, y `Sprite_Picture.update*`.
- Le sigue TerraxLighting.
- Es código del juego que QuickJS interpreta. En la A35 va unas 20–30 veces más lento que en PC.

## Motor navegador (0.8.0)

- Vuelve el motor NW.js, ahora por defecto cuando `rpg2lx/nwjs/` está instalado, sin Westonpack y con vuelta automática al nativo si no arranca. Detalle en [ENGINES.md](ENGINES.md).
- Probado en PC con NW.js v0.119.2 para Windows (la misma versión que la de ARM64), con la app generada por `rpg2lx browser-app`:
  - SAOR DOMINA: partida nueva, mapa 26, ~60 fps, guardado en `saves/<id>/`.
  - Fear & Hunger 1.4.1: partida nueva, mapa 72, ~60 fps, guardado. Su único error es el `SyntaxError` de su propio `physical_attack_animation.js`, que también sale en el juego original.
- Las dependencias de NW.js ARM64 y sus versiones de glibc se comprobaron contra el `SYSTEM` real de ROCKNIX 20260809.
- En la consola no llegó a arrancar; ver la sección 0.8.1–0.8.3.

## Robustez (0.7.6)

- Enlaces simbólicos: `path_is_inside()` (`src/mv/project_source.cpp`) resuelve la ruta real con `weakly_canonical`. `find_project_file()` devuelve una ruta vacía si un enlace dentro del juego lleva fuera, y el shim `fs` solo escribe dentro de `saves/` y solo lee de `saves/` o del juego. Probado en PC con uniones de Windows: leer a través de un enlace da `ENOENT` y escribir da `EACCES`.
- Guardado a prueba de cortes: cada escritura del shim `fs` (también los `.rpgsave`) va a un archivo temporal, se vuelca a la tarjeta (`fsync`) y se renombra encima del original. Si la batería se agota a mitad, queda el archivo viejo o el nuevo, nunca uno a medias.
- El relleno de degradado radial nativo ignora centros o radios no finitos (`NaN`); antes podían dar un índice fuera de la tabla de colores.
- Los cambios propios en QuickJS están también en `third_party/quickjs/rpg2lx-regexp-legacy.patch`, para volver a aplicarlos al actualizar QuickJS.
- CI: `.github/workflows` en la rama `ci/github-actions` (pendiente de PR), con cuatro jobs que pasan.

## Luces y seguridad (0.7.5)

- `fillRect` con un degradado radial concéntrico (el que dibujan los plugins de luces como TerraxLighting cada frame) tiene una ruta nativa propia en vez de `canvas_ity`. Interpola los colores en RGB sin premultiplicar, como dice el estándar de Canvas y hace NW.js (`canvas_ity` lo hace en espacio lineal). Fear & Hunger, selección de personaje, en PC: de ~16 ms a ~4,4 ms por frame.
- Seguridad del shim `fs`: la ruta se comprobaba antes de decodificarla, y `%2e%2e/` podía salir de `saves/`. Ahora se decodifica primero y se rechaza cualquier componente `..`, raíz o unidad. Un nombre como `Hero..png` sigue siendo válido.
- La comprobación vive en un solo sitio, `is_safe_relative()` (`src/mv/project_source.cpp`), y `find_project_file()` la aplica siempre: devuelve una ruta vacía para cualquier ruta que salga del juego, aunque quien la llame olvide comprobarla. Sustituye a las búsquedas de la cadena `".."`, que también rechazaban nombres legítimos.
- `fs.writeFileSync` escribe bytes reales para `ArrayBuffer`, arrays tipados, `DataView` y `Buffer` (antes los convertía a texto, `1,2,3`), acepta `'base64'` y lanza `EACCES` si no puede escribir. `readFileSync` sin codificación devuelve un `Buffer`, como Node. `appendFileSync` y `copyFileSync` funcionan con binarios.
- `crypto.getRandomValues` y `randomUUID` usan la fuente aleatoria del sistema en vez de `Math.random`.
- Documentación al día: `port.json` ya no promete soporte "completo" de DOM y Node, `THIRD_PARTY.md` lista todas las dependencias y esta sección separa las pruebas automáticas de las manuales.

## Compatibilidad (0.7.4)

- `RegExp.$1`–`$9`, `lastMatch`, `input`, `leftContext`, `rightContext` y `lastParen` implementados dentro de QuickJS (`third_party/quickjs/quickjs.c`, marcado `RPG2LX`). Los usan casi todos los plugins que leen notetags (Yanfly y otros); sin ellos Fear & Hunger se caía al empezar.
- `require` de `greenworks` (Steam) devuelve un Steam que no está en marcha en vez de fallar.
- Nombres antiguos de PIXI que pixi.js v4 sigue aceptando: `DisplayObjectContainer`, `SpriteBatch`, `AbstractFilter`, `TilingSprite`, `MovieClip`, `BitmapText`.
- El bucle se limita siempre a 60 FPS. En la K36 con ROCKNIX el vsync no limitaba (80 FPS) y los juegos iban un 33 % más rápido.
- Comprobado en PC: Fear & Hunger 1.4.1 llega al título, la selección de personaje, la introducción y la pantalla de nombre.

## Menús y memoria (0.7.3)

- Abrir el menú ya no lee la pantalla de vuelta a la CPU: la captura de fondo que hace MV se queda en la GPU y su desenfoque también se hace allí (reducir a la mitad y volver a ampliar con filtro lineal). Solo se lee a la CPU si el juego toca sus píxeles.
- El fondo y el marco de cada ventana se guardan por windowskin, tamaño y tono, y se copian en vez de dibujarse otra vez desde la skin cada vez que una escena crea sus ventanas.
- El blit escalado y la mezcla de píxeles ya no hacen divisiones por píxel.
- Medido en PC (GPU): abrir el menú del proyecto base pasó de ~44 ms a menos de 20 ms; el primer menú de SAOR DOMINA, de ~170 ms a ~37 ms.
- Las imágenes que el juego solo dibuja como sprites liberan su copia en CPU una vez subidas a la GPU; si luego el juego lee sus píxeles, se decodifican otra vez del archivo.
- Fugas: 150 aperturas de menú en SAOR DOMINA no hacen crecer la memoria (el heap JS se queda en ~20 MB). SAOR DOMINA precarga 142 imágenes (~110 MB decodificados) y las mantiene todas.

## Rendimiento (0.7.2)

- Salto de frames como `SceneManager.updateMain`: si el dispositivo no llega a 60 FPS, cada frame dibujado hace hasta 4 actualizaciones de lógica. El juego mantiene su velocidad real en vez de ir a cámara lenta; solo baja la fluidez del dibujo.
- Vsync adaptativo: mientras el juego va lento el vsync se apaga, para no perder hasta un refresco entero esperando al siguiente vblank en cada frame; se vuelve a activar cuando el juego recupera 60 FPS. Necesita SDL 2.0.18 o posterior en el dispositivo; con uno más antiguo el vsync se queda como estaba.
- La línea `perf:` del log añade `game speed`: el porcentaje de la velocidad real a la que corre el juego.
- Medido en PC (SAOR DOMINA, mapa, caminando, renderizador GPU): ~13 ms de lógica por actualización y ~1 ms de dibujo. Casi todo es JavaScript de sus plugins; las llamadas al motor nativo suman menos de 0,5 ms por frame. Los picos de 1 s del probe solo existen con el renderizador por software de SDL.

## Rendimiento (0.7.1)

- Las imágenes se decodifican en hilos aparte: cargar un mapa o un menú ya no congela el frame.
- La caché de tiles del mapa solo repinta las filas y columnas que entran al hacer scroll, y en cada paso de la animación del agua solo las celdas animadas.
- `RPG2LX.sh` pone el gobernador de CPU en `performance` mientras corre un juego y restaura el anterior al salir.
- `logs/launcher.log` registra cada 10 s una línea `perf:` con fps, ms de juego (lógica + dibujo) por frame y el peor frame.
- Medido en PC con SAOR DOMINA en el mapa: ~8,7 ms de JavaScript por frame, y el ~60 % es código de sus plugins (HUD, colisiones, menú). En una K36 (RK3326, Cortex-A35) eso no llega a 60 fps con un intérprete.

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

## Qué está comprobado

### Pruebas automáticas (`tools/build_native.py --release`, Windows)

- Pruebas unitarias del núcleo (JSON, mapas, inspector, biblioteca, sesión, estado de juego, runtime JS) y pruebas de la CLI.
- Prueba de humo del HUB y del motor MV contra `fixtures/playable_project`, que está en el repositorio.
- Prueba completa contra la plantilla `NewData` de RPG Maker MV (logo, título, mapa, mensaje, menú, guardado, batalla, teclado vía `keyMapper`, píxeles del Canvas 2D). **Solo corre si RPG Maker MV está instalado desde Steam**; si no, se salta y lo avisa.
- No hay CI: estas pruebas solo corren cuando alguien ejecuta el script.

### Pruebas manuales con juegos reales (PC, `tests/mv_probe.cpp`, no automatizadas)

- SAOR DOMINA (21 plugins activos), sin modificar: arranca, título MOG, nueva partida, mapa con su HUD, movimiento, menú.
- Fear & Hunger 1.4.1: título, selección de personaje, introducción, pantalla de nombre.
- `fs` del shim de Node: escritura binaria, base64, `appendFileSync`/`copyFileSync` y rechazo de rutas que salen de `saves/`.

### En la consola (K36, RK3326, ROCKNIX)

- La probó el usuario. Las líneas `perf:` de `logs/launcher.log` dieron, en SAOR DOMINA, entre el 53 % y el 96 % de la velocidad real en los tramos de mapa (antes de 0.7.4 algunos tramos salían por encima del 100 % porque el vsync no limitaba).

## Qué no está comprobado

- Fear & Hunger y los cambios desde 0.7.4 en la consola.
- Controles en la consola más allá del uso normal del usuario.
- En la prueba de Domina aparecen un cuadrado magenta y rectángulos negros sin comparar contra NW.js: pueden ser del juego o del motor.
- Filtros de PIXI (se ignoran), vídeo (`.webm` se salta) y las APIs de PIXI más allá de las que usa MV.
