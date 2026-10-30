# Compatibilidad MV de RPG2LX 0.2.0-alpha

Estado 2026-10-30: primer motor nativo de exploración. Datos propios de prueba con dos mapas verificados; ningún juego comercial completo, plugin ni dispositivo R36S certificado.

| Área | Estado real | Límite |
|---|---|---|
| Biblioteca games | Implementada | Detección estructural, no garantía de compatibilidad |
| System.json | Arranque nativo | startMapId, startX/Y, partyMembers; no toda la configuración del juego |
| Actors.json | Actor inicial | Character sheet del primer miembro; sin estadísticas/batalla |
| Tilesets.json | Implementado | Nombres de sheets, flags, paso por dirección y estrella |
| MapXXX.json | Carga y exploración | Dimensiones, planos, regiones, eventos, loops; ver NATIVE_EXPLORATION.md |
| PNG y sprites | Renderer SDL2 | PNG no cifrado, tiles 48px, A1–A5/B–E y characters; faltan reglas especiales de mesa/overpass/parallax |
| Movimiento | Básico | Cuatro direcciones, interpolación, colisión de origen/destino y eventos; sin vehículos/followers/dash |
| Eventos | Parcial | Acción, toque, autorun; texto, switches, self-switches, transferencias directas y espera |
| Páginas/condiciones | Parcial | Prioridad inversa, switches/self-switches, grupo y variables iniciales; sin inventario dinámico |
| Diálogos | Parcial | UTF-8 y paginación; sin faces, escape codes, elecciones ni todas las opciones de ventana |
| CommonEvents.json | Solo inspección | Aún no se ejecutan eventos comunes |
| Plugins.js | Solo metadatos | El JavaScript no se ejecuta; no hay VM ni soporte de plugins |
| Scripts MV/NW.js | No implementado | Los comandos script detienen la sesión con diagnóstico |
| Batalla y menús MV | Pendiente | No disponibles |
| Audio | Pendiente | BGM/BGS/ME/SE no reproducidos |
| Guardado | Pendiente | Existen directorios por juego, no serialización de partidas |
| Imágenes cifradas | No soportado | Rechazadas explícitamente |
| Windows x86_64 | Pruebas nativas | Build Debug/Release; SDL dummy/render y recorrido en C++ |
| R36S/Linux ARM | Pendiente | Sin build ARM, prueba de firmware, mandos físicos ni métricas de rendimiento |

No se afirma compatibilidad universal. Un proyecto puede abrir un mapa y aun así fallar al activar un evento o depender de APIs no soportadas. Los comandos desconocidos se diagnostican; no se omiten para aparentar funcionamiento.

La demo RPG2LX - Primer viaje valida el camino: arranque → movimiento → diálogo → switch → transferencia al segundo mapa → retorno. Ver NATIVE_EXPLORATION.md para detalles y controles.
