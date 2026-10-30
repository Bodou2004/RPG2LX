# Motor nativo y escenas — RPG2LX 0.3.1-alpha

Usa los archivos de un proyecto real sin convertir ni editar sus mapas. Abrir con `run <carpeta>` o desde games/. Una exportación dentro de www/ también sirve. Los datos sintéticos de fixtures/ se usan solamente para regresiones de desarrollo.

## Controles

En biblioteca: cruceta/stick elige; X/P inicia y muestra el título; A/Enter abre detalles. En juego: cruceta/stick mueve; A/Enter/Z/espacio confirma/interactúa; B/X/Start/Esc abre o cancela el menú; Y/Shift corre. B durante un mensaje no cierra la sesión. El título ignora cancelar. Volver a biblioteca exige seleccionar expresamente Menú → Salir → Biblioteca RPG2LX; Salir abre con Cancelar seleccionado.

## Datos respetados

System: título, capas title1Name/title2Name, optDrawTitle, optFollowers, grupo, términos y flags de comandos del menú. Actores/clases/equipo: personajes, faces, nivel inicial, parámetros básicos, habilidades iniciales, inventario y permisos de equipo. Las ventanas usan img/system/Window.png; la fuente bitmap sigue siendo provisional. El viewport del juego parte de 816x624 o de las dimensiones detectadas por el inspector; la biblioteca usa 640x480.

Los seguidores persiguen la posición previa del miembro anterior y respetan los mapas con loops. Se reúnen tras una transferencia y se pueden ocultar mediante el comando MV. Hasta cuatro personajes visibles, como el grupo de batalla MV por defecto.

Guardado: saves/<identidad de carpeta>/file1.json a file20.json, fuera del proyecto. Al reemplazar un slot se conserva .bak; la carga valida y construye la sesión antes de sustituir la actual. Guarda posición, dirección, switches, self-switches, variables, inventario, oro, grupo/equipo/habilidades y HP/MP. Formato propio, no intercambiable con .rpgsave. Guardado durante un intérprete activo no soportado.

## Intérprete actual

| IDs | Función | Límites |
|---|---|---|
| 0, 108/408 | Fin/comentarios | Preserva orden |
| 101/401, 102/402/403/404 | Texto y elecciones | Faces/fondo/posición, paginación y cancelación configurada; faltan escapes y efectos |
| 111/411/412 | Condiciones | Switches, variables, self-switch, miembro del grupo, oro e ítems; otros tipos producen diagnóstico |
| 112/113/413, 115 | Bucles, romper, salir | Presupuesto de 1000 instrucciones por ejecución |
| 117 | Evento común | Llamadas anidadas hasta 32, espera/retorno; autorun común por switch; no paralelos |
| 118/119 | Etiquetas/saltar | Etiquetas de la misma lista |
| 121/122/123 | Switches/variables/self-switch | Variables enteras con constantes u otra variable; faltan random, game data y scripts |
| 125–129 | Oro, ítems, armas/armaduras y grupo | Inventario limitado; estado de actores fuera del grupo pendiente |
| 134/135 | Guardar/menú habilitado | Flags persistidos |
| 201 | Transferencia | ID/coordenadas directos o variables, dirección y reunión del grupo; sin fade |
| 216/217 | Seguidores | Mostrar/ocultar/reunir |
| 230 | Espera | Frames a 60 Hz |
| 311/312/314/318 | HP/MP, recuperar todo y habilidades | Solo actores actuales, no todos los estados/traits |
| 351/352/354 | Menú/guardar/título | Guardado dentro de un intérprete no implementado |

Un comando no implementado, como 355/script, se registra y detiene la sesión; no se ejecuta ni se omite silenciosamente. El diagnóstico se muestra fuera de la partida, sin HUD sobre el mapa.

## Verificación y pendientes

GameStateTests comprueba seguidores, menú que conserva sesión, save/load y fallo atómico, elecciones/ramas/variables/eventos comunes, uso/consumo de objetos, habilidades y equipo persistente. GameViewTests renderiza escenas con SDL oculto. Project1 real se probó con su título Castle, caras y Window.png, sin editar el juego. Capturas: build/project1-scenes/.

Siguen pendientes batalla, audio, VM/plugins, estados completos, rutas de eventos, vehículos, parallax, películas, cifrado y numerosos comandos/efectos. Abrir un mapa no equivale a ejecutar el motor MV completo. Matriz: MV_COMPATIBILITY.md.

Las tablas de autotiles derivan del corescript oficial MIT; aviso conservado en docs/legal/MV-CORESCRIPT-LICENSE.txt. No se redistribuyen assets de prueba del usuario públicamente.
