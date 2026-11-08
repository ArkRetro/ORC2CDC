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
| OPB: 300 teselas x (opacos 32 + translucidos 16 + recorte 16 palabras) = 76,8 KB, por 1 + 4 de reserva | 384 KB |
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
trafico crecen mas de ~120 KB, el margen se acaba ahi (primero: compartir las texturas repetidas de cs_BK_PALM y
cs_ENVBK_PALM, que van duplicadas en cada BKkk.DAT, o pasar a VQ las pequenas que aun no lo estan).
Despues (coches montados como el original y trafico con su LOD 0): el coche de mas texturas es COCHE06 (187 KB, +7
sobre los 180 de COCHE04) y el trafico 297 KB (+32): en ese peor caso quedan unos 85 KB (calculado, sin medir).

## 2. Presupuesto del TA

### Lo que se midio en la consola

Demo 12c en GDEMU, primer cuadro de carrera (diagnostico `pvr_colgado`):
`tri 22012 vtx 24656 err 000C`, `VB 1048576/1048576`, `OPB 141568/460800`. El bufer de parametros de 1 MB se lleno
entero (error 0x0C: bits 2 y 3) con 24656 vertices contados (sin los del HUD): **mas de 42,5 bytes por vertice**.
El OPB iba al 30 %.

### La cuenta teorica (se queda corta)

Formato de los parametros que guarda el TA para nuestros poligonos (cabecera tipo 0, vertice tipo 3: textura, UV de
32 bits, color empaquetado, sin color de desplazamiento):

- el TA corta cada tira en trozos de hasta 6 triangulos (8 vertices) que comparten 2 vertices;
- cada trozo: palabras ISP/TSP/TCW (12 bytes) + sus vertices de 24 bytes (x, y, z, u, v, color; 16 sin textura);
- tira de n vertices con k = techo((n - 2) / 6) trozos: `24 (n - 2) + 60 k` bytes: ~34 por triangulo en tiras
  largas, 84 un triangulo suelto, 108 un cuadrilatero (sprites del HUD, recortes).
- OPB: una palabra por trozo y por tesela de 32x32 que toca; el primer bloque de cada tesela esta en la zona fija y
  los siguientes salen de la reserva comun (bloques de 32 o 16 palabras, la ultima enlaza).

Con esta cuenta el cuadro que colgo la consola ocupaba unos 780 KB (unos 32 bytes por vertice): la consola gasto al
menos 1,34 veces mas. Por eso el presupuesto no se apoya en la formula sino en la medida.

### El tope (demo/main.c, `ta_*`)

Se cuentan **todos** los vertices del cuadro: etapa, coche, trafico y efectos (`ta_cabe`, antes de enviar cada
tira: si no cabe no se envia) y lo 2D (HUD, menus y textos: `ta_prim`; `hud.c` y `menu.c` se compilan con
`-Dpvr_prim=ta_prim` desde `demo/Makefile`). Cada vertice cuesta `ta_bpv` bytes: 56 de salida (1,3 x 42,5) y, en la
consola, el valor real del cuadro anterior x 1,2 si es mayor (registro `TA_ITP_CURRENT`; Flycast no lo actualiza).

| Tope por cuadro | Valor |
|---|---|
| total | `TA_VB_BYTES - TA_MARGEN` = 1792 - 64 = 1728 KB |
| 2D (va al final, no se corta) | max(64 KB, 2 x lo 2D del cuadro anterior x ta_bpv) |
| 3D (`ta_lim`) | total - 2D = 1664 KB |
| etapa (`ta_tope_etapa`) | 3D - max(1,25 x coche+trafico+efectos del cuadro anterior, coche + 3 coches de trafico) |

Medidas (Flycast, `-DDIAG_VTX`, con 56 B/vertice): peor cuadro de carrera 28263 vertices contados (etapa 19615,
coche 6972, trafico y efectos ~1700) + ~150 de HUD = **1545 KB de 1664**; con 42,5 B/vertice reales serian unos
1,2 MB del bufer de 1,75 MB (69 %). Peor menu: coche 6972 + 412 de 2D = 413 KB. El tope aguanta mientras la consola
gaste menos de ~60 B por vertice contado, y la calibracion sube `ta_bpv` sola si gasta mas de 46.

Coches del jugador (`tools/conv_coche.py`): tope de 11000 vertices dibujados a la vez por coche. Montados enteros como
el original (46A560, con las uniones de tiras que no dibuja el VU1 ya quitadas) dibujan 9320-12080, y el F40 17376 (su
pieza +14 es una copia entera de la carroceria que la carroceria tapa). Por que hace falta el tope: el 3D del cuadro son
1664 KB, unos 30400 vertices a 56 B; la etapa recibe lo que queda tras reservar 1,25 x (coche + trafico + efectos)
del cuadro anterior. Con el F40 entero y unos 3000 de trafico quedarian ~4960 vertices para la etapa (su pico medido
es 19615); con el tope, ~16000 (Flycast, `DIAG_VTX`, salida de PALM con el F40: 27159 vertices, etapa 15984 en su
tope de 874 KB). Por encima del tope se quita solo lo que no se ve desde las camaras del juego (seleccion, carrera con
deriva y, si cabe, una vuelta baja): 10 de los 15 pasan por ahi y quedan en 10348-11195; el render desde esas camaras
sale igual que el del original entero (0,00-0,09 % de pixeles distintos, de la compresion VQ). Las luces van todas en
el fichero (pilotos, con luces, freno y luz de faros: 76-490 vertices) pero solo se dibuja un juego (`coche_luces`).
El reflejo no anade vertices (una pasada con color de desplazamiento: 4 bytes mas por vertice en el bufer de
parametros, 1615-5347 vertices de entorno por coche, <= 22 KB) y cuesta en la CPU unas 4000 transformaciones de
normal por cuadro (`coche_entorno`). VRAM: COCHEnn.DAT 127-187 KB y el trafico 297 KB (antes 265: ahora el LOD 0 del
original, con sus ruedas y su sombra). Trafico: 823-1089 vertices por coche (antes 472-794: la carroceria del LOD 2
con ruedas puestas a ojo); lo cuenta `ta_cabe` como todo lo demas, y `ta_vmin_otros` reserva el coche y tres de
trafico. Un coche cada vez (`coche_carga` en `demo/main.c`: libera RAM y VRAM del anterior; en el menu, si no cabe,
echa la cache de pantallas).

OPB: 4 juegos de reserva (307 KB por cuadro); en la consola se midieron 141568 bytes en total (65 KB de reserva) en
el cuadro que se colgo (incompleto). Si el registro `TA_OPB_POS` pasa del 70 % del OPB, `ta_freno` baja el tope de la
etapa por cuartos y acorta la distancia.

Distancia: cada carrera y cada etapa nueva empiezan a `LEJOS_MIN` (600 m) y la distancia crece si sobra presupuesto
y CPU; si se corta geometria (`ta_corto`) o la CPU pasa de 15 ms, primero baja la distancia y, en el minimo, el
detalle (`detalle`: escala la distancia maxima de las mallas pequenas, `dmax` de conv_tramo.py). `VTX_ETAPA_MAX`
(20000) es el tope de CPU de la etapa.

El diagnostico `pvr_colgado()` (si `pvr_wait_ready` falla 5 veces seguidas escribe en la imagen el estado, los
vertices, el registro de errores y el uso de VB y OPB) se queda: es lo que permite medir en la consola.

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
| CPU | Flycast no reproduce el tiempo del SH4; la distancia adaptativa usa el reloj real (`ms + ms_log`), asi que se ajusta sola en la consola. |
| Video | VGA: 640x480 a 60 Hz. Sin VGA: NTSC entrelazado a 60 Hz (el generico de KallistiOS). En una consola PAL (flash: Europa) sin VGA se pregunta a 50 Hz: A = 60 Hz, B o nada en 10 s = 50 Hz (`elige_video`). La logica va a 60 pasos por segundo con el reloj (salto de cuadros), asi que a 50 Hz va a la misma velocidad. Probado en Flycast con region Europa y emision PAL. |
| Arranque desde GDEMU | `tools/mkgdi.py`: IP.BIN de `makeip` con areas JUE (sin region), soporte VGA (perifericos E000F10), 1ST_READ.BIN sin desordenar (GD, no CD-R) al final de la pista 3 (LBA 45000). |

## 4. Pruebas

- `cd demo && make` sin avisos; `make CPPFLAGS=-DDIAG_VTX` imprime por la consola serie cada 60 cuadros los
  vertices (total y etapa), la CPU, la distancia, el detalle, la VRAM libre, el presupuesto del TA (3D, etapa y su
  tope, 2D, `ta_bpv`) y, en la consola, lo que gasto de verdad el TA (`real`, `opb`).
- En la consola: si se cuelga, foto del texto de `pvr_colgado`; con `DIAG_VTX` y un cable serie, las lineas `DIAG`.
