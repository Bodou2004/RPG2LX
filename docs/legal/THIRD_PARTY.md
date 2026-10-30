# Dependencias del menú

- SDL2 2.32.10: [release oficial](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10), licencia zlib. El SDK Windows descargado para desarrollo queda dentro de build/deps y no se versiona. Un paquete distribuido debe conservar la licencia de la biblioteca que incluya. No se incorporó código de RPG Maker al launcher.
- font8x8, Daniel Hepper y colaboradores: [repositorio original](https://github.com/dhepper/font8x8). Los archivos font8x8_basic.h y font8x8_ext_latin.h declaran dominio público; se conservan sus cabeceras y README. Modificación local: arrays convertidos de char a static const unsigned char para C++17 y almacenamiento de solo lectura. Fuente disponible: ASCII y Latin-1.

No se eligió todavía una licencia general para RPG2LX ni se publicó un paquete. Las dependencias conservan sus términos propios.

- Corescript oficial RPG Maker MV: tablas de autotiles y fórmulas de layout adaptadas de Tilemap.js, copyright (c) 2015 KADOKAWA CORPORATION./YOJI OJIMA, licencia MIT. Aviso completo en MV-CORESCRIPT-LICENSE.txt. El resto del repositorio no se incorporó al runtime.
- stb_image.h de [nothings/stb](https://github.com/nothings/stb): decodificador compilado solo para PNG, licencia MIT o dominio público a elección, ambas incluidas al final del header original en third_party/stb. No se usan sus funciones de apertura de archivos; RPG2LX lee con paths de filesystem para soportar Unicode.
