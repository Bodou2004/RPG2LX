# Estado de RPG2LX

Actualizado por Codex: 2026-10-30, America/Buenos_Aires.

## Reparto vigente

Codex implementa núcleo, arquitectura, compilación y pruebas nativas. Antigravity se ocupa de tareas auxiliares en PortMaster y su documentación. Traspaso registrado en C004 y confirmado en la conversación de Antigravity.

## Implementado

- CLI C++17 con --help, --version, inspect, inspect-map y list; menú gráfico SDL2 al abrir sin argumentos o usar launcher.
- Inspector: lectura de System, MapInfos, Actors y CommonEvents; metadatos, conteos y configuración estática de plugins sin ejecución. Aún no valida toda la semántica de la base de datos ni certifica que el juego sea jugable. El menú detecta estructura, no ejecución ni compatibilidad de plugins.
- Parser JSON con validación de sintaxis, Unicode UTF-8/escapes/pares sustitutos, números finitos, claves duplicadas y límite de profundidad. No se ha realizado una auditoría exhaustiva ni fuzzing.
- Módulo MapLoader/MapData de Codex: seis planos, dimensiones, tileset, scroll por eje, regiones, sombras y eventos/páginas/comandos/parámetros. Lectura independiente del renderer y diagnósticos por datos inválidos. Ver MAP_DATA.md.
- CMake, pruebas nativas y pruebas CLI sobre el ejecutable real; instrucciones reproducibles en BUILDING.md.
- Biblioteca games y menú SDL2 de Codex: lista paginada, detalles, comprobación de datos, navegación de mando/teclado y salida; detecta proyecto directo o www. Partidas/configuración separadas por carpeta; X/P inicia el motor nativo parcial.
- Motor GameSession: tilesets/flags, colisión bilateral, movimiento, actor inicial, eventos y transferencias. Renderer PNG con autotiles/sprites/cámara. Demo propia de dos mapas instalada localmente; ver NATIVE_EXPLORATION.md.
- Plantillas PortMaster adaptadas al menú launcher --home, sin duplicación de controles SDL/gptokeyb. No hay paquete ARM jugable.

## Validación ejecutada

Windows x86_64 con Zig 0.13.0 / Clang 18.1.6, CMake 4.4.4 y Ninja 1.13.2 preparados localmente en build/.

- Configuración y build CMake Debug: completadas sin avisos del código en el log de build.
- CTest Debug/Release del menú: 8/8 grupos del nuevo motor: inspector, JSON, mapas, biblioteca/modelo, sesión nativa, arranque SDL, CLI y renderizado de juego. Resultados finales del nuevo hito en build/session-debug-tests.log y build/session-release-tests.log.
- La suite CLI ejecuta 10 casos (incluyendo biblioteca, directorios en conflicto y carpetas Unicode) sobre el binario C++, con fixtures temporales, corrupción de cada archivo requerido, errores de tipos, Unicode, rutas con espacios, plugins comentados y datos de mapa inválidos.
- La primera prueba de plugins detectó un caso de lectura incorrecta de comentarios con corchetes; Codex lo corrigió y la suite final pasa.
- Build Release del menú: CMake Release con NDEBUG y suites nativas; resultados finales en build/menu-release-tests.log. La prueba Unicode detectó argv ANSI en Windows; Codex agregó conversión explícita desde argumentos UTF-16 y el caso final pasa.

## Pendiente

Hay renderer de mapas, movimiento e intérprete inicial de eventos. Faltan audio, VM/plugins JavaScript, batalla, menús internos, guardado y muchos comandos MV. Se verificó únicamente el recorrido de la demo propia, no juegos completos de terceros. Sin build ARM ni prueba R36S. Las fixtures son datos de prueba propios, no juegos completos.

Próximo trabajo principal de Codex: extender eventos (variables, condiciones, elecciones y eventos comunes), persistencia y audio; validar con un proyecto MV real aportado legítimamente. Luego preparar y medir build ARM. Antigravity continúa con empaquetado y tareas auxiliares según encargo.
