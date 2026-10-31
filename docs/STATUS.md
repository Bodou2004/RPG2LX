# Estado de RPG2LX

2026-10-31 — versión 0.6.0.

## Qué hay

- HUB para PortMaster: biblioteca, menú por juego (motor, controles) y editor de controles botón → tecla.
- HUB y juego en procesos separados: `RPG2LX.sh` abre el HUB, lanza el juego elegido y vuelve al HUB.
  Si un juego se cierra con error, el HUB sigue funcionando.
- Motor nativo (QuickJS + SDL2) que ejecuta los corescripts y plugins originales.
- Motor navegador opcional (NW.js `linux-arm64` dentro de Westonpack) para plugins que el nativo no soporta.
- Entrada como teclado real en ambos motores. Ver [ENGINES.md](ENGINES.md).

## Qué está comprobado (PC, Windows, Zig/clang)

- Todas las pruebas de `tools/build_native.py --release`: biblioteca, HUB, perfiles de controles,
  sesión nativa antigua, JSON, mapas, JS, y la prueba completa del motor contra la plantilla `NewData`
  de RPG Maker MV de Steam (logo → título → mapa → mensaje → menú → guardado → batalla).
- Teclado emulado: `X` abre el menú vía `Input.keyMapper`, `Escape` lo cierra, un listener `keydown`
  de plugin recibe la tecla y un remapeo de `keyMapper` hecho por plugin se respeta.
- Ciclo HUB → juego → HUB de `RPG2LX.sh` con un PortMaster simulado (rutas con espacios, SD2,
  registro de resultados, rechazo de arquitectura).
- En PC, 120 frames de `Scene_Map` en ~2 ms/frame. No es una medida de la consola.

## Qué no está comprobado

- Nada de 0.6.0 se ha ejecutado en una consola: rendimiento real en RK3326, mandos reales,
  Westonpack + NW.js + gptokeyb.
- La compatibilidad del motor nativo con plugins de terceros concretos.
