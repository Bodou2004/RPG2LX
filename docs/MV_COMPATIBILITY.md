# Compatibilidad MV — RPG2LX `0.5.0`

Actualizado 2026-10-30. Motor 1:1 de RPG Maker MV: ejecuta directamente los scripts oficiales sin modificar del proyecto (`rpg_core.js`, `rpg_managers.js`, `rpg_objects.js`, `rpg_scenes.js`, `rpg_sprites.js`, `rpg_windows.js` y `js/plugins/*.js`) sobre QuickJS con renderizado de escena acelerado por GPU en C++/SDL2 y audio polifónico `stb_vorbis`.

| Área | Implementado (`0.5.0`) | Notas / Alcance |
|---|---|---|
| **Núcleo y Scripts MV** | Ejecución 1:1 de `rpg_*.js`, `plugins.js` y `$plugins` sobre QuickJS con puente nativo C++ | Plugins que dependan de shaders WebGL personalizados o APIs de Chromium/DOM ajenas a PIXI v4 de MV pueden requerir adaptación |
| **Presentación y Título** | Plugins de splash (`CustomLogo.js` / `Scene_Logo` / `MadeWithMv.png`) y `Scene_Title` completa (`titles1`, `titles2`, título 72px contorneado, `Window_TitleCommand` animada y BGM) | 1:1 con proyectos base de RPG Maker MV |
| **Tipografía y Pantallas 640×480** | `GameFont` TrueType (`fonts/*.ttf`), métrica `em` exacta, escala de legibilidad portátil (+10%), contorno circular, `maxWidth`, UTF-8 completo (`¡¿áéíóúñÑ`) y escalado lineal con aspecto preservado | Mantiene pixel art interno nítido (`nearest`) sin romper trazos de letras al reducir `816×624` a `640×480` |
| **Mapas (`Tilemap` GPU)** | Autotiles A1–A4 animados, A5, B–E, sombras, bordes de mesas, paso superior (`☆`), scroll sub-tile, loop horizontal/vertical, `TilingSprite` parallax y caché en texturas GPU | Omite capas superiores vacías y bitmaps transparentes para sostener 60 FPS |
| **Personajes y Eventos** | Intérprete oficial `Game_Interpreter` completo (todos los comandos estándar MV, eventos paralelos, rutas de movimiento, vehículos, eventos comunes, scripts `355/655`) | 1:1 mediante `rpg_objects.js` |
| **Ventanas y Mensajes** | `WindowLayer` con máscara de superposición, `Window.openness`, pulso de cursor, flecha de pausa, máquina de escribir y todos los escape codes (`\C[n]`, `\I[n]`, `\N[n]`, `\V[n]`, `\.`, `\|`, `\$`) | 1:1 mediante `rpg_windows.js` |
| **Menús y Escenas** | `Scene_Menu`, `Scene_Item`, `Scene_Skill`, `Scene_Equip`, `Scene_Status`, `Scene_Options`, `Scene_Save`, `Scene_Load`, `Scene_GameEnd`, `Scene_Shop`, `Scene_Name`, `Scene_Gameover` + `Bitmap.snap` con `blur()` | 1:1 mediante `rpg_scenes.js` |
| **Batallas y Animaciones** | `Scene_Battle` (vista frontal y lateral SV), `BattleManager`, fórmulas JS reales, `Animations.json` (`Sprite_Animation` con rotación/escala/flash/SE), globos (`Sprite_Balloon`), clima (`Weather`), tintes y sacudidas | 1:1 mediante `rpg_sprites.js` + `rpg_managers.js` |
| **Audio** | BGM, BGS, ME y SE en `.ogg` y `.rpgmvo` con `LOOPSTART`/`LOOPLENGTH`, `volume`, `pitch`, `pan`, `fadeIn`, `fadeOut` y `seek` | Vídeo `.webm`/`.mp4` se omite de forma segura sin bloquear la escena |
| **Recursos Cifrados** | Descifrado transparente en memoria de imágenes `.rpgmvp` y audios `.rpgmvo` usando `encryptionKey` de `System.json` | Sin modificar archivos en disco |
| **Guardado (`.rpgsave`)** | `StorageManager` + `LZString` estándar (`global.rpgsave`, `config.rpgsave`, `file1..20.rpgsave`) aislado en `saves/<juego>/` | Nunca escribe dentro de la carpeta del proyecto original |
| **PortMaster ARM64** | Binario `rpg2lx.aarch64` (`GLIBC_2.17`+, SDL2 dinámico del firmware, 60 FPS estables sin doble espera VSync+`SDL_Delay`) | Controles mapeados a consola portátil (`B` confirma, `A`/`X`/`Start` menú/cancelar, `Y` correr, `Select+Start` volver a biblioteca) |
