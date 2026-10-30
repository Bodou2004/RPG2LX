# Compilar y probar RPG2LX

## Entorno C++ habitual

Se requiere CMake >= 3.16 y compilador C++17. No hay dependencias externas del núcleo actual.

```sh
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

En generadores con varias configuraciones, usar `--config Debug` para build y `-C Debug` para CTest. Si hay Python 3 disponible, CTest incorpora las pruebas de caja negra del binario compilado.

## Herramientas locales en Windows sin compilador instalado

Codex preparó herramientas dentro de `build/` (ignoradas por Git). Para reconstruir ese entorno:

```powershell
python -m pip install --target build/tooling-stable ziglang==0.13.0
python tools/build_native.py --zig build/tooling-stable/ziglang/zig.exe
python tools/build_native.py --zig build/tooling-stable/ziglang/zig.exe --release
```

El script compila fuentes C++ reales y ejecuta sus pruebas; Python solo coordina los procesos. Logs y ejecutables quedan en `build/native-debug/` o `build/native-release/`. Las primeras compilaciones de Zig preparan su biblioteca C++ y pueden tardar varios minutos. Las herramientas no deben incluirse en el paquete PortMaster.

Ejecutables: `rpg2lx.exe`, `test_inspector.exe`, `test_json.exe` y `test_map.exe`. Las pruebas CLI también se pueden ejecutar con:

```powershell
python tests/test_cli_native.py build/native-debug/rpg2lx.exe
```

## Alcance

Una build Windows verifica el núcleo portable, no ABI ARM, gráficos, controles ni integración PortMaster. Compilación ARM con sysroot/firmware adecuado y prueba en consola siguen pendientes. No usar el antiguo prototipo `tools/rpg2lx_ref.py` para certificar cambios C++.
