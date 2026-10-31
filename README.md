# RPG2LX

Runtime 1:1 portable para proyectos RPG Maker MV, diseñado para consolas de bolsillo Linux ARM64 (R36S / Rockchip RK3326 y compatibles) y distribución mediante PortMaster.

## Estado (`0.6.0`)

HUB para PortMaster con dos motores por juego:

- **Nativo:** QuickJS + SDL2. Ejecuta los `js/rpg_*.js` y plugins originales del juego. Ligero.
- **Navegador:** NW.js (Chromium real) dentro del runtime Westonpack de PortMaster, para plugins que el nativo no soporta.
- **Auto** (por defecto): nativo; si falla, la próxima vez usa el navegador.

El HUB y cada juego corren en procesos separados: si un juego se cae, se vuelve al HUB. Lo comprobado y lo pendiente está en [STATUS.md](docs/STATUS.md); los motores, en [ENGINES.md](docs/ENGINES.md).

## Controles

Los botones se nombran por posición, porque A/B cambian según la consola.

- **HUB:** cruceta/stick elegir · botón inferior abrir/aceptar · botón derecho volver/salir · Select controles globales · R1 recargar · Start (en el menú del juego) jugar.
- **En el juego:** cada botón emula una tecla y se configura en el HUB (global o por juego). Por defecto: inferior=Enter, derecho=Escape, izquierdo=Shift, superior=X, Start=Escape, Select=Tab, L1/R1=RePag/AvPag, L2=Ctrl, R2=Alt, cruceta/stick=flechas. Las teclas pasan por `Input.keyMapper`, así que se respetan los botones internos de MV y los que añadan los plugins.
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
