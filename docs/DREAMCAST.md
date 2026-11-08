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
tamano del mayor SEGnn.DAT: 6090 KB, METR) y la union en otro (el mayor BKkk.DAT: 1568 KB), reservados al arrancar.
La ruta es una ventana: al soltar una etapa se quitan sus puntos (`ruta_recorta`) y el marco se recentra en el
principio de la union (`curso_recentra`), asi que ni la RAM de la ruta ni las coordenadas crecen de etapa en etapa.

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
- compartir texturas (las de cs_BK_PALM van repetidas en cada BKkk.DAT): ahorraria VRAM, no TA; pendiente.

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

Bufer de parametros mas grande: no cabe. Cada 64 KB mas son 128 KB de VRAM (doble bufer) y en el peor caso medido
quedan 79 KB libres (seccion 1). Quitar juegos de reserva del OPB daria sitio (154 KB por juego) pero el OPB crece
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

## 3. Otros riesgos de la consola real

| Riesgo | Estado |
|---|---|
| Lecturas del GD de la musica mezcladas con las cargas | KallistiOS las serializa, pero en un GD real la cabeza saltaria entre ficheros en cada trozo (~100-200 ms por salto). `gd.c` marca las lecturas que no son del hilo de la musica (`gd_hilo_fondo`, `gd_ocupado`) y el lector espera 300 ms tras cada una (salvo con el anillo casi vacio). Con la carga de etapas en segundo plano leen dos hilos: cada lectura de `gd.c` va entera bajo un mutex propio y se repite si falla (con ordenes solapadas KallistiOS devolvia ERR_SYS en Flycast). Las cargas de etapa (bifurcacion, vuelta a SEG00) van entre `musica_pausa_lectura(1/0)`; mientras tanto el lector alimenta el AICA (`snd_stream_poll` con un mutex), porque el bucle no llama a `musica_poll`. El anillo de 1 MB son ~21 s de ADPCM a 48 kHz. `musica_pon` ya no espera sin fin el colchon (3 s como mucho). |
| Store Queues (`pvr_dr`) y otros hilos | `pvr_list_begin` toma el mutex de las SQ (`sq_lock`) hasta `pvr_list_finish`; `snd_stream` y `pvr_txr_load` tambien lo toman. El hilo de la musica solo usa SQ (via `snd_stream_poll`) durante las cargas, fuera de las listas. Correcto. |
| Tiras sin fin (EOL) | los efectos saltaban vertices sueltos detras del plano cercano y la tira se quedaba abierta antes de la siguiente cabecera (parametro ilegal para el TA real). Ahora cada cuadrilatero va entero o no va (`pt_cuad`). |
| Cache y DMA del GD | `cdrom_read_sectors_ex` invalida la cache sobre el destino; todos los destinos van alineados a 32 y en multiplos de 2048 (`gd_carga`, `hud.c`, el lector de musica). Sin DMA hacia memoria que la CPU toque a la vez. |
| `snd_sh4_to_aica` | en esta version de KallistiOS no espera al ARM; `sfx.c` solo manda cambios (frecuencia > 0,2 % o volumen), unas pocas ordenes por cuadro. |
| Musica a 48 kHz | el AICA reproduce cada canal con su paso (OCT/FNS); 48 kHz es un paso algo mayor que 1 sobre la salida de 44,1 kHz: valido. |
| Alineaciones | blob de texturas a 32 (cada textura en multiplos de 32, `pvr_txr_load` copia de 32 en 32), vertices de 24 bytes (floats a 4), texturas VQ en direcciones multiplo de 8. |
| CPU | Flycast no reproduce el tiempo del SH4; el acortado de seguridad de `lejos` usa el reloj real (`ms + ms_log`), asi que se ajusta solo en la consola. |
| Video | VGA: 640x480 a 60 Hz. Sin VGA: NTSC entrelazado a 60 Hz (el generico de KallistiOS). En una consola PAL (flash: Europa) sin VGA se pregunta a 50 Hz: A = 60 Hz, B o nada en 10 s = 50 Hz (`elige_video`). La logica va a 60 pasos por segundo con el reloj (salto de cuadros), asi que a 50 Hz va a la misma velocidad. Probado en Flycast con region Europa y emision PAL. |
| Arranque desde GDEMU | `tools/mkgdi.py`: IP.BIN de `makeip` con areas JUE (sin region), soporte VGA (perifericos E000F10), 1ST_READ.BIN sin desordenar (GD, no CD-R) al final de la pista 3 (LBA 45000). |

## 4. Pruebas

- `cd demo && make` sin avisos; `make CPPFLAGS=-DDIAG_VTX` imprime por la consola serie cada 60 cuadros los
  vertices (total y etapa), la CPU, la distancia, la VRAM libre, el presupuesto del TA (3D, etapa y su tope, 2D,
  `ta_f`), las tiras aceptadas y su cuenta teorica (`tiras`, `teo`), lo que dibujaria el original de la etapa (`orig`,
  vertices de los nodos visibles), los cortes del reparto (`cerca`, `clase`, `alfa`), el nodo quitado mas cercano
  (`desde`, su radio) y la niebla; en la consola, lo que gasto de verdad el TA (`real`, `opb`). `-DDIAG_SIN_CPU` deja
  `lejos` fijo (para medir en Flycast, que no da el tiempo del SH4).
- En la consola: si se cuelga, foto del texto de `pvr_colgado`; con `DIAG_VTX` y un cable serie, las lineas `DIAG`.
