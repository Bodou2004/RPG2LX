# Estado de RPG2LX

2026-10-30 — versión 0.3.0-alpha.

Codex implementa núcleo, escenas, controles, grupo, guardado, renderer e integración. Antigravity programó LibraryView con preview de título en paralelo (A003); Codex integró y corrigió proporciones, Unicode y distribución. Registro compartido: COORDINATION.md.

La versión anterior instalada en H:/ports arrancó en consola según el usuario, pero solo permitía exploración y B destruía la sesión. La actualización cambia B/Start/X a menú/cancelación del juego. Volver a biblioteca exige elegir esa opción en la pantalla Salir; esa pantalla abre con Cancelar seleccionado.

Implementado: título del proyecto, menú nativo con términos/caras/Window.png, seguidores, dash, formación, estado, inventario y uso básico de objetos/habilidades, equipo y estadísticas. Guardado/carga en slots propios fuera del juego, con respaldo al reemplazar y carga que conserva la sesión si falla. Eventos ampliados con elecciones, condiciones, variables, llamadas comunes, etiquetas/bucles e inventario/HP/MP.

La ejecución sigue siendo nativa parcial. No hay VM/plugins, batalla ni audio; faltan rutas, efectos, estados y otros comandos. GameFont no se carga todavía. La matriz exacta está en MV_COMPATIBILITY.md. Los saves propios no son los de MV original.

Validación de desarrollo: suites CTest Debug/Release, render oculto SDL con Project1 y prueba de título → nueva partida → seguidores → menú/equipo → guardar → continuar. Capturas y logs solo en build/. No se pide al usuario jugar en PC ni se modifican los archivos del juego.

ARM64 usa el mismo contrato PortMaster, SDL2 del firmware y símbolos GLIBC_2.17. Ver SD_PACKAGE.md para artefactos y controles. La prueba física de esta nueva versión sigue pendiente; no confundirla con la prueba de la versión anterior.