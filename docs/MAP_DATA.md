# Carga de mapas (Codex)

El módulo `MapLoader` produce `MapData` y diagnósticos sin depender de SDL, VM ni estado mutable del juego. Se expone mediante `rpg2lx inspect-map <Map001.json>`.

Carga dimensiones, tileset, modo de scroll, nombre, seis planos y eventos con páginas/comandos/parámetros. Conserva las demás propiedades de las páginas para el futuro intérprete; todavía no las ejecuta. No implementa colisiones ni dibuja tiles.

El índice es `(z * height + y) * width + x`: planos 0–3 de tiles, plano 4 de sombras y plano 5 de regiones. `cell` devuelve cero fuera del mapa y aplica wrap solo en los ejes configurados. Eventos nulos se omiten y cada ID se comprueba contra su posición en el array original.

Referencia de comportamiento consultada, sin copiar implementación: [Game_Map del repositorio oficial MV](https://github.com/rpgtkoolmv/corescript/blob/master/js/rpg_objects/Game_Map.js), métodos tileId, regionId, isLoopHorizontal e isLoopVertical.

Límites iniciales explícitos: entrada de mapa hasta 64 MiB, hasta 6.291.456 celdas, profundidad JSON hasta 128, dimensiones positivas hasta 10.000 y IDs de tiles hasta 8191. Proyectos que superen estos presupuestos reciben diagnóstico; estos límites no constituyen soporte universal de plugins.

Pruebas nativas cubren orden de planos, sombras/regiones, bordes, wrap por eje, comandos, tipos incorrectos, cantidad de celdas, coordenadas de eventos y archivo inexistente. Las comprobaciones siguen activas con NDEBUG.
