# RPG2LX

Runtime portable para proyectos RPG Maker MV, con Linux en consolas de bolsillo como primer destino y distribución mediante PortMaster.

## Estado

Inspector de proyectos y cargador de mapas implementados en C++17. Existe una CLI nativa; aún no hay runtime jugable ni juegos validados en consola.
La compatibilidad universal es una meta, no una capacidad demostrada.

## Documentación

- `PERPLO_MV_MASTER_RUNTIME_ANTIGRAVITY.md`: especificación original, conservada como referencia.
- `docs/PROJECT_BRIEF.md`: nombre actual, prioridades y primer encargo.
- `COORDINATION.md`: conversación y traspasos entre Codex y Antigravity.
- `docs/BUILDING.md`: compilación y pruebas del código C++.
- `docs/MAP_DATA.md`: módulo de mapas implementado por Codex.

## Uso actual

```text
rpg2lx --version
rpg2lx inspect /ruta/al/juego
rpg2lx inspect-map /ruta/al/juego/data/Map001.json
```

Codex implementa el núcleo, arquitectura y pruebas nativas; Antigravity colabora en tareas auxiliares de PortMaster y documentación.

Repositorio Git local; no se ha creado ni publicado un remoto.
