# RPG2LX — integración con PortMaster

Investigación inicial: Antigravity. Actualización verificada por Codex: 2026-10-30.

## Fuentes oficiales

- [Packaging](https://portmaster.games/packaging.html): estructura del port, ejemplo de JSON versión 4, licencias y script de arranque. Secciones New Port Structure y The Launchscript.
- [Generador de port.json](https://portmaster.games/port-json.html): arquitectura, estado experimental y requisitos. Es un generador, no una especificación formal completa del esquema.
- [gptokeyb](https://github.com/PortsMaster/gptokeyb): referencia para aplicaciones que requieran traducción de mando a teclado. El menú RPG2LX usa SDL2 GameController directamente.

La dirección port-scripting.html citada en la investigación inicial no pudo verificarse; la referencia de scripting comprobada es la sección de launchscript de packaging.html.

## Estructura prevista

```text
rpg2lx/                 (fuentes de empaquetado, no instalación)
├── port.json
├── README.md           (pendiente)
├── screenshot.png      (pendiente validación en consola)
├── gameinfo.xml        (pendiente)
├── RPG2LX.sh
└── rpg2lx/             (directorio instalado bajo ports/)
    ├── rpg2lx.aarch64  (pendiente compilación ARM)
    ├── games/
    │   └── Mi juego/data/System.json
    ├── saves/<carpeta-del-juego>/
    ├── config/games/<carpeta-del-juego>/
    ├── logs/
    └── licenses/       (a preparar al distribuir dependencias)
```

Cada juego tiene una carpeta inmediata en games; también se detecta games/Mi juego/www/data/System.json. No se usa un gamedata único y no se necesita un script de PortMaster por juego.

## Arranque implementado

1. Localizar y cargar control.txt y mods del firmware.
2. Obtener el mapping SDL con get_controls.
3. Crear games, saves, config y logs.
4. Configurar SDL_GAMECONTROLLERCONFIG y las rutas de bibliotecas.
5. Comprobar el binario de la arquitectura y llamar al helper de plataforma.
6. Ejecutar `"$BIN" launcher --home "$GAMEDIR"`.
7. Preservar el código de salida y ejecutar pm_finish mediante trap EXIT.

El script escribe logs/portmaster.log; la aplicación escribe logs/launcher.log. XDG_DATA_HOME apunta a config; RPG2LX usa explícitamente su directorio saves para separar partidas de los assets. No hay todavía serialización de partidas.

No se lanza gptokeyb para el menú: duplicaría las entradas que SDL ya recibe. El archivo .gptk existente queda como referencia histórica, sin invocación automática.

## Metadatos

Plantilla port.json v4 con arquitectura aarch64, experimental activo y ready-to-run falso. La descripción advierte que el menú y la inspección existen, pero la ejecución de juegos aún no está implementada.

No se establece reqs=opengl: el generador oficial define ese requisito como OpenGL de escritorio, no OpenGL ES. Este menú usa SDL2 y permite renderer software. Los futuros backends GLES deberán validarse contra el firmware y no confundirse con ese requisito.

## Límites de validación

El menú se compila y prueba en Windows x86_64; script comprobado con bash -n. Esto no prueba control.txt, pm_finish, hardware, ABI ARM ni disponibilidad de SDL2 en el dispositivo. Faltan build aarch64 con sysroot compatible y prueba R36S/firmware; no hay paquete publicado o listo para el catálogo.

Para compilar el menú incluir `-DRPG2LX_WITH_SDL=ON` y SDL2 del target. Las bibliotecas Windows del entorno de desarrollo no deben copiarse al paquete Linux.
