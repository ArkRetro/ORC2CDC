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

Error RMS medio 4,4-4,8 (maximo 9). VRAM libre medida (Flycast, `DIAG_VTX`): en carrera 961 KB con SEG00 y 809 KB
tras la bifurcacion (antes ~290 KB, con buferes mas pequenos); en los menus la cache de pantallas (`comun/menu.c`)
ocupa lo que haya libre y echa las pantallas viejas.

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

Coches del jugador (`tools/conv_coche.py`): tope de 9200 vertices por coche. Montados enteros (carroceria, puertas,
volante, faros, pilotos y las cuatro ruedas del original) tienen 9400-11800; los que pasan se vacian (fuera lo que no
se ve desde las camaras del juego, el interior bajo una tapa oscura y, si hace falta, ruedas del modelo ligero) y
quedan en 6800-9130. Un coche cada vez (`coche_carga` en `demo/main.c`: libera RAM y VRAM del anterior; en el menu,
si no cabe, echa la cache de pantallas). `ta_vmin_otros` se recalcula con el coche cargado; con 9100 vertices de
coche el peor cuadro de carrera pasaria de los 1664 KB contados a 56 B/vertice y `ta_cabe` recorta la etapa de lejos
(en la consola, a ~42,5 B/vertice, cabe entero: unos 1,3 MB de 1,75).

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
| Lecturas del GD de la musica mezcladas con las cargas | KallistiOS las serializa, pero en un GD real la cabeza saltaria entre ficheros en cada trozo (~100-200 ms por salto). `gd.c` marca las lecturas que no son del hilo de la musica (`gd_hilo_fondo`, `gd_ocupado`) y el lector espera 300 ms tras cada una (salvo con el anillo casi vacio). Las cargas de etapa (bifurcacion, vuelta a SEG00) van entre `musica_pausa_lectura(1/0)`; mientras tanto el lector alimenta el AICA (`snd_stream_poll` con un mutex), porque el bucle no llama a `musica_poll`. El anillo de 1 MB son ~21 s de ADPCM a 48 kHz. `musica_pon` ya no espera sin fin el colchon (3 s como mucho). |
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
