# Primer motor nativo de exploración

Implementado por Codex el 2026-10-30. Es una primera porción del runtime MV, no una máquina JavaScript ni soporte completo de juegos.

## Abrir archivos originales de RPG Maker MV

El objetivo es ejecutar proyectos exportados por MV. Copiar una carpeta completa a games/ o abrirla directamente con:

```text
rpg2lx run /ruta/al/proyecto/MV --home /ruta/a/rpg2lx
```

En Windows también se puede arrastrar la carpeta, index.html, Game.exe o .rpgproject sobre RPG2LX.cmd. No se ejecuta el EXE ni se modifican los datos; se resuelven los archivos MV desde esa ruta, incluyendo exportaciones con www/.

En la biblioteca, seleccionar el juego y pulsar X/P en detalles inicia el modo nativo. Flechas/D-pad/stick caminan, A/Enter/Z interactúa y B/Escape vuelve a la biblioteca. Todavía no hay guardado.

## Primera validación con el proyecto del usuario

Project1 se cargó desde su carpeta original de Documentos/Games: System, Map001 (17x13), Tilesets, Actor1/Harold y seis eventos People3. Se verificaron arranque, render de PNG originales, movimiento y colisión con EV002. Se compararon 1102 archivos originales por tamaño/fecha y hashes de JSON/JS/HTML/proyecto: sin cambios. No se añadieron personajes, mapas ni eventos al proyecto del usuario.

Community_Basic y CustomLogo están activos en su plugins.js; se informa explícitamente que aún no se ejecutan. La validación es del modo nativo parcial, no de escenas de arranque, plugins, batalla o juego completo.

La escena sintética anterior se retiró de la biblioteca de trabajo. Sus archivos siguen únicamente como fixtures internas para automatizar regresiones sin redistribuir el proyecto ni assets del usuario.

## Implementación actual

- Arranque desde System.json: mapa, coordenadas, actor de la primera posición del grupo y su character sheet.
- Tilesets.json: nueve nombres de imágenes, flags por tile y comprobación de referencias.
- Mapas de seis planos; tiles normales B–E/A5 y ensamblado de autotiles A1–A4. A1 incluye la cadencia inicial de agua/cascada.
- PNG RGBA con transparencia, personajes individuales `$` y sheets de ocho personajes, direcciones y animación al caminar. Convención `!` sin desplazamiento vertical.
- Cámara que sigue al jugador, mapas pequeños centrados, scroll continuo según ejes, sombras y tiles con estrella por encima de personajes.
- Movimiento en cuatro direcciones con interpolación; se comprueban la salida de origen y la entrada del destino. Los flags estrella no deciden paso. Los eventos sólidos bloquean la casilla.
- Páginas en orden inverso, con condiciones de switches, self-switches, actor del grupo y variables iniciales en cero. Inventario inicial vacío; no hay todavía comandos de inventario.
- Eventos por acción, toque y autorun. Mensajes paginados con hasta cuatro líneas por página. Los comandos posteriores esperan a que se cierren todas las páginas.
- Al transferir se valida el nuevo mapa/tileset antes de cambiar los datos. Se descartan texturas al cambiar de mapa o salir de la sesión.

## Comandos de eventos soportados

| ID | Comando | Alcance |
|---|---|---|
| 0 | Fin/entrada vacía | Finaliza la lista cuando no quedan comandos |
| 101, 401 | Mostrar texto | Texto UTF-8/Latin-1 visible, paginación; sin faces, escape codes ni selección de ventana |
| 108, 408 | Comentarios | Sin efecto de juego |
| 121 | Control de switches | Rangos limitados, ON/OFF |
| 123 | Control de self-switch | A–D por mapa/evento |
| 201 | Transferir jugador | Solo coordenadas e ID directos; sin transición de pantalla |
| 230 | Esperar | Frames a 60 Hz con límite de duración |

Un comando desconocido, por ejemplo 355 (script), detiene la sesión con diagnóstico en pantalla y registro. No se ignora para fingir que el juego funciona.

## Límites explícitos

No están implementados: batalla, audio, guardado/carga, menús internos MV, VM/plugin JavaScript, elecciones y ramas, inventario, rutas autónomas de eventos, vehículos, followers, parallax, movie, efectos, escape codes de mensajes, bordes especiales de mesas y reglas de overpass. Los PNG cifrados se rechazan con diagnóstico. Los plugins no se ejecutan, por lo que un juego que dependa de ellos no es compatible por el hecho de abrir su mapa.

El renderer usa tiles MV estándar de 48px y viewport 640x480. No pretende reproducir todavía todos los tamaños y escalados de cada proyecto. Los nombres de assets deben ser nombres de archivo dentro de img/tilesets e img/characters. El cache actual admite hasta 96 MiB de texturas y 64 MiB por imagen decodificada; imágenes/comandos inválidos generan error visible.

## Pruebas

SessionTests ejecuta el recorrido completo (movimiento, NPC sólido, diálogo, switch, entrada y salida), interpolación, bloqueo inverso del destino, geometría de tiles/autotiles, arranque fallido sin destruir sesión, paginación y rechazo de scripts.

NativeGameSmoke ejecuta el binario SDL2 real con driver dummy: comprueba ambos mapas, presencia de píxeles del tileset y del jugador, PNG faltante, assets cifrados y autorun desconocido. Las pruebas de renderer en Windows no validan mandos físicos, ABI ARM, firmware ni rendimiento en R36S.

## Procedencia

Las tablas de cuartos de autotiles y las fórmulas de layout se adaptaron de [Tilemap.js del corescript oficial MV](https://github.com/rpgtkoolmv/corescript/blob/master/js/rpg_core/Tilemap.js), con licencia MIT explícita. Se conserva el aviso en docs/legal/MV-CORESCRIPT-LICENSE.txt. La sesión, renderer SDL, carga de PNG, pruebas y assets de demo son implementación de RPG2LX. No se incorporó el código de ningún juego comercial.
