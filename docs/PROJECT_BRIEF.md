# RPG2LX — encargo vigente

Fecha: 2026-10-29 (America/Buenos_Aires).

## Objetivo

Ejecutar proyectos RPG Maker MV en R36S y otras consolas Linux de bolsillo. PortMaster será la plataforma de empaquetado y distribución. RPG2LX es el nombre vigente; PERPLO MV MASTER RUNTIME es el nombre de la especificación anterior.

## Base arquitectónica

Consultar la especificación original antes de implementar. Base propuesta: C++17, CMake, SDL2, OpenGL ES y VM JavaScript encapsulada. Verificar decisiones con documentación primaria y pruebas; no presentar propuestas como implementaciones.

## Primer hito encargado a Antigravity

1. Inspeccionar esta carpeta y leer COORDINATION.md y la especificación original.
2. Implementar fases 0 y 1: estructura, CMake, CLI rpg2lx --version/--help y rpg2lx inspect <proyecto>, logging y detección con datos reales.
3. Agregar fixtures propios y pruebas de proyectos válidos, incompletos y JSON inválido. No ejecutar plugins durante la inspección.
4. Documentar arquitectura, compatibilidad y estado. Compilar y reportar pruebas efectivamente ejecutadas.
5. Investigar requisitos actuales de PortMaster en fuentes oficiales; documentar launcher, estructura del paquete, controles, dependencias, logs, guardados y compilación ARM. No declarar un paquete listo sin validarlo.
6. Registrar respuesta, cambios, pruebas, limitaciones y siguiente paso en COORDINATION.md.

## Validación posterior

Inspector → arranque → mapa vanilla → eventos → audio/controles → guardado → batalla/menús → plugins → paquete PortMaster y prueba en hardware.

Mantener matriz por función/plugin/juego, arquitectura y dispositivo. No prometer que todos los juegos funcionan. No incluir juegos comerciales, assets privados ni código propietario en Git. No publicar ni elegir licencia definitiva sin decisión del propietario.
