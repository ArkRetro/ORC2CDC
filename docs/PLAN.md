# Plan de accion (tras la demo 31)

Normas de siempre: nada inventado (todo del codigo descompilado o de la ISO), la imagen y los calculos no cambian
al optimizar (cuadro congelado identico), cada fase se prueba en Flycast NTSC y PAL desde el menu y acaba en una demo.

## Fase 1. Memoria y recursos (base de todo lo demas)
1. Mapa de la RAM: que ocupa cada bloque al arrancar, en el menu y en carrera (por modo), con el hueco contiguo mayor.
2. Datos repetidos EN MEMORIA: buscar tablas, texturas, mallas y sonidos que esten dos veces (coche del menu y de
   carrera, texturas iguales entre etapa / union / trafico / HUD / menu, tablas de MENU.DAT que solo usa el menu,
   CHR / BAND / META / SALIDA) y dejar una sola copia con funciones de acceso (cache por huella: pedir / soltar con
   contador de usos) en vez de copias por modulo.
3. Cargar solo lo del modo en curso y soltar lo del menu en carrera (y al reves); nada del monton en carrera: todo en
   huecos fijos (etapa, union, colision) para que no haya fragmentacion.
4. Objetivo: >= 300 KB libres contiguos en carrera en todos los modos (INFARTO hoy: 66 KB) y las 4 etapas y 7
   uniones de las misiones que no caben.
5. VRAM: texturas iguales compartidas tambien en los arboles de las misiones (14 combinaciones se pasan de la zona).

Estado (2026-11-10): hecho 1, 2 (HUD.DAT y MENU.DAT; union y etapa comparten texturas), 4 (934 KB seguidos en carrera
en los tres modos; caben todas las etapas y uniones) y parte de 3 y 5: quedan las asignaciones pequenas del monton
en las cargas del curso (sin huecos fijos) y 5 combinaciones "union + LASV" que se pasan de la zona de VRAM
(docs/DREAMCAST.md, Mapa de la RAM y datos repetidos).

## Fase 2. Glitches
1. Confirmar en la consola real el cielo (demo 31) y lo que diga el probador: etapa y objeto de cada fallo.
2. Luces de freno del coche del jugador (y de marcha atras) y luces del trafico al frenar, como el original. HECHO
   (2026-11-10; el original no tiene luces de marcha atras: PARIDAD.md, Luces de freno).
3. Repaso por etapas con el piloto automatico en los 15 tramos + SP: hoja de capturas y lista de defectos.
4. Humo / efectos provisionales por los del original; cartel de destino al final de etapa.

## Fase 3. Cerrar el modo OutRun
1. Tras la meta: tabla de tiempos por etapa, clasificacion con iniciales, millas OutRun, continuar; voz de la meta.
   Millas OutRun, continuar (REINTENTAR / ABANDONAR) y GAME OVER HECHOS (2026-11-11, tambien en el fin del tiempo y
   desde la pausa; PARIDAD.md, Fin de la carrera); faltan la tabla de tiempos, los records (bandera de las millas) y
   la voz de la meta.
2. OUTRUN2SP elegible en el menu (HECHO 2026-11-11: [780260] = 0 y sus reglas; el frontal de recreativa
   SELECTOR_CVT HECHO 2026-11-11 (comun/fsp.c, PARIDAD.md) y la vuelta a el tras el GAME OVER; los resultados (modo 25, el
   mapa de la ruta) HECHOS 2026-11-11; la entrada de nombre (26) y los rankings de OUTRUN2SP (VMU) HECHOS 2026-11-11; el final (24) y los 15
   recorridos continuos (presets 2 / 3: el PS2 los ensena, [0x54c108] = 2) siguen SIN PORTAR: PARIDAD.md, OUTRUN2SP; la pantalla de carga, el GAME OVER (27) de la pausa, la meta y el fin del
   tiempo, la salida en (0, -16) con el banderero visible y el trafico de las etapas al reves, HECHOS). Vista interior. Saltos. Bonus de puntuacion que faltan.

## Fase 4. COAST 2 COAST
1. Menu de categorias y mapa de misiones (con la chica). 2. Misiones de carrera con rivales (IA, choques).
3. Notas, recompensas, desbloqueos y progreso en la licencia. 4. Campeonatos de encargos.

## Fase 5. INFARTO y CONTRARRELOJ completos
1. Objetos en carretera y sus tipos de encargo; hoja de resultados; silueta de la chica; reacciones.
2. Contrarreloj: eleccion de recorrido, fantasma (VMU), records, resultados.

## Fase 6. Fidelidad y consola real
1. Confirmar en el ELF de PS2 los ~30 puntos PROBABLE / SUPUESTO. 2. Salon y tienda de millas. 3. Opciones completas.
4. Medir FPS y memoria en la Dreamcast real y ajustar.

## Plan de recursos (2026-11-12)

Pedido del usuario: (a) la tasa fija a 45 cuadros por segundo, (b) mas distancia de dibujo, (c) texturas comprimidas
con la VQ de la PowerVR y (d) un plan de recursos sin aproximaciones, con los valores de la ISO o del codigo
descompilado. Regla: lo del juego (texturas, geometria, LOD, distancias, tiempos de la logica) sale de la ISO / el
descompilado y no se toca; lo que se decide aqui es del port (formatos, presupuestos, ritmo de presentacion) y va
dicho como tal. Todo medido, nada supuesto: Flycast (`DIAG_VTX`, `DIAG_MS`, `DIAG_PVRMEM`), `tools/vram.py` y el
inventario de los .DAT.

### 1. Inventario (antes del plan; datos de dcwork/datos2 del 2026-11-12, codigo 5e6e27a)

**VRAM de la carrera, modo OutRun** (8 MB; mapa `PVRMAPA` de docs/DREAMCAST.md, Carreras seguidas, y Flycast con
`-DDIAG_VTX`, COCHE06 de GHOS a la meta de IMPE):

| Parte | KB | Que es |
|---|---|---|
| bufer de parametros del TA x 2 | 2 x 1792 | geometria del cuadro (doble bufer: el TA llena uno mientras se dibuja el otro) |
| OPB x 2 | 2 x 384 | 1 + 4 juegos de reserva (en la consola se midio un cuadro: 141 KB usados) |
| imagen 640x480x16 x 2 + teselas | 2 x 607 | |
| texto de pruebas (`txt_arr` 512x128 + `txt_cen`) | 144 | solo se ve con Y (`ver`) o sin HUD |
| trafico (9 modelos, 5 LOD) + sol | 347 | |
| efectos (marcas, humo) | 15 | |
| pareja (CHRppaa) + coche | 198 (CHR1109 + COCHE13) | coche 127-187 KB segun el modelo |
| HUD de carrera (HUD.DAT, paletas de 4 / 8 bits) | 522 | el atlas de los avisos (cuenta, META, se acabo el tiempo) 256 KB en 16 bits: 576 colores, sin paleta |
| zona del curso (etapa + union) | 1312 | lo que queda menos la reserva (48 KB), la pausa y el banderero |
| libre fuera de la zona | 57-98 | |

INFARTO: la zona es 92 KB menor (`HA_VRAM_RESERVA`, HAHUD.DAT fuera de la zona). OUTRUN2SP: 1344 KB.

**Texturas del curso por formato** (todas las del arbol del modo OutRun; KB en VRAM, alineadas a 32):

| Etapa | VQ | VQ con mipmaps | 16 bits (>= 32x32) | 16 bits (< 32x32) |
|---|---|---|---|---|
| PALM (SEG00) | 363 | 107 | 48 | 7 |
| LAKE (SEG03) | 332 | 129 | 20 | 3 |
| INDU (SEG04) | 514 | 93 | 2 | 5 |
| ALPI (SEG05) | 406 | 68 | 160 | 15 |
| SNOW (SEG06) | 324 | 121 | 328 | 2 |
| CLOU (SEG07) | 309 | 72 | 216 | 0 |
| CAST (SEG08) | 355 | 62 | 10 | 8 |
| GHOS (SEG09) | 390 | 65 | 354 | 0 |
| FORE (SEG10) | 423 | 166 | 146 | 6 |
| DESE (SEG11) | 197 | 86 | 50 | 2 |
| TULI (SEG12) | 302 | 140 | 260 | 8 |
| METR (SEG13) | 344 | 78 | 98 | 1 |
| RUIN (SEG14) | 329 | 31 | 96 | 0 |
| CAPE (SEG15) | 321 | 141 | 144 | 2 |
| IMPE (SEG16) | 684 | 79 | 144 | 2 |
| uniones BK00-14 | 91-259 | 7-112 | 0-2 | 0-1 |

En los 62 SEGnn.DAT de todos los arboles: 13360 KB en 16 bits de 32x32 o mas (529 texturas) que `tools/vq.py` dejo
sin VQ por pasar su error RMS 9 (`VQ_UMBRAL`); en las 262 uniones 2220 KB (221); en las parejas 496 KB (53). Son
texturas con ruido fino (rocas, montes, gente, hojas): la VQ da 9-17 de error y a la vista no se distingue (abajo).
Coches del jugador: 26 texturas en 16 bits (322 KB en los 15; 32-40 KB en el que se usa): insignias y logotipos.

**Peor caso de VRAM del curso** (`tools/vram.py`, COCHE06): BK08 / BK09 + IMPE 1127 KB en la zona -> 185 KB libres
(medido en Flycast: "zona libre 185K"); INFARTO 92 KB menos (19-93 KB con el peor coche y la peor pareja);
OUTRUN2SP BK19 + LASV 1286 KB (58 KB libres con una zona de 1344).

**TA** (Flycast, `-DDIAG_VTX -DDIAG_SIN_CPU`, el piloto de GHOS a la meta de IMPE con COCHE06): tope de la etapa
712-718 KB de los 1728 del cuadro (el resto: coche ~600 KB, trafico, 2D y la reserva), usados 585-595; se dibuja
entero hasta 362 m (GHOS) / 430 m (IMPE) y lo primero que se quita esta a 400 / 484 m (medianas de `cerca` y
`quitados desde`); 26000-27000 vertices por cuadro. Lo que limita la distancia es el bufer de parametros, no la CPU
(docs/DREAMCAST.md, Presupuesto del TA).

**CPU** (Flycast, mismas pruebas, ms por cuadro; media de las lineas `MS` / `PRF`): GHOS dibujo 14,95 + logica 4,48
(con la carga de la union), IMPE 15,43 + 2,64; partes: principio del cuadro 2,1-2,3, visibilidad 0,9-1,1, cielo 0,4,
etapa 4,9-5,6, coche 2,6, piloto y acompanante 2,2, trafico 0,2-0,7, 2D 0,4. 54-56 cuadros por segundo. Limites de
Flycast: no cuenta la cache ni las esperas de la memoria del SH4 y la PowerVR emulada no tarda lo que la real
(`pvr 7,5` es el tiempo de Flycast): las cifras sirven para comparar antes / despues, no como tiempo de la consola.
Ademas el marcador de pruebas se rehacia cada 6 cuadros aunque no se viera (128 KB borrados, escritos y subidos y
`mallinfo` recorriendo el monton).

**RAM** (docs/DREAMCAST.md, Carreras seguidas): bloques fijos de la etapa (8200 KB), la union (1730 KB), el coche
(424 KB) y la pareja (272 KB); al empezar la carrera 1137 KB libres (598 seguidos en la cima) en los tres modos y
vuelve a lo mismo carrera tras carrera. No hay nada repetido en la RAM de la carrera (la meta comparte las texturas
de su etapa, el trafico suelta sus texeles al subirlos, el coche y la pareja estan una vez).

**GD en carrera**: la etapa siguiente (SEGS.PAK recompuesto, 2,8-8,2 MB) y la union (0,4-1,5 MB) mientras se conduce,
BKD.DAT (105 KB), META.DAT (113 KB), objetos de la union (BOnn.DAT) y la musica en flujo; nada mas.

**Repetido o desperdiciado**:
- texturas en 16 bits que la VQ deja igual a la vista (arriba): 16 MB en el GD y hasta 354 KB por etapa en la VRAM;
- el atlas de los avisos del HUD (512x256 y 256x128 en 16 bits, 288 KB) en la VRAM toda la carrera;
- el marcador de pruebas (144 KB de VRAM y CPU cada 6 cuadros sin verse);
- cs_BK_PALM (22 texturas, 130 KB) va en cada union, pero nunca hay dos uniones a la vez: no se repite en la VRAM
  (en el GD lo quita UNIONES.PAK);
- los juegos de reserva del OPB (4 x 77 KB x 2): solo se ha medido un cuadro en la consola; no se tocan.

### 2. Presupuesto de 45 cuadros por segundo (decision del port)

22,2 ms por cuadro (3 cuadros cada 4 campos a 60 Hz, 9 cada 10 a 50 Hz). La logica sigue el reloj del original
(59,94 pasos por segundo, `PERIODO_60`): a 45 cuadros por segundo da 1, 1 y 2 pasos por cuadro. CPU de dibujo +
logica por debajo de 22,2 con margen para las interrupciones y el hilo de carga: `CPU_MS_CORTA` / `CPU_MS_ALARGA`.

### 3. Pasos

1. VQ (decision del port por clase de textura): segunda pasada desde los 16 bits guardados con el umbral de las
   metas (24) en etapas, uniones, trafico, personajes, banderero y objetos de las uniones; los avisos del HUD en VQ;
   las cifras, tacometros y paneles del HUD (paletas exactas), los menus (textos) y los coches del jugador (insignias
   de cerca) se quedan. Las de menos de 32x32 tambien (el libro pesa mas que lo que ahorran).
2. Distancia: la VRAM ganada al bufer de parametros (`TA_VB_BYTES`; cada 64 KB cuestan 128) para que el reparto del
   original (`nodos_visibles_todos`, LOD, PVS) llegue mas lejos, con el peor caso de cada modo dentro de la zona.
3. 45 fijos: presentacion por campos (sin esperas por tiempo) y `CPU_MS_CORTA` / `CPU_MS_ALARGA` para 22,2 ms.

### 4. Hecho (2026-11-12) y lo que queda

| Paso | Resultado (Flycast / `tools/vram.py`) |
|---|---|
| VQ (8283e33) | 0 texturas de 16 bits de 32x32 o mas en etapas, uniones, trafico, parejas, banderero y objetos (antes 13360 + 2220 + 496 KB en el GD); peor combinacion de la zona: modo OutRun 1127 -> 1013 KB, OUTRUN2SP 1286 -> 941; HUD de carrera 522 -> 278 KB; cuadro congelado de SNOW: el 31 % de los pixeles cambia con diferencia media 3,2 de 765, igual a la vista |
| Distancia (5b57a04) | `TA_VB_BYTES` 1792 -> 1920 KB: tope de la etapa 712-718 -> 846 KB; IMPE entera hasta 430 -> 608 m y recortada desde 484 -> 770 m (GHOS 362 -> 512, 400 -> 532); peor caso de VRAM BK09 + IMPE 185 -> 266 KB libres (INFARTO 170) |
| 45 fijos | presentacion por campos (`FPS_OBJETIVO`), 44,8 cuadros por segundo a 60 Hz y 45,0 a 50 Hz, 0 tarde; logica a 59,94 pasos por segundo como el original; `CPU_MS_CORTA` / `ALARGA` 21 / 17 |
| Carreras seguidas | 7 por el menu real: zona y RAM iguales, 0 "Out of memory", 0 "pvr_mem: ERROR", 0 rebajadas |

Queda (medido, no hecho):
- el marcador de pruebas (144 KB de VRAM, solo con Y o sin HUD): con 4 bits de paleta serian 36 KB, pero la tabla de
  paletas la reparte entera comun/hud.c; con esos 108 KB el bufer podria crecer otros 64 KB (de 846 a 910 KB de
  etapa) con el mismo margen;
- los coches del jugador (32-40 KB de 16 bits, insignias) y las cifras / tacometros del HUD: se quedan por calidad;
- los juegos de reserva del OPB (614 KB): solo con medidas de la consola real (`TA_OPB_POS`, `ta_freno`);
- medir en la consola: `DIAG_MS` (cuadros tarde, ms de CPU) y si el recorte por CPU acorta `lejos` a 45.
