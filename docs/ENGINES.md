# Motores

Cada juego elige motor en su menú del HUB (**Motor: Auto / Navegador / Nativo**). Se guarda en `config/games/<id>/engine.txt`. Sin elección, el juego usa Auto si `rpg2lx/nwjs/` está instalado, y si no el nativo. Las partidas (`saves/<id>/`) son las mismas con los tres.

- **Auto:** el navegador si Chromium puede dibujar con la GPU de la consola, el nativo si no. Lo decide una prueba que se hace una vez; ver "GPU" más abajo.
- **Navegador:** NW.js siempre. Sin GPU dibuja con la CPU, a 1–3 fps en la K36, y el HUB lo muestra como "Navegador sin GPU".
- **Nativo:** el motor propio de RPG2LX.

## Navegador (NW.js)

El juego corre en NW.js v0.119.2 (Chromium 156 + Node), el mismo tipo de runtime con el que RPG Maker MV exporta a PC. Todos los plugins funcionan como en PC y el JavaScript se compila en vez de interpretarse.

**Cómo arranca** (`portmaster/RPG2LX.sh`, función `run_browser`):

1. `rpg2lx browser-app` (`src/launcher/browser_engine.cpp`) escribe en `config/games/<id>/nwapp/` una mini-app. La carpeta del juego no se toca:
   - `package.json`: ventana kiosk a pantalla completa y opciones de Chromium. Destacan `--allow-file-access-from-files` (MV carga `data/*.json` con XHR), la desactivación de la pausa en segundo plano y el uso de la GPU Mali aunque Chromium no la tenga en su lista.
   - `start.html`: abre el `index.html` del juego.
   - `rpg2lx-browser.js`: se ejecuta antes que los scripts del juego. Ver más abajo qué hace.
2. También escribe `config/games/<id>/browser.gptk` desde el perfil de controles. Las acciones de MV se convierten en su tecla por defecto: [Aceptar]=Enter, [Cancelar]/[Menú]=Esc, [Correr]=Shift…
3. NW.js dibuja directamente en el compositor del firmware: Wayland (sway en ROCKNIX, `--ozone-platform=wayland`) o X11. **No usa Westonpack**: el motor antiguo (0.6) dependía de él y PortMaster lo rechazaba ("Unknown runtime"), así que nunca llegaba a arrancar.
4. Como ROCKNIX ejecuta los ports como root, se añade `--no-sandbox`. Consecuencia: el JavaScript del juego, con el Node de NW.js, corre sin aislamiento y con los permisos del port, como en un PC. Las comprobaciones de rutas del shim `fs` son del motor nativo y aquí no aplican. Solo hay que abrir juegos de confianza.
5. El perfil de Chromium va a `/tmp/rpg2lx-nw/<id>` (tmpfs: la FAT/exFAT de la SD no admite su enlace `SingletonLock`). Como tmpfs es RAM, se borra antes y después de cada juego, y las cachés de disco quedan casi a cero.
6. Si NW.js falla en menos de 8 segundos (falta una librería, no hay pantalla), el juego se abre en el motor nativo. Cerrarlo con Select+Start no cuenta como fallo.

**GPU.** La primera vez que un juego se abre en Auto o Navegador, `browser_gpu_flags` (en `RPG2LX.sh`) abre una página oculta de NW.js con cada ajuste candidato y lee el renderer de WebGL. Gana el primero que da una GPU real; SwiftShader, llvmpipe y softpipe no cuentan. La respuesta se guarda en `config/browser-gpu.txt`: los ajustes o `none`, el entorno (driver de GPU del kernel, versión del kernel, NW.js) y el renderer. Se vuelve a probar sola si cambia el entorno.

- En RK3326, ROCKNIX usa por defecto el driver cerrado **libmali**. Se elige con su ajuste `gpu.driver`; el script `/usr/bin/gpudriver` monta `/usr/lib/mali/*` encima de `libEGL`/`libGLESv2`.
- El proceso de GPU normal de Chromium necesita un nodo render DRM y GBM, que libmali no tiene. Con ellos ANGLE no consigue ninguna configuración EGL ("No configs were generated").
- Por eso el primer candidato es `--in-process-gpu`: dibuja en el proceso principal a través de la ventana de Wayland, como SDL.
- Los siguientes son los ajustes de ANGLE con proceso de GPU, que funcionan con **panfrost** (Mesa).
- Si ninguno funciona con libmali, `browser.log` lo dice. Cambiar ROCKNIX a panfrost hace que la prueba se repita sola.

**Qué hace `rpg2lx-browser.js`:**

- Guarda las partidas en `saves/<id>/`.
- Hace que `document.hasFocus()` responda siempre `true`. Algunos plugins pausan sin foco; SAOR DOMINA se quedaba congelado en el logo por eso.
- Oculta los mandos a la Gamepad API, porque los botones ya llegan por `gptokeyb`.
- Resuelve los `require("./…")` de los plugins desde la carpeta del juego, y sustituye a Steam (`greenworks`) por uno "no disponible".
- `SceneManager.terminate` cierra NW.js y vuelve al HUB.
- Escribe en `logs/browser.log` los errores del juego y una línea `perf:` cada 10 s. Cada línea lleva fps, peor frame, escena, heap de JS, memoria de los procesos de NW.js, memoria libre del sistema y swap.

**Librerías.** NW.js trae las suyas en `nwjs/lib/`. De lo que pide al sistema, a ROCKNIX solo le faltan NSS y NSPR. `tools/fetch_nwjs.py` las toma de los paquetes arm64 de Debian 12, comprobados con el SHA-256 del índice de Debian, y las deja en `nwjs/syslibs/`. Contra el `SYSTEM` real de ROCKNIX 20260809 se comprobaron todas las dependencias de cada binario de NW.js y todas las versiones de glibc (ROCKNIX tiene 2.40): no falta nada.

**Preparar y empaquetar:**

```sh
python tools/fetch_nwjs.py            # build/nwjs-arm64 (~415 MB), con syslibs/
python tools/package_sd.py --output dist/RPG2LX-x.y.z-sd.zip   # incluye nwjs/ (--no-nwjs para omitirlo)
```

**Probar en PC** (NW.js para Windows en `build/nwjs-win`): ejecuta `rpg2lx browser-app` y abre `nw.exe <carpeta nwapp>` con `RPG2LX_BROWSER_LOG`, `RPG2LX_SAVE_DIR` y `RPG2LX_BROWSER_AUTOPILOT=1`. El piloto automático empieza partida nueva, pasa 20 s en el mapa, guarda en la ranura 1 y cierra.

## Nativo (QuickJS + SDL2)

Es el motor propio de RPG2LX: ejecuta los mismos scripts del juego sobre una capa de compatibilidad de navegador y Node. Es más ligero en memoria, pero interpreta el JavaScript y no cubre todo lo que hace un navegador. Ver [STATUS.md](STATUS.md).

**Medir en la consola.** `logs/launcher.log` tiene dos tipos de línea:

- Una línea `perf:` cada 10 s, con:
  - fps reales y velocidad de juego;
  - milisegundos por frame, separados en JS y dibujo;
  - peor frame;
  - memoria del proceso, memoria libre y swap.
- Cada 30 s, si el juego no llega a 60 FPS, el perfil de JavaScript: las funciones donde se va el tiempo, como `nombre (archivo:línea)`. La línea es la de la definición, así que identifica la función del juego o del plugin aunque sea anónima.
