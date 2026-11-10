# Dreamcast real: lo que Flycast no ensena

Notas para que la demo funcione en una consola de verdad (GDEMU, GD-ROM), no solo en el emulador. Flycast no se
cuelga cuando el TA desborda sus buferes, no lleva los registros de uso del TA, no cuesta lo mismo de CPU y no tiene
los tiempos de busqueda de un lector de GD. Las cifras de este documento son de nuestros propios datos convertidos;
no contienen nada del juego.

## 1. Memoria de video (8 MB)

KallistiOS reparte la VRAM asi (`pvr_buffers.c`): dos juegos (doble bufer: el TA llena uno mientras la PowerVR
dibuja el otro), cada uno con

| Parte | Tamano por juego |
|---|---|
| bufer de parametros (vertices), `TA_VB_BYTES` | 1792 KB |
| OPB: 300 teselas x (opacos 32 + modificadores opacos 8 + translucidos 8 + recorte 16 palabras) = 76,8 KB, por 1 + 4 de reserva | 384 KB |
| matriz de teselas | 7 KB |
| imagen 640x480 a 16 bits | 600 KB |
| **total** | **~2783 KB** |

Texturas: 8 MB - 2 x 2783 KB = unos 2,4 MB (antes, con 1 MB de vertices y 5 OPB de reserva, 3,8 MB).

### Texturas VQ

`tools/vq.py` comprime en VQ de la PowerVR las texturas de la etapa, el coche y el trafico (`tools/modelo.py`, formato
ORD6): libro de hasta 256 entradas de 4 texeles de 16 bits (2 KB) + 1 byte por bloque de 2x2 (1/8 de la textura de
16 bits mas el libro). Detalles:

- Orden: el de la textura twiddled de 16 bits; cada 4 texeles seguidos forman un bloque de 2x2 (tambien en las
  rectangulares), como hace `pvrtex` de KallistiOS.
- Libro reducido para las de menos de 64x64 (N entradas, las ultimas): la textura se direcciona `2048 - 8 N` bytes
  antes de sus datos (`sube_modelo` en `demo/main.c`; mismo truco que el formato .DT de pvrtex).
- Codificador: k-means ponderado (numpy) sobre los vectores distintos, arranque k-means++, tope de iteraciones y de
  tiempo; el libro se cuantifica al formato y se reasigna. Las de menos de 32x32 y las que superan un error RMS de 9
  (escala 0-255, frente a la misma textura en 16 bits; `VQ_UMBRAL`) se quedan sin comprimir. `VQ=0` lo desactiva
  (cabecera ORD5); `VQ_PNG=dir` guarda cada textura antes y despues.
- `conv_tramo.py` ademas descarta las texturas que no usa ninguna malla.

| Fichero | Antes | Despues |
|---|---|---|
| SEG00.DAT | 1801 KB | 496 KB |
| SEG03.DAT | 2541 KB | 644 KB |
| SEG07.DAT | 2398 KB | 563 KB |
| COCHE.DAT (el de antes: era el 512 BB) | 439 KB | 133 KB |
| COCHE00-14.DAT (texturas de cada uno) | | 86-151 KB |
| TRAF0-8.DAT (suma) | 573 KB | 140 KB |

Error RMS medio 4,4-4,8 (maximo 9). VRAM libre medida (Flycast, `DIAG_VTX`, con el coche de 17 texturas): en
carrera 845 KB con SEG00 y la union (BKI.DAT, 118 KB de texturas) ya subida, 1347 KB al liberar SEG00 y 697-715 KB
con la union y la etapa siguiente (SEG03 644 KB, SEG05 626 KB); en los menus la cache de pantallas
(`comun/menu.c`) ocupa lo que haya libre y echa las pantallas viejas.

### RAM y cargas durante la carrera

El curso entero (15 etapas, 10 bifurcaciones y 5 metas, `CURSO.DAT`) se lee mientras se conduce (`curso_paso` en
`demo/main.c`): un hilo lee el fichero del GD y lo coloca en el marco de la carrera (`modelo_coloca`) y el bucle sube
las texturas al terminar. Nunca hay mas de dos a la vez: etapa + union hasta UNION_LIBERA (300 m) dentro de la union
(y con la rama elegida), y union + etapa siguiente despues. Las etapas van siempre en el mismo hueco fijo de RAM (el
tamano del mayor SEGnn.DAT: 7842 KB, SEG13, desde la PVS y el plano del agua) y la union en otro (el mayor BKkk.DAT: 1480 KB),
reservados al arrancar. Fuera de la carrera el hueco de la union se presta (ver RAM del arranque y del menu, abajo).
La ruta es una ventana: al soltar una etapa se quitan sus puntos (`ruta_recorta`) y el marco se recentra en el
principio de la union (`curso_recentra`), asi que ni la RAM de la ruta ni las coordenadas crecen de etapa en etapa.
La colision del suelo (COLnn.DAT, PARIDAD.md Suelo del original) va en el hueco de la etapa detras de su SEGnn.DAT,
en lo que le sobra (el hueco no crece: 7842 KB); la de METR (COL13, 208 KB), que no cabe detras de SEG13, y la de la
union (el mayor COLBK*, 184 KB) van en dos huecos que solo existen en la carrera (`col_reserva` en `curso_reinicia`,
`col_suelta` en `curso_al_menu`): reservarlos al arrancar dejaba sin RAM las tablas de MENU.DAT (~1 MB, "Out of
memory" y menus en gris). Medido (Flycast, menu real): al arrancar 823 KB libres como antes; al empezar la carrera, con
los dos huecos, 450 KB libres.
BKD.DAT (la ruta de la rama derecha de cada bifurcacion, 105 KB) va en lo que sobra del hueco de la etapa detras de
su SEGnn.DAT y su COLnn.DAT (`der_sitio`; cabe en todas las etapas con bifurcacion, la de mas: SEG48 deja 644 KB):
del monton, con ~250 KB libres pero troceados, no habia 108 KB seguidos y la carrera se paraba al final de PALM (s
~2410, ~27 s desde la salida: "Out of memory" al leer BKD.DAT tras BK00.DAT y "No se pudo leer la etapa siguiente";
reproducido en Flycast con el Dino 246 por `tools/prueba/conduce.sh`, 2026-11-10). La etapa se suelta despues de la
decision de la rama, cuando BKD.DAT ya esta suelto.

Medido en Flycast (`DIAG_VTX`, pruebas encadenadas por las dos rutas y por IMPE, 2026-11-08):

| | Valor |
|---|---|
| RAM libre en carrera (con los dos huecos reservados) | 2510-2650 KB; peor momento: union + ruta derecha recien leida |
| VRAM libre, etapa + union | 287-640 KB (GHOS + BK09: 287 KB; 256 KB con el coche de mas texturas, COCHE04 180 KB, y el trafico nuevo de 261 KB) |
| VRAM libre, union + etapa siguiente | 155-741 KB (BK09 + IMPE, la etapa de texturas mayor (890 KB): 155 KB; peor caso con COCHE04 y el trafico nuevo: **124 KB**) |
| VRAM libre, IMPE + su meta BK13 (COCHE04, trafico nuevo) | 150 KB |
| VRAM libre con solo la union (etapa soltada) | 1044-1108 KB |
| Lectura de una bifurcacion BKkk.DAT (1,5 MB) | 0,85-0,96 s |
| Lectura de una meta BKkk.DAT (0,4-0,8 MB) | 0,31-0,45 s |
| Lectura de BKD.DAT (ruta derecha) | 0,11-0,12 s |
| Lectura y colocacion de una etapa SEGnn.DAT (2,9-6,1 MB) | 2,07-2,84 s |

En un GD real las lecturas seran mas lentas: entre la peticion de la etapa siguiente y el final de la union quedan
~1000 m (~12 s a 300 km/h); si la carretera se acaba antes de que llegue la lectura, el bucle espera
(`carga_espera`). Texturas de las metas: las de mas de error RMS 9 se aceptan en VQ hasta 24 (`UMBRAL_META` de
`tools/conv_union.py`, error medio 3,6-5,8, maximo 14,7): sin eso la meta de TULI (812 KB) o METR (712 KB) no cabia
con su etapa. Datos del curso en el GD: 15 SEGnn.DAT (62 MB), 15 BKkk.DAT + BKD.DAT (17 MB).

Peor caso de VRAM medido el 2026-11-08 (Flycast, `-DPRUEBA_FICHA=4 -DPRUEBA_NODO=9 -DPRUEBA_RAMA=0`): 124 KB libres
con la bifurcacion de GHOS y la etapa IMPE subidas, el coche de mas texturas y el trafico de 261 KB. Cabe sin recortar
nada; las otras combinaciones posibles (INDU/CLOU -> METR, METR + su meta) suman menos texturas. Si un coche o el
trafico crecen mas de ~120 KB, el margen se acaba ahi (primero: compartir las texturas repetidas de cs_BK_PALM, que
van duplicadas en cada BKkk.DAT, o pasar a VQ las pequenas que aun no lo estan). Desde 2026-11-08 los BKkk.DAT ya no
llevan cs_ENVBK_PALM (es del reflejo): 146-222 KB de texturas cada uno.
Despues (coches montados como el original y trafico con su LOD 0): el coche de mas texturas es COCHE06 (187 KB, +7
sobre los 180 de COCHE04) y el trafico 297 KB (+32): en ese peor caso quedan unos 85 KB (calculado, sin medir).
Medido el 2026-11-08 con el trafico con sus 5 LOD (328 KB) y COCHE06 (Flycast, `-DPRUEBA_FICHA=6 -DPRUEBA_NODO=9
-DPRUEBA_RAMA=0 -DPRUEBA_FIN=400`): BK09 + GHOS 220 KB libres, BK09 + IMPE (SEG16) **88 KB**, IMPE + su meta BK13
**79 KB** (el peor). No hay sitio para agrandar el bufer de parametros (cada 64 KB mas son 128 KB).

### Entorno (cielo, sol, luz; 2026-11-08)

- VRAM: SOL.DAT (sol y destello, siempre subido) 11 KB (8 texturas VQ). El cielo ya solo lleva los objetos que dibuja
  el original: las etapas y uniones bajan 3-20 KB de texturas (BK09 -3, SEG16 IMPE -10, BK13 -10); SNOW (SEG06) +66 y
  BK08 +67 (antes no tenia cielo), lejos del peor caso. Neto en el peor caso: BK09 + IMPE -2 KB, IMPE + BK13 -9 KB.
  Medido en Flycast (`DIAG_VTX`, COCHE06, `-DPRUEBA_NODO=9 -DPRUEBA_RAMA=0 -DPRUEBA_FIN=400` y `-DPRUEBA_NODO=13
  -DPRUEBA_FIN=400`, con los datos de dcwork de ese momento): BK09 + IMPE 68 KB libres, IMPE + BK13 66 KB.
- TA: el cielo dibuja menos mallas (antes todos los registros del bloque); el fundido de la union dibuja dos capas
  (como mucho ~500 vertices de cielo, 2 s); el sol con destello ~120 vertices cuando se ve. Sin cambios en el reparto.
- CPU: la luz de la pintura del coche (coche_luz) se rehace solo si la luz en el marco del coche cambia (como mucho cada
  4 cuadros): 1700-5000 vertices; la luz de la escena va en los ficheros (0 en carrera).

## 2. Presupuesto del TA

### Lo que se midio en la consola

Demo 12c en GDEMU, primer cuadro de carrera (diagnostico `pvr_colgado`):
`tri 22012 vtx 24656 err 000C`, `VB 1048576/1048576`, `OPB 141568/460800`. El bufer de parametros de 1 MB se lleno
entero (error 0x0C: bits 2 y 3) con 24656 vertices contados (sin los del HUD): **mas de 42,5 bytes por vertice**.
El OPB iba al 30 %.

### La cuenta por tira (formato de los parametros del TA)

Formato de los parametros que guarda el TA para nuestros poligonos (cabecera tipo 0, vertice tipo 3: textura, UV de
32 bits, color empaquetado; el coche ademas con color de desplazamiento):

- el TA corta cada tira en trozos de hasta 6 triangulos (8 vertices) que comparten 2 vertices;
- cada trozo: palabras ISP/TSP/TCW (12 bytes) + sus vertices de 24 bytes (x, y, z, u, v, color; 28 con color de
  desplazamiento);
- tira de n vertices con k = techo((n - 2) / 6) trozos: `24 (n - 2) + 60 k` bytes (`28 (n - 2) + 68 k` con
  desplazamiento; `ta_teo` en `demo/main.c`): 25,5 bytes por vertice en una tira de 8 (la mejor), 28 un triangulo
  suelto, 27 un cuadrilatero, ~34 en tiras largas (10 bytes por triangulo de cabeceras).
- OPB: una palabra por trozo y por tesela de 32x32 que toca; el primer bloque de cada tesela esta en la zona fija y
  los siguientes salen de la reserva comun (bloques de 32 o 16 palabras, la ultima enlaza).

Con esta cuenta el cuadro que colgo la consola ocupaba unos 780 KB (22012 triangulos y 24656 vertices: unas 1322
tiras de 18,6 vertices de media, ~4300 trozos): la consola gasto al menos 1,34 veces mas. Lo que mandamos hoy, medido
en Flycast (`DIAG_VTX`: `tiras` y `teo` son las tiras aceptadas y su cuenta), son tiras cortas: 3000-5000 tiras por
cuadro, 5,7 vertices por tira de media en las etapas (SEG00/SEG03/BK00: 28 bytes teoricos por vertice; resto medio de
3,5 triangulos sin usar en el ultimo trozo), 8,4 en el coche (30 bytes) y 7,1 en el trafico (29 bytes); un cuadro de
carrera entero sale a 29-31 bytes teoricos por vertice.

### El tope (demo/main.c, `ta_*`)

Se cuentan **todas** las tiras del cuadro: etapa, coche, trafico, efectos y el volumen de la sombra (`ta_cabe`, antes
de enviar cada tira, y `sombra_dibuja`: si no cabe no se envia) y lo 2D (HUD, menus y textos: `ta_prim`; `hud.c` y `menu.c` se compilan con `-Dpvr_prim=ta_prim`
desde `demo/Makefile`). Cada tira 3D cuesta `ta_teo` x `ta_f` / 256 con `ta_f` = 448 (1,75): con eso el cuadro que
colgo la consola costaria 56 bytes por vertice, el mismo margen que antes (1,3 x 42,5), y lo que la consola guarda
mejor (tiras de 8) cuesta menos y lo que guarda peor (triangulos sueltos del recorte cercano, tiras de 9-13) mas. Lo 2D
cuesta `ta_bpv` = 56 bytes por vertice. En la consola, `ta_f` sube solo si el TA gasta mas (registro
`TA_ITP_CURRENT` del cuadro anterior / lo teorico x 1,2; `ta_bpv` en proporcion); Flycast no lo actualiza.

| Tope por cuadro | Valor |
|---|---|
| total | `TA_VB_BYTES - TA_MARGEN` = 1792 - 64 = 1728 KB |
| 2D (va al final, no se corta) | max(64 KB, 2 x lo 2D del cuadro anterior x ta_bpv) |
| 3D (`ta_lim`) | total - 2D = 1664 KB |
| etapa (`ta_tope_etapa`) | 3D - max(coche+trafico+efectos del cuadro anterior + 2 coches de trafico con LOD 0, coche + 3 coches de trafico con LOD 0) |

Antes la reserva era 1,25 x lo del cuadro anterior: con el coche de ~600 KB se perdian ~150 KB de la etapa; un coche de
trafico que se acerca pasa de su LOD 3-4 al 0 (~50 KB), y como mucho dos cambian de un cuadro al siguiente.

Medido (Flycast, `-DDIAG_VTX -DDIAG_SIN_CPU`, PRUEBA_AUTO a 100 km/h, 60 s en PALM, LAKE, ALPI, CAST y la union de
PALM, 2026-11-08): cuadros de 14588-29624 vertices, 3D contado hasta **1530 KB de 1664** (antes 1500), 53,1-54,5 bytes
contados por vertice (antes 56 fijos) y 30,4-31,1 teoricos; ninguna tira rechazada (sin `TOPE` ni `CORTO`). Si la
consola gastara de verdad 1,34 x lo teorico, el peor cuadro llenaria 0,9 MB del bufer de 1,75 MB; si gastara 56 bytes
por vertice (la hipotesis de antes), 1,62 MB. El numero de vertices por cuadro es el de antes (24000-29000): lo ganado
para la etapa sale del trafico (LOD) y de la reserva, no de apretar el margen.

Optimizaciones que conservan la imagen (medidas con `ta_teo` sobre los .DAT y en Flycast):

- cuenta por tira en vez de 56 bytes por vertice: 53,1-54,5 bytes contados por vertice en carrera (un 3-5 % mas de
  vertices con el mismo margen);
- tiras mas largas: no ganan nada en este formato. El TA ya trocea en 6 triangulos; unir dos tiras con triangulos
  degenerados no quita trozos (cada trozo lleva su cabecera) y anade 2-3 vertices: no se hace;
- duplicados exactos (misma textura y posiciones): 14 mallas / 56 vertices en SEG00, 16 / 237 en SEG03, 47 / 188 en
  BK00 (<= 0,5 %); en el trafico ninguno salvo lo que el LOD 1 repite del 0 (ya una sola vez). No compensa;
- culling: la PowerVR va sin culling (`PVR_CULLING_NONE`) como el GS de la PS2, que no tiene; con culling se perderian
  las caras que el original ve por detras. Se queda;
- texeles del trafico fuera de la RAM tras subirlos (`compacta_modelo`): -330 KB de RAM (no cambia el TA);
- compartir texturas: ahorra VRAM, no TA; hecho entre cada meta y su etapa (seccion 1, Memoria de video del curso).

Coches del jugador (`tools/conv_coche.py`): tope de 11000 vertices dibujados a la vez por coche. Montados enteros como
el original (46A560, con las uniones de tiras que no dibuja el VU1 ya quitadas) dibujan 9320-12080, y el F40 17376 (su
pieza +14 es una copia entera de la carroceria que la carroceria tapa). Por encima del tope se quita solo lo que no se
ve desde las camaras del juego (seleccion, carrera con deriva y, si cabe, una vuelta baja): 10 de los 15 pasan por ahi
y quedan en 10348-11195 (desde las piezas animadas, PARIDAD.md Coches: piezas animadas, el vaciado mira 6 posturas: 10035-11674, cinco por encima de 11000; COCHE06 +6 KB de VRAM por el disco caliente: BK09 + IMPE con COCHE06 medido con DIAG_VTX en Flycast, 67 KB libres); el render desde esas camaras sale igual que el del original entero (0,00-0,09 % de pixeles
distintos, de la compresion VQ). El original no tiene LOD para el coche del jugador (46AE70 dibuja la disposicion de
5B2F68 entera; los 5 LOD del registro de 4866C0 de los modelos 0..29 son para cuando ese coche lo dibuja 4AE0F0, el de
los otros coches): en carrera es lo que mas gasta del TA (~600 KB de 1664). Las luces van todas en el fichero (pilotos,
con luces, freno y luz de faros: 76-490 vertices) pero solo se dibuja un juego (`coche_luces`). El reflejo no anade
vertices (una pasada con color de desplazamiento: 4 bytes mas por vertice y 8 por trozo, ya en `ta_teo`) y cuesta en
la CPU unas 4000 transformaciones de normal por cuadro (`coche_entorno`). VRAM: COCHEnn.DAT 127-187 KB. Un coche cada
vez (`coche_carga` en `demo/main.c`: libera RAM y VRAM del anterior; en el menu, si no cabe, echa la cache de
pantallas).

Trafico: con los 5 LOD del original (`tools/conv_trafico.py`; registro de 4866C0, descriptores de 4ADBC0 / 4ADD80 /
4ADF00) y el LOD por distancia de 46DF50 (en SLES-53998 0x1dd728; `traf_lod` en `demo/main.c`): LOD 1 desde 40 m, 2
desde 50, 3 desde 85 y 4 desde 130 (z de la camara; +35 m los modelos 0x1f y 0x27, +25 el 0x28). Vertices por LOD:
823-1089 / igual / 341-517 / 69-207 / 20-68. En carrera el trafico pasa de ~9500 vertices (11 coches con el LOD 0) a
~1000-1500. Con la densidad del original (2026-11-08, PARIDAD.md: Trafico: densidad del original) hay 16 plazas
pero en carrera 3-4 coches de media y como mucho 7-10 (antes siempre 11): el TA lo reparte `ta_cabe` como antes. El original ademas baja el LOD de los mas cercanos si la suma de poligonos pasa de su tope (46E170, tope
de 46E110: 12000 menos lo del coche + 5000 + 44C520): no hace falta (cabe con el tope del TA) y no se porta. VRAM del
trafico 328 KB (antes 297: texturas de los LOD 2-4); RAM: los TRAFn.DAT pasan de 543 a 730 KB, pero ahora sueltan sus
texeles tras subirlos (`compacta_modelo`, como el coche): quedan ~400 KB en la RAM (antes 543). Las mallas que repite
el LOD 1 del LOD 0 van una vez (mascara de LOD en el campo del nodo).

OPB: 4 juegos de reserva (307 KB por cuadro); en la consola se midieron 141568 bytes en total (65 KB de reserva) en
el cuadro que se colgo (incompleto). Si el registro `TA_OPB_POS` pasa del 70 % del OPB, `ta_freno` baja el tope de la
etapa por cuartos.

Bufer de parametros mas grande: cada 64 KB mas son 128 KB de VRAM (doble bufer); en el peor caso quedan 290 KB libres
(seccion 1, desde 2026-11-09; antes 79 KB): no se ha tocado. Quitar juegos de reserva del OPB daria sitio (154 KB por juego) pero el OPB crece
con la geometria y en la consola solo se ha medido un cuadro: no se toca.

Distancia y que se dibuja de la etapa (ver PARIDAD.md, seccion Graficos): lo del original. Planos 0,1 y 4000 (camara
de PS2, 0x178580), LOD por tamano en pantalla de cada nodo (404B00: se dibuja hasta z = r 2^(umbral-1) / tan(fov_v /
2), umbral 6 o 7) y recorte por esfera de nodos y tiras (`nodos_visibles`, `dibuja_rango`). Lo que el original dibuja
no cabe en el TA de la Dreamcast; medido en Flycast con `-DDIAG_SIN_TOPE` (sin tope; solo para medir, en la consola se
colgaria), salida de PALM a 100 km/h, cada 60 cuadros:

| s (m) | vertices etapa (original) | coche + trafico + efectos | total |
|---|---|---|---|
| 667 | 47365 | 20990 | 68355 |
| 906 | 33299 | 19631 | 52930 |
| 1100 | 24021 | 19299 | 43320 |
| 1452 | 21038 | 20425 | 41463 |
| 1952 | 5944 | 19534 | 25478 |

(con el trafico del LOD 0; ahora coche + trafico + efectos son ~11500.)

Reparto del tope de la etapa (`nodos_visibles_todos`), como lo veria el jugador:

- clase de distancia de cada nodo visible por la parte mas cercana de su esfera (4 log2(z - r): pasos del 19 %); se
  dibujan enteras las clases cercanas mientras quepan en 3/4 del tope: lo cercano y la calzada siempre;
- de lo de mas alla, primero se quita lo que el LOD del original quitaria antes: clase de LOD por p = z / (r 2^(umbral
  - 1) / tan(fov_v / 2)), la fraccion de la distancia de LOD de 404B00 (8 log2(p): pasos del 9 %); se quedan los nodos
  grandes (montes, mar, calzada lejana) y se van los pequenos y lejanos;
- lo que la niebla del original (scn_env_fog) ya tapa (63/64 o mas en la parte mas cercana) va a la ultima clase: es
  lo primero que se quita. No se anade ningun fundido propio a la distancia del recorte: la niebla es la del original
  (el fundido de 0,55 lejos a lejos solo si la CPU acorta `lejos`, como antes);
- histeresis: los cortes bajan en el mismo cuadro si no cabe y suben de una clase en una, cada 4 cuadros y con 2 clases
  de holgura: lo del limite no aparece y desaparece de un cuadro a otro;
- el coste estimado de cada clase es la cuenta teorica de sus nodos (`relleno[0]`, al leer el fichero) x `ta_f` x
  `prio_alfa` (lo enviado / lo estimado del cuadro anterior: 0,5-0,9, porque las mallas se recortan una a una); si aun
  asi una tira no cabe, baja el corte y sube `prio_alfa` un 15 %.

Antes se quitaba todo lo que estuviera mas alla de `corte_d` (que bajaba un 20 % si algo no cabia y subia un 6 % por
cuadro: las mallas del limite parpadeaban). Medido (Flycast, mismas pruebas; "distancia efectiva" = la del nodo
quitado mas cercano, por la parte mas cercana de su esfera; antes `corte_d`):

| Prueba | distancia efectiva antes: min / mediana (m) | despues: min / mediana (m) | vertices de la etapa antes / despues (media) |
|---|---|---|---|
| PALM | 60 / 166 | 130 / 224 | 5837 / 15208 |
| LAKE | 119 / 278 | 513 / 660 | 5692 / 15042 |
| ALPI | 111 / 200 | 196 / 387 | 5731 / 11584 |
| CAST | 92 / 185 | 129 / 258 | 5286 / 15730 |
| union de PALM | 85 / 291 | 193 / 655 | 5198 / 13234 |

Mas alla de esa distancia ya no falta todo, como antes, sino solo lo pequeno: los nodos grandes siguen hasta su LOD del
original. `lejos` solo baja (50 m cada 30 cuadros, minimo `LEJOS_MIN` 600 m) si la CPU pasa de 15 ms. Ojo: Flycast da
24-29 ms por cuadro en carrera (antes y despues) y sin `-DDIAG_SIN_CPU` `lejos` acaba en 600 m; si la consola tarda lo
mismo, eso corta antes que el TA (no medido en la consola).

### Visibilidad precalculada (PVS, 2026-11-09)

Mucho de lo que el original dibuja de la etapa no se ve (arboles y casas detras de montes, murallas y edificios): se
dibujaba y gastaba TA y CPU. Ahora la conversion calcula que mallas se pueden ver desde cada seccion de la ruta
(`tools/pvs.py` + `tools/c/pvs.c`, llamado desde `conv_tramo.py` y `conv_union.py` al escribir cada SEGnn.DAT /
BKkk.DAT) y la demo no envia las demas (`pvs_paso` en `demo/main.c`); el reparto (`nodos_visibles_todos`) se hace
despues con el coste de lo que queda de cada nodo, asi que lo que sobra va a la distancia.

- Muestras del ojo: cada punto de la ruta (4 m), a lo ancho del limite transitable mas 3 m a cada lado (cada 4 m como
  mucho) y a 0,6, 3,3 y 6 m sobre el suelo del perfil; en la bifurcacion tambien por la rama derecha (BKD.DAT). Desde
  cada una se dibujan por software cuatro caras alrededor (todos los rumbos: vale para el derrape, los trompos y las
  vistas; cabeceo de -45 a +60 grados), 256 x 350 pixeles por cara de 90 grados.
- Tapan solo los triangulos opacos (lista 0) de nodos que el LOD de 404B00 dibuja seguro desde ahi (con la fov mas
  abierta, 100 grados) y que no son de una rama de la union ni carteles; el follaje (recorte) y lo transparente no
  tapan. El z-buffer se erosiona un pixel. Cada malla (cada tira de un nodo) se ve si algun pixel que toca (rasterizado
  conservador, con la 1/z mas cercana del triangulo en el pixel) no queda detras; los carteles por su esfera. Se
  recorta contra el plano cercano.
- Secciones de 8 m (2 puntos); a cada una van las muestras cuyo punto de la ruta mas cercano es suyo o de una vecina.
  Los huecos de 8 secciones o menos en los que una malla no se ve se rellenan (visible). Las 2 primeras y 2 ultimas
  secciones llevan todo (el ojo puede estar en la etapa de antes o en la de despues).
- Fichero (ORD7 compatible): nsecciones con el bit 31; por seccion las mallas que cambian respecto a la anterior (XOR),
  en bytes (diferencias). La demo lleva la mascara de la seccion del punto de la ruta mas cercano al ojo y la mueve
  aplicando listas (adelante o atras) y lleva por nodo los bytes teoricos y vertices de lo visible. Si el ojo esta
  fuera de lo muestreado (mas de 3 m fuera del limite, mas de 6 m sobre el suelo, otra rama u otra etapa), ese modelo
  se dibuja entero como antes. `-DSIN_PVS` lo desactiva (para comparar); `DIAG_VTX` imprime `pvs` (en uso en etapa,
  union, siguiente) y `-N` (vertices de nodos en la vista que quita).
- RAM: cada SEGnn.DAT crece lo que su PVS, con un tope de 300 KB (`MAX_KB`: si no cabe se rellenan huecos mayores;
  METR, SEG13, el mayor, queda en 235 KB con huecos de 128 secciones y le quedan 70 % de las mallas en vez de 41 %;
  SEG04 258 KB con 64); las uniones 1-12 KB. El hueco de la etapa es el del SEGnn.DAT mayor (seccion 1): 7796 KB con
  este juego de datos (el anterior, sin PVS, ya tenia un SEG13 de 7829 KB); en carrera, con la union de METR leida,
  1156 KB libres (Flycast, `DIAG_VTX`). Mas la mascara (1 bit por malla, <= 6 KB) y 8 bytes por nodo. CPU: aplicar una
  lista por seccion (unos 400 bytes) y una prueba de bit por malla.
- Conversion: 2-9 min por etapa y 15-50 s por union con 4 nucleos (la de todo el curso, ~1 h 30 min).

Medido en Flycast (`DIAG_VTX`, `-DDIAG_SIN_CPU`, PRUEBA_AUTO a 100 km/h, congelado en el paso 300 del crono para que
las dos versiones esten en el mismo sitio; misma posicion con y sin `-DSIN_PVS`):

| Prueba (s) | vertices de la etapa enviados sin / con PVS | TA de la etapa sin / con (KB) | nodos en vista y LOD (lo que dibujaria el original), vertices sin / con PVS | distancia efectiva sin / con | pixeles distintos sin tope / con tope (fuera del coche) |
|---|---|---|---|---|---|
| PALM (1656 m) | 16422 / 11676 | 808 / 586 | 20238 / 15216 | 4000 / 4000 (sin el, recorte de la clase de LOD 62) | 0 / 0 |
| LAKE (2838 m) | 9309 / 7994 | 453 / 391 | 12259 / 9622 | 4000 / 4000 | 0 / 0 |
| ALPI (2504 m) | 14437 / 7083 | 706 / 360 | 17497 / 8238 | 4000 / 4000 | 0 / 0 |
| CAST (2462 m) | 15790 / 8840 | 781 / 443 | 88518 / 12469 | **192 / 4000** | 0 / 469 (la muralla del fondo que sin PVS se recortaba) |
| FORE (2187 m) | 16499 / 8504 | 810 / 430 | 20681 / 10972 | 4000 / 4000 | 0 / 0 |
| union de PALM (262 m dentro) | 9946 / 9946 | 486 / 486 | 14938 / 14938 | 4000 / 4000 | 0 / 0 |

En la union (bifurcacion, abierta) no se quita nada: desde alguna muestra se ve todo (BKkk.DAT de bifurcacion: 100 %
de las mallas; las metas 47-98 %). Media de las mallas que quedan por seccion (conversion): PALM 74 %, LAKE 60 %, ALPI
35 %, CAST 23 %, FORE 45 %, DESE 31 %, CLOU 35 % (de todas las del fichero; de lo que esta en la vista desde el
centro de la ruta: PALM 82 %, LAKE 78 %, ALPI 56 %, CAST 37 %, FORE 49 % de los vertices).

En marcha (sin congelar, 2 min desde PRUEBA_FIN; las dos versiones no pasan exactamente por los mismos sitios):

| Prueba | vertices de la etapa enviados (media) sin / con | lo que dibujaria el original sin / con PVS (media) | distancia efectiva min / mediana sin -> con (m) | cuadros sin ningun corte sin / con |
|---|---|---|---|---|
| PALM | 13781 / 13605 | 35359 / 30751 | 128 / 443 -> 161 / 527 | 28 / 35 % |
| LAKE | 14111 / 13934 | 32971 / 26539 | 173 / 671 -> 206 / 926 | 37 / 45 % |
| ALPI | 13116 / 11223 | 29159 / 18072 | 193 / 450 -> 192 / 4000 | 38 / 62 % |
| CAST | 15535 / 14343 | 62104 / 25118 | 131 / 257 -> 160 / 550 | 10 / 38 % |
| FORE | 12785 / 10099 | 28364 / 17495 | 225 / 641 -> 321 / 4000 | 49 / 70 % |
| final de PALM y su union | 12937 / 11314 | 30787 / 22022 | 128 / 647 -> 160 / 4000 | 43 / 60 % |

(distancia efectiva: la del nodo quitado mas cercano por el reparto, 4000 si no se quita nada). Donde el tope ya
cortaba, lo ganado va a la distancia (mismos vertices, mas lejos); donde no, se envian menos vertices.

Imagen: capturas congeladas (5 por prueba) con y sin PVS, en dos modos: con el reparto normal y sin tope ni reparto
(`-DDIAG_SIN_TOPE` y los cortes de `nodos_quita` quitados en la copia de prueba), comparadas pixel a pixel fuera del
coche del jugador (su luz y el piloto del cartel cambian solos aun congelado): 0 pixeles distintos en las seis pruebas
sin tope y en cinco con tope; en CAST con tope, lo distinto es lo que el PVS deja meter (una muralla del castillo al
fondo). Capturas en el scratchpad (`pvs/*_tope.png`, `pvs/*_todo.png`).

El diagnostico `pvr_colgado()` (si `pvr_wait_ready` falla 5 veces seguidas escribe en la imagen el estado, los
vertices, el registro de errores y el uso de VB y OPB) se queda: es lo que permite medir en la consola.

### Sombra del coche y marcas de las ruedas (2026-11-08)

La sombra del coche del jugador va como en el original: volumenes de sombra (+08 y +10 de la disposicion) extruidos
por la luz del sol (PARIDAD.md, Sombras y marcas). En la PowerVR es un volumen modificador en la lista OP_MOD en modo
"cheap shadow" (`sombra_dibuja` en `demo/main.c`): lo opaco y lo recortado de la escena y del trafico (cabeceras con el
bit de modificador; el coche del jugador no) que queda dentro se multiplica por 160/255.

- OPB: la lista OP_MOD necesita sus bloques en cada tesela. Se pasan los translucidos de 16 a 8 palabras y los
  modificadores opacos llevan 8: `{32, 8, 8, 0, 16}` = 64 palabras por tesela, lo mismo que antes
  (`{32, 0, 16, 0, 16}`): la VRAM del OPB no cambia (384 KB por juego). Los translucidos con mas de 7 punteros en una
  tesela enlazan bloques de la reserva comun (4 juegos), que en la consola iba al 30 % en el cuadro medido.
- TA: cada triangulo del volumen son 40 bytes teoricos (palabra ISP y 9 floats) x `ta_f`; el volumen va entero o no va
  (`ta_cabe` no puede cortarlo: un volumen sin su ultimo triangulo no se cierra). Medido (Flycast, `DIAG_VTX`, campo
  `sombra`): 118-186 triangulos por cuadro, 4-7 KB teoricos (7-12 KB contados). Solo los dibuja el coche del jugador
  (el trafico lleva su sombra plana).
- Paridad: la PowerVR decide dentro / fuera por par / impar de las caras del volumen delante de cada pixel (Flycast
  igual: plantilla con XOR), el original cuenta (+1 / -1 en la plantilla). Con las mallas tal cual, donde la silueta
  extruida se dobla (entre el capo y la cabina) la sombra salia par (vacia); por eso cada parte conexa va como su
  envolvente convexa (`tools/conv_coche.py volumenes`): 1-5 volumenes por coche.
- Marcas (`efectos_paso` / `efectos_dibuja`): anillos del original de 256 + 64 + 64 cuadrilateros (4 vertices, 108
  bytes teoricos cada uno; solo los de menos de 250 m delante): 41 KB teoricos como mucho. Texturas: EFECTOS.DAT, 3 de
  64x32 ARGB4444 (12 KB de VRAM).
- VRAM: la sombra plana +0C anade su textura (128x32) a cada COCHEnn.DAT: COCHE06 pasa de 187 a 196 KB (el de mas
  texturas). Medido el 2026-11-08 (Flycast, `-DPRUEBA_FICHA=6 -DPRUEBA_NODO=9 -DPRUEBA_RAMA=0 -DPRUEBA_FIN=400`):
  BK09 + IMPE (SEG16) **51 KB libres** (antes 88: -9 del coche, -12 de EFECTOS.DAT y el resto de otros cambios del
  mismo dia en el arbol); IMPE + su meta BK13 (antes 79) queda, por la misma diferencia, en unos 42 KB (sin medir).
  Si hace falta sitio: la sombra plana es negra con 16 niveles de alfa (cabria en 4 bits con paleta: 2 KB en vez de
  8) y lo mismo las marcas.

### Suelos: listas del estado del GS, mezcla aditiva y mipmaps (2026-11-09)

Ver PARIDAD.md, Suelos. Coste medido (Flycast, `DIAG_VTX`, datos regenerados con `tools/conv_union.py`):

- VRAM: los mipmaps de las texturas cuadradas del suelo (6-27 por etapa) suman un tercio de los indices de su VQ (el
  libro no crece) o de sus texeles si son de menos de 32x32; si la VQ con niveles pasa del error RMS 9 la textura va
  sin mipmaps. Texturas por fichero: SEG00 492 -> 519 KB, SEG03 556 -> 469 (la calzada y lo que era 4444 por error
  pasa a VQ 565), SEG05 623 -> 634, SEG06 726 -> 753, SEG10 665 -> 700, SEG13 775 -> 505, SEG16 880 -> 893, BK09 213
  -> 215, BK13 214 -> 217. Peor caso medido (COCHE06, `-DPRUEBA_FICHA=6`): IMPE + su meta BK13 (`-DPRUEBA_NODO=13
  -DPRUEBA_FIN=400`) **51 KB libres** (antes 67 KB), BK09 + IMPE (`-DPRUEBA_NODO=9 -DPRUEBA_RAMA=0 -DPRUEBA_FIN=400`)
  **53 KB** (antes 69 KB). Si hace falta sitio, lo primero: quitar los mipmaps de IMPE (13 KB) o las ideas de la
  sombra plana (arriba).
- TA y OPB: los mismos vertices y casi las mismas cabeceras (3 texturas de PALM llevan una entrada por lista). Pasan
  de la lista transparente a la de recorte o a la opaca miles de tiras (mallas transparentes de los ficheros: PALM 6925
  -> 295, METR 5933 -> 4546, CAPE 601 -> 84) y al reves las que el original mezcla (ALPI 992 -> 1793, INDU 269 ->
  1042). Los translucidos ordenados (autosort) son los caros para la PowerVR; los de recorte tienen 16 palabras por
  tesela en el OPB (los translucidos 8).
- CPU: nada (las listas, la mezcla y los mipmaps van en los ficheros y en las cabeceras).
- Mipmaps de la PowerVR: solo texturas cuadradas twiddled o VQ; datos de 16 bits con 6 bytes de relleno y los niveles
  de 1x1 a la base (el de lado 2^k en 0x6, 0x8, 0x10, 0x30, 0xB0...); VQ con el libro y los indices de los niveles
  seguidos (0x0, 0x1, 0x2, 0x6, 0x16, 0x56... tras el libro; el de 1x1 un bloque de 2x2 de su color). La direccion de
  la textura es la de siempre (con el libro reducido, 2048 - 8 N bytes antes).

### Piloto y acompanante (CHR.DAT, 2026-11-09)

Ver PARIDAD.md (Coches: el piloto y la acompanante). Medido en Flycast (`DIAG_VTX`, que ahora da `rob` = vertices de
los personajes y us de CPU del cuadro anterior):

| | Valor |
|---|---|
| Vertices dibujados | 2698 por cuadro (piloto 1238, acompanante 1460; 325 tiras de 8,3 de media; una de las dos manos) |
| TA | 80 KB teoricos (`ta_teo`), 139 KB contados (x 1,75); van en lo "que no es la etapa": el reparto de `ta_cabe` quita de la etapa lo que no cabe (PALM con el 360 Spider: 16800-17040 vertices de etapa sin ellos, 12630-13640 con ellos; total del cuadro igual, 26300-27600; sin `TOPE` ni `CORTO`) |
| VRAM | 22 KB (7 texturas, 5 en VQ; de 106 KB en 16 bits), siempre subidas |
| RAM | CHR.DAT 177 KB en el monton (199 KB menos los texeles; 2026-11-10: 219 KB con los 20 movimientos de la meta del piloto) + ~76 KB de tablas fijas (motor: 2 x 31 KB; matrices de las piezas 9 KB); META.DAT 110 KB desde el arranque (escena de la meta, demo/meta.c) y 30 KB del AICA (efecto 0x96) solo durante la meta |
| CPU (Flycast) | 3,2 ms por cuadro: 0,76 la logica y el motor (dos esqueletos de 53 y 50 huesos con IK), 2,45 la mezcla y la luz por vertice (una pasada de ftrv por matriz; color por tabla de 33 valores de n.l por material) |

RAM: CHR.DAT se lee al arrancar, antes que los huecos y la ruta (lo que se pide despues troceaba el monton), y nada mas
se pide luego (tablas fijas). Aun asi la ruta de la carrera, que se pedia otra vez (`realloc`, ~700 KB seguidos) al
salir de una etapa con mas puntos que la anterior, ya no cabia: la cima del monton quedaba 177 KB mas arriba y el
hueco era de 664 KB (prueba GHOS: "RUTA: sin 704 KB contiguos (RAM libre 904 KB)"). Ahora `ruta_pon` suelta la ruta
vieja antes de pedir la nueva y la primera vez la pide con 300 puntos de mas (`RUTA_HOLGURA`, 66 KB; las etapas
tienen 1156-1375 puntos): la de salida de cualquier carrera cabe en el mismo bloque. RAM libre medida (Flycast,
`-DPRUEBA_AUTO -DPRUEBA_FICHA=6 -DPRUEBA_RAMA=0 -DPRUEBA_FIN=400`):

| Momento | Sin personajes | Con personajes |
|---|---|---|
| INDU: BK07 + BKD (el peor) | 882 KB | 709 KB |
| METR (SEG13) y su meta BK11 | 979-982 KB | 806-809 KB |
| GHOS: BK09 + BKD | 901 KB | 721 KB |

VRAM del peor caso (mismas pruebas): IMPE + su meta BK13 51 KB sin personajes, 29 KB con ellos
(`-DPRUEBA_NODO=13 -DPRUEBA_FIN=300`); GHOS -> IMPE: SEG16 subida con BK09 aun en VRAM 53 / 30 KB. OJO, de antes y sin
relacion con los personajes (pasa igual sin CHR.DAT): en esa misma prueba (`-DPRUEBA_NODO=9`), al pedir despues la meta
BK13 sale "Sin memoria de video para la etapa" (la VRAM queda troceada al soltar BK09; empezando en IMPE, BK13 sube
con 51 KB libres). PENDIENTE: compartir las texturas repetidas de cs_BK_PALM o subir las uniones en un hueco fijo de
VRAM.

### Memoria de video del curso y del HUD (2026-11-09)

**Fallo.** En la ruta GHOS -> IMPE la meta BK13 no subia ("Sin memoria de video para la etapa" y la carrera parada en
un bucle; tambien sin CHR.DAT). Reproducido en Flycast con el codigo y los datos de antes (`-DPRUEBA_AUTO
-DPRUEBA_FICHA=6 -DPRUEBA_NODO=9 -DPRUEBA_RAMA=0 -DPRUEBA_FIN=400 -DDIAG_VTX`): al pedir BK13 en IMPE (s 9812) habia
**253 KB libres** y BK13 necesita 219 KB, pero no contiguos. Causa: cada textura de las etapas y uniones se pedia a
`pvr_mem` (el dlmalloc de KallistiOS) una a una: GHOS abajo, BK09 encima; se suelta GHOS (hueco de 771 KB); IMPE
(893 KB) llena ese hueco y sigue por encima de BK09; se suelta BK09 y queda un hueco de 217 KB entre trozos de IMPE:
lo libre quedaba repartido en huecos menores que las texturas grandes de BK13. Empezando en IMPE (sin pasar por GHOS)
BK13 si subia. Ademas el margen era minimo: 29-31 KB en IMPE + BK13, BK09 + IMPE y BK08 + IMPE (COCHE06, personajes).

**Medida en host.** `tools/vram.py datos/ [--zona KB]` suma las texturas de cada .DAT como la demo (antes, un bloque de
`pvr_mem` por textura: tamano + 4 redondeado a 32, `request2size` de pvr_mem_core.c; ahora, alineadas a 32 en la zona
del curso) para todas las combinaciones del arbol (etapa + su union, union + etapa de cada rama), lo que se repite
entre la union y su etapa y lo fijo (coche, trafico, personajes, sol). Lo que no esta en esos .DAT se midio aparte: HUD
de carrera 776 KB (23 texturas, casi todas ARGB4444 sin comprimir), letreros 144 KB, efectos 14 KB, pausa ~39 KB (solo
en pausa).

**Cambios** (ninguno cambia lo que se ve, salvo el ultimo recurso del punto 4, que no se da en ninguna combinacion):

1. Zona del curso (`demo/main.c`: `ar_reserva`, `vram_pide`, `sube_tramo`). Al empezar la carrera (`curso_reinicia`,
   ya subidos el coche y el HUD) se pide a `pvr_mem` un solo bloque con lo libre menos `CURSO_VRAM_RESERVA` (48 KB) y
   lo que pedira la pausa (`menu_pausa_vram`); si `pvr_mem` estuviera troceado, el mayor bloque que cabe (busqueda a
   32 KB). Las etapas se suben desde abajo y las uniones desde arriba. Nunca hay mas de una etapa y una union, y cada
   una se suelta entera antes de que entre la siguiente de su lado (la etapa que se deja antes de pedir la siguiente;
   la union UNION_LIBERA m despues de llegar a la etapa nueva, antes de pedir la siguiente union): no queda ningun
   hueco y cabe cualquier combinacion que sume menos que la zona. Las texturas van alineadas a 32 sin la cabecera de
   dlmalloc (3-6 KB menos por combinacion). Las grandes se suben primero (por si alguna tuviera que ir a `pvr_mem`).
   Al volver al menu (`curso_al_menu`, 60 ms despues, cuando la PowerVR ya no las lee) se sueltan la union, la etapa
   siguiente, las texturas de la etapa y la zona: el menu no dibuja la etapa y asi tiene esa VRAM para sus pantallas
   (antes, tras una carrera acabada en IMPE, el menu tenia 1117 KB menos; al arrancar, los 525 KB de PALM menos; en el
   menu de arranque ahora quedan 1521 KB libres con COCHE06). La etapa de salida sigue en su hueco de la RAM y se
   vuelve a subir desde ahi (sin leer el GD). Las cabeceras guardadas del cielo (`cielo_cab`, por direccion) se borran
   al soltar texturas: con la zona las direcciones se reutilizan siempre.
2. La meta comparte con su etapa las texturas iguales (`tex_igual`: mismo tamano, formato y bytes). Cada meta (BKkk de
   las etapas finales) lleva el cielo de su etapa y parte de cs_BK_<zona> que ya estan en la etapa: TULI 114 KB, METR
   186, RUIN 74, IMPE 94, CAPE 131. Las dos se quedan hasta el final de la carrera. En las bifurcaciones no se
   comparte: la etapa se suelta antes que la union, y las de cs_BK_PALM (comunes a todas las uniones, 22 texturas,
   130 KB) no ahorran nada porque nunca hay dos uniones a la vez.
3. HUD de carrera con paleta (`comun/hud.c`, `pal_reparte`). 17 de sus texturas ARGB4444 tienen 16 colores distintos
   o menos (paneles, cifras de puntos y crono, marchas, mapa de ruta, la parte pequena de cada tacometro) y 19 entre 17 y
   256 (cifras del tiempo, tacometros, rebufo, avisos de bifurcacion y tiempo extra): vienen de texturas de PS2 con
   CLUT. Se suben con indices de 4 u 8 bits y una paleta ARGB4444 de la PowerVR con exactamente sus colores (los
   tacometros comparten banco, las cifras del tiempo tambien: 4 bancos de 8 bits y 8 de 4 de las 1024 entradas), asi
   que cada texel da el mismo valor de 16 bits que antes: **-254 KB** (776 -> 522 KB con el tacometro del Testarossa).
   Se quedan como estaban la de los avisos de salida y meta (512x256, 576 colores), las ARGB1555 de la velocidad (sus
   colores no caben exactos en una paleta 4444) y las VQ. Comprobado en Flycast con la misma imagen congelada antes y
   despues (`-DPRUEBA_AUTO -DPRUEBA_GUION`, crono 300): 0 pixeles distintos en paneles, cifras, tacometro y marchas
   (los 7001 distintos son el coche, cuya luz cambia sola aun congelado, la aguja y el punto que parpadea en el mapa).
4. Si aun asi algo no cabe (no pasa en ninguna combinacion; la consola podria diferir): la textura va a `pvr_mem` fuera
   de la zona; si tampoco, `vram_apuro` echa lo prescindible (las texturas del HUD que no se han dibujado en 8
   cuadros, que vuelven solas el cuadro en que se quieren dibujar, y lo pendiente de liberar del HUD y los menus) y
   reintenta; como ultimo recurso la textura se sube rebajada (`tex_rebaja`: 1/2, 1/4... de lado en 16 bits, cada
   texel la media de los que cubre: se ve borrosa) y si ni eso cabe, una gris de 8x8. La carrera ya no se para por la
   VRAM (antes: mensaje y bucle infinito). `CURSO: GOAL` y `DIAG` dicen cuantas texturas se rebajaron (`reb`).

No hicieron falta (no se han hecho): quitar los mipmaps de IMPE (~13 KB), subir el umbral de VQ ni paletas en las
texturas de las etapas.

VRAM por combinacion (KB; con COCHE06, el coche de mas texturas, trafico, HUD, personajes, efectos y sol). Antes: suma
con la cabecera de dlmalloc; libre = 1146 - suma (lo que quedaba para el curso: 29-31 KB medidos en las tres peores),
sin contar que estaba troceada. Despues: lo que ocupa en la zona (sin lo compartido) y libre = zona (1312 KB medidos) -
ocupado + 87 KB fuera de la zona (reserva y pausa). Calculado con `tools/vram.py`; las cifras en negrita se midieron
en Flycast (`DIAG_VTX`, lineas `VRAM:` y `CURSO:`) y coinciden con el calculo a 1-2 KB:

| Combinacion | Etapa | Union | Antes: suma | Antes: libre | Compartido | Despues: en la zona | Despues: libre |
|---|---|---|---|---|---|---|---|
| PALM + BK00 | 525 | 157 | 681 | 465 | 0 | 675 | 724 |
| BK00 + LAKE | 474 | 157 | 631 | 515 | 0 | 625 | 774 |
| BK00 + ALPI | 639 | 157 | 796 | 350 | 0 | 790 | 609 |
| LAKE + BK01 | 474 | 157 | 631 | 515 | 0 | 625 | 774 |
| BK01 + CAST | 421 | 157 | 578 | 568 | 0 | 574 | 825 |
| BK01 + FORE | 703 | 157 | 859 | 287 | 0 | 856 | 543 |
| ALPI + BK02 | 639 | 217 | 856 | 290 | 0 | 850 | 549 |
| BK02 + FORE | 703 | 217 | 919 | 227 | 0 | 916 | 483 |
| BK02 + DESE | 314 | 217 | 531 | 615 | 0 | 528 | 871 |
| CAST + BK03 | 421 | 217 | 638 | 508 | 0 | 634 | 765 |
| BK03 + CLOU | 569 | 217 | 785 | 361 | 0 | 783 | 616 |
| BK03 + INDU | 590 | 217 | 807 | 339 | 0 | 801 | 598 |
| FORE + BK04 | 703 | 217 | 919 | 227 | 0 | 916 | 483 |
| BK04 + INDU | 590 | 217 | 807 | 339 | 0 | 801 | 598 |
| BK04 + SNOW | 756 | 217 | 973 | 173 | 0 | 969 | 430 |
| DESE + BK05 | 314 | 217 | 531 | 615 | 0 | 528 | 871 |
| BK05 + SNOW | 756 | 217 | 973 | 173 | 0 | 969 | 430 |
| BK05 + GHOS | 771 | 217 | 988 | 158 | 0 | 985 | 414 |
| CLOU + BK06 | 569 | 169 | 737 | 409 | 0 | 735 | **664** (medido) |
| BK06 + TULI | 682 | 169 | 851 | 295 | 0 | 846 | **553** (medido) |
| BK06 + METR | 509 | 169 | 678 | 468 | 0 | 673 | 726 |
| INDU + BK07 | 590 | 217 | 807 | 339 | 0 | 801 | **598** (medido) |
| BK07 + METR | 509 | 217 | 726 | 420 | 0 | 721 | **678** (medido) |
| BK07 + RUIN | 418 | 217 | 634 | 512 | 0 | 632 | 767 |
| SNOW + BK08 | 756 | 217 | 973 | 173 | 0 | 969 | **430** (medido) |
| BK08 + RUIN | 418 | 217 | 634 | 512 | 0 | 632 | **767** (medido) |
| BK08 + IMPE | 898 | 217 | 1115 | 31 | 0 | 1109 | **290** (medido) |
| GHOS + BK09 | 771 | 217 | 988 | 158 | 0 | 985 | **414** (medido) |
| BK09 + IMPE | 898 | 217 | 1115 | 31 | 0 | 1109 | **290** (medido: 202 en la zona + 87) |
| BK09 + CAPE | 587 | 217 | 804 | 342 | 0 | 799 | **600** (medido) |
| TULI + BK10 | 682 | 232 | 914 | 232 | 114 | 795 | **604** (medido) |
| METR + BK11 | 509 | 251 | 760 | 386 | 186 | 568 | **831** (medido) |
| RUIN + BK12 | 418 | 222 | 640 | 506 | 74 | 564 | **835** (medido) |
| IMPE + BK13 | 898 | 219 | 1117 | 29 | 94 | 1017 | **382** (medido: 294 en la zona + 87) |
| CAPE + BK14 | 587 | 191 | 778 | 368 | 131 | 643 | **756** (medido) |

Peor caso: BK08 + IMPE y BK09 + IMPE, **290 KB libres** (antes ~30), e IMPE + BK13 **382 KB** (antes 29 y troceada).
Con un coche de menos texturas la zona es mayor (lo que le sobra al coche).

Verificacion en Flycast (`-DPRUEBA_AUTO -DPRUEBA_FICHA=6 -DPRUEBA_FIN=400 -DPRUEBA_V=250 -DPRUEBA_SIN_TIEMPO -DDIAG_VTX`;
`PRUEBA_SIN_TIEMPO`, nuevo, no deja que se acabe el tiempo para llegar a la meta saliendo de 400 m antes del final de
la etapa del nivel 3), las cinco metas hasta GOAL, cada una dos o tres carreras seguidas (GOAL, menu, carrera otra
vez: mismas cifras en cada vuelta, sin perder VRAM): GHOS -> IMPE (`-DPRUEBA_NODO=9 -DPRUEBA_RAMA=0`, el fallo de
antes), SNOW -> IMPE (`8`, `1`), CLOU -> TULI (`6`, `0`), INDU -> METR (`7`, `0`), SNOW -> RUIN (`8`, `0`) y GHOS ->
CAPE (`9`, `1`): 0 texturas rebajadas, ninguna fuera de la zona; lo libre medido coincide con `tools/vram.py` a
1-2 KB (tabla). Ultimo recurso: `-DPRUEBA_NODO=13 -DPRUEBA_FIN=2000 -DPRUEBA_VRAM_KB=700` (zona de 700 KB y el resto de
la VRAM ocupada salvo la reserva): IMPE no cabe, el apuro echa el HUD (522 KB, aun sin dibujar), 26 texturas van
rebajadas o grises (edificios borrosos), el HUD vuelve al dibujarse y la carrera llega a GOAL sin pararse.

### RAM del arranque y del menu (2026-11-09)

**Fallo** (recorrido completo por el menu, sin `PRUEBA_*`): (1) la pelicula de inicio no salia ("VIDEO MVATTR.VQV:
sin memoria (anillo 0x0 de 384 KB ... RAM libre 235 KB)"); (2) las pantallas de coche, cambio y musica salian vacias
("menu: pantalla 3/4/5 sin VRAM (libre 1949K ...)" con 1,9 MB de VRAM libres); (3) en la carrera no se dibujaba la
carroceria del coche ("coche: ficha 9 fuera de la VRAM (la pide el menu)" y nunca se volvia a cargar). Las pruebas con
`PRUEBA_AUTO` / `PRUEBA_FICHA` no pasan por la pelicula ni por el menu y no lo veian.

**Causa: una sola, RAM.** No era VRAM. Lo residente fue creciendo hasta que el monton del arranque y del menu se quedo
sin bloques grandes seguidos: los personajes (88803b4: CHR.DAT 177 KB + 76 KB de tablas; con el codigo de 4d90759 y
los mismos datos la pelicula aun salia, con el anillo minimo de 512 KB; con el de 88803b4 ya no) y luego BO.DAT
(d98608f, 74 KB). Medido en Flycast al arrancar (lineas temporales con `ram_kb` y `mallinfo`):

| Paso | En uso (KB) | Libre (KB) |
|---|---|---|
| Programa y BSS (anillo de la musica 1 MB, etc.) | 90 | 14180 |
| CHR.DAT, CURSO.DAT, huecos de etapa (7832) y union (1480), SEG00 en su hueco | 9622 | 4648 |
| TRAFn.DAT (730), BO.DAT y la ruta (~770 con la holgura) | 11185 | 3085 |
| texeles del trafico a la VRAM (`compacta_modelo`) | 10930 | 3341 |
| coche (COCHE09 sin texeles) | 11301 | 2970 |
| HUD.DAT entero (1211) | 12488 | 1782 |
| musica, efectos, voces | 12589 | 1681 |
| MENU.DAT (tablas: 974) y VMU | **13563** | **699** (235 en huecos del monton, ~464 seguidos arriba) |

Con 699 KB libres en trozos: la pelicula pide 512 KB seguidos (y 192 KB de margen); `hudd_sube` de una pantalla del
menu lee del GD sus sectores (100-400 KB seguidos: "Out of memory ... diff 352256") y devolvia -3, que el menu tomaba
por falta de VRAM: a los 2 cuadros echaba el coche 3D (`menu_suelta_coche`, que no servia de nada) y a los 90 dejaba
la pantalla vacia; al salir, `coche_carga` volvia a leer el COCHEnn.DAT entero (450-580 KB seguidos), fallaba sin
avisar y la carrera salia sin carroceria (el piloto y la acompanante, que van aparte, si). Los dos "pvr_mem: ERROR --
out of PVR memory" eran la busqueda a saltos de la zona del curso (`ar_reserva`) con la VRAM partida por eso.

**Arreglo** (sin cambiar nada de lo que se ve):

1. El hueco de la union (1480 KB) no se usa fuera de la carrera: se presta (`presta_union` / `union_prestable` en
   `demo/main.c`: ni union en el ni lectura en marcha hacia el) al arrancar y en cada cuadro del menu, y se retira al
   empezar la carrera, antes de `curso_reinicia`. Lo usan, uno cada vez y desde el hilo principal:
   - las peliculas (`video_bufer`, comun/video.c): anillo de 1 MB y los dos de audio dentro (antes 512 KB del monton);
   - las subidas del menu (`hudd_bufer`, comun/hud.c): los sectores leidos de MENU.DAT y los indices de las texturas
     con paleta del HUD (en la carrera, sin prestamo, como antes: la pausa los pide al monton, 39 KB);
   - `coche_carga`: el COCHEnn.DAT se lee en el hueco y solo lo que queda sin los texeles (300-400 KB) se copia al
     monton (`compacta_modelo_de`; antes hacian falta los 450-580 KB del fichero entero seguidos). Si no cabe ni eso,
     el coche no se queda en el hueco (error -5 y log).
2. `curso_al_menu` suelta tambien los objetos de la union (`bo_quita`: hasta 360 KB de RAM de BOnn.DAT y su VRAM fuera
   de la zona), que antes seguian ocupando en el menu hasta la carrera siguiente.
3. El menu distingue la causa (`sube_err`: "sin RAM", "sin VRAM", "lectura del GD") y solo echa el coche 3D si lo que
   falta es VRAM.
4. Si la RAM no da para un BOnn.DAT en la carrera (los objetos animados de la union), no salen y la carrera sigue
   ("BO: no se pudo leer"); antes `curso_paso` lo tomaba por la etapa siguiente y paraba el juego
   ("No se pudo leer la etapa siguiente").

Medido en Flycast (2026-11-09, recorrido completo por el menu con `nav.sh`: START y A en cada pantalla, coche por
defecto del guardado de la VMU, ficha 13):

| | Valor |
|---|---|
| RAM al arrancar (linea `RAM:` del log) | en uso 13563 KB, libre 699 KB (235-239 en huecos); hueco de la union prestado |
| Pelicula de inicio | anillo de 1024 KB, precarga de 43 cuadros en 430 ms, sin faltas de audio |
| Menu | todas las pantallas (titulo, principal, modo, coche, cambio, musica), ningun "sin RAM/VRAM" |
| VRAM en el menu | 1968 KB libres tras el coche de arranque; 863 KB con el coche 3D (COCHE13) y las pantallas subidas |
| VRAM en la carrera (PALM + BK00, COCHE13) | zona del curso 1344 KB, fuera 75 KB; 737 KB libres con la union |
| RAM en la carrera | 552-720 KB libres (peor: BK09 + BKD + BO01); con el dirigible (BO11, 283 KB, `-DPRUEBA_BO=11`) 291 KB |
| Otro coche por el menu tras abandonar una carrera (pausa, ABANDONAR) | COCHE14 leido en el hueco prestado; carrera con el 365 GTS/4 |

VRAM del curso con los objetos de etapa de d98608f (pancarta CHECK POINT y carteles en cada SEGnn.DAT, +17-41 KB de
texturas por etapa; `tools/vram.py`, COCHE06): peor BK08 / BK09 + IMPE 1127 KB en la zona (antes 1109), IMPE + BK13
1033 (antes 1017). Medido (`-DPRUEBA_AUTO -DPRUEBA_FICHA=6 -DPRUEBA_NODO=9 -DPRUEBA_RAMA=0 -DPRUEBA_FIN=400 -DPRUEBA_V=250
-DPRUEBA_SIN_TIEMPO -DDIAG_VTX`, dos carreras seguidas GHOS -> IMPE -> GOAL): zona 1312 KB; BK09 + IMPE **272 KB**
libres (185 en la zona + 87; antes 290), IMPE + BK13 **366 KB** (279 + 87; antes 382), 0 texturas rebajadas,
ninguna fuera de la zona salvo la del objeto de la union.

### Objetos animados de las uniones en la cola del hueco de la union (2026-11-09)

Fallo (Flycast, PRUEBA_AUTO con COCHE09): el dragon (BO11.DAT, conjunto 11, el de la etapa limpia) no se leia:
"Out of memory ... diff 282624" con 288 KB libres en total pero la cima del monton en ~135 KB (lo demas, troceado).
Ahora: BOnn.DAT compacto (cuadros 1..3 en int16, BO11 167 KB) y leido en la cola del hueco de la union, donde empiezan
los texeles del BKnn.DAT (ya estan en la VRAM): 217-230 KB libres ahi en las 15 uniones; si no cabe, al
monton como antes. El monton no cambia (medido en la carrera desde el menu: "BO11.DAT en 0.15 s ... RAM libre 278K"
frente a 279K antes de leerlo), y fuera de la carrera (pelicula, menus) no se usa: el prestamo del hueco de la union
(presta_union) no se toca. libera_modelo no libera un modelo que este dentro del hueco de la union.

### Piezas del HUD y de los menus a 24 bytes (2026-11-09)

**Que crecio.** La RAM libre al arrancar paso de 699 KB (4f60223) a 367-391 KB: casi todo de c5790a9 (seleccion de
musica completa), que llevo las tablas de MENU.DAT (en la RAM siempre, tambien en la carrera) de 974 a 1270 KB (30895
piezas de 40 bytes: 1207 KB). El resto de commits (pajaros, dragon, modos) suman unos 30 KB. Medido en Flycast con el
recorrido real del menu: c5790a9 391 KB al arrancar y 359 / 286 KB libres al leer BK00 / BKD; 53fbf08 367 KB y 327 /
253 KB.

**Arreglo** (comun/hud.c `hudd_abre`, sin cambiar los ficheros): cada pieza se guarda en la RAM en 24 bytes (x, y, ax,
ay en 1/8 de pixel, angulo en 1/64 de grado, escalas en 1/512, alfa en 1/255) en vez de los 40 del fichero; en un
fichero parcial (MENU.DAT, HAHUD.DAT) lo de detras de las piezas (usos, tacometros, MNU1 / MNU2) se baja y el bloque se
encoge (realloc). Error maximo medido sobre todas las piezas de MENU.DAT y HUD.DAT: 0,06 px, 0,007 grados, 0,0005 de
escala, 0,002 de alfa: no se ve a 640x480. Ahorro: 483 KB (MENU.DAT) + 16 KB (HUD.DAT, sin reclamar: va dentro del
fichero entero).

| Medido (Flycast, menu real, OutRun) | antes (53fbf08) | despues |
|---|---|---|
| RAM libre al arrancar | 367 KB (235 en huecos) | 843 KB (239 en huecos) |
| leyendo BK00 / BKD / BO01 | 327 / 253 / 253 KB | 811 / 738 / 737 KB |
| despues de soltar la etapa y leer SEG05 | 364 KB | 849 KB |

Las pantallas del menu (titulo, principal, modo, coche, cambio, musica) se ven igual (capturas en el scratchpad
`modos/p_hoja.png`).

### Modos INFARTO y CONTRARRELOJ (2026-11-09)

- RAM: el modo OutRun queda igual salvo ~17 KB (codigo de comun/infarto.c, la cola de voces mas larga y un bufer de 4 KB
  para las tablas de coches). En el INFARTO, al empezar su carrera: las tablas de INFARTO.DAT (13 KB; las de coches,
  51 KB, se leen del GD de 2 en 2 sectores en cada accion -14, ~10 por etapa) y las de HAHUD.DAT (hudd_abre parcial,
  ~22 KB), que se liberan al volver al menu. Medido en Flycast (recorrido real del menu, PALM -> BK00 -> LAKE, 2026-11-09):
  OutRun 342 / 269 / 248 KB libres al leer BK00 / BKD / BO01 (sin estos cambios 359 / 286 / 265), INFARTO 313 / 240 /
  220. Con el HUD de los corazones dentro de HUD.DAT (+31 KB siempre en la RAM) la lectura de BKD fallaba
  ("No se pudo leer la etapa siguiente"): por eso van aparte. El margen ya era poco (la RAM del arranque bajo de 699 KB
  a ~375 KB con los cambios de la musica del menu): cualquier aumento puede volver a romper la lectura de la union.
- AICA: el juego de voces del modo se cambia al empezar la carrera (sfx_voces_juego): VOCES.DAT (207 KB) o VOCESH.DAT
  (740 KB, 117 muestras); se lee entero en el hueco de la union aun prestado, asi que no pide RAM.
- VRAM: HAHUD.DAT es una textura VQ de 128 x 512 (10 KB) que se sube despues de reservar la zona del curso (va fuera
  de la zona). Dentro de HUD.DAT partia la zona del curso (1344 -> 832 KB): no se hace.
- TA: el CONTRARRELOJ no abre trafico (menos vertices y CPU que el OutRun).

### CPU de dibujo (2026-11-10)

Medido con un cuadro congelado (`-DDIAG_CONGELA=600`: un paso de logica por cuadro hasta el paso 600, asi el estado no
depende de los fps, y despues siempre el mismo cuadro; sin temblor de la carroceria ni histeresis del corte del TA) con
`-DDIAG_MS -DDIAG_SIN_CPU`, y perfil por muestreo del SH4 (`-DDIAG_PERFIL`, `demo/perfil.c`: el TMU1 a 20 kHz cuenta el
PC en cubos de 16 bytes; con DIAG_CONGELA vuelca los 400 mas llenos a los 300 cuadros congelados, para `addr2line`). Las
capturas del cuadro congelado se comparan pixel a pixel antes / despues: solo cambian el punto que parpadea del mapa
del HUD y la linea del mar en el horizonte de PALM (cambian igual entre dos capturas del mismo binario).

Lo que costaba (PALM, 25,3 ms): el recorrido de las mallas de la etapa (31 %: ~30000 mallas por cuadro para dibujar
~3000), el bucle de vertices (28 %, ~50 instrucciones por vertice: GCC no usa fmov con predecremento y recargaba la
direccion de la Store Queue de `pvr_dr_addr` en cada vertice), el piloto (7 %) y `nodos_visibles` (4 %).

- Tramos de nodo (`modelo_t.tr`, `tramos_nodo`): las mallas van por textura y dentro por nodo (unas 8 seguidas del mismo
  nodo); al leer el modelo se guarda por lista el fin y el nodo de cada tramo (4 bytes por tramo: 15 KB en SEG00, 18 KB
  en SEG13, del monton) y `dibuja_rango` salta de una vez los tramos de nodos no visibles. Mismas mallas en el mismo
  orden (dentro del tramo se sigue mirando malla a malla). Sin RAM para la tabla, como antes.
- Bucle de vertices en ensamblador (`sq_vtx`, `sq_vtx_ofs`, `sq_pt`): 27 instrucciones por vertice (xyz con `fmov.s
  @Rn+` y `ftrv`, u / v / color copiados como palabras, 1/w con FSRRA como `inv_w`, el vertice escrito en la SQ de atras
  adelante con `mov.l` / `fmov.s @-Rn` y la direccion de la SQ en un registro; `pref` 72 bytes por delante del vertice
  para la cache de la consola). Los mismos valores llegan a la PowerVR.
- Coche y trafico: si el modelo entero no esta delante del plano cercano, cada malla se mira por su esfera (contiene
  sus vertices en los 15 coches, los 9 de trafico y CHR.DAT): w del centro con la matriz cargada menos r por lo que
  mas cambia w por metro; si pasa de 1,5 x cerca, directa. Piloto: las piezas rigidas (un hueso) sin acumular pesos, y
  las tiras sin recorte por `sq_pt` (tambien `dibuja_malla`).

- `dibuja_etapa`: el recorrido de las etapas y uniones con tabla de tramos (lo mismo que `dibuja_rango` con recorte:
  mismas mallas, mismo orden, mismas pruebas) con el ojo y los ejes en variables locales, el cartel una vez por tramo
  y `dibuja_malla_directa` en linea (una llamada por malla costaba tanto como sus vertices).
- `ta_teo_tab`: el coste teorico del TA de cada tira (`ta_teo`) por tabla para n <= 256.

| Flycast, CPU de dibujo (ms) | congelado antes | congelado despues | carrera antes: dib + log, fps | carrera despues: dib + log, fps |
|---|---|---|---|---|
| PALM | 25,4 | 15,5 | 26,3 + 2,9, 33 | 15,9 + 1,9, 53 |
| LAKE | 23,0 | 13,5 | 23,6 + 3,8, 36 | 14,4 + 2,5, 55 |
| INDU | 29,9 | 17,6 | 27,1 + 3,2, 32 | 15,6 + 2,0, 53 |
| METR | 33,9 | 18,9 | 30,6 + 3,1, 29 | 17,5 + 2,0, 49 |
| CAPE | 23,8 | 13,4 | 23,0 + 2,7, 38 | 14,4 + 1,8, 57 |

Carrera: el piloto 45 s desde la salida (`-DPRUEBA_AUTO -DDIAG_MS -DDIAG_SIN_CPU`, media de los MS); "log" baja porque
con mas fps hay menos pasos de logica por cuadro. Los vertices y el presupuesto del TA no cambian (mismas tiras, misma
cuenta `ta_cabe`). Lo que queda (PALM congelado): la etapa 6,5 ms (2 de vertices), el coche 2,7, el piloto 2,2,
`nodos_visibles` 1,5 y el principio del cuadro 1,6 (`coche_entorno`, matrices). En la consola el recorte de la
distancia por CPU (`ms + ms_log > 15`) se activara menos: mas cerca de la distancia del original.

Flycast no cuenta la cache ni las esperas de la memoria: en la consola el recorrido de las mallas de antes leia ~950 KB
de `malla_t` por cuadro, asi que lo ganado alli deberia ser al menos lo mismo.

## 3. Otros riesgos de la consola real

| Riesgo | Estado |
|---|---|
| Lecturas del GD de la musica mezcladas con las cargas | KallistiOS las serializa, pero en un GD real la cabeza saltaria entre ficheros en cada trozo (~100-200 ms por salto). `gd.c` marca las lecturas que no son del hilo de la musica (`gd_hilo_fondo`, `gd_ocupado`) y el lector espera 300 ms tras cada una (salvo con el anillo casi vacio). Con la carga de etapas en segundo plano leen dos hilos: cada lectura de `gd.c` va entera bajo un mutex propio y se repite si falla (con ordenes solapadas KallistiOS devolvia ERR_SYS en Flycast). Las cargas de etapa (bifurcacion, vuelta a SEG00) van entre `musica_pausa_lectura(1/0)`; mientras tanto el lector alimenta el AICA (`snd_stream_poll` con un mutex), porque el bucle no llama a `musica_poll`. El anillo de 1 MB son ~21 s de ADPCM a 48 kHz. `musica_pon` ya no espera sin fin el colchon (3 s como mucho). |
| Store Queues (`pvr_dr`) y otros hilos | `pvr_list_begin` toma el mutex de las SQ (`sq_lock`) hasta `pvr_list_finish`; `snd_stream` y `pvr_txr_load` tambien lo toman. El hilo de la musica solo usa SQ (via `snd_stream_poll`) durante las cargas, fuera de las listas. Correcto. |
| Tiras sin fin (EOL) | los efectos saltaban vertices sueltos detras del plano cercano y la tira se quedaba abierta antes de la siguiente cabecera (parametro ilegal para el TA real). Ahora cada cuadrilatero va entero o no va (`pt_cuad`). |
| Cache y DMA del GD | `cdrom_read_sectors_ex` invalida la cache sobre el destino; todos los destinos van alineados a 32 y en multiplos de 2048 (`gd_carga`, `hud.c`, el lector de musica). Sin DMA hacia memoria que la CPU toque a la vez. |
| `snd_sh4_to_aica` | en esta version de KallistiOS no espera al ARM; `sfx.c` solo manda cambios (frecuencia > 0,2 % o volumen), unas pocas ordenes por cuadro. |
| Flujos de audio en canales >= 32 | KallistiOS arranca los dos canales de un `snd_stream` estereo con un mapa de 32 bits (`aica_cmd_t.cmd_id`, `aica_sync_play` del ARM): con canales >= 32 nunca suenan. La musica pide su flujo antes que `sfx.c` (canales 0 y 1); la pelicula recibe el 33 y el 34 y `video.c` los enciende a mano (ver Peliculas). Cualquier flujo nuevo pedido despues de `sfx_inicia` tiene el mismo problema. |
| Musica a 48 kHz | el AICA reproduce cada canal con su paso (OCT/FNS); 48 kHz es un paso algo mayor que 1 sobre la salida de 44,1 kHz: valido. |
| Alineaciones | blob de texturas a 32 (cada textura en multiplos de 32, `pvr_txr_load` copia de 32 en 32), vertices de 24 bytes (floats a 4), texturas VQ en direcciones multiplo de 8. |
| CPU | Flycast no reproduce el tiempo del SH4; el acortado de seguridad de `lejos` usa el reloj real (`ms + ms_log`), asi que se ajusta solo en la consola. |
| Video | VGA: 640x480 a 60 Hz. Sin VGA: NTSC entrelazado a 60 Hz (el generico de KallistiOS). En una consola PAL (flash: Europa) sin VGA se pregunta a 50 Hz: A = 60 Hz, B o nada en 10 s = 50 Hz (`elige_video`). La logica va a 60 pasos por segundo con el reloj (salto de cuadros), asi que a 50 Hz va a la misma velocidad. Probado en Flycast con region Europa y emision PAL. |
| Arranque desde GDEMU | `tools/mkgdi.py`: IP.BIN de `makeip` con areas JUE (sin region), soporte VGA (perifericos E000F10), 1ST_READ.BIN sin desordenar (GD, no CD-R) al final de la pista 3 (LBA 45000). |

### Peliculas (comun/video.c, 2026-11-09)

Fallo en la consola (GDEMU, PAL, RGB): "muestra el nombre del titulo, se congela y no hay audio"; se daba por bueno en
Flycast. **Causa, reproducida en Flycast con el video.c y los .VQV de antes y los datos actuales:** el flujo de audio
de la pelicula recibia los canales 33 y 34 del AICA (sfx.c ya habia reservado del 2 al 32) y KallistiOS no los
enciende (ver la tabla de arriba: KEYONB a 0 en el registro del canal, posicion 0, snd_stream no pide datos nunca).
El anillo de audio (128 KB, 2,7 s a 48 kHz) se llenaba, el lector del GD se paraba esperando sitio y el video se
quedaba en el cuadro 76 (~3 s: con la pelicula de titulo primero, justo cuando aparece el rotulo) sin sonido. Las
demas hipotesis no hacian falta para explicarlo, pero se han cubierto igual:

| Hipotesis | Comprobado / hecho |
|---|---|
| (a) caudal | Antes 0,9 MB/s (512x256, libro por cuadro, audio a 48 kHz, cuadros alineados a sector). Ahora ~440 KB/s (256x256, libro por grupo de hasta 12 cuadros y en cada corte de plano, audio a 32 kHz, registros de tamano variable): MVATTR 441 KB/s, MVTS 436 KB/s. Calidad (PSNR contra el MPEG a 640x480, cuadros de 8, 20 y 38 s de attr50): 35,4 dB antes, 33,7 ahora; rotulo de ts50: 31,1 y 27,1 dB (mas blando, legible). Las .VQV van al final del disco (`mkgdi.py`, peso 2: LBA ~533000-549000), la zona mas rapida en un GD de velocidad angular constante. |
| (b) lector | Lecturas de 64 sectores (128 KB) seguidas, por DMA a destinos alineados a 32 que nunca dan la vuelta al anillo; `gd.c` cuenta los fallos (`gd_errores`) y con ERR_DISC_CHG / ERR_NO_DISC reinicia la unidad antes de repetir. El lector va con prioridad 9 (el bucle 10). |
| (c) dos snd_stream | El flujo de la pelicula usa la llamada directa (`snd_stream_set_callback_direct`: el SH4 copia el audio a la RAM del AICA con `spu_memload_sq`), sin el DMA del G2 ni el semaforo `stream_sem` que comparte con la musica, y nunca devuelve 0 (con 0 snd_stream rellena por DMA): si falta audio rellena con silencio (0x80). Si el AICA no pide datos en 2,5 s el audio se tira y el video sigue. |
| (d) CPU | El bucle se para en `pvr_wait_ready` cada cuadro; el lector duerme cuando el anillo esta lleno. Musica parada y sin leer (`musica_para`, `musica_pausa_lectura(1)`) durante la pelicula. |
| (e) primer cuadro | Precarga de 1,5 s (o el anillo lleno) pintando en negro; con tope de 10 s. |

RAM: desde 2026-11-09 los anillos van en el hueco de la union prestado (`video_bufer`, ver RAM del arranque y del
menu): anillo de video de 1 MB (~2,3 s) + 2 x 64 KB de audio, sin pedir nada al monton. Sin bufer prestado, como
antes: lo que quepa en el monton dejando 192 KB libres (512 KB-1 MB). VRAM: 2 texturas de 18 KB.

Diagnostico para el probador (como `pvr_colgado`): si no llega un cuadro nuevo en 1 s, o la PowerVR no termina 20
cuadros seguidos, la pelicula se para, escribe sobre la imagen (cuadros leidos/vistos/reloj, KB/s del GD leyendo y de
media, peor lectura, errores del GD y del lector, estado del lector, audio y sus canales, faltas de audio) y a los
12 s (o con START) sigue al titulo. Un hilo vigilante (prioridad 8) escribe lo mismo con el paso en que esta
("COLGADO en 'snd_stream_poll'", etc.) si el bucle no da senales en 1,5 s: el hilo principal bloqueado dentro de
KallistiOS. La letra es la de la BIOS copiada al empezar (bfont espera a que el GD quede libre: con el GD colgado se
bloquearia). En Flycast el texto sale sobre negro (Flycast no copia a la VRAM lo que dibuja la PowerVR); en la
consola sale sobre el ultimo cuadro. Ademas cada pelicula deja en la consola serie la precarga, los canales del audio
y un resumen con el caudal medido.

Medido en Flycast (2026-11-09, las dos peliculas enteras con audio, sin faltas): lectura 1860 KB/s (64 sectores en
69-72 ms), media 438 KB/s. Lector limitado con `-DPRUEBA_VIDEO_KBS=470`: igual, sin faltas de audio; a 300 KB/s la
imagen va a saltos (siempre el ultimo cuadro leido) pero no se para; a 20 KB/s: diagnostico "sin cuadros nuevos en
1 s" y al titulo. `-DPRUEBA_VIDEO_COLGADO=100` duerme el bucle en el cuadro 100: el vigilante escribe el diagnostico.
SIN VERIFICAR en la consola: el caudal real del GDEMU/GD con la pelicula fuera del disco (el diagnostico lo dira si no
llega).

## Espacio del GD-ROM (2026-11-10)

mkgdi.py: track03 desde el LBA 45000 hasta 549150 - 32: (549118 - 45000) x 2048 = 1 032 433 664 bytes (984 MiB). Los
datos de antes de C2C ocupaban 599 MiB (musica 450 MiB, etapas del modo OutRun 78 MiB). Las 47 etapas que faltaban
(15 de OutRun 2 SP, 30 al reves, PALM_T, BEAC_T; conv_union.py etapa_ficheros: SEGnn + COLnn) son unos 250 MiB; el
arbol de OUTRUN2SP (10 bifurcaciones de ~1,4 MiB y 5 metas de 0,3..0,8 MiB) 18 MiB; las misiones 0..7 de Races.bin
(15 uniones LRBK / SUMOEND) ~16 MiB. Quedan ~100 MiB: no caben las 230 uniones que faltan para todas las misiones
(~280 MiB: cada union lleva la luz de su etapa cocida en los vertices, el cielo de la etapa y los carteles de sus
destinos). PALM_BT / BEAC_BT / PALM_BR / BEAC_BR no se convierten (ningun curso de Races.bin ni csc_data los usa).

Uniones empaquetadas (2026-11-10, tools/pak_union.py, comun/gd.c): las 262 uniones de todos los arboles (BK00..BK261:
modo OutRun, OUTRUN2SP y las 94 misiones de Races.bin) ocupaban 334,5 MiB como ORD7 sueltos; casi todo se repite entre
ellas (las 194 LRBK son el mismo cs_LRBK_PALM con la luz de la zona cocida en el color de los vertices, los mismos
porticos con dos carteles de destino y el cielo de su etapa). UNIONES.PAK guarda una vez cada trozo distinto (corte por
contenido, ~1,3 KB de media; los vertices en dos flujos, x y z u v y color, porque la luz solo cambia el color) y cada
BKnn.DAT queda en un descriptor de < 4 KB con las operaciones que recomponen el ORD7 de antes byte a byte (pak_union.py
lo comprueba al escribir; gd_carga_en lo hace en la consola, gd_tam da el tamano recompuesto para el hueco de la
union). 334,5 MiB -> 21,6 MiB (paquete 21,1 MiB). Datos: 887 MiB con 47 uniones -> 854 MiB con las 262 (130 MiB
libres). Lectura: las operaciones van por orden de origen en el paquete con una ventana de 64 KB (memoria temporal; 16
KB o un sector si no hay), asi que la cabeza solo avanza; como mucho 13 saltos de mas de 64 KB por union (~1,3..2,6 s
mas en un GD real, a 100-200 ms por salto; la union se pide 2500 m antes del final de la etapa, mas de 25 s antes).
En Flycast BK30: 0,95 s suelta, 1,46 s empaquetada; BK100 0,96 / 1,56 s. Sin memoria del monton: la ventana es lo que
sobra del hueco de la union detras del fichero (o el sector de gd.c) y el descriptor un sector fijo (2 KB de RAM): con
una ventana pedida a malloc el hueco que dejaba hacia fallar BKD.DAT detras de BK135 (misma RAM libre, otro reparto).
Comprobado: `-DDIAG_PAK` imprime la huella del ORD7 recompuesto en la consola (`pak_union.py --huella` la del suelto):
BK100 1aa403be y BK101 8593a659 iguales; cuadro congelado (DIAG_CONGELA) de BK30 igual pixel a pixel; en BK100 76
pixeles distintos, todos del tiempo de carga (el punto que parpadea en el mapa del curso, un coche del trafico y una
farola a lo lejos), no de la union.
RAM: con todos los arboles en CURSO.DAT los huecos crecerian a 8224 KB (SEG46, arboles 125, 177, 187, 192) y 1730 KB
(las 7 uniones con el cielo de FLOR_R: BK145, 161, 166, 219, 225, 241, 253): "Out of memory" al arrancar. curso_huecos
los deja en 7842 / 1480 KB (HUECO_*_MAX, los de antes) y avisa de los 14 ficheros que no caben; esas carreras se paran
con su error. VRAM (tools/vram.py --zona 1344, todos los arboles): BK241 + LASV -254 KB y BK135 + LASV -19 KB (la demo
sube entonces fuera de la zona o rebaja). En las metas de las misiones META.DAT no cabe (8 KB: "META: sin META.DAT")
tambien con las uniones sueltas. PENDIENTE todo eso.
Al convertir mas uniones: `conv_union.py iso dir/ --arboles=1,100,...,193 [--parte=k/n]` sobre los ORD7 sueltos y despues
`pak_union.py dir/ salida/` con todas (el paquete se rehace entero).

Etapas empaquetadas (2026-11-10, `pak_union.py --pak=SEGS.PAK --prefijo=SEG --tope=7842`): lo mismo con las SEGnn.DAT
(las 62 etapas de todos los arboles; las de las misiones repiten modelo, texturas y geometria de su zona con otra luz).
Byte a byte se repetian 29 MiB entre etapas (corte por contenido; alineado a sectores solo 2,3 MiB) y apenas nada en
lo demas (musica 3 MiB en sectores de silencio, coches 0,7, voces 0,3, colisiones 0,3; ficheros enteros iguales: 9
COLBK de 16 KB y 3 descriptores de union). 60 etapas: 289,1 -> 262,1 MiB (SEGS.PAK 262,0 MiB); SEG13 y SEG46 van
sueltas (no dejan sitio en el hueco a la ventana y al descriptor). Datos: 852,7 -> 825,6 MiB (**27,1 MiB menos**).
El descriptor de una etapa ocupa varios sectores (hasta 1007 operaciones, 12 KB): `pak_lee` lo lee al final del hueco
de la etapa y usa de ventana (hasta 128 KB) lo que queda entre el fichero y el descriptor; despues la colision va detras
del fichero como antes. `gd_tam` da el tamano recompuesto (curso_huecos) y guarda los tamanos (512 ficheros) para no
leer un sector por pregunta. Comprobado con `-DDIAG_PAK` y `pak_union.py --huella` de los sueltos: SEG00 850e04b1,
SEG03 032199a3, SEG08 b1637383 iguales (y el empaquetador recompone y compara las 60). Lectura en Flycast: hasta 14
saltos de mas de 64 KB por etapa; SEG03 2,68 -> 3,82 s y SEG08 3,80 -> 5,47 s con la ventana de 64 KB (varios Flycast
a la vez), SEG03 3,52 s con la de 128 KB; en un GD real ~1,4..2,8 s mas por los saltos; la etapa se pide al soltar la anterior, ~1000 m (~12 s a 300 km/h) antes de necesitarla.

## 4. Pruebas

- Entorno y datos desde cero (contenedor nuevo): `tools/instala_entorno.sh` (KallistiOS d81aa3f con kos-chain stable,
  GCC 15.2; Flycast Release sin Vulkan; paquetes de apt y pip) y `tools/regenera_datos.sh <iso> <datos/>` (todos los
  conversores, 4 pasos a la vez con un hilo cada uno, intermedios en /dev/shm; idempotente). Regenerado el 2026-11-10:
  513 ficheros, 852,7 MiB (62 SEG + 62 COL, 262 descriptores BKnn + UNIONES.PAK de 20,1 MiB, 62 MUS; con SEGS.PAK desde
  el 2026-11-10: 514, 825,6 MiB); huellas de
  BK100 / BK101 sueltas 1aa403be / 8593a659, las de arriba; ~2 h 40 min en 4 nucleos. Pruebas en Flycast:
  `tools/prueba/` (nav.sh, nav3.sh, pal.sh; ver su README). Con todos los arboles el arranque tarda minutos en
  Flycast antes de la pelicula: curso_huecos hace miles de gd_busca y cada una relee el directorio del GD (PENDIENTE:
  guardarlo una vez).

- `cd demo && make` sin avisos; `make CPPFLAGS=-DDIAG_VTX` imprime por la consola serie cada 60 cuadros los
  vertices (total y etapa), la CPU, la distancia, la VRAM libre, el presupuesto del TA (3D, etapa y su tope, 2D,
  `ta_f`), las tiras aceptadas y su cuenta teorica (`tiras`, `teo`), lo que dibujaria el original de la etapa (`orig`,
  vertices de los nodos visibles), los cortes del reparto (`cerca`, `clase`, `alfa`), el nodo quitado mas cercano
  (`desde`, su radio) y la niebla; en la consola, lo que gasto de verdad el TA (`real`, `opb`). `-DDIAG_SIN_CPU` deja
  `lejos` fijo (para medir en Flycast, que no da el tiempo del SH4). `-DDIAG_SIN_COCHE` no dibuja el coche del jugador
  (para ver al piloto y a la acompanante); `-DDIAG_RUTA` cuenta las reservas de la ruta y el monton si una falla.
- En la consola: si se cuelga, foto del texto de `pvr_colgado`; con `DIAG_VTX` y un cable serie, las lineas `DIAG`.
