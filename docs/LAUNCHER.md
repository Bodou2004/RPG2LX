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

El título viene de `System.json`; la identidad para partidas y configuración es el nombre de la carpeta. Dos juegos con el mismo título mantienen directorios separados. Si se cambia el nombre de una carpeta, también hay que renombrar su carpeta correspondiente de saves/config para conservar esa asociación. RPG2LX no escribe partidas en los assets. El motor nativo guarda `.rpgsave` estándar de MV en saves/<carpeta>/.

La biblioteca creada por Antigravity compone title1Name/title2Name desde img/titles1 e img/titles2 como vista previa. Si falta una imagen muestra una tarjeta alternativa sin interrumpir la biblioteca.

## HUB y controles

En PortMaster, `RPG2LX.sh` ejecuta `rpg2lx hub --home DIR --request FILE`. Al elegir Jugar, el HUB
escribe el juego y el motor en `config/launch.request`, sale con código 10, el script lanza el juego
(`rpg2lx run ... --game-id` o NW.js) y vuelve a abrir el HUB. Fuera de PortMaster,
`rpg2lx launcher` sigue ejecutando el motor nativo dentro del mismo proceso.

- Biblioteca: cruceta/stick elegir, botón inferior abrir menú del juego, botón derecho salir,
  Select controles globales, R1 recargar.
- Menú del juego: Jugar, Motor (Auto/Nativo/Navegador), Controles de este juego, Usar controles globales.
- Editor de controles: una fila por botón; izquierda/derecha cambia la tecla, pulsar un botón salta a
  su fila, botón derecho guarda y vuelve. La navegación del HUB es fija para que un mapeo erróneo no
  impida salir.

Teclado en PC: flechas, Enter/Z aceptar, Esc/X volver, Tab/C controles, R recargar, P jugar.

## Estado actual

Ver [STATUS.md](STATUS.md) y [ENGINES.md](ENGINES.md).

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
