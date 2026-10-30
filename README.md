# RPG2LX

Runtime portable para proyectos RPG Maker MV, con Linux en consolas de bolsillo como primer destino y distribución mediante PortMaster.

## Estado

0.3.1-alpha: biblioteca SDL2 con preview de título, escenas de título/menú, seguidores, objetos/habilidades/equipo básicos, formación, guardado/carga propios y eventos ampliados. Project1 tiene un recorrido de 95 pasos comprobado con sus eventos y menús reales; ver docs/TEST_PROJECT.md. La compatibilidad sigue siendo parcial: faltan batalla, audio, JavaScript/plugins y numerosos sistemas MV. La versión anterior arrancó en consola según el usuario; esta actualización requiere nueva prueba física.
La compatibilidad universal es una meta, no una capacidad demostrada.

## Documentación

- `PERPLO_MV_MASTER_RUNTIME_ANTIGRAVITY.md`: especificación original, conservada como referencia.
- `docs/PROJECT_BRIEF.md`: nombre actual, prioridades y primer encargo.
- `COORDINATION.md`: conversación y traspasos entre Codex y Antigravity.
- `docs/BUILDING.md`: compilación y pruebas del código C++.
- `docs/MAP_DATA.md`: módulo de mapas implementado por Codex.
- `docs/LAUNCHER.md`: biblioteca games, menú y controles.
- `docs/NATIVE_EXPLORATION.md`: archivos MV originales y alcance real del motor nativo.
- `docs/SD_PACKAGE.md`: paquete ARM64 de PortMaster preparado para copiar a la SD.
- `docs/TEST_PROJECT.md`: ubicación de las pruebas de Project1 y recorrido automatizado.
- [examples/Project1](examples/Project1/): los siete JSON del recorrido, sin recursos comerciales; instrucciones para aplicarlos sobre un proyecto MV propio.

## Uso actual

**Actualización para consola:** `dist/RPG2LX-0.3.1-PortMaster-update.zip` (runtime sin juegos), sobre la instalación inicial que contiene Project1. Los datos del recorrido se instalan por separado con tools/sync_game_data.py; ya se actualizaron en H:/ports. Es un port ARM64 estándar, sin acoplamiento a un firmware y sin HUD de diagnóstico durante la partida. Los saves propios quedan fuera de los assets y no son intercambiables con .rpgsave.

X inicia el juego desde la biblioteca. En juego, A confirma/interactúa; B/Start abre o cancela el menú, Y corre. Para volver a biblioteca: menú → Salir → Biblioteca RPG2LX.

Copiá cada juego completo dentro de `games/Nombre del juego/`. RPG2LX crea `games`, `saves`, `config` y `logs` al arrancar. También reconoce proyectos dentro de `games/Nombre del juego/www/`.

Para abrir un proyecto exportado por RPG Maker MV, copiá su carpeta completa a `games/` y elegilo en `RPG2LX.cmd`. También podés arrastrar su carpeta original, `index.html`, `Game.exe` o archivo `.rpgproject` sobre `RPG2LX.cmd`: el runtime resuelve los datos y recursos sin modificar el proyecto ni ejecutar el EXE.

La primera validación con un proyecto real se realizó con **Project1**, proporcionado por el usuario: Map001, terreno, actor inicial y seis eventos cargados desde sus archivos originales; movimiento y colisión de NPC comprobados. `Community_Basic` y `CustomLogo` se detectan pero todavía no se ejecutan. Los datos sintéticos del repositorio quedan como pruebas internas, no como el juego a entregar.

```text
rpg2lx --version
rpg2lx inspect /ruta/al/juego
rpg2lx inspect-map /ruta/al/juego/data/Map001.json
rpg2lx launcher --home /ruta/a/rpg2lx
rpg2lx list --home /ruta/a/rpg2lx
rpg2lx run /ruta/al/proyecto/MV --home /ruta/a/rpg2lx
```

Codex implementa núcleo, arquitectura, escenas, integración y pruebas; Antigravity programó la nueva vista de biblioteca con imágenes de título en paralelo. El intercambio queda en COORDINATION.md.

Repositorio: [Bodou2004/RPG2LX](https://github.com/Bodou2004/RPG2LX). Los paquetes de consola se adjuntan en [Releases](https://github.com/Bodou2004/RPG2LX/releases); el historial Git conserva código, documentación, fixtures propias y datos del recorrido. Juegos completos, recursos comerciales, partidas, registros y herramientas de compilación locales quedan fuera del repositorio.
