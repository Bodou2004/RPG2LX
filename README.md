# RPG2LX

Runtime 1:1 portable para proyectos RPG Maker MV, diseñado para consolas de bolsillo Linux ARM64 (R36S / Rockchip RK3326 y compatibles) y distribución mediante PortMaster.

## Estado (`0.5.0`)

`0.5.0`: motor 1:1 de RPG Maker MV acelerado por GPU (SDL2 + QuickJS + `stb_truetype` + `stb_vorbis`). Ejecuta directamente los scripts originales sin modificar del proyecto (`js/rpg_core.js`, `js/rpg_managers.js`, `js/rpg_objects.js`, `js/rpg_scenes.js`, `js/rpg_sprites.js`, `js/rpg_windows.js` y `js/plugins/*.js` como `Community_Basic` y `CustomLogo` / `MadeWithMv`) sobre un grafo de escena acelerado en C++/SDL2 a **60 FPS** estables.

Incluye:
- **Pantallas de presentación y título originales:** ejecución real de plugins de splash (`Scene_Logo` / `MadeWithMv.png` con fade-in/fade-out) y `Scene_Title` idéntica a escritorio (`titles1`, `titles2`, título contorneado y animación de `Window_TitleCommand` con BGM).
- **Tipografía TrueType adaptada a pantallas portátiles (640×480):** escalado `em` fiel a HTML5 Canvas, refuerzo de legibilidad para pantallas de 3.5", contorno circular nítido, compresión horizontal `maxWidth`, decodificación UTF-8 completa (`¡¿áéíóúÁÉÍÓÚñÑ`) y presentación lineal con relación de aspecto preservada.
- **Estructura, escenas, funciones y animaciones 1:1 de MV:** `Scene_Boot`, `Scene_Title`, `Scene_Map`, `Scene_Menu`, `Scene_Item`, `Scene_Skill`, `Scene_Equip`, `Scene_Status`, `Scene_Options`, `Scene_Save`, `Scene_Load`, `Scene_GameEnd`, `Scene_Shop`, `Scene_Name`, `Scene_Battle` y `Scene_Gameover`; animaciones de apertura/cierre de ventanas (`openness`), pulso de cursor, flecha de pausa, máquina de escribir con códigos de escape (`\C[n]`, `\I[n]`, `\N[n]`, `\V[n]`), `Animations.json`, globos de expresión, seguidores, clima, tintes/flashes/sacudidas y desenfoque `Bitmap.snap` de fondo de menú.
- **Audio polifónico OGG / `.rpgmvo`:** mezclador SDL2 + `stb_vorbis` con soporte de metadatos `LOOPSTART` y `LOOPLENGTH`, `volume`, `pitch`, `pan`, `fadeIn` y `fadeOut` para BGM, BGS, ME y SE.
- **Guardado estándar `.rpgsave` aislado:** compresión `LZString` compatible con RPG Maker MV almacenada en `saves/<juego>/` sin alterar jamás la carpeta original del proyecto.

## Controles (PortMaster / Consola / Teclado)

- **Biblioteca RPG2LX:**
  - `B` (Sur) / `A` / `Enter` / `Z`: Iniciar juego directamente
  - `R` / `F5`: Actualizar biblioteca
  - `Start` / `Esc`: Salir
- **Dentro del juego (RPG Maker MV):**
  - `B` (Sur en consola Nintendo/R36S) / `Enter` / `Z` / `Espacio`: Confirmar / Interactuar (`ok`)
  - `A` (Este) / `X` (Norte) / `Start` / `Esc`: Cancelar / Abrir y cerrar Menú (`escape` / `menu` / `cancel`)
  - `Y` (Oeste) / `Shift`: Correr (`shift`)
  - `L1` / `R1` (`Q` / `W`): Cambiar personaje en menús (`pageup` / `pagedown`)
  - `Select + Start`: Cerrar partida y volver a la biblioteca RPG2LX

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
