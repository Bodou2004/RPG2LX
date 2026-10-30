# PERPLO MV MASTER RUNTIME
## Especificación técnica y directrices de desarrollo para Antigravity

**Estado:** propuesta inicial / arquitectura maestra  
**Objetivo principal:** crear un runtime independiente, portable y modular capaz de ejecutar proyectos compatibles con RPG Maker MV sin depender de NW.js/Chromium como requisito obligatorio.  
**Primer target prioritario:** Linux ARM64 / RK3326 / dispositivos tipo R36S ejecutando AURKNIX/ROCKNIX u otras distribuciones Linux compatibles.  
**Objetivo posterior:** reutilizar el mismo núcleo para Windows, Linux x86_64 y otros targets mediante backends.

---

# 1. MISIÓN DEL PROYECTO

Construir un **runtime maestro compatible con RPG Maker MV**.

El proyecto NO debe ser un port de un juego individual.

Debe ser una plataforma capaz de recibir una carpeta de proyecto compatible con RPG Maker MV y ejecutar la mayor cantidad posible de su contenido mediante un runtime propio.

La arquitectura debe separar:

1. Compatibilidad con datos de RPG Maker MV.
2. Lógica del runtime.
3. Máquina JavaScript y compatibilidad con plugins.
4. Renderizado.
5. Audio.
6. Input.
7. Sistema de archivos.
8. Persistencia.
9. Backend específico de cada plataforma.

Arquitectura conceptual:

```text
Proyecto RPG Maker MV
        |
        v
+---------------------------+
| PERPLO MV MASTER RUNTIME  |
+---------------------------+
| MV Data Compatibility     |
| Event Interpreter         |
| Game Object Model         |
| JavaScript VM             |
| Plugin Compatibility API  |
| Renderer API              |
| Audio API                 |
| Input API                 |
| Save API                  |
+---------------------------+
             |
             v
+---------------------------+
| Platform Abstraction      |
+---------------------------+
     |        |        |
     v        v        v
   SDL2     Linux    Windows
     |
     v
 RK3326 / R36S
```

El núcleo NO debe contener dependencias específicas de R36S.

R36S será solamente el primer backend/plataforma de validación.

---

# 2. PRINCIPIO FUNDAMENTAL

No intentar portar NW.js completo.

RPG Maker MV utiliza una combinación de:

- HTML5
- JavaScript
- Canvas/WebGL
- PixiJS
- WebAudio
- DOM
- NW.js para builds de escritorio

Reproducir Chromium/NW.js completo sería innecesariamente pesado para un RK3326.

El objetivo debe ser reproducir únicamente las interfaces y comportamientos que RPG Maker MV necesita.

La implementación ideal será híbrida:

```text
C / C++
    |
    +-- plataforma
    +-- filesystem
    +-- render
    +-- audio
    +-- input
    +-- timing
    +-- memoria
    |
    +-- JavaScript VM
            |
            +-- API compatible MV
            +-- scripts del juego
            +-- plugins
```

---

# 3. TECNOLOGÍAS RECOMENDADAS

## Lenguaje principal

Preferencia:

```text
C++17
```

Motivos:

- buen soporte ARM64;
- integración directa con SDL;
- rendimiento;
- abstracciones claras;
- amplia disponibilidad de librerías;
- compilación cruzada;
- adecuado para futuros targets.

C puro puede utilizarse en módulos de bajo nivel cuando sea conveniente.

---

# 4. BACKEND INICIAL

Usar SDL2 inicialmente para:

- ventana;
- framebuffer;
- input;
- gamepads;
- audio básico;
- timing;
- lifecycle.

Renderer inicial:

```text
SDL2
  +
OpenGL ES 2.0/3.x
```

No asumir OpenGL de escritorio.

El renderer debe diseñarse mediante interfaz.

Ejemplo:

```cpp
class IRenderer {
public:
    virtual bool initialize() = 0;
    virtual void beginFrame() = 0;
    virtual void drawSprite(...) = 0;
    virtual void drawTilemap(...) = 0;
    virtual void drawText(...) = 0;
    virtual void endFrame() = 0;
};
```

Backends futuros:

```text
RendererSDL
RendererGLES
RendererOpenGL
RendererPSX
RendererVulkan
```

---

# 5. JAVASCRIPT

La compatibilidad con JavaScript es uno de los puntos más importantes.

NO reimplementar inicialmente todos los plugins en C++.

Debe existir una VM JavaScript embebida.

Candidato inicial recomendado:

```text
QuickJS
```

Razones:

- pequeño;
- embebible;
- portable;
- adecuado para ARM;
- permite exponer funciones C/C++ a JavaScript;
- no requiere Chromium.

La VM debe estar encapsulada.

```text
src/js/
    JavaScriptVM.hpp
    JavaScriptVM.cpp
    JSBindings.cpp
    MVBindings.cpp
```

Nunca permitir que el resto del runtime dependa directamente de QuickJS.

Crear una abstracción:

```cpp
class IJavaScriptRuntime;
```

Así QuickJS podría sustituirse en el futuro.

---

# 6. DOS MODOS DE COMPATIBILIDAD

## Modo A — Runtime nativo

Reimplementar en C++ las funciones fundamentales:

- DataManager;
- SceneManager;
- Game_Map;
- Game_Player;
- Game_Event;
- Game_Interpreter;
- Game_Actor;
- Game_Party;
- Game_System;
- Game_Switches;
- Game_Variables;
- Game_SelfSwitches;
- Game_Message;
- BattleManager;
- input;
- audio;
- sprites;
- ventanas.

Ventaja: máximo rendimiento.

## Modo B — Compatibility JavaScript Layer

Exponer objetos compatibles dentro de la VM:

```javascript
Graphics
Input
TouchInput
AudioManager
ImageManager
DataManager
SceneManager
PluginManager
```

Esto permitirá ejecutar plugins JavaScript.

No afirmar inicialmente que todos los plugins MV funcionan. Debe existir una matriz real de compatibilidad.

---

# 7. FORMATO DEL PROYECTO

Aceptar inicialmente una estructura tradicional:

```text
Game/
├── data/
├── img/
├── audio/
├── movies/
├── fonts/
├── js/
│   ├── plugins/
│   └── plugins.js
└── package.json
```

No exigir modificar manualmente cada juego.

Uso conceptual:

```bash
perplo-mv-runtime /ruta/al/proyecto
```

---

# 8. CARGA DE DATA

Implementar parser JSON para:

```text
Actors.json
Animations.json
Armors.json
Classes.json
CommonEvents.json
Enemies.json
Items.json
MapInfos.json
MapXXX.json
Skills.json
States.json
System.json
Tilesets.json
Troops.json
Weapons.json
```

Crear estructuras internas claramente separadas del JSON original.

```text
JSON MV
  |
  v
MVDataLoader
  |
  v
Runtime Database
```

Nunca consultar JSON crudo constantemente durante cada frame.

---

# 9. MAPAS

Implementar:

- lectura de MapXXX.json;
- dimensiones;
- tiles;
- capas;
- tilesets;
- regiones;
- passability;
- eventos;
- prioridades;
- scroll;
- parallax;
- loops;
- encounter data.

Separar:

```text
MapData
MapRuntime
MapRenderer
MapCollision
```

---

# 10. EVENTOS

Crear un `MVEventInterpreter`.

Debe ejecutar progresivamente los event commands de RPG Maker MV.

```text
Event page
   |
   v
Event command list
   |
   v
MVEventInterpreter
   |
   +-- message
   +-- switch
   +-- variable
   +-- transfer
   +-- movement
   +-- conditional
   +-- loop
   +-- common event
   +-- script
   +-- plugin command
```

Cada command debe registrarse por ID.

```cpp
registerCommand(101, commandShowText);
registerCommand(102, commandShowChoices);
registerCommand(111, commandConditionalBranch);
```

Evitar un `switch` gigantesco si puede diseñarse un registro limpio.

---

# 11. PLAYER Y MOVIMIENTO

Primera implementación:

- movimiento de 4 direcciones;
- grid estándar;
- dash;
- encounters;
- transferencias;
- followers básicos;
- vehículos posteriormente.

Input abstracto:

```text
UP
DOWN
LEFT
RIGHT
OK
CANCEL
MENU
PAGEUP
PAGEDOWN
```

El runtime no debe depender de teclas específicas.

---

# 12. RENDERIZADO 2D

Objetivos iniciales:

- PNG;
- spritesheets MV;
- characters;
- faces;
- pictures;
- parallaxes;
- tilesets;
- animations;
- windowskin;
- text rendering;
- viewport;
- opacity;
- blend;
- tone;
- tint;
- scaling;
- rotation.

Evitar:

- crear/destruir texturas continuamente;
- decodificar imágenes durante cada frame;
- una draw call por tile cuando pueda agruparse.

---

# 13. RESOLUCIÓN

No hardcodear una sola resolución.

Mantener:

```text
logical resolution
physical resolution
```

Ejemplo:

```text
Juego:        816x624
Pantalla:     640x480
```

Modos futuros:

```text
integer
fit
stretch
crop
native
```

---

# 14. AUDIO

Implementar:

- BGM;
- BGS;
- ME;
- SE;
- volumen;
- pitch;
- pan;
- fade;
- loop.

Formatos prioritarios:

```text
OGG
WAV
```

No depender de WebAudio.

Crear:

```text
IAudioBackend
```

---

# 15. INPUT R36S

Backend SDL GameController / Joystick.

Mapeo inicial sugerido:

```text
D-Pad     -> movimiento
A         -> OK
B         -> Cancel
X         -> acción secundaria
Y         -> acción secundaria
Start     -> Menu
Select    -> función sistema
L1 / R1   -> PageUp / PageDown
```

El mapping debe ser configurable.

```text
config/input.json
```

---

# 16. SISTEMA DE PLUGINS

Crear:

```text
PluginManager
PluginLoader
PluginParameters
PluginCommands
JSCompatibility
```

Leer `js/plugins.js`.

Cargar los plugins activos en orden y mantener sus parámetros.

Un plugin defectuoso NO debe producir un crash silencioso.

Ejemplo:

```text
[PLUGIN] YEP_CoreEngine.js ........ OK
[PLUGIN] Example.js ............... PARTIAL
[PLUGIN] UnsupportedPlugin.js ..... ERROR
```

---

# 17. NIVELES DE COMPATIBILIDAD

## LEVEL 0 — DATA
El proyecto puede abrirse y analizarse.

## LEVEL 1 — BOOT
Carga base de datos y llega al title.

## LEVEL 2 — VANILLA MAP
Mapas, eventos básicos, movimiento, mensajes.

## LEVEL 3 — VANILLA COMPLETE
Menús, batalla, guardado y funcionalidad estándar MV.

## LEVEL 4 — COMMON PLUGINS
Plugins comunes mediante compatibility layer.

## LEVEL 5 — ADVANCED PLUGINS
Plugins complejos que alteran profundamente escenas, PIXI, render o filesystem.

## LEVEL 6 — PROJECT SPECIFIC
Extensiones específicas como PERPLO/DOMINA.

Nunca declarar “MV compatible” sin indicar el nivel.

---

# 18. COMPATIBILIDAD PIXI

No implementar PixiJS completo desde el primer día.

Crear:

```text
PixiCompat
```

Implementar progresivamente solo las APIs requeridas.

Ejemplos:

```text
PIXI.Sprite
PIXI.Container
PIXI.Texture
PIXI.BaseTexture
PIXI.Rectangle
PIXI.Point
PIXI.Graphics
```

Internamente podrán mapear a objetos del renderer nativo.

---

# 19. SHIMS DE ENTORNO WEB

Crear shims mínimos cuando sean necesarios:

```text
window
document
navigator
localStorage
performance
requestAnimationFrame
XMLHttpRequest/fetch
```

NO implementar un navegador.

Cada shim debe documentar exactamente qué soporta.

---

# 20. NW.JS COMPATIBILITY

Plugins pueden consultar:

```javascript
Utils.isNwjs()
require(...)
process
nw
```

No falsificar soporte completo.

Crear una capa:

```text
NWCompat
```

Para llamadas comunes y seguras.

El uso arbitrario de módulos Node debe considerarse inicialmente no compatible.

---

# 21. GUARDADO

Crear sistema portable.

Compatibilidad deseable con formato MV cuando sea técnicamente viable.

```text
Game State
   |
Serializer
   |
Save Backend
```

Backend R36S:

```text
/saves/<game-id>/
```

No escribir dentro de los assets del juego.

---

# 22. CIFRADO DE ASSETS

No convertir el proyecto en una herramienta para eludir protecciones.

El runtime debe funcionar prioritariamente con proyectos propios y assets accesibles legítimamente.

No incorporar herramientas orientadas a extracción masiva de contenido de terceros.

---

# 23. SEGURIDAD DE PLUGINS

Los plugins son código.

No asumir que un plugin descargado es seguro.

Cuando sea posible limitar:

- filesystem;
- networking;
- ejecución externa;
- shell;
- acceso nativo.

Permisos futuros:

```text
filesystem.read
filesystem.write
network
process
native
```

Modo predeterminado:

```text
sandboxed
```

---

# 24. LICENCIAS Y CLEAN-ROOM

No copiar código propietario del runtime de RPG Maker MV dentro del proyecto salvo que una licencia permita expresamente ese uso y redistribución.

La implementación debe centrarse en:

- formatos de datos;
- comportamiento observable;
- documentación propia;
- pruebas propias;
- APIs de compatibilidad.

Mantener:

```text
docs/legal/
    COMPATIBILITY_POLICY.md
    THIRD_PARTY.md
    CLEAN_ROOM_NOTES.md
```

Los assets y proyectos comerciales del usuario no deben formar parte del repositorio.

---

# 25. ESTRUCTURA DEL REPOSITORIO

```text
perplo-mv-runtime/
│
├── CMakeLists.txt
├── README.md
├── LICENSE
│
├── docs/
│   ├── ARCHITECTURE.md
│   ├── MV_COMPATIBILITY.md
│   ├── PLUGINS.md
│   ├── PORTING.md
│   ├── TESTING.md
│   └── legal/
│
├── src/
│   ├── main.cpp
│   ├── core/
│   ├── mv/
│   ├── game/
│   ├── render/
│   ├── audio/
│   ├── input/
│   ├── js/
│   ├── plugins/
│   ├── save/
│   └── platform/
│       ├── linux/
│       ├── windows/
│       └── rk3326/
│
├── third_party/
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── compatibility/
│   └── projects/
│
└── tools/
```

---

# 26. NO CREAR UN BACKEND “R36S” DEMASIADO ESPECÍFICO

Siempre que sea posible:

```text
platform/linux
```

debe resolver la funcionalidad.

Solo crear:

```text
platform/rk3326
```

cuando exista una diferencia real de hardware o sistema.

---

# 27. COMPILACIÓN

Usar CMake.

```bash
cmake -B build
cmake --build build
```

Para ARM:

```text
toolchains/aarch64-linux.cmake
```

Mantener:

```text
Debug
Release
RelWithDebInfo
```

---

# 28. LOGGING

Categorías:

```text
BOOT
DATA
MAP
EVENT
JS
PLUGIN
RENDER
AUDIO
INPUT
SAVE
PERF
ERROR
```

Niveles:

```text
TRACE
DEBUG
INFO
WARN
ERROR
FATAL
```

---

# 29. CRASH REPORTS

Cuando exista una excepción JavaScript mostrar:

```text
Plugin
Archivo
Línea
Stack
Mensaje
```

Nunca cerrar silenciosamente.

---

# 30. TEST PROJECT

Crear un proyecto MV de pruebas propio y mínimo.

```text
001_boot
002_movement
003_events
004_switches
005_variables
006_messages
007_transfer
008_audio
009_menu
010_battle
011_save
012_plugins
```

Cada mapa valida una función.

---

# 31. GOLDEN TESTS

No depender únicamente de pruebas visuales.

Ejemplo:

```text
Input:
  Switch #1 OFF

Event:
  Control Switch #1 ON

Expected:
  Switch #1 == true
```

---

# 32. PRUEBAS DE FRAME

Registrar:

```text
FPS
frame time
draw calls
texture memory
RAM
JS heap
map load time
```

Target inicial R36S:

```text
60 FPS cuando el proyecto lo permita
30 FPS mínimo aceptable para escenas pesadas
```

---

# 33. PRESUPUESTO DE MEMORIA

Diseñar para memoria limitada.

Evitar:

- cargar todas las imágenes;
- duplicar texturas;
- mantener mapas anteriores completos;
- buffers enormes;
- fugas en JS.

Crear:

```text
TextureCache
AudioCache
MapCache
```

---

# 34. FASE 0 — RECONOCIMIENTO

Objetivos:

1. crear repositorio;
2. generar árbol base;
3. preparar CMake;
4. inicializar SDL2;
5. inicializar logging;
6. definir interfaces;
7. escribir documentación;
8. crear test project.

Resultado esperado:

```text
perplo-mv-runtime --version
perplo-mv-runtime --help
```

---

# 35. FASE 1 — PROJECT INSPECTOR

Implementar:

```bash
perplo-mv-runtime inspect ./Game
```

Debe mostrar datos reales:

```text
RPG Maker MV project detected
Maps: 27
Actors: 8
Plugins: 14
Images: ...
Audio: ...
Encrypted assets: yes/no
```

Esta fase NO necesita renderizar.

---

# 36. FASE 2 — BOOT + DATA

Objetivo:

- cargar `System.json`;
- database completa;
- localizar mapa inicial;
- cargar actor/party;
- crear estado de juego.

Resultado:

```text
BOOT TEST: PASS
```

---

# 37. FASE 3 — PRIMER MAPA

Renderizar:

- tiles;
- player.

Permitir caminar.

Todavía sin eventos complejos.

---

# 38. FASE 4 — EVENT INTERPRETER

Orden recomendado:

### Grupo A
- Show Text
- Show Choices
- Control Switches
- Control Variables
- Conditional Branch

### Grupo B
- Transfer Player
- Set Event Location
- Set Movement Route
- Wait

### Grupo C
- Common Event
- Loop
- Break Loop
- Label
- Jump to Label

### Grupo D
- Party
- Items
- Gold
- State

### Grupo E
- Script
- Plugin Command

Cada grupo requiere tests.

---

# 39. FASE 5 — UI

Implementar:

```text
Window
Window_Base
Window_Selectable
Window_Command
```

Luego:

- mensajes;
- choices;
- title;
- menu.

---

# 40. FASE 6 — SAVE

Implementar:

- new game;
- save;
- load;
- config.

Prueba obligatoria:

```text
state -> save -> load -> state
```

---

# 41. FASE 7 — BATTLE

Orden:

1. battlers;
2. stats;
3. actions;
4. damage;
5. states;
6. skills;
7. items;
8. enemy AI;
9. battle UI;
10. animations.

---

# 42. FASE 8 — JAVASCRIPT PLUGINS

Primero cargar un plugin trivial propio.

Después probar:

```text
Test_CorePatch.js
Test_PluginCommand.js
Test_Window.js
Test_Sprite.js
Test_Save.js
```

Solo después probar suites externas.

---

# 43. FASE 9 — PLUGINS REALES

Mantener matriz:

| Plugin | Carga | Runtime | Visual | Save | Estado |
|---|---:|---:|---:|---:|---|
| Plugin A | Sí | Sí | Sí | Sí | Compatible |
| Plugin B | Sí | Parcial | No | Sí | Parcial |

No romper vanilla para acomodar un plugin.

---

# 44. FASE 10 — R36S

Cuando funcione en Linux de escritorio:

1. cross compile ARM64;
2. ejecutar en AURKNIX/ROCKNIX;
3. detectar gamepad;
4. validar GLES;
5. validar audio;
6. medir RAM;
7. medir FPS;
8. ajustar escalado 640x480;
9. crear launcher.

Estructura orientativa:

```text
/roms/ports/PERPLO-MV/
    runtime
    config/
    games/
```

---

# 45. GAME MANIFEST

Opcional:

```text
perplo.json
```

Ejemplo:

```json
{
  "runtime": "mv",
  "gameId": "saor",
  "title": "SAOR",
  "resolution": {
    "width": 816,
    "height": 624
  },
  "compatibility": {
    "pluginMode": true
  }
}
```

No debe ser obligatorio.

---

# 46. COMMAND LINE

Implementar:

```bash
perplo-mv-runtime Game/
perplo-mv-runtime inspect Game/
perplo-mv-runtime validate Game/
perplo-mv-runtime plugins Game/
perplo-mv-runtime benchmark Game/
```

---

# 47. VALIDATOR

`validate` debe detectar:

- JSON faltante;
- mapas inválidos;
- assets faltantes;
- plugins ausentes;
- extensión incorrecta;
- scripts incompatibles;
- llamadas NW.js;
- llamadas Node;
- APIs PIXI desconocidas.

Salida:

```text
ERROR
WARNING
INFO
```

---

# 48. PLUGIN SCANNER

Analizar referencias comunes:

```text
PIXI.
SceneManager.
Graphics.
require(
process.
nw.
document.
window.
XMLHttpRequest
WebGL
```

Sirve como diagnóstico, no como prueba definitiva.

---

# 49. MODO STRICT

Crear:

```bash
--strict
```

Con strict, una API no implementada produce error claro.

Sin strict, pueden existir fallbacks seguros.

Nunca ocultar el error sin registrarlo.

---

# 50. PERFORMANCE MODE

Perfiles futuros:

```text
accurate
balanced
performance
```

Solo cambiar parámetros gráficos o de cache; nunca lógica.

---

# 51. PERPLO / DOMINA

PERPLO y DOMINA NO deben incorporarse al núcleo inicial.

Primero alcanzar:

```text
RPG Maker MV vanilla
```

Después:

```text
modules/
    perplo/
    domina/
```

DOMINA utilizará interfaces públicas del runtime.

---

# 52. RELACIÓN CON MV2PSX

Si existe MV2PSX, no duplicar parsers, especificaciones ni tests innecesariamente.

Crear eventualmente:

```text
mv-common/
```

que describa:

- database;
- maps;
- events;
- commands;
- resources;
- compatibility tests.

Arquitectura:

```text
                    MV PROJECT
                        |
                    MV COMMON
                  /            \
                 v              v
        PERPLO MV Runtime      MV2PSX
             |                   |
        SDL/GLES             PSn00bSDK
             |
          R36S
```

---

# 53. DEFINICIÓN DE “MASTER RUNTIME”

Se considerará Master Runtime cuando:

- cargue proyectos MV sin preparación manual;
- ejecute vanilla con alta compatibilidad;
- tenga backend multiplataforma;
- posea VM JavaScript;
- soporte plugins mediante compatibility layer;
- detecte incompatibilidades;
- tenga suite de tests;
- tenga documentación de APIs;
- tenga matriz de compatibilidad;
- no esté acoplado a un único juego.

No significa ejecutar mágicamente el 100% de los plugins existentes.

---

# 54. REGLAS PARA ANTIGRAVITY

## A. Antes de modificar

Leer:

```text
README.md
docs/ARCHITECTURE.md
docs/MV_COMPATIBILITY.md
```

## B. No improvisar arquitectura

Si un módulo cruza capas, documentarlo.

## C. Cambios pequeños

Cada milestone debe compilar.

## D. Tests

Toda funcionalidad de compatibilidad debe incluir pruebas.

## E. No borrar para “arreglar”

No eliminar código funcional solo para hacer pasar una prueba.

## F. No fingir implementación

Si algo no está soportado:

```text
NOT_IMPLEMENTED
```

y registrarlo.

## G. No ocultar errores

No usar `catch (...) {}` vacío.

## H. Documentación simultánea

Cuando cambia una API, actualizar documentación.

## I. Separación de plataforma

Nada específico de RK3326 debe contaminar lógica MV.

## J. Source code real

Todo componente creado debe conservar su source y formar parte del build reproducible.

---

# 55. PROTOCOLO DE TRABAJO PARA ANTIGRAVITY

Ante cada orden:

### 1. ANALIZAR
Determinar módulo, dependencias, riesgo y tests.

### 2. PLANIFICAR
Escribir una lista breve de cambios.

### 3. IMPLEMENTAR
Trabajar solo sobre el alcance necesario.

### 4. COMPILAR
Ejecutar build.

### 5. TESTEAR
Ejecutar tests pertinentes.

### 6. REVISAR
Buscar regresiones, leaks, APIs rotas y coupling accidental.

### 7. DOCUMENTAR
Actualizar estado.

### 8. INFORMAR
Entregar:

```text
Cambios realizados
Archivos modificados
Tests ejecutados
Resultado
Problemas pendientes
Siguiente paso recomendado
```

---

# 56. ARCHIVO DE ESTADO

Mantener:

```text
docs/STATUS.md
```

Ejemplo:

```markdown
# Runtime Status

## Build
PASS

## MV Compatibility
Level 2

## Implemented
- Data loading
- Map rendering
- Player movement

## Partial
- Events

## Missing
- Battle
- Save
- Plugins

## Known regressions
None
```

---

# 57. COMPATIBILITY DATABASE

Crear:

```text
compatibility/
    vanilla.json
    plugins.json
    projects.json
```

Esto permitirá generar reportes automáticamente.

---

# 58. API VERSION

Definir:

```text
PERPLO_MV_RUNTIME_API
```

Ejemplo:

```text
0.1
```

Los módulos externos podrán declarar requisitos mínimos.

---

# 59. PRIMERA META CONCRETA

NO comenzar intentando ejecutar SAOR.

Primer objetivo:

```text
Un proyecto MV vanilla propio
        |
        v
Carga System.json
        |
        v
Carga Map001.json
        |
        v
Renderiza el mapa
        |
        v
Muestra al jugador
        |
        v
Acepta input
        |
        v
Permite caminar
```

Cuando funcione en PC:

```text
mismo proyecto
     |
cross compile
     |
     v
R36S
```

---

# 60. SEGUNDA META

Agregar:

```text
evento NPC
   |
Show Text
   |
Control Switch
   |
Conditional Branch
   |
Transfer Player
```

Si esto funciona, ya existe un runtime MV rudimentario real.

---

# 61. TERCERA META

Llegar a:

```text
Title
New Game
Map
Events
Menu
Save
Load
Battle
Game Over
```

Entonces puede llamarse:

```text
MV Vanilla Runtime Alpha
```

---

# 62. CUARTA META

Activar VM JavaScript y ejecutar plugins propios de prueba.

Luego avanzar hacia plugins externos.

---

# 63. QUINTA META

Introducir módulo de compatibilidad PERPLO.

Después DOMINA.

No antes.

---

# 64. CRITERIO DE ÉXITO EN R36S

La build ARM será considerada exitosa cuando:

1. arranque desde AURKNIX/ROCKNIX;
2. encuentre el proyecto;
3. cargue database;
4. renderice Map001;
5. detecte controles;
6. reproduzca audio;
7. mantenga estabilidad;
8. guarde datos;
9. cierre limpiamente;
10. genere logs legibles.

---

# 65. COSAS QUE NO DEBEMOS HACER

No:

- portar Chromium entero;
- intentar clonar NW.js completo;
- hardcodear SAOR;
- hardcodear R36S dentro del core;
- empezar por Yanfly;
- empezar por DOMINA;
- copiar todo el runtime original;
- afirmar compatibilidad sin pruebas;
- mezclar renderer con Game_Map;
- mezclar filesystem con lógica de eventos;
- usar globals nativos sin abstracción;
- silenciar APIs no implementadas;
- optimizar prematuramente rompiendo exactitud.

---

# 66. POSIBLE EVOLUCIÓN

```text
PERPLO MV MASTER RUNTIME
        |
        +-- Linux ARM
        |     +-- R36S
        |     +-- RK3326 handhelds
        |     +-- otros ARM SBC
        |
        +-- Linux x86_64
        |
        +-- Windows
        |
        +-- Android
        |
        +-- futuros backends
```

---

# 67. REFERENCIAS TÉCNICAS PARA INVESTIGACIÓN

Antes de diseñar compatibilidad de una API, estudiar:

- estructura de proyectos RPG Maker MV;
- áreas del corescript: core, managers, objects, scenes, sprites y windows;
- runtimes open source compatibles con formatos MV;
- SDL2;
- OpenGL ES;
- QuickJS;
- CMake;
- JSON;
- compresión compatible con saves cuando corresponda;
- comportamiento observable de proyectos propios de prueba.

Un proyecto contemporáneo relevante como referencia conceptual es **RPG Reactor**, que demuestra que puede existir un runtime propio compatible con proyectos MV/MZ sin depender arquitectónicamente del runtime original.

Usarlo solo como referencia respetando su licencia y procedencia.

---

# 68. INSTRUCCIÓN INICIAL PARA ANTIGRAVITY

Antigravity:

Tu tarea es comenzar la construcción de **PERPLO MV MASTER RUNTIME** siguiendo este documento como especificación arquitectónica.

No intentes completar todo el runtime en una sola intervención.

Comienza exclusivamente por **FASE 0** y **FASE 1**:

1. inspecciona el repositorio actual si ya existe;
2. no destruyas código funcional previo;
3. crea o normaliza la estructura de carpetas;
4. configura CMake;
5. crea el ejecutable base;
6. implementa logging;
7. crea interfaces iniciales de plataforma;
8. crea `MVProject`;
9. implementa detección de un proyecto MV;
10. implementa el comando `inspect`;
11. genera `docs/ARCHITECTURE.md`;
12. genera `docs/MV_COMPATIBILITY.md`;
13. genera `docs/STATUS.md`;
14. agrega tests;
15. compila;
16. ejecuta los tests.

El comando mínimo que debe funcionar al terminar será:

```bash
perplo-mv-runtime inspect /ruta/a/proyecto
```

y deberá devolver información real obtenida del proyecto, no datos simulados.

No avances todavía a renderizado del mapa hasta que el inspector, la arquitectura y los tests de carga de datos sean sólidos.

Al finalizar entrega:

```text
RESUMEN
ARCHIVOS CREADOS
ARCHIVOS MODIFICADOS
ARQUITECTURA RESULTANTE
BUILD
TESTS
LIMITACIONES
DEUDA TÉCNICA
SIGUIENTE MILESTONE
```

Si una decisión arquitectónica contradice este documento, no la cambies silenciosamente: documenta el motivo técnico y propone la alternativa antes de acoplar el proyecto.

El objetivo no es producir una demo rápida.

El objetivo es construir una base mantenible que pueda convertirse realmente en un runtime maestro de RPG Maker MV para múltiples plataformas.

---

# FIN DE LA ESPECIFICACIÓN
