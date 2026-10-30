# RPG2LX — Investigación oficial de integración con PortMaster

Fecha: 2026-10-30
Autor: Antigravity
Estado: Hito 0/1 — Plantillas técnicas de integración e investigación de fuentes oficiales.

---

## 1. Fuentes Oficiales Consultadas

La investigación y diseño de los archivos de integración se basan directamente en la documentación técnica oficial de PortMaster y HarbourMaster:

1. **Estructura y Empaquetado oficial (Packaging / New Port Structure):**  
   - Enlace oficial: [https://portmaster.games/packaging.html](https://portmaster.games/packaging.html)
   - Guía de arquitectura de directorios de ports y estándares de nombres.
2. **Especificación de `port.json` (HarbourMaster metadata schema):**  
   - Enlace oficial: [https://portmaster.games/port-json.html](https://portmaster.games/port-json.html)
   - Especificaciones de atributos v2/v4 (`version`, `items`, `attr.title`, `attr.inst`, `attr.rtr`, `attr.reqs`).
3. **Plantilla estándar de Scripts de Lanzamiento (`control.txt`, `gptokeyb`):**  
   - Enlace oficial: [https://portmaster.games/port-scripting.html](https://portmaster.games/port-scripting.html)
   - Ciclo de vida: Detección de ruta de PortMaster, `source control.txt`, ejecución de `get_controls`, hook de `gptokeyb`, helper de plataforma y `pm_finish`.
4. **Mapeo de mandos con Gamepad-To-Keyboard (`gptokeyb`):**  
   - Repositorio oficial: [https://github.com/PortsMaster/gptokeyb](https://github.com/PortsMaster/gptokeyb)

---

## 2. Estructura Oficial del Paquete (New Port Structure)

PortMaster requiere aislar cada port en una carpeta contenedora en el árbol de ports. Para un port llamado `rpg2lx`:

```text
rpg2lx/
├── port.json              (Metadatos de HarbourMaster versión 4)
├── README.md              (Instrucciones, notas y créditos)
├── screenshot.jpg         (Captura para la interfaz de PortMaster)
├── cover.jpg              (Opcional: carátula)
├── gameinfo.xml           (Metadatos para EmulationStation / temas)
├── RPG2LX.sh              (Script de lanzamiento ejecutable)
└── rpg2lx/                (Directorio de runtime y datos del juego)
    ├── rpg2lx.aarch64     (Binario nativo compilado para ARM64)
    ├── rpg2lx.armhf       (Binario nativo opcional para 32-bit ARM)
    ├── rpg2lx.gptk        (Configuración de mapeo gptokeyb)
    ├── libs.aarch64/      (Librerías compartidas si no están en el SO/squashfs)
    ├── conf/              (Directorio de configuración/saves XDG_DATA_HOME)
    ├── gamedata/          (Archivos del juego MV provistos por el usuario)
    └── licenses/          (Licencias de dependencias y código)
```

---

## 3. Requisitos de `port.json` (Especificación v4)

HarbourMaster requiere un archivo JSON estricto con los siguientes campos:
- `version`: `4` (o `2`).
- `name`: Nombre del archivo comprimido (`rpg2lx.zip`).
- `items`: Lista de archivos base desempaquetados en `/ports/` (`["RPG2LX.sh", "rpg2lx/"]`).
- `attr.title`: Nombre legible en PortMaster.
- `attr.desc`: Descripción concisa.
- `attr.inst`: Instrucciones para el usuario sobre dónde colocar assets ("Coloca los archivos de RPG Maker MV en `ports/rpg2lx/gamedata`").
- `attr.rtr`: `false` (Ready-To-Run es falso, ya que los juegos RPG Maker MV son provistos por el usuario).
- `attr.runtime`: Si usa un squashfs global provisto por PortMaster o `null` si se suministran binarios propios con `libs.aarch64`.
- `attr.reqs`: Requisitos de hardware/gráficos, p. ej. `["opengl"]`.

---

## 4. Launcher Script y Ciclo de Vida (`control.txt` y `gptokeyb`)

El script de lanzamiento (`RPG2LX.sh`) implementa el ciclo canónico documentado en `port-scripting.html`:
1. Detecta `controlfolder` en `/opt/system/Tools/PortMaster/`, `/opt/tools/PortMaster/`, `$XDG_DATA_HOME/PortMaster/` o `/roms/ports/PortMaster`.
2. Ejecuta `source $controlfolder/control.txt`.
3. Aplica parches por firmware si existen (`mod_${CFW_NAME}.txt`).
4. Invoca `get_controls` para obtener variables de entorno (`$directory`, `$ESUDO`, `$ANALOGSTICKS`, `$DEVICE_ARCH`, `$sdl_controllerconfig`).
5. Redirige logs a `log.txt`:
   `> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1`
6. Redirige `$XDG_DATA_HOME` a `$GAMEDIR/conf/` para aislar partidas guardadas.
7. Lanza `$GPTOKEYB` en segundo plano con el archivo `.gptk` correspondiente.
8. Ejecuta `pm_platform_helper` con el binario antes de la ejecución.
9. Lanza el ejecutable principal.
10. Llama a `pm_finish` al salir.

> [!NOTE]
> **Distinción importante:** El script actual [`portmaster/RPG2LX.sh`](file:///C:/Users/herre/OneDrive/Escritorio/PERPLO%20STUFFS/IA/RPG-RUNTIME/portmaster/RPG2LX.sh) es una **plantilla técnica de inspección**, no un launcher de runtime de juego funcional. En esta fase ejecuta `./rpg2lx.${DEVICE_ARCH} inspect ./gamedata` para validar proyectos in situ. Cuando el runtime gráfico y de eventos esté desarrollado en fases posteriores, el script cambiará a `./rpg2lx.${DEVICE_ARCH} run ./gamedata`.

---

## 5. Controles y Mapeo en RPG Maker MV (`.gptk`)

Los juegos RPG Maker MV estándar usan controles de teclado/gamepad HTML5:
- Confirmar / Acción: `Z`, `Enter`, `Space`
- Cancelar / Menú: `X`, `Escape`, `NumPad 0`
- Dash (Correr): `Shift`
- Navegación: Flechas direccionales
- Paginación: `PageUp` (L1), `PageDown` (R1)

El archivo `.gptk` mapea los botones físicos de la consola a las teclas esperadas por el motor o maneja directamente SDL2 GameController API.

---

## 6. Compilación ARM64 (RK3326 / R36S)

- **Arquitectura:** `aarch64` (ARMv8-A Cortex-A35 en RK3326 con distribuciones de 64 bits como ROCKNIX/AURKNIX o `armhf` en firmwares antiguos de 32 bits).
- **Toolchain recomendado:** GCC cross-compiler `aarch64-linux-gnu-g++` o compilación dentro de contenedor Docker con sysroot de PortMaster.
- **Gráficos:** OpenGL ES 2.0 / EGL (Mali-G31 MP2). El runtime debe usar GLES 2/3 y SDL2 compilado con soporte KMS/DRM o X11 según el frontend del SO.

---

## 7. Estado y Advertencia de Validación

> [!WARNING]
> Ningún paquete PortMaster debe marcarse como "listo para distribución" o "funcional" hasta que se haya compilado para ARM64 (`aarch64`) y se haya probado físicamente en una consola (R36S / RK3326) con firmware activo (ROCKNIX/AURKNIX/ArkOS). Por el momento, la estructura y scripts se encuentran en estado de plantilla técnica de integración para el hito 0/1.
