# Biblioteca y menú de RPG2LX

Al abrir RPG2LX sin argumentos se abre su menú gráfico. El programa crea, junto al ejecutable, esta estructura:

```text
rpg2lx/
├── games/
│   ├── Mi juego/
│   │   ├── data/
│   │   ├── img/
│   │   ├── audio/
│   │   └── js/
│   └── Otro juego/
│       └── www/
│           ├── data/
│           ├── img/
│           └── ...
├── saves/<carpeta-del-juego>/
├── config/games/<carpeta-del-juego>/
└── logs/launcher.log
```

Copiar cada juego completo dentro de una carpeta inmediata de `games/`. Se reconoce la estructura directa y la estructura con `www/`. No hace falta crear un port separado por juego. Las carpetas ocultas, enlaces simbólicos de la biblioteca y archivos sueltos no aparecen en la lista. Una carpeta incompleta aparece con aviso.

El título viene de `System.json`; la identidad para partidas y configuración es el nombre de la carpeta. Dos juegos con el mismo título mantienen directorios separados. Si se cambia el nombre de una carpeta, también hay que renombrar su carpeta correspondiente de saves/config para conservar esa asociación. RPG2LX no escribe partidas en los assets. La versión 0.3 guarda 20 slots JSON propios en saves/<carpeta>/, con respaldo al reemplazar. No son .rpgsave de MV.

La biblioteca creada por Antigravity compone title1Name/title2Name desde img/titles1 e img/titles2 como vista previa. Si falta una imagen muestra una tarjeta alternativa sin interrumpir la biblioteca.

## Controles

- D-pad o stick izquierdo: elegir juego.
- A / Enter / Z: ver detalles; dentro de detalles, comprobar los datos.
- B / Escape / X: volver; desde la lista, salir.
- X / P desde lista o detalles: abrir el título del juego. En mapa, A/Enter/Z/espacio interactúa; B/Start/X/Escape abre o cancela el menú; Y/Shift corre. Volver a biblioteca exige Menú → Salir → Biblioteca RPG2LX. No se destruye la sesión al cancelar.
- Y / R: actualizar la biblioteca sin reiniciar. La selección se conserva por carpeta cuando sigue disponible.

La biblioteca usa resolución lógica 640x480 y SDL2 GameController; el juego usa las dimensiones detectadas, 816x624 por defecto. El bitmap de texto incluye ASCII y Latin-1 (tildes y ñ); otros caracteres se muestran como `?` por ahora. GameFont sigue pendiente. Se rechazan proyectos identificados explícitamente como MZ; detectar MV no prueba compatibilidad de plugins.

## Estado actual

La biblioteca ofrece lista paginada, preview, detalles y comprobación. X/P abre el título original dentro del soporte nativo. Hay seguidores, menús del juego y persistencia propia; ver NATIVE_EXPLORATION.md y MV_COMPATIBILITY.md. No se ejecuta JavaScript: batalla, audio, plugins y muchos sistemas siguen pendientes.

## Compilación y diagnóstico

La build de menú necesita SDL2. La build de diagnóstico sin SDL conserva inspect/list y responde con un error explícito al intentar abrir el menú.

```sh
cmake -S . -B build/menu -DRPG2LX_WITH_SDL=ON
cmake --build build/menu
ctest --test-dir build/menu --output-on-failure
```

Si SDL2 está en un directorio local, pasar `-DSDL2_DIR=/ruta/al/lib/cmake/SDL2`. En Windows se copia SDL2.dll junto al ejecutable mediante CMake; en la consola hay que usar SDL2 del firmware/sysroot compatible. No mezclar la DLL Windows con el paquete ARM.

```text
rpg2lx launcher --home /ruta/a/rpg2lx
rpg2lx list --home /ruta/a/rpg2lx
```

El directorio elegido con `--home` no depende del directorio desde el que se invoca el programa. Sin `--home`, se usa la ubicación del ejecutable, no la carpeta de trabajo del shell. En Linux se resuelve `/proc/self/exe` y en Windows la ruta del módulo.

Para usar una exportación original desde otra ruta sin copiar ni convertir sus archivos:

```text
rpg2lx run /ruta/al/proyecto/MV --home /ruta/a/rpg2lx
```

Se acepta la carpeta de proyecto/exportación, su `index.html`, un `.rpgproject` o `Game.exe` (solo como referencia de ruta, nunca se ejecuta el EXE). Si los datos están bajo `www/`, se resuelven allí. El inspector usa la misma resolución de rutas y rechaza proyectos identificados explícitamente como MZ.

En esta carpeta de desarrollo Windows, `RPG2LX.cmd` abre la build SDL2 Release (o Debug si no hay Release) usando la raíz del proyecto como home. Así utiliza la carpeta games de la raíz sin copiar el ejecutable. No forma parte del paquete Linux.
Arrastrar una carpeta MV sobre ese archivo abre directamente el proyecto original. Los logs se guardan bajo el home de RPG2LX, no dentro del juego.

Prueba de renderizado reproducible sin abrir ventana:

```sh
SDL_VIDEODRIVER=dummy rpg2lx launcher --home /tmp/rpg2lx-test --smoke-frames 2 --screenshot /tmp/menu.bmp
```

La prueba no simula soporte de hardware: controles reales, escalado en consola y lanzamiento desde PortMaster requieren prueba R36S.
