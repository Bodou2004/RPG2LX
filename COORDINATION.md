# RPG2LX — coordinación Codex ↔ Antigravity

Este archivo es el registro compartido del proyecto. No es un canal automático: cada IA debe leerlo al iniciar y agregar su respuesta al terminar una intervención.

## Protocolo

- Agregar entradas al final; conservar el historial y atribuir cada mensaje a su autor real.
- Formato: ID, fecha con zona, autor, destinatario, estado, mensaje, archivos, validación y próximo paso.
- Antes de editar, revisar cambios locales y anunciar aquí el alcance. Evitar editar los mismos archivos simultáneamente.
- Codex se ocupa inicialmente de preparación y coordinación; Antigravity, del primer hito de implementación. Los cambios de reparto deben quedar escritos.
- Distinguir solicitado, en curso, implementado, probado y bloqueado. No atribuir respuestas a la otra IA.
- No guardar credenciales ni datos privados. Documentar desacuerdos y evidencia antes de cambiar arquitectura.

## Mensajes

### C001 | 2026-10-29 America/Buenos_Aires | Codex → Antigravity

Estado: pendiente de recepción.

El usuario solicita trabajar juntos en RPG2LX, runtime general para RPG Maker MV en R36S y otras consolas Linux usando PortMaster. Esta carpeta es la raíz del repositorio local. Leer README.md, docs/PROJECT_BRIEF.md y PERPLO_MV_MASTER_RUNTIME_ANTIGRAVITY.md.

Encargo: implementar el primer hito descrito en PROJECT_BRIEF (fases 0 y 1), investigar integración oficial con PortMaster y responder aquí con resultados reales. No empezar renderizado antes de consolidar inspector y pruebas. El nombre del binario nuevo será rpg2lx.

Archivos preparados por Codex: README.md, .gitignore, docs/PROJECT_BRIEF.md y este registro. Especificación original conservada.

Validación: Git inicializado; no se ha compilado código porque aún no existe implementación. Sin pruebas de dispositivo.

Próximo paso: Antigravity agrega A001 confirmando recepción y alcance antes de implementar. Codex revisará cambios y resultados cuando estén disponibles.
