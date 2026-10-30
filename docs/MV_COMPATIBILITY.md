# Compatibilidad MV — RPG2LX 0.3.0-alpha

Actualizado 2026-10-30. Motor nativo parcial: no ejecuta los scripts del motor MV ni plugins. Abrir un mapa no certifica un juego completo.

| Área | Implementado | Pendiente / alcance |
|---|---|---|
| Biblioteca | Lista/detalles con preview compuesto de title1Name/title2Name | No garantiza compatibilidad del proyecto |
| Título | Fondo original, nombre si optDrawTitle, Nueva partida/Continuar/Opciones y términos de System | Plugins de logos/título y transiciones; fuente GameFont aún no usada |
| Mapas | Tiles/autotiles PNG, colisiones, sombras, cámara y loops | Parallax, overpass, vehículos y reglas de mesas incompletas |
| Grupo | Actores iniciales, estadísticas básicas, faces, cambio de formación, seguidores optFollowers | Estados/traits completos, personajes fuera del grupo, experiencia/niveles dinámicos |
| Movimiento | Interpolación, dash, seguidores hasta cuatro miembros | Rutas autónomas/forzadas de eventos y vehículos |
| Menús | Objeto, habilidades, equipo, estado, formación, opciones, guardar y salir | Diseño nativo con Window.png/caras/términos; no reproduce scripts de ventanas ni todas las opciones |
| Objetos/habilidades | Recuperar HP/MP, revivir, consumo y coste MP; inventario | Fórmulas JavaScript, TP, estados y efectos adicionales sin implementar |
| Equipo | Elegir actor/slot/objeto, inventario, parámetros y permisos de clase | Optimizar, dual wield y reglas completas de traits pendientes |
| Eventos | Acción/toque/autorun; elecciones, ramas, variables, llamadas comunes, bucles, etiquetas, transferencias | Paralelos y muchos operandos/comandos; ver NATIVE_EXPLORATION.md |
| Mensajes | Paginación, face, fondo y posición, elecciones con cancelación configurada | Escape codes, fuente GameFont, efectos/timing originales |
| Guardado | 20 slots propios, escritura con respaldo, carga atómica, grupo/equipo/recursos/mapa/switches/variables | No compatible con .rpgsave original. No guarda un intérprete en ejecución ni todos los sistemas MV |
| Batallas | Pendiente | Tropas, acciones, daño, turnos, estados, recompensas, UI y animaciones |
| Audio | Pendiente | BGM/BGS/ME/SE y opciones de volumen |
| JavaScript/plugins | Solo inspección | Sin VM, APIs PIXI/DOM/NW.js, plugins ni scripts de eventos |
| Imágenes cifradas | Rechazo explícito | No descifra assets |
| PortMaster | ARM64 portable, SDL2 del firmware, GLIBC_2.17 | La versión anterior fue probada por el usuario; esta actualización requiere prueba física |

Errores de funciones desconocidas se registran y no se omiten para aparentar compatibilidad. No se añaden barras de diagnóstico al mapa. La fuente bitmap provisional y el layout nativo todavía difieren de MV.

Objetivo pendiente: un juego completo con eventos, combate, audio y scripts originales. No se afirma compatibilidad universal ni con todos los juegos de RPG Maker MV.