# RPG2LX — Matriz de Compatibilidad RPG Maker MV

Fecha: 2026-10-30
Estado: Hito 0/1 (Inspector de Proyectos)

> [!IMPORTANT]
> RPG2LX se encuentra actualmente en Fase 1 (Inspector y CLI). **No existe aún un runtime jugable ni renderer activo**. La presente matriz detalla el nivel de soporte del inspector estático frente a las especificaciones de RPG Maker MV v1.6.x.

---

## 1. Archivos de Datos de RPG Maker MV (`data/*.json`)

| Archivo MV | Obligatorio en MV | Estado en Inspector RPG2LX | Notas |
| :--- | :---: | :---: | :--- |
| `data/System.json` | Sí | **Soportado (Parseo Estricto)** | Extrae `gameTitle`, `startMapId`, valida objeto raíz y sintaxis JSON. |
| `data/MapInfos.json` | Sí | **Soportado (Parseo Estricto)** | Valida raíz tipo array y cuenta mapas definidos. |
| `data/Actors.json` | Sí | **Soportado (Parseo Estricto)** | Valida raíz tipo array y cuenta actores en base de datos. |
| `data/CommonEvents.json` | Sí | **Soportado (Parseo Estricto)** | Valida raíz tipo array y cuenta eventos comunes. |
| `data/MapXXX.json` | Depende del mapa inicial | **Inspección de Existencia** | `valid_project` incluye `Map001.json` como fixture mínima. |
| `data/Enemies.json` | Opcional en intro | Pendiente Fase 2 | No evaluado en Fase 1. |
| `data/Troops.json` | Opcional en intro | Pendiente Fase 2 | No evaluado en Fase 1. |
| `data/Items.json` | Opcional en intro | Pendiente Fase 2 | No evaluado en Fase 1. |
| `data/Skills.json` | Opcional en intro | Pendiente Fase 2 | No evaluado en Fase 1. |
| `data/Weapons.json` | Opcional en intro | Pendiente Fase 2 | No evaluado en Fase 1. |
| `data/Armors.json` | Opcional en intro | Pendiente Fase 2 | No evaluado en Fase 1. |
| `data/Animations.json`| Opcional | Pendiente Fase 2 | No evaluado en Fase 1. |

---

## 2. Configuración de Plataforma y Plugins

| Componente | Estado | Detalle |
| :--- | :---: | :--- |
| `package.json` | **Soportado** | Extrae título alternativo y resolución de pantalla (`window.width`, `window.height`). |
| `js/plugins.js` | **Soportado (Estático)** | Extrae nombre, estado activo (`status`), descripción y diccionario de parámetros (`parameters`). **Aislamiento:** No se evalúa código JS en el inspector. |
| Scripts core (`rpg_core.js`, `rpg_managers.js`, etc.) | Pendiente VM | En RPG2LX, la lógica core se implementará nativamente en C++ para evitar el sobrecoste de NW.js. |

---

## 3. Dispositivos y Plataformas Objetivo

| Plataforma / Dispositivo | Arquitectura | Estado | Notas |
| :--- | :--- | :---: | :--- |
| **PC Host (Linux/Windows)** | x86_64 / x86 | En desarrollo (C++17) | Entorno primario para desarrollo y pruebas de integración. |
| **R36S / RK3326 (ROCKNIX/AURKNIX)** | ARM64 (`aarch64`) | Pendiente toolchain/hardware | Target prioritario para PortMaster. |
| **Otras consolas PortMaster** | ARM64 / `armhf` | Pendiente validación | Requiere paquete estandarizado y prueba física. |

> [!WARNING]
> No se promete compatibilidad universal con todos los juegos comerciales o plugins complejos de RPG Maker MV (por ejemplo, plugins que dependan de WebGL avanzado, llamadas arbitrarias a `require('nw.gui')` o manipulación pesada del DOM de Chromium). Cada plugin requerirá un shim nativo o aislamiento progresivo.
