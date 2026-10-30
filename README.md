# RPG2LX

Runtime portable para proyectos RPG Maker MV, con Linux en consolas de bolsillo como primer destino y distribución mediante PortMaster.

## Estado

Menú gráfico SDL2, biblioteca `games`, inspector de proyectos y cargador de mapas implementados en C++17. Aún no hay ejecución de juegos ni validación en consola.
La compatibilidad universal es una meta, no una capacidad demostrada.

## Documentación

- `PERPLO_MV_MASTER_RUNTIME_ANTIGRAVITY.md`: especificación original, conservada como referencia.
- `docs/PROJECT_BRIEF.md`: nombre actual, prioridades y primer encargo.
- `COORDINATION.md`: conversación y traspasos entre Codex y Antigravity.
- `docs/BUILDING.md`: compilación y pruebas del código C++.
- `docs/MAP_DATA.md`: módulo de mapas implementado por Codex.
- `docs/LAUNCHER.md`: biblioteca games, menú y controles.

## Uso actual

Copiá cada juego completo dentro de `games/Nombre del juego/`. RPG2LX crea `games`, `saves`, `config` y `logs` al arrancar. También reconoce proyectos dentro de `games/Nombre del juego/www/`.

```text
rpg2lx --version
rpg2lx inspect /ruta/al/juego
rpg2lx inspect-map /ruta/al/juego/data/Map001.json
rpg2lx launcher --home /ruta/a/rpg2lx
rpg2lx list --home /ruta/a/rpg2lx
```

Codex implementa el núcleo, arquitectura y pruebas nativas; Antigravity colabora en tareas auxiliares de PortMaster y documentación.

Repositorio Git local; no se ha creado ni publicado un remoto.
