# Assets del DVD de PS2: mapa de paquetes y formatos

Documento de referencia para convertir el resto del juego. Describe estructuras y convenciones; no contiene datos
del juego (ni tablas, ni textos, ni listas de nombres de fichero). Todo se lee de tu ISO con las herramientas de
`tools/ps2/`.

## Herramientas

| Herramienta | Uso |
|---|---|
| `tools/ps2/assets.py` | API: `A = Assets(iso)`; `A.lista()`, `A.fichero(n)`, `A.paquete(p)`, `A.entrada(p, nombre o indice o hash)`, `A.datos(e)` (descomprimido), `A.tipo(e)`, `A.busca(glob)`, `A.elf` (`lee`, `u32`, `f32`, `cadena`, `busca`, `referencias`, `cadenas`, `secciones`, `segmentos`), `A.irx(nombre)`; lectores: `xst`, `xst_cabeceras`, `xst_imagen`, `sprani`, `sprani_xst`, `escena`, `bloque`, `texturas_bloque`, `coli`, `segmentos`, `adpcm`, `banco_ics` |
| `tools/ps2/indexa.py <iso> <indice.json>` | catalogo JSON de todas las entradas (nombre, offset, tamano, compresion, tipo y datos basicos). Unos 20 s la primera vez (reconstruye los nombres) y 1,5 s despues (cache) |
| `tools/ps2/ver.py` | `lista`, `tipos`, `info`, `hex`, `saca`, `png` (XST y modelos), `escena` (SPRANI a 640x480), `wav` (bancos .PAK y musica), `elf`, `firma`, `cadenas` |

Variables de entorno: `OR2_ISO` (ISO por defecto), `OR2_CACHE` (cache de nombres, por defecto
`~/.cache/or2_assets`), `OR2_INDICE` (indice para `ver.py`), `OR2_SALIDA` (directorio de salida de `ver.py`, por
defecto `<dir del indice>/ver`). El indice, la cache y lo que saca `ver.py` son tuyos: no van al repositorio.

Ejemplos:

```
python3 tools/ps2/indexa.py "$OR2_ISO" ~/or2/indice.json
export OR2_INDICE=~/or2/indice.json
python3 tools/ps2/ver.py tipos
python3 tools/ps2/ver.py lista coli_ -t coli
python3 tools/ps2/ver.py png 'STAGE.PS2#15'           # texturas de un modelo, hoja incluida
python3 tools/ps2/ver.py escena ani_SPRANI_ETC_CVT 40 0
python3 tools/ps2/ver.py wav 'MUSIC.PS2#3' --seg 20
```

```python
import sys; sys.path.insert(0, 'tools')
from ps2.assets import Assets
A = Assets()                                  # OR2_ISO
e = A.entrada('CARS.PS2', 1)                  # el mismo indice que usan conv_coche.py / conv_trafico.py
b = A.bloque(e)                               # stage.Bloque sobre los datos descomprimidos
for e in A.busca('*coli_CS_*'): print(e.segmento, e.nombre, A.coli(e)[0].shape)
```

## Disco y paquetes

El DVD es ISO 9660 plano: el ejecutable `SLES_539.98` (ELF MIPS, segmento unico cargado en 0x100000 desde el
desplazamiento 0x80), `SYSTEM.CNF`, `DEV9.IRX`, `DNAS310.IMG` y veinte paquetes `*.PS2`.

**Paquete `*.PS2`**: `u32 n` repetido 4 veces (16 bytes) y `n` entradas de 16 bytes
`{u32 hash, u32 desplazamiento (desde el inicio del paquete), u32 tamano, u32 tamano guardado}` ordenadas por
hash. Los datos estan alineados a 16 (los grandes a 2048) y, en desplazamiento, en el orden en que el empaquetador
recorrio el arbol de directorios (en cada directorio primero los ficheros y luego los subdirectorios, cada grupo en
orden alfabetico en mayusculas; STAGE.PS2 no sigue ese orden dentro de cada zona). El **indice** que usan
`iso.paquete()`, los `conv_*.py`, `assets.py` y el catalogo es la posicion en orden de desplazamiento.

- **Compresion**: si `tamano guardado < tamano` la entrada esta comprimida con zlib por el paquete (GHOST, GHOSTS,
  SCRIPTS). Los ficheros `*.sz` son zlib por si mismos (en el paquete figuran con el tamano comprimido).
  `Assets.datos()` deshace las dos cosas.
- **Hash del nombre** (funcion 0x170048 del ELF; en el PC no existe porque los ficheros van sueltos):
  `h = h * 131 + toupper(c)` recorriendo la ruta de atras adelante, es decir `h = sum(c_i * 131^i) mod 2^32`.
  La ruta es la relativa al directorio del paquete, con `\` delante: `\NOMBRE.EXT`, `\SUBDIR\NOMBRE`. Al ser
  lineal se pueden componer prefijos y sufijos sin recalcular (`assets._compone`).
- **Nombres**: no estan en el disco. `Assets.resolver()` los reconstruye y los guarda en la cache:
  1. cadenas del ELF (cada cola de cada ruta, con sufijos habituales: `.sz`, `_bin`, `_pmt`, `.bin`, `.pak`...);
  2. cadenas de los IRX y de los ficheros pequenos de los paquetes (tablas de nombres como la de movimientos);
  3. cadenas de formato del ELF (`%02d`, `%d`, `%s`), con `%s` sustituido por las cadenas vecinas en el ELF, o por
     todos los tokens si es la unica `%s`;
  4. directorios conocidos (componentes de rutas y vecinas de los formatos con `%s\`) delante de los nombres;
  5. plantillas: numeros y codigos de zona de los nombres hallados, y recombinacion de palabras del mismo paquete;
  6. validacion: en los paquetes ordenados se descartan los nombres fuera de la subsecuencia creciente mas larga
     (falsos positivos del hash de 32 bits).
  Cobertura actual: 2452 de 3065 entradas (80 %); con tipo reconocido el 99 %.

## Paquetes por categoria

| Categoria | Paquete | Contenido | Formatos | Conversor del port |
|---|---|---|---|---|
| Escenarios | STAGE.PS2 | un directorio por zona y variante (`ZONA` y variantes `ZONA_R`, `ZONA_T`, `ZONA_BR`/`_BT`...: sentido inverso y versiones de mision, probable); por tramo: colision, geometria de la calzada y decorado, entorno lejano, cielo, objetos colocados, niebla, sol, spline, 8 tablas SO, `oso_cs_*` | COLI0200, modelo, SO, oso, binarios | `conv_tramo.py` (geometria, texturas, ruta, superficies y muros de la colision) |
| Escenarios | COMMON.PS2 | carteles de bifurcacion por zona (`obj_OBJ_<ZONA>_BUNKI_TX_pmt`), objetos comunes y puertas, sombra de vehiculo, colores, base de objetos (`object_db`), huesos, tabla de movimientos, destellos, records por defecto, datos de curso | modelo, binarios | no |
| Escenarios | BK.PS2 | uniones entre tramos y metas: la bifurcacion (`coli_BK_PALM`, `coli_LBK/RBK/LRBK_PALM`: las dos ramas, una sola o la variante de mision; modelo `cs_BK_PALM_pmt`, el mismo para todas las bifurcaciones), las metas de los tramos finales (`coli_BK_<zona>`, `cs_BK_<zona>[_R]_pmt`) y los finales (`ENDA`-`ENDE`, `SUMOEND`); entorno `ENV\cs_ENVBK_*`, objetos animados del fondo (aviones, pajaros, barcos...: `ANIMS`), tablas SO; una colision `COLI0105` | COLI, modelo, SO, oso | `conv_union.py` (todo el arbol: bifurcaciones con sus carteles, metas, CURSO.DAT) |
| Escenarios | OSO.PS2 | `oso_qt_*` y `oso_dyn_*`: colocaciones/trayectorias (probable) | oso (sin estudiar) | no |
| Coches | CARS.PS2 | coches del jugador (`obj_plcar_*`, con variante `_t`), rivales (`obj_rc_*`, `obj_rc_all`) y trafico (`obj_othcar`) | modelo (zlib) | `conv_coche.py` (COCHEnn.DAT por ficha del menu y COCHES.DAT; montaje con la tabla de disposiciones del ELF), `conv_trafico.py` (TRAFn.DAT, TRAFC.DAT), `conv_param.py` (PARAM.DAT, del ELF) |
| Personajes | DRIVER.PS2, CHR.PS2, ANIMS.PS2 | pilotos y acompanantes (modelos), datos de personaje `CHR_*.bin`, movimientos `mot_*` (celebraciones, finales, salida) | modelo, binario, movimiento | no |
| Finales | OBJ.PS2, AS.PS2 | escenas de final (`obj_end_*`) y guiones de escena (`as_e_NN`, `as_SPE_*`) | modelo, binario | no |
| Fantasmas | GHOST.PS2, GHOSTS.PS2 | repeticiones grabadas (`gc_default_*`, `gc_RACE_*`) para contrarreloj, rivales y demo | `QHOT`/`NHOT` (zlib del paquete) | no |
| Reglas | SCRIPTS.PS2 | tablas de carreras y misiones (`Races`, `RaceAttack`, `OutrunMiles`, `Req_*` por zona y modo, metas, `csc_data_*` de cursos, camaras) | binarios (zlib del paquete) | parcialmente leidos desde el ELF por `conv_param.py`/`conv_trafico.py`; los .bin no |
| HUD y menus | SPRANI.PS2 | animaciones 2D `ani_SPRANI_*` (HUD, menus, carga, ranking, finales, tacometros por coche) | SPRANI (zlib) | `conv_hud.py` (HUD de carrera); menus: `conv_menu.py` (en curso) |
| HUD y menus | SPRITE.PS2 | texturas `spr_*xst` de cada SPRANI (variantes por idioma `_E/_F/_G/_I/_S` y `J`), fuente, seleccion, ranking, entrada de nombre | XST (zlib, LZ dentro) | idem |
| Textos | TEXT.PS2 | un fichero por idioma | `txet` | no |
| Sonido | SOUND.PS2 | bancos `.PAK`: efectos del coche (ICS), interfaz, resultados, finales, objetos del fondo; voces por idioma (`\IDIOMA\...`: locutor, misiones, ranking...) | banco ICS, banco SE | `conv_sonido.py` (ICS: motor, derrape, golpe; SE: efectos de menu de MENU.pak y Fe.pak) |
| Musica | MUSIC.PS2 | una pista por fichero `\NN.ps2` | PS-ADPCM estereo | `tools/c/musica.c` (a ADPCM de Yamaha) |
| Video | MV.PS2 | videos de titulo y demo (50 y 60 Hz) | PSS (MPEG-2 + audio) | no |
| Sistema | SYSTEM.PS2 | modulos del IOP (`\IRX\*`), iconos de tarjeta, DNAS | IRX, icono | `conv_sonido.py` lee la tabla de muestras de `\IRX\SOUNDCD.IRX` |

## Formatos

**Modelo / bloque de geometria** (`*_pmt`, `*_pmt.sz`; STAGE sin comprimir, el resto zlib). Cabecera
`{u32 n registros, u32 n texturas, u32 tamano A, u32 tamano B}` + A + B. A: registros de 72 bytes (8 tablas
`(cuenta, desplazamiento)` y la marca `87658765 12341234`), tablas y paquetes DMA/VIF de geometria; B: texeles.
Tabla 0 objetos de 56 bytes (esfera), 3 mallas de 24 bytes (tercer word = paquete DMA en A), 4 de 36 bytes.
Mallas: GIFtag, posiciones V3-32, normales V3-16 (/32768), UV V2-16 (/4096) o V2-32, tiras de hasta 16 vertices,
STROW/STMOD (las UV pueden ir en diferencias). Texturas: tabla de desplazamientos a cabeceras de 56 bytes (datos,
paleta, qwords, `log2 ancho/psm`, `log2 alto`, mips); PSMCT32, PSMT8 subido como PSMCT32, PSMT4 como PSMCT16,
paletas CSM1, alfa 0x80 = opaco. Codigo: `ps2/stage.py`, `ps2/vif.py`, `ps2/tex.py`, `ps2/gs.py`, `modelo.py`.
Su equivalente en el PC es el `.pmt` de `pc_pmt.hpp`/`pc_pmt.cpp` (codificacion de vertices distinta).

**COLI0200** (colision): `COLI0200`, `u32 n poligonos` (+8), poligonos de la calzada (+12), cabecera (+16), 8
secciones (+20): rejilla, material por poligono (indice en los 5 bits bajos), poligonos de 64 bytes (4 vertices,
centro, banderas en +60), normales, banderas extra, longitud de carretera por poligono. `stage.coli`,
`Assets.coli`. PC: `platform/pc_coli0200.cpp`, `course_collision_pack.cpp` (0x43DCD2). BK tiene ademas una version
`COLI0105` sin estudiar.

**SO** (8 por tramo en STAGE, varias en BK): `'SO'`, `s16 n`, `n x {s16, s16, s16}` (hay alguna con datos detras).
La lee la funcion 458460 del PC (`bulk_tr.cpp`, comprueba la marca 0x4F53 y accede a `+4 + i*6`). Sin nombre en el
ELF ni en el PC; contenido sin estudiar (desplazamientos por punto de la ruta, probable).

**XST** (texturas 2D, `spr_*xst.sz`): `u32 tamano`; cabecera relativa a +4 `{0, meta, n tex, 0, n grupos,
grupos*, n imagenes, imagenes*}`; en `+4+meta` desplazamientos a cabeceras de textura como las de los modelos; bit
0x8000 de la palabra 11 = texeles con LZ (rutina 0x165788: bandera 1 = 12 bits de distancia + 4 de longitud+3;
distancia 0 = relleno). Los PSMT8/PSMT4 se suben como un rectangulo PSMCT32/PSMCT16 de la mitad de ancho y alto
con ancho de bufer = ancho / 128 (minimo 1); la palabra 2 de la cabecera no es ese ancho. Imagen 0x1C
`{textura, f32 u0 v0 u1 v1, u16 izq arr der abj}`. `conv_hud.texturas`, `conv_hud.imagen_xst`.

**SPRANI** (`ani_SPRANI_*.sz`): animaciones 2D por capas (estilo After Effects): tabla de escenas, componentes
0x24, capas 0x4C con 6 canales de Hermite, footage 0x18 con cuadros 0x14 (textura y rectangulo). Detalle completo
en la cabecera de `tools/conv_hud.py`. Token de escena = `banco << 16 | escena` (bancos desde 0x20 en el orden de las
cadenas `\SPRANI\ani_*` del ELF). Su XST es `spr_sprani_<clave>_[E]xst.sz` (`Assets.sprani_xst`). PC:
`frontend_sprites.cpp` (427F70/4282B0), `pc_sprite_2d.cpp`.

**Bancos de sonido `.PAK`**: cabecera de 16 bytes `{u32 tamano A, u32 tipo (1 = banco ICS), u32 tamano B, 0}`,
A = PS-ADPCM (SPU2) desde +16 (el primer bloque es mudo), B = segundo bloque ADPCM (opcional) y al final una tabla
`{u32 n, n x {u32 id, u32 desplazamiento}}` (en los bancos SE). ICS: 68 muestras en bucle; su tabla de
desplazamientos y pitch esta en el `.data` de `\IRX\SOUNDCD.IRX` (0x66a0) y las definiciones (capas, curvas) en el
ELF (0x3526a4); ver la cabecera de `tools/conv_sonido.py`. Rutas `\Sound\%s\%s` (idioma y fichero) en el ELF.

**Musica `\NN.ps2`**: PS-ADPCM estereo sin cabecera, intercalado por bloques de 0x2000 bytes (izquierdo, derecho,
...); los bloques pares de cada canal terminan con la marca 3 (doble bufer de la SPU). 48 kHz (probable).
`tools/c/musica.c`, `comun/musica.c`; `ver.py wav`.

**PSS** (MV.PS2): MPEG-PS de Sony (`00 00 01 BA`), video MPEG-2 y audio en paquetes privados. El PC usa Bink
(`system/title_movie.cpp`).

**Texto `txet`**: `'txet'`, `u32 tamano`, tabla de desplazamientos y cadenas (cada caracter en 16 bits; el byte
bajo es el codigo de la fuente). PC: `frontend_text.cpp` (465DF0, 42CFE0). Los numeros de texto del PS2 son los del
PC menos 1 en la zona de la barra de ayuda. **Fuentes**: `spr_font_xst` (textura n = fuente n, celdas de 16 por
fila) y, en el ELF, una tabla de 15 punteros a registros `{id, metricas*, interletraje*, u16 ancho, alto, primer
caracter, i32 espaciado x, y, u16 n}` (metricas: 2 bytes por caracter, desplazamiento y ancho; interletraje n x n
bytes). `conv_menu.Fuente`, `conv_menu.textos` (barra de ayuda de los menus).

**Fantasmas `.rec`** (`QHOT`/`NHOT`): repeticiones; nombres `gc_default_<old|cvt>_<nml|tn1|tn2>..._NN_N[r].rec`.
PC: `race_ghosts.cpp` (465F30..4687C0), `pc_car_services_ghost.cpp`.

**Scripts** (`\bin\*.bin`, zlib del paquete): tablas binarias de carreras, misiones y cursos. PC:
`course_asset_pack.cpp` (`csc_data_cvt.bin`), `frontend_requests.cpp`, `mission_manager.cpp`.

**object_db** (COMMON): nombres de objeto a manejadores `recurso << 16 | objeto`. PC: `object_db.cpp` (448B90).

**IRX**: ELF del IOP (`e_type` 0xFF80); nombre del modulo en `.iopmod` + 0x1A (`Elf.modulo()`).

**Descriptores de tramo y de union** (ejecutable; el PC hace lo mismo en `runtime_apply_course_data_44d720`): una
tabla de 66 punteros a descriptores de tramo de 0xb0 bytes seguida de la de 77 descriptores de union. Tramo: +0x00
su indice, +0x0c ruta de la colision (`\Stage\ZONA\coli_CS_*`), +0x20.. el resto de sus ficheros, +0x98 colocacion
de su union respecto al tramo (traslacion x, y, z y giros x, y, z; en el PC +0x80, matriz 7D3190). Union: +0x0c
colision de las dos ramas, +0x18 / +0x1c la de la rama izquierda / derecha. Todos los tramos empiezan en el origen
hacia -z; la union tambien, y sus ramas acaban en (+-1207,6, 0, -987,6) hacia -z, donde empieza el tramo siguiente.
**Arbol del curso** (`\bin\csc_data_2.bin` para OutRun 2, `_cvt` el de Coast 2 Coast, `_easy`; zlib del paquete):
categoria reubicable (`u32` categorias, `u32` reubicaciones, indice de 12 bytes `{hash, cuenta, desplazamiento}`) y
registros de 0x78 bytes: +0x04 clave, +0x1c descriptor de tramo, +0x20 de union, +0x2c / +0x30 clave del hijo
izquierdo / derecho (-1 en las metas); el +0x08 (nivel, que lee 44C940 para la tabla de tiempos) vale 0 en todos:
la demo usa la profundidad en el arbol (0..4). `conv_union.py` los lee.

**Niebla** (`scn_env_fog_<zona>_bin`, STAGE): tres desplazamientos `u32` (listas 0, 1 y 2: entornos del original;
la 1 es la de la carretera) a registros de 0xb0 bytes terminados en 0xffff (0xfffe: hueco), como los lee
`course_environment_progress_44b020` del PC: +0x00 / +0x10 posicion de carretera inicial / final, +0x34 modo (1 exp,
2 exp2, 3 lineal), +0x38 inicio, fin y densidad, +0x48 color RGB, +0x54 / +0x64 los mismos al final del tramo. La
posicion de carretera es la seccion 5 de la colision: una cada ~7,9 m.

**Materiales** (tabla 5 de cada registro de modelo, 64 bytes): difuso r, g, b, a en unidades del GS (128 = 1),
ambiente, especular, potencia, emision. El primer campo de la entrada de la tabla 4 (36 bytes) es el material y el
cuarto la textura. `modelo.material_de`.

**Carteles de bifurcacion**: `COMMON obj_course_obj_bk_<zona>` (en el disco solo el de PALM) son los porticos; sus
texturas 0-3 son huecos de 8x8 que se llenan con las de `obj_OBJ_<ZONA>_BUNKI_TX` de cada destino (icono, nombre con
flecha a la izquierda, nombre con flecha a la derecha). Las metas llevan la pancarta GOAL en el principio de
`cs_BK_<zona>` (z = 0).

**Ejecutable**: `Assets.elf` lee por direccion virtual. Tablas ya localizadas (por direccion o por firma) en los
conversores: parametros de coches (`conv_param.py`), trafico (`conv_trafico.py`), HUD (`conv_hud.py`: 0x404818,
0x404478, 0x404a70), ICS y rpm (`conv_sonido.py`).

## Estado del port y lo que falta por convertir

Hecho: un tramo jugable con geometria, texturas, ruta y colision del original (`conv_tramo.py`), coche del
jugador y trafico (`conv_coche.py`, `conv_trafico.py`), fisica y choques con las tablas del ELF (`conv_param.py`),
HUD de carrera y rotulos (`conv_hud.py`), efectos ICS (`conv_sonido.py`), musica por streaming (`musica.c`); la
arbol entero del modo OutRun (15 etapas, 10 bifurcaciones con sus carteles, 5 metas, niebla de cada etapa) encadenado
mientras se conduce (`conv_union.py`, `CURSO.DAT`).

Falta, por orden aproximado de necesidad para un juego completo:

1. **Resto de tramos y cursos**: los tramos de Coast 2 Coast (`csc_data_cvt`) y las variantes inversas/de mision,
   `Req_*` de SCRIPTS, puertas de COMMON, entorno lejano (`cs_ENV_*_pmt`), objetos de tramo
   (`obj_course_obj_cs_*`, `oso_cs_*`), sol (`scn_env_sun`), objetos animados de BK (`ANIMS`), spline (`maya_spl`)
   en lugar de la ruta deducida, tablas SO (sin estudiar).
2. **Coches**: hechos los 15 del modo OUTRUN (`conv_coche.py --todos`) con sus materiales, colores y reflejo;
   faltan las variantes `_t` (modelos 15-29), luces de freno, rivales `obj_rc_*`, sombra (`obj_vshadow`).
3. **Menus y textos**: resto de SPRANI/XST (titulo, seleccion de coche y musica, carga, ranking, entrada de nombre,
   finales) en `conv_menu.py`, y los textos `txet` por idioma (TEXT.PS2) con la fuente `spr_font_xst`.
4. **Modos**: reglas de Coast 2 Coast/misiones (`Races`, `RaceAttack`, `OutrunMiles`, metas), records por defecto,
   fantasmas para contrarreloj.
5. **Sonido**: bancos SE (interfaz, resultados, finales, objetos del fondo), voces por idioma; el resto de pistas
   de musica (solo hace falta pasar cada `\NN.ps2` por `musica.c`).
6. **Finales y personajes**: OBJ (`obj_end_*`), AS, DRIVER/CHR/ANIMS (modelos con huesos y movimientos), BK
   (fondos y objetos animados).
7. **Videos**: PSS a un formato reproducible en Dreamcast (o prescindir de ellos).

Sin identificar (nombre): las tablas SO de STAGE/BK, cinco fantasmas de GHOSTS, dos coches de CARS, un piloto de
DRIVER, dos scripts, una entrada de COMMON y otra de SOUND. Sin estudiar (formato): SO, oso, AS, movimientos,
`CHR_*.bin`, COLI0105, fantasmas, scripts, segunda parte y tabla final de los bancos SE.
