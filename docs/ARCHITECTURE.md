> Actualización 0.3.0-alpha: sesión/intérprete en src/mv/session.cpp; escenas, grupo y persistencia en src/mv/game_state.cpp; renderer de juego en src/render/map_view.cpp; biblioteca de Antigravity en src/launcher/library_view.cpp. Ver NATIVE_EXPLORATION.md y MV_COMPATIBILITY.md para el alcance vigente. El resto de este documento conserva el diseño base inicial.

# RPG2LX — Arquitectura Técnica

Fecha: 2026-10-30
Autor: Antigravity / Pair programming con Codex
Estado: Hito 0/1 (Fase 0 y Fase 1 - Preparación, Inspector y CLI)

---

## 1. Misión y Enfoque

RPG2LX es un runtime modular e independiente diseñado para ejecutar juegos creados con RPG Maker MV en consolas portátiles Linux (prioritariamente dispositivos con SoC RK3326 tipo R36S bajo PortMaster, y ampliable a otras plataformas).

### Principio de diseño fundamental
No intentar emular o portar un navegador web completo (NW.js / Chromium) hacia un microprocesador con recursos limitados (Cortex-A35 / Mali-G31 MP2). En su lugar, RPG2LX implementa de forma nativa en C++17 las interfaces, carga de datos, bucle principal, gráficos y audio que RPG Maker MV realmente utiliza.

---

## 2. Diagrama de Módulos

```text
+-----------------------------------------------------------+
|                       rpg2lx (CLI)                        |
|   (--help, --version, inspect <path>, [futuro: run <path>])|
+-----------------------------------------------------------+
                             |
                             v
+-----------------------------------------------------------+
|                      rpg2lx_core                          |
+-----------------------------------------------------------+
| 1. ProjectInspector                                       |
|    - Parser JSON Estricto (rpg2lx::JsonParser)           |
|    - Validador de esquemas MV (System, MapInfos, etc.)    |
|    - Inspector estático de plugins (plugins.js)          |
|    - Sin ejecución de código JS durante inspección       |
|                                                           |
| 2. Logger                                                 |
|    - Niveles: Debug, Info, Warn, Error                    |
|    - Salida estándar y desacople para archivo/PortMaster  |
+-----------------------------------------------------------+
                             |
         +-------------------+-------------------+
         | (Próximos componentes: Fases 2 a 8)   |
         v                                       v
+-----------------------+              +--------------------+
|  Game Object Model    |              |  Platform Layer    |
|  - $dataSystem        |              |  - SDL2 Core       |
|  - $dataMap / Events  |              |  - OpenGL ES 2.0   |
|  - Event Interpreter  |              |  - Audio (SDL/ALSA)|
+-----------------------+              +--------------------+
```

---

## 3. Componentes Implementados (Fases 0 y 1)

### 3.1 Parser JSON Estricto (`rpg2lx::JsonParser`)
- Implementado en `include/rpg2lx/json/json_parser.hpp`.
- Parser recursivo descendente con cero dependencias externas.
- Soporta tipos nativos JSON: Object, Array, String, Number, Boolean, Null.
- Valida exhaustivamente comas faltantes, caracteres inesperados y llaves/corchetes no balanceados o mal ubicados. Rechaza JSON sintácticamente corrupto incluso si las llaves están balanceadas.

### 3.2 Inspector de Proyectos (`rpg2lx::ProjectInspector`)
- Localizado en `include/rpg2lx/core.hpp` y `src/core/inspector.cpp`.
- Comprueba presencia y estructura de los archivos mínimos indispensables:
  - `data/System.json`: Título del juego, StartMapId, variables, switches.
  - `data/MapInfos.json`: Array de metadatos de mapas.
  - `data/Actors.json`: Array de actores jugables.
  - `data/CommonEvents.json`: Array de eventos comunes.
  - `package.json`: Configuración de ventana (ancho/alto) del export de escritorio.
  - `js/plugins.js`: Lista de plugins sin evaluación arbitraria de código JavaScript. Extrae el nombre, estado (`status: true/false`), descripción y parámetros.
- Manejo estricto de errores de filesystem mediante `std::error_code` (sin lanzar excepciones no controladas) y soporte completo para rutas con espacios.

### 3.3 CLI `rpg2lx`
- Localizado en `src/cli/main.cpp`.
- Parámetros:
  - `--help` / `-h`: Documentación de uso.
  - `--version` / `-v`: Versión actual del runtime e información de plataforma.
  - `inspect <path>`: Analiza la carpeta del proyecto, emitiendo diagnóstico detallado.
  - Códigos de salida: `0` (éxito/válido), `1` (argumento inválido o error CLI), `2` (proyecto ausente, inválido o incompleto).

---

## 4. Procedimiento Reproducible de Build C++

### Requisitos:
- Compilador compatible con C++17 (`g++` >= 9, `clang++` >= 10, o MSVC >= 2019).
- CMake >= 3.16.

### Compilación Host (Linux / Windows / macOS):
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Ejecución de Pruebas Automatizadas (CTest):
```bash
cd build
ctest --output-on-failure
```
*Nota:* `CMakeLists.txt` define la propiedad `WORKING_DIRECTORY` y pasa la ruta de `fixtures` para que las pruebas funcionen independientemente de si se ejecutan desde la raíz o dentro del directorio `build/`. Además, el harness C++ no depende de `assert()` que desaparece en builds Release con `NDEBUG`.

### Compilación Cruzada para PortMaster (ARM64 / aarch64):
```bash
cmake -B build_arm64 \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build_arm64
```
