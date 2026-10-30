# Estado de RPG2LX

Actualizado por Codex: 2026-10-30, America/Buenos_Aires.

## Reparto vigente

Codex implementa núcleo, arquitectura, compilación y pruebas nativas. Antigravity se ocupa de tareas auxiliares en PortMaster y su documentación. Traspaso registrado en C004 y confirmado en la conversación de Antigravity.

## Implementado

- CLI C++17 con --help, --version, inspect e inspect-map.
- Inspector: lectura de System, MapInfos, Actors y CommonEvents; metadatos, conteos y configuración estática de plugins sin ejecución. Aún no valida toda la semántica de la base de datos ni certifica que el juego sea jugable.
- Parser JSON con validación de sintaxis, Unicode UTF-8/escapes/pares sustitutos, números finitos, claves duplicadas y límite de profundidad. No se ha realizado una auditoría exhaustiva ni fuzzing.
- Módulo MapLoader/MapData de Codex: seis planos, dimensiones, tileset, scroll por eje, regiones, sombras y eventos/páginas/comandos/parámetros. Lectura independiente del renderer y diagnósticos por datos inválidos. Ver MAP_DATA.md.
- CMake, pruebas nativas y pruebas CLI sobre el ejecutable real; instrucciones reproducibles en BUILDING.md.
- Plantillas PortMaster auxiliares de Antigravity. Solo invocan el inspector; no hay paquete de runtime jugable.

## Validación ejecutada

Windows x86_64 con Zig 0.13.0 / Clang 18.1.6, CMake 4.4.4 y Ninja 1.13.2 preparados localmente en build/.

- Configuración y build CMake Debug: completadas sin avisos del código en el log de build.
- CTest Debug: 4/4 grupos pasados (inspector, JSON, mapas y CLI nativa).
- La suite CLI ejecuta 7 casos sobre el binario C++, con fixtures temporales, corrupción de cada archivo requerido, errores de tipos, Unicode, rutas con espacios, plugins comentados y datos de mapa inválidos.
- La primera prueba de plugins detectó un caso de lectura incorrecta de comentarios con corchetes; Codex lo corrigió y la suite final pasa.
- Build Release (-O2 -DNDEBUG): completada; pasan las suites C++ de inspector, JSON y mapas, y los 7 casos CLI contra el binario Release. Logs locales en build/release-results.log.

## Pendiente

No hay renderer, bucle SDL2/GLES, audio, VM JavaScript, ejecución de eventos, guardado ni juegos validados. Sin build ARM ni prueba R36S. Las fixtures son datos de prueba propios, no juegos completos.

Próximo trabajo principal de Codex: extender carga de tilesets y colisiones, luego conectar mapa vanilla a backend SDL2/GLES. Antigravity continúa con empaquetado y tareas auxiliares según encargo.
