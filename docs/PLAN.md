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
   mapa de la ruta) HECHOS 2026-11-11; la entrada de nombre (26) y los rankings de OUTRUN2SP (VMU) HECHOS 2026-11-11; el final (24) HECHO en parte 2026-11-13 (el
   sitio, la camara del guion, el coche y los creditos; sin la pareja del final); los 15
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

## Popping (2026-11-13)

Pedido del usuario: quitar en lo posible el popping de edificios, estructuras y objetos del escenario sin cambiar el
aspecto ni las proporciones, sin perder rendimiento y sin pasarse de la memoria. Este apartado es el sistema en
tiempo real (recorte, LOD, reparto del TA, cargas); lo que no se ve nunca (PVS mas fina) y los edificios palidos de la
salida de PALM los lleva el otro trabajo (aa695bb, de03c08).

### 1. Medida: `-DDIAG_POPPING`

Por nodo de la etapa, la union y la etapa siguiente (`nodos_visibles`, `nodos_quita`) la razon por la que no se dibuja
en el cuadro: `lejos` (plano lejano), `detras` (plano cercano), `lado` (laterales de la vista), `lod` (LOD de 404B00),
`rama` (registro de la union, 44E290), `pvs` (ninguna malla visible desde la seccion), `tope` (reparto del TA),
`tramo` (modelo sin subir o sin dibujar, 44C530). Un nodo que aparece o desaparece con el centro dentro de la
pantalla es un salto que se ve; se cuenta por la razon de antes (o la de ahora si desaparece) y por el radio de su
esfera en la pantalla (2, 8 y 32 px de 640x480). Ademas: mallas que la PVS enciende o apaga en un nodo que ya se
dibujaba, mallas que el tope salta al dibujar (orden de las listas) y "falta": la suma de pi px^2 de lo que el tope
quita en la pantalla (con solapes) / la pantalla. Linea `POP:` cada ~1 s y los 4 saltos mayores (`POPEV:`). Las razones
van por fichero (datos), no por hueco: el paso de la etapa siguiente a etapa no cuenta. Solo medida (4 x 8 KB).

Prueba: `-DPRUEBA_AUTO -DPRUEBA_SIN_TIEMPO -DDIAG_POPPING -DDIAG_VTX -DDIAG_MS` (el piloto desde la salida de PALM
por LAKE), 240 s; resumen por razon de s = 60 a 10000 m (sin la escena de la salida, un corte de
camara) normalizado por 45 cuadros (Flycast puede ir mas lento que el reloj con otras pruebas en marcha). La prueba es
determinista: dos pasadas del mismo ELF dan las mismas cifras.

### 2. Diagnostico (antes, 5ae7d9c con `-DREPARTO_CERCA=0.75f -DREPARTO_HIST_D=0 -DREPARTO_HIST_P=0 -DSIN_REPARTO_GRANDES -DREPARTO_COSTE=0`, lo de 3da8c7e)

| Razon | aparecen por s (>= 2 / 8 / 32 px) | desaparecen por s (>= 2 / 8 / 32 px) | distancia minima |
|---|---|---|---|
| tope del TA | 25,3 / 19,8 / 6,0 | 17,3 / 12,8 / 4,0 | 79-95 m (salida), 190-430 m (resto) |
| LOD de 404B00 (original) | 7,0 / 0,02 / 0 | 0,2 / 0 / 0 | 44-62 m (todo de ~7,5 px: el umbral) |
| PVS (nodo entero) | 3,3 / 2,5 / 1,4 | 1,9 / 1,6 / 0,9 | 31-80 m |
| lateral | 0,2 / 0,06 / 0,01 | 0 | > 700 m (giros) |
| registro de la union | 0,07 / 0,05 / 0,05 | 0 | |
| tramo sin subir / carga | 0 | 0 | |
| mallas de la PVS en nodos ya dibujados | 70 / 31 / 5,0 | 39 / 16 / 3,3 | |

Pantalla sin dibujar por el tope: 43 % de media (s 60-10000). Mallas que el tope salta al dibujar: 1197 en 12 cuadros.

- **El tope del TA es casi todo el popping**: lo que el original dibuja (16000-70000 vertices de la etapa despues de
  la PVS, linea `orig` de `DIAG_VTX`) no cabe en lo que deja el bufer de parametros (~16000 con el coche y el trafico,
  846 KB de etapa). Con `REPARTO_CERCA` 0,75 lo cercano por distancia se llevaba casi todo el tope y de lo de mas alla
  solo quedaban las clases de LOD bajas (lo enorme); el corte de distancia iba de 76 a 430 m (mediana 430) y
  edificios de 40-300 px de radio entraban y salian ahi. Ademas, si ni la clase de LOD 0 cabia, `prio_p` quedaba en
  -1 y se iba todo lo de mas alla, el terreno de r 980 m incluido (POPEV: `sig aparece nodo 227 tope r 982 px 340`).
- El LOD del original no es un problema de la Dreamcast: quita lo que baja de 7,5 px de radio (1/32 de media
  pantalla, 404B00), lo mismo que en el PS2 y el PC.
- El recorte lateral es correcto: la vista es 4:3 de 640x480 con fx de la fov del original (483C10 / 484BD0) y el
  recorte por esfera usa esos mismos limites; lo que entra por `lado` en mitad de la pantalla es lejano y en giros.
- Cargas: ningun salto por tramos sin subir (la etapa siguiente se lee ~1000 m antes y se dibuja con la escala de
  44C530; si la lectura llega tarde, el bucle espera: `carga_espera`). No hace falta una precarga predictiva.
- Profundidad: la PowerVR compara 1/w en coma flotante (no un z-buffer entero de 24 bits como el D3D9 del PC); el plano
  cercano 0,1 del original (PS2 0x178580) no quita precision lejos y nada se pierde por profundidad (sin w recortada:
  `inv_w` es 1/w con FSRRA).
- LOD de mallas: los nodos de las etapas del PS2 solo traen la malla 0 que se dibuje (en STAGE.PS2, 17630 nodos de 7
  segmentos: 80 con la bandera 0x10 de 404B00 y mallas 1-3, todos en el segmento 3 y con las 4 mallas iguales, 960
  vertices cada una): no hay LOD del original que usar en el curso. El trafico ya usa sus 5 LOD (46DF50) y el coche del
  jugador no tiene.
- PVS: 3,3 nodos por segundo (1,4 de 32 px o mas) y 75 mallas aparecen al cambiar de seccion; es del otro trabajo.
- CPU: `lejos` solo bajo a 3650 m en Flycast (`CPU_MS_CORTA` 21 ms); en la consola, si la CPU no llega, se acorta de 50
  en 50 m con el fundido de la niebla (`niebla_pon`): hay que mirarlo alli (`DIAG_MS`).

### 3. OutRun2006Tweaks (emoose, clonado solo para leer en el scratchpad: or2tweaks)

| Ajuste | Que arregla en el PC | Que toca (direccion del exe; decompilacion) | En el port |
|---|---|---|---|
| `DisableStageCulling` | "objetos del escenario lejanos que aparecen de golpe" | 2 NOP en 0x40501F, un salto condicional dentro de 404D10 (el recorrido de nodos, protegido por SecuROM: la decompilacion lo tiene como puente, `walk_404d10` en pc_render_queue.cpp). PROBABLE: la prueba del resultado de 404B00 (esfera contra la vista y LOD por tamano: se descarta si 127 - exponente(r / (z tan(fov/2))) >= umbral 6, 7 con la bandera 0x40) | la prueba de LOD de `nodos_visibles`. El concepto (dibujar mas alla del LOD) va como `REPARTO_LOD_MAS`, dentro del reparto del TA (ultimas clases): 0 por defecto (abajo) |
| `FixZBufferPrecision` | z-fighting y objetos lejanos mal dibujados con el plano cercano 0,1 en un z-buffer entero | gancho de CalcCameraMatrix 0x484BD0 (`camera_project_484bd0`): camara +0xBC (plano cercano) a 1,0 en carrera (0,25 / 0,2 en la vista interior, 0,05 para los efectos de 0x40BE70) | nada: la PowerVR guarda 1/w en coma flotante; el cercano sigue siendo 0,1 (subirlo recortaria lo cercano que el original ve) |
| `DrawDistanceIncrease` | popping por las listas de nodos por seccion | gancho en 0x44DF6D de `race_area_geometry_44de10`: el original dibuja de cada objeto de la etapa solo los nodos de la lista de la seccion `bp` del coche (cs_CS_<zona>, `geo + [geo + bp x 4]`); el ajuste une las listas de bp a bp + N sin repetir nodos (y quita nodos de LOD malos por etapa, OutRun2006Tweaks.lods.ini) | el port no usa esas listas (su formato de PS2 sigue sin descifrar): dibuja la union de todas (`tools/modelo.py` vis) con la vista, el LOD de 404B00 y la PVS. Es un DrawDistanceIncrease sin limite; lo que limita es el TA. Queda por mirar si salen los nodos "malos" de la lista del ajuste (PALM objeto 1 nodos 0xD-0xF desde la bifurcacion izquierda; ALPI objeto 1 nodos 1, 0xD, 0x22) |
| `ScreenEdgeCullFix` | objetos (conos) que desaparecen antes del borde en pantallas que no son 4:3 | gancho de CalcBall3D2D 0x449E70 (`project_449e70` de race_traffic.cpp): reescala la x / y de pantalla a 4:3 | nada: la Dreamcast sale en 4:3 (640x480) y el recorte usa esos limites |

(`DisableVehicleLODs`, 0x4AE4E9, LOD 0 para todo el trafico: no cabe en el TA; el trafico sigue con el LOD de 46DF50.)
No se copia ningun parche: solo las ideas, y el valor del juego (LOD, planos, fov) sigue siendo el de la ISO.

### 4. Cambios (decisiones del port, cada una revertible con su define; `demo/main.c`, `nodos_visibles_todos`)

1. `REPARTO_CERCA` 0,75 -> 0,25: lo cercano por distancia ya no se lleva el tope; lo demas se reparte por la clase de
   LOD (tamano en la pantalla respecto al LOD del original).
2. `REPARTO_COSTE` 4: la clase de LOD sube 1 por cada vez que el nodo dobla los 64 vertices: de lo que no cabe se
   quita antes lo que mas TA gasta por pixel (palmeras, gente, barandillas lejos) y quedan los edificios y las
   siluetas (pocas caras grandes). Probados y peores: 8 (menos saltos pero mas pantalla sin dibujar), sin coste para
   lo de 60 px o mas, y un tope de clase 24 para lo grande y caro.
3. `SIN_REPARTO_GRANDES` (por defecto activo): la clase de LOD 0 (240 px o mas: terreno, mar) antes que lo cercano;
   si no cabe, se acorta lo cercano (antes `prio_p` = -1 y se iba el terreno).
4. `REPARTO_HIST_D` / `_P` 4 / 8: histeresis por nodo; lo que se dibujo en el cuadro anterior cuenta como 2 veces mas
   cerca / mas grande (mascara `vis_ant`, 400 bytes por modelo junto a `vis`): entra al cruzar el corte y solo sale si
   queda 2 veces mas alla. Con 2 / 4 (1,41 veces) 5,8 desapariciones de 8 px o mas por segundo; con 4 / 8, 4,5.
5. `REPARTO_LOD_MAS` 0 (sin cambio): con 2, los nodos hasta 4 veces su distancia de LOD entran en las ultimas clases
   (solo si sobra TA). Los saltos del LOD (5,7 por segundo, de menos de 8 px) pasan a saltos del tope de 2-8 px (+4,1),
   los de 8 px o mas igual y +0,3-0,6 ms de CPU en Flycast: no compensa hasta medir la CPU en la consola.

Lo que no cambia: la geometria y las texturas, el orden de dibujo, la PVS, la niebla y el velo del limite (`nlim_paso`
sigue a `prio_d`), el tope del TA y sus margenes (`ta_f`), la RAM (+400 bytes por modelo del monton al leerlo) y la
VRAM. Sin fundidos ni transparencias.

### 5. Antes / despues (Flycast; los tiempos de Flycast no son los de la consola: falta confirmarlo en la PAL real)

Mismo ELF de prueba con los defines de antes (`-DREPARTO_CERCA=0.75f -DREPARTO_HIST_D=0 -DREPARTO_HIST_P=0
-DSIN_REPARTO_GRANDES -DREPARTO_COSTE=0`) y con los de ahora; tramo de s que cubren las dos pasadas, por 45 cuadros.
Saltos del tope del TA (la razon que cambia); "falta" = pantalla sin dibujar por el tope; CPU = `MS` dib (ms por
cuadro de dibujo, Flycast).

| Prueba | aparecen >= 8 / >= 32 px por s | desaparecen >= 8 / >= 32 px por s | falta | CPU dib (ms) | cuadros por s | zona libre min (KB) |
|---|---|---|---|---|---|---|
| PALM -> LAKE (s 60-10937) | 19,8 / 5,8 -> **13,4 / 2,0** | 12,8 / 3,8 -> **4,5 / 0,6** | 40,8 -> **15,4 %** | 16,10 -> 16,40 | 44,84 / 44,84 | 669 / 669 |
| GHOS -> IMPE, COCHE06 (PRUEBA_NODO=9 RAMA=0 FIN=400) | 14,0 / 4,1 -> **10,1 / 2,2** | 9,1 / 2,7 -> **6,9 / 1,9** | 24,9 -> **20,1 %** | 16,00 -> 16,42 | 43,99 / 43,76 | 266 / 266 (BK09 + IMPE) |
| OUTRUN2SP (arbol 1: BEAC, BK15, SEG25, BK16, SEG20) | 14,9 / 6,1 -> **9,1 / 2,0** | 9,9 / 3,7 -> **4,4 / 0,8** | 30,3 -> **17,3 %** | 16,27 -> 16,39 | 44,57 / 44,64 | 502 / 502 |
| INFARTO, GHOS -> IMPE (PRUEBA_VARIANTE=2) | 15,1 / 4,9 -> **12,0 / 2,5** | 12,8 / 4,3 -> **7,1 / 1,7** | 32,6 -> **21,2 %** | 16,44 -> 16,71 | 43,99 / 43,96 | 170 / 170 |

- Vertices por cuadro iguales (28257 / 28176 en PALM; el tope del TA no cambia) y las mismas mallas saltadas por el
  tope al dibujar (10-22 cuadros de cada pasada, como antes). 0 "Out of memory" y 0 "pvr_mem: ERROR" en las ocho
  pasadas; RAM: +400 bytes por modelo leido (el minimo del monton cambia con el momento de las cargas: INFARTO 464 /
  431 KB, los demas iguales); VRAM igual.
- LOD del original: igual (4-6 por segundo, todo de menos de 8 px).
- PVS: los saltos de nodos de 8 px o mas suben (PALM 2,5 -> 4,2, OUTRUN2SP 5,1 -> 7,4): ahora se dibuja mas de lo
  de lejos y se ven mas los cambios de seccion de la PVS (otro trabajo).
- CPU: +0,1-0,4 ms por cuadro en Flycast (la mascara del cuadro anterior y el coste en la clase); `lejos` igual en las
  dos (1850-3650 m como minimo en Flycast con las cargas).
- Imagen (`-DDIAG_CONGELA`, el mismo paso en las dos; capturas en el scratchpad, pop/congelado_antes_despues.png): en
  la salida de PALM (crono 150) antes faltaban los edificios del fondo de la calle a la derecha (la calle acababa en el
  cielo) y ahora estan; a cambio, lo caro y pequeno de lejos (la gente de la grada de la izquierda, un arbol) se quita
  antes. En el crono 1500 (playa) las dos iguales: alli cabe todo.

### 6. Lo que queda

- El TA: lo que el original dibuja es 1-4 veces lo que cabe. Con el reparto nuevo lo que salta es mas pequeno y no
  parpadea, pero sigue habiendo ~13 apariciones por segundo de 8 px o mas en PALM. Mas sitio solo con VRAM (cada 64 KB
  de bufer son 128 KB) o midiendo en la consola lo que gasta de verdad el TA (`ta_f` 1,75 sobre lo teorico es el margen
  de la medida de la demo 12c; `ta_fin_cuadro` ya lo sube solo si la consola gasta mas, nunca lo baja).
- PVS: los saltos de nodos y mallas al cambiar de seccion (otro trabajo).
- Medir en la consola real: `DIAG_POPPING` (necesita la consola serie) o al menos `DIAG_MS` y `lejos`.
- Los nodos "malos" de OutRun2006Tweaks.lods.ini (LOD de otras secciones que el port, sin las listas por seccion,
  puede dibujar a la vez que el modelo entero): mirarlo en PALM (bifurcacion izquierda) y ALPI.

### 7. Las palmeras (revision 45, 2026-11-14)

Queja del usuario (Dreamcast real, revision 44): "sigue habiendo popping en las palmeras". Medida nueva en
`DIAG_POPPING`: la vegetacion de recorte (nodos con mas de la mitad de sus vertices en la lista de recorte, la 1: hojas
con el alfa de recorte de su textura) va aparte, linea `POPVEG:` (como `POP:`, mas la distancia y el radio maximo de
las apariciones de 8 px o mas) y `veg` en `POPEV:`. Prueba: `-DPRUEBA_AUTO -DPRUEBA_SIN_TIEMPO -DDIAG_POPPING -DDIAG_VTX
-DDIAG_MS` 240 s (PALM -> ALPI -> DESE; con `-DPRUEBA_JUEGO=3` BEAC -> SANF), por 45 cuadros, s >= 60.

Lo que hace el original (404B00, sin cambiar): las palmeras de PALM (174 nodos con la textura de hojas 113, ARGB1555
con su recorte) llevan casi todas la bandera 0x40 (umbral 7: 170 de 174), asi que el LOD las quita a 3,75 px de radio
(240 / 2^6), no a 7,5. Con la focal de la camara en carrera (fx ~380, de px = r fx / (z + r) en POPEV) son
z = r fx / 3,75 (umbral 6: r fx / 7,5): **PALM (SEG00), vegetacion de 64 vertices o mas: mediana 1126 m (p10 457, p90
3773)**; en las etapas con muchas palmeras / arboles de recorte (SEG01, 13, 17, 18, 22, 32, 34, 42, 46, 50, 54, 64:
150-320 nodos, 40000-60000 vertices) la mediana va de 400 a 1700 m (los arboles sin la bandera 0x40, a 7,5 px: 400-930
m). Siempre bastante mas alla de donde el port las hace aparecer. El LOD del original no hace saltar palmeras de 8 px o mas (0,03-0,07 por segundo, nodos de
umbral 6 justo en sus 7,5 px a 58-62 m).

Por que aparecen (apariciones por segundo de vegetacion de recorte, >= 8 / >= 32 px; entre parentesis todo):

| Prueba | reparto del TA (tope) | PVS | LOD 404B00 | lateral | fps / dib (ms) |
|---|---|---|---|---|---|
| PALM, TA 448 | **6,52 / 0,64** (17,6 / 2,4) | 1,82 / 0,68 | 0,03 / 0 | 0,06 / 0 | 43,3 / 18,1 |
| PALM, TA_F_MIN=576 | **11,87 / 5,01** (29,3 / 14,4) | 0,82 / 0,47 | 0,04 / 0 | 0,06 / 0 | 43,9 / 15,8 |
| BEAC, TA 448 | **3,27 / 0,44** (9,8 / 1,5) | 1,34 / 0,44 | 0 / 0 | 0,34 / 0,13 | 44,0 / 17,4 |
| BEAC, TA_F_MIN=576 | **5,39 / 2,99** (14,8 / 7,7) | 0,83 / 0,39 | 0 / 0 | 0,10 / 0,06 | 44,3 / 15,4 |

- **Es el reparto del TA del port**, no el original: aparecen por el tope a 12-150 m con 30-150 px de radio (el
  original las dibuja hasta 3,75 px). Grupos de nodos: 65 de las 174 palmeras van agrupadas, todas con un nodo de 4-12
  vertices (su sombra / base): la clase del grupo es la de la palmera, no cambia nada. La niebla / el velo: nada (lo
  tapado por la niebla es la ultima clase y no cuenta como salto en la pantalla). La PVS: 0,8-1,8 por segundo (otro
  trabajo).
- Por que justo las palmeras: son lo mas caro por pixel de la etapa. Una palmera de PALM: 251 vertices de mediana
  (hasta 326), 27 tiras (5 del tronco, opacas, textura 109; 21-22 de hojas de 3-16 vertices en el recorte), **7,3 KB
  teoricos del TA** (x 1,75 = 12,8 KB contados). La vegetacion de 64 vertices o mas de PALM suma 1453 KB teoricos: mas
  que todo el tope de la etapa (848 KB con 448, 579 con 576). El coche del jugador y la pareja se llevan 608 + 149 KB
  con 448 (782 + 192 con 576): el 45 % del bufer.
- Unir las hojas en una tira (idea del usuario: "que esten unidas y el corte solo dibujado"): **no gana, pierde**. Las
  hojas ya son tiras con el alfa de recorte de su textura (ARGB1555); no hay geometria que solo haga de silueta. El TA
  guarda cada trozo de 6 triangulos con su ISP/TSP/TCW, asi que unir con triangulos degenerados no quita trozos y anade
  2-3 vertices por union: una palmera pasa de 7308 a 11940 bytes teoricos (+63 %; todas las de PALM 1310 -> 1993 KB).
  Las tiras que siguen exactamente a la anterior (mismos 2 vertices, partidas por ADC) son 15 en todo el recorte de PALM
  (1858 KB -> 1857). Las cabeceras ya van una vez por textura y cuadro (mallas ordenadas por lista y textura). Lo mismo
  que DREAMCAST.md (El tope, tiras mas largas).
- Probado en el reparto (Flycast, misma prueba; vegetacion >= 8 / >= 32 px por segundo por el tope, todo >= 8 / >= 32,
  pantalla sin dibujar mediana):

| Cambio (PALM, TA 448 salvo lo indicado) | vegetacion | todo | falta | fps |
|---|---|---|---|---|
| ninguno (lo de ahora) | 6,52 / 0,64 | 17,61 / 2,37 | 5,1 % | 43,30 |
| sin coste para los vertices de recorte (REPARTO_COSTE solo con lo opaco) | 7,50 / 1,55 | 20,56 / 4,51 | 11,3 % | 43,58 |
| recorte a la mitad en el coste | 5,69 / 0,59 | 18,18 / 3,04 | 5,7 % | 43,36 |
| histeresis 12 / 6 (antes 8 / 4) | 6,94 / 0,69 | 18,34 / 2,80 | 4,8 % | 43,53 |
| histeresis 12 / 6 y recorte a la mitad | 6,07 / 0,62 | 19,02 / 3,37 | 6,8 % | 43,46 |
| REPARTO_CERCA 0,40 (antes 0,25) | 6,58 / 0,72 | 18,66 / 2,73 | 6,5 % | 43,52 |
| 576: ninguno / sin coste de recorte / a la mitad | 11,87 / 5,01; 8,34 / 5,29; 11,33 / 4,79 | 29,3 / 14,4; 25,0 / 15,6; 28,4 / 14,8 | | 43,9 |
| BEAC: ninguno / recorte a la mitad | 3,27 / 0,44; 3,31 / 0,37 | 9,75 / 1,50; 10,88 / 1,81 | 1,2 / 1,4 % | 44,0 |

  Ninguno sirve: lo que las palmeras ganan lo pierde el resto (mas apariciones de 32 px o mas y mas pantalla sin
  dibujar), porque lo que no cabe no cabe; sin el coste del recorte salen peor hasta las palmeras (cada palmera que
  entra echa a otras). El reparto se queda como estaba (sin cambio de codigo).
- Capturas congeladas (`-DDIAG_CONGELA`, el mismo paso; scratchpad capturas_r45/palmeras_*): en el crono 1200 (la calle
  de la salida) 448 sale igual que sin tope (4 pixeles distintos) y 576 casi (473); en el crono 2400 (s 3605, el paseo
  de la playa) **con 576 faltan las 6-8 palmeras del borde de la carretera de la derecha** que estan sin tope y con 448
  (palmeras_c2400_zoom_sintope_448_576.png): es lo que ve el usuario si su consola gasta ~2,25 veces lo teorico.

Lo que arreglaria las palmeras de verdad (decision del usuario):
1. Medir el TA de su consola: la linea `TA <real>K/<teorico>K x.. max.. f..` de la pantalla de medidas (pausa, Y) tras
   una vuelta por PALM. Si sale x1,3-1,4 (lo de la demo 12c), `ta_f` se queda en 1,75 y en PALM aparecen ~6,5 por
   segundo de 8 px (casi todas de menos de 32); si sale ~2, son las 12 por segundo de 576 y las de 30-150 px.
2. Sitio en el TA: el coche del jugador (608 KB, sin LOD en el original) es lo que mas gasta; mas bufer solo con VRAM
   (cada 64 KB de bufer son 128 KB de VRAM por ir doble).
3. Dibujar mas alla del recorte del original no es posible sin cambiar el aspecto: no hay LOD de palmeras en los
   datos del PS2 ni texturas de siluetas que usar.

### 8. Palmeras lejanas en version de bajo coste (revision 46, DESVIACION pedida, 2026-11-14)

Peticion del usuario: "No quiero recortes; en tal caso, las palmeras mas lejanas que se vean pero en version low-poly,
y cuando te acerques que se vean bien". En lugar de que el reparto del TA quite la vegetacion de lejos, cada nodo de
vegetacion caro tiene una version lejana (tools/lod_veg.py) que la demo dibuja cuando se ve pequeno (demo/main.c
nodos_visibles, LOD_LEJ_PX / LOD_LEJ_HIST). docs/PARIDAD.md, Revision 46: palmeras lejanas.

Por que un impostor y no una malla simplificada (medido sobre PALM): una palmera son 27 tiras (5 del tronco, 21-22 hojas
de 3-16 vertices); el TA guarda cada trozo de 6 triangulos con su cabecera, asi que lo minimo por hoja es un
cuadrilatero (108 bytes teoricos): reducir cada hoja a 4 vertices y el tronco a una tira deja ~2,6 KB de 7,3 (2,8 veces
menos, lejos de 4-5) sin quitar hojas (cambiaria la silueta). Un impostor de un cuadrilatero son 108 bytes: 68 veces
menos. Dos o tres planos cruzados fijos: 2-3 veces eso y se ven de canto; el cuadrilatero girado hacia la camara
alrededor de la vertical (el mecanismo de los carteles del original, carga_cartel) se ve siempre de frente.

Seleccion (lod_veg.candidatos y planta, medido en las 62 etapas: 1928 modelos de recorte distintos, mosaicos en el
scratchpad lod46/): POPVEG (> 1/2 de los vertices en la lista de recorte), >= 1500 bytes teoricos, no carteles, y con la
forma de algo que se ve parecido de cualquier lado: desviacion menor / mayor en planta >= 0,35, alto >= 1/2 del lado en
planta, 2-55 % de texeles opacos en su imagen, >= 12 tiras y no azulado. Quedan palmeras, arboles (verdes, amarillos de
otono y en flor), arbustos y pinos, y algunas farolas, la cabina del teleferico y ruinas pequenas; fuera barandillas,
vallas, setos, macizos de flores, fachadas con ventanas, gente y carteles. 3948 nodos en 51 etapas; 4 uniones de 498.

Imagen: vista ortografica desde la direccion de llegada de la camara (la ruta del fichero hacia atras desde el punto
mas cercano), 62 texeles el lado mayor, 4 x 4 muestras por texel, cobertura >= 0,5 (el recorte de la PowerVR), color de
lo transparente rellenado con el de los vecinos; los nodos del mismo modelo (ajuste afin de sus vertices al primero)
comparten imagen redondeando su rumbo de llegada a 8 (PALM: 179 nodos, 11 modelos, 47 imagenes). Atlas VQ ARGB1555
rectangulares (el ancho que menos ocupa con su libro): PALM 256x512 + 256x64, 40 KB; la que mas, SEG50, 44 KB.

En la demo: radio en la pantalla < LOD_LEJ_PX (48) -> el impostor; con histeresis (el lejano sigue hasta 60 px, el
completo hasta 38). Probado antes con 24 px: las palmeras de 24-48 px (50-110 m) seguian completas y el tope las
quitaba (vegetacion de 8 px o mas por el tope 6,28 -> 5,40 por s). El impostor va en la clase de LOD 0 del reparto
(antes, con la clase del completo menos el coste, el tope lo quitaba alguna vez); sin grupo.

| PALM -> ALPI, por s (>= 2 / 8 / 32 px) | 448 antes | 448 ahora | 576 antes | 576 ahora |
|---|---|---|---|---|
| vegetacion con version lejana, aparece por el tope | 2,76 / 2,58 / 0,24 | **0,01 / 0,01 / 0,01** | 2,44 / 2,44 / 2,14 | **1,37 / 1,37 / 1,37** |
| toda la vegetacion, aparece por el tope | 8,34 / 6,28 / 0,66 | 5,23 / 4,00 / 0,49 | 12,33 / 11,59 / 5,05 | 11,39 / 10,30 / 4,31 |
| todo, aparece por el tope | 22,79 / 17,15 / 2,44 | 19,11 / 14,29 / 2,25 | 30,48 / 29,07 / 14,55 | 31,01 / 29,00 / 13,82 |
| pantalla sin dibujar por el tope (mediana) | 6,3 % | 1,9 % | 117 % | 104 % |
| fps / dib (ms) | 43,31 / 18,13 | 43,22 / 18,21 | 43,77 / 15,80 | 43,79 / 15,98 |

| BEAC -> SANF (PRUEBA_JUEGO=3), BEAC | 448 antes | 448 ahora | 576 antes | 576 ahora |
|---|---|---|---|---|
| vegetacion con version lejana, por el tope | 2,05 / 2,00 / 0,39 | **0,10 / 0,10 / 0,10** | 2,55 / 2,54 / 2,15 | **1,04 / 1,04 / 1,04** |
| toda la vegetacion, por el tope | 3,66 / 3,25 / 0,72 | 1,40 / 1,16 / 0,30 | 6,63 / 5,99 / 3,06 | 4,45 / 3,94 / 1,96 |
| todo, por el tope | 10,34 / 8,64 / 1,67 | 6,52 / 5,16 / 0,86 | 16,29 / 15,18 / 7,70 | 14,71 / 13,52 / 6,38 |
| fps / dib (ms) | 43,81 / 17,42 | 43,99 / 16,96 | 44,17 / 15,43 | 44,18 / 15,49 |

(antes = los mismos datos con -DLOD_LEJ_PX=0: el reparto y los costes de antes; ~20-22 impostores por cuadro en PALM y
BEAC; 2,0-2,5 pasos de lejano a completo por s al acercarse, ~0 al reves; 0 "Out of memory"; RAM minima del monton
677 -> 669 KB; la zona del curso en PALM 785 -> 745 KB libre, la minima de la prueba igual, 546 KB.) Lo que queda con
576 son palmeras completas de 25-50 m y 60-130 px que no caben en la salida de PALM (el reparto acorta lo cercano;
POPEV), y de toda la vegetacion la que no tiene version lejana (setos, macizos, lo barato).

Capturas (scratchpad capturas_r45/): palmeras_lod_c2400_zoom.png (crono 2400, la playa de PALM: 576 antes con 1
palmera en el borde derecho, 576 ahora con las 8, todo completo sin tope y todo lejano sin tope: iguales a esa
distancia, 1566 pixeles distintos de 1024x768), palmeras_lod_c2400_4.png (las cuatro enteras). El paso de cerca:
palmeras_lod_c1200_lado.png / _zoom.png (crono 1200, la calle de la salida, sin tope: todo completo frente a todo
lejano; las palmeras de la acera a ~30-50 px salen iguales de tamano, color y silueta, 1040 pixeles distintos, solo la
colocacion de algunas hojas de la copa porque la imagen es la de la direccion de llegada redondeada).
