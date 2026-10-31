# RPG2LX

Runtime 1:1 portable para proyectos RPG Maker MV, diseñado para consolas de bolsillo Linux ARM64 (R36S / Rockchip RK3326 y compatibles) y distribución mediante PortMaster.

## Estado (`0.7.0`)

HUB para PortMaster con un motor nativo (QuickJS + SDL2) que ejecuta los `js/rpg_*.js` y los plugins originales del juego, sin modificarlos, como un juego exportado (sin modo de prueba). Incluye un Canvas 2D completo, DOM, APIs de navegador y módulos de Node/NW.js para los plugins. El HUB y cada juego corren en procesos separados. Detalle, lo comprobado y lo pendiente en [STATUS.md](docs/STATUS.md).

## Controles

Los botones se nombran por posición, porque A/B cambian según la consola.

- **HUB:** cruceta/stick elegir · botón inferior abrir/aceptar · botón derecho volver/salir · Select controles globales · R1 recargar · Start (en el menú del juego) jugar.
- **En el juego:** cada botón envía una acción de MV o una tecla, configurable en el HUB (global o por juego, antes de entrar). Por defecto: inferior=[Aceptar], derecho=[Cancelar], izquierdo=[Correr], superior y Start=[Menú], Select=[Tab], L1/R1=[Pág. anterior/siguiente], L2=[Control], cruceta/stick=direcciones. Una acción pulsa la tecla que el propio juego asigna, así que funciona aunque un plugin cambie el teclado.
- **Select + Start:** cerrar el juego y volver al HUB.

## Uso

Copiá cada juego exportado de RPG Maker MV dentro de `games/Nombre del juego/` (reconoce tanto raíz directa como subcarpeta `www/`, así como recursos cifrados `.rpgmvp` / `.rpgmvo`). RPG2LX crea `games`, `saves`, `config` y `logs` al arrancar.

```text
rpg2lx --version
rpg2lx inspect /ruta/al/juego
rpg2lx inspect-map /ruta/al/juego/data/Map001.json
rpg2lx launcher --home /ruta/a/rpg2lx
rpg2lx list --home /ruta/a/rpg2lx
rpg2lx run /ruta/al/proyecto/MV --home /ruta/a/rpg2lx
```

## Documentación

- [PROJECT_BRIEF.md](docs/PROJECT_BRIEF.md): objetivos y prioridades del proyecto.
- [MV_COMPATIBILITY.md](docs/MV_COMPATIBILITY.md): matriz actualizada de compatibilidad `0.5.0`.
- [BUILDING.md](docs/BUILDING.md): compilación nativa y cross-compilación ARM64 con Zig/CMake.
- [LAUNCHER.md](docs/LAUNCHER.md): biblioteca `games/`, menú y controles.
- [SD_PACKAGE.md](docs/SD_PACKAGE.md): empaquetado PortMaster ARM64 para tarjeta SD.
- [COORDINATION.md](COORDINATION.md): registro técnico de arquitectura, pruebas y entregas.

Repositorio: [Bodou2004/RPG2LX](https://github.com/Bodou2004/RPG2LX). Juegos completos, recursos comerciales de RPG Maker MV, partidas guardadas, registros y toolchains locales están excluidos del control de versiones.
