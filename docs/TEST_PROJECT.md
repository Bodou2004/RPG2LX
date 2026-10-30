# Project1 — recorrido de validación

Proyecto: games/Project1/Game.rpgproject. El original de Documentos/Games/Project1 se conserva. Antigravity preparó los eventos nativos; Codex abrió la copia en RPG Maker MV, revisó su base de datos y guardó las correcciones desde el editor. No se ejecutó una partida interactiva en PC.

Elegir **Nueva partida** para comenzar la prueba desde cero. A interactúa; B/Start abre o cancela el menú; Y corre. Las coordenadas siguientes son las de RPG Maker, empezando en cero.

| Lugar | Prueba |
|---|---|
| Guía al norte, (8,5) | Mensajes con cara, elecciones, cancelación y explicación de controles/grupo |
| Cofre al oeste, (6,6) | +500 oro, 3 pociones y 3 aguas mágicas; resta 150 PV y 30 PM al grupo sin matarlo |
| Maestro al sur, (8,8) | Enseña Curar a Harold/Therese; recupera 200 PV y cuesta 5 PM |
| Armería al este, (10,6) | Entrega espada, hacha, escudo y casco; probar quitar/equipar desde el menú |
| Palanca al suroeste, (6,8) | Activa switch 1, llama al evento común e incrementa variable 1 en cada uso |
| Cristal al este, (13,6) | Elección y transferencia a mapa 2; su cristal en (8,6) vuelve al mapa inicial |

Probar objetos en el menú: una poción recupera PV y se consume; agua mágica recupera PM y también se consume. En Formación se cambia el líder; el sprite del jugador y sus seguidores respetan el nuevo orden. En Opciones se puede alternar Correr siempre. Guardar y luego Continuar debe recuperar grupo, recursos, inventario, equipo, switches, variable y posición.

## Verificación reproducible

El ejecutable de desarrollo project_scenario usa el mismo GameSession y renderer del runtime. Recorre el mapa mediante colisiones reales, interactúa con eventos, escoge opciones y usa los menús. No genera ni modifica mapas. El escenario tests/scenarios/project1_features.json contiene 95 pasos y comprobaciones de efectos reales, incluidos parámetros al quitar/reponer equipo, gasto de MP, consumo, velocidad de marcha y carga de partida. SDL usa una ventana oculta para las capturas.

```powershell
$env:SDL_VIDEODRIVER='dummy'
$env:SDL_AUDIODRIVER='dummy'
build/cmake-release/project_scenario.exe games/Project1 tests/scenarios/project1_features.json build/project1-playthrough-release
```

Resultados locales: build/project1-playthrough-debug.log y build/project1-playthrough-release.log, 95 pasos pasados en ambas. Capturas BMP en build/project1-playthrough-release/. CTest Debug/Release: 10/10 grupos.

Fallos encontrados y corregidos durante esta prueba:

- Curar conservaba daño por fórmula además del efecto de 200 PV. En RPG Maker se cambió el tipo de daño a Nada; el efecto nativo queda como única recuperación.
- Map002 tenía siete planos. Codex regeneró sus datos desde Propiedades del mapa en RPG Maker y restableció 17x13; el archivo guardado tiene los seis planos requeridos.
- Los aprendizajes repetidos de una clase duplicaban habilidades en RPG2LX. El runtime ahora las aprende una sola vez, con regresión automatizada.

Este recorrido verifica las funciones nativas implementadas. Batalla, audio, JavaScript/plugins, animaciones y otros sistemas MV siguen pendientes. La ejecución automatizada host no reemplaza la nueva prueba física en consola.

## Entrega SD

Runtime 0.3.1-alpha: dist/RPG2LX-0.3.1-PortMaster-update.zip. La actualización de datos se hace por separado con tools/sync_game_data.py, limitada a data/*.json del juego elegido y con respaldo anterior. No copia artwork, JavaScript ni partidas. Ambos instaladores verifican el manifiesto de la SD y registran sus reportes en build/.
