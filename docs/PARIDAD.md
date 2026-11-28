# Paridad con OutRun 2006 Coast 2 Coast de PS2

## Referencia

| Dato | Valor | Como se comprobo |
|---|---|---|
| Edicion | PAL Europa, SLES-53998 (cadena `BESLES-53998` en el ELF; el ELF lleva tambien la clave de SLUS-21274) | SYSTEM.CNF y cadenas del ELF |
| Revision | `VER = 1.02`, `VMODE = PAL` | SYSTEM.CNF del disco |
| Volumen ISO | `OUTRUNC2C`, editor `SEGA EUROPE LTD`, creado 2006-02-19 19:10:25 | descriptor primario (sector 16) |
| MD5 de la imagen usada | `e41576287d3547b3cbb0c01211852a33` (1 809 907 712 bytes) | md5sum; contrastar con redump |
| MD5 de SLES_539.98 | `2551f798ff979ca3b1e3be328e21595c` | md5sum |
| PC (evidencia, no referencia) | decompilacion de r4dius (`outrun2-decomp`, ultimo commit e1e1c59, 2026-10-07); direcciones `0x4xxxxx`/`0x5xxxxx` son del ejecutable de PC | |

Regla: el PC sirve para encontrar logica y nombres; el dato bueno es el de SLES-53998. Toda tabla que use la
demo se extrae del ELF de PS2 en la conversion (tools/conv_*.py) y se localiza por codigo o estructura.

Tick de la logica del original: `dt = 0.016611295 s` (1/60,2), presente 1 vez en el ELF de PS2 (bytes
`6914883c`) y igual que `fixed_dt` del PC (pc_chassis.cpp, 0x7162C4); 60.2 aparece 22 veces. CONFIRMADO que la
fisica usa ese paso. Cadencia real (CONFIRMADO en SLES-53998): el bucle principal 0x1078c8 da en cada cuadro tantos
pasos (0x107958..0x1079a8, 7 llamadas como 417C7B del PC) como campos de video han pasado (0x107b50 en cada vblank,
cuenta 0x448b28; mas de 600 -> 1; 0 -> 0). Modos del disco (TEXT.PS2: "50Hz", "60Hz - Test", "60Hz Progressive (480P)",
0x1070f8 / 0x166638): a 60 Hz y 480p, 1 paso por campo (59,94 Hz); a 50 Hz ([0x328a8c] == 1) 1 paso extra cada 5
campos salvo uno de cada 1000 (cuenta s4): 50 x 6/5 x 1000/1001 = 59,94 pasos por segundo, con mezcla de dibujo
(s3 mod 5) x 0,2 en 0x36f334. O sea: 60000/1001 pasos por segundo real en todos los modos, sin tope de pasos por
cuadro (si los fps bajan el juego no se ralentiza). El PC (417890 / 417C7B) da 60 por segundo con su reloj. La demo:
PERIODO_60 (1001000 us/60 = 16683,33 us) con el reloj de KOS, hasta 600 pasos por cuadro (antes 3: por debajo de 20
fps la demo se ralentizaba y el original no).

## Como compilar y probar

- `cd demo && make` (KallistiOS en /opt/toolchains/dc/kos). Defines de prueba: `PRUEBA_JUGADOR`, `PRUEBA_AUTO`,
  `PRUEBA_TRAFICO`, `PRUEBA_CHOQUE`, `PRUEBA_RAMA=0/1` (rama que toma el piloto), `PRUEBA_FICHA=k` (sale con el coche de la ficha k, no con el que deja elegido el menu), `PRUEBA_NODO=k` (etapa de salida, clave del arbol), `PRUEBA_V`, `PRUEBA_FISICA` (resumen de la fisica cada segundo), `PRUEBA_VMU` (guarda la licencia al arrancar y la vuelve a leer), `SALIDA`, `VER=1`, `DIAG_VTX`.
- Datos: conversores de tools/ (ver ASSETS.md) a un dir de datos; `tools/mkgdi.py demo/demo.elf dir --datos datos`.
- Niveles de verificacion usados abajo: **C** compila, **H** verificado en host (tools/c/prueba_fisica.c),
  **E** verificado en emulador (Flycast), **R** verificado en Dreamcast real (GDEMU del probador).

## Inventario

Estado: OK (conectado y verificado al nivel indicado), PARCIAL, PROVISIONAL (aproximacion delimitada), FALTA.

| Sistema | Referencia PS2 / PC | Estado DC | Evidencia | Diferencia | Proxima accion | Verificacion |
|---|---|---|---|---|---|---|
| Aceleracion, motor, cambio auto/manual | tabla de parametros 0x2dbc30 (19 col.), par 162 f x 2 (automatico y manual, PAR2), freno 256 f; cambio PS2 0x18d530 | OK | or2_fisica.c clonado; columna por modelo MAP1 | ninguna conocida (`ACEL` = 1 por defecto; `-DACEL=1.35f` da la aceleracion de mas que se pidio antes, no es del juego) | — | H, E |
| Direccion, derrape (inicio/mantenimiento/salida) | SlipAngleCtrl / RearGripCtrl / CorneringCtrl; temporizador 40 cuadros | OK | decompilacion + banco | ninguna conocida | trazas de entrada comparables con PS2 | H, E |
| Adherencia por superficie | get_road_mu 500B30, running_resistance 502120, COLI0200 | OK | coli por rueda; medido en la arena de PALM (2026-11-13, ver Arena, grava y polvo) | ninguna conocida | — | H, E |
| Choques coche-coche y muro, choque fuerte, grua | 4F9450/4F9000/4F8490/4F6FE0/4F76E0/4F7BF0/476040/473F30 (ver Choques entre coches), CarBodyWallColiCheck 504CE0 y cbw_coli_wall (la forma de la carroceria contra la COLI: ver Muros y suelo de la carroceria del original), 5038D0, PlWrecker | PARCIAL | CHQ1/POS1 de PARAM.DAT, CJM1/SEC1 de TRAFC.DAT, efectos de IGC.PAK en SFX.DAT | coche-coche: ver Choques entre coches (PROVISIONALES alli; golpeados: parpadeo y cierre del original); vibracion del mando de PS2 (0x1d0420) completa, superficie incluida (2026-11-13; verificado contra el original 2026-11-19, PENDIENTE el termino de +D34 y las banderas +8 0x40/0x100); grua recoloca en el centro de la fila de poligonos de la carretera (CORREGIDO 2026-11-11: la ruta es ese centro, ver Muros y suelo de la carroceria del original; antes el carril del asfalto mas cercano); avance 10 posiciones x m_pos (CONFIRMADO, antes 25 m supuestos); angulo del choque fuerte por nivel de etapa (CORREGIDO 2026-11-11: 45 grados solo en la primera, 25 / 18 en las demas, PS2 0x1994c4: antes siempre 45 y el vuelco casi no salia); tramos de atrapamiento y de desvio 1 -> 5 por etapa HECHOS (2026-11-11, ver Choque fuerte contra el muro); sin el SE 0x43 de los estados 1 y 5 ni el SE por material (pc_collision_material_sound), sin el handicap +DB4; chispas del muro y de los coches hechas (ver Sombras y marcas) | — | H, E |
| Camara de carrera | evento CAMERA 385: 4858D0, 484810, 483110, 4A1680 SetCarCamera, 485590/483CE0/4834A0/483600, 4832F0/484A40, 483C10/484BD0 (PS2 0x178500..0x17b540, 0x1817c0) | PARCIAL | demo/camara.c con las tablas CAM1 de PARAM.DAT (vistas 0x2ccd88, por vista y modelo 0x2cd3d8, por modelo 0x2d2838 del fichero SLES) | vista 1 con el habitaculo desde la Rev 2.9.1 (sin el desvanecido +EA0 de la 1 a la 0); camara de salida (485250) con el ojo y la mira de la escena de la salida los 120 cuadros de START (ver Escena de la salida); sin vista 0x1D (780258 = 0) ni camara de fin (485830); la de la meta (4853D0) en demo/meta.c (ver Escena de la meta) | ver seccion Camara de carrera | E |
| Trafico (clases, IA, reaccion) | 472090, 472450, 472A50, 47DC10, 4736D0, hit_476040; aparicion 479080 (PS2 0x1dcdd0): 478B90 (0x1dbba0), 4787E0 (0x1db6d0), 4EFB90 (0x1f3818), 4F0030 (0x1f1a88), 46CE30, 478430, 46CC10; cierre 477F60 | PARCIAL | TRAFC.DAT (TRC1, ACL1 y TAP1: tablas de aparicion, velocidad y puntos fijos por etapa); colores (tabla 0x36bc18, modelos 30..43) y materiales en TRAFn.DAT (TCX1) | camino "Lead" de 472450 portado (velocidad por clase y del jugador, ver Choques entre coches: velocidad del trafico de delante) salvo 472320 / 471F90; 478640/46CDA0 en la union simplificados; puntos fijos de coches de carreras sin coche; camiones 39/40/41/43 solo en OUTRUN2SP (en el juego de la consola sin coche; ver OUTRUN2SP: camiones) | ver Trafico: densidad del original | E |
| Piloto y acompanante | ROB01 / ROB02: PS2 0x201b10 / 0x201c40 / 0x202bf0 y 0x202c58 / 0x202de0; pareja 0x205518 / 0x2055b8; CHR.PS2 (CHR_DR_M00 / S00 / W00 / L00 / G00 / H00, G00_USA), bone.bin, ANIMS.PS2 | PARCIAL | CHRPAR.DAT y CHRppaa.DAT (conv_chr.py --pares), comun/robot.c (con el osage de G00): motor de movimientos, conducir con el volante, codo en la puerta, choque, fuera del asfalto, aburrida, META y TIME OVER (ver Coches: el piloto y la acompanante) | cambio de area, asustada, trompo ni poses pedidas (la meta del piloto, estado 9, hecha: ver Escena de la meta) | - | H, E |
| Rivales | 47CF40 (PS2 0x1ef900) solo desde el gestor de misiones de C2C (496A30, PS2 0x1f9e4c) y la variante 9 (4B02B0, PS2 0x1f7a50 -> 0x1f0910); coches de carreras 47C8A0 / 47BC90 / 477090 / 477510 del modulo del trafico | PARCIAL (2026-11-13): en las misiones de carrera de C2C (ver COAST 2 COAST: misiones de carrera y el rival) | MISION.DAT (conv_mision.py), RC00-29.DAT (conv_trafico.py --rivales), comun/rival.c | no hay rivales en OUTRUN, INFARTO ni CONTRARRELOJ (el original tampoco); SIN PORTAR 472E60, 474080, vaivenes 476CA0..476F40 (ninguna mision de tipo 0 los pide), variante 9 | ver la seccion | E (Flycast) |
| Carretera, pendientes, bifurcacion | STAGE.PS2, BK.PS2 cs_BK_PALM, COMMON obj_course_obj_bk_palm + obj_OBJ_<ZONA>_BUNKI_TX, descriptores 0xb0, csc_data_2 | OK | conv_tramo/conv_union, CURSO.DAT | arbol entero (15 etapas, 10 bifurcaciones con sus carteles, 5 metas); cielo, niebla, luz y sol de la union como el original (ver Entorno); bifurcaciones = las de PALM (las demas no estan en el disco) | objetos animados de BK (ANIMS) | E |
| Temporizador (inicial y ampliacion) | 44DC70 = PS2 0x176730, 450CC0 = PS2 0x1cee40, 450AC0 = PS2 0x1ce1f8 (-1 por paso en 0x5c5f44, crono +1 en 0x5c5f48); cuenta atras t / 60,2 + 0,34 (4BDA30 = PS2 0x1bfff8); crono con CalcFrame2Time (449B30 = PS2 0x1f6d70: t / 60,2, milesimas truncadas); paso real 59,94 Hz: HUD 0,43 % mas lento que el reloj, como el original; tabla 7D2E98 = PS2 0x3cb708 copiada por 0x176040; k636904 = PS2 0x35a938 | OK | TIM1 y KTE1 de PARAM.DAT | formula y tablas CONFIRMADAS en el ELF; variante 1 (fila 0) y dificultad 0 PROBABLES (frontal del PC); preset CONFIRMADO (ver Preset del curso) | trazar el frontal de PS2 | E |
| Checkpoints, meta, resultados | 44FF20 / 4515B0 (branch_state 7D38E8) con las banderas +0x3C de COLI0200; 4513C0 meta | OK | ampliacion al pasar la pancarta CHECK POINT del principio de la etapa siguiente (ver Checkpoint); meta del original en las 5 etapas finales (z = 0 de la union); voz y efecto de EXTEND TIME, GOAL y TIME OVER (voces.c) | escena de la meta del original (modo 19: guion, camara, coche, piloto, efecto y musica; ver Escena de la meta); millas OutRun, continuar (REINTENTAR / ABANDONAR) y GAME OVER del original (modos 34 / 35 / 27, ver Fin de la carrera); tabla de tiempos por etapa de 498670 / 4979E0 desde la Rev 2.9.1 (variantes 1 y 7, sin sus efectos); sin el ranking | — | E |
| Puntuacion modo OutRun | 4F2DF0, 4F2660, 4F2B20, 4B9770 | PARCIAL | main.c pt_* | posicion de carretera = m_pos de CURSO.DAT (CONFIRMADO; antes 2,5 m supuestos, 3,2 veces los puntos por avance); bonus de etapa en el checkpoint | el salto de +64 al cambiar de curso (PENDIENTE) | E |
| Eventos, scripts, progresion, misiones | SCRIPTS.PS2 (Races, RaceAttack, OutrunMiles, Req_*) | FALTA | — | — | — | — |
| Modos de un jugador (INFARTO, CONTRARRELOJ) | 464350 / 464D20 (PS2 0x1ae440 / 0x1a7f28), 44DC70, 479080 | PARCIAL | ver Modos de un jugador | tipos de peticion con objetos; fantasma desde la Rev 2.9.2 (con el coche del jugador, formato propio) | — | E |
| Coches del jugador | CARS.PS2 1-32 / 33-63, disposicion 0x3d3ff0; materiales tabla 4/5 del bloque; colores 0x35ad48 (PC 64D120/408C80); mapa de entorno del bloque | PARCIAL | 15 coches montados como 46A560 (port comprobado), con interior, 9320-11195 vertices dibujados; pintura, 8 colores (Y en la seleccion), reflejo y luces (+7C/+80/+84/+60, CEX2) del original con los bits de noche / tunel de 4A25F0 y la luz de los faros en el suelo (0x188680); piezas animadas (ruedas, volante, aleron, faros, disco caliente, car+F0 y el cuerpo de la fisica; ANI1) | UV de entorno con la formula esferica clasica (la del microcodigo VU1 sin leer); focos de los faros sobre la escena (0x20e1b8 / 40C550) sin portar | ver secciones Coches | E |
| Escenarios, texturas, materiales | STAGE.PS2, VQ, scn_env_fog_<zona> | PARCIAL | 15 tramos + 15 uniones; mallas con su material (tabla 5) y la luz del entorno 1 de la zona; niebla, cielo y sol del original (ver Entorno); tiras (ADC), nodos, LOD y planos del original | objetos de tramo: el registro de obj_course_obj_cs_<zona> que dibuja la etapa (pancarta CHECK POINT y carteles, ver Checkpoint: la pancarta); objetos animados de las uniones (ver Objetos animados de las uniones); cs_ENV_* no es de la escena (es del reflejo del coche); destello con oclusion aproximada por rayos (ver Entorno); luces locales (maya_spl) sin portar | — | E |
| HUD, menus, transiciones | SPRANI/XST, TEXT.PS2 (espanol: SPANISH.bin y XST _sxst) | PARCIAL | HUD.DAT, MENU.DAT; todas las pantallas del menu principal y de un jugador, licencia con VMU (icono, eyecatch y LCD: VMU.DAT, ver VMU), seleccion de coche del original, menu de pausa con AUDIO (ver Menus y Pausa y mezcla) | titulo es un cuadro del video (la pelicula de titulo en bucle detras del rotulo, sin hacer); textos bfont antiguos en la demo automatica y el aviso de rama; rotulos de las filas de opciones (sprites sin localizar) | quitar textos bfont | E |
| Musica y efectos | MUSIC.PS2, SOUND.PS2 ICS/MENU/FE; volumenes 0x11ae38 / 0x100210 / 0x11b600, SOUNDCD.IRX 0x2118 | PARCIAL | musica.c, sfx.c; los 28 temas, sus 28 avances y la del menu (ver Seleccion de musica); mezcla del original (ver Pausa y mezcla) | 48 kHz supuesto; sin sonido por superficie; del choque fuerte solo el SE 0x42 (ver Choque fuerte contra el muro) | — | E |
| Voces (acompanante y locutor) | SOUND.PS2 `\SPANISH\ORC.PAK`, `PERO.pak`; race_robots 489A90/488D80 = PS2 0x202e60..0x205cac; 4B93E0, 4B9CD0, 49C9C0, 499020, GAME 0x1b83e8 | OK | VOCES.DAT (conv_sonido.py), voces.c; verificado en Flycast (ver Voces y video) | una voz nueva corta la anterior (SUPUESTO); "choque con otro coche" = +4 0x4000000 del jugador (4F7BF0 le ha cambiado la velocidad) en un choque; sin voces de asustada/trompo (solo Coast 2 Coast) | — | E |
| Peliculas (titulo y atraccion) | MV.PS2 ts50/attr50.pss; 0x12d480, ADVERTISE 0x25e1d8 | OK | MVTS.VQV / MVATTR.VQV (conv_video.py), video.c | VQ de la PowerVR a 256x256 estirado a 640x480 y audio a 32 kHz (el original: MPEG-2 640x480, PCM a 48 kHz) para caber en ~440 KB/s del GD; al arrancar y por inactividad del titulo, en vez de la demo de carrera | — | E |
| Cargas y estabilidad | — | PARCIAL | carga en segundo plano, mutex del GD | — | medir en GDEMU | E |
| Rendimiento / TA en consola real | — | PARCIAL | tope global del TA (DREAMCAST.md) | primer cuelgue medido en la consola (VB lleno a 24656 vtx) | esperar prueba R de la demo 14+ | E (R pendiente) |

## Voces y video (2026-11-08)

**De donde salen.** SOUND.PS2 guarda los bancos por idioma en `\<IDIOMA>\...` (ENGLISHC/H/J, FRENCH, GERMAN, ITALIAN,
SPANISH). El ELF elige el directorio con la tabla de prefijos de 0x3527a0 (5 juegos x 6 idiomas; PC 754A88): en una
carrera normal el juego 4 (`SPRACE`, FRENCH, GERMAN, ITALIAN, SPANISH, `SPRACE`); con el idioma espanol, `\SPANISH\`
(por defecto en la demo; `conv_sonido.py --idioma` para otro). `SPRACE` (el ingles de la carrera) no esta en el disco
PAL. Los bancos de voz de la carrera del modo OutRun son ORC.PAK (banco 3 de la tabla 0x352840, pedido con 0x8000 =
con prefijo de idioma) y, para GOAL / TIME OVER / GAME OVER, PERO.pak. Los id de sonido son los mismos que en el PC:
cada banco trae dos tablas {id, desplazamiento} (partes A y B); la tabla SE del IRX (0x4b98) da 12 kHz y volumen
0x3fff a todas las voces. 34 muestras en espanol, 34,7 s, VOCES.DAT 214 KB (cabe entero en la RAM del AICA junto a
SFX.DAT y los buferes de la musica).

**Cuando suenan** (voces.c; tablas del ELF en VOCES.DAT; el codigo de PS2 es el del PC, comprobado con capstone: la
cola y los comentarios en 0x2059c0..0x205cac, las reacciones en 0x202e60..0x203d94, el sonido de carrera 0x11C090 =
424940 del PC, la variante 780258 = PS2 0x443508):

| Momento | Voz (id) | Logica del original |
|---|---|---|
| Empieza la carrera (modo GAME) | 0x82 | 0x1b83e8 (init del modo 16), salvo al venir de RESTART |
| Checkpoint (EXTEND TIME) | efecto 0x4d y voz 0x16c | 4B93E0 / 0x1be78c |
| Despues del checkpoint, a 40 posiciones de carretera en la etapa nueva | 0x79..0x7c (zona 1..4 = nivel en el arbol) | 488BD0 (solo modo OutRun), tabla 0x36f958, por la cola |
| Derrapes (cada uno cuenta a los 120 cuadros) | 1.o 0x6e, 2.o-5.o 0x6f, 6.o-13.o 0x70, 7.o y 10.o 0x74, desde el 14.o uno al azar de 0x75/0x76/0x77 | 4882B0 / 488C90, tabla 0x36f9a8, por la cola |
| Derrape largo | 0x72 a los 360 cuadros, 0x73 a los 600 | 488220, tabla 0x36fa80, por la cola |
| Empieza un choque (estados 1, 2, 5) | 0x15a, 0x15a, 0x159 | 489A90 "aire" (car+2C8 > 0), lista 0x40eba8 (personajes 9 y 10; la acompanante del modo OutRun es la 9 sin licencia en 487F50: PROBABLE; con licencia, 0x40ebb8: 0x15d/0x15c/0x15b) |
| Recuperacion tras el choque | grande (estados 2 y 5): 0x169/0x162/0x15f/0x160 por turno (2 en uso); el resto y el fin del estado 7: 0x167 | 489A90 (car+2F8 > 0), listas 0x40eb10 / 0x40eb20 |
| Choque con otro coche durante un choque | una de 0x15d/0x15c/0x15b/0x15a/0x159 al azar, espera 90 cuadros | 489A90 (car+4 & 0x4000000 y +2F0 & 2), lista 0x40ef70 |
| GOAL | 0x16b | 49C9C0 (4249F0) |
| TIME OVER | 0x155 | 499020 |
| Poco tiempo (600 cuadros o menos) | efecto 0x18d cada 16, 8 (< 420) o 4 (< 240) cuadros | 4B9CD0 / 0x1c00d8 |

Cola de comentarios (488130 / 4881C0): 8 entradas; una nueva no entra si la anterior aun espera con prioridad mayor
o igual; cada entrada suena y ocupa 120 cuadros. Las voces de asustada (0x16d, 0xcc) y trompo (0x160, 0x15f) solo las
dice en Coast 2 Coast (variantes 3 y 4). El id lleva a veces 0x800 o 0x5000 encima (banderas del controlador de sonido
del IOP, sin estudiar): se usan los 9 bits bajos. No hay voces en los menus principales (Fe.pak y MENU.pak son
efectos; la voz de ranking 0x158 es de SPRANK.pak).
SUPUESTO: una voz nueva corta la que suena (un canal para voces y otro para los efectos de carrera). El volumen de las
voces sale de la tabla SE (0x3fff) por el general como en el IRX (ver Pausa y mezcla). Sin la voz de GAME OVER 0x154
(no hay modo GAME OVER).

**Peliculas.** MV.PS2: `ts50/ts60.pss` (pantalla de titulo con el rotulo, 17,6 s) y `attr50/attr60.pss` (atraccion,
55 s); MPEG-2 640x480 a 25 fps (las de 50 Hz) o 29,97, audio PCM de 16 bits estereo a 48 kHz en paquetes privados (sin
idiomas ni subtitulos). 0x12d480 elige la de 50 o 60 Hz (0x328a8c) y la de titulo (1) o atraccion (0); el modo
ADVERTISE (0x25e1d8) pone primero la de titulo y luego la de atraccion, alternando, hasta START. La Dreamcast no puede
con MPEG-2 a 640x480 (ni KallistiOS trae RoQ ni MPEG en esta version, sin kos-ports): `conv_video.py` las pasa a
texturas VQ de la PowerVR (256x256 RGB565 estiradas a 640x480; libro de 256 entradas por grupo de hasta 12 cuadros y
en cada corte de plano; 16 KB de indices por cuadro que descomprime el chip) con el audio en ADPCM de Yamaha a 32 kHz
(un flujo por canal), ~440 KB/s a 25 fps (antes 512x256 con libro por cuadro y 48 kHz: 0,9 MB/s, demasiado para
asegurar el GD real; ver docs/DREAMCAST.md, Peliculas, con la causa del fallo en la consola: los canales 33 y 34 del
flujo de audio no se encendian). Se usan las de 50 Hz (la demo sale en PAL 50 Hz). MVTS.VQV 7,9 MB, MVATTR.VQV
24,9 MB, al final del disco (`mkgdi.py`). video.c las pasa al arrancar (gancho
`video_titulo` antes del primer titulo) y por inactividad en el titulo (MENU_DEMO, en vez de la demo de carrera);
START las salta. Las pruebas con PRUEBA_AUTO / PRUEBA_JUGADOR no las pasan. Si una se atasca (sin cuadros nuevos en
1 s) escribe un diagnostico sobre la imagen y sigue al titulo. Verificado en Flycast el 2026-11-09: las dos enteras
con audio y sin faltas (tambien con el lector limitado a 470 KB/s).

## Arranque: modo de video 50 / 60 Hz y pantalla de carga (2026-11-09)

- La pantalla del original PAL para elegir el modo (estado 4-5 del arranque 0x23B190, descriptor 0x3BF8C8: init
  0x104E40, paso 0x105110, dibujo 0x105390) en lugar del texto bfont: ventana 0x271800 / 0x271CA8 con las piezas de
  spr_etc_xst (banco 3), titulo 0x32E, boton 0x294 con su icono (escena 0x480009 cuadro 25, A del port), lista
  0x26A940 (50Hz / 60Hz - Prueba, resalte 0xFFFFCE0A / 0xFF8B9091), ayuda por fila (0x441 / 0x333), y la prueba de
  60 Hz con "No" / "Si" y cuenta atras de 15 s (0x335, t > 900 pasos vuelve a 50 Hz; 30 pasos sin aceptar tras cada
  cambio). Fuente 14 y textos de TEXT.PS2 en el idioma del port (es). tools/conv_modo_video.py -> MODOV.DAT;
  comun/modo_video.c lo dibuja en el framebuffer (aun sin PowerVR). Igual que antes: solo en consola PAL sin VGA;
  60 Hz = NTSC entrelazado (DM_640x480), 50 Hz = PAL.
- Diferencias: no se ofrece "60Hz Progressive (480P) - Test" (sin VGA la Dreamcast no saca 480p); la ventana conserva
  el alto del original (calculado con los textos de las tres filas), la tercera fila queda vacia. Efectos SE 1 / 0x40
  con sfx_menu (sfx_inicia se adelanta a esta pantalla). El fondo es negro (nada mas se dibuja en ese estado).
- Pantalla de carga: el texto "OutRun 2006 C2C - demo Dreamcast. Cargando la etapa..." y el "DEMO" de la demo
  automatica se quitan; la carga queda en negro (el original pasa de esta pantalla a los logos de SPLASH_CVT sin
  rotulo de carga). Pendiente: lo que pinte el original durante la demo automatica (sin investigar; hay un banco 0x45
  DEMOSPLASH_CVT).

## Cuadros por segundo: 45 fijos en carrera (DESVIACION pedida por el usuario, 2026-11-12)

El original presenta un cuadro por campo cuando el cuadro esta listo (hasta 60 / 50 por segundo) y da a cada cuadro
tantos pasos de logica como campos han pasado (0x1078c8). A peticion del usuario la demo presenta en carrera 45
cuadros por segundo con el ritmo del barrido (3 cada 4 campos a 60 Hz, 9 cada 10 a 50 Hz; `FPS_OBJETIVO`,
docs/DREAMCAST.md, 45 cuadros por segundo fijos). La logica no cambia: 59,94 pasos por segundo como el original en
los dos modos (1, 1, 2 pasos por cuadro a 60 Hz), asi que la fisica, el crono, el tiempo y las animaciones van a la
misma velocidad; solo se ven menos cuadros. Menus, peliculas y cargas como antes.

## Menus (2026-11-08)

Todo de SUMO_FE_CVT (banco 0x44) y de tablas del ELF localizadas por su codigo (comprobadas en conv_menu.comprueba_tablas);
textos de TEXT.PS2 en espanol (OR2_IDIOMA=es por defecto en conv_menu y conv_hud; la PAL elige el idioma por el de la
consola, el port lo fija al convertir). Hoja de capturas de Flycast: scratchpad `menu3/hoja.png`.

- Fallos graficos y causa: (1) hudd_baja liberaba la VRAM de una pantalla en el paso de logica, cuando la PowerVR aun
  dibujaba el cuadro anterior con ella; la pantalla nueva se subia en ese hueco y salia un cuadro con basura. Ahora
  hud.c apunta el ultimo cuadro en que se uso cada textura y la libera 3 cuadros despues (hudd_fin_cuadro tras
  pvr_scene_finish; hudd_vacia espera 40 ms antes de una carga: carrera, coche nuevo). (2) Si no cabia, el menu subia
  "lo que hubiera" y dibujaba la pantalla a medias; ahora espera (sin fundido) a que este entera (hasta 90 cuadros).
  (3) `pvr_mem: out of PVR memory`: se intentaba subir con la VRAM libre justa (partida); ahora echa antes las pantallas
  viejas si no sobran 256 KB. Verificado: ningun aviso en Flycast recorriendo todo, coches incluidos. (4) RAM: 35 000
  piezas (1,4 MB) por guardar cada pieza en cada cuadro; ahora una pieza vale para una racha de cuadros iguales
  (banderas >> 4, orden de capas de la escena, conv_menu.tramos_piezas): 23 000 piezas con el doble de pantallas, 960 KB.
- Pantallas (flujo de 4D7300 / 4C9290 / 4E2970 / 4DE2B0 del PC con los tokens del PS2): titulo -> sin licencia, elegir
  licencia -> menu principal (UN JUGADOR, MULTIJUGADOR, OUTRUN2SP, RANKINGS, OPCIONES; sin MULTIJUGADOR desde la demo 41,
  ver Menu principal sin MULTIPLAYER) -> un jugador -> coche -> cambio ->
  musica -> carrera; LICENCIA (X) desde el menu principal y un jugador. MULTIJUGADOR (0x44002A, tabla 0x41C260: en
  linea, LAN), RANKINGS (0x4400A3, 0x41D908: un jugador, OUTRUN2SP, multijugador), OUTRUN2SP, HEART ATTACK, TIME ATTACK y
  SHOWROOM existian con la accion bloqueada (sin red, ranking ni esos modos); desde 2026-11-09 HEART ATTACK y TIME ATTACK
  y desde 2026-11-13 SALON (ver SALON) llevan a lo suyo; HEART ATTACK y TIME ATTACK
  llevan a su carrera (ver Modos de un jugador) por los tableros de cursos del original (desde 2026-11-13, ver Tableros
  de cursos de INFARTO y CONTRARRELOJ). OPCIONES (cabecera 0x44006D, barra
  0x44000B): vista (+D8, textos 3-5), unidades (+DC, 6-7) y dificultad (+E0, 8-0xC) de la licencia, sin dar la vuelta,
  se guardan al salir (4D76F0); los rotulos de fila del original son sprites no localizados: se escriben "Ver config." y
  "Config. juego" (0x2B, 0x2A) (PROVISIONAL). menu_vista/menu_mph/menu_dificultad las exportan (la carrera aun no las
  usa: TIM_DIF sigue en 0).
- Licencia (CONFIRMADO en el ELF: tablas 0x416EA8 y 0x41F8E0, menu 0x23FFC0 y resalte 0x23FB90 = 4DD9C0 / 4DD990 del
  PC): tarjeta 0x440088 (entra 0-12), flechas 0x44008D/0x44008E a +-200, CREATE NEW LICENSE (0x218) y nombre nuevo
  0x219; menu de la licencia Elegir / Editar / Reiniciar (0x233 / 0x237 / 0x234, con si / no). Editor: rayas 0x440087
  (opcion i en 51+i), tarjeta 0x44009F en -8, rejillas de bandera (26, 4 columnas) y signo (12, 3 columnas), LISTO
  0x440090 en (-84, 110); desde el nombre se baja a la nacionalidad (la foto no se alcanza: igual en PS2 y PC); B deshace
  nombre, bandera y signo; los nombres ENTIRETY (todo desbloqueado, 447360) y MILESANDMILES se aplican al salir con B,
  como el original. Registro de 0x40C bytes con la disposicion del original y los valores nuevos de 4471A0; hasta 4.
  Guardado: VMU A1, fichero OR2C2C (cabecera VMS con icono animado y eyecatch del logo, ver VMU), al elegir, al acabar
  el editor y al salir de opciones.
  El teclado del nombre (468D40: 1-0, A-Z, espacio y borrar; START acepta) no tiene sprites en el disco del PS2: se
  escribe con la fuente 14 en 6 columnas donde van las rejillas (PROVISIONAL). Textos de la tarjeta con la fuente 14
  y el color de sus rotulos (medido en la textura); sin cifras de COMPLETADO / RANKING / T.CONDUC. (la demo no las lleva).
- Seleccion de coche (0x2445D8 = 4C9010, control 0x2448xx = 4C9290): carrusel 0x4400E5 (normales) y 0x4400E6 ("_t"),
  ficha k quieta en 20(k+1) (antes 0x44007E, que es otro carrusel con otro orden: el fallo de "coches no correctos");
  orden = tabla 0x375968 (Dino 246, 365 GTS/4, F50, 360 Spider, F355 Spider, Superamerica, Testarossa, GTO, 328 GTS,
  250 GTO, 512 BB, F40, 550 Barchetta, F430, Enzo); desbloqueo 0x3759E0 (las 2 primeras siempre, el resto por bit de la
  licencia +0x28: con una licencia nueva solo Dino y 365, como el original), colores 0x375A20 (-1 siempre, -2 nunca; ver
  Colores de los coches 2026-11-10) y
  color inicial 0x375C00; Y = siguiente color desbloqueado (sin sonido, como el PC), A en una bloqueada = sonido 3,
  candado 0x440066 cuadro 5 en (-275, -24). Variante "_t" con X (en el original el bit 0x2000 del mando: boton de la DC
  SUPUESTO); los modelos "_t" no estan convertidos y se ven y corren con el normal (PROVISIONAL). La ficha del coche lleva
  barras de manejo / velocidad / aceleracion, clase y cambios de la propia escena. El coche gira +100/65536 de vuelta por
  paso (4A5B20, CONFIRMADO en el PC); la camara de la seleccion sigue siendo la de antes (la del evento 0x181 sin leer).
  Los movimientos del carrusel guardan un cuadro de cada dos (RAM).
- Ganchos: `menu_gancho(pantalla, evento, valor)` (comun/menu.h) al entrar en cada pantalla y al elegir; `video_titulo()`
  (simbolo debil en demo/main.c) antes del primer titulo.
- VRAM por pantalla (MENU.DAT): COMUN 170 KB, MENU 290, MODO 68, COCHE 512, CAMBIO 256, MUSICA 274 + la lista que se ve
  (MUSICA0 546, MUSICA1 546, MUSICA2 808; ver Seleccion de musica), LICENCIA 546,
  RANKING 52, MULTI 34, OPCIONES 34, TITULO 38, PAUSA 18 (se sube al pulsar START en carrera). TA: el 2D del menu son
  menos de 300 cuadrilateros por cuadro.
- Pantalla de musica en blanco (queja del probador, 2026-11-08; reproducido en Flycast: solo el fondo y la barra, y
  `out of PVR memory`). Causa: la VRAM partida. El coche 3D de la seleccion (Dino: 37 texturas) se sube cuando las
  pantallas del menu ya estan; al echar COCHE / CAMBIO quedan huecos a los lados de sus texturas: 1319 KB libres pero
  el mayor bloque seguido era de 704 KB, y MUSICA son 3 texturas de 256 KB + 34 KB (802 KB). asegura() no encontraba
  sitio, esperaba 90 cuadros y dibujaba sin texturas. Arreglo (menu.c / main.c): si una pantalla no cabe, a los 2
  cuadros el menu pide soltar el coche 3D (gancho `menu_suelta_coche`; main lo vuelve a cargar al volver a la
  seleccion o al empezar la carrera) y, si a los 20 cuadros aun no cabe, baja todas sus pantallas y las sube otra vez,
  las de mas VRAM primero. Ademas musica_pon ya no espera (hasta 3 s) a llenar el colchon antes de sonar: arranca
  en musica_poll; antes el menu se quedaba sin dibujar al entrar en la musica y al cambiar de tema. Verificado en E:
  la musica sale entera tras el cambio, cambio de tema y vuelta a la seleccion con el coche recargado; sin avisos de
  VRAM.
- Verificado en E (Flycast): todas las pantallas y transiciones, crear y editar licencia, guardar y leer la VMU entre
  arranques, coche bloqueado, variante, color, cambio, musica y salida a carrera con el Dino; demo y vuelta al titulo.
  No probado en R.

## Menu principal sin MULTIPLAYER, musica al volver del frontal y pantallas de carga (demo 41, 2026-11-13)

- Musica al volver de OUTRUN2SP (fallo): B en el coche del frontal de recreativa (4BF510, funcion 0xB) para la musica
  (401030) y pide el modo 32; el init del menu (estado 0xC del cargador, PC 4456EA; con [7B17EC], la vuelta a un menu
  ya puesto) la pone otra vez desde el principio: 401000(0, 0x1E, 1) en 445746..44574C (decompilacion:
  object_runtime_loader_stage12_4456ea de pc_common_control.cpp). La demo no la volvia a poner: el menu creia que la
  pista 30 seguia sonando (pista_sonando) y pon_pista no hacia nada. Ahora suena la 30 desde el principio, como el
  original (comun/menu.c, FSP_MENU).
- MULTIPLAYER fuera del menu principal: DESVIACION pedida por el usuario (demo 41; sin multijugador). Quedan UN JUGADOR,
  OUTRUN2SP, RANKINGS y OPCIONES. El carrusel del original es la escena 0x440000 (tabla 0x418CA8: 5 opciones, entra
  0-20, opcion i quieta en el cuadro 20 + 20 i): una fila de rotulos en y = 360 que pasa de derecha a izquierda (el
  elegido en x = 62, los de su derecha cada vez mas transparentes, un punto separador delante de cada uno), el titulo y
  la descripcion de la elegida (y = 166 / 211, quietos, se funden) y una fila de 5 puntos (y = 391,5) con el de la
  elegida resaltado. Se dibuja como un carrusel de 4 sin hueco: los sitios 0..3 de la escena (sus cuadros SEL0..3 y
  MUEVE01..23) con las opciones 0, 2, 3, 4: el rotulo de la opcion en el sitio del rotulo de ese sitio (con su
  movimiento y su transparencia), el titulo y la descripcion en su propio sitio, y sin lo del sitio 4 (su rotulo, su
  separador y su punto). Las reglas (hudd_reglas de comun/hud.c) salen de las piezas de MENU.DAT (menu_sin_multi: en
  SELi el rotulo mas opaco de y = 360, la descripcion de y = 211, el titulo por encima; el separador de y = 371; los
  puntos de y = 391,5); si MENU.DAT no fuera el esperado, el menu sigue con las 5 opciones. El espacio entre rotulos es
  el de la escena para el rotulo de ese sitio (MULTIPLAYER es mas ancho que OUTRUN2SP: 29 px mas de hueco entre
  OUTRUN2SP y RANKINGS cuando OUTRUN2SP esta en el sitio 1). MENU.DAT no cambia. La pantalla MULTIJUGADOR y la fila
  multijugador de RANKINGS siguen en MENU.DAT sin camino que lleve a ellas (RANKINGS, bloqueado como antes).
- Boton Y del menu principal y del de un jugador: GUARDAR en lugar de FRIENDS (DESVIACION pedida por el usuario,
  revision 42; en el original es el juego en linea, 0x296 "AMIGOS", que no hay). Guarda la licencia y los rankings en
  la VMU (comun/menu.c vmu_guarda, ver docs/DREAMCAST.md, Guardado en la VMU): primero "Guardando datos." (el principio
  de TEXT.PS2 0x206; lo demas de ese texto habla de la memory card del PS2) en una franja en medio de la pantalla y, ya
  guardado, "Exito al guardar." (0x202) con el sonido de aceptar o "Error al guardar!" (0x31D) en rojo con el de
  bloqueado, unos 2,5 s. La barra de ayuda dice GUARDAR (0x42F "Guardar" en mayusculas, como los demas textos de la
  barra). Textos y barra en MENU.DAT (conv_menu.py: AYUDA, TEXTOS_GUARDAR). Revision 44 (tambien DESVIACION, para
  la consola real sin cable serie): el guardado va en un hilo y el menu se sigue moviendo con "Guardando datos."; si
  falla, debajo de "Error al guardar!" sale un codigo de diagnostico (docs/DREAMCAST.md, Guardado en la VMU) y el
  aviso dura ~8,5 s; sin sitio en la VMU dice "No hay espacio suficiente para guardar." (la primera frase de TEXT.PS2
  0x30D, el aviso de sin sitio del PSP; los de la memory card hablan de la ranura del PS2); L + R + Y abre una pantalla
  de diagnostico de la VMU (letra de la BIOS), que el original no tiene.
- Iconos de los botones de la barra de ayuda: DESVIACION pedida por el usuario (revision 42). Antes eran el boton gris
  del PS2 (banco 0x48, cuadro 25) sin su simbolo y con la letra de la Dreamcast pintada encima en su color; ahora son
  botones de la Dreamcast dibujados al convertir (conv_menu.py boton_dc): circulos de los colores del mando (A rojo, B
  azul, X amarillo, Y verde) con degradado, borde mas oscuro, brillo arriba y sombra, y la letra blanca de la fuente
  14 con contorno oscuro. Mismo tamano y sitio que el icono del PS2 (la misma VRAM y la misma alineacion con el texto).
  Los mismos en FIN.DAT / FINC2C.DAT (CONT_ELIGE, conv_fin.py) y MODOV.DAT (conv_modo_video.py), que usan
  conv_menu.iconos_dc. En la ventana de pausa (comun/menu.c pz_boton) se dibujan igual con poligonos de color (sin
  textura: sin VRAM) delante de ELEGIR / ATRAS en vez de la letra de color suelta.
- Pantalla de carga de OUTRUN2SP (el mapa del preset, ver OUTRUN2SP, Carga) mas larga: DESVIACION pedida por el usuario
  (demo 41). Se pone al salir del frontal (antes de leer voces, coche, pareja, HUD y la etapa de salida con sus
  texturas, que antes se leian con la ultima imagen del frontal quieta) y se quita cuando la carga ha acabado y como
  poco a los 3 s (PCARGA_MINIMO; una vuelta y media de la escena de 0x78 cuadros). En el original la escena se crea al
  acabar de leer el curso y dura lo que tarda START en cargar del DVD; en Flycast la demo carga al instante y antes no
  se llegaba a ver. Con REINTENTAR (modo 30 -> START) sale tambien, sin minimo.
- Pantalla de carga propia al empezar un modo de un jugador: DESVIACION pedida por el usuario (demo 41: "una pantalla de
  carga con estilo de outrun implementada por nosotros para precarga ... o para que funcione de buffer, al principio
  del modo, no entre etapas"). En OutRun, Heart Attack, Time Attack y las misiones de Coast 2 Coast (variante 6, con
  los modelos de su rival; pcarga_tipo da la propia a todo el juego de la consola), al salir de la musica del menu: el
  blanco y el logo "OutRun2006 Coast 2 Coast" del titulo (TITULO_FONDO, TITULO_LOGO de MENU.DAT, en su sitio) y, en el
  sitio del START del titulo (320, 400), el medidor de volumen de la pausa (0x2C00EF, cuadros 0..10) como barra de lo
  cargado (sin pasar de la parte cumplida del minimo). Sin textos: TEXT.PS2 no tiene un "Cargando" suelto. Lo que el
  original pone en ese momento (el icono de carga 0x480000 de los modos 28 / 30, banco 0x48 SUMO_LOADING) sigue sin
  convertir. Debajo, toda la carga de la carrera (voces, coche, pareja, HUD y sus texturas, coches del rival, la etapa
  de salida y sus texturas a la VRAM antes de la cuenta); como poco 3 s. No sale en REINTENTAR (no es empezar el modo;
  las piezas del titulo ya no estan en la RAM), ni en la demo de atraccion, ni entre etapas.
- Como se anima: la escena se dibuja entre los pasos de la carga y tras cada lectura del GD del hilo del juego (gd_latido
  de comun/gd.c: cada 128 KB como mucho; en la consola ~15 cuadros por segundo mientras lee) y sin parar durante el
  minimo. Mientras se sube a la VRAM lo que se queda en la carrera (coche, pareja, HUD, camiones, coches del rival: sin
  la pantalla en la VRAM, para que la de la carrera quede igual) y despues de quitarla (la zona del curso con las
  texturas de la etapa, el banderero y la escena de la salida) se ve su ultimo cuadro, quieto. El reloj de la logica
  empieza al acabar (como antes).
- Gancho para los modos nuevos: demo/pcarga.h. Un modo que empiece por la salida de demo/main.c tiene la pantalla con
  pcarga_tipo (asi las misiones de Coast 2 Coast); uno con su propia carga pone pcarga_entra(PCARGA_PROPIA, ...),
  pcarga_avance(0..1) entre sus pasos, pcarga_pausa / pcarga_sigue alrededor de lo que suba a la VRAM para toda la
  carrera y pcarga_suelta() antes de reservar la zona del curso.

## Colores de los coches (2026-11-10)

Queja: "algunos coches no dejan elegir el color rojo". Causa: conv_menu.py leia la tabla de bits de los colores en
0x375A1C, 4 bytes antes de la buena: el codigo que la usa (4C8FC0 = PS2 0x245038: `lui v0,0x37; addiu v0,v0,0x5a20`,
indice ficha x 16 + color x 2) lee 0x375A20 (30 x 8 i16, acaba en 0x375C00 donde empieza el color inicial). Con la
tabla corrida dos colores, cada ficha tenia los bits de otros colores (p. ej. el Dino: el color 1, rojo y por defecto,
pedia el bit 0 y salia otro color; ahora es -1, siempre). conv_menu comprueba el par lui/addiu. MENU.DAT regenerado
(solo cambian esos 480 bytes). Verificado en E (menu real NTSC y PAL): el Dino sale rojo por defecto (antes negro).

## La chica en los menus (estudio 2026-11-10)

Queja: "falta la imagen de la chica en el menu". Donde sale una chica en el frontal del PS2 (SUMO_FE 0x44, escenas
recorridas cuadro a cuadro buscando sus texturas): 0x4400F0 cuadros 0..3 (las chicas; 5..7 los chicos) con la tabla
0x417480, que dibuja 0x241228 (pantalla de C2C de campeonatos / peticiones, vtable 0x417094; palabra +0x208 del
frontal); 0x4400F1 (tabla 0x417998, mapa de misiones de C2C); 0x4400D1 caras (C2C); 0x440086 la foto de la licencia
(cuadro +0x20; 3 = chica); en carrera, la silueta 2C0067 del HUD del INFARTO (ver arriba). Las pantallas de COAST 2
COAST (categorias, mapa de misiones, campeonatos) no estan en la demo (C2C corre como OutRun: ver Modos de un jugador),
por eso no sale; el menu principal, UN JUGADOR, coche, cambio y musica del original no llevan chica. PENDIENTE: saber
cual de estas pantallas es la de la queja (la foto de la licencia sale si +0x20 la elige).

## Menus: fuente, progreso y R1 (2026-11-09)

Quejas (DC real, RGB): letras del menu que no se leen; la tarjeta no muestra el progreso (con ENTIRETY deberia dar
100%); R1 no hace nada en la musica.
- **R1**: el mando del PS2 se traduce en 0x101e90 (libpad -> bits del juego): R1 (0x800) y R2 (0x200) dan 0x2000, L1 /
  L2 0x1000, cruz 2, circulo 4, cuadrado 8, triangulo 0x10. La musica (0x24e748, = 4C9C20 del PC) y el coche (0x244c44,
  variante normal / "_t", = 4C9290) miran 0x2000 en lo pulsado del cuadro (+8, 0x101bd4). En la DC era X (supuesto):
  ahora el gatillo R (menu.c lee_r1; analogico, pulsado desde 128, el cuadro en que pasa). La propia pantalla lleva el
  icono "R1" junto al nombre de la lista. Listas: 0 OutRun 2, 1 OutRun 1986 / Turbo OutRun, 2 remezclas (9 + 7 + 12 =
  los 28 temas; verificado en E: avances MUS33, MUS42, MUS49 al pasar 0 -> 1 -> 2).
- **Fuente**: los glifos de la fuente 14 (17 px) y los textos e iconos de la barra de ayuda compartian textura con el
  fondo y salian en VQ (bloques de 2x2, libro de 256 para toda la textura): bordes comidos. Ahora conv_menu.py los
  empaqueta aparte (pantalla(texto=True) en COMUN y PAUSA) y nunca los comprime (SIN_VQ): 16 bits, texel a texel
  (COMUN 170 -> 234 KB, PAUSA 18 -> 26 KB). Sin mipmaps (nunca los hubo) y filtro bilineal a escala 1. PENDIENTE: el
  arte del frontal con letras dentro (rotulos de los menus, descripciones, titulos de la musica) sigue en VQ cuando el
  error es pequeno; sin ella, p. ej. COMUN pasaria de 234 a 1344 KB y MUSICA2 de 808 a 1056 KB (no probado en R).
- **Tarjeta de la licencia** (elegir licencia; PS2 0x275080 rellena, 0x275f80 coloca = 4E2150 / 4E2640 del PC): fuente
  14 sin alinear, nombre blanco en (202, 167) (editor: 126, 168, PS2 0x23f30c); porcentaje "%5.2f%%" en (354, 209)
  (359 en la nueva), rango "%s (%d)" (nombres 0x3781d8 con +0x118, puntos +0x110) en (359, 232), proporcion
  +0x104 / +0x108 x 100 en (359, 255) ("?" los dos si +0x108 <= 0), tiempo "%2d''%02d'%02d" de +0x10C (0x1f6d70:
  cuadros / 60,2) en (359, 278), millas "%.0f" de +0x24 en (210, 311); la nueva: "0.00%", "?", "?", "0", "0". Antes
  el nombre iba en (172, 175) centrado en vertical (medido). Color: el de los rotulos (col_tarjeta, medido); el PS2
  pasa 0xFF3F474A (PENDIENTE: como lo modula el GS).
- **Porcentaje** (0x1a17e8 = 447400): suma de pesos {10, 20, 30, 40, 60, 80, 100; 0 sin nota} (0x35c8d8) de las notas
  de los 7 grupos (0x2420a0) / 7. Grupos 0-3: categorias 0-4, 5-13, 14-24, 25-39 (0x241ed0: media de las notas de
  licencia +0x1DD + 5c, tantas como carreras de Races.bin con la clave de la categoria en RaceAssignment.bin; 7 si
  falta una) -> suma(nota + 1) / n - 1. Grupos 4-6: campeonatos Request_Course_SP / Request_Course_OR2 /
  Req_Course_MIX_PS2 (0x375398; 15 carreras, tipo en +0x1C de sus registros de 0x78), notas en los cuartetos de
  +0x36E + 2 tipo (1,1,1,2,2,2,3,3,3,3,4,4,4,4,4 por carrera, 0x4172cc) -> suma / 41 - 1. Las tablas van en MENU.DAT
  (bloque PRG1 de MNU2, de SCRIPTS.PS2 y del ELF, con comprobaciones). Una licencia nueva da 0.00% y ENTIRETY
  (447360: notas 6, cuartetos 0x66) 100.00% (todos los grupos nota 6).
- **Falta** (inventario del frontal del PC frente al port): las carreras del port no escriben aun notas, millas ni
  tiempo en la licencia (misiones de C2C, SCRIPTS: FALTA), asi que una licencia normal se queda en 0.00% / 0 millas;
  sin pantalla de misiones (frontend_missions / categories), rankings (frontend_rankings, records), showroom (hecho
  despues: ver SALON),
  OUTRUN2SP, multijugador ni red (sus pantallas estan con la accion bloqueada). En la tarjeta, los iconos pequenos
  siguen siendo los del editor desplazados (el PC usa 0x44006A / 0x440077 / 0x440078 en (-120,-8), (-137,44),
  (-103,44)).

## Seleccion de musica (2026-11-09)

Queja: "faltan musicas en el selector" (solo estaban la lista 0 y 3 temas de carrera: MUS0-2, los avances 33-35 y la
del menu 30). Ahora como el original (4C9790 / 4C9C20 del PC, frontend_music.cpp; tablas del PS2):
- **Listas** (PS2 0x375DA8 = 68F36C: 9 / 7 / 12): carruseles 0x4400C6 / 0x4400C7 / 0x440033 y titulos 0x440030 /
  0x44002F / 0x44002E (tabla 0x419298, 0x4195D0, 0x419628), nombre de la lista 0x4400C8 (cuadros 1, 2, 0 = 5C9F18). Tema
  plano k = cursor de la lista mas los temas de las anteriores (4C9750), cursor propio por lista (84B0EC) que se guarda
  entre carreras. Pista de carrera de cada tema: PS2 0x375DB0 = 68F370, la identidad (tema k = `\k.ps2` de MUSIC.PS2 =
  MUSk.ADP); avance = k + 0x21 (MUS33..MUS60); menu = 0x1E (MUS30). conv_menu.py comprueba las tres tablas en el ELF.
  Orden (cartas del propio MENU.DAT): lista 0 (OutRun 2) SPLASH WAVE, MAGICAL SOUND SHOWER, PASSING BREEZE, RISKY RIDE,
  SHINY WORLD, NIGHT FLIGHT, LIFE WAS A BORE, RADIATION, NIGHTBIRD; lista 1 (OutRun 1986 / Turbo OutRun 1989) SPLASH
  WAVE, MAGICAL SOUND SHOWER, PASSING BREEZE (1986), SHAKE THE STREET, RUSH A DIFFICULTY, WHO ARE YOU, KEEP YOUR HEART
  (1989); lista 2 (remezclas de OutRun 2) SPLASH WAVE, MAGICAL SOUND SHOWER, PASSING BREEZE (EURO REMIX), RISKY RIDE
  (GUITAR MIX), SHAKE THE STREET, WHO ARE YOU, RUSH A DIFFICULTY, KEEP YOUR HEART, SHINY WORLD y NIGHT FLIGHT
  (PROTOTYPE), LIFE WAS A BORE y NIGHT FLIGHT (INSTRUMENTAL).
- **Desbloqueo** (PS2 0x4191E8 = 5C9AB0): temas 0..6 siempre; 7..27 con los bits 172, 173, 174..192 de la licencia (+0x28).
  Con una licencia nueva (4471A0) solo los 7 primeros, como el original: los demas con candado 0x440066 cuadro 5 ("NO
  DISPONIBLE") y A da el sonido 3; ENTIRETY (447360) los abre todos. El avance suena aunque este bloqueado (como el PC).
- **Botones**: izquierda / derecha sin dar la vuelta; gatillo R = lista siguiente (0 -> 1 -> 2 -> 0; bit 0x2000 del
  frontal = R1 / R2 del PS2, ver "Menus: fuente, progreso y R1") sin sonido; Y = ALEATORIO (0x10 del PS2): `(rand & 0x1F) *
  27 >> 5` (0..26: el tema 27 nunca sale, como el original) hasta uno desbloqueado (100 intentos, si no el 0) y sale a la
  carrera sin el sonido de aceptar y sin mover el cursor; A con desbloqueado: sonido 0x40 y carrera; B vuelve al cambio
  con la musica de menu.
- **VRAM**: una pantalla por lista (MUSICA0/1/2) ademas de la comun (MUSICA: cabecera, radio, ayuda, nombre, candado);
  `mascara()` de menu.c pide la de la lista que se ve y, al cambiar de lista, espera a subirla (sin carrusel mientras;
  el resto de la pantalla sigue). Las tres juntas eran 1,9 MB.
- **Audio** (`tools/c/musica.c`, desplazamiento 0 e intercalado 0x2000; reproduce bit a bit los MUS0-2/30/33-35 de
  antes): ADPCM de Yamaha estereo a 48 kHz, el mismo formato que ya sonaba en GDEMU (48 KB/s). Sin desviacion: 28 temas
  422 MB (de 9,1 a 19,4 MB; 2 h 26 min) + 28 avances 49 MB + menu 1 MB = 471 MB (antes 50 MB). Datos de la pista 3 sin el
  relleno: unos 622 MB de los ~1008 MB del area de alta densidad (mkgdi.py rellena hasta el final: la pista 3 mide siempre
  1008 MB); las peliculas siguen al final (MVATTR.VQV en el LBA 532853, MVTS.VQV en 545018, 1ST_READ.BIN en 548896).
  Mono o menos frecuencia no hacen falta.
- Verificado en E (Flycast, arranque real con la pelicula de inicio leida del final del disco): las 3 listas con los
  28 titulos, carteles y cintas, su avance (MUS33..MUS60 en el registro), candados de una licencia nueva, A en uno
  bloqueado (se queda), lista que da la vuelta, y carrera con SHINY WORLD (MUS4.ADP, nuevo): la grabacion del audio de
  Flycast correlaciona con MUS4 (0,13 frente a 0,03 de MUS3/MUS5) desde el segundo ~12 del tema. ALEATORIO (Y) salio directo a la
  carrera con NIGHT FLIGHT (MUS5; correlacion 0,4-0,75 en ventanas de 4 s); en la pausa, AUDIO con la musica a 0 la
  quita (0,03; el flujo sigue avanzando) y a 10 vuelve (0,4-0,5), guardado en la VMU. No probado en R (GDEMU): solo cambia el tamano del disco y el numero de ficheros.

### ALL I WANT (retirada en la Rev 1.1, 2026-11-20)

La tarjeta ALL I WANT (Crazy Taxi, tema 28, `MUS67.ADP`) que se habia anadido a la lista 2 ya no existe: la lista 2
vuelve a sus 12 temas del original (`TEMAS_LISTA = { 9, 7, 12 }`, `N_TEMAS 28`) y `tools/menu_musica.py` se ha quitado.

## Pausa y mezcla de audio (2026-11-08)

**Menu de pausa** (START en carrera; comun/menu.c `menu_pausa_*`, demo/main.c). Es el 4D5E40 / 4D7300 del PC con la
raiz 3; en el PS2, 0x247b40 con la raiz 16: filas 8, 7, 6, 10, 11, 14 de la tabla de filas 0x4187F8 (PC 0x5CBF10) =
VOLVER A LA PARTIDA, REINTENTAR, ABANDONAR, CONFIGURACION, CONTROLES, AUDIO (escena 0x2C00EE del banco ETC_CVT con su
XST en espanol, cuadros 9, 8, 7, 0, 1, 4). Barra de la fila 0x2C00ED (cuadro 10 normal, 12 elegida), lista en (-13,
-72) del centro y 16 px por fila (FrontendChoiceList 48D970), ventana en (135, 110) de 350 de ancho (0x375CF0..) y
alto filas * 16 + 104 (48CAC0). Los botones esperan 15 cuadros (692C9C = 15,05). A / START elige, B cierra (guarda la
licencia si cambio, 4D66D0 / 4D7DA0), arriba / abajo con vuelta. REINTENTAR y ABANDONAR preguntan "Estas seguro?"
(0x3A6; PS2 0x249d40: 0x3D1 solo en contrarreloj y 0x3A7 en red) con A ELEGIR / B ATRAS; REINTENTAR vuelve a la
salida con el mismo coche, cambio y musica; ABANDONAR vuelve al titulo del frontal. AUDIO (PS2 0x2492a0 = 4D78C0,
control 0x249530 = 4D89B0): filas 26 y 27 (0x2C00FB cuadros 12 y 13: VOLUMEN DE SONIDO y VOLUMEN DE MUSICA), medidor
0x2C00EF cuadro = valor (tabla 0x418988), de 0 a 10 con izquierda / derecha y sonido de cursor; se oye al momento; la
fila de la musica manda tambien su valor al volumen general de efectos (asi lo hace el PS2) hasta cambiar de fila o
salir; B (o A) lo guarda en la licencia (+D4 efectos, +D0 musica, 4D7AB0) y vuelve a la lista; se escribe en la VMU al
cerrar la pausa (tambien al reintentar o abandonar: SUPUESTO, el original guarda despues). El PS2 no tiene volumen de
voz (la tercera opcion del PC, +ED orden de la musica, no tiene fila). Los textos y botones con la fuente 14.
PROVISIONAL: CONFIGURACION y CONTROLES suenan bloqueadas (brillo, pantalla, velocimetro, sensibilidad, vibracion...
no estan); la ventana es un rectangulo claro con cabecera, franja de botones y pie oscuros con las medidas de 48C5F0 (sus
imagenes 0x3004x son de un XST sin localizar en el PS2), las flechas del medidor son "<" ">" de la fuente y los
iconos de los botones, la letra A / B. Verificado en E: abrir, mover, AUDIO (subir, bajar, cambiar de fila), volver,
confirmar y cancelar REINTENTAR (la carrera vuelve a la cuenta atras), ABANDONAR al frontal, guardado en la VMU.

**Mezcla** (antes: efectos x3 inventado, musica a 200, voces a 255). Del original:
- Opciones de la licencia: efectos +D4 y musica +D0, de 0 a 10, 10 en una licencia nueva (4471A0). Se aplican al
  leer la licencia (PS2 0x1a1a8c) y en AUDIO.
- Musica: 0x100210(v / 10): volumen del flujo = nivel * 0x3fff (comando 11 al IOP); con 10, el maximo. En el AICA,
  nivel * 255 (musica_nivel; el volumen del AICA en KallistiOS es lineal: TL = 16 log2(255 / v)).
- General de efectos: 0x11ae38(trunc(v / 10 * 144)) (comando 0xA001 al IOP, guardado tambien en el EE 0x530fb4);
  144 con la opcion a 10 (el IRX arranca con 128).
- Efectos SE (menus y voces de ORC.PAK / PERO.pak): SOUNDCD.IRX 0x2118 / 0x2380: volumen de la tabla SE (0x3fff) *
  general >> 7. Con 144 sale 0x47ff, que no cabe en los 15 bits con signo del volumen de la SPU2 (sceSdSetParam lo
  escribe tal cual): fase invertida, 0,875 del maximo (223 en el AICA). Con 9 (general 129) sale 0x407f: 0,99. Asi
  se reproduce (sfx.c vol_se); el efecto audible del desbordamiento es PROBABLE (el calculo esta CONFIRMADO).
- Motor, derrape y golpe (ICS): 0x11b600 del ELF: volumen de cada capa = 0x3fff * canal / 144 * curva / 127 *
  2^-((15 - aten) / 2) * factor de la definicion (floats en 0x352748: motor con y sin gas 1,75, derrape 2,0, golpe 1,0)
  * general / 144, hasta 0x3fff. El canal (0..127) es el de antes (motor 51,5 * D, derrape 151,2 * D, golpe 0,56 * 127).
  Respecto de lo de antes: motor 0,52 veces (-5,8 dB), derrape 0,59, golpe 0,29; musica +2,1 dB; voces -1,2 dB. El
  motor queda unos 8 dB por debajo de la musica de lo que estaba y la voz 4,6 dB mas por encima del motor.
- Peliculas: el audio sin atenuar (255, como la musica; el PC lo mezcla tal cual).
Medido en Flycast (SDL_AUDIODRIVER=disk, 44,1 kHz; una grabacion por fuente con `-DPRUEBA_MEZCLA` = bits 1 musica, 2
ICS, 4 SE callados; menu -> carrera con el Dino y gas a fondo 25 s, opciones a 10): todo -17,5 dBFS RMS; motor solo
-21,5 dBFS (maximo en 0,5 s -18,7); musica sola (SPLASH WAVE) -19,0 (-17,5); voz 0x82 de la salida -15,5 dBFS RMS
mientras suena (pico -1,9). Es decir: la musica 2,5 dB por encima del motor y la voz unos 3,5 dB por encima de la
musica. No hay medida del PS2 con que comparar (sin grabacion de referencia): los niveles salen del calculo de arriba.

## Auditoria de inventos (2026-11-08)

Recorrido de demo/main.c, comun/or2_fisica.c, comun/hud.c, comun/sfx.c y demo/camara.c buscando valores o logica de
juego sin origen. Certeza: CONFIRMADO (leido en el ELF de SLES-53998 o en datos del disco, con la logica del PC),
PROBABLE (logica del PC sin trazar en PS2, o deduccion directa), PROVISIONAL (sin evidencia suficiente: queda como
estaba y se dice lo que falta). Estado: CORREGIDO, SE QUEDA (ya era del original o es tecnico), PROVISIONAL.

| Valor de la demo (antes) | Origen real (PC / PS2) | Valor real | Certeza | Estado |
|---|---|---|---|---|
| `ACEL 1.35` multiplica el par motor | curvas de par de PARAM.DAT sin factor (or2_fisica = pc_driving) | 1,0 | CONFIRMADO | CORREGIDO: `ACEL` 1 por defecto, el define se queda como opcion (banco: 0-100 en 3,24 s, punta 293,7 km/h de velocimetro con el Testarossa) |
| "Posicion de carretera" = 2,5 m (`TR_POS`, puntuacion, IA del trafico, grua) | car+0x64 = seccion 5 de COLI0200 del poligono bajo el coche (calc_ground_coli_face) | m_pos de CURSO.DAT: 7,91-7,99 m segun la etapa | CONFIRMADO | CORREGIDO (`m_pos`, se actualiza en cada etapa) |
| Rumbo adelante (+D4E) a 40 m (`ruta_en(s + 40)`) | pc_common_control.cpp (calculo de +D4E): cuerda hasta el sitio 1 + 1 + 14 = 16 posiciones mas alla (r034_next_road) | 16 x m_pos (~127 m) | CONFIRMADO | CORREGIDO (`ADELANTE_POS`); el banco usa lo mismo (`ADELANTE=m` para comparar) |
| `GRUA_AVANCE` 25 m | pc_pl_wrecker_delayed: pc_advance_on_road_place(sitio, 10) (+8 = posicion) | 10 posiciones (~79 m) | CONFIRMADO | CORREGIDO |
| Ampliacion de tiempo al entrar en la union | 44FF20 (stage clear) con las banderas 0x10/0x100 de la posicion 0 de la etapa siguiente | principio de la etapa siguiente (pancarta CHECK POINT) | CONFIRMADO (datos de colision del disco + logica del PC; 450CC0/44DC70 iguales en PS2) | CORREGIDO, ver Checkpoint |
| `TIM_FILA 2` (variante 0) | 44DC70: variante 1 (OutRun) -> fila 0; 4C3DC0 da +1F = 1 a la fila "OutRun" | fila 0 | PROBABLE (menu del PC) | CORREGIDO |
| `TIM_PRESET 0` | 44C020 = PS2 0x176018 por [78024C] = PS2 [0x44350c]: preset 0 / 1 / 2 / 3 -> fila 0 / 1 / 5 / 6 de la tabla base | el del juego de cursos (1 en OUTRUN de la consola, ver Preset del curso; el del frontal en OUTRUN2SP) (`preset_curso`) | CONFIRMADO (0x176018) | CORREGIDO (2026-11-11 y 2026-11-13: antes 0, la fila de OutRun 2 SP, en la consola; ver OUTRUN2SP) |
| `TIM_DIF 0` | 0x1cdc98 (44DC70): 0 si 0x1b4878 ([0x59acac] = [780260], 1 en el juego de la consola: 0x237e28); si no 2 en la variante 4 o la licencia +E0 | 0 en la consola, licencia +E0 en OUTRUN2SP | CONFIRMADO | CORREGIDO (2026-11-11: antes la demo leia la licencia en los modos de la consola) |
| Sin el +60 ni k636904 | 44DC70 = PS2 0x176730: + k636904[clave] si 43F960 == 0 y 456D60 <= 1; + 60 si 48B1A0 != 1 y nivel > 0 | k636904 = {0,0,-60,0,-60,-90,0,-60,-90,-120,0,-60,-90,-120,-150} (PS2 0x35a938, KTE1); +60 con coche de serie | CONFIRMADO (formula y tabla); PROBABLE (coche de serie = 48B1A0 != 1) | CORREGIDO (Flycast: LAKE +3480 = 3420 + 0 + 60; DESE +3450 = 3480 - 90 + 60) |
| Bonus de etapa (4F2B20) al entrar en la union | 450CC0 lo da tras la ampliacion con 7D3948 (cuadros de la etapa, union incluida); 4F2DF0 en la meta (modo 0x13) | en el checkpoint y en la meta | CONFIRMADO | CORREGIDO |
| Cuenta atras: GO a los 240 cuadros de la escena | 8367BC = 180 al entrar en GAME; coche, trafico y tiempo quietos mientras > 60 (4A8100, 450AC0) | la escena de la salida empieza en START (4871A0), START pide GAME a los 120 pasos (43FA90, 780250 = 0x78) y el coche sale 120 despues: cuadro 240, el GO (0x9b) del guion | CONFIRMADO (codigo; el desfase de la etapa 64 es de 0 cuadros: 0x1c67d8 devuelve 1 sin red) | CORREGIDO (2026-11-10, ver Escena de la salida) |
| Camara de salida los 60 primeros cuadros | 485250 mientras 8367BC > 180 (durante START) | los 120 cuadros de START, con el ojo y la mira del movimiento de la escena (486EF0, op 14(0) en el 120) | CONFIRMADO | CORREGIDO (2026-11-10) |
| Trafico: 11 coches al salir en s0 + 130 + i 121 + azar 66 m | 4787E0: sin coches al salir (salvo puntos fijos 478B90); aparecen de uno en uno | ninguno al salir | CONFIRMADO (logica) | CORREGIDO |
| Trafico: reaparece a 550..1100 m si queda 150 m detras | 477F60 cierra con distancia < -2 o > 60 posiciones; 4787E0 abre a 50 posiciones (0x32; 35 en la union pasada la 120) cada max(3, registro +5) posiciones | -2 / +60 / +50 (35) posiciones, separacion del registro (12..20 en el curso) | CONFIRMADO | CORREGIDO (2026-11-08: separacion del registro de aparicion y caso de la union) |
| `NTRAF 11` (todas las plazas llenas) | tope 20 (0x14) x densidad 46CD60 (1 fuera de la red) x registro +4 de 4EFB90 (por tramo), mas los puntos fijos de 478B90 | registro +4: 0..12 en el curso (PALM 5/5/3/0/4, LAKE 12/7/5/2/3) | CONFIRMADO (tablas TAP1) | CORREGIDO (NTRAF 16 = plazas; cuantos abre lo dice el registro) |
| Velocidad del trafico 36-50 m/s al azar | 4F0030 (PS2 0x1f1a88): registro de 0x46 bytes del sitio del coche (+0xC i16 x K5B43A4 = 0,0046142, PS2 0x1dba94), tabla 84BF60 copiada por 4EFAF0 del descriptor +0x84 (PS2) | 100 km/h del modelo (27,7 m/s) en todo el curso | CONFIRMADO | CORREGIDO (el camino "Lead" de 472450 y 472320 portados: Velocidad del trafico de delante y Cambios de carril del trafico) |
| Trafico: frenar a 0,9 del de delante a < 10 m y carril libre 15/30 m al cambiar | el original tiene choques entre coches de trafico (4F9450) | ahora portados | CONFIRMADO | CORREGIDO (2026-11-08: quitados; 25 m al nacer = 43EB60, se queda) |
| Trafico: vuelta al centro del carril 0,02 m/cuadro | 6280EC = 0,02 es un tope (B08 no se acerca a 0 mas de eso); lo que lo devuelve es 473D10 (x 0,995 o 0,05 apartandose del jugador) y los cambios de carril | 473D10 portado (PS2 0x1e77e0) | CONFIRMADO | CORREGIDO (2026-11-08) |
| Caja de choque con el jugador: medio largo + 2,3 m, ancho + 1 m | cajas del descriptor 520950 de los dos modelos, esquinas contra bordes (4F9000) | CJM1/TRC1 | CONFIRMADO | CORREGIDO (2026-11-08) |
| Golpe al trafico: deriva segun la posicion lateral relativa, animacion reflejada segun el lado, fuerza del sonido por diferencia de velocidad | 476040: +B58 por +26C del jugador, 4A6EA0 con lado 0, efectos por tabla | — | CONFIRMADO | CORREGIDO (2026-11-08) |
| Muro a 1 m del limite de la ruta | CarBodyWallColiCheck 504CE0 (PS2 0x1987e8) con las cuatro esquinas de la carroceria contra la COLI y CbwColiWall 5041B0 (PS2 0x198e98) | ALPI s 3526-3948: la ruta 10-15 m fuera de la carretera y el coche metido en el muro hasta 4,5 m (la foto de la demo 35); ALAS s 508-840: el limite en el borde del asfalto, hasta 5 m pasado el muro; contra el muro en angulo el morro entraba hasta 1 m | CONFIRMADO | CORREGIDO (2026-11-11: Muros y suelo de la carroceria del original) |
| Altura del suelo: perfil a lo ancho de la ruta (mediana de las tiras de dibujo casi horizontales mas cercanas, tools/conv_tramo.py) | GetYPositionProg 43EB60 / GetRoadCond / CalcYPosSpl sobre la colision COLI0200 de la etapa (raiz 0) y de la union (raiz 1) | en TULI (SEG12, s 440-530) el perfil cogia la carretera de debajo del puente: hasta 5,7 m bajo la calzada (el coche pasaba por debajo) | CONFIRMADO | CORREGIDO (2026-11-09: Suelo del original) |
| Orientacion del coche: 4 muestras a 1,6/0,8 m | calc_ground_coli_face 519500 (PS2 0x1913e8): el centro y las cuatro ruedas con 43EB60 0x400, normal (c1 - c2) x (c0 - c3); car_sus_coli_check con el suelo de cada rueda | las muestras iban por el rumbo de la velocidad y la carroceria va cruzada: derrapando una rueda hasta 30 cm bajo la calzada (ALPI) | CONFIRMADO | CORREGIDO (2026-11-11: Muros y suelo de la carroceria del original) |
| Escala de puntos por avance (x10, x110) | PS2 0x28F984 / 4F2DF0 (K628244, K5C68E0) | igual | CONFIRMADO | SE QUEDA (ahora con m_pos) |
| Piloto automatico de la demo (DEMO del titulo) | el original pasa la pelicula de MV.PS2 (ahora tambien la demo: video.c) | — | invento propio (solo si faltan las peliculas, o pruebas) | SE QUEDA |
| META/AGOTADO: freno 0,6 y vuelta al titulo a los 600 cuadros | modo 19: la escena de la meta (4871A0 / 486730 / 486EF0, PasPlCar 475720, camara 4853D0) durante 0x4B0 cuadros | — | CONFIRMADO | CORREGIDO en la meta (2026-11-10: Escena de la meta); AGOTADO CORREGIDO (demo 40): el modo 20 (49A1E0 = PS2 0x1b9990) suspende los sucesos 8..0x1F (440A10 = PS2 0x19d318, bandera 0x10; 43FAB0 no llama a su control): el coche y el trafico quietos donde se acaba el tiempo |
| Aviso de bifurcacion a 450 m de la separacion | 4BB890 (PS2 0x1c50c0, desde NAVI_PUB 0x1bd534: variante 1, modo 16): 2B0000 "<- FACIL" con el avance car+0x102C entre 0,65 y 0,72 (nivel 0) / 0,70 y 0,75 (nivel 1), presets 0, 1 y 4 | — | CONFIRMADO | CORREGIDO (2026-11-11, ver Bifurcacion: el rotulo <- FACIL) |
| Marcas de las ruedas: 4 ruedas, media anchura del neumatico (filas 40/41 de la tabla de parametros), 0,2 m entre tramos, alfa 0,5, +0,05 m, v = longitud x 0,5, anillos 256/64/64, texturas 0x570000..2 (EFECTOS.DAT), clase por la superficie (0xC02 asfalto, 0x8004 tierra/arena, 0x18 hierba) | tire_mark_req 41FB50 / 41FDD0 / 4862C0 del PC; SLES-53998 0x10dcd0 / 0x10df38 / 0x1198f0, tabla 0x344d90, constantes 0x32b390..98 | igual | CONFIRMADO | CORREGIDO (2026-11-08) |
| Marcas en asfalto: las traseras mientras dura el derrape | el original usa la mascara 95AFA8 de PcTireSmokeReq 41C570 (deslizamiento +E0 y agarre +E4 de cada rueda contra 74FF5C) | — | PROVISIONAL (la fisica de la demo no da +E0) | SE QUEDA |
| Humo y polvo de las ruedas del jugador (64, PcTireSmokeReq por superficie), grava y hierba (256 + 256) de race_particles; tope de 2 pantallas de area por cuadro para el humo (limite de la PowerVR, ver Escena de la salida) | PcTireSmoke 41C420 / 41C570 / 41CB30 / 41D1E0 / 41D330 = SLES-53998 0x109eb0 / 0x10a0d8 / 0x10a708 / 0x10af70 / 0x10b178; grava 41DE20.. = 0x10bd18..; hierba 41E360.. = 0x10c2a8.. (texturas 0x2a / 0x2b / 0x2d de obj_course_obj_common) | igual salvo la prueba del derrape en asfalto | CONFIRMADO (PROVISIONAL: la prueba de deslizamiento +E0 / agarre +E4, car+58 = 1, el estilo del humo de la meta) | CORREGIDO (2026-11-13, ver Arena, grava y polvo) |
| Sonido: `ESCALA 3` de los efectos frente a la musica | balance 4279C0 (624238 / 624258) | — | PROVISIONAL (mezcla del AICA) | SE QUEDA |
| Sonido del derrape: deslizamiento / 30 grados | 4260E0 PlaySkid: maximo del deslizamiento de las 4 ruedas (+60/+64.. del cuerpo) | — | PROVISIONAL | SE QUEDA |
| Golpe: fuerza 0,4 + dv/25 y v/40, 20 cuadros entre golpes | 4264C0 PlayWall, 4F65D0 (una vez cada 60 cuadros) | — | PROVISIONAL | SE QUEDA |
| Salto de cuadros (paso de 16683,33 us, hasta 600 por cuadro) | 0x1078c8: pasos = campos de video, 59,94 por segundo a 50 y 60 Hz, sin tope hasta 600 (CONFIRMADO) | antes hasta 3 pasos (por debajo de 20 fps iba lento); TIEMPO TOTAL de resultados dividia por 60 | CONFIRMADO | CORREGIDO (2026-11-09) |
| camara.c: la carrera siguiente empieza con la vista elegida | — | — | PROVISIONAL | SE QUEDA |
| Voz y sonido de EXTEND TIME | 4B93E0 del PC (race_hud_navi.cpp timer_4b93e0) = PS2 0x1be78c: efecto 0x4d (0x84d) y voz 0x16c (0x96c) | — | CONFIRMADO | SE QUEDA (voces.c) |

## Suelo del original: colision COLI0200 (2026-11-09)

Antes la altura del coche (y del trafico y de la camara) salia de un perfil inventado en la ruta (tools/conv_tramo.py
recorrido_soo: mediana de la altura de las tiras de dibujo casi horizontales mas cercanas, ajusta_al_asfalto...). Donde
dos carreteras se cruzan a distinta altura cogia la de debajo: en TULI (SEG12) la ruta bajaba a -2,9 m mientras la
calzada sube por el puente hasta +5 m (el coche "pasaba por debajo de la carretera"). Ahora es la consulta del original:

- `tools/conv_coli.py` (tambien lo llama `conv_union.py`): COLnn.DAT por etapa (\Stage\ZONA\coli_CS_*), COLBK.DAT
  (\BK\coli_BK_PALM_bin, la bifurcacion entera) y COLBKkk.DAT (la meta de la etapa kk). Formato COL1: los valores del
  COLI0200 tal cual (floats), sin repetir vertices; la rejilla de 24 m recortada a las celdas con lista; las listas;
  tipo y banderas por poligono; los vecinos de ColiGetForward/Back/Left/RightPolygonNumber calculados en el conversor
  (son funcion fija de las tablas: posicion de carretera, tramos de 43D4D0 y la distancia de 0,5 m entre esquinas; en
  la raiz 1 no hay) y las normales de las esquinas solo de los poligonos que las usan (CalcHermiteTangent, cuando falta
  el vecino de delante o el de detras). 146-314 KB por etapa, 183 KB la bifurcacion, 25 KB las metas.
- `demo/coli.c`: port de pc_course_world.cpp (get_y_position_prog: raiz predicha primero, la otra despues; tabla
  EasyLct de 16; -0,1 y clase 1 sin suelo; get_y_position_spl_chk con sus cuatro intentos de +0,001), pc_course_query.cpp
  (get_road_cond: poligonos de la lista de la celda que contienen el punto en xz, plano 0x100 o Hermite 0x400, gana el
  de altura mas cercana a la de referencia salvo bandera 4), pc_course_spline.cpp (CalcYPosSpl, CalcHermite2,
  tangentes con vecinos o con normales, SolveHermite) y pc_collision.cpp (CalcCollisionArea, planos 43CC90). Tuning
  lateral / longitudinal 1,0 (PC 0x67D94C / 0x67D950). En float (el PC hace parte en x87).
- RAM: COLnn.DAT detras de su SEGnn.DAT en el hueco de la etapa; COL13 y la de la union en huecos solo de carrera
  (DREAMCAST.md, RAM y cargas durante la carrera).
- Marcos: raiz 0 la etapa cargada (la de salida en el origen, la siguiente en marco_sig), raiz 1 la union (marco_un),
  como 7D2DA0 / 7D3190; `curso_recentra` los mueve con todo lo demas.
- Uso: el coche del jugador con 0x400 + reintentos (el de calc_ground_coli_face), el trafico con 0x100 (43F2F0) y la
  camara con 0x100 (43EB60 del suelo de la camara), con la referencia en la altura del propio coche (`y_ok`; recien
  colocado, la de la ruta). La superficie de las ruedas y del centro (wheel+0x14 / +0x248) es la clase del poligono
  de la misma consulta. La ruta queda para el avance y los carriles; los muros y el suelo de las ruedas del jugador son
  los del original desde 2026-11-11 (ver Muros y suelo de la carroceria del original).
- Sin suelo bajo una muestra (fuera de la colision) esa muestra se queda a la altura del coche (el original dejaria
  caer el coche: la demo no tiene salto); sin ficheros COL (datos antiguos), el perfil de antes. `-DSIN_COLI` fuerza
  el perfil (para comparar) y `-DDIAG_SUELO` imprime la altura de la colision frente al perfil.
- Comprobado en host (todas las etapas y uniones): en el centro de cada poligono de calzada el plano y el Hermite
  difieren de media 0,5-1,3 cm (max 17 cm, curvatura); el perfil de la ruta difiere de la colision mas de 0,5 m solo
  en TULI (22 puntos, puente) e IMPE (5 puntos, 0,72 m). Flycast: ver Ultimo cambio verificado.

## Sombras y marcas (2026-11-08)

Peticion del probador: efectos de sombras. Lo que hace el original y como va en la Dreamcast:

- Sombra del coche del jugador (SLES-53998 0x1886e0, el dibujo del coche del jugador en carrera = 469600 del PC,
  llamado desde 0x188578 con la matriz car+B0; vm = 1): (1) los volumenes de sombra +08 y
  +10 de la disposicion (mallas cerradas en el marco del coche: la parte baja de la carroceria con los bultos de las
  ruedas hasta y = 0, y las piezas altas) con
  0x1138b8, ya en car+F0 (la matriz local: suspension y subida de 4A2910): luz en el marco del coche, silueta 0x113bf8
  (aristas de 0x24 bytes con las normales de sus dos caras, como
  4211F0 / 421680 / 421740 del PC) y cuadrilateros extruidos por la luz, dos pasadas en una plantilla emulada en el GS
  (estados 0x169ce0) y 0x114150 oscurece lo marcado leyendo el bufer de imagen como textura, antes de dibujar la
  carroceria (la carroceria no recibe su sombra); el PC hace lo mismo con la plantilla de D3D (422550 / 421A90, cuadro
  0x5F000000 de 4220F0). (2) La sombra plana +0C (mancha negra con alfa, 128x32) con 0x114630 (0x189040..0x18920c):
  matriz car+B0 del coche del jugador ([0x557108] + 0xB0, sin car+F0) y T(0, 0,05, 0) (0x3d4ccccd); 0x111998 pondria
  el alfa del material a 0,5 x f12 (f12 = 1, o 2 (0,5 - car+2DC) con car+2C8 > 0) solo si [0x4be37c] != 0, y nadie lo
  escribe (bss): alfa 1. Su material lleva CLAMP (+18 del estado del GS) en u y v en 7 coches, solo en v en 6 y en ninguno en 2
  (`tools/conv_coche.py sombra_clamp`). La seleccion (0x18a138 desde 0x189c3c / 0x189d20) dibuja la +0C igual y sin
  volumenes; 0x1893f8 (+0C en car+F0 subida 0,01 con el vector 0x3d3fe0) es otro dibujo que no es el de carrera. En
  el PC 49A650 es un RET. Luz: la 1 del entorno (4082B0(1, 0, 0) + 44): scn_env_sun
  lista 1, (0, 0, -1) girada por la inclinacion y el rumbo (44A430) en el marco de la etapa (CURSO.DAT, `sol`; PALM
  -40 / -60). CONFIRMADO (codigo de PS2 y PC). En la demo: volumen modificador OP_MOD "cheap shadow" (`sombra_dibuja`,
  bloque SVL1 de COCHEnn.DAT, con mod_coche x car+F0) y la +0C como pieza propia P_SOMBRA (`tools/conv_coche.py
  llamadas`, demo `PZ_SOMBRA`: mod_coche x T(0, 0,05, 0)). Diferencias: (a)
  oscurecimiento 1 - 0x5F / 255 (el del PC; el factor exacto de 0x114150 no se ha descifrado: PROBABLE); (b) extrusion
  25 m (6282D0 del PC; la PS2 la lleva hasta el borde de la vista), acortada si llegara al plano cercano; (c)
  DESVIACION: trozos convexos de cada parte (`rebanadas`: rebanadas entre los planos z de sus vertices, juntas mientras
  la envolvente no crezca mas de un 3 % o 5 litros; la PowerVR cuenta par / impar y el original suma: DREAMCAST.md); (d)
  recibe la sombra lo opaco y lo recortado (la escena y el trafico), no lo translucido. Prueba: `-DSOL_PRUEBA=grados`
  gira el sol para verla de lado.
  CORREGIDO (2026-11-12, demo 37: "arregla las sombras de los Ferrari"), comparado con el original en Flycast
  (PRUEBA_GUION, la escena de la salida y PRUEBA_FICHA 4 / 9 / 11; capturas en el scratchpad, som/): (1) la +0C iba
  en car+F0 subida 0,01 m (la de 0x1893f8): con el balanceo, el cabeceo y la subida de la suspension quedaba entre -2 y
  +4 cm sobre la calzada (medido) y casi no se veia: ningun contacto con el suelo en la salida; ahora como 0x114630.
  (2) Sin el CLAMP del material la textura se repetia: el borde (v = 0, alfa 0) se mezclaba con el centro (v = 1) y
  salia una raya oscura punteada en el suelo a 1,08 m del centro, a lo largo del coche. (3) La envolvente convexa de
  la carroceria entera unia por debajo los bultos de las ruedas: su cara de abajo quedaba en y = 0 (sin sombra bajo el
  coche entre las ruedas). (4) El cargador de SVL1 admitia 8 volumenes y el 288 GTO, el 512 BB y el Daytona (12, 18 y 9
  partes) no tenian sombra de volumen (medido en el 512 BB: 0 triangulos; ahora 324); ahora hasta 32 (6-20 trozos por
  coche). (5) Los volumenes iban sin car+F0 (la carroceria con su suspension y la sombra no se movian juntas).
- Trafico: su sombra plana (obj_vshadow, subida 0,1 m en el LOD 0) ya iba (Coches: trafico).
- Marcas de las ruedas: ver la tabla (CORREGIDO). El original tambien deja marcas de tierra y hierba (siempre, sin
  derrapar) y la demo ahora tambien (superficie de la COLI bajo cada rueda).
- Chispas (2026-11-11, HECHO): fuente 3 de las particulas, PtclSparkInit 41D6B0 / 41D810 / 41DC60 / 41DCC0 del PC =
  SLES-53998 0x10b588 / 0x10b698 / 0x10bb20 / 0x10bba0 (constantes 0x32b26c..0x32b294; generador 0x32b0d8). 500
  particulas. Las pide cada rueda cuando (a) el punto de la forma del coche de esa rueda ha entrado en la cara del
  muro (+670 = 82EE60, bit 31 - n) y la carroceria pisa un poligono de clase muro (+2A8 & 0x10F0020; CbwColiWall
  acepta ademas 0x4000 y 0x40, que no dan chispas): dispersion (0,02; 0,06; 0,02) m/cuadro; o (b) la esquina de la
  rueda ha entrado en la caja de otro coche (+514 / +700 de 4F9000; tabla 0x32b280 = 2, 1, 3, 0: ruedas del. izq.,
  del. der., tras. izq., tras. der.): (0,06; 0,02; 0,06). Contador de la rueda a 15 cuadros; con el vivo y el
  contacto moviendose 0,01 m o mas por cuadro, 30 chispas por cuadro del contacto anterior al actual, 0,7 m por
  encima (+-0,01), velocidad 0,9 x la del coche + 0,03 arriba + dispersion x azar (suma de seis trozos de 15 bits,
  de -3 a 3). Vida 10..59 cuadros; cada cuadro x y z x 0,99, rebote entero en el plano del suelo de la rueda 0,
  gravedad 0x3B315723. Texturas 0x570029 / 0x570028 (16x16, suma) alternadas con la paridad de la vida: van detras de
  las marcas en EFECTOS.DAT (conv_efectos.py; un EFECTOS.DAT antiguo sigue valiendo, sin chispas). Dibujo: sprites
  de punto (0x109010, tipo 5; PC 41A6F0): 16 x 0,6 x 0,5 x proyeccion / distancia, entre 2,5 y 32 puntos (PROBABLE:
  microprograma de la VU1 sin leer). En la demo (demo/main.c chispas_paso / chispas_dibuja / muro_original): +670 y
  +2A8 son los de CarBodyWallColiCheck con la forma del original (2026-11-11, Muros y suelo de la carroceria del
  original; sin la colision cargada, muro_chispas: la clase 0,25 m pasado el limite de la ruta y las ruedas en lugar de
  los puntos de la forma, PROVISIONAL). RAM: las 500 particulas (14 KB)
  ocupan lo que dejan el bufer del texto del centro y el de la mancha del humo, que ahora pasan por buf_arr.
  PENDIENTE: el destello 0x2C0001 (SPRANI 428320(0x2C0001, 5, 1) en la rueda, solo con cam+34A == 2).
  Verificado en Flycast (scratchpad chq/roce, chq/embiste, chq/vuelco1: `-DPRUEBA_AUTO -DPRUEBA_GUION -DPRUEBA_CHOQUE
  -DCHOQUE_ANG=.. -DCHOQUE_V=.. -DCHOQUE_GIRO=.. -DCHOQUE_CONG=.. -DDIAG_CHISPA`): muro de clase 0x20 y 0x10000 con
  chispas en la rueda del lado del muro, 0x4000 sin ellas; contra el autobus, en la esquina que entra.
- Humo, polvo, grava y hierba de race_particles: HECHO (2026-11-13, ver Arena, grava y polvo; la textura del
  humo y del polvo es la 0x2d: la 0x2f no la usan las particulas de las ruedas). PENDIENTE: sombras de la escena sobre el coche y oscurecimiento en tuneles (bit 0x200 de 4A25F0); reflejos de la
  carretera (capa 2: ver Suelos). \OBJ\obj_cs_shadow_pmt.sz (recurso 0x56) no esta en el disco de PS2 (ver Suelos).

## Suelos: estado del GS, mipmaps y capas (2026-11-09)

Queja del probador: "hay suelos que no se dibujan bien y efectos en el suelo que no estan". Recorrido en Flycast
(PRUEBA_AUTO: PALM, LAKE, ALPI, CAST, DESE, SNOW, METR y la union de PALM) y en los datos de SLES-53998. Causas:

- Tabla 7 del registro (CONFIRMADO, datos y microcodigo): 48 bytes por estado, la plantilla del paquete del GS que
  manda el VU1 (GIFtag con PRE y PRIM: 0x1C tira con textura, 0x5C con mezcla (ABE), 0x0C / 0x4C sin textura; en +08
  el registro ALPHA; +10 TEST; +18 CLAMP; +20 TEX1). La tabla 4 (36 bytes) da {material, 2 campos sin descifrar,
  textura y estado de la capa 1, textura y estado de la capa 2, un par sin descifrar}; el grupo de la malla del nodo
  (tabla 1: opacas / de mezcla) coincide siempre con ABE. Valores: TEST 0x5040B (prueba de alfa >= 0x40, z GEQUAL;
  el 90 % de lo opaco), 0x5040A (sin prueba de alfa), 0x5004B (mezcla, alfa >= 4), 0x51001 (ATST NEVER + FB_ONLY: sin
  escribir z, solo en capas 2); ALPHA 0x44 (Cs - Cd) As + Cd, 0x48 Cs As + Cd (aditiva), 0x58 Cs Ad + Cd y 0x89 Cd As
  (capas 2); ZTST GEQUAL en todos; TEX1 MXL = niveles de mip - 1 (los mismos que la cabecera de la textura).
  `modelo.estado_gs` / `lista_gs`.
- Lista de la PowerVR por el contenido de la textura (antes, `textura_dc`) en vez de por el estado: (a) texturas con
  alfa que el GS dibuja opacas (sin ABE ni prueba de alfa: el alfa es la mascara del brillo de la capa 2) iban a la
  transparente: las calzadas de METR (texturas 1 y 2 del registro 0, 9 000 vertices) y de CAPE (4 173) y trozos de LAKE
  y TULI se veian translucidas (el color de fondo a traves del asfalto: la calzada azulada de METR) y en 4444; (b) lo
  que el GS dibuja con prueba de alfa (arboles, vallas, hierba) iba a la transparente si su alfa no era casi binario:
  9 324 vertices en PALM (ordenados por la PowerVR por triangulo, sin escribir bien la z); (c) lo que el GS mezcla
  iba a la opaca o a la de recorte si el alfa de la textura era casi constante: ALPI 3 266 vertices, SNOW 748, METR 809,
  CAPE 356, INDU 254. Ahora la lista sale del estado (sin ABE ni prueba: opaca, en RGB565; prueba de alfa y algun
  texel por debajo de AREF: recorte, ARGB1555 con el umbral de AREF; ABE: transparente) y una textura usada en dos
  listas lleva una entrada por lista (3 en PALM). Vertices transparentes de cada etapa (datos): PALM 13 097 -> 3 811,
  METR 14 953 -> 4 695, CAPE 4 346 -> 529, TULI 3 079 -> 2 947; ALPI 1 986 -> 5 252 y SNOW 1 729 -> 2 477 (lo que
  mezcla el original).
- Mezcla aditiva (ALPHA 0x48): el brillo del mar de PALM (registro 5, textura 116 de 64x64, dos tiras sobre la bahia
  a 10 m bajo la calzada) y los destellos de METR iban con la mezcla normal (una capa de ruido gris sobre el mar). Ahora
  SRCALPHA / ONE, y el alfa de los vertices de lo que mezcla (0x6E de las tiras; el GS: alfa = textura x vertice, TFX
  MODULATE) con MODULATEALPHA: el brillo del mar se funde hacia sus bordes (alfa 0 en el borde exterior). PROBABLE: el
  color 0x6E va en cada vertice (STCYCL 4/1: posicion, color, normal, UV), como ya se usaba su RGB; el programa del
  VU1 que lo lee no se ha comprobado.
- Mipmaps (CONFIRMADO que el original los usa): las texturas del suelo traen sus niveles en el disco (cabecera de la textura +0x34) y
  su estado TEX1 los pide (MXL 1-4, MMIN 4 = bilineal en el nivel mas cercano); la demo no los tenia y el suelo lejano
  centelleaba (rayas y bordillos a mas de 100 m). Ahora las texturas cuadradas de mas de la mitad de sus vertices en
  el suelo (normal |y| > 0,7) y con MXL > 0 van con mipmaps de la PowerVR (bilineal + mipmap, el mismo filtro):
  niveles por promedio de 2x2 de la base (`modelo.niveles_mip`; los del disco son de la misma base), en VQ con un
  libro para todos los niveles (`vq.codifica_mip`); si la VQ con niveles no pasa el umbral de error, la textura va sin
  mipmaps (sin VQ costaria 2,7 veces mas). La PowerVR solo los admite en texturas cuadradas: las rectangulares del
  suelo (bordillos, arcenes de 128x64...) siguen sin ellos (DIFERENCIA). Por etapa 6-27 texturas.
- z: el GS compara con GEQUAL (lo coplanario que llega despues gana); la PowerVR con GREATER por defecto. Las
  cabeceras de la escena van ahora con GEQUAL. Comprobado en los datos: las manchas de sombra pintadas de la calzada
  (registro 0, mezcla) van 5-15 cm por encima del asfalto (mediana 10 cm), sin z-fighting; tiras opacas coplanarias
  con otra de distinta textura hay pocas (LAKE registro 8, ~70 triangulos): con GEQUAL gana la ultima enviada, que en
  la demo es la del orden por textura y no la del original (DIFERENCIA menor).
- Lo que no es causa: culling (el GS no recorta caras; la demo tampoco), geometria de la calzada (tiras ADC y nodos,
  ver Graficos), orden de las listas (opaco, recorte, transparente, como las pasadas del original 405890).

Efectos del suelo del original y su estado:

- Sombras pintadas: en el PC el recurso 0x56 `\OBJ\obj_cs_shadow_pmt.sz` (XMT_CS_SHADOW 86); el nombre esta en el
  ELF de PS2 pero no en el disco (OBJ.PS2 tiene 11 entradas, todas con nombre; el hash del nombre no esta en ningun
  paquete: CONFIRMADO). En la PS2 las sombras pintadas son tiras de mezcla de las propias etapas (PALM registro 0,
  textura 0 de 64x8; LAKE textura 30), que ya se dibujaban y ahora van con su alfa de vertice.
- Agua: el mar de PALM es opaco con su brillo aditivo (arriba); el lago de LAKE son mallas opacas del registro 0
  (textura 55, a 95-98 m bajo la salida), sin capa 2 ni mezcla, y debajo el agua del descriptor (+0x5C..+0x70: el
  plano que solo escribe el alfa y el reflejo; ver Agua: el reflejo del original, 2026-11-11). Sin animacion de textura en sus nodos
  (sin la bandera 0x20 de 404D10).
- Capas 2 (PENDIENTE, sin portar): la mayoria de las tiras de la escena llevan una segunda capa (tabla 4 +14/+18) y su
  programa del VU1 la dibuja en una segunda pasada: guarda dos ST por vertice y manda el mismo bufer dos veces
  (XGKICK) con los REGS del GIFtag cambiados (0x3F80412F: la ST del hueco 2; 0x3F8041F2: la del hueco 1) y entre medias
  un paquete de estado que el EE deja en las direcciones 0x31 / 0x3A del VU (no localizado). Lo leido: (1) clave 0x10200
  (programa 0x31AAE0): ST2 = matriz de textura (VU 15-18) x vector reflejado 2 N (N.V) - V: mapa de entorno, con la
  textura 12 de PALM (256x256 negra, alfa ~1) o la 53 de LAKE (512x512, alfa con siluetas) y ALPHA 0x89 (Cd As:
  oscurece lo de detras); el alfa del vertice sale de 1 - |N.V| (fresnel) con una potencia aproximada (ITOF/FTOI de los
  bits del float). (2) Calzada (clave 0x5610200, programa 0x31B1B0): color = ambiente + difuso x max(N.L, 0) y alfa =
  especular (R.V)^n aproximado, R la luz reflejada; segunda pasada con la textura 13 de PALM / 54 de LAKE (mancha de
  64x64), ALPHA 0x58 (Cs Ad + Cd: suma por el alfa de destino que dejo la primera pasada = mascara del asfalto x
  especular) y prueba de alfa >= 0x60; las dos ST son la misma UV. (3) Edificios con mapa esferico (PALM texturas 95/96,
  clave 0x15D600) y focos proyectados por la posicion (clave 0x158200, programa 0x317CF0). No se portan: el paquete de
  estado de la segunda pasada no esta localizado (la textura y la mezcla exactas son PROBABLES), la PowerVR a 16 bits no
  tiene alfa de destino (Cs Ad), y una segunda pasada de la calzada duplica sus vertices en el TA (ver DREAMCAST.md).
- Luces en el suelo de METR: en los datos no hay manchas de luz en la calzada; lo aditivo de METR son destellos y haces
  (registros 4, 5 y 10, textura 131 de 16x16, pilas de cuadrilateros verticales). Van ahora con la mezcla aditiva.

Codigo: `tools/modelo.py` (estado_gs, lista_gs, _variante, ajusta_texturas, niveles_mip, datos_mip; formato de textura
bits 0x200 mipmaps, 0x400 aditiva, 0x800 escena con z GEQUAL), `tools/vq.py` (codifica_mip, elige_mip),
`demo/main.c` (compila_cab). Verificacion y coste: ver Ultimo cambio verificado y DREAMCAST.md (Suelos).

## Fisica: auditoria (2026-11-08)

Recorrido de comun/or2_fisica.c funcion a funcion contra la decompilacion de PC (src/driving, src/platform:
race_input, vehicle_constructor, vehicle_body_init) y el ELF de SLES-53998 (capstone; las funciones de PS2 del manejo
estan en 0x17e000..0x18e000 y tienen la misma estructura que las del PC: se comprobaron las de abajo marcadas "PS2").
La decompilacion no trae trazas del juego: sus pruebas (tests/test_driving*.cpp, test_wheel_dynamics.cpp...) son de su
propio codigo con entradas sinteticas, asi que el banco compara magnitudes. Unidades: m/s y rad internos, +1C4 en
m/cuadro, +1F4 = +1C4 x 216,72 (km/h del modelo), angulos de 16 bits (65536 = 2 pi), paso 1/60,2 s.

| Funcion (or2_fisica.c) | Original PC / PS2 | Estado | Evidencia |
|---|---|---|---|
| orden del paso (tick) | GamePlCarCtrl 4A8330: OperationInput, CheckSlipStream, CheckShiftWarning, CheckWanderer, DrivingControl 502C90, CommonPlCar 4A8100 | igual (CalcCrushMotion va al principio del paso en vez de tras CopyCarWork: mismo numero de cuadros) | pc_common_control.cpp 2320, pc_driving_control.cpp |
| or2_carga_param, umbrales | vehicle_gear_thresholds_487570, 4A6840 / tabla PS2 0x3dbbb0 | igual | conv_param.py |
| or2_inicia / or2_coloca | vehicle_construct_4a5830 | CORREGIDO: sale en 1a (antes en punto muerto: el primer paso hacia un cambio 0->1 con el embrague suelto), rpm de ralenti en +48/+20C, +E98 = FLT_MAX | vehicle_constructor.cpp 85 |
| or2_volante (nuevo) | pc_input_analog_453860 + 4537C0 / PS2 0x1b3fb8 + 0x1b42e0, filas 0x3f8800 | NUEVO: eje x 2^-15, zona muerta 0,25 (PS2; el PC 0,2) reescalada, rampa de la cruceta (0,10/0,117 por cuadro), x 127 y filtro por cuadro (alejandose: +-17 o /4 desde el centro; volviendo: /3 hasta +-20); opcion 4 (4471A0 del PC; PROBABLE en PS2). Antes: joyx/110, zona 0,08, cruceta a +-1 de golpe | disassembly 0x1b40ac..0x1b42a4 |
| entradas (OperationInput) | 49FAD0 / PS2 0x17ef10 | CORREGIDO: freno +38 = 255 mientras +2F8 > 0 (recuperacion); +202 = volumen << 8. Pedales 0..255 sin curva (PS2: presion del boton tal cual) | igual en las dos |
| steering_operation, toe_angle, tire_direction_angles | pc_steering.cpp | igual | |
| tire_velocity | pc_tire_geometry.cpp | simplificado (2D) | |
| cambio_auto | auto_transmission | igual | |
| cambio_manual | manual_transmission | CORREGIDO: sin el 0->1 inventado; +E84 hace de automatico; reducir con el motor al corte, sin freno y con gas = sin golpe de gas, +D36 = 80, +D94 = 180, +D98/+D9C = 60 | pc_transmission.cpp |
| aviso_cambio (nuevo) | CheckShiftWarning 4A50F0 | NUEVO: 480 cuadros pidiendo subir -> +E84 (automatico); +E80 a los 180 | pc_common_control.cpp 2201 |
| acelerador_op | accel_operation | igual | |
| reparte_freno | distribute_brake_torque | igual | |
| embrague_auto | auto_clutch_control | CORREGIDO: estado 4 con la media de +E4 de las traseras (< 1: -4); +2F8 > 0: suelto | pc_driving.cpp |
| par_motor, friccion_motor | engine_torque / engine_friction / PS2 0x18cc00 | CORREGIDO: la curva 1 (manual, PAR2, ganancia 4,5) con +13 y sin +E84 (tabla de punteros PS2 0x3db118); en la ultima marcha de los preparados (modelo >= 15) el limitador baja a la mitad de paso (PS2 0x18ce5c) | 0x18ccac lbu 0x13 / lw 0xe84 |
| mu por rueda | get_road_mu + assMuControl | igual | |
| agarre (tire_grip) | 5000xx | CORREGIDO: 0 durante el rebote (+283). Falta: bit 0x100000 de +4 (carga de referencia) e induce_spin (+304): nadie los pone en la demo | pc_driving.cpp tire_grip |
| rigidez (cornering_power) | 500EA0 / PS2 0x18bab8 | CORREGIDO: x 0,01 por debajo de 20 km/h del modelo con +D90 > 0 (60 cuadros tras el rebote y tras el choque) | |
| fuerza lateral, delantera, trasera, circulo, giros, rodadura | 5019C0, 500FF0, 501240, 5026F0, 501190, 501680, 501C90 | igual (+E4 guardado; +F0, +E0 slip_ratio y el angulo de giro +30 de la rueda no se usan en la fisica) | pc_wheel_dynamics.cpp |
| resistencia_tierra | running_resistance 502120 | igual | |
| fuerzas al cuerpo | copy_physical_work 502270 + make_force_work_tire 4A0000 | CORREGIDO: sin fuerza de las ruedas en el aire ni de las delanteras con una trasera en el aire; preparados en la ultima marcha con limitador < 0,975: sin empuje por marcha; pares de cabeceo y balanceo de +7C/+88 en el punto de contacto | pc_chassis.cpp |
| aire, frenada tras golpe | calc_game_resist, force_work_49fed0 | igual | |
| gravedad, apriete, pendiente | make_force_work (gravedad, assPressDown 5 g, assCancelInclineResistance) | igual en lo que importa: la pendiente no frena ni acelera (se anula); el apriete esta en la suspension | pc_chassis.cpp |
| suspension (nueva) | ColiCar 519830 (car_sus_coli_check, car_sus_bump_push), CalcSuspensionForce 4A1B70, CalcTireLoad 4A1A90, assSpecificAmountOfTireLoad, assDiagonalTireLoad | NUEVO (antes cargas estaticas): muelle 0x688 x 6 del apriete, topes 0x4C0/0x558, estabilizadora 0x720, amortiguador (constantes 0x5c36b4..0x5c36c0); la carga es la fuerza / 6, normalizada a la total y sin diferencia entre diagonales. El cuerpo se reduce a altura, cabeceo y balanceo (angulos pequenos, inercias masa x 0x4c / 0x0e4) sobre el plano del suelo del coche (centro y normal de calc_ground_coli_face) con el suelo de cada rueda en car_sus_coli_check (2026-11-11); por el / 6 la transferencia es la sexta parte de la fisica real (frenando a 3,3 g: +40 kgf por rueda delantera) | pc_suspension.cpp, pc_collision.cpp |
| ActionForce2 | 517410 | simplificado: 2D + los 3 grados de la suspension; MaximumVelocityCheck (166,7 m/s, 60,2 rad/s) igual, sin la inclinacion maxima 0x900 | |
| ctrl_agarre_trasero | RearGripCtrl 4A2FA0 | CORREGIDO: +D94/+D98/+D9C bajan en manual; contacto con el muro (work+244 bit 3): agarre >= 0,75 + 0,25 abs(sin +4E) | pc_common_control.cpp 75 |
| curva_agarre | 4A3260 | CORREGIDO: t = +D94 x 0,1107 (antes 0 fijo) | |
| ctrl_deriva | SlipAngleCtrl 4A3310 | CORREGIDO: d44 = d46 = 0 durante el rebote. La ayuda de 7F94C0 es de la partida en red (0 fuera de linea) | |
| CorneringCtrl | 4A3950 | CORREGIDO: la ley de senos del original (antes la forma tangente con tope 1,4 rad) | |
| CalcPlBody_2nd y limitador | 4A7EC0, 4A3C60 | igual (+DC4 = 1) | |
| ass_compulsive_move | 5184B0 / PS2 0x187d70 | ERROR CORREGIDO: los coeficientes 0x2140 / 0x218c iban cambiados (cross.y x sin(d46) < 0 -> 0x2140; PS2 0x187db4 c.olt.s / lwc1 0x2140). Cambia solo las columnas 6, 7, 17 y 18 (0,1 frente a 0). Rumbos y bordes del llamador | |
| ayuda_tierra / asiste_tierra (nuevas) | CheckWanderer 4A5260 + AssistWanderer 4A5470 | NUEVO: con las cuatro ruedas en tierra (0x801C; 0x809C en el nivel 0) y al volver al asfalto, gira la velocidad y la carroceria hacia la carretera | pc_common_control.cpp 2427, 419 |
| rebote | RebPlBody 4A62E0 | CORREGIDO: se quita el par de guinada; +283 baja despues de SlipAngleCtrl; +D90 = 60 al acabar | |
| grua | WrecPlBody 4A0C70 | igual | |
| CopyCarWork | 4A1140 | igual + alturas de rueda +134 (rueda_y) para el dibujo | |
| choques | CalcCrushMotion 4A2400 | igual + +D90 = 60 al acabar | pc_crash.cpp 149 |
| or2_rebufo | CheckSlipStream 4A4D20 | simplificado: los pedales son los del paso anterior (un cuadro) | |
| redondeo | FPU del EE (redondea hacia cero, sin IEEE) | CORREGIDO: or2_paso pone FPSCR.RM = 1 en la SH4 y lo devuelve al acabar; diferencia medida en el banco (REDONDEO=cero): < 0,3 % | |
| falta | ControlTimeupBraking 49FB70 (lo hace main.c con freno 0,6), HandicapControl 458E40 (1 para el jugador), saltos (el juego de la demo no da alturas: en_suelo siempre 1) | FALTA | |

Banco (tools/c/prueba_fisica.c, PARAM.DAT de SLES-53998; antes = sin esta auditoria):

| Coche (modelo) | 0-100 km/h | 0-200 km/h | punta (velocimetro) | frenada punta-100 | viraje a 200, volante a fondo | deriva a 1,99 s en la prueba de derrape |
|---|---|---|---|---|---|---|
| m6 Testarossa | 3,24 -> 3,21 s | 8,14 -> 8,11 s | 293,7 -> 293,7 | 2,66 -> 2,67 s | 6,50 -> 6,37 g | 53,8 -> 56,4 gr |
| m0 | 4,30 -> 4,29 s | 10,00 -> 9,97 s | 293,7 -> 293,7 | 2,66 -> 2,66 s | 6,50 -> 6,34 g | 53,8 -> 56,5 gr |
| m3 | 4,12 -> 4,10 s | 9,80 -> 9,77 s | 300,7 -> 300,6 | 2,77 -> 2,77 s | 6,50 -> 6,34 g | 53,8 -> 56,6 gr |
| m16 (preparado) | 3,37 -> 3,34 s | 8,12 -> 8,09 s | 300,8 -> 300,7 | 2,79 -> 2,79 s | 7,00 -> 7,00 g | 58,5 -> 59,6 gr |
| m20 (preparado) | 2,57 -> 2,54 s | 5,63 -> 5,60 s | 310,2 -> 309,9 | 2,87 -> 2,89 s | 6,90 -> 6,74 g | 52,7 -> 54,3 gr |

El 0-100 es 0,03 s mas corto por salir en 1a sin el corte de embrague; el viraje a fondo baja un poco porque la carga
pasa a la rueda exterior (el agarre es lineal con la carga: mu_max = mu_min en las 19 columnas) y el circulo de friccion
suavizado no lo es. Suspension en el banco: reposo 271/374 kgf por rueda (las estaticas), CG a 0,317 m; frenando 311/334
y -0,3 gr de cabeceo; viraje a 150 km/h 347/195 delante y 0,7 gr de balanceo. Manual (PAR2): mismos tiempos subiendo a
8400 rpm; reducir de 4a a 3a a 200 km/h arranca el derrape (+D36 = 80); 480 cuadros sin cambiar pasan a automatico.
Volante (opcion 4): eje 32 -> 0 (zona muerta), 64 -> 42, 127 -> 125; de 0 a 127 en 8 cuadros, de 127 a 0 en mas de 10.
Flycast (-DPRUEBA_AUTO -DPRUEBA_FISICA, resumen cada segundo): hasta el checkpoint de LAKE sin incidencias, cargas
entre 166 y 479 kgf, cabeceo -0,15..0,03 gr, balanceo +-0,9 gr.

## Choque fuerte contra el muro (2026-11-11)

Auditoria de CbwColiWall / CwCrushStatus / StartCrash contra SLES-53998 (capstone): 0x199430 (CbwColiWall),
0x199b90 (CwCrushStatus), 0x199d60 (cw_rebound_status), 0x19a1d8 (calc_friction_status), 0x182d38 (StartCrash).
Ya estaban: estados 1 (vuelta de campana), 2 y 5, sus poses (+2D8..+2EC), la grua (PlWrecker) y las vistas de choque.

| Asunto | Original | Demo antes | Demo ahora |
|---|---|---|---|
| angulo del choque fuerte | 0x1994c4: 45 grados si 44C940 (0x177270, nivel de la etapa) es 0; si no 25 (curva que viene <= 20 grados) o 18 | siempre 45: el vuelco casi no salia | igual (nivel = profundidad en el arbol, PROBABLE: ver ASSETS, el +8 del registro vale 0 en el fichero) |
| limites de velocidad (+1F4, km/h del modelo) | 0x199c04: punta - 70 / punta - 20; dentro de un tramo de atrapamiento de la etapa (tabla 0x3dee58 por descriptor de tramo, 16 parejas de posiciones +64, fuera de la union) 180 / 350 | solo punta - 70 / - 20 | igual (bloque ATR1 de EFECTOS.DAT por SEGnn; or2_coche_t.atrapa) |
| desvio del estado 1 al 5 | 0x182d6c: tabla 0x3d2f38 (tramos donde no hay sitio para la vuelta de campana) | no | igual |
| limites sin clase de muro | +2A8 & 0x10F0020 = 0 (clases 0x4000 y 0x40): solo roce, sin rebote ni choque fuerte | rebote y choque en cualquier limite | igual: +2A8 es el OR de las clases bajo las cuatro esquinas de la forma y sin suelo bajo las cuatro actua PlWrecker(1) (2026-11-11, Muros y suelo de la carroceria del original; sin la colision cargada, la sonda de antes a 0,25 m del limite de la ruta) |
| frenada del roce (+282) | 0x19a200: kmh >> 7; >> 6 en tramo de atrapamiento del choque o del roce (0x3dfed8), >> 3 en los dos; un escalon menos sin clase de muro (>> 8 sin tramos) | kmh >> 7 | igual |
| sonido | rebote: SE 0x4b cada 60 cuadros (+D23); choque fuerte: tabla 0x3dedf8 por estado (0x43 los estados 1 y 5, 0x42 el 2) y despues el SE por material (0x19a4c0, tabla 0x3dee18) | golpe ICS con fuerza inventada | 0x4b y 0x42 (SFX.DAT, conv_sonido.py MURO_SE); PENDIENTE el 0x43 (pitch 0 en la tabla SE de SOUNDCD.IRX: no se sabe como lo toca) y el SE por material |

Sin hacer: handicap +DB4 y asistencia +DF8 (0 en la demo), temporizador de colision +DEC (0x1859c8), efectos
49A650 (vacio en el PC), reacciones nuevas del piloto. Pruebas en Flycast (`-DPRUEBA_AUTO -DPRUEBA_GUION
-DPRUEBA_CHOQUE -DDIAG_CHISPA` con `PRUEBA_NODO`, `CHOQUE_POS`, `CHOQUE_ANG`, `CHOQUE_V`, `CHOQUE_GIRO`, `CHOQUE_CONG`):
nivel 1 a 34 grados y 462 / 468 km/h del modelo: estado 1 (2,5 s, grua 151 cuadros, +2DC hasta 5,1 m, giros hasta
-2 pi); PALM posicion 208 (tramo de desvio) a 50 grados: estado 5 en vez de 1; etapa SEG10 a 249 km/h del modelo:
fuera de tramo (posicion 302) solo rebote, dentro (542) estado 5.

## Muros y suelo de la carroceria del original (2026-11-11)

Queja de la demo 35 (consola PAL, fotos): "quitamiedos que se traspasan" (el coche a 225 km/h medio metido en la roca
de una carretera de montana con quitamiedos; en otra, pasado el muro, dentro de la roca junto a la nieve, con la
camara dentro) y "ruedas que se hunden bajo la carretera".

Causas (medidas en el host con tools de scratchpad sobre SEGnn.DAT / COLnn.DAT de todas las etapas y en Flycast con
`-DPRUEBA_MURO`):
- La demo no tenia los muros del original: el coche era un punto que no podia pasar de 1 m del limite de la ruta
  (tools/conv_tramo.py superficies: el primer muro o vacio de la colision por la perpendicular, pero nunca por dentro
  del borde del asfalto dibujado, y medido desde el centro de la ruta). Donde la ruta se va de la carretera (el centro
  y los bordes salen de los carriles SOO y del asfalto dibujado: ALPI s 2544-3964 hasta 15 m, INDU hasta 22 m, RUIN
  31 m, METR 19 m, IMPE 37 m) los limites caen en la franja de muro: en ALPI s 3680-3932 los dos limites quedaban a
  3 m de un centro que esta encima del muro y el coche iba por dentro de la roca (la segunda foto: ALPI es la de la
  nieve y los quitamiedos). Donde el asfalto dibujado pasa del muro de la colision el limite era el borde del asfalto
  (ALAS s 508-840: 7,5 m con el muro a 2,4-7 m). Puntos de la ruta con el limite mas de 0,5 m pasado el primer muro:
  ALPI 238, INDU 311, METR 479, RUIN 66, IMPE 103, TULI 123, GHOS 35, LAKE 42, ALAS 100, MAYA 333, BEAC 18, YOSE 12,
  SANF 5 (y en las calzadas partidas LASV, FLOR y NEWY la mediana, que si cubrian las franjas). Ademas, con un punto
  a 1 m del limite, contra el muro en angulo el morro entraba: 0,4 m a 10 grados, 0,8 a 20, 1,1 a 30. Esto no viene
  de af3448d, 83a83d3 ni 27e4ea3 (solo cambian la respuesta en el limite) ni de 02cce07 (ruta_punto da lo mismo que
  el codigo de antes en ruta_en, altura, limites y carril_x).
- El suelo del coche: la altura y la inclinacion salian de 4 muestras a +-1,6 m por el rumbo de la velocidad y +-0,8
  m por el x de la ruta, y las ruedas iban sobre ese plano; derrapando la carroceria va cruzada (hasta 80 grados) y
  el plano no es el de las ruedas: en ALPI una rueda hasta 0,30 m bajo la calzada.

Ahora (demo/coli.c coli_muro, demo/main.c muro_original / suelo_jugador, comun/or2_fisica.c):
- Forma de la carroceria: vehicle_body_parameters 4A6840 (PS2 0x182298: work+674 / +678 = -/+|P 0x1ab8|, +67C..+688
  = P 0x1b04, 0x1b50, 0x1b9c, 0x1be8) y vehicle_body_corners 4A1FC0 (PS2 0x182718): cuatro puntos (+674 / +678,
  +67C, +684 / +688) menos +220 / +224 / +228 (0, P 0x260, L / (1 + d) - L / 2 + 0,09; PS2 0x1822f8 con la media de
  las z de las ruedas de 0x3d2ec8). En las 19 columnas de PARAM.DAT: +-0,95 m, 0,078 m del suelo, 2,55 m delante
  y 1,93 m detras del CG. fpar.forma.
- CarBodyWallColiCheck 504CE0 (PS2 0x1987e8, igual que el PC): cada punto con la matriz del cuerpo (la del suelo del
  coche con el cabeceo y el balanceo de la suspension y el CG a fis.alt) contra la COLI con GetYPositionProg modo 0
  (el poligono que lo contiene en xz y el de altura mas cercana; actualiza EasyLct como el original); event+2A8 = OR
  de las clases; si los cuatro estan sin suelo, PlWrecker(1); si no estan todos en la clase 2 y alguno pisa un muro
  (0x010F4060), CbwColiWall; si no, work+670 = 0.
- CbwColiWall 5041B0 (PS2 0x198e98), la geometria: por cada punto sobre un muro, CopColiPoint 43D1D0 (los vertices en
  el marco de su raiz), las caras especiales de los triangulos (p3 = p2 en xz) por el codigo de superficie (seccion 4
  de la COLI, & 15: PS2 0x556e40[tipo], PC 780170) y la tabla de modos PS2 0x3dede8 (= PC 5E0DE0: 00 01 01 C2 00 01
  01 C2 01 42 02 03 01 42 02 03), o CalcColiWallFace 502E20 con el centro del coche (work+40 / +48, que ya va movido
  por las caras anteriores); PushOutPosMatObsolete 502F80 (normal, cara a 0,01 y borde: el coche sale lo que haya
  entrado el punto mas metido; las caras se suman, la matriz no se mueve) y ColiSetObsolete 5030F0 con la primera
  (punto de contacto en el marco del cuerpo: event+281; sin el, CbwColiWall acaba sin respuesta). La normal de
  work+64C = (n.z, n.y, -n.x) x work+628. Lo empujado se pasa a s y x de la ruta. La respuesta (calc_friction_status
  / cw_rebound_status / CwCrushStatus) sigue en or2_choque_muro con esa normal, max_distance, +2A8 y +670 (el lado
  del choque fuerte es reverse = +670 & 0xA0000000, como CwCrushStatus). Las chispas (41D810) piden la rueda n con
  +670 bit 31 - n y +2A8 & 0x10F0020.
- PlWrecker(1) (504900 fase 1, PS2 0x19a810): +1C4 = 0 (estados 2 y 5: x 0,67, tope 0x3F3CFFE8 = 0,738 m/cuadro), el
  rumbo y la velocidad por la carretera, sin giro (or2_grua_inmediata). El sitio: el centro de la fila de poligonos de
  la posicion de carretera (GetCsRoadInfoByCsLen 43E3B0), que es el centro de la ruta (ver abajo); la grua del choque
  fuerte, igual 10 posiciones mas alla (antes el carril del asfalto mas cercano). SIN PORTAR: el cambio de signo de
  +2C / +30 y ReconstructPostureMatrixAndFaceWork. Con la colision cargada casi no se da: por la perpendicular a la
  ruta lo primero que no se pisa es un muro salvo en 0-7 puntos de los extremos de las etapas y en los huecos de SEG49
  (abajo).
- calc_ground_coli_face 519500 (PS2 0x1913e8): el suelo bajo el centro del cuerpo (work+40) y bajo el punto de cada
  rueda (x de +04, y = +28 - el radio de su eje, z de +0C, con la matriz del cuerpo) con GetYPositionSplChk; el coche
  va en el suelo del centro con la normal (c1 - c2) x (c0 - c3) (work+640 / +628) y la carroceria por el rumbo del
  cuerpo; la clase de cada consulta es la superficie de cada rueda y del centro (wheel+14, event+248; antes salian de
  otra consulta en el sitio aproximado de cada rueda). car_sus_coli_check compara cada rueda con su suelo (wheel+3C):
  or2_entrada_t.suelo_rueda, lo que cada una queda sobre el plano del coche; car_sus_bump_push contra el plano.
  DESVIACION: sin suelo bajo una rueda el original da -0,1 (la rueda cae); en la demo esa rueda se queda en el suelo
  del centro (la demo no deja caer el coche). PROBABLE: la fisica de la demo es 2D con la altura, el cabeceo y el
  balanceo de la suspension; la matriz del cuerpo se arma con ellos sobre el plano del suelo (no es el cuerpo 3D de
  ActionForce2) y las consultas van con el sitio del paso anterior (el original las hace tras integrar).
- Sin la colision cargada (ficheros antiguos) todo sigue como antes (limites de la ruta, muro_interior, superficie,
  4 muestras). `-DMURO_ANTES` lo fuerza para comparar.
- COL1: los bits 4-7 de las banderas llevan el codigo de superficie (tools/conv_coli.py; palabra 11 de la cabecera =
  1). Regenerados los 74 COL*.DAT (iguales salvo esos bits y la palabra; ficheros sin ella: material 0, sin caras
  especiales). Solo 5 triangulos de muro de PALM (y 3 en las SEG01 / SEG02 de las misiones) son de cara especial.

- La ruta (tools/conv_tramo.py recorrido_filas, del parche sin terminar del barrido de la camara, revisado): el centro
  de la carretera del original, el de la fila de poligonos principales de cada posicion de carretera
  (GetCsRoadInfoByCsLen 43E3B0: v0 y v3 del primero y v1 y v2 del ultimo), cada 4 m; los bordes del asfalto dibujado
  sin recentrar y el limite el de la colision aunque el asfalto siga (solo_colision; en las uniones como antes). Antes
  el centro salia de los carriles SOO y del asfalto dibujado y otro asfalto cercano lo sacaba de la carretera; con el
  coche libre por la carretera lo que la demo toma de la ruta (rumbo +D4C, bordes +264 / +268 y desvio +26C de la
  fisica, que el original saca de esa fila en GetRoadOfs 4A4010; el sitio de la grua) quedaba fuera de ella (ver INDU
  y RUIN en los resultados). Regeneradas las 62 SEGnn.DAT y SEGS.PAK (con el agua de 6c8658f). La grua del choque y
  PlWrecker(1) dejan el coche en x = 0 de la ruta, el centro de esa fila (antes el carril del asfalto mas cercano).
- El trafico: el original lo asienta cada cuadro (PS2 0x1eb0c0, tras su muro 0x198a20) con el suelo 0x100 bajo su
  centro y, en la etapa (+5C = 0) a menos de 100 m de la camara (car+300 >= -100: 0xC2C80000) o en los pesados (+320 0
  o 1), con 46E4B0 (PS2 0x1de4b8): tres puntos del descriptor del LOD 0 (md = [[[4866C0]]]: +3C con x = 0, +5C y +6C;
  son las ruedas que ya lleva TAN1, filas 0, 2 y 4) girados con el rumbo, cada uno con 43EB60 (0x400 en el LOD 0, si
  no 0x100); la altura ((p1 + p2) / 2 + p0) / 2 y los ejes de esos tres (demo/main.c traf_asienta). SIN PORTAR: lejos
  o en la union el original usa la normal del poligono bajo el centro (46E740 = PS2 0x1de740: la suma de las normales
  de sus esquinas, que COL1 solo lleva de algunos poligonos); alli la demo sigue con sus 4 muestras.

Prueba (tools/prueba/muro.sh; `-DPRUEBA_AUTO -DPRUEBA_MURO -DPRUEBA_SIN_TIEMPO` con PRUEBA_CURSO / PRUEBA_NODO /
PRUEBA_RAMA / PRUEBA_FIN; demo/main.c prueba_mide): el piloto va contra los muros cada 8 s (volante a fondo 1 s o al
30 % 3,3 s) y la demo mide cada cuadro la carroceria contra la COLI (coli_muro con la forma y el marco dibujado), las
ruedas dibujadas contra el suelo, el trafico y la camara. Resultados:
- Host (las 30 etapas de los arboles 0 y 1, el coche a 1 m del limite de la ruta recto y a 10 / 20 / 30 grados hacia
  el muro): con coli_muro el empuje saca la forma en un paso salvo en esquinas del muro (se termina en los pasos
  siguientes) y sin ningun caso de "sin contacto" ni de PlWrecker. Ruedas: con la carroceria a 30 / 60 grados de la
  velocidad, el plano de las 4 muestras dejaba una rueda bajo el suelo hasta 0,47 / 0,85 m (GRAN; mas de 5 cm en
  miles de puntos de cada etapa); con calc_ground_coli_face el plano es el de las ruedas y lo que cada una se aparta
  de el (0,01-0,17 m, NEWY 0,26 en bordillos) lo recoge su suspension. Trafico (un coche de TRAF0 en x = -6..6 de
  cada punto, sus cuatro ruedas contra el suelo 0x400 de los contactos): con las 4 muestras de antes hasta 0,19-0,21 m
  bajo el suelo (BEAC, ALAS; 179-1153 puntos de mas de 5 cm por etapa), con 46E4B0 como mucho 0,07 m (0-8 puntos).
- Flycast, ALPI desde la salida 240 s (r_alpi_a / r_alpi_d) y los tramos malos con PRUEBA_FIN (ALPI s 3450.., ALAS
  s 300.., IMPE s 4650.., INDU s 3500.., RUIN s 100.., METR s 450..), 200 s cada uno:
  con los datos de antes (la ruta de los carriles) y `-DMURO_ANTES` / el codigo nuevo (cuadros con la carroceria mas
  de 0,25 m dentro de un muro, la mayor; cuadros con una rueda mas de 5 cm bajo el suelo, la peor):

| tramo | antes | ahora |
|---|---|---|
| ALPI desde la salida | 255 (4,51 m, s 3526-3948), 333 (-0,37 m derrapando) | 0, 1 (-0,07) |
| ALPI s 3450.. | (la de arriba) | 0, 0; roza el quitamiedos de la galeria con chispas |
| ALAS s 300.. | 371 (4,50 m), 207 (-0,18) | 0, 0; contra el muro en x = -4,7 (antes hasta -6,5) |
| IMPE s 4650.. | 132 (4,30 m); el coche fuera de la colision 364 cuadros y la camara 20,65 m sobre el (s 4881-4896: el salto de la camara en ROMA) | 0, 0; camara a 2,8 m como mucho |
| INDU s 3500.. | 268 (6,02 m), 16 (-0,13) | 3 (0,45); ver abajo |
| RUIN s 100.., METR s 450.. | — | 0, 0 (RUIN s 4540-4576: ver abajo) |
  En INDU s 3667-3741 y RUIN s 4540-4576 (la ruta 22 / 35 m fuera de la carretera) la grua del choque llevaba el
  coche al carril del asfalto de esa ruta, al otro lado del muro: el muro lo frenaba, WrecPlBody (4A0C70: lo que falta
  entre los cuadros que quedan) subia la velocidad hasta el tope (600 km/h) y el coche acababa fuera de la colision,
  con PlWrecker(1) cada cuadro (el carril tampoco tenia suelo) y la camara a 47 m. Por eso la ruta y el sitio de la
  grua son ahora los del original (arriba).
- Con las SEGnn.DAT nuevas, en el host (lim / vacio de scratchpad sobre las 62): ningun punto de la ruta con el centro
  fuera de lo que se pisa (antes ALPI 68, INDU 42, METR 31, RUIN 27, IMPE 43, LASV 332, NEWY 186, FLOR SEG64 467 y
  otras) y con el limite mas de 0,5 m pasado del primer muro solo las medianas de las calzadas partidas (LASV SEG22 /
  SEG54 264 / 256 puntos, antes 986 en SEG22; NEWY SEG30 / SEG62 90 / 90, antes 654; FLOR SEG32 / SEG64 868 / 889,
  antes 1286 en SEG64: el limite abarca las dos calzadas, como antes, y el muro de la mediana lo para coli_muro), 168
  de SEG49 (IMPE de la otra serie: lo primero es un hueco de la colision a 10 m, clase 1 sin suelo, en el que la forma
  no choca, como en el original) y 4 de la salida de BEAC (s 24-68, antes 18). En Flycast con los datos nuevos
  (cuadros con la carroceria mas de 0,25 m dentro de un muro; cuadros con una rueda mas de 5 cm bajo el suelo, la
  peor; la camara, lo mas que queda sobre el coche; ALPI y BEAC con el juego de datos completo, las demas con las
  etapas que pisan ya regeneradas):

| tramo | muro | ruedas | camara y otros |
|---|---|---|---|
| ALPI s 3450.. hasta DESE, 200 s | 0 | 8 (-0,06 derrapando a 41 grados) | 4,0 m (DESE s 10115); 415 km/h como mucho |
| INDU s 3500.. hasta METR | 0 | 0 | 2,3 m; sin PlWrecker ni grua fuera de la carretera; 382 km/h |
| IMPE s 4650.. y la meta | 0 | 0 | 1,9 m (antes 20,65) |
| ALAS s 300.. | 0 | 0 | 2,4 m |
| RUIN s 4300.. y la meta | 0 | 4 (-0,10, en el aire, s 4535-4561) | 4,1 m |
| arbol 0 rama 1 desde PALM, 480 s (hasta CAST) | 0 | 3 (-0,05) | 3,1 m |
| arbol 1 rama 1 desde SANF, 420 s (hasta NIAG) | 0 | 0 | 2,1 m |
| arbol 1 rama 0 desde BEAC, 300 s (hasta NIAG) | 0 | 0 | 3,5 m; 405 km/h |
  En ninguno PlWrecker, coche fuera de la colision ni velocidad pasada del tope del modelo; la camara solo se mueve de
  golpe (1-4 m en un cuadro) al chocar con el muro (el coche se para y la camara sigue). El trafico, sus ruedas (TAN1)
  contra el suelo con el que traf_asienta lo pone (ruedas-cuadro a menos de 120 m del jugador): ALAS 27 de 7408 mas de
  5 cm bajo el suelo (-0,09 como mucho), arbol 0 1009 de 65560 (-0,06 en el LOD 0, -0,19 a 40-100 m con 0x100 en
  LAKE), arbol 1 3320 de 44072 (-0,09 en el LOD 0, -0,16 con 0x100) y 1137 de 57972 (-0,12); es el plano de los tres
  puntos de 46E4B0 (el delantero en x = 0) sobre un suelo que no es plano: las ruedas delanteras quedan fuera de el.
  Con las 4 muestras de antes (contra el suelo 0x400) ALAS 94 (-0,13). PROBABLE: el original da lo mismo (es su
  calculo; el PC usa siempre 0x400, el PS2 0x100 si car+0x328 no es 0: PS2 0x1de594). Las ruedas que no se asientan
  (46E740, lejos o en la union) no se miden.
- La segunda carrera tras la meta con PRUEBA_FIN (IMPE, RUIN) da "Out of memory" (sbrk de 928 KB al entrar) y
  pvr_mem sin VRAM: igual con origin/main 23f5c9b sin estos cambios (no viene de aqui y queda como estaba).


## Choques entre coches (2026-11-08)

Auditoria del choque jugador-trafico y trafico-trafico contra el PC (race_traffic.cpp) y SLES-53998 (capstone): la
estructura de PS2 es la del PC, con los mismos desplazamientos del coche. Codigo: comun/or2_fisica.c (or2_contacto,
or2_choque_coche, or2_choque_trafico), demo/main.c (traf_choques, traf_choca, traf_golpeado, traf_recupera,
traf_esquiva). Datos: TRAFC.DAT (tools/conv_trafico.py) lleva ahora CJM1 (registro 4866C0 y caja 520950 de los modelos
0..29 del jugador: clase 5, radio 2,2..2,5) y SEC1 (tabla de efectos 4F65D0/4F67A0, PS2 0x3de318); SFX.DAT
(tools/conv_sonido.py) lleva los efectos de \\IGC.PAK que usan (0x47, 0x49, 0x4a, 0x4c, 0x4f, 0x5e, 0x64, 0x65,
0x66, 0x67); sfx.c admite 48 muestras y 4 canales de efectos.

| Paso | Original (PC / PS2) | Antes en la demo | Ahora |
|---|---|---|---|
| deteccion | 4F9450 (PS2 0x191fa0): radios 4866C0+0x18, altura (+2DC - radio/2 > alto de la caja del otro: no se tocan); 4F9000/4F8E70/4F86C0 (0x196850/0x196c40/0x196e00): cada esquina de una caja del pose anterior al actual contra los 4 bordes de la otra (epsilon 1e-8, filtro por la diagonal de la esquina) | rectangulo en coordenadas de carretera, +2,3 m / +1 m inventados | igual (en el plano; solo la caja 0) |
| separacion | 4F8490: el cruce mas largo, sin su componente a lo largo del borde, repartido vB/(vA+vB) con el minimo de la tabla [clase A*7+B]; el jugador es la clase 5 (0x17e288) | reparto con la fila 0 | fila 5: contra coches de calle y pesados el jugador se lleva toda la separacion (contra la clase 4, al menos la mitad); al trafico solo la parte a lo largo (su IA lo devuelve a carril+B08 cada cuadro) |
| reaccion del jugador | 4F6FE0 (0x1942d8): roce (+4 0x400000, sin cambio de velocidad) si una esquina del otro entra por mis lados o morro contra morro, si mis esquinas de cola entran, o si mi esquina entra en su lado mas alla del 5 % de cola; mismo coche en 120 cuadros (+DA0/+DA4) = roce; me dan por detras = sin reaccion; estado 7 si cierre > 80 km/h del modelo o (otro con +320 != 3 y yo >= 80); golpe al otro si cierre >= 30 | cualquier contacto por delante o detras; estado 7 si iba a >= 80 contra todo | igual; los coches de calle tienen +320 = 3: a menos de 80 de cierre no hay salto |
| lado del estado 7 | 4F6640: esquinas/bordes de la caja (derecha 0 = sin reflejar) | invertido | igual |
| reaccion del trafico | 4F76E0 (0x194f48): roce por los mismos bordes; el trafico (+4 bit 0 a 0) nunca toma estado propio: solo el efecto 4F67A0 tipo 0 | — | igual |
| golpe al trafico | 476040 (0x1ec620): estado 16/17 coche, 19/18 pesado (475F70, cierre 80), lado 0, +B58 0xB/0xC por +26C del jugador, +B60 = +B64 = 120, v = 1,5 x la del jugador (la pisa el intercambio), efecto 0x4f/0x5e/0x64/0x65 | lado por posicion relativa, animacion reflejada | igual |
| intercambio | 4F7BF0 (0x195708), solo si el +2C8 del jugador <= 1,2e-7 antes de reaccionar, sin derrape (+1030) y el otro sin choque: cierre <= 80: += unidad(vO - vS) x min(v, 80 km/h); si no += (vO - vS) x masa (1; 0,7; 0,1 pesados) x e (1/0,9/0,8); el trafico guarda el modulo (+1C4) | durante la recuperacion no habia intercambio; el trafico guardaba la componente a lo largo | igual (en la recuperacion, +2F8, si hay intercambio) |
| estado 7 | 4F6820 (0x1960e8): tope 1,3843 m/cuadro; POS1: 1,5 s, salto de 0,5 m, guinada +-2 gr, balanceo +-6 gr | igual | igual |
| derrapando | +1030 = 60 con >= 100 km/h y deriva 0x400..0x4000: hay separacion (no se atraviesa), sin reaccion ni intercambio, efecto tipo 0 | igual salvo el efecto | igual |
| en cadena | +B64 = 120 saca al golpeado de la pasada (no choca con nadie); el resto del trafico si choca entre si (4F76E0 + 4F7BF0) | sin choques entre trafico | igual; solo reacciona el ultimo coche tocado de cada uno |
| recuperacion | 473F30 (0x1e7ef0): 120 cuadros derivando 0,2/cuadro (hasta +-12) mientras siga dentro (+268 > 0 / +264 < 0); al acabar, los pesados cambian de carril; los coches de calle siguen con su B08 | carril +-0,5 como borde; vuelta al centro 0,02/cuadro inventada | bordes de la carretera (en la union, los de su calzada); vuelta por 473D10 (0x1e77e0) y los cambios de carril |
| efectos | 4F65D0 (jugador, tipo 3 golpe, 0 roce/derrape) por familia del otro (coche, camion 39..43, autobus 31), cada 60 cuadros (+D22); 4F67A0 del trafico (si esta a menos de 100 m de la camara) | ICS del muro con fuerza inventada | igual (SE de la tabla; la profundidad se mide desde el jugador) |
| puntos y voz | 4F2860: el ultimo tocado sin puntos si al jugador le cambio la velocidad (+4 0x4000000), a la mitad si no; la voz de choque lee el mismo bit | golpe o cambio del otro | igual |

Banco (tools/c/prueba_fisica.c con TRAFC=TRAFC.DAT, m6; km/h del modelo; "a mano" = 4F68F0/4F7BF0 con CHQ1):
alcance 150 a 100: cierre 65, sin salto, 165 -> 85 (a mano 85), el otro 100 -> 180 (180), golpe 17; 250 a 100: cierre
157, estado 7, 257 -> 116 (116), otro -> 199 (199), golpe 16; 420 a 100: 422 -> 164 (164), otro -> 280; desfase
+-1,2 m: lado 0 / 1 como 4F6640; laterales (15 gr y de lado): roce, sin cambio de velocidad; frontal: roce (morro
contra morro); me alcanza a 250 yendo a 100: 108 -> 236 (236), sin salto; derrapando: sin cambio, efecto de roce;
autobus 150 a 100 y 90 a 70: estado 7 (+320 = 0), golpe 18, efectos 0x847 + 0x849. POS1: 16 vuela (3 m, vuelta y
media), 17 trompo (720 gr), 18 sacudida, 19 vuelco (-82 gr de balanceo), 3 s, sin +2F8.
Flycast (-DPRUEBA_GUION -DPRUEBA_EMBISTE -DPRUEBA_TRAFICO -DDIAG_CHOQUE; capturas en scratchpad choques2/a_*.png):
golpe a 64 km/h de velocimetro -> estado 7, el otro estado 16 (vuela) y a 47; roce largo con el autobus (separacion,
sin cambio de velocidad, mismo coche en +DA0) y empujon por detras; 65 s de -DPRUEBA_AUTO: 11 roces laterales, sin
choques en cadena ni incidencias.
PROVISIONAL: la caja 1 (el remolque de los camiones, solo en OUTRUN2SP: ver OUTRUN2SP: camiones) en el plano sin el
cabeceo del remolque; sin el filtro +300 < K(5E0AC0) ni
+329 > 10 de la pasada (constantes sin localizar en PS2); la profundidad de camara de 4F67A0 se mide desde el jugador;
lane_range (46D0C0) con la calzada por carriles 0..2 / 3..5; el rumbo del pose anterior del trafico es el de su
avance (sin +2E8 ni cabeceo/balanceo).

### Velocidad del trafico de delante: el camino "Lead" de 472450 (2026-11-09)

Queja del probador: "en ocasiones frenan demasiado al ferrari". Auditoria otra vez de la reaccion y el intercambio
contra SLES-53998 instruccion a instruccion: 4F6FE0 (0x1942d8: roce por esquinas y bordes, umbral 0,05 en 0x1944cc,
estado 7 con el cierre > 80 km/h del modelo 0x3ebcffe8 o +320 != 3 e ir a 80; golpe con 30 km/h 0x3e0dbfee), 4F7BF0
(0x195708: con cierre <= 80, += unidad(vO - vS) x min(v, 80); si no, += (vO - vS) x masa x e), su tope 4F6820
(0x1960e8, 1,38427508 m/cuadro solo en el estado 7), las tablas (0x35b9d0: 80 / 280, e 1 / 0,9 / 0,8, masa 1 / 0,7 /
0,1 en 0x35ba20, reparto 0x35ba30) y su uso (0x191d20): IGUALES a lo de la demo; el reparto, una sola vez por cuadro
con el ultimo coche tocado, tambien. No es la respuesta al choque.
Lo que divergia es la velocidad del coche contra el que se choca: el factor del velocimetro de todos los coches es
0,628 (P(0x2438)): el trafico a 100 km/h del modelo (+0xC del registro de velocidad, el unico camino portado, "Plain")
va a 63 km/h del marcador, y el jugador a 230-290 del marcador (370-460 del modelo) llegaba con un cierre de 270-360:
estado 7 y el intercambio con e = 0,9 le dejaba a 0,1 v + 0,9 vO (~ 70-80 km/h del marcador). En el original todos
los coches de delante del jugador van por el camino "Lead" de 472450 (PC race_traffic.cpp traffic_472450; registro de
4F0030 = PS2 0x1f1a88..0x1f20c4: minimas por clase 250 / 290 / 310 y maximas 270,9..473 km/h del modelo): unos
180-275 del marcador; solo el de detras del jugador y los de atras van a +0xC. Ahora `traf_objetivo` (demo/main.c)
porta el camino (con el jugador chocando la maxima; a menos de +4 posiciones 470B30 con la velocidad del jugador; mas
alla la cola de 471E70) y el ritmo entero de 472450 (incluido el (v / minima)^2 que faltaba). 472320 y 471F90: ver
"Cambios de carril del trafico" (abajo); K6281C0 = 0,01 CONFIRMADO (PS2 0x1e2680 divide por 100).
Medido en Flycast (`-DPRUEBA_GUION -DPRUEBA_EMBISTE -DPRUEBA_TRAFICO -DDIAG_CHOQUE`, el piloto de prueba va a por el
coche de delante): antes, golpes por detras 200 -> 76, 187 -> 75, 156 -> 72, 154 -> 72, 147 -> 71, 142 -> 71 km/h del
marcador (el trafico a 60-65; pierde el 50-62 %); ahora 245 -> 195 (sin estado 7, el otro a 264), 231 -> 162, 224 ->
172, 227 -> 163, 229 -> 152, 229 -> 144, 244 -> 165, 212 -> 153 (el trafico a 150-200; pierde el 20-37 %). Los roces
siguen sin cambio de velocidad. Valores esperados del original con 4F7BF0: cierre <= 80 del modelo: -80 del modelo
(-50 del marcador); estado 7: 0,1 v + 0,9 vO.

### Cambios de carril del trafico (2026-11-09)

Queja del probador: "el trafico a veces hace cambios de carril subitos y erraticos". Auditoria de toda la parte lateral
de la IA (PC race_traffic.cpp, contrastada con SLES-53998 donde habia duda) y de la de velocidad que la dispara. Codigo:
demo/main.c (tr_*, traf_cambio, traf_decide, traf_seguro, traf_aparta, traf_esquiva, traf_recupera, traf_carriles,
traf_470b30 / traf_470cb0 / traf_470ed0 / traf_alcanza / traf_aparta_de / traf_detras / traf_cola, traf_objetivo,
traf_guia, traf_paso).

| Pieza | Original | Antes en la demo | Ahora | Certeza |
|---|---|---|---|---|
| Vecinos | 479420: lista por avance, el de delante primero; +314 = el de delante, +318 = el de detras; el jugador esta en la lista (comparador 479360 y 46E250(a, b) = lo que b va por delante de a) | "delante" = el mas cercano por delante, "detras" = por detras, al reves de como los usa el original | lista ordenada con el jugador (tr_enlaza) | CONFIRMADO (comparador; PS2 1e68a0 / 1e2d30 usan +318 igual) |
| Movimiento | 47BC00: el coche conduce hacia un punto de mira (47B600: la muestra del carril +66 tres filas mas alla, desplazada B08 m, 474E70) avanzando +1C4 hacia el (475040); la carroceria +2E gira hacia la muestra siguiente con tope 4 grados x 0,8 por cuadro; en las 10 primeras / 5 ultimas filas del tramo pesa la velocidad anterior (k = 0,02 con la direccion alineada, 628218 = 0,04) | c.x = centro del carril + B08 en el mismo cuadro: cualquier salto del objetivo o de los carriles de la ruta salia entero | traf_guia: el coche sigue al punto de mira (de media 2,5 filas, SIMPLIFICADO), giro de +2E limitado, zona de giro | CONFIRMADO (PS2 0x1eab70.. con 0,02 / 0,04 / 0,0698 / 0,8) |
| B08 | m del original (474E70 lo usa como distancia) | cuartos de la separacion entre carriles de la ruta (con centros repetidos donde hay menos de 6 carriles, tools/conv_tramo.py, el coche se iba 4 m al terminar un cambio) | m; +268 / +264 en m desde los centros de los carriles de la ruta | CONFIRMADO |
| Cambio en curso (4736D0, PS2 0x1e6f60) | B08 = (1 - t) B08 +- 4,5 t^2 cada cuadro; si el jugador tapa (471380) +B14 + 1 pero B08 sigue; se anula con el destino prohibido, tocado por el lado al que va o en las 10 primeras filas de la etapa / 5 ultimas de la union; al acabar (o anulado) B08 = desvio real, entre -4 y 4 | con el jugador tapando se congelaba (al soltar, B08 saltaba hasta 0,1 m del original por cuadro); sin anulaciones; B08 -= 4 | igual que el original | CONFIRMADO |
| Arranque (470A30 / 470AB0, PS2 0x1e2520 / 0x1e25b0) | a la izquierda solo con B08 < 1 y a la derecha con B08 > -1 (o el carril actual prohibido); 804350 = 0 | sin la condicion | igual | CONFIRMADO |
| Decision (47DC10, PS2 0x1e68a0) | carril prohibido (+0xB del registro de aparicion) -> 471590; union: pesados del carril del medio a la derecha si el de detras (+318) es el jugador o mas lento; etapa: autobus 473540; clases 3 y 4 con el de detras el jugador o mas lento, fila > 15 y 804350 > 60, hacia el lado de menos peso (4734C0 con los pesos 5B3DE4 de los coches de la etapa); solo si \|lane_pos - cl x 4\| > 4 (a la derecha solo desviado > 2 m a la izquierda o dos carriles), 471010 y sin uno mas lento delante (+314) a menos de 50 m | solo la regla de las clases 3 y 4, con el de delante en vez del de detras, sin la condicion de 4 m (un carril a la derecha siempre), cuentas sin pesos | igual (autobus: sin el radio +B54, PENDIENTE) | CONFIRMADO |
| Seguro (471010, PS2 0x1e2d30) | detras (+318, hasta el jugador): uno mas rapido en el destino que el cambio acerca (solo el jugador, o cualquiera para la clase 0); delante (+314, hasta 50 m): uno mas lento en el destino; con el mas cercano entre 4 y 50 m, no | el jugador por delante y los de detras mas lentos | igual | CONFIRMADO (comparaciones de +1C4 en PS2) |
| Apartarse (473B90, PS2 0x1e75e0) | del de delante o detras de la lista (46DD00 / 46E250 por debajo de 30 m) a menos de 4 m y 2 m de lado | del mas cercano en s de todos | igual | CONFIRMADO |
| Esquivar (473D10) | 46E250(jugador, coche) - 50: empuje maximo entre 0 y 50 m por delante del jugador y nada a mas de 250 | (s jugador - s coche) - 50: siempre el maximo por delante | igual | CONFIRMADO |
| El de detras le alcanza (472320 / 470CB0 / 470ED0 / 4718D0 / 471680, PS2 0x1e46a0 / 0x1e28c8 / 0x1e2ba0 / 0x1e3800 / 0x1e3520) | los de delante del jugador miran al de detras (+318, sin pasar del jugador): si viene mas rapido en su carril a 4..50 m acelera hacia 1,05 x su velocidad (en hueco / diferencia cuadros, tope 480 km/h del modelo) y se aparta un carril (4714D0: lejos de el o del jugador; 471010) o anula el cambio hacia su lado; si no, objetivo 1,05 x (o min(470B30, 1,05 x) en otro carril). Lejos (d >= +4) ese es el objetivo; cerca, max(objetivo, +0xC, 470B30) (PS2 0x1e4300) | 472A50 (de los autobuses y rivales): no ir mas rapido que el de delante a menos de 50 m | igual; 471F90 no se porta (min(1,05 v, 470B30) no pasa de 470B30, el max de despues lo borra) | CONFIRMADO (5B43B0 = 1,05, 5B43AC = 2,2148, 5C403C = 0,7, 6282A4 = 3 en PS2) |

SIMPLIFICADO / PENDIENTE: 803714 (anchura minima de delante del jugador) = 24 / 12, asi que 470CB0 mira un carril (dos la
clase 0); 44F0F0 (calzada partida en la etapa, solo una etapa del original) por la colision con muro entre los carriles 2
y 3 (SUPUESTO); peso del jugador en 46E250 = 1 (SUPUESTO); 44C940 = no ser la primera etapa (SUPUESTO); el autobus lejos
del jugador (473540 con +B54) y 472FE0 sin portar. Los carriles de la ruta de la demo cerca del final de la union (filas
fin-10..fin) se mueven varios metros en pocas filas (carriles repartidos por la anchura del asfalto, conv_union.py): los
coches los siguen (ya sin saltos); PENDIENTE suavizarlos en el conversor.
Medido en Flycast (`-DPRUEBA_AUTO -DDIAG_LAT`, el mismo recorrido del piloto, ~85 s de carrera; `LAT` por coche y cuadro):
cambios de la velocidad lateral de un cuadro al siguiente de mas de 0,03 m: 547 antes, 32 ahora (los que quedan: el
final de cada cambio, donde B08 pasa a ser el desvio real, y los carriles de la ruta al final de la union); antes, la
x del coche era una linea quebrada (esquinas al empezar, al esperar al jugador y al acabar) y con carriles repetidos el
coche cruzaba de lado varios metros en pocos cuadros; ahora la curva del cambio es la del original, sin esquinas (grafica
`lat_antes_ahora.png` del informe). Cambios encadenados (uno al acabar otro, 1 -> 0 -> 1): 2 de 16 antes, 2 de 10
ahora; el original tambien los hace (el de detras le alcanza y 471680 le aparta). Los que empiezan y se anulan al cuadro
siguiente (471680 hacia un lado y 4718D0 lo anula si es el del que le alcanza) no mueven el coche (t = 0). Arranque real
con el menu (nav.sh): video, menus y carrera con trafico, sin cambios.

### Carriles de la ruta: los del original (2026-11-09)

Despues del cambio anterior quedaban vaivenes de 5-7 m al final de las uniones: los carriles de la ruta eran
inventados. tools/conv_tramo.py los repartia cada ~3,7 m por la anchura del asfalto (ajusta_al_asfalto) en todas las
etapas, y conv_union.py igual en las uniones (carriles_uniformes); la lectura de los SOO de la etapa (carriles_soo) solo
reconocia la cabecera 'SOO\x02' (5 de 66 segmentos) y ya no se usaba para los carriles.

| Pieza | Original | Antes | Ahora | Certeza |
|---|---|---|---|---|
| Tablas | 470120 lee muestras de 6 bytes (x, z, y; bit 15 de y = sin carril) a 0x804480 (etapa, 6 carriles x 0x1030) y 0x80A670 (union, 12 carriles x 0x70C), escala K62818C x K5A29EC = 1000 / 4096 m; ficheros "\\OCP\\ocp" grupo "_stg" / "_bra" curso "_othcar_01.._12_tgt.sz" (44C680). En SLES-53998: en cada segmento de STAGE.PS2 8 entradas 'SO' + u16 n (6 de n muestras, una por fila, y 2 de n - 1); en BK.PS2 las entradas 43..102 sin nombre: 12 de 242 muestras (bifurcacion, dos veces) y 12 de 86 (meta, tres veces) | carriles por la anchura del asfalto | conv_tramo.tablas_carriles / carriles_originales y conv_union.tablas_so: el corte de cada carril con la perpendicular a la ruta en cada punto | CONFIRMADO (escala: la media de los carriles al final de la rama da x = -1207,5 frente a -1207,6 de la colision; en la etapa 0 el /4 de antes daba 2,4 % de mas) |
| Bifurcacion | carriles 0..2 por la rama izquierda y 3..5 por la derecha desde el principio; 6..8 / 9..11 solo en las ~56 ultimas filas; 46FE70 al pasar a la etapa: izquierda 0..2 -> 3..5 y 6..8 -> 0..2, derecha 3..5 -> 0..2 y 9..11 -> 3..5 | el mismo numero de carril al pasar (con los carriles del asfalto no se notaba tanto) | ruta de cada rama con los carriles 0..5; al pasar a la etapa el cambio de 46FE70 (trs_fin; el punto de mira al otro lado ya con el carril nuevo; el ultimo tramo de ruta, que mezcla los dos, no se usa); los coches de la otra rama se cierran al salirse de la calzada (en el original siguen por su rama: SIMPLIFICADO); al elegir la derecha guardan su desvio | CONFIRMADO (46FE70, 470120); el cierre, SIMPLIFICADO |
| Coche nuevo en la union | 478640: contador de 12 llevado al lado del jugador, 14 de 16 a 6..8 / 9..11 (con 46CDA0 vuelven a 0..2 / 3..5 donde no hay muestras), mascaras propias del +0xB | 478430 (siempre el carril menos cargado: con la rama derecha, siempre uno de la izquierda) | traf_carril_478640 (sin 6..11: SIMPLIFICADO) | CONFIRMADO |
| Punto de mira | de 2 a 3 filas segun la fraccion de la suya | 2,5 filas fijas | (3 - fraccion) filas | CONFIRMADO |
| Salida del jugador | 4A6ED0: 5C2560 = (0, 0, -16) del marco de la etapa (un jugador; 5C24D0 por puesto con rivales), mirando a -z | 100 m y 11,5 m a la derecha (la otra calzada de PALM, inventado) | el punto de la ruta cuya perpendicular pasa por (0, -16): en PALM s = 13,6 m, x = -10,8 (carril 3 del trafico del original, bajo el arco de START); el coche del menu igual (4A7270) | CONFIRMADO |
| Grua y piloto de pruebas | - | carriles de la ruta | grua y piloto: "carriles del asfalto" cada 3,7 m (PENDIENTE: la grua del original va al centro de la fila de poligonos, GetCsRoadInfoByCsLen) | PENDIENTE |

Datos: `tools/conv_union.py tu.iso datos/ --solo-carriles` cambia solo las columnas de carriles de la ruta de
BKnn.DAT, BKD.DAT y SEGnn.DAT ya convertidos (sin volver a convertir los modelos). Medido en Flycast (`-DPRUEBA_AUTO
-DDIAG_LAT`, rama izquierda y `-DPRUEBA_RAMA=1`): al final de la union el carril (marco de la ruta) se movia 5-7 m en
3-4 filas y los coches con el; ahora la posicion en el mundo (`LAT` lleva x, z) solo se desvia ~0,3 m en el paso a la
etapa (aceleracion lateral maxima 0,17 m/cuadro^2; en todo el recorrido, 1-8 casos por encima de 0,08, frente a las
curvas normales de 0,03-0,06). PENDIENTE: la ruta de la etapa 0..2 sale del SOO con la escala de 1/4 (recorrido_soo) y
el centro de la ruta oscila al principio de las etapas (en el marco de la ruta el carril se mueve, en el mundo no);
los carriles 6..11 de la union; en algunas etapas (5, 13) la anchura del asfalto de la ruta no cubre los carriles del
original (el limite de la colision si).

### Golpeados: parpadeo y cierre (2026-11-09)

Queja del probador: al chocar, el coche "parece sentir el choque, difuminarse y desaparecer". En el original no hay
alfa: es un parpadeo a 30 Hz y despues el coche se cierra (la plaza queda libre). Codigo: demo/main.c (traf_golpeado,
traf_paso, traf_libera, traf_parpadea y el bucle del trafico en el dibujo).

| Paso | Original (PC / PS2) | Antes en la demo | Ahora |
|---|---|---|---|
| golpe | 476040 (PS2 0x1ec620): +B64 = 120 (0x1ec914) y +C5C bit 0 (0x1ec930..0x1ec940, `sd` de +C58 bit 32) salvo en las variantes 3/4 (red), en los coches de carreras con 80FB14 o con 45C440 = 0xE | +B64 = 120 (solo fuera de la pasada de choques) | igual: tambien la marca de cierre |
| aparicion | 4704A0 (PS2 0x1ec44c..0x1ec46c): +B62 = 60 (0 en la variante 4) | - | igual |
| cuentas | 47E780 al final del control: +B62 y +B64 bajan 1 por cuadro; con el suceso suspendido (440A50 bit 0x10 / PS2 0x19d398, la pausa) los baja 4AE5F0 | +B64 en el control | igual |
| dibujo | 4AE5F0 (PS2 0x1d786c..0x1d791c, base +AF0): si +B62 > 0, o +B64 > 0 y no (+B6C y +2F0 & 2 y +2F4 < 30), +B53 ^= 1 (con el modo != 0x12) y con +B53 = 1 no se dibuja; antes de la prueba de distancia (a todo coche vivo, se vea o no) | siempre entero | igual: un cuadro si y otro no; entero los 30 primeros cuadros de la animacion del golpe (+2F4, cuadros de 4A2400) |
| cierre | 478340 (PS2 0x1da828..0x1da858; 479080 lo llama tras 477F60 y antes de abrir): +C5C bit 0 y +B64 <= 0 -> 4401D0 (cierra el suceso) | el golpeado seguia en la carretera tras la recuperacion | igual (traf_libera al principio de traf_aparece): a los 120 cuadros del golpe desaparece y su plaza se puede volver a abrir |
| choques | +B64 > 0 lo saca de la pasada (4F9450) | igual | igual (no choca mientras parpadea) |
| alfa | 4AE0F0: alfa +B68 solo con +4 0x40 (fantasmas de contrarreloj); +4 bit 2 = 0,5 si la camara esta dentro de una caja del coche (470570, solo +320 != 3: autobus y camiones) | - | sin alfa (no hace falta lista TR). PENDIENTE: el 0,5 de 470570 (la camara de carrera no entra en el autobus) |
| sombra | 4ADAC0 no mira +B53 | - | el trafico de la demo no tiene sombra |
| jugador | el coche del jugador no usa +B62/+B64/+B53 (su dibujo es otro): no parpadea ni se vuelve invulnerable; tras el estado 7 solo el roce de 120 cuadros con el mismo coche (+DA0/+DA4, ver arriba) | - | igual |

Coche golpeado (state 16..19): 30 cuadros entero (dando vueltas), 90 parpadeando, cerrado en el cuadro 120 (2 s); la
animacion de 3 s no llega a acabar. Los que aparecen parpadean sus 60 primeros cuadros (normalmente fuera de la vista).

### Vibracion del mando (2026-11-09)

Original (SLES-53998; el PC no la tiene): 0x1d0420 (la llama el control del jugador 0x17c228 cada cuadro; suspendido,
0x17c284 pone los motores a 0) con el registro 0x36b4a8 (+4 escala = opcion/10 de 0x588668+0xCC, +8 motor grande, +0xC
cuadros que se sostiene, +0x10 pequeno, +0x14 cuadros casi parado) y 0x1023a0 (DualShock: grande = min(255, x 255),
pequeno con PWM de 8 cuadros: bit (cuenta & 7) de (int)(x 24)) -> 0x16def8 -> scePadSetActDirect (0x2ba948 desde
0x16d44c). Las banderas: +4 0x10000 separado de otro coche en 4F9450 (el otro en +DD4), +4 0x20000 CbwColiWall en el
cuadro (PC 0x504e3b); +8 0x800/0x1000 las mismas del cuadro anterior (4A8330); +1D0 = +1C4 - +178.

| Causa | Grande | Cuadros | Pequeno |
|---|---|---|---|
| aterriza (+2DC a 0, o su velocidad de < 0 a >= 0) | + 0,75 | 15 | - |
| muro, primer cuadro | + 0,75 | abs(+1D0) x 10 (tope 20) | - |
| muro, siguientes | - | - | + v |
| coche, primer cuadro | + 0,75 si hay cuadros | abs(v otro - v) x 25 (tope 20) | - |
| coche, siguientes | - | - | + v x 0,5 |
| cambio de marcha | + 0,5 | 4 | - |
| tirones (+1D0 cambia de signo, abs > 0,0018) / frenada (< -0,002) | + v x 0,4 (v x 0,3 con freno y < -0,004) | 4 | - |
| arranque (v < 0,33, +1D0 > 0,001) | + 0,3 | 10 | - |

Tope 0,75 los dos (el pequeno antes a 0,5); el grande se sostiene sus cuadros (tope 25) y solo lo cambia uno mayor;
debajo de 0,001 m/cuadro, o 600 cuadros debajo de 0,01, todo a 0. Demo: vib_paso (al final de traf_paso) y vib_cuadro
(cada cuadro dibujado). Dreamcast: un motor (Puru Puru en el puerto del mando, MAPLE_FUNC_PURUPURU): nivel 1..7 = el
grande en 7 escalones, 1 si solo va el pequeno; se manda al cambiar, continuo, frecuencia 26 (SUPUESTO); a 0 si el
control no corre 3 cuadros (pausa, menus). Escala 1,0 CONFIRMADA (2026-11-19): el init de las opciones del SLES-53998
(0x1a1508) escribe el valor por defecto 10, el maximo, en 0x588668+0xCC (tambien +0xD4 y +0xD0), y 0x1d0420 lo lee via
0x1201d0 y lo divide entre 10,0 (fuera de 0..10, a 0); la opcion de CONFIGURACION no esta en la demo, asi que VIB_ESCALA
1,0 es la del original recien arrancado. Sin Jump Pack en el mando no hay vibracion (real o Flycast: el slot del puerto
A en "Puru Puru", emu.cfg device1.2 = 3); vib_cuadro lo avisa una vez por el registro (VIBRACION_INFORME 2026-11-19).
Lo de la superficie (0x1d0138: intensidad por material de cada rueda +24C..+258, bordes +264/+268):
HECHO (2026-11-13, ver Arena, grava y polvo y el comentario de vib_paso); PENDIENTE el termino de +D34 y las banderas
+8 0x40/0x80/0x2000/0x100.

Otras reacciones al choque: la camara (vistas de choque 0x1B/0x1C, la animacion +2D8.. del cuerpo) ya esta en
demo/camara.c; no se ha encontrado una sacudida de camara propia del golpe al trafico (CalcVibrateMatrix 4A2D70 es la
vibracion del cuerpo con +1C8, no del choque). Humo del golpe: 41C570 / 41C940 (OcTireSmokeReq) echan humo de las ruedas traseras en los estados 2, 5,
12, 13, 15, 17, 18 y 19 (smoke_event), del jugador y del trafico: PENDIENTE (efectos de suelo, ver Sombras y marcas).
Verificacion: ver Ultimo cambio verificado.

## Checkpoint

### La pancarta CHECK POINT (2026-11-09)

Queja del probador: "falta el panel que pone checkpoint en carretera". La pancarta no es de la geometria de la etapa
(cs_CS_<zona>) sino del banco de objetos de la etapa `\Stage\<ZONA>\obj_course_obj_cs_<zona>_pmt` (recursos 0x58..0x66
de la tabla de recursos de PS2, `conv_coche.recursos`), que la demo no convertia (se creia que solo era del reflejo).
- Quien la dibuja: PC race_area_geometry_44de10 `p.obj([5D4DC8 + [desc] * 4])` sin condiciones, en el marco de la etapa
  (la decompilacion lo llama "sky"); en SLES-53998 0x172780: `addiu v1, fp, -0x6470` = tabla 0x3f9b90, un id por
  descriptor de tramo (recurso << 16 | registro: 0x580001 PALM, 0x590001 LAKE... 0x630002 METR), dibujado con
  0x1118b8 tras las mallas de la etapa. CONFIRMADO.
- Que lleva (LAKE, registro 1): la pancarta CHECK POINT (textura de 128x32) a 9,2 m sobre z = 0 (el principio de la
  etapa, donde estan las banderas 0x10/0x100 del checkpoint) con sus postes a x +-16..22, y a lo largo de la etapa los
  carteles SLOW DOWN (64x16) y las flechas de las curvas (32x32). 434 vertices en LAKE.
- Los otros registros no son de la escena: el de la tabla 0x3f9c98 (PC 5D4ED0, PS2 0x173100) solo lo dibuja el cubo
  del reflejo del coche (race_env_model_44cd30); el 0 (0x3fa1d0 + id * 0x14, PC 5D5408) es el cartel de los destinos
  del final de la etapa con las texturas de los BUNKI cambiadas (listas 63687C), a partir de la posicion +0x74 del
  descriptor de PS2 (+0x5E del PC): PENDIENTE.
- Ahora: `tools/conv_union.py objetos_etapa` (tabla localizada por su contenido) y `conv_tramo.convierte(objetos=...)`
  anaden ese registro a SEGnn.DAT como nodos de la etapa (estado del GS, visibilidad precalculada de pvs.py, el tope del
  TA de la etapa). Coste: SEG03 +30 KB de fichero; VRAM de la zona del curso +17..+41 KB por etapa (tools/vram.py
  --zona 1344: la peor combinacion sigue siendo BK09 + IMPE, 1127 KB, quedan 217 KB; antes 1109 y 235). Verificado en E (PALM -> LAKE): la pancarta sobre la carretera al llegar al checkpoint (captura
  scratchpad efectos/checkpoint_lake.png).


- Donde: al principio de cada etapa (la de la rama elegida), en los primeros ~8 m de su colision. Ahi esta la
  pancarta CHECK POINT de la propia etapa (vista en Flycast al pasar de PALM a LAKE: la pancarta y en el cuadro
  siguiente EXTEND TIME). Antes la demo ampliaba al entrar en la union (unos 1000 m antes).
- Por que: 4515B0 lleva branch_state (7D38E8): 4 al empezar; 8 en cuanto la posicion pasa de [desc+0x5C] + 1; 1 al
  pisar las banderas 0x400/0x800 (rama izquierda/derecha); 2 con la 0x80 (signo de car+0x25C); y 44FF20 da el stage
  clear con 0x10 o 0x100 fuera del curso 1. car+0x25C son las banderas +0x3C del poligono de COLI0200 bajo el coche.
  Medido en los datos del disco (todas las etapas y coli_BK/LBK/RBK_PALM): 0x10 (1 poligono principal) y 0x100 (4 a 6
  laterales) solo en la posicion 0 de cada curso; 0x400/0x800 en la posicion 8 de la union (x < 0 / x > 0); 0x80 en la
  24 de la union (y una a media etapa que no cuenta: hace falta el estado 1); 0x20/0x200 en la ultima posicion. Asi
  que el unico 0x10/0x100 que dispara es el de la posicion 0 de la etapa nueva.
- Que hace: la clave del coche pasa a la de la etapa nueva; 450CC0 (PS2 0x1cee40) suma 44DC70 de esa clave (PS2
  0x176730, misma formula que el PC) y da el bonus 4F2B20 con los cuadros desde el anterior; 84490C -> 4B9630: escena
  EXTEND TIME (2C00AC, 180 cuadros de temporizador). En la meta no hay ampliacion: GOAL al entrar en la union de meta.
- Ampliacion para el modo de la demo (fila 0, preset 1 (el 0 da lo mismo: filas iguales), dificultad 0, un jugador, coche de serie), en cuadros de 60,2
  Hz: nivel 1 = 3420 + 60 + k (LAKE 3480, ALPI 3420); nivel 2 = 3480 + 60 + k (CAST 3540, FORE 3480, DESE 3450);
  nivel 3 = 3060 + 60 + k (CLOU 3120, INDU 3060, SNOW 3030, GHOS 3000); nivel 4 = 2700 + 60 + k (TULI 2760 ... CAPE
  2610). Inicial 5999 (nivel 0, sin +60).
- Comprobado en E: `-DPRUEBA_AUTO -DPRUEBA_NODO=0 -DPRUEBA_FIN=500 -DPRUEBA_V=280`: "CHECKPOINT: etapa 1 (LAKE) nivel
  1, +3480"; `-DPRUEBA_NODO=2 -DPRUEBA_RAMA=1`: "etapa 5 (DESE) nivel 2, +3450". Capturas fuera del repositorio.

## Escena de la salida (2026-11-10)

El coche que entra derrapando, la camara que lo sigue, los pitidos 3, 2, 1, GO y la cuenta atras: la escena de la salida
del original, con el mismo motor que la de la meta (demo/meta.c, 486730 / 486EF0) y SALIDA.DAT (tools/conv_salida.py).
- Que escena: START etapa 60 (PC 49DD40, PS2 0x1b6824..0x1b69ac): con un jugador el byte de 0x3f9338[eleccion de coche
  0x369268] (30 palabras: 11, 12, 14, 11, 13, 14, 13, 12, 11, 11, 11, 12, 11, 11, 11 dos veces); 12 / 13 solo si la
  palabra 0 del descriptor de la etapa (0x1766f0) es 0 o 15 (si no 11); con mas jugadores 0x3f9328 (0, 10, 10, 10).
  CONFIRMADO. La palabra 0 del descriptor se toma como la clave de la etapa (cur_nodo): PROBABLE.
- Tiempos (CONFIRMADO en el MIPS, 2026-11-11; antes PROBABLE): el control de START es 0x1b6360 (etapa en 0x5a1f50,
  tabla de saltos 0x3f93b0). La etapa 60 (0x1b66ac) abre los sucesos de la carrera, llama a 0x1f4ef0 (4871A0) con la
  escena y sale (0x1b69b4 -> 0x1b6a68) con la etapa 64. Al cuadro siguiente la 64 (0x1b69bc) pregunta a 0x1c67d8
  (45A920), que sin red ([0x5b0a80] == 0) devuelve 1 en el acto: **la espera es de 0 cuadros**; en ese mismo cuadro
  suelta la pantalla de carga (0x1b6040: el icono 0x480000; 0x114f38 borra los sprites; 0x118510 / 0x115ec0 sueltan
  los recursos 0x15, 0x2f y 0x49 del rotulo 490001 de 499A30) y cae en la 65 (0x1b6a04), la unica que llama a
  0x1b4a48 (43FA90: el temporizador del modo, 0x78, solo baja cuando se le llama) y a 0x1bb7e8(1) (8367BC, 0x168 del
  init, -1). A los 120 cuadros pide GAME (0x1b49b8(0x10)) y pone 8367BC = 0xB4 (0x1bb7d8); el coche sale con 8367BC
  <= 60: cuadro 240 de la escena, el del GO del guion. No hay fundido: la escena se ve desde su cuadro 1 (el 0 aun
  lleva la pantalla de carga encima). La camara de la escena (485250 con +E0..+F4 de 486EF0) dura esos 120 cuadros
  (8367BC > 180 solo en START, y op 14(0) en el 120): 1 s de primer plano con teleobjetivo (136,7 mm, 15 grados) del
  coche parado de traves con las ruedas traseras patinando, 1 s desde arriba (43,5 mm) mientras entra derrapando y,
  ya en GAME, la camara de carrera con el coche acabando de colocarse (el movimiento dura 2,5 s) y la cuenta 3, 2,
  1. La voz 0x82 del init de GAME (PC 499E0D, PS2 0x1b83e8) va en el cuadro 120.
- Queja de la demo 31 ("apenas se ve; lo que se ve va lento"), tres causas (2026-11-11):
  1. **El coche no estaba donde mira la camara**: pon_coche aplicaba el desplazamiento +2D8 con los ejes ya girados
     por +2E4..+2EC; el original lo aplica antes de los giros (PC 484810: T(+2D8) Rz Ry Rx; PS2 0x183384: 0x1646fc
     y despues los giros). Con la guinada de 90 grados y 3,5 m de lado del guion el coche salia 3,5 m por delante:
     el primer plano ensenaba un edificio y en el plano de arriba asomaba por el borde. Tambien corrige el
     desplazamiento de las animaciones de choque. (El orden de los giros de pon_coche, y - x - z, no se ha tocado:
     PENDIENTE de contrastar con 0x1833d0.)
  2. **La carga se comia la escena**: la carga de la carrera (coche, VRAM de la etapa, HUD, BAND.DAT, guion) va en un
     solo cuadro y el reloj de la logica seguia contando: el cuadro siguiente daba tantos pasos como tiempo habia
     durado (hasta 600; con el GD de Flycast, instantaneo, no se notaba). Medido con el GD frenado (`-DDIAG_GD_LENTO
     =1200`: KB/s y 120 ms por salto): 1,56 s de carga -> 93 + 10 pasos de golpe, primer cuadro dibujado el 102 de
     120 de la camara de la escena. Ahora el reloj no cuenta lo que el hilo del juego pasa leyendo del GD
     (comun/gd.c gd_us_juego) y se pone a cero al acabar la carga de la carrera: la escena empieza en su cuadro 0.
     Durante la escena no se lee del GD (medido: 0 lecturas del hilo del juego; la union no se pide hasta
     UNION_ANTES y BAND.DAT, el guion y los efectos se leen antes).
  3. **El humo**: en la lista translucida ordenada el coste de la PowerVR crece con el cuadrado de las capas; el humo
     del derrape sumaba 3,5-8,5 pantallas de carteles superpuestos (primer plano y camara de carrera, cuadros 60 a
     207). Tope de HUMO_PANTALLAS (2) pantallas por cuadro, del mas nuevo al mas viejo: LIMITE DEL HARDWARE (valor
     PROVISIONAL hasta medirlo en la consola; el humo sigue siendo el PROVISIONAL de la demo). El humo del original
     (41CB30 / 41D1E0 / 41D330 = PS2 0x10a708 / 0x10aee0 / 0x10b050, constantes 0x32b17c..: tamano 0,5 + 0,05 por
     cuadro, vida 5 + azar x 85 cuadros, una o mas particulas por rueda y cuadro) suma un area parecida.
  Medida (`-DDIAG_SALIDA`, Flycast, por cuadro dibujado): primer plano 1,5 ms de logica y 13,5 de dibujo (15,7-23 mil
  vertices), plano de arriba 1,5 y 11,6 (15,7 mil), camara de carrera 1,5-2,3 y 16,4 (28,6 mil: lo mismo que la
  carrera en ese sitio). `-DDIAG_SALIDA` guarda cuadro, pasos, dt, logica, dibujo, vertices, humo y lecturas del GD
  de cada cuadro de la escena y los vuelca al acabar ("SAL:").
- Guion de las escenas 11..14 (el mismo, 0x36d2f0; la 10, de varios jugadores, otro): 0 op 11(0) (car+38 = 0, car+1F4
  = 0), op 6(1) PasPlCar (475720: con las escenas 8..14 llama a GamePlCar 4A8330, el coche sigue con su fisica), op
  12, op 3(3) y op 4(0) (la musica; la pone main.c), op 15(0); 1 op 0(2C000A, 4, 3) la cuenta atras del HUD, op 5 0x97,
  0x98 y 0x16A, op 18(1) (RET); 11 op 5 0x9C y op 8(2) (humo y marcas de las ruedas 2 y 3); 60, 120, 180 op 5 0x9A (3,
  2, 1); 80 0x9D; 81 0x99; 120 op 14(0) (la camara vuelve a la de carrera) y op 18(0); 150 op 8(0); 240 op 5 0x9B (GO)
  y op 6(0); 300 op 30. CONFIRMADO (ELF).
- Movimiento (4 s): canales 0 / 2 (car+2D8 / +2E0: el coche llega 3,5 m de lado), 3 (+2E8: la guinada de 90 grados a 0),
  4 / 5 (+2E4 / +2EC), 6 / 7 (la direccion de las ruedas delanteras), 8..11 (el giro de las ruedas: las traseras
  patinan 54 rad), 12..19 la camara (ojo y mira en el marco del coche para 485250, +130, focal). El coche se dibuja
  con +2D8..+2EC como la animacion del choque (pon_coche) y el humo y las marcas salen de las ruedas del coche dibujado.
  Las ruedas y la direccion las pone la escena (como en la meta): PROBABLE que GamePlCar no las cambie en la espera.
- Efectos: banco \PAS1.PAK parte A, tono y volumen de la tabla SE de SOUNDCD.IRX (30 kHz; 0x16A 12 kHz); 0x16A (el
  locutor en ingles) solo con el idioma 0 ([0x5d7220], 0x1f6548; 0x1071d8 lo pone a 0 con el ingles): con las voces en
  espanol no suena, como en el original. Se suben al AICA al arrancar (206 KB; queda 856 KB libre) y suenan por 6
  canales propios en turno. 0x99 y 0x9C pasan de 65535 muestras (el limite del LEA del AICA): van en dos trozos y el
  segundo se arranca a su hora desde la CPU (puede repetir hasta un cuadro de sonido): limite del hardware.
- RAM: el guion (16 KB) solo durante la escena; al arrancar 36 KB prestados para subir los efectos. La RAM libre al
  arrancar baja 9 KB (674 -> 665 KB: codigo y huecos).
- Banderero (ROB03, 2026-11-10): comun/robot.c (robot_band_*: init PS2 0x2046f0, control 0x204888, huida 0x204ca0,
  la tela 0x100a68 / 0x100d20 / 0x100d40) y demo/main.c (dibujo 0x204c28: el cuerpo con las piezas de BAND.DAT en
  work+10, la tela de 5 x 7 como tiras con la luz del entorno 2 y el material 0xFFC0C0C0 de 401B80).
  Datos: BAND.DAT (tools/conv_chr.py --banderero; CHR.DAT no cambia): personaje 4 = CHR_AUT04_CVT (recurso 0x12B,
  su textura 1 es la de la tela), esqueleto 4 de bone.bin (0x205038 / 0x40e3f0, 39 huesos), 7 movimientos (88 KB),
  tabla BAND por preset: inicial 0x36f778, huida 0x36f790 (si sigue en el inicial) / 0x36f7a4, lista de 6 0x36f700,
  posicion 0x40ef88, la de los siguientes 0x40efc8, giro 0x40f008; distancias 0x36f7c4 (40: dibujo de la bandera y
  control), 0x36f7c8 (13: huida), 0x36f7cc (1,2: minima), eje 0x40e3c8 (0, 0, -1) y punto 0x40e3d8 (0, 0, 3).
  CONFIRMADO en el MIPS: SetFrame(0) mientras 8367BC >= 0x78, el siguiente de la lista al acabar (mezcla 0 o 20
  cuadros), la huida con 8367BC < 1 (mezcla 4), sin control (ni Calc ni tela) a mas de 40 m o detras de la camara.
  Tela: las constantes de PS2 0x328700..0x32872c son las del PC (gravedad 0,0027222, asta (0,146, 0,18, 0,007) ->
  (0,175, 0,68, 0,015)); el viento (+CAC, 95AEB8, 95AEAC) nadie lo escribe (0). La posicion va en el marco de la
  etapa de salida (col_et.marco): PROBABLE.
  RAM: en CHR.DAT no cabe (la RAM libre al arrancar bajaba de 665 a 492 KB y los huecos de colision ya no entraban).
  BAND.DAT (151 KB) y su motor (31 KB) se leen al empezar la carrera en el hueco de la union (1480 KB), libre en la
  salida (la union se pide a UNION_ANTES = 2500 m del final de la etapa), sin memoria del monton (sus 5 texturas, 13
  KB, en pvr_mem). Se suelta antes de pedir la union (la union no espera), al volver al menu y, en carrera, en cuanto
  la camara esta a mas de sus 40 m (el original deja de moverlo y de dibujar la bandera; el cuerpo lo seguiria
  dibujando, pero de lejos y detras). Diferencia: si se vuelve marcha atras a la salida ya no esta.
- Prueba: tools/prueba/salida.sh (hoja de contactos de la salida, NTSC y PAL).

## Escena de la meta (2026-11-10)

Queja: "agrega la animacion de llegar a la final de etapa que tambien falta" (en META el coche frenaba solo con el
cartel y volvia al menu). Lo que hace el original (PC: race_end_modes.cpp, race_goal_camera.cpp, race_robots.cpp,
race_ending.cpp; SLES-53998 contrastado):
- **Flujo**: 4513C0 (GOAL al entrar en la union de meta) -> modo 19 GOAL (49C9C0 / 49CC50 / 49CFD0; PS2 init
  0x1b8e98). Con [780260] = 1 (SUMO_FE: la consola, 43F860) el modo 19 sigue con 33 (ranking, sin red: directo) y 34
  (OutRun Miles) y 35 (continuar). El final de los recreativos (modo 24: AUTOSCENE 4B5F60 con \AS\as_E_xx, 485470,
  fondo END_*_STAGE, 4527F0) solo se pide desde el 33 sin 43F860 (`43F8C0(43F860 ? 0x22 : 0x18)`): **la consola no
  tiene escenas de final por meta** (AS.PS2 esta en el disco pero no se usa en este flujo). No hay pelicula de final
  en MV.PS2 para las metas.
- **49C9C0**: temporizador del modo 0x4B0 (0x258 en la variante 2); escena 3, o (variantes != 4, 5, 6, 7 y [0x359d98]
  == 0x42) el byte 0x3f99b0 + (meta + preset x 5) x 4 con meta = 450380(8) - 0xA: preset 0 (OutRun 2 SP,
  csc_data_cvt) metas A..E -> escenas 3, 33, 3, 35, 34; preset 1 (OutRun 2, el del modo OUTRUN) -> 1..5. 4871A0(escena). Musica 401000(0, pista, 0)
  (sin bucle): variante 1 0x3f9a18 = {0x42, 0x3e, 0x3d, 0x41, 0x3f}, variante 2 0x3f9a30 = {0x3e, 0x3d, 0x42, 0x41,
  0x3f} (MUSIC.PS2 `\61`..`\66`). La voz GOAL 0x16b de 49C9C0 no suena con 43F860 (la que suena es la de voces.c).
- **4871A0 / 4874A0 / 486730 / 486EF0** (PS2 0x1f4ef0, 0x1f43a0, 0x1f43d0, 0x1f4c08): guion 0x36f2a0[escena]
  (registros de 0x2C hasta op 0x1F; las 8 escenas usan el mismo 0x36d8a0) y movimiento 0x4229d0[escena] (20
  canales de claves Hermite, 10 s). Guion: op 11(0) car+38 = car+1F4 = 0; op 6(1) PasPlCar_Ctrl 475720 (sin fisica);
  op 7(1) el coche al origen del marco de la union (7D3190) con sus angulos; op 12 / 15 / 18 (seccion de curso de la
  camara, estilo del humo, 2057C0 = RET); op 19 / 20 cercano 0,5 / lejano 1900; cuadro 60 efecto 0x96 (\PAS3.PAK,
  cargado durante la carrera: 427700(0x206)); 120..220 82E7C8 = 3 (humo y marcas de las ruedas 0 y 1, 41C570);
  599 cercano 0,1 / lejano 4000; 600 op 30 (fin; el coche y la camara se quedan). Movimiento: car+2D8 / +2E0
  (desplazamiento: el coche entra desde z = -4 y se para en z = -120 de la union), +2E4 / +2E8 / +2EC (giros: trompo
  de 2,17 rad), direccion de las ruedas delanteras y car+32 (c6 / c7), giro de las 4 ruedas (c8..c11), ojo / mira
  de la camara en el marco del coche (c12..c17), balanceo (c18) y focal en mm (c19: FOV = 2 atan(0,70865 / (f x
  0,03937))). La camara del modo 19 es 4853D0 (car+280 = T(car+14) Ry(car+2E) Rx(car+2C)).
- **Coche** (475720 y 4A2650 con car+4 & 0x800000): car+B0 = T(car+14) Ry Rx Rz T(+2D8) Ry(+2E8) Rz(+2EC) Rx(+2E4),
  car+F0 = identidad, cada rueda sobre el suelo (43EB60 0x400: +134 += reposo + suelo - y), 4A4710 (volante por
  car+32). Los sucesos de la carrera siguen (trafico).
- **Piloto** (488DD0 estado 9 con 44FF10; PS2 0x20250c): fila del modelo 0x40e9b8 y tabla por 44DC50 de la clave
  (0x1B 0x40e978, 0x1A / 0x1C 0x40e998, si no 0x40e958); dos registros {id, bucle, fin} (0x36f500..: 0x60057 /
  0x200016, {0x200000, 0x200010, 0x20001b}, ...); de cada uno el id, el bucle hasta 0xF0 / 0x168 cuadros y el de fin.
  La acompanante: 0x60049 (ya estaba).
- **GAD 498670**: el cartel GOAL (0x2c0009, ya estaba) y, cuando el temporizador baja de 0x3FC, la tabla de tiempos
  por etapa (4979E0) y la voz 49A190: sin portar (ver Fin de la carrera).
- **Checkpoint** (fin de cada etapa intermedia): en el modo OutRun no hay escena: EXTEND TIME (2C00AC + 0x4d / 0x16c),
  el bonus de etapa, los objetos de la union (BO) y, para el piloto, el cambio de area 44BCE0 con tablas -1 (nada).
  Todo eso ya estaba (ver Checkpoint y Objetos animados de las uniones).

En la demo: `tools/conv_meta.py` -> META.DAT (escenas 1..5, 33..35 con su guion y movimiento, tablas de escena y de
musica, el efecto 0x96 en ADPCM de Yamaha; 110 KB de RAM desde el arranque); `demo/meta.c` (49C9C0 / 4871A0 /
4874A0 / 486730 / 486EF0); demo/main.c `meta_*` (475720, 4853D0, planos, humo y marcas por la mascara, 0x4B0 cuadros);
comun/robot.c `meta_piloto` (tablas METR / METE de CHR.DAT, conv_chr.py: 20 movimientos mas, +42 KB);
comun/musica.c `musica_pon_una` (MUS61..MUS66.ADP sin MUS64, con tools/c/musica.c como las demas); comun/sfx.c
`sfx_muestra_*` (30 KB del AICA durante la meta). Op del guion sin portar: 12 (la demo elige lo que dibuja a su manera)
y 15 (el humo de la demo es el PROVISIONAL de siempre). PENDIENTE: la tabla de 498670 (los modos 33 / 34 / 35:
ver Fin de la carrera) y la musica de la meta de la variante 0 (Time Attack del PC).

Verificado en E (Flycast, `-DPRUEBA_AUTO -DPRUEBA_NODO=13 -DPRUEBA_FIN=400 -DPRUEBA_SIN_TIEMPO` y `PRUEBA_NODO=11`;
`-DDIAG_META` registra camara y coche cada 60 cuadros, `-DDIAG_META_CUADRO=n` congela la escena): meta D (IMPE,
escena 35, MUS65) y B (METR, escena 33, MUS62): el coche entra, derrapa con humo de las ruedas delanteras y se para
cruzado delante del publico; piloto y acompanante con los brazos arriba; teleobjetivo de la escena 33; vuelta al
menu a los 1200 cuadros; arranque normal (peliculas y menu) igual.

## Objetos animados de las uniones (2026-11-09)

Queja del probador: "hay algunos efectos cuando se cambian de etapa que faltan, como pajaros volando". Son los objetos
del fondo de BK.PS2 `\ANIMS\obj_bo_*_pmt.sz` (recursos 0x136..0x143) con sus trayectorias `oso_dyn_bo_*_bin.sz`.
Codigo del original: PC race_area_objects_44f190 (PS2 0x174328), race_area_ending_44d410 (0x174a48), 44EDE0 (las
cuentas), 4F0CB0 (0x1d75c0), 4A9BD0 / 4A9C80 / 4A9DF0 (race_traffic_objects.inc). Conversor `tools/conv_bo.py` (BO.DAT
y BOnn.DAT); demo/main.c bo_*.
- Conjuntos: 14 registros de 0x24 bytes en 0x35a540 (modelo, trayectoria, objetos, posicion de la union a la que
  salen: 100 o 60, velocidad lateral 1,48 / 0,74 m/cuadro, objetos de 0x40 bytes con su clase, trayectoria, banderas y
  retraso); clase -> modelo en 0x40a958 (8 bytes por clase). Variante 1 (fila 1 de 0x35a738): 2 pajaros sueltos, 1
  bandada, 13 halcones, 12 flamencos, 11 dragon (obj_bo_waib; antes se decia dirigible), 7 pegaso, 6 tanque. CONFIRMADO.
- Eleccion al entrar en la union (no en las metas), por lo hecho en la etapa (44EDE0, solo fuera de la union): choque
  fuerte 1 / 2, 5, 6, 12, 13 -> pajaros sueltos; muro o fuera del asfalto >= 10 % de la etapa -> bandada; fuera del
  asfalto -> halcones; contacto con otro coche (+4 0x10000 o 0x400000) -> flamencos; si no, el dragon (7D30A4 o el
  nivel del registro del arbol, 0 en csc_data_2: el pegaso y el tanque no salen en el modo OutRun). CONFIRMADO en
  0x174b9c..0x174c74. 7D3094 (posiciones de la etapa) se toma de la ruta (s de la union - s de la etapa) / m_pos:
  PROBABLE (el original lee +0x7C del descriptor).
- Salen al pasar la posicion de la union (rama elegida; 451350 = 2 no los mueve) con su trayectoria de 6 canales
  (513650, t += 0,0166 por paso, en bucle con la bandera 8 o quietos al final), en el marco de la union movido a la x
  del coche (bandera 1, luego + su velocidad lateral; bandera 4: a 1,48 m/cuadro), z = -690 con la bandera 2 (PS2
  0x35a924), reflejados en la rama derecha. En la rama izquierda los pajaros cruzan la carretera por delante del coche
  en el tramo transversal de la union (z ~ -685). Se quitan al llegar a la etapa siguiente (car+0x5C a 0).
- Alas: los nodos con la bandera 0x20 tienen animacion de vertices (las tiras con 0x800 en la tabla 3 traen 4 paquetes,
  uno por cuadro); el cuadro sale de la tabla de 0x328aa8 (la morph_var.dat del PC dentro del ELF: 23 grupos {indice,
  k, m, m floats}; 0x103120) con el reloj de 0x448850 (+1 por cuadro, 0x1030d0 desde 0x20d7f8; aqui +1 por paso de
  logica, 60 por segundo).
- Alas quietas (queja del probador de la demo 22, "los pajaros no tienen animacion", 2026-11-09). CAUSA: los datos
  estaban bien (BO01: 4 cuadros distintos, 136 vertices cada uno) pero `lee_modelo_en` pone a 0 el relleno[0] de todos
  los nodos para el presupuesto del TA, y ahi lleva conv_bo.py la malla y el indice de animacion de cada nodo: todas
  las alas usaban la animacion 0 (0..0,7 en 120 cuadros: casi siempre el cuadro 0) y el primer nodo de cada registro
  recibia la suma del TA como malla (no se dibujaba: faltaba un pajaro de cada grupo). Ahora el modelo de la union se
  pide con `relleno_propio` y el lector no lo toca. CONFIRMADO en Flycast (capturas consecutivas con poses distintas).
- Mezcla de cuadros: PS2 0x103018 da k = (int) f (0x448854) y la fraccion f - k; 0x110700 guarda la fraccion con cada
  tira en la cola de dibujo (+0x14) y el paquete del cuadro k % cuadros de la tira (0x1030a8); el PC (40ECC0 / 404700)
  mezcla los cuadros k y k + 1 (0 tras el ultimo) con pesos 1 - f y f (vsc 7). Aqui se mezclan en la CPU las
  posiciones y los colores de las dos mallas (bo_dibuja_malla; las mallas de los 4 cuadros casan una a una en n y
  textura en los 5 conjuntos del modo OutRun, comprobado en bo_subido). CONFIRMADO en el PC; PROBABLE en el PS2 (la
  fraccion llega a la cola; no se ha seguido el microcodigo del VU1). Coste: CPU ~1800 vertices interpolados la
  bandada, ~2400 los halcones, ~2300 el dragon; el TA no cambia.
- Memoria: cada malla del original una vez por cuadro y los nodos con su matriz (la bandada: 13 pajaros con la misma
  malla): BO01 18 KB, BO02 15 KB, BO12 37 KB, BO13 122 KB, BO11 276 KB de RAM (fichero) mientras se esta en la union;
  VRAM 1-32 KB por conjunto (fuera de la zona del curso: pvr_mem con el apuro y la rebaja de sube_tramo); BO.DAT 74 KB
  en RAM siempre (tablas, morph y las 7 trayectorias de la variante 1). TA: dibuja_malla con ta_cabe (la bandada ~70
  vertices por pajaro, el dragon ~2300 por cuadro).
- Dragon (conjunto 11, el que sale si la etapa va limpia: el caso mas comun; 2026-11-09): no salia. BO11.DAT (276 KB)
  no cabia en el monton con las dos ramas de la union leidas (288 KB libres pero troceados: la cima del monton tenia
  ~135 KB). Arreglo doble, sin cambio de imagen: (1) conv_bo.py compacta_cuadros deja en el ORD7 solo las mallas del
  cuadro 0 y pone los cuadros 1..3 en una cola BOF1 (posiciones int16 con una escala por fichero, 0,27 mm en BO11, y
  el color de cada vertice; las UV no cambian entre cuadros): BO11 167 KB, BO13 84, BO12 25, BO01 13; (2) el BOnn.DAT
  se lee en la cola del hueco de la union, donde empiezan los texeles del BKnn.DAT: estos (155-249 KB) ya estan en la
  VRAM y nadie los vuelve a leer (comp de sube_tramo solo es la etapa, para la meta, y en las metas no hay objetos);
  quedan 217-230 KB en las 15 uniones. Ya no usa el monton (si no cabe en la cola, se intenta en el monton como
  antes). Verificado en Flycast en una carrera desde el menu real (pelicula, menus, Dino 246, acelerador a fondo,
  `-DPRUEBA_BO=11` solo para forzar el conjunto: la eleccion natural fue la bandada por el muro): "CURSO: BO11.DAT en
  0.15 s ... RAM libre 278K" (igual que antes de leerlo), el dragon cruza con sus alas (capturas aves/bo11_*.png).
- PENDIENTE: los sonidos (+0x18 / +0x1C: efectos 0x83..0x91 de los bancos 0x30a..0x316), la acompanante senalandolos
  (race_robots_runtime) y el filtro de visibilidad 4F0E40 (aqui: esfera contra la vista y el LOD por tamano, umbral 8).
- Verificado en E (`-DPRUEBA_AUTO -DPRUEBA_NODO=0 -DPRUEBA_FIN=500 -DPRUEBA_V=280 -DPRUEBA_RAMA=0`; la eleccion natural
  fue la bandada por un roce con el muro; `-DPRUEBA_BO=n` fuerza un conjunto): la bandada cruza por delante (capturas
  scratchpad efectos/pajaros_bandada_*.png).

## Camara de carrera

Clonada de la decompilacion de PC y contrastada instruccion a instruccion en SLES-53998 (`demo/camara.c`, un paso por
cuadro de logica; tablas 'CAM1' de PARAM.DAT, `tools/conv_param.py`, localizadas por el memcpy de 0x5460 bytes de
0x178810).

- Marco (SetCarCamera 4A1680 / PS2 0x1817c0): velocidad proyectada en el plano del suelo (por debajo de 0,015
  m/cuadro se mezcla con el morro), girada la mitad del angulo hasta la carroceria. Da +1FE (giro), +1FC (cabeceo) y
  +200 (balanceo del suelo).
- Giro de la vista (483110 / 0x179d80): el del marco por encima de 0,00922849774 m/cuadro (2 km/h); entre 0,0046 y
  0,0092 se mezcla con la carroceria; parado, la carroceria. Vistas 0/1: + 0,6/0,8 x |sin deriva| x (carroceria -
  marco). Tras un choque con grua: del giro guardado al empezar (+D8C) al de la carroceria en dur x 60,2 + 1 cuadros.
- Matriz (484810): pivote = coche + carroceria x (0, 0, +2C de la vista) (-1: 1 m delante); Ry(giro) Rx(cabeceo de la
  carroceria) Rz(balanceo del suelo) (+240) o, en las vistas con +30 = 1, Rz(balanceo de la carroceria) y la
  animacion del choque (+200). Sin inercias: la camara va rigida al marco; el unico filtro es el cabeceo.
- Cabeceo (483CE0 / 0x17b3a8): media de los 30 ultimos +1FC x factor de la tabla (1 o 2), entre el tope bajo y alto
  (0..15 o -1..16 grados), gira el ojo alrededor de la mira (radio |mira.z - ojo.z|). El PC trunca la media a entero;
  PS2 no (se sigue a PS2).
- Vista 2 (exterior, por defecto), modelo 0: ojo (0; 2,05; 5,5), mira (0; 2; 0) respecto al pivote (+1 m en z):
  ojo 6,5 m detras del pivote y 2,05 m alto, mira 2 m sobre el pivote; FOV horizontal 1,396 rad (80 grados). Vista 1
  (interior): ojo de la tabla por modelo, FOV 85. Vista 0 (parachoques): ojo (0; 1,25; 0) mira (0; 0,86; -3), FOV
  85. Boton (entrada 0x40000; en la demo Y): 2 -> 1 -> 0 -> 2, con mezcla de 30 cuadros (4833E0/483600).
- Choque (4832F0/484A40): estados 1, 2 y 5 al entrar pasan a la vista 0x1C (estado 1; ojo (0,1,10)) o 0x1B (ojo
  (0,2,10)), mira (0,0,0) + la y de la animacion, FOV 70, durante dur x 60,2 + 1 cuadros, y vuelven con mezcla.
- Ojo como minimo 0,75 m sobre el suelo (485590 / 0x17a5d4).
- Proyeccion (483C10 / 0x17b168, 484BD0): pantalla virtual 640x480; en 4:3 FOV horizontal = el de la tabla, vertical
  2 atan(0,75 tan(FOV/2)); escalas +B0 = 320/tan(h/2), +B4 = 240/tan(v/2): pixeles cuadrados. En la demo fx = fy =
  320/tan(FOV/2) (vista 2: 381). La salida PAL de PS2 reescala ese 640x480 virtual al modo de video: no cambia el
  encuadre. PS2 tiene ademas 16:9 (+B8 = 1,7778: mismo FOV vertical, horizontal mas ancho), que el PC no tiene.
- Diferencias PC/PS2 encontradas: lejano 4000 (PC 5000); +84 0,1 (PC 0,5, uso desconocido); 16:9 solo en PS2; media
  del cabeceo sin truncar en PS2; PS2 llama a 2057C0(0x16A/0x16B) al cambiar de vista (en el PC son RET). Tablas,
  umbrales, 0,6/0,8, 0,75, 60,2 y la logica de vistas: iguales.
- Comprobado: tablas y codigo leidos del ELF (capstone); banco en host (recta, deriva, cuesta, boton, choque) y
  Flycast con `-DPRUEBA_GUION` (piloto, toque de freno, Y, imagen congelada 3 s en cuadros fijos; `-DPRUEBA_CAM_ANTES`
  da la camara de antes con el mismo guion). Referencia visual: la pelicula de atraccion de MV.PS2 (entrada 1,
  segundo 14,5: persecucion con derrape) encuadra el coche como la vista 2 nueva (coche ~1/3 del ancho, horizonte a
  media pantalla).
- Camara del fin del tiempo (modos 20..22: 485830 = PS2 0x178e58, desde la tabla de 0x178b28; HECHA 2026-11-13,
  `cam_entrada_t.agotado` de demo/camara.c): el coche y 484810 como la de carrera, sin 4832F0 ni 484A40 (sin el boton
  de la vista, sin las vistas del choque y sin descontar +346); fuera de las vistas 0x1A..0x1D guarda la actual
  (4833E0 con 0x1E cuadros, 0x17a338) y toma la 0x1A (ojo (0; 1,6; 3,9), mira (0; 1,1; 0), FOV 60 grados, con la
  carroceria: CAM1 de PARAM.DAT); despues 2057C0(0x16A, 1) y (0x16B, 1) (RET en el PS2) y 485590 con 0x1E cuadros
  (0x17a3d0): la mezcla de 30 cuadros de la vista de carrera a la 0x1A. Antes la demo seguia con la de carrera.
  Flycast (resultados.sh, OUTRUN2SP preset 1, fin del tiempo en SANF): capturas cada 0,5 s desde el fin del tiempo: de
  la vista 2 (ojo 6,5 m detras) a la 0x1A (mas cerca y mas baja) en medio segundo y quieta despues; 25, 26, 27 y el
  frontal como antes, 0 "Out of memory".
- PENDIENTE: interior del coche para la vista 1; camara de salida con el temporizador real (8367BC) y camaras de
  fin (485470 es la del final de los recreativos, modo 24, que la consola no usa: ver Escena de la meta); desplazamiento del centro de proyeccion +C4/+C8 (cero en las vistas usadas).
  Hecho: planos cercano 0,1 y lejano 4000 del original (seccion Graficos).

## Coches: materiales, colores y reflejo

Antes eran inventados: la pintura sin textura en "rojo con luz y un brillo por arriba" (tools/modelo.py), una "tapa"
oscura que cerraba el habitaculo (conv_coche.py), el gris claro del trafico y el orden de dos modelos del trafico.
Ahora (tools/conv_coche.py `materiales`, `extension`; tools/conv_trafico.py; demo/main.c `coche_cex`, `coche_color`,
`coche_entorno`, `traf_colores`):

- Material de cada malla: tabla 4 del registro (36 bytes: material, -, ?, textura capa 1, estado (tabla 7), textura
  capa 2, ...) y tabla 5 (64 bytes: difuso RGBA, ambiente, especular RGB + potencia, emision; 128 = 1). CONFIRMADO
  por la estructura (los indices caen en las tablas de cada registro); el significado de especular/emision es el del
  D3DMATERIAL del PC (PROBABLE). Mallas sin textura: difuso x luz + emision, alfa del difuso.
- Pintura: textura de 8x8 de un solo color cuya clave `recurso << 16 | textura` esta en la lista de colores del
  modelo: tabla de 30 listas en 0x35ad48 de SLES-53998 (PC 64D120; 408C80 busca la clave y toma la entrada
  car+0x12), entradas {clave, colores 1..8}, cada color una textura de 8x8 de `\COMMON\obj_pc_color_pmt.sz` (recurso
  0xBB) o del propio coche. car+0x12 = color elegido + 1; la seleccion pasa al siguiente con (c + 1) % 8 (PC,
  colores en 68ECA8). CONFIRMADO (tabla y texturas leidas; 8 colores por coche, el 1 es el de serie). En la demo: Y
  en la seleccion de coche (la barra de ayuda del original dice Y COLOR); el color va con el coche a la carrera.
- Reflejo: la carroceria lleva en la capa 2 el mapa de entorno esferico del coche (textura de 256x256 con un circulo,
  alfa 0 fuera); cristales, espejos y cromados no tienen UV y su capa 1 es ese mapa. En la Dreamcast: una sola pasada
  con color de desplazamiento (PowerVR "offset color"): textura = mapa de entorno, base = especular, desplazamiento =
  pintura x luz. Sin vertices de mas; el TA guarda 4 bytes mas por vertice con desplazamiento (1680-4980 vertices
  de entorno por coche: <= 20 KB por cuadro). UV por cuadro en la CPU (normal del modelo en s8, girada por el coche
  y la camara: u = 0,5 + 0,5 nx, v = 0,5 - 0,5 ny, que es exactamente el mapa esferico del vector reflejado para un
  ojo en el infinito). Cristales (mallas con la bandera 0x800): con alfa del difuso < 1, mezcla normal; con alfa 1,
  el reflejo se suma a lo de detras (cabecera ONE/ONE). SUPUESTO: que el PS2 sume el reflejo x especular y la mezcla
  aditiva de esos cristales (la tabla 7 del registro, estados del GS, no se ha descifrado; el PC usa un cubo
  dinamico; el microcodigo VU1 de PS2 no se ha leido).
- Interior: el real (asientos, salpicadero, volante, palanca) se queda: se ve por los cristales translucidos. Sin
  piezas anadidas.
- Presupuesto: TOPE 11000 vertices dibujados a la vez por coche; ver DREAMCAST.md.
- Trafico: materiales igual (los cristales con UV de entorno fijas: vista desde detras, sin calculo por cuadro); la
  capa 2 de la carroceria no se dibuja. Colores: tabla de 44 listas en 0x36bc18 (modelos 30..43: {clave, color 1,
  color 2}, otras texturas del mismo bloque); el original da a cada coche nuevo de un modelo el color siguiente
  (0, 1, 2; 2 en el modelo 38 y 1 en el 39 y el 41: PC 478B90). Las listas confirman que los registros 69 y 136 son
  los modelos 33 y 37 (antes al reves).
- PENDIENTE: capa 2 de las mallas con textura propia (mismo mapa: haria falta una segunda pasada); el estado de la
  tabla 7 (mezclas del GS); leer el microcodigo VU1 para la formula exacta del reflejo; reflejo del trafico por cuadro.

### Estructuras descolocadas y popping cercano (Rev 1.4, 2026-11-20)

Queja: en MAYA, IMPE (Paris) y NEWY estructuras de los margenes salen en el cielo o cruzando la carretera, y popping a
corta y media distancia.
- Reproduccion en Flycast (`-DPRUEBA_AUTO`, salida en MAYA / IMPE / NEWY y el arbol 1 entero hasta NEWY por la derecha
  con `-DPRUEBA_RAMA=1 -DPRUEBA_RAMA_FIJA=1 -DPRUEBA_SALTO=900`): nada descolocado. La causa probable es de la consola:
  la PowerVR no recorta en la pantalla y un vertice proyectado a decenas de miles de pixeles (delante del plano
  cercano, pero muy cerca de la camara y muy a un lado: muros, pilares, paredes que se pasan rozando) estropea la tira
  (sale estirada hacia el cielo o cruzando la calzada); Flycast la dibuja bien. El arreglo de 7dfe8a9 solo recortaba a
  la pantalla los vertices nuevos del plano cercano (tri_recortado).
- Banda de guarda (CORREGIDO, sin verificar en consola real): `fuera_guarda` / `esfera_en_guarda` (GUARDA_PX 4096): la
  via directa (malla_sq, `seguro`) solo con la esfera de la malla entera dentro de la banda; `dibuja_malla` recorta a
  la pantalla (envia_recorte_lateral) las tiras enteras delante del plano cercano con algun vertice fuera; y
  tri_recortado mira todos sus vertices, no solo los recortados. Medido (`-DDIAG_GUARDA`, arbol 1 hasta NEWY): ~11300
  tiras en toda la carrera (~1 por cuadro, mas en NEWY) que antes iban a la PowerVR con vertices fuera de la banda.
- PVS cerca de la camara (DECISION DEL PORT): los nodos pequenos (radio < PVS_CERCA_R = 80 m) con el centro a menos de
  PVS_CERCA = 120 m se dibujan enteros aunque la tabla de tools/pvs.py de alguna de sus tiras por tapada (lo que las
  tapaba puede haberse quitado por el tope del TA). Coste medido en NEWY (DIAG_POPPING): el mismo reparto (corte y
  falta iguales); sin el limite de radio, el suelo y la calzada (radios de cientos de m) contaban enteros y el corte
  bajaba a 1 m.
- Lo que queda del popping es el tope del TA (falta 13-31 % en partes de NEWY con el coche 608 KB y la pareja 149 KB por
  cuadro) y el LOD del original: no se ha cambiado el reparto.

### LASV -> MAYA, la meta de MAYA y el agua de LASV (Rev 1.4, 2026-11-20)

Queja: en OutRun 2 (arbol 1), de LASV a MAYA, al salir de la bifurcacion una estructura tapa la vista; al principio de
MAYA cosas flotando en el aire; en la meta lineas blancas en el suelo y la piramide mal hecha; quitar el brillo del cielo
de la carroceria; y en LASV el suelo mojado sin agua de las ruedas. Reproducido en Flycast (`-DPRUEBA_AUTO
-DPRUEBA_SIN_TIEMPO -DPRUEBA_CURSO=1 -DPRUEBA_NODO=7 -DPRUEBA_RAMA=0 -DPRUEBA_RAMA_FIJA=0`) y comparado con un video
de OutRun 2 SP (las 15 etapas seguidas) que dio el usuario.
- La pared negra de la union (1'23"-1'25" de carrera; al final la camara la atravesaba: pantalla negra): MAYA (la etapa
  siguiente) desde la union. `-DDIAG_SIN_TRAMO=2` la quita y `-DDIAG_SIGV` dio los nodos: el 1460 de SEG29 es un anillo
  de cartones de selva de 164 m de alto que rodea la etapa y pasa 400 m por detras de su entrada (cruza la carretera de
  la union) y el 195 una panoramica de 2,4 km de radio. DESVIACION (CORREGIDO): con la camara antes de la entrada de la
  etapa siguiente (`sig_antes`, pvs_paso) no se dibujan sus mallas con el centro detras de la entrada
  (DETRAS_ENTRADA: ese sitio es de la union). En el original los aplasta el crecimiento (44C530); en el video la
  entrada a MAYA es una autovia despejada.
- La losa flotante sobre la union: los nodos que VISO.DAT da para la seccion 0 de MAYA con la camara fuera (el 194 de
  SEG29) y que desde dentro no se ven nunca. CORREGIDO: antes de la entrada (y < 0, sin mirar el desvio lateral: la
  carretera de la union no sigue la ruta prolongada y fuera de PVS_LADO no habia PVS) la PVS de la seccion 0
  (`-DPVS_ANTES_VISO`: lo de antes).
- El crecimiento de la etapa siguiente (geo_vista) se apago en la Rev 1.4 y vuelve en la Rev 1.5 (`-DSIN_GEO_CRECE`
  lo quita): sin el, en todas las bifurcaciones la etapa siguiente aparecia de golpe y entera. Lo que hacia de lamina
  encima de la union era lo de detras de la entrada, que ya no se dibuja (DETRAS_ENTRADA).
- La mancha amarilla que tapaba la pantalla en el CHECK POINT de MAYA y las franjas blancas del suelo de la meta: la
  textura 101 de SEG29 son tiras de polvo translucidas en la salida y en la meta, ocho fotogramas de una animacion de
  nodos del original (pilas de 4 nodos iguales en el mismo sitio y la tira corriendo por la calzada) que la demo
  dibujaba todos a la vez (Rev 1.5: la regla contaba mallas y no nodos y quitaba tambien las cascadas de NIAG, SEG21
  texturas 105 / 106 / 107 / 111, y cosas de LASV, AMAZ y SEG53 / 54 / 58; ahora cuenta nodos distintos y solo queda
  la de MAYA; y la prueba era de todas las parejas de mallas, hasta 33 millones de comparaciones en el hilo de carga
  en mitad de la union, que quitaba tiempo a los cuadros: ahora ordenada, n log n). `-DDIAG_SIN_L2` y `-DDIAG_CICLO` (apaga por turnos el sol, los efectos y la etapa) llevaron
  hasta ellas; no era el humo (`-DDIAG_SIN_HUMO`) ni z-fighting (subirlas 4 cm no cambiaba nada). DESVIACION
  (CORREGIDO): una textura de la lista translucida con al menos 16 mallas en pilas de 3 o mas nodos en el mismo centro
  no se dibuja (lee_modelo_en; con los datos de hoy solo esa: las pilas de 2 de la textura 16 de las uniones no
  llegan). PENDIENTE: la animacion de nodos del original (+18 del PmtNode) para sacarlas como en la PS2.
- La piramide de la meta (BK26, nodo 4): un render de los datos fuera de la demo (scratchpad rtex.py) da la misma
  forma que Flycast: la "cupula" gris de detras es geometria del disco (texturas 7 y 14, x +-400 m, hasta 271 m de
  alto), no un fallo de dibujo. Sin referencia de la meta del original (el video es el modo de 15 etapas, sin meta en
  MAYA): sin cambios.
- Brillo del cielo en la carroceria (Rev 1.3): quitado a peticion (refl_busca; `-DCON_REFLEJO_CIELO` lo vuelve a poner).
  El reflejo del tunel tambien depende de esa tabla y se va con el; el coche usa su mapa de entorno fijo.
- Agua de las ruedas sobre mojado (NUEVO): la fuente 6 de las particulas del SLES-53998 (0x10c8b8 / 0x10c988 /
  0x10cc80 / 0x10cd58, textura 0x57002c, que tools/conv_efectos.py anade a EFECTOS.DAT como cuarta de PAR1), con la
  superficie +40 & 0x400080: LASV usa la 0x400000 en casi toda la etapa (`-DDIAG_PARTICULAS`). Detalle en agua_pide.
  PROVISIONAL: el tamano crece car+1C4 x AGUA_CRECE (0,25) por cuadro; con x 1, lo leido, las gotas llegaban a 20 m de
  lado y tapaban media pantalla, y en el video la estela es baja y pegada a las ruedas (el dibujo del tipo 3, 0x108d30,
  no esta leido). Sin EFECTOS.DAT nuevo, sin agua (como antes).
- NO verificado en consola real.

### Rev 2.9.8: lecciones de Crazy Taxi

Comparacion tecnica con Crazy Taxi de Dreamcast (material de referencia: de el se toma la TECNICA, nunca codigo ni
datos) y cambios P1..P7 propuestos a partir de ella. El principio que se aplica: lo que se dibuja depende de la
posicion y de la vista, no de la carga del cuadro anterior (Crazy Taxi tiene bufers de tamano fijo por lista, lejanos
por tramo y listas precalculadas, y no se le ha encontrado ningun tope dinamico). Todo medido en Flycast, NO en la
consola real (Flycast no da el tiempo de la PowerVR ni llena el TA como la consola).

#### P1. Tope de la etapa estable (`-DTOPE_FIJO`, `-DTOPE_RES_KB`)

DESVIACION (`demo/main.c`: `TOPE_FIJO`, `tope_fijo_paso`, `ta_inicia_cuadro`). Antes el tope del TA de la etapa salia
cada cuadro de lo que gasto el anterior (2D, coche, pareja, trafico y efectos + 2 coches de trafico con LOD 0) y de
`salto_ta`, interpolado con el nivel del salto (que se mueve un poco en cada cuadro): cambiaba unas 19-38 veces por
segundo (medido, linea `POPTOPE` de `-DDIAG_POPPING`), y el reparto quitaba y devolvia clases (razon "tope", 2.9.5).
Con `TOPE_FIJO` (1 por defecto):
- La reserva de lo que no es la etapa y la de lo 2D son fijas por etapa (y por union): el mayor valor visto en ella,
  en pasos de 16 KB, como minimo `TOPE_RES_KB` (0 por defecto). Sube en el acto si hace falta (el coche y el trafico
  van despues de la etapa; si no cupieran se cortarian por `ta_lim`) y solo baja al cruzar una seccion de la PVS, si
  en las ultimas `TOPE_BAJA_SEC` (16) secciones no ha pasado del `TOPE_BAJA_PCT` (90) % de ella (`TOPE_MARGEN`: margen
  opcional de 1/n; 0).
- `salto_ta` va en los 4 escalones de `SALTO_TA` (256, 240, 222, 205), sin interpolar, y solo cambia de escalon al
  cruzar una seccion de la PVS (o cada `TOPE_SIN_SEC`, 60, cuadros sin cambiar de seccion), con histeresis: sube al
  llegar el nivel al escalon siguiente y baja si queda medio escalon por debajo.
- Los objetos de la union (`bo`), `ta_freno` y `ta_cabe` siguen como antes; el tope total del TA no cambia.
- `-DTOPE_FIJO=0`: lo de antes. `-DDIAG_POPPING` anade la linea `POPTOPE:` (cambios del tope por s, los que no
  coinciden con un cruce de seccion, minimo y maximo, reservas y `salto_ta`).

Medida: `-DPRUEBA_AUTO -DPRUEBA_NODO=n -DPRUEBA_SIN_TIEMPO -DDIAG_MS -DDIAG_POPPING` con `-DTOPE_FIJO=0` y `=1` (el
mismo codigo), datos lgdat2 (sin datos nuevos), 140 s con el piloto automatico (paso 10), tres Flycast a la vez.
Saltos por s de 8 px o mas / de 32 px o mas (todas las razones; "tope" aparte), solo las lineas de la zona del nodo.
Valores definitivos (reserva en pasos de 16 KB, bajada tras 16 secciones al 90 %):

| Etapa | TOPE_FIJO | fps | tarde (%) | 8 px+ | 32 px+ | tope +8/32 -8/32 | cambios del tope / s | reserva media |
|---|---|---|---|---|---|---|---|---|
| GHOS (9) | 0 | 37,46 | 113 (4,7) | 22,9 | 3,5 | +10,5/0,8 -3,8/0,1 | 37,9 | (variable) |
| GHOS (9) | 1 | 37,58 | 105 (4,3) | 21,7 | 3,2 | +9,9/0,7 -3,5/0,1 | 0,4 | 817 KB |
| METR (11) | 0 | 33,83 | 337 (13,4) | 22,2 | 6,9 | +8,0/1,7 -2,1/0,3 | 33,1 | (variable) |
| METR (11) | 1 | 34,08 | 290 (12,4) | 20,9 | 5,3 | +8,3/1,6 -2,2/0,4 | 0,7 | 863 KB |
| PALM (0) | 0 | 37,44 | 120 (4,7) | 25,4 | 5,6 | +16,0/4,1 -4,8/0,5 | 19,2 | (variable) |
| PALM (0) | 1 | 37,42 | 123 (4,8) | 26,9 | 6,0 | +16,8/4,5 -4,7/0,4 | 0,7 | 817 KB |
| INDU (7) | 0 | 37,99 | 121 (3,6) | 21,0 | 5,2 | +8,6/1,4 -4,7/0,5 | 18,8 | (variable) |
| INDU (7) | 1 | 37,99 | 119 (3,5) | 20,0 | 5,0 | +8,2/1,4 -4,5/0,4 | 1,2 | 791 KB |
| Suma de las 4 | 0 | | | 91,5 | 21,2 | | | |
| Suma de las 4 | 1 | | | 89,5 | 19,5 | | | |

- Medido: el tope ya no cambia en cada cuadro (de 19-38 cambios por s a 0,4-1,2). El total de saltos baja un 2 % en
  8 px (91,5 -> 89,5) y un 8 % en 32 px (21,2 -> 19,5); por etapa baja en GHOS, METR e INDU y sube en PALM (+1,5 / +0,4).
  fps: +0,12, +0,25, -0,02 y 0,00; cuadros tarde: -7 %, -14 %, +2,5 % y -2 %. Criterio (bajar el total de 8 px o de
  32 px sin perder mas de 0,3 fps ni subir mas de un 10 % los cuadros tarde en ninguna): cumplido, queda ACTIVO.
- Primera version (descartada): reserva con 1/16 de margen y bajada solo tras 64 secciones por debajo del 80 %. En PALM
  la reserva de la salida (1096 KB) tardaba en bajar; suma 90,4 -> 90,8 (8 px) y 19,6 -> 20,6 (32 px), PALM 24,5 ->
  27,7. No cumplia.
- Ruido: con `TOPE_FIJO=0` las dos tandas dan 23,3 / 22,9 (GHOS), 22,3 / 22,2 (METR), 24,5 / 25,4 (PALM) y 20,3 /
  21,0 (INDU) saltos de 8 px por s: la carrera no se repite del todo (el trafico y la razon "detras" cambian). La
  mejora de la suma (-2,0 en 8 px) es del orden de ese ruido, y casi toda la de 32 px (-1,7) viene de la razon
  "detras" de METR (+2,1/1,5 por s con `TOPE_FIJO=0`, que la otra tanda y `TOPE_FIJO=1` no tienen): sin ella las sumas
  quedan 89,4 / 19,7 frente a 89,5 / 19,5, practicamente iguales. Se deja activo porque cumple el criterio, el tope es
  estable de verdad (la columna de cambios) y fps y cuadros tarde no empeoran, pero la mejora del popping en Flycast es
  marginal. SUPUESTO: en la consola
  real, donde el TA si limita, el tope estable deberia notarse mas; sin medir.
- La razon "tope" casi no cambia (desapariciones de 8 px: 3,8 -> 3,5, 2,1 -> 2,2, 4,8 -> 4,7 y 4,7 -> 4,5 por s): la
  estimacion de la propuesta (un tercio a la mitad menos) no se cumple en Flycast; lo que baja es sobre todo lo de 32 px
  de otras razones. Las apariciones del "tope" al acercarse siguen (el reparto sigue cortando por coste): P2/P3.
- Ficheros de la medida: `m298/p1/` del directorio de trabajo (medidas.txt, res/flycast_p1b_<nodo>_<0|1>.txt).

#### P2. Corte del reparto precalculado por seccion (`-DREPARTO_SEC`, apagado por defecto)

DESVIACION (`tools/reparto_sec.py`, nuevo; `tools/regenera.py`, paso `repsec`; `demo/main.c`: `REPARTO_SEC`,
`repsec_lee`, `rsec_corte`, `prio_sube_sec`, `nodos_visibles_todos`). Tecnica de Crazy Taxi (el conjunto dibujado es
una funcion de la posicion, decidida fuera de linea para un presupuesto fijo): solo la idea, nada de su codigo ni de
sus datos. El original no tiene reparto del TA (lo dibuja todo).
- Herramienta: lee los SEGnn.DAT ya convertidos (sueltos o recompuestos de SEGS.PAK; no regenera nada ni toca ningun
  fichero) y escribe REPSEC.DAT ('RSC1': por etapa SEG, secciones y desplazamiento; 2 bytes con signo por seccion de la
  PVS: `prio_d` -1..47 y `prio_p` -1..63). Por cada punto de la ruta de la seccion, una camara de persecucion nominal
  (6 m detras, 2,5 m arriba, horizontal, fov 50) y lo que deja ver la PVS de la seccion y de s -+ 3 (el mayor coste de
  los tres, como `pvs_coste`), repite lo de `nodos_visibles` (lejos, cercano, laterales, LOD de 404B00, lejanos de
  `lod_veg.py` / `lod_grande.py`, nodos cercanos enteros, clases con el coste por vertices, grupos) y el reparto de
  `nodos_visibles_todos` con un tope nominal (800 KB al 95 %, k = ta_f 1,75 x alfa). No repite la niebla, la
  histeresis por nodo, la gracia de la PVS, `decor_reduce` ni `PVS_GRANDE`. El corte es el menor de los puntos de la
  seccion y luego el menor de `--suave` secciones a cada lado. 61 etapas, 79 KB (en la RAM solo las 15 del arbol: 19 KB,
  como VISO.DAT); 48 s.
- Demo, con `REPARTO_SEC` 1 y la tabla de la etapa (no en la union; la cuenta de secciones tiene que coincidir): el
  corte de la seccion del ojo es el objetivo si lo que dibuja cabe en el tope entero (el 100 % de `REPARTO_SCHMITT`);
  si no cabe (emergencia) el reparto de siempre sin pasar de la tabla. Con la tabla no se usa la histeresis de
  `REPARTO_SCHMITT`; el corte sube hacia la tabla una clase cada `REPARTO_SEC_SUBE` (4) cuadros y baja en el acto;
  `prio_ajusta` (cuadro que no cabe de verdad) sigue igual. Sin REPSEC.DAT, o con `REPARTO_SEC` 0, lo de antes (con 0 y
  sin `-DDIAG_POPPING` ni se lee el fichero). `-DDIAG_POPPING`: linea `POPSEC:` por s (cuadros con tabla, emergencias,
  cuadros con el corte igual a la tabla, cambios de la tabla, corte de siempre menos tabla, lo pedido y el tope).
- Calibracion (medida): con alfa 0,75 (el `prio_alfa` de carrera) lo pedido fuera de linea salia ~1,4 veces menor
  que en carrera y la tabla quedaba 8-9 clases de distancia y 4-13 de LOD por encima del corte de siempre (GHOS, METR,
  PALM). Se midieron dos tablas: v1 (alfa 1,10, suave 1) y v2 (alfa 1,00, suave 8; la que dan los valores por defecto).
- Datos nuevos: REPSEC.DAT (en d298 del directorio de trabajo, la v2; las tres en m298/P2/tablas). Es opcional: no
  esta en la comprobacion de `regenera.py`.

Medida: `-DPRUEBA_AUTO -DPRUEBA_NODO=n -DPRUEBA_SIN_TIEMPO -DDIAG_MS -DDIAG_POPPING` con `-DREPARTO_SEC=0` y `=1` (el
mismo codigo y los mismos datos d298 con REPSEC.DAT; con 0 la tabla solo se mide), 140 s con el piloto automatico
(paso 10), tres Flycast a la vez. Las de 0 son una tanda y las de 1 otra (v2, la tercera). "corte = tabla": cuadros
con tabla en los que el corte final es el de la tabla; "emerg.": cuadros con tabla en los que no cabe.

| Etapa | SEC | fps | tarde (%) | 8 px+ | 32 px+ | tope +8/32 -8/32 | corte = tabla | emerg. | cambios de la tabla / s | siempre - tabla (d / p) |
|---|---|---|---|---|---|---|---|---|---|---|
| GHOS (9) | 0 | 36,56 | 191 (6,4) | 18,3 | 2,8 | +8,5/0,6 -2,8/0,0 | 0 % | - | 0,67 | -9,0 / -12,6 |
| GHOS (9) | 1 | 37,04 | 159 (5,2) | 20,2 | 3,9 | +11,9/1,9 -2,6/0,1 | 55 % | 9 % | 0,99 | -3,4 / +0,8 |
| METR (11) | 0 | 33,26 | 336 (13,7) | 22,9 | 7,0 | +8,1/1,7 -2,2/0,4 | 16 % | - | 1,77 | -8,9 / -4,4 |
| METR (11) | 1 | 34,29 | 261 (10,5) | 23,4 | 7,6 | +11,6/3,2 -1,9/0,6 | 57 % | 7 % | 1,25 | +7,2 / +8,7 |
| PALM (0) | 0 | 36,82 | 147 (5,4) | 26,3 | 5,4 | +15,3/3,8 -3,8/0,3 | 0 % | - | 1,62 | -7,7 / -8,4 |
| PALM (0) | 1 | 37,32 | 125 (4,5) | 23,9 | 8,4 | +17,9/7,0 -3,8/0,8 | 53 % | 13 % | 1,23 | +12,0 / +13,1 |
| INDU (7) | 0 | 37,45 | 131 (4,0) | 20,5 | 5,2 | +9,0/1,5 -4,5/0,5 | 13 % | - | 2,42 | -3,9 / -0,4 |
| INDU (7) | 1 | 37,70 | 122 (3,7) | 20,1 | 6,0 | +9,9/2,3 -3,2/0,5 | 49 % | 14 % | 1,28 | -1,0 / +3,4 |
| Suma de las 4 | 0 | | | 88,0 | 20,4 | | | | | |
| Suma de las 4 | 1 | | | 87,6 | 25,9 | | | | | |

(Con 0 la tabla solo se mide y las columnas de la tabla son de la que habia en los datos: la de calibracion, alfa 0,75
y suave 1, en GHOS, METR y PALM, y la v1 en INDU; con 1, "siempre - tabla" es el corte que pediria el reparto de
siempre en ese cuadro menos la tabla.)
- Medido: fps +0,48, +1,03, +0,50 y +0,25; cuadros tarde -17 %, -22 %, -15 % y -7 %. Los saltos de 8 px quedan igual
  (88,0 -> 87,6, dentro del ruido de 1-2 por etapa entre tandas, P1) y los de 32 px suben un 27 % (20,4 -> 25,9; PALM
  5,4 -> 8,4), casi todo apariciones del "tope" (+3,1 en 32 px en las cuatro): el corte de la tabla cambia al cruzar
  secciones (1-1,3 veces por s) y lo que entra, entra a la vez; con el reparto de siempre entraba de clase en clase.
  En METR y PALM la tabla queda 7-13 clases por debajo de lo que cabria (el lejano adaptativo, 1600-2300 m de media,
  quita lo lejano y la tabla, calculada con 3800 m, no lo sabe): se dibuja menos y de ahi salen los fps. En GHOS e
  INDU queda 1-3 por encima y hay emergencia en el 9-14 % de los cuadros.
- v1 (alfa 1,10, suave 1; descartada): suma 88,0 -> 89,4 (8 px) y 20,4 -> 24,8 (32 px), fps +0,31, +0,47, +0,40 y
  +0,13, la tabla cambiaba 1,8-2,4 veces por s.
- Criterio (bajar el total de 8 px o de 32 px sin perder mas de 0,3 fps ni subir mas de un 10 % los cuadros tarde en
  ninguna): a la letra la v2 lo cumple por 0,4 saltos de 8 px por s, menos que el ruido, mientras los de 32 px suben un
  27 % y en la mitad de los cuadros el corte no es el de la tabla. DECISION: queda APAGADO por defecto (`REPARTO_SEC`
  0): no baja el popping, que es para lo que se propuso; la mejora de fps es por dibujar menos en METR y PALM.
- SUPUESTO, sin medir: una tabla que supiera el lejano adaptativo (o que guardara el corte relativo a lo que hay que
  repartir) y que cambiara solo de una clase por seccion quedaria mas cerca del corte de siempre; en la consola real
  el tope nominal no se ha comparado con `ta_f` real.
- Todas las pruebas dan el "Out of memory ... diff 217088" de newlib al arrancar, el conocido.
- Ficheros: `m298/P2/` del directorio de trabajo (medidas.txt, res/, tablas/, sh/, py/sec.py).

#### P3. Coche trasero segun la camara (`-DCOCHE_TRASERO`, apagado por defecto)

DESVIACION (`tools/conv_coche.py`: `parte_trasera`, `camaras_trasero`, `comprueba_trasero`, `separa`; `demo/main.c`:
`NODO_TRASERO`, `coche_trasero_paso`, `sin_trasero`). Tecnica de Crazy Taxi (con la camara detras dibuja un cuerpo sin
el frontal): solo la idea, nada de su codigo ni de sus datos. El original dibuja el coche entero.
- Herramienta (con `--visible`, en el ligero COCHEnnL.DAT; `--sin-trasero`: como en la 2.9.7): los triangulos que no
  se ven desde ningun ojo del cono trasero van en tiras aparte con el bit `NODO_TRASERO` (0x20000) en el campo del
  nodo, al final de su lista y textura (no al final de la lista: `escribe` vuelve a ordenar por lista y textura y CEX2
  va por numero de malla; con las traseras al final de la lista la pintura, el reflejo y las luces caian en otras
  mallas, visto en las capturas congeladas de una primera version, descartada con sus medidas). Cono, en el marco del
  coche y desde su origen: guinada del ojo 180 +- 35 grados del morro, cabeceo de la camara -65..+5 (el ojo de 5
  grados por debajo a 65 por encima), alabeo +-35, ojo a 3 m o mas. Lo que se ve desde un ojo no depende de hacia donde
  mire la camara si el coche cabe en la imagen: camaras que miran al coche con el campo justo, a 3-60 m, cada 5 grados,
  alabeo 0 y +-35 en los bordes, por las 6 posturas de `poses()`, con el z-buffer por software de tools/c/visible.c a
  960x720. Despues: fuera del oculto el borde (el triangulo que comparte un vertice con uno visible) y 4 rondas de 240
  camaras y 4 posturas al azar (giro de ruedas, direccion, aleron, faros) a 1280x960; lo que asoma pasa a visible.
  `comprueba_trasero`: otras ~940 camaras x posturas al azar.
- Sin el borde se separaba el 4,6-6,8 % de los vertices de carrera, pero la comprobacion encontraba 1-8 triangulos
  traseros que asomaban (astillas de la carroceria entre los angulos de la rejilla), aun con las rondas; con el borde
  por aristas, 2,3 % y 5 que asomaban (modelo 9); con el borde por vertices queda en 0-5,2 % por coche (media 1,7 %):
  el 360 Spider (ficha 0) 7456 -> 7351, la ficha 14 7179 -> 6805 (-5,2 %), la 12 -4,5 %, la 11 -3,5 %, la ficha 9 (la de
  las pruebas automaticas) 0 (su COCHE09L.DAT sale igual que antes). Comprobacion: 0 triangulos traseros que ganan un
  pixel en 13 coches y 1 en dos (fichas 6 y 13). Lo que solo se ve desde el cono es poco: el vaciado de la 2.9.6/2.9.7
  ya quito lo que no se ve desde ninguna camara, y desde 65 grados por encima se ve casi todo el frontal.
- Demo: `coche_trasero_paso` (una vez por cuadro, tras `pon_coche`): el ojo en el marco de `mod_coche`, guinada,
  cabeceo y alabeo (el arriba del coche frente al de la pantalla) y distancia. Histeresis de 4 grados (Crazy Taxi no
  tiene): se entra con 4 grados dentro del cono (y 0,25 m mas lejos) y se sale al salir del cono comprobado. Solo donde
  `coche_sin_cerca` (carrera, sin la escena de la salida ni la de la meta, sin mezcla de vistas, no en los finales) y
  en las vistas exteriores; nunca en la vista interior, el salon ni la seleccion (COCHEnn.DAT, sin el bit). El coche no
  se dibuja en ningun reflejo (el reflejo es el mapa de entorno de la pintura, que sigue igual) y la sombra no cambia.
  `-DDIAG_POPPING`: linea `POPTRAS:` (cuadros dentro del cono, cambios, vertices traseros) y con `-DDIAG_CONGELA` la
  linea `TRASCONG:` con los angulos del cuadro congelado. Sin el bit (los ficheros de antes) no cambia nada.
- Datos nuevos: los 15 COCHEnnL.DAT (en el directorio de trabajo, d298 y m298/P3/coches); COCHE09L.DAT igual que antes.

Medida: `-DPRUEBA_AUTO -DPRUEBA_NODO=n -DPRUEBA_SIN_TIEMPO -DDIAG_MS -DDIAG_POPPING -DPRUEBA_FICHA=14` con
`-DCOCHE_TRASERO=0` y `=1` (el mismo codigo y los mismos datos d298), 140 s con el piloto automatico (paso 10), tres
Flycast a la vez. La ficha 14 es la que mas separa; con la ficha 9 de siempre no hay nada que separar (tanda aparte:
sirve de medida del ruido). "TA coche": KB por cuadro del coche (POPTA).

| Etapa | TRASERO | fps | tarde (%) | 8 px+ | 32 px+ | tope +8/32 -8/32 | en el cono | TA coche |
|---|---|---|---|---|---|---|---|---|
| GHOS (9) | 0 | 37,41 | 109 (4,5) | 21,5 | 3,2 | +10,1/0,8 -3,4/0,1 | - | 397 KB |
| GHOS (9) | 1 | 37,28 | 116 (4,8) | 22,9 | 3,3 | +11,0/1,0 -3,9/0,0 | 100 % | 372 KB |
| METR (11) | 0 | 34,57 | 235 (11,0) | 20,0 | 5,3 | +7,3/1,6 -2,2/0,4 | - | 397 KB |
| METR (11) | 1 | 34,47 | 241 (11,2) | 21,1 | 6,0 | +7,7/2,0 -2,5/0,5 | 100 % | 372 KB |
| PALM (0) | 0 | 37,99 | 73 (3,1) | 27,9 | 6,3 | +17,5/4,5 -3,2/0,5 | - | 394 KB |
| PALM (0) | 1 | 37,52 | 104 (4,3) | 25,9 | 5,6 | +17,6/4,4 -3,4/0,3 | 100 % | 369 KB |
| INDU (7) | 0 | 38,11 | 83 (3,0) | 24,2 | 6,2 | +10,3/1,6 -5,8/0,6 | - | 394 KB |
| INDU (7) | 1 | 38,04 | 78 (3,2) | 25,2 | 6,8 | +10,2/1,7 -6,0/0,7 | 100 % | 369 KB |
| Suma de las 4 | 0 | | | 93,6 | 21,0 | | | |
| Suma de las 4 | 1 | | | 95,1 | 21,7 | | | |

- Medido: con la camara de persecucion el ojo esta siempre en el cono (100 % de los cuadros de la zona, 0 cambios; en
  los cuadros congelados guinada 0,0-0,3, cabeceo -20,4 / -28,9, alabeo 0, 5,9-6,2 m) y se dejan de enviar 410
  vertices por cuadro: el TA del coche baja 25 KB por cuadro (-6 %). Pero la suma de saltos sube (8 px 93,6 -> 95,1, 32
  px 21,0 -> 21,7), los fps bajan en las cuatro (-0,13, -0,10, -0,47 y -0,07) y los cuadros tarde suben un 42 % en
  PALM (73 -> 104) y un 6 % en GHOS. Criterio (bajar el total de 8 px o de 32 px sin perder mas de 0,3 fps ni subir
  mas de un 10 % los cuadros tarde en ninguna): NO se cumple. Queda APAGADO por defecto (`COCHE_TRASERO` 0).
- Ruido (tanda con la ficha 9, sin mallas traseras: las dos compilaciones hacen lo mismo): sumas 89,3 / 89,6 (8 px) y
  19,2 / 19,2 (32 px); fps -0,26, -0,14, -0,13 y -0,01; cuadros tarde +10 %, +2 %, +6 % y +2 %. Las diferencias de la
  ficha 14 son de ese orden salvo PALM (-0,47 fps, +42 % tarde; su prueba con 0 salio con una desviacion de 1,59
  frente a 2,3-2,8 de las demas: probablemente es la de 0 la que salio buena). Lo que deja libre el coche (25 KB de
  unos 1000 por cuadro) lo vuelve a ocupar la etapa, como con la variante B de la 2.9.6.
- Capturas congeladas (`-DDIAG_CONGELA`, GHOS, ficha 14, en el cono): crono 2100, 0 pixeles distintos en el coche
  entre `COCHE_TRASERO` 1 y 0, y 0 pixeles entre los datos nuevos con 0 y los de antes (lgdat2) con 0 (el reparto en
  tiras no cambia la imagen); fuera del coche 1730 que tambien cambian entre dos capturas de la misma prueba. Crono 3300:
  0 pixeles en toda la captura en las tres comparaciones.
  En el crono 900 la lluvia y el humo cambian la imagen entre capturas de la misma prueba (25-30 mil pixeles en la
  caja del coche): no sirve. No se han hecho capturas en derrape fuerte ni en el borde del cono: el piloto no sale del
  cono; lo del borde lo cubre la comprobacion por software.
- SUPUESTO, sin medir: en la consola real, si el TA limita, 25 KB menos por cuadro podrian notarse algo.
- Ficheros: `m298/P3/` del directorio de trabajo (medidas.txt, res/, cong/, coches/, conv/ y v1_orden_roto/).

#### P4. Subida de texturas por DMA (`-DSUBE_DMA`, apagado por defecto)

DESVIACION (`demo/main.c`: `SUBE_DMA`, `tex_carga`, `sdma_lanza`, `sdma_hecho`, `sdma_espera`). Tecnica de Crazy Taxi
(sube las texturas por el DMA del canal 2 por la ruta de texturas del TA cuando origen, destino y tamano son multiplos
de 32): solo la idea, nada de su codigo ni de sus datos. Con `SUBE_DMA` 1 las texturas de `sube_trozo` (en carrera, 32 KB
por cuadro, y de una vez en las cargas de etapa y union de `sube_tramo`), `sube_modelo` y `tex_rebaja` van por
`pvr_txr_load_dma` en vez de por las Store Queues (`pvr_txr_load`):
- Alineacion: no hace falta tocar las herramientas. `tools/modelo.py` ya escribe cada textura del blob en multiplo de 32
  y con el tamano en multiplo de 32; el blob va a 32 dentro de su bufer (`memalign(32)`), `vram_pide` y `pvr_mem_malloc`
  dan direcciones de 32 y los trozos de una textura grande van en multiplos de 32. Medido: 0 subidas por SQ en carrera
  con `SUBE_DMA` 1 (linea `SUBE`, columna `sqK`); lo que no este alineado iria por SQ.
- Cola de 64 subidas: la interrupcion del fin de cada DMA de la PVR lanza la siguiente (el DMA de la PVR va de una en
  una) y la CPU sigue con el cuadro. `sdma_espera` vacia la cola antes de soltar un origen o una textura (`tex_suelta`,
  `compacta_modelo_de`, `carga_pide`, `tex_muda`), antes de dar al modelo sus tablas (fin de `sube_trozo` y de
  `sube_modelo`) y antes de cualquier subida por SQ de estas funciones. KOS vacia la cache del origen (`dma_map_src`).
  La PVR va con `dma_enabled` 0: nadie mas usa el canal 2 (con P7 habria que poner la subida en cola con los vertices).
- `-DDIAG_MS`: linea `SUBE:` cada 30 cuadros (trozos, us dentro de `sube_trozo`, el trozo mayor, us esperando a la
  cola, DMAs, KB por DMA y por SQ). `-DDIAG_SUBE_CMP`: al acabar cada subida compara lo leido de la VRAM con el blob
  (linea `SUBECMP:`).
- Con `SUBE_DMA` 0 todo va como antes (las esperas no hacen nada). Datos: ninguno nuevo (lgdat2).

Medida: `-DPRUEBA_AUTO -DPRUEBA_NODO=n -DPRUEBA_SIN_TIEMPO -DDIAG_MS -DDIAG_POPPING` con `-DSUBE_DMA=0` y `=1` (el
mismo codigo y los mismos datos lgdat2), 140 s con el piloto automatico (paso 10), dos tandas (la segunda con los
carriles cambiados), dos o tres Flycast a la vez. La tabla es la media de las dos tandas (tarde: suma de las dos y
media de los porcentajes). "us subida": us de CPU dentro de `sube_trozo` en carrera en la prueba (lo mismo en las dos
tandas salvo PALM con 0 en la segunda, que no llego a subir ALPI), y entre parentesis el trozo mayor.

| Etapa | SUBE_DMA | fps | tarde (%) | 8 px+ | 32 px+ | us subida (trozo max) |
|---|---|---|---|---|---|---|
| GHOS (9) | 0 | 36,24 | 241 (6,4) | 27,2 | 3,7 | 8497 (6975) |
| GHOS (9) | 1 | 36,25 | 233 (6,4) | 28,4 | 4,0 | 1755 (1041) |
| METR (11) | 0 | 33,45 | 553 (12,9) | 19,7 | 5,3 | 3491 (1791) |
| METR (11) | 1 | 33,84 | 491 (12,2) | 20,9 | 5,8 | 3429 (1810) |
| PALM (0) | 0 | 36,72 | 258 (5,1) | 28,5 | 6,0 | 11504 (7791) |
| PALM (0) | 1 | 36,73 | 241 (4,9) | 26,9 | 5,2 | 3800 (684) |
| INDU (7) | 0 | 37,25 | 186 (4,3) | 24,4 | 6,4 | 2193 (1071) |
| INDU (7) | 1 | 37,39 | 169 (3,8) | 25,9 | 6,8 | 2024 (1042) |
| Suma de las 4 | 0 | | | 99,7 | 21,3 | |
| Suma de las 4 | 1 | | | 102,1 | 21,7 | |

- Por tanda: la primera da sumas 95,1 -> 100,3 (8 px) y 21,3 -> 21,2 (32 px); la segunda 104,3 -> 103,8 y 21,2 ->
  22,1. Entre las dos tandas con el mismo valor la suma de 8 px cambia 9 por s (95,1 / 104,3): las diferencias de
  la tabla son ruido. El cambio solo actua en 1-5 ventanas de 30 cuadros de unas 70-90 por etapa (las de subida) y no
  toca lo que se dibuja: no puede mover el popping.
- Criterio (bajar el total de 8 px o de 32 px sin perder mas de 0,3 fps ni subir mas de un 10 % los cuadros tarde en
  ninguna): con la media de las dos tandas NO se cumple (los dos totales suben, dentro del ruido); los fps no bajan en
  ninguna (+0,01, +0,39, +0,01 y +0,14) y los cuadros tarde bajan en las cuatro. A la letra, la primera tanda sola
  pasaria (32 px -0,1) y la segunda no (PALM +18 % de cuadros tarde). Queda APAGADO por defecto (`SUBE_DMA` 0); poner
  1 no cambia la imagen y quita CPU en las subidas.
- Tramos de union (medido en Flycast, todas las pruebas de las dos tandas y LASV -> MAYA con `-DPRUEBA_CURSO=1
  -DPRUEBA_NODO=7 -DPRUEBA_RAMA=0 -DPRUEBA_RAMA_FIJA=0`, 230 s): en las ventanas con subida la CPU de `sube_trozo` baja
  de 24,8 a 9,9 us por KB (51,9 ms para 2099 KB con 0, 25,4 ms para 2570 KB con 1) y el trozo mayor de 7791 a 1810 us;
  esas ventanas pasan de 32,16 a 32,70 fps, de 29,91 a 29,23 ms de CPU (dib + log) y de 4,13 a 3,32 cuadros tarde por
  ventana (15 y 19 ventanas: pocas muestras). PALM -> union (141 KB en 5 trozos): 1171 -> 998 us; la subida de ALPI en la
  union (416 / 473 KB): 1955 + 8254 -> 82 + 2611 us. LASV -> union (141 KB): 1577 -> 1400 us; la de MAYA en la union
  (535 KB, 17 trozos): 9106 us (el mayor 6916) -> 1885 us (el mayor 395). La carga de la etapa de salida de una vez
  (1088-1122 KB): 13642 -> 13353 us y 13373 -> 12873 us (de una vez la CPU espera a la cola). La espera medida en
  `sdma_espera` es 0 us en todas: en Flycast el DMA acaba enseguida.
- Texturas identicas: `-DDIAG_SUBE_CMP` en 6 pruebas de PALM -> union -> ALPI (36 subidas, 3060 texturas): 0 distintas
  entre la VRAM y el blob, con 0 y con 1. Capturas congeladas (`-DDIAG_CONGELA`, PALM) en los cronos 1800 y 2300
  (PALM), 3900 (union, con ALPI ya subida) y 6600 (ALPI): 0, 0, 3 y 0 pixeles distintos entre 1 y 0 de 786432; los 3 del
  crono 3900 tambien cambian entre dos capturas de la misma prueba con 1 (ruido).
- Las pruebas dan el "Out of memory ... diff 217088" de newlib al arrancar, el ya conocido.
- SUPUESTO, sin medir: en la consola real el DMA corre en paralelo con la CPU, y la copia por SQ de 32 KB cuesta del
  orden de 0,1-0,3 ms; la ganancia seria de ese orden por cuadro de subida. No se ha comprobado en la consola que la
  subida por DMA a la ruta de texturas del TA mientras se envian vertices por SQ al TA no de problemas.
- Ficheros: `m298/P4/` del directorio de trabajo (medidas.txt, union.txt, res/, cong/, sh/, py/sube.py, py/tabla.py,
  py/difcong.py).

#### P5. Lejano por seccion, solo hacia abajo (`-DLEJ_SEC`, apagado por defecto)

DESVIACION (`tools/reparto_sec.py` `lejanos`; `demo/main.c`: `LEJ_SEC`, `LEJ_SEC_MARGEN`, `LEJ_SEC_MIN`, `lej_base`,
`lsec_objetivo`, `lejsec_paso`, `repsec_lee`). Tecnica de Crazy Taxi (un lejano distinto por tramo): solo la idea, nada
de su codigo ni de sus datos.
- Fuera de linea, en el mismo fichero de P2 (REPSEC.DAT, ahora 'RSC2': entradas de 16 bytes con el desplazamiento del
  lejano; 'RSC1' se sigue leyendo; el corte de P2 sale identico byte a byte en las 61 etapas): por seccion de la PVS un
  u16 con la mayor distancia de un punto de la ruta de la seccion a la cara de atras de la esfera de cada malla que la
  PVS deja ver desde la seccion o sus dos vecinas (|c - p| + r: lo grande, el mar y los montes por su esfera), mas las
  mallas que no son de un nodo y las de la version lejana de un nodo visible (el cielo, lista 3, no lo corta el lejano).
  Despues se adelanta lo que hara falta mas adelante: lej[s] = max(lej[s], lej[k] - 1,25 x (ruta de s a k)), para que el
  lejano, que sube 16 m cada 4 cuadros, llegue a tiempo a una seccion abierta yendo a unos 120 m/s (`--ritmo`). 61
  etapas, 158 KB, 85 s.
- En la demo, solo en la etapa (no en la union), en CARRERA y con el ojo dentro de lo que muestrea la PVS: objetivo =
  lej[seccion del ojo] + `LEJ_SEC_MARGEN` (200 m), como poco `LEJ_SEC_MIN` (400) y, con la union o la etapa siguiente en
  la vista, como poco lo mas lejano de sus nodos. Se sigue con `SALTO_LEJ_BAJA` / `SALTO_LEJ_SUBE` cada 4 cuadros y
  lejos = min(lejano del salto, ese). Nunca sube el lejano.
- La franja del limite (`NLIM_FRANJA`) va con el lejano cortado; el velo tenue (`NLIM_INI`, `NLIM_ANCHO`), el recorte de
  la franja a 0,55 y el LOD de lo grande (`LOD_GRANDE_FR`) siguen con el lejano del salto (`lej_base`), para no cambiar lo
  de delante.
- `-DDIAG_POPPING`: linea `POPLEJ:` por segundo (cuadros, cuantos corta, objetivo, seccion, salto, lejos).
  `-DDIAG_NLIM`: solo la franja del limite, en magenta y a toda fuerza (sin la niebla del original ni el velo).
- Con `LEJ_SEC` 0 todo va como antes. Datos nuevos: solo REPSEC.DAT 'RSC2' en d298 (sustituye a la v2 de P2).

Lejano de la seccion (salida de la herramienta, con el adelanto): PALM 3477 m de media (2854..4063), GHOS 1805
(1200..2468), METR 2604 (1944..3351), INDU 2333 (1783..3025), CAST 1848 (1231..2623), RUIN 1601 (1167..2068). En CAST lo
que lo fija son los montes y los lejanos lowpoly de lo grande a 1-2,5 km que la PVS deja ver, no los 550 m de la
distancia efectiva que se estimaba.

Medida: `-DPRUEBA_AUTO -DPRUEBA_NODO=n -DPRUEBA_SIN_TIEMPO -DDIAG_MS -DDIAG_POPPING` con `-DLEJ_SEC=0` y `=1` (el mismo
codigo y los mismos datos d298), 140 s con el piloto (paso 10), dos tandas con los carriles cambiados, tres Flycast a la
vez (:92, :93, :94). La tabla es la media de las dos tandas (tarde: la suma). "corta": cuadros en que el lejano de la
seccion queda por debajo del del salto.

| Etapa | LEJ_SEC | fps | tarde (%) | 8 px+ | 32 px+ | lejos +8/32 -8/32 | lejano medio | corta |
|---|---|---|---|---|---|---|---|---|
| GHOS (9) | 0 | 36,38 | 244 (6,0) | 26,5 | 3,65 | +0,0/0,0 -0,8/0,0 | 1702 | - |
| GHOS (9) | 1 | 36,27 | 257 (6,1) | 25,2 | 3,40 | +0,0/0,0 -0,8/0,0 | 1613 | 24 % |
| METR (11) | 0 | 32,74 | 743 (14,6) | 19,1 | 5,20 | +0,7/0,3 -0,6/0,2 | 1880 | - |
| METR (11) | 1 | 32,05 | 901 (16,7) | 18,4 | 5,15 | +0,6/0,2 -0,5/0,2 | 1752 | 21 % |
| PALM (0) | 0 | 36,56 | 300 (5,6) | 26,7 | 5,90 | +1,6/0,3 -2,9/0,4 | 1632 | - |
| PALM (0) | 1 | 36,38 | 289 (5,7) | 27,0 | 5,35 | +1,4/0,3 -2,8/0,3 | 1653 | 0 % |
| INDU (7) | 0 | 37,38 | 200 (3,9) | 25,2 | 6,60 | +0,0/0,0 -0,2/0,0 | 2055 | - |
| INDU (7) | 1 | 37,15 | 205 (4,1) | 24,0 | 6,50 | +0,0/0,0 -0,2/0,0 | 1833 | 29 % |
| Suma de las 4 | 0 | | | 97,4 | 21,35 | | | |
| Suma de las 4 | 1 | | | 94,5 | 20,40 | | | |

- Criterio: NO se cumple. Las sumas bajan (8 px -3,0 %, 32 px -4,4 %), pero METR pierde 0,69 fps y sube un 21 % los
  cuadros tarde (primera tanda -1,28 fps y +55 %; segunda -0,10 y +1 %). Queda APAGADO por defecto (`LEJ_SEC` 0).
- Es ruido: entre las dos tandas con el mismo valor la suma de 8 px cambia 8,6 por s (101,7 / 93,1). En PALM el lejano
  del salto ya esta por debajo del de la seccion en todos los cuadros (corta el 0 %) y aun asi la prueba con 1 sale
  distinta. La razon "lejos" no se mueve (0,0-1,6 por s en las dos): lo que corta el lejano de la seccion la PVS ya no
  lo dejaba ver.
- Cuando corta, poco: 70-240 m de media (hasta 1309 m en INDU). En las etapas pesadas el salto ya lleva el lejano a
  1200-2000 m (GHOS 1649-1755, METR 1835-1925 de media), por debajo de lo que pide la seccion; solo corta al empezar la
  etapa o tras bajar el nivel. Los fps no suben en ninguna (-0,10 a -0,69).
- Capturas congeladas (`-DDIAG_CONGELA`, el salto fijo en 0 con `-DSALTO_OBS=0`, lejos 3800 frente a 1592 en CAST crono
  900 y 2397 en METR crono 600; tambien con el salto libre en CAST, 2100 frente a 1592). Con `-DDIAG_NLIM` hay 0 pixeles
  magenta con 0 y con 1: la franja cae en las dos sobre nada visible (con 1 de 1353 a 1592 m en CAST y de 2037 a 2397 en
  METR). Entre 0 y 1 hay 0 pixeles distintos en la escena; solo cambian 196, el cuadro de diagnostico que pinta el
  lejano en color. La imagen no cambia.
- Las pruebas dan el "Out of memory ... diff 217088" de newlib al arrancar, el ya conocido.
- SUPUESTO, sin medir: en la consola real, donde el lejano si ahorra por debajo de ~1000 m, solo se notaria en las
  secciones de lejano corto (RUIN, CAST, FORE: 1167-1431 m como minimo), y solo con el salto por encima.
- Ficheros: `m298/P5/` del directorio de trabajo (medidas.txt, res/, cong/, tablas/, sh/, py/lej.py, py/tabla.py,
  py/quien.py).

#### Medida final de la Rev 2.9.8 (las 15 etapas; `-DDIAG_POP298`)

Medida final de la Rev 2.9.8 en las 15 etapas de m298/etapas.txt, todo en Flycast (NO en la consola). Flags de la
referencia: -DPRUEBA_AUTO -DPRUEBA_NODO=n -DPRUEBA_SIN_TIEMPO -DDIAG_MS -DDIAG_POPPING; 140 s de piloto automatico,
paso 10; mide.py (fps294) y pop.py (np295), solo las lineas de la zona del nodo.

**Rondas con la misma carga (la tabla que cuenta).** Por cada nodo, tres Flycast a la vez (:90, :91, :92):
A = 060dc9a (Rev 2.9.7) con lgdat2 (la referencia rehecha), B = la 2.9.8 por defecto con d298, C = la 2.9.8 con
lgdat2 (para separar los datos nuevos del codigo). La referencia de m298/ (2 Flycast a la vez) va en la columna "fps
ref": con 3 Flycast el anfitrion (4 nucleos) va mas cargado y no es comparable cuadro a cuadro.

| Etapa (nodo) | fps A | fps B | B-A | fps C | tarde % A / B / C | salto A / B / C | lejano A / B / C | 8 px+ A / B / C | 32 px+ A / B / C | fps ref (2 Fc) | fps fin (3 Fc) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| PALM (0) | 37,74 | 37,77 | +0,03 | 37,56 | 4,3 / 4,2 / 4,6 | 7,48 / 7,61 / 7,67 | 1779 / 1721 / 1702 | 22,6 / 24,8 / 25,8 | 4,8 / 4,9 / 5,3 | 37,73 | 36,56 |
| LAKE (1) | 39,54 | 39,49 | -0,05 | 39,35 | 0,7 / 0,7 / 1,0 | 2,25 / 2,13 / 2,13 | 3651 / 3725 / 3729 | 6,3 / 6,1 / 6,3 | 1,3 / 1,4 / 1,4 | 38,78 | 37,70 |
| ALPI (2) | 39,47 | 39,37 | -0,10 | 39,39 | 0,7 / 0,9 / 0,9 | 0,57 / 0,93 / 0,89 | 3800 / 3800 / 3800 | 8,4 / 9,0 / 9,0 | 1,9 / 1,7 / 1,8 | 39,41 | 38,07 |
| CAST (3) | 39,34 | 39,23 | -0,11 | 39,18 | 1,0 / 1,2 / 1,3 | 3,90 / 4,19 / 4,29 | 3128 / 3018 / 3007 | 17,8 / 18,0 / 17,9 | 5,3 / 5,2 / 5,4 | 38,70 | 38,30 |
| FORE (4) | 39,60 | 39,58 | -0,02 | 39,52 | 0,5 / 0,5 / 0,6 | 0,32 / 0,57 / 0,58 | 3800 / 3800 / 3800 | 12,0 / 12,2 / 11,8 | 2,5 / 2,5 / 2,4 | 39,10 | 38,38 |
| DESE (5) | 39,32 | 39,37 | +0,05 | 39,36 | 1,0 / 0,9 / 0,9 | 1,91 / 1,85 / 1,80 | 3561 / 3601 / 3610 | 9,5 / 9,4 / 9,7 | 3,0 / 2,9 / 3,2 | 38,97 | 38,13 |
| CLOU (6) | 36,99 | 36,98 | -0,01 | 36,93 | 6,2 / 6,2 / 6,4 | 7,53 / 7,45 / 7,54 | 1773 / 1806 / 1772 | 28,0 / 29,7 / 28,8 | 3,8 / 3,5 / 3,7 | 36,47 | 35,42 |
| INDU (7) | 38,48 | 38,41 | -0,07 | 38,47 | 2,6 / 2,6 / 2,5 | 5,69 / 5,72 / 5,82 | 2329 / 2317 / 2283 | 24,9 / 25,7 / 25,5 | 6,5 / 6,6 / 6,6 | 38,28 | 37,18 |
| SNOW (8) | 39,51 | 39,51 | +0,00 | 39,47 | 0,7 / 0,7 / 0,7 | 0,98 / 1,16 / 1,20 | 3800 / 3785 / 3778 | 3,5 / 3,4 / 3,3 | 1,3 / 1,3 / 1,2 | 38,95 | 38,04 |
| GHOS (9) | 37,48 | 37,62 | +0,14 | 37,58 | 4,8 / 4,4 / 4,5 | 7,80 / 7,68 / 7,77 | 1712 / 1744 / 1708 | 24,6 / 25,4 / 24,9 | 3,7 / 3,8 / 3,6 | 37,32 | 36,66 |
| TULI (10) | 38,73 | 38,60 | -0,13 | 38,62 | 2,1 / 2,4 / 2,3 | 6,28 / 6,38 / 6,32 | 2247 / 2207 / 2227 | 34,0 / 30,2 / 30,2 | 4,3 / 3,4 / 3,4 | 38,70 | 37,69 |
| METR (11) | 34,76 | 34,57 | -0,19 | 34,57 | 11,0 / 11,5 / 11,4 | 7,49 / 7,50 / 7,49 | 1823 / 1826 / 1827 | 20,4 / 19,3 / 19,6 | 5,9 / 5,2 / 5,3 | 33,30 | 33,24 |
| RUIN (12) | 39,03 | 39,08 | +0,05 | 39,06 | 1,6 / 1,5 / 1,6 | 2,84 / 3,09 / 3,07 | 3432 / 3424 / 3403 | 9,9 / 9,3 / 8,6 | 1,8 / 1,7 / 1,3 | 38,85 | 38,14 |
| IMPE (13) | 39,18 | 39,09 | -0,09 | 39,04 | 1,2 / 1,4 / 1,5 | 3,50 / 4,20 / 4,35 | 3264 / 2952 / 2897 | 18,3 / 17,4 / 18,7 | 4,5 / 4,0 / 3,9 | 39,06 | 38,10 |
| CAPE (14) | 39,42 | 39,35 | -0,07 | 39,40 | 0,8 / 1,0 / 0,8 | 1,20 / 1,63 / 1,53 | 3791 / 3768 / 3739 | 15,8 / 15,8 / 15,8 | 4,6 / 4,5 / 4,6 | 39,19 | 38,39 |
| **Total** | 38,57 | 38,53 | -0,04 | 38,50 | cuadros 946.0 / 950.0 / 993.0 | | | 256,0 / 255,7 / 255,9 | 55,2 / 52,6 / 53,1 | 38,19 | 37,33 |

Saltos sumados en las 15 etapas, por razon (por s, +8/32 apariciones, -8/32 desapariciones):

| razon | A (2.9.7) | B (2.9.8, d298) | C (2.9.8, lgdat2) |
|---|---|---|---|
| tope | +103,8/18,3 -39,9/3,6 | +102,2/17,2 -40,4/3,6 | +102,5/17,1 -40,6/3,6 |
| pvs | +94,2/28,2 -3,6/1,0 | +95,4/28,2 -3,6/1,0 | +94,7/28,2 -3,7/1,0 |
| lejos | +3,4/0,4 -3,0/0,4 | +3,5/0,4 -4,1/0,3 | +3,7/0,5 -4,5/0,5 |
| lado | +4,6/1,0 | +4,4/0,5 | +3,7/0,5 |
| detras | +1,4/0,5 | 0 | 0 |
| rama | +2,1/1,6 | +2,2/1,6 | +2,1/1,5 |
| lod, tramo | ~0 | ~0 | ~0 |

**Medido**
- Media de fps de las 15: A 38,57, B 38,53 (-0,04), C 38,50. Por etapa B-A va de -0,19 (METR) a +0,14 (GHOS);
  ninguna pierde mas de 0,3 fps. Cuadros tarde sumados: A 946, B 950, C 993.
- Saltos sumados: 8 px o mas A 256,0, B 255,7, C 255,9 (igual); 32 px o mas A 55,2, B 52,6 (-4,7 %), C 53,1. Lo de
  32 px que baja es sobre todo "detras" de METR/TULI (solo sale en A) y "lado"; "tope" baja algo (+18,3 a +17,2 en
  32 px) y "lejos" sube un poco en las desapariciones (-3,0 a -4,1 en 8 px, casi todo PALM).
- Mayor subida de 8 px: PALM 22,6 -> 24,8 (+9,7 %, justo por debajo del 10 %), de "lejos" (-0,6 -> -1,7) y "tope".
  Con lgdat2 (C) 25,8: no es de los datos nuevos. TULI baja 34,0 -> 30,2 y METR 20,4 -> 19,3.
- Salto medio: igual o un poco mas alto en B (IMPE 3,50 -> 4,20, CAPE 1,20 -> 1,63, ALPI 0,57 -> 0,93), lejano medio
  algo menor alli (IMPE 3264 -> 2952 m).
- Out of memory: las 45 pruebas de las rondas (y las 15 de la tanda suelta) dan solo el "Out of memory ... diff 217088"
  de newlib al arrancar, el ya conocido; ninguno nuevo. Ningun cuelgue: todas escriben lineas MS hasta el final de los
  140 s y avanzan lo mismo que A (s final de la etapa a +-6 %). Todas las etapas cargan (75-97 muestras MS por prueba).
  Ninguna linea "No se pud", panic, assert ni excepcion.

**Lo que encontro la medida: el diagnostico nuevo bajaba ~1 fps (corregido).** La primera tanda (punta de c298 tal
cual, d298, 3 Flycast en carriles) y la primera ronda A/B/C daban en TODAS las etapas ~1 fps menos (media 37,47 frente
a 38,56, de -0,84 a -1,39 por etapa), un 50 % mas de cuadros tarde (1450 frente a 928) y el salto mucho mas alto en las
ligeras (ALPI 0,56 -> 4,37, FORE 0,30 -> 4,96, SNOW 0,95 -> 4,96). Igual con d298 que con lgdat2.
- Sin -DDIAG_POPPING no habia diferencia (ALPI: 39,82 / 39,81 / 39,82 fps con 2.9.7, 2.9.8+lgdat2 y 2.9.8+d298).
- Por commits (ALPI, todos con -DTOPE_FIJO=0): 060dc9a 39,45, P1 39,22, P1+P3 38,90, P1..P4 38,40: cada P sumaba un poco.
  TOPE_FIJO=0 en la punta no lo arreglaba (38,26).
- La causa: las lineas nuevas por segundo (POPTOPE, POPTRAS, POPSEC y la SUBE de DIAG_MS cada 30 cuadros) llevan ~35 %
  mas de texto por la consola serie (117 frente a 87 KB en ALPI); la CPU del cuadro sube ~0,5-0,6 ms y el salto
  adaptativo lo amplifica. Es solo de la medida: el juego sin diagnostico no cambia.
- Arreglo (commit de esta medida): esas lineas, y POPLEJ, solo se escriben con -DDIAG_POP298 (los contadores siguen);
  coche_trasero_paso no mira el cono con COCHE_TRASERO=0 (salvo con DIAG_CONGELA). Con solo las POP* fuera quedaba
  -0,3 fps (ALPI 39,18, PALM 37,30); con SUBE y el cono tambien fuera, -0,1..0,2 en las pruebas sueltas y la tabla de
  arriba.
- Las tablas antes/despues de P1..P5 se hicieron con las dos variantes compiladas con el mismo diagnostico, asi que su
  comparacion vale; pero sus cifras absolutas (fps, salto) estan ~0,5-1 fps por debajo de la referencia por esto.
- Para usar topes.py, tras.py, sec.py, lej.py o sube.py hay que compilar con -DDIAG_POP298.

**Tanda suelta (m298/fin/res/, punta sin el arreglo, 3 Flycast en carriles):** media 37,33 fps frente a 38,19 de la
referencia de 2 Flycast; saltos 8 px 244,5 frente a 235,6. No cuenta para la comparacion (otra carga y el diagnostico
caro), solo como deteccion del problema.

**Supuesto (no comprobado)**
- Que la consola serie de Flycast bloquee al ritmo del puerto (~11 KB/s) se deduce de las cifras (~215 B/s mas, ~19 ms
  por s, ~0,5 ms por cuadro), no se ha medido el tiempo de printf.
- En la consola real la 2.9.8 por defecto solo difiere de la 2.9.7 en TOPE_FIJO=1 (P1) y el orden de mallas del coche
  ligero (P3, datos d298); no se ha medido en la consola.
- Ficheros: `m298/fin/` y `m298/final.md` del directorio de trabajo (res4/ las rondas definitivas, res3/ la primera
  ronda, res/ la tanda suelta, bis/ la biseccion, sh/, py/comp4.py).

### Rev 2.9.9 (diagnostico): edificios enteros que faltan (`-DDIAG_FALTA`)

Queja: en la salida de la playa (captura del usuario en Flycast de Android: parado, 0'10"963, F50) faltan edificios
enteros, un trozo del edificio blanco de la izquierda flota sin su parte de abajo y a la derecha hay huecos. Solo
medida (nada cambia sin los -D): todo en el Flycast de aqui, NO en la consola ni en Android.

Herramientas nuevas (`demo/main.c`):
- `-DDIAG_FALTA` (con `-DDIAG_POPPING`): `falta_mide` / `falta_prepara` y las marcas por malla `fm_buf` (FM_PON en
  `dibuja_etapa`, `dibuja_rango` y `ta_cabe`). Lo que esta en la vista (esfera contra la piramide, hasta LEJOS_MAX) y NO
  se dibuja, con la razon: por nodo lejos, lod, rama, pvs, tope (corte prio_d / prio_p), tramo, lista; por malla de un
  nodo dibujado ta_cabe, vtx, pvsm (mascara de la PVS), m_lejos, ltope, entrada; y tex0 / texg (enviada sin su
  textura). Linea `FALTA:` por s (cada `FALTA_CADA` cuadros una muestra; cuenta lo de `FALTA_GRANDE_PX` = 24 px o mas
  de radio en la pantalla) y con `-DDIAG_CONGELA` el listado del cuadro congelado (`FALTAINI`, `FALTAN`, `FALTAM`,
  `FALTAP`, `FALTAFIN`).
- `-DPRUEBA_PARADO` (el piloto no acelera), `-DPRUEBA_PARA_S=m` (conduce hasta m metros de la etapa y se para en seco),
  `-DPRUEBA_PARA_GIRA=rad`, `-DPRUEBA_GIRA=rad`.

La captura es del arbol 1 (preset de OutRun 2 SP: BEAC, la playa de salida), no de PALM del arbol 0: se reproduce con
`-DPRUEBA_CURSO=1 -DPRUEBA_NODO=0 -DPRUEBA_FICHA=7 -DPRUEBA_PARADO -DPRUEBA_PARA_S=95 -DDIAG_CONGELA=658`.

Medido (cuadro congelado, BEAC s 95, 24 px o mas):
- Con el Flycast de aqui (ta_f 448): 19 nodos por el corte del reparto (tope, hasta 36 px), 16 por el lejano (salto 7,4:
  lejano 1853 m), 0 mallas por ta_cabe; la imagen sale entera salvo lo lejano.
- Con `-DTA_F_MIN=576` (el TA medido 2,25 veces lo teorico): prio_d 0, 28 nodos por el tope (hasta 124 px) y 86 mallas por
  ta_cabe (hasta 247 px), de nodos a 60-150 m: el edificio blanco de la izquierda (nodos 1132-1148) pierde su parte de
  abajo y el de la derecha (484) se queda en las ventanas. Es la captura del usuario. Con 768: 91 nodos y 100 mallas.
- La razon: `ta_f` sube con lo que diga el registro del TA (`ta_fin_cuadro`, con 20 % de margen) y no baja nunca; el
  reparto se queda en su minimo (prio_d 0, la clase de LOD 0 siempre) y lo que aun no cabe lo tira `ta_cabe` en el orden
  de envio de `dibuja_etapa` (por textura): trozos de edificios cercanos.
- El salto adaptativo: con el coche parado el trabajo del cuadro (24-26 ms en Flycast) lo lleva a 7-9 y el lejano a
  1200-1850 m (en PALM de la carrera, nivel 9 desde los 1150 m). Con `-DSALTO_OBS=0` en la carrera de 140 s: PALM 35,17
  frente a 36,00 fps y de 20,7 a 3,4 elementos grandes que faltan por muestra (lejos 10,2 -> 0, tope 8,2 -> 3,4); METR
  31,34 frente a 31,67 fps.

Resultados en m299/diag del directorio de trabajo (capturas, tablas, consolas).

### Rev 2.9.9: que no falten edificios

Queja (captura del usuario en el Flycast de Android, parado en la salida, 0'10"963, F50): "faltan edificios enteros,
no cargan bien, lo estas gestionando mal". El diagnostico (seccion de arriba, `-DDIAG_FALTA`) encontro tres causas:
el salto adaptativo recortaba el lejano y el tope del TA segun el tiempo del cuadro; cuando el reparto no cabia,
`ta_cabe` tiraba mallas sueltas en el orden de envio (por textura), y de un edificio quedaba el piso de arriba
flotando; y el reparto quitaba antes lo grande con muchos vertices que lo pequeno y barato. Principio (Crazy Taxi, Rev
2.9.8): lo que se dibuja depende de la posicion y de la vista, no del tiempo medido ni de la carga del cuadro
anterior; si no cabe, se quita lo pequeno y lejano por nodos enteros y lo grande que tiene version lejana se cambia
por ella. Todo es DESVIACION (el original no tiene tope del TA). Todo medido en el Flycast de aqui, NO en la consola
ni en Android.

#### Cambios (`demo/main.c`)

- `SALTO_GEOM` (0 por defecto; 1 = lo de antes): el nivel del salto adaptativo ya no toca lo que se dibuja (ni
  `salto_ta`, el tope del TA al 94/87/80 %, ni el lejano, que bajaba hasta 1200 m). Queda la cadencia de presentacion
  (`pres_espera`) y la logica a 59,94 pasos por segundo. Con el coche parado el cuadro de Flycast cuesta 24-26 ms y el
  nivel subia a 7-9: en una maquina lenta (Android) el reparto se quedaba en su minimo.
- `CORTE_NODO` (1): `corte_nodos`, despues del reparto y antes de enviar nada, suma lo que va a costar cada nodo
  visible (TA y vertices): lo que costo de verdad en el cuadro anterior si se dibujo (`dibuja_etapa` lo apunta en
  `cn_ult` / `cn_vult`, por nodo y tramo) o, si es nuevo, su coste teorico de lo que deja la PVS por `ta_f` y por lo que
  la vista recorta de media (`prio_alfa` x 1,1; cerca de la camara, por el factor de lo que no va por la directa medido,
  `cn_ns_f`). Si no cabe en el tope de la etapa (las tres listas, sin el cielo del cuadro anterior y con un 3 % de
  margen, `CORTE_MARGEN`) quita NODOS ENTEROS por la clase de LOD del reparto (tamano en la pantalla: lo pequeno y
  lejano primero); lo de menos de 16 m (`CORTE_CERCA_CD`) nunca, salvo que ni eso quepa (entonces solo por la clase de
  LOD). Los de un grupo (`tools/grupos_nodos.py`, los pisos de un edificio) llevan la misma clase y van o se quitan
  juntos. Con el corte hecho, `dibuja_etapa` ya no reparte el tope entre las listas (`cn_activo`: sin la reserva de
  las listas 1 y 2) y el tope de vertices tiene un 12,5 % de holgura (`CORTE_VTX_HOLGURA`; es tiempo de CPU, no
  cuelga nada): `ta_cabe` no parte edificios. Una primera version que contaba las mallas una a una costaba ~3 ms por
  cuadro en PALM (descartada).
- Presupuesto de lo pequeno (`CORTE_GRANDE_PX` 24, `CORTE_PEQ_TA`, `CORTE_PEQ_VTX`): lo que mide menos de 24 px de
  radio en la pantalla (y no es de un grupo) solo cabe en el tope de la 2.9.8 (`ta_f` 448 y 20000 vertices); el sitio
  de mas solo lo gasta lo grande. Sin esto la geometria pequena y lejana crecia y el cuadro costaba ~0,4 fps mas (PALM,
  CLOU).
- `LEJ_EN_TOPE` (1): lo que el reparto (`nodos_quita`) o `corte_nodos` quitan y tiene version lejana (lowpoly de lo
  grande de `tools/lod_grande.py`, cartel de la vegetacion de `tools/lod_veg.py`) va con ella si cabe en lo que sobra,
  de lo mayor a lo menor; no los de un grupo (el lowpoly de un piso sin la base quedaba flotando: visto con `ta_f`
  576). Esas versiones no cuentan como "ya se dibujaba" para el nodo completo en el cuadro siguiente (`lq_marca` /
  `lq_ant`): si contaran, el completo entraba con la clase rebajada y el reparto se hundia (medido: `prio_p` -1).
- Reparto (`nodos_visibles`): lo de 24 px o mas (`REPARTO_COSTE_PX`) no se penaliza por sus vertices
  (`REPARTO_COSTE`), y si su umbral de LOD es bajo va con la clase de su tamano (umbral 6): un edificio grande ya no se
  quita antes que lo pequeno y barato.
- `TA_F_MIN` 400 (1,5625; antes 448 = 1,75) y `TA_F_SEGURO` (1): `ta_f` solo sigue al registro de la consola
  (`TA_ITP_CURRENT`) con cuadros que llenan al menos el 25 % del bufer (`TA_F_LLENO`), con un 10 % de margen
  (`TA_F_MARGEN`, antes 20 %) y sin bajar. En Flycast (sin el registro: `TA_ITP_CURRENT` = `TA_ISP_BASE`, comprobado en
  su codigo, pvr_regs.cpp) se queda en 400.
- `VTX_ETAPA_MAX` 24000 (antes 20000): en la salida de BEAC parado, con `ta_f` 400, los vertices eran lo primero que se
  acababa.

#### Medida

Flags: `-DPRUEBA_AUTO -DPRUEBA_NODO=n -DPRUEBA_SIN_TIEMPO -DDIAG_MS -DDIAG_POPPING -DDIAG_FALTA`, 140 s con el piloto
automatico (paso 10), tres Flycast a la vez (:90 :91 :92, como la tanda del diagnostico). "Antes" = faa81ea (2.9.8 con
el diagnostico; m299/diag/etapas/flycast_f*), "despues" = la punta. "Grandes que faltan" = media por muestra de
nodos y mallas de 24 px o mas de radio que estan en la vista (hasta LEJOS_MAX) y no se dibujan, por las razones que
son faltas de verdad (lejos, tope, tramo, ta_cabe, vtx, m_lejos, ltope...). No cuenta "rama" (registros de la union que
el original tampoco dibuja), ni "pvs" / "pvsm" (oclusion precalculada; SUPUESTO que es correcta, no se ha comprobado
pixel a pixel): esas dos van en la ultima columna. "999 px": la esfera envuelve la camara.

| Etapa | fps antes | fps despues | dif | tarde antes / despues | lejano antes / despues | grandes que faltan antes | despues (razon) | mayor antes / despues (px) | pvs+rama antes / despues |
|---|---|---|---|---|---|---|---|---|---|
| PALM (0) | 36,18 | 34,35 | -1,82 | 189 / 295 | 1592 / 3800 | 22,78 (lejos 10,22, tope 8,16, tramo 1,93, ta_cabe 0,31, m_lejos 1,98, entrada 0,17) | 2,02 (tope 0,92, tramo 0,94, entrada 0,16) | 999 / 999 | 36,5 / 43,9 |
| LAKE (1) | 38,55 | 37,24 | -1,31 | 61 / 141 | 2578 / 3800 | 3,68 (lejos 0,17, tope 2,33, ta_cabe 0,55, vtx 0,04, m_lejos 0,60) | 0,86 (tope 0,21, tramo 0,65) | 135 / 999 | 29,1 / 28,4 |
| ALPI (2) | 38,29 | 38,18 | -0,11 | 97 / 84 | 2653 / 3800 | 3,12 (lejos 0,00, tope 1,44, tramo 1,42, ta_cabe 0,23, m_lejos 0,02) | 0,03 (tope 0,03) | 999 / 27 | 53,3 / 61,4 |
| CAST (3) | 38,00 | 36,53 | -1,48 | 108 / 201 | 1907 / 3800 | 10,67 (lejos 3,69, tope 5,76, tramo 0,39, ta_cabe 0,82) | 1,59 (tope 0,76, tramo 0,81, entrada 0,02) | 209 / 999 | 88,5 / 87,4 |
| FORE (4) | 38,95 | 38,61 | -0,35 | 39 / 56 | 2914 / 3800 | 0,20 (tope 0,10, tramo 0,10) | 0,42 (tramo 0,42) | 999 / 210 | 101,3 / 100,0 |
| DESE (5) | 38,49 | 38,04 | -0,45 | 86 / 105 | 3082 / 3800 | 5,68 (tope 2,48, tramo 2,19, ta_cabe 0,01, vtx 0,87, entrada 0,12) | 1,67 (tope 0,01, tramo 1,66) | 999 / 999 | 61,6 / 65,9 |
| CLOU (6) | 35,56 | 33,45 | -2,11 | 202 / 397 | 1684 / 3800 | 13,15 (lejos 5,27, tope 7,43, ta_cabe 0,27, vtx 0,01, m_lejos 0,17) | 4,35 (tope 3,05, tramo 1,22, entrada 0,08) | 172 / 999 | 33,1 / 30,2 |
| INDU (7) | 37,20 | 35,54 | -1,65 | 107 / 212 | 2057 / 3800 | 17,78 (lejos 13,53, tope 1,83, ta_cabe 2,41, m_lejos 0,01) | 0,03 (tope 0,03) | 176 / 61 | 113,9 / 117,5 |
| SNOW (8) | 38,22 | 38,52 | +0,30 | 103 / 70 | 2905 / 3800 | 7,39 (lejos 0,01, tope 4,79, tramo 2,04, ta_cabe 0,46, vtx 0,07, entrada 0,01) | 3,95 (tope 3,82, tramo 0,13) | 310 / 217 | 39,5 / 43,9 |
| GHOS (9) | 36,68 | 36,09 | -0,59 | 130 / 205 | 1687 / 3800 | 10,50 (lejos 6,72, tope 2,16, ta_cabe 1,62) | 0,77 (tope 0,34, tramo 0,43) | 114 / 136 | 51,8 / 52,0 |
| TULI (10) | 37,39 | 36,25 | -1,14 | 103 / 188 | 1737 / 3686 | 6,10 (lejos 0,08, tope 3,50, tramo 0,01, ta_cabe 2,52) | 0,11 (tope 0,11) | 999 / 38 | 49,9 / 45,4 |
| METR (11) | 31,87 | 31,34 | -0,53 | 538 / 430 | 1937 / 3722 | 8,55 (lejos 2,09, tope 3,42, tramo 0,10, ta_cabe 0,40, m_lejos 2,55) | 0,42 (tope 0,42) | 256 / 99 | 97,6 / 94,2 |
| RUIN (12) | 38,54 | 38,41 | -0,13 | 65 / 85 | 2727 / 3573 | 12,93 (tope 2,19, tramo 0,07, ta_cabe 9,83, vtx 0,83) | 3,74 (tope 3,49, ta_cabe 0,24) | 999 / 999 | 13,2 / 14,2 |
| IMPE (13) | 38,10 | 37,12 | -0,98 | 79 / 146 | 2255 / 3596 | 4,61 (tope 3,10, ta_cabe 1,45, vtx 0,06) | 0,21 (tope 0,21) | 282 / 51 | 46,1 / 45,4 |
| CAPE (14) | 38,75 | 38,31 | -0,44 | 85 / 92 | 2750 / 3599 | 1,60 (tope 1,40, ta_cabe 0,06, vtx 0,01, m_lejos 0,13) | 0,01 (tramo 0,01) | 164 / 255 | 109,3 / 105,3 |
| **Media / suma** | 37,38 | 36,53 | -0,85 | 1992 / 2707 (+36 %) | | 128,73 (suma) | 20,19 (suma) | | |

Cuadro congelado de la captura del usuario (BEAC, arbol 1, F50, parado en s 95: `-DPRUEBA_CURSO=1 -DPRUEBA_NODO=0
-DPRUEBA_FICHA=7 -DPRUEBA_PARADO -DPRUEBA_PARA_S=95 -DDIAG_CONGELA=658`; lo de 24 px o mas que falta en el cuadro):

| Variante | ta_f | tope de la etapa | nodos que faltan (tope / lejos) | mallas (ta_cabe / m_lejos) | pvs / pvsm |
|---|---|---|---|---|---|
| 2.9.8 aqui (salto 7,4: lejano 1853 m, TA al 80 %) | 448 | 794K | 19 (hasta 36 px) / 16 (hasta 58 px) | 0 / 7 | 12 / 13 |
| 2.9.8 con `-DTA_F_MIN=576` | 576 | 720K | 28 (hasta 124 px) / 0 | 85 (hasta 247 px) / 0 | 12 / 11 |
| 2.9.8 con 640 y la maquina lenta | 640 | 512K | 30 (hasta 197 px) / 30 (hasta 155 px) | 1 / 6 | 8 / 9 |
| **2.9.9 por defecto** | 400 | 1088K | **0 / 0** | **0 / 0** | 8 / 11 |
| 2.9.9 con la maquina lenta (`-DDIAG_RETARDO=12000`) | 400 | 1088K | 0 / 0 (la misma imagen) | 0 / 0 | 8 / 11 |
| 2.9.9 en s 75 (cerca de donde estaba el usuario; 09c2bde) | 400 | 1088K | 0 / 0 | 0 / 0 | 14 / 16 |
| 2.9.9 con `-DTA_F_MIN=576` | 576 | 800K | 129 (hasta 111 px) / 0 | 0 / 0 | 12 / 3 |

Imagenes: m299/fix/comp_final_beac_s95.jpg (usuario, 2.9.8 aqui, 2.9.8 con ta_f 576; 2.9.9 por defecto, con la
maquina lenta y con ta_f 576).

**Medido**
- Los grandes que faltan bajan de 128,7 a 20,2 por muestra (suma de las 15 etapas). Desaparecen del todo
  lejos (41,8), m_lejos (5,5), ta_cabe (21,0: nunca mas un piso flotando) y vtx (1,9).
- Lo que queda: "tope" (lo que no cabe ni con la version lejana: sobre todo SNOW y RUIN al salir, donde la vista pide
  mas de 24000 vertices) y "tramo" (la union o la etapa siguiente a la vista antes de acabar de subir; depende de la
  carga del GD, no del reparto).
- En la captura del usuario: ningun edificio que falte por el reparto (1 nodo de 24 px, el skyline lejano) y la imagen
  es la misma con la maquina lenta (`-DDIAG_RETARDO=12000`): ya no depende del tiempo. En la carrera con la maquina
  lenta (PALM y METR, 12 ms mas por cuadro; 09c2bde) faltan lo mismo que a velocidad normal (PALM 2,40: tope 1,58,
  tramo 0,67; METR 0,34: tope), frente a 23,0 y 8,1 de la 2.9.8 lenta (lejos, tope, ta_cabe y m_lejos); fps 24,27 y
  21,75 frente a 25,17 y 22,30.
- Con `-DTA_F_MIN=576` (una consola que gastara 2,25 veces la cuenta) no cabe: faltan nodos enteros (129 de hasta 111
  px) y ninguna malla suelta (ta_cabe 0, antes 85 de hasta 247 px). Pero queda un caso de trozo flotando: el tejado de
  la torre gris de detras del edificio amarillo es un nodo aparte y sus pisos (nodos 572-575, sin grupo en los datos:
  `tools/grupos_nodos.py` no los junta) se quitan uno a uno; el tejado, que cabe, se queda en el aire
  (m299/fix/g576_vs_nl.jpg; con `-DLEJ_EN_TOPE=0` cabe la torre y no pasa). Con el `ta_f` por defecto no ocurre.
  Arreglarlo del todo pide juntar esos pisos en los datos (`tools/regenera.py --interno grupos`), no hecho aqui.
- Coste: -0,85 fps de media (37,38 -> 36,53; de +0,30 en SNOW a -2,11 en CLOU) y +36 % de cuadros tarde (1992 -> 2707); lo que mas pierde son las etapas en las que el salto quitaba mas (PALM, CLOU, INDU, CAST, LAKE: -1,3 a -2,1). Por partes (PALM, ms por cuadro, PRF de DIAG_MS): el reparto y `corte_nodos` 3,84 frente a 3,79,
  la etapa 9,22 frente a 7,87 (8,75 con solo el salto quitado, `-DSALTO_OBS=0` del diagnostico): ~0,9 ms es dibujar
  lo que el salto quitaba y ~0,5 ms lo grande que antes no cabia.
- El objetivo era no perder mas de 0,5 fps de media ni subir los cuadros tarde mas de un 15 %: NO se cumple. Se ha
  elegido que no falten edificios, lo que pidio el usuario: con el salto quitando geometria otra vez se recuperaria
  ~0,9 fps en las etapas cargadas a costa de volver a perder el lejano y el tope en las maquinas lentas.

**Supuesto (no comprobado)**
- Que en el Flycast de Android pase lo mismo que aqui: se supone que el registro del TA tampoco se lleva (mismo
  codigo de Flycast) y que lo que veia el usuario era el salto (maquina lenta: tope al 80 % y lejano a 1200-1850 m) mas
  `ta_cabe`. No se ha probado en Android.
- La consola real: `ta_f` 1,5625 deja un 16 % sobre el 1,34 medido en la demo 12c (que era "al menos"); si la consola
  gastara mas de ~1,6 veces la cuenta, el primer cuadro cargado antes de que `ta_f` suba podria llenar el bufer. Hace
  falta la foto de la linea TA de la pausa (Y) en la consola. 24000 vertices: mas CPU por cuadro en la consola, sin
  medir alli.
- Que "pvs" / "pvsm" sea oclusion correcta.
- Los interruptores de la 2.9.8 que estaban apagados (REPARTO_SEC, LEJ_SEC, COCHE_TRASERO, SUBE_DMA) siguen apagados:
  no se han medido con estos cambios. LEJ_SEC bajaria el lejano (faltas "lejos" otra vez); REPARTO_SEC daria un corte
  fijo por seccion, pero el que manda ahora es `corte_nodos`.

Ficheros: m299/fix/ del directorio de trabajo (tabla_final.md, comp_final_beac_s95.jpg, etapas/ las consolas de la
tanda final, res/ las de los congelados, sh/ y py/: comp.py, cong.py, prf.py).

#### Medida final de la 2.9.9 (A/B en las 15 etapas, 3 Flycast a la vez)

Todo en el Flycast de aqui (NO en la consola ni en Android). Flags: `-DPRUEBA_AUTO -DPRUEBA_NODO=n -DPRUEBA_SIN_TIEMPO
-DDIAG_MS -DDIAG_POPPING`, 140 s de piloto, paso 10, lgdat2; mide.py y pop.py solo con las lineas de la zona. Por cada
etapa una ronda de tres Flycast a la vez (:90 :91 :92), como la medida final de la 2.9.8.
- Pasada 1 (con el contador `-DDIAG_FALTA`): A = faa81ea (rev-1.1 39509d3 + solo el contador, que sin el -D no cambia
  el juego), B = punta de c299 (d1f8600), Bz = la punta sin el contador.
- Pasada 2 (sin el contador): A = 39509d3 (rev-1.1 tal cual), B = la punta, Ag = faa81ea con el contador (repeticion
  de la A de la pasada 1).

**Lo que encontro la medida: la 2.9.9 se quedaba sin efectos ni musica con el diagnostico (corregido, d1f8600).** Con
`-DDIAG_MS -DDIAG_POPPING` la 2.9.9 (109K mas de bss que la 2.9.8: cn_ult, cn_vult, cn_lista, lq_*) no cabia al
arrancar: "Out of memory ... diff 61440" (EFECTOS: marcas 0 chispas 0 particulas 0 agua 0) y "Out of memory ... diff
65536" + "snd_stream_init_ex(): memory allocation failed" (sin musica). Pasaba en todas las tandas de m299/fix
(flycast_e*, h*, j*, m*: 15 de 15), asi que la tabla de la seccion de arriba midio la 2.9.9 sin chispas, marcas,
particulas ni agua. La compilacion por defecto si arrancaba con todo, pero con 84K libres bajo la pila del nucleo
(`_arch_mem_top - THD_KERNEL_STACK_SIZE`) frente a 192K de la 2.9.8. Arreglo: cn_ult en 16 bits (bytes / 32), una
lista para los tres tramos en cn_lista y lq_* (mismo orden y mismos topes por tramo), y en la medida REPSEC.DAT solo
con `-DDIAG_POP298`, vr / vt en 16 bits y mv_* de falta_mide del monton. Por defecto 38K menos (2.966.716 bytes de
imagen frente a 3.004.636). Tras el arreglo las 90 pruebas arrancan con EFECTOS 3/1/1/1/1/1 y sin el fallo de
snd_stream; el unico "Out of memory" es el "diff 217088" de siempre (90 de 90, tambien en A).

| Etapa | fps A0 | fps B0 | B-A | tarde % A0 / B0 | salto A0 / B0 | lejano A0 / B0 | 8 px+ A0 / B0 | 32 px+ A0 / B0 | grandes que faltan Af / Bf | fps Af / Bf / Bz |
|---|---|---|---|---|---|---|---|---|---|---|
| PALM | 37,67 | 35,72 | -1,95 | 4,4 / 9,1 | 7,68 / 8,14 | 1696 / 3800 | 24,6 / 9,9 | 4,9 / 0,9 | 22,78 / 3,19 | 36,18 / 34,27 / 35,72 |
| LAKE | 38,81 | 37,98 | -0,83 | 2,4 / 4,0 | 2,15 / 5,93 | 3722 / 3800 | 5,8 / 2,8 | 1,4 / 0,2 | 3,79 / 0,56 | 38,10 / 37,02 / 38,14 |
| ALPI | 38,98 | 38,89 | -0,09 | 1,8 / 1,9 | 1,07 / 3,65 | 3792 / 3800 | 7,8 / 7,0 | 1,5 / 1,2 | 1,63 / 0,25 | 38,78 / 37,97 / 39,14 |
| CAST | 38,60 | 38,48 | -0,12 | 3,0 / 2,7 | 3,79 / 6,99 | 3153 / 3800 | 14,9 / 14,6 | 4,5 / 2,6 | 10,88 / 0,83 | 38,36 / 37,07 / 38,45 |
| FORE | 39,60 | 39,29 | -0,31 | 0,5 / 1,1 | 0,50 / 3,68 | 3800 / 3800 | 11,1 / 11,2 | 2,2 / 2,2 | 0,19 / 0,00 | 38,94 / 38,71 / 39,32 |
| DESE | 39,21 | 38,73 | -0,48 | 1,3 / 2,4 | 1,50 / 3,14 | 3640 / 3800 | 8,2 / 4,8 | 2,5 / 1,8 | 3,11 / 0,22 | 38,71 / 38,21 / 38,86 |
| CLOU | 37,10 | 35,13 | -1,97 | 5,9 / 10,8 | 7,61 / 7,95 | 1744 / 3800 | 28,4 / 21,7 | 3,6 / 2,2 | 12,62 / 3,94 | 35,76 / 33,77 / 35,08 |
| INDU | 38,43 | 37,05 | -1,38 | 2,7 / 5,7 | 5,98 / 6,66 | 2233 / 3800 | 25,4 / 11,1 | 6,5 / 4,2 | 16,95 / 0,03 | 37,10 / 35,51 / 37,01 |
| SNOW | 39,59 | 39,35 | -0,24 | 0,6 / 1,0 | 1,01 / 4,03 | 3782 / 3800 | 3,0 / 3,4 | 1,1 / 0,8 | 1,80 / 3,96 | 38,89 / 38,36 / 39,30 |
| GHOS | 37,75 | 36,45 | -1,30 | 4,1 / 7,3 | 7,77 / 8,03 | 1708 / 3800 | 23,6 / 15,6 | 3,5 / 2,7 | 10,81 / 0,93 | 36,70 / 35,35 / 36,37 |
| TULI | 38,60 | 37,26 | -1,34 | 2,3 / 5,4 | 6,47 / 7,70 | 2171 / 3754 | 29,6 / 23,0 | 3,5 / 3,2 | 6,38 / 0,07 | 37,39 / 36,00 / 37,37 |
| METR | 34,09 | 32,86 | -1,23 | 12,6 / 17,1 | 7,60 / 7,65 | 1787 / 3800 | 22,0 / 11,8 | 6,4 / 3,1 | 8,98 / 0,49 | 32,39 / 30,99 / 32,72 |
| RUIN | 39,08 | 38,91 | -0,17 | 1,5 / 1,7 | 2,73 / 5,03 | 3395 / 3800 | 8,6 / 10,5 | 1,6 / 1,6 | 12,93 / 4,26 | 38,55 / 38,21 / 38,89 |
| IMPE | 39,02 | 38,00 | -1,02 | 1,5 / 3,6 | 4,22 / 6,13 | 2943 / 3800 | 18,6 / 15,5 | 4,0 / 2,7 | 4,61 / 0,15 | 38,13 / 36,79 / 37,96 |
| CAPE | 39,34 | 38,85 | -0,49 | 1,0 / 1,9 | 1,56 / 4,71 | 3764 / 3800 | 16,4 / 12,4 | 4,7 / 4,2 | 1,40 / 0,00 | 38,77 / 38,16 / 38,83 |
| **Total** | 38,39 | 37,53 | -0,86 | cuadros 1184 / 1851 | | | 248,0 / 175,3 | 51,9 / 33,6 | 118,8 / 18,9 | 37,52 / 36,43 / 37,54 |

"grandes que faltan": nodos y mallas de 24 px o mas dentro de la vista que no se dibujan, por las razones reales
(lejos, tope, tramo, ta_cabe, vtx, m_lejos, entrada; no rama, pvs ni pvsm), suma por muestra. La repeticion Ag da
120,3 (A de la pasada 1: 118,8).

Saltos por segundo sumados en las 15 (8 px+ / 32 px+, apariciones + desapariciones):

| razon | A0 (rev-1.1) | B0 (2.9.9) |
|---|---|---|
| tope | 138,9 / 19,6 | 31,1 / 1,0 |
| lejos | 7,9 / 1,0 | 0 / 0 |
| pvs | 91,3 / 27,1 | 131,0 / 29,1 |
| lado | 4,9 / 0,8 | 6,3 / 1,1 |
| rama | 2,2 / 1,5 | 4,1 / 1,7 |
| detras, lod, tramo | 2,6 / 1,6 | 2,1 / 0,7 |

Grandes que faltan por razon (Af / Bf): lejos 41,50 / 0; tope 45,17 / 15,03; ta_cabe 20,84 / 0; m_lejos 5,53 / 0;
vtx 2,02 / 0; tramo 3,62 / 3,70; entrada 0,17 / 0,16. De diseno: rama 126,6 / 157,7; pvs 341 / 342; pvsm 453 / 453.

**Medido**
- Lo que pidio el usuario: los grandes que faltan bajan de 118,8 a 18,9 (-84 %); "lejos", "ta_cabe", "m_lejos" y
  "vtx" desaparecen; "tope" baja de 45,2 a 15,0. El lejano medio pasa a 3730-3800 m en todas (A: 1696 en PALM, 1744 en
  CLOU). Los saltos de 8 px o mas bajan de 248,0 a 175,3 por s (-29 %) y los de 32 px de 51,9 a 33,6 (-35 %).
- Coste: -0,86 fps de media sin el contador (38,39 -> 37,53) y +56 % de cuadros tarde (1184 -> 1851); con el
  contador -1,09 (37,52 -> 36,43). Las cargadas pierden mas: PALM -1,95, CLOU -1,97, INDU -1,38, TULI -1,34, GHOS
  -1,30, METR -1,23, IMPE -1,02; ALPI, CAST, RUIN, SNOW y FORE menos de 0,35. La desviacion de los fps sube en casi
  todas (PALM 3,07 -> 3,98, IMPE 1,24 -> 2,62).
- Ya con efectos y musica en B (la medida de m299/fix los tenia apagados en B): -0,86 aqui frente a -0,85 alli, la
  misma cifra, pero aquella era con A con efectos y B sin ellos.
- Capturas lado a lado A | B con la imagen congelada y parado (F50; m299/capturas/): la salida de BEAC s 95 (la del
  usuario), tambien con la maquina lenta (`-DDIAG_RETARDO=12000`), INDU s 2297, CAST s 2864, PALM s 2618, DESE s 3188 y
  CLOU s 517. En A faltan por lejos / tope / m_lejos: BEAC 16 + 19 nodos y 7 mallas (lenta: 26 nodos por lejos, hasta
  155 px, y el cielo sin lo de detras de las palmeras), INDU 40 + 10 nodos (la refineria y la grua de la izquierda),
  PALM s 2618 66 nodos y 14 mallas (toda la ciudad del fondo); en B ninguno por esas razones. CAST y DESE congelados
  casi no tienen huecos ni en A (tope 1 y 3 nodos). CLOU s 517 sigue igual en las dos: 50 y 48 nodos por tope (hasta
  133 px, tapados por la niebla en la imagen).
- Cuelgues: ninguno. Las 90 pruebas escriben lineas MS hasta el final (72-106 por prueba) y llegan a la misma s
  final que A (+-6 %); ninguna linea "No se pud", panic, assert ni excepcion.

**Regresiones (B frente a A)**
- fps y cuadros tarde (arriba): el objetivo de -0,5 fps / +15 % de cuadros tarde no se cumple.
- Saltos "pvs" +43 % (91,3 -> 131,0 de 8 px por s): con el lejano a 3800 m entra mas lejos y la PVS lo pone y lo quita
  al cambiar de seccion. Los de 32 px "pvs" casi igual (27,1 -> 29,1).
- SNOW: grandes que faltan 1,80 -> 3,96 (por "tope", en la salida) y 8 px 3,0 -> 3,4. RUIN: 8 px 8,6 -> 10,5 (aunque
  los grandes que faltan bajan de 12,9 a 4,3).
- "rama" (lo de la otra rama de la union, que el original tampoco dibuja) 126,6 -> 157,7: no es un fallo, mas lejano.
- RAM (corregida en d1f8600): sin el arreglo, la 2.9.9 con el diagnostico se quedaba sin efectos ni musica, y por
  defecto con 84K de margen. Con el arreglo, por defecto queda ~122K de margen (sin medir el margen exacto por defecto
  tras el arreglo; si medido que la de diagnostico arranca con todo).

**Supuesto (no comprobado)**
- Android y la consola: lo de arriba de esta seccion. El coste en fps puede ser mayor en una maquina mas lenta: el
  salto ya no quita geometria, asi que alli se dibuja todo y bajan los fps.
- Que el contador (DIAG_FALTA) no cambie lo que falta: cuesta ~0,9-1,1 fps a los dos y las dos pasadas dan lo mismo
  en A (118,8 / 120,3).

Ficheros: m299/final/ del directorio de trabajo (final.md, tablas.md, res/ las consolas y medida_<n>.txt, cap/ los
congelados, sh/ y py/), m299/capturas/*.png (recorte 192,144..832,624 de 1024x768).

#### Revision de la 2.9.9: costes viejos en corte_nodos

- **Salida (medido en Flycast, BEAC, linea CN1 temporal):** en la cuenta atras no hay corte, pero `dibuja_etapa` seguia
  sumando en `cn_ult` cada cuadro. En el primer cuadro de la carrera el corte veia 52 MB para un tope de 1041K, pasaba
  al modo 1 y quitaba 139 nodos, tambien los cercanos: un cuadro con la etapa casi vacia justo en la salida. Con la
  estimacion de lo nuevo en vez de lo viejo aun quitaba 101 (1091K estimado frente a 846K medido en el cuadro
  siguiente).
- **Cambio de etapa (`etapa = sig`):** el tramo 0 leia los costes de la etapa vieja por numero de nodo. En BEAC s 6666
  daba 425K frente a 822K en el cuadro siguiente.
- **Arreglo:** `cn_ult` solo vale si `corte_nodos` corrio en el cuadro anterior con el mismo modelo en el tramo. Si no,
  ese cuadro no corta y va como antes de la 2.9.9 (tope por listas y `ta_cabe`). Tras el arreglo, el primer cuadro con
  corte en la salida ya da 852K y 33 nodos, como los siguientes.


### Coche visible mas ligero (Rev 2.9.7)

Pedido del usuario (variante "C2", para reducir los saltos): menos poligonos y texturas en las partes VISIBLES de los
Ferrari sin perder estetica: ~35-40 % menos triangulos dibujados por coche simplificando la geometria visible con
contornos, siluetas y aristas vivas; luces, emblemas y matriculas sin tocar; la pintura, el reflejo y los emblemas sin
tocar y las demas texturas visibles de 256 a la mitad, con VQ; si de cerca se nota, el modelo completo en la seleccion
y el ligero en carrera. DESVIACION (el original lleva el coche entero). Medido en Flycast, no en la consola real.

- tools/conv_coche.py `--visible` (implica `--rebaja`; sin ella sale lo mismo que antes, comprobado byte a byte en los
  15): escribe el COCHEnn.DAT de siempre (el de `--rebaja`, Rev 2.9.6, identico) y ademas COCHEnnL.DAT, el ligero:
  - siempre sin lo oculto (el vaciado de cuando se pasa del tope, con la vuelta baja: no cambia la imagen desde las
    camaras del juego);
  - simplifica(): colapso de aristas por error cuadrico (Garland-Heckbert) de posicion, UV, color (la luz cocida) y
    normal (de ella sale el reflejo de la pintura), con el area de peso, en las mallas opacas de carroceria, extra,
    puertas, volante, palanca, aleron, ruedas (llanta y neumatico), pinzas y discos. Colapsos u -> v sin vertices
    nuevos (cada "cuna" de u, sus atributos en un grupo de material, pasa a la de v del otro lado de la arista);
    bordes abiertos, costuras de UV / normal / color y limites de material solo se acortan a lo largo de si mismos (con
    planos de arista de peso VIS_BORDE) y sus esquinas quedan; fijas las posiciones de lo que no se simplifica (luces,
    cristales, emblemas, matriculas, sombra) y las que estan a menos de 4 cm de ello (VIS_CERCA: sin esto la carroceria
    de alrededor de la matricula del 360 se aplanaba y la tapaba a medias); fuera el colapso que gire una cara mas de
    37 grados (VIS_GIRO 0,8) o rompa la variedad. Hasta el 60 % de los triangulos simplificables (VIS_RAZON) o el
    error VIS_TOPE. Las tiras de las mismas posiciones traen normales y luz algo distintas (0xBE / 0xC0): se juntan
    (media) si la UV, el color (12 por canal) y la normal (~25 grados) casi coinciden. Re-estripificado voraz (tiras de
    hasta 256 vertices; sin descarte de caras el sentido da igual);
  - texturas: las de `--rebaja` y ademas (clase 'visible') las opacas de fuera con un lado de 256, sin pintura,
    reflejo, luces ni sombra, a la mitad en el lado en que menos se nota (menor error RMS al ampliarla otra vez) o en
    los dos si ese error es de 3 o menos; si aun la mejor mitad pasa de 8 (VIS_TEX_MAX: los atlas de motor y
    salpicadero del 360, 575, F430 y 550, que en carrera se notaban: el motor del 360 se ve bajo el cristal) se queda.
    Bajan sobre todo los bajos (256x128 -> 128x128 o 256x64) y un atlas de 256x256 (Testarossa, a 256x128). Todas
    con VQ (comprobado en los 15).
- demo/main.c coche_carga: en carrera el ligero COCHEnnL.DAT (coche_pide_ligero, al empezar la carrera; al
  reintentar se queda) y en el menu el completo COCHEnn.DAT (cuando el menu ensena el coche, menu_coche3d, lo vuelve a
  cargar si esta el ligero). Compatible: sin COCHEnnL.DAT (datos de antes, el taxi) se queda el completo (coche_sin_ligero, sin volver a
  pedirlo). De cerca SI se notaba (capturas de la seleccion con el ligero: reflejos a facetas en el capo y el
  paragolpes, la toma lateral del 360), y los dos modelos en un fichero con NODO_CERCA (como la variante B) no caben en
  el bloque del coche (COCHE_RAM 496 KB: saldrian 660-740 KB): por eso dos ficheros. Coste: en la carga de cada carrera
  se lee el ligero (300-380 KB del GD) y al volver a la seleccion el completo. La salida y la meta (escenas de cerca
  dentro de la carrera) usan el ligero.
- tools/regenera.py: los pasos cocheN usan `--visible` y la comprobacion pide COCHE00..14L.DAT; coches_visibles()
  rehace los coches de unos datos sin COCHE00L.DAT (o con uno de mas de 10500 vertices).
- comun/menu.c `-DDIAG_GIRO_FIJO=angulo` (medida): el coche de la seleccion quieto, para capturas comparables.

Triangulos dibujados a la vez en carrera (con area: lo fijo, el faro, el mayor juego de pilotos y de discos), vertices
del TA del coche en carrera y VRAM de sus texturas (base = la Rev 2.9.6, COCHEnn.DAT):

| ficha | coche | triangulos base / C2 | vertices en carrera base / C2 | VRAM KB base / C2 |
|---|---|---|---|---|
| 00 | 360sp | 7351 / 4665 (-37 %) | 10910 / 7456 (-32 %) | 182 / 178 |
| 01 | 575sa | 8256 / 5255 (-36 %) | 11674 / 7832 (-33 %) | 141 / 138 |
| 02 | gto | 7003 / 4420 (-37 %) | 10916 / 7516 (-31 %) | 161 / 154 |
| 03 | 250gto | 6587 / 4093 (-38 %) | 9374 / 6345 (-32 %) | 158 / 154 |
| 04 | f40 | 7584 / 5186 (-32 %) | 11511 / 8484 (-26 %) | 183 / 179 |
| 05 | f430 | 7580 / 4704 (-38 %) | 11288 / 7535 (-33 %) | 127 / 123 |
| 06 | fx | 7569 / 5004 (-34 %) | 11212 / 8092 (-28 %) | 186 / 178 |
| 07 | f50 | 7594 / 4869 (-36 %) | 10921 / 7466 (-32 %) | 147 / 143 |
| 08 | f355sp | 7178 / 4490 (-37 %) | 10746 / 7468 (-31 %) | 165 / 149 |
| 09 | testa | 6971 / 4786 (-31 %) | 10585 / 7939 (-25 %) | 151 / 139 |
| 10 | 328gts | 6941 / 4160 (-40 %) | 10882 / 7195 (-34 %) | 153 / 145 |
| 11 | 512bb | 6435 / 3772 (-41 %) | 10035 / 6680 (-33 %) | 180 / 176 |
| 12 | 550b | 7743 / 4788 (-38 %) | 11091 / 7364 (-34 %) | 133 / 125 |
| 13 | dino | 6940 / 4097 (-41 %) | 10089 / 6329 (-37 %) | 140 / 136 |
| 14 | dayts | 7110 / 4435 (-38 %) | 10502 / 7179 (-32 %) | 148 / 141 |
| | total | 108842 / 68724 (-37 %) | 161736 / 110880 (-31 %) | 2354 / 2255 (-4,2 %) |

Medidas (Flycast, piloto, `-DPRUEBA_AUTO -DPRUEBA_SIN_TIEMPO -DDIAG_MS -DDIAG_POPPING -DPRUEBA_NODO=n -DPRUEBA_FICHA=f`,
140 s, tres Flycast a la vez; el mismo tramo de s de las dos pruebas: lineas MS desde s 200, POP del tramo comun;
coche = us de CPU y vertices de la linea PRF; saltos por segundo de 8 px o mas / de 32 px o mas, total y por razon):

| prueba | fps | tarde | salto | lejano | coche us / vtx | saltos 8 / 32 px | tope (+ / -) | pvs (+ / -) |
|---|---|---|---|---|---|---|---|---|
| Dino PALM base | 38,33 | 47 | 6,27 | 2246 | 2406 / 10054 | 33,6 / 7,8 | +25,4/6,6 -5,3/0,5 | +2,3/0,4 -0,1/0,1 |
| Dino PALM C2 | 38,37 | 45 | 6,85 | 2099 | 1930 / 6294 | 27,9 / 4,9 | +19,9/4,1 -4,4/0,3 | +3,1/0,3 -0,1/0,1 |
| Dino GHOS base | 37,64 | 51 | 7,63 | 1864 | 2423 / 10081 | 27,8 / 4,2 | +16,1/1,5 -3,4/0,0 | +7,1/1,7 -1,2/0,9 |
| Dino GHOS C2 | 37,39 | 59 | 7,95 | 1668 | 1952 / 6321 | 29,3 / 3,8 | +14,0/0,9 -5,3/0,1 | +8,7/1,8 -1,2/0,8 |
| Dino METR base | 34,22 | 160 | 8,34 | 1644 | 2407 / 10081 | 22,5 / 6,8 | +7,7/2,2 -1,9/0,4 | +11,4/3,5 -0,1/0,0 |
| Dino METR C2 | 34,80 | 143 | 8,17 | 1695 | 1944 / 6321 | 23,8 / 6,7 | +7,7/1,9 -2,6/0,6 | +11,4/3,5 -0,1/0,0 |
| 360 PALM base | 38,15 | 48 | 6,92 | 2052 | 2685 / 10615 | 33,0 / 7,1 | +23,8/5,9 -5,7/0,5 | +1,7/0,2 -0,1/0,1 |
| 360 PALM C2 | 38,24 | 47 | 6,78 | 2101 | 2279 / 7161 | 29,4 / 5,3 | +19,0/4,0 -5,6/0,3 | +2,5/0,2 -0,1/0,1 |
| 360 GHOS base | 37,59 | 57 | 7,97 | 1634 | 2772 / 10888 | 27,4 / 4,5 | +14,8/1,6 -4,2/0,1 | +7,2/1,9 -1,1/0,8 |
| 360 GHOS C2 | 37,61 | 57 | 7,98 | 1647 | 2363 / 7434 | 30,1 / 3,9 | +14,2/0,9 -5,7/0,1 | +8,7/2,0 -1,3/0,9 |
| 360 METR base | 35,24 | 157 | 8,44 | 1579 | 2765 / 10887 | 21,0 / 6,2 | +7,8/2,1 -2,4/0,5 | +9,3/3,2 -0,1/0,0 |
| 360 METR C2 | 35,50 | 148 | 8,27 | 1621 | 2365 / 7433 | 19,8 / 5,6 | +6,2/1,4 -2,4/0,5 | +9,6/3,2 -0,1/0,0 |
- El coche cuesta un 15-20 % menos de CPU (2400 -> 1950 us el Dino, 2770 -> 2370 el 360) y un 31-37 % menos de vertices
  del TA. Los fps y los cuadros tarde casi no cambian (+-0,3 fps, como en la variante B: lo que deja libre el coche lo
  ocupa la etapa, cuyo tope sale de lo que gasto lo demas). En PALM bajan los saltos (8 px 33 -> 28 / 29 por s, 32 px
  7,8 -> 4,9 y 7,1 -> 5,3, casi todos de "tope"); en GHOS suben los de 8 px (27,8 -> 29,3 y 27,4 -> 30,1: mas
  apariciones "pvs" y desapariciones "tope"; el lejano del Dino baja 1864 -> 1668) y bajan los de 32 px; en METR casi
  igual (22,5 -> 23,8 y 21,0 -> 19,8 de 8 px; 6,8 -> 6,7 y 6,2 -> 5,6 de 32 px) con algo menos de cuadros tarde.
  Flycast repite la carrera casi igual (una primera version de C2, con otros ajustes, dio los mismos fps a +-0,2 y los
  mismos saltos a +-1 por s): diferencias de ese orden son ruido. VRAM: -4 KB por coche de
  media (-99 KB los 15): poca, porque la pintura, los reflejos, las luces, los emblemas y los atlas con detalle se
  quedan.
- Lo que se pierde a la vista (capturas con la imagen congelada, `-DDIAG_CONGELA=1500`, el mismo cuadro): en carrera
  las curvas de las aletas traseras y del paragolpes salen algo mas poligonales en el contorno (1-2 pixeles a 640x480)
  y el reflejo de la pintura en la tapa del motor y las aletas sale a facetas (zonas mas planas, mas visible en el 360);
  las llantas y los neumaticos no se distinguen. Los bajos con la mitad de resolucion. Matriculas, pilotos, emblemas y
  el motor del 360 igual. La seleccion de coche, igual pixel a pixel (el completo).
- No verificado en la consola real (alli el TA puede pesar mas que en Flycast y los vertices de menos rendir mas), ni
  la vuelta al menu tras una carrera en Flycast (la recarga del completo: el mismo camino que el cambio de ficha).

### Coches mas ligeros (Rev 2.9.6)

Pedido del usuario: optimizar los 15 coches del jugador (COCHE00..14.DAT) para ganar rendimiento sin perder estetica,
midiendo dos variantes y con capturas lado a lado antes de decidir. DESVIACION (las dos): el original lleva el coche
entero y las texturas a su tamano. Las dos son opciones de tools/conv_coche.py; sin ellas sale lo mismo que antes (el
COCHE00.DAT de ahora sale igual: 1425 mallas, 12307 vertices). Medido en Flycast, no en la consola real.

- Variante A, `--rebaja` (rebaja_texturas / clasifica_texturas): menos resolucion en las texturas que no se ven de
  cerca. Regla por las piezas de llamadas() que usan cada textura, ya vaciado el coche: FIJA (no se toca) la de una
  malla con pintura o reflejo (CEX2), con luz (G_PILOTOS..G_FARO, faros, faro_luz, pilotos*, aleron_freno), la sombra y
  las de recorte o transparentes (emblemas, letras, matriculas, rejillas); RUEDA (solo buje, pinza y discos): 64 de lado
  como mucho; INTERIOR (solo el habitaculo +04, que solo dibuja la vista interior): la mitad; OCULTA (opaca, de
  carroceria / puertas / volante / palanca / habitaculo, con el 85 % o mas de su area sin ver desde las camaras del
  vaciado contando cristales y rejillas como opacos: bajos, motor bajo cristal o rejilla, lo que solo se ve por las
  ventanillas): la mitad si la comparte el habitaculo, volante o palanca, si no 64 de lado. Las de 64 o menos no
  cambian. Filtro de caja con el alfa premultiplicado, y la VQ de siempre. En los 15 coches: 53 de ruedas y discos, 3
  del interior (la 512x256 del 360 Spider es el salpicadero: solo la usa el habitaculo, queda en 256x128, no en
  256x256), 4 ocultas (motor y bajos del Enzo, F50, 328 y Dino) y 2 ocultas del habitaculo (Dino y Daytona). Lo de la carroceria, faros, pilotos,
  emblemas y matriculas no cambia; los bajos del 360 (256x128) y el atlas de motor y habitaculo del 360 (256x256)
  quedan FIJA porque se ven (spider). VRAM de los 15: 2618 KB -> 2354 KB (360 Spider 229 -> 182 KB, Dino 177 -> 140).
- Variante B, `--carrera` (parte_carrera): las tiras que no se ven desde la persecucion van aparte con el bit
  NODO_CERCA (0x10000) en el campo del nodo y la demo no las dibuja en carrera con la camara de persecucion
  (coche_sin_cerca: estado CARRERA, sin la escena de la salida ni la de la meta, sin mezcla de vistas, no en los
  finales); la seleccion, el salon, la salida, la meta y la vista interior dibujan todo (el habitaculo ya solo se
  enviaba con la vista 1: coche_dibuja salta PZ_INTERIOR en las demas, comprobado). Criterio: el del vaciado con otras
  camaras: se queda en carrera el triangulo que gana algun pixel a 640x480 en 4 o mas de las camaras de carrera de
  camaras() (detras con +-100 grados de deriva, 1,9-2,9 m de alto, 4,5-6,5 m) por las 6 posturas de las piezas
  animadas; las luces siempre. Sale lo que solo se ve de lado o de frente desde cerca, desde abajo o desde la
  seleccion, y lo que asoma en un pixel de 1 a 3 camaras (piezas pequenas a esa distancia). Las tiras se parten como
  en vacia (huecos de 2); 1 o 2 triangulos sueltos en las puntas se quedan en carrera (partir cuesta 2 vertices). El
  minimo reservado del TA para el coche (ta_min_otros_teo) cuenta solo lo de carrera (coche_teo_carrera). Compatible:
  sin el bit (los ficheros de antes) se dibuja y se reserva todo, como hasta ahora. En carrera: 360 Spider 10910 ->
  9998 vertices, Dino 10089 -> 8631 (todos: -5 % a -15 %); en la seleccion suben 100-300 por los cortes.

PROVISIONAL (pendiente de que el usuario elija con las capturas). Medidas en Flycast (piloto automatico, `-DPRUEBA_AUTO
-DPRUEBA_SIN_TIEMPO -DDIAG_MS -DPRUEBA_NODO=n -DPRUEBA_FICHA=f`, 140 s; Flycast repite la misma carrera, asi que se
comparan las lineas MS del mismo tramo de s, de 200 al final comun; coche = us de CPU y vertices de la linea PRF;
VRAM: textura del coche, libre al cargarlo y zona libre minima de la carrera):

| prueba | fps | tarde | lejano | salto | coche us / vtx | VRAM coche / libre / zona libre |
|---|---|---|---|---|---|---|
| 360 PALM base | 39,72 | 7 | 3227 | 3,75 | 2578 / 10616 | 229 / 1594 / 594 KB |
| 360 PALM A | 39,75 | 6 | 3301 | 3,58 | 2588 / 10616 | 182 / 1641 / 658 KB |
| 360 PALM B | 39,60 | 13 | 2890 | 4,86 | 2521 / 9704 | 229 / 1594 / 622 KB |
| Dino PALM base | 39,19 | 35 | 3697 | 2,31 | 2381 / 10054 | 177 / 1646 / 686 KB |
| Dino PALM A | 39,22 | 33 | 3732 | 2,22 | 2391 / 10054 | 140 / 1683 / 718 KB |
| Dino PALM B | 39,04 | 41 | 2990 | 4,57 | 2215 / 8596 | 177 / 1646 / 686 KB |
| 360 METR base | 36,27 | 168 | 1562 | 8,32 | 2655 / 10888 | zona libre 779 KB |
| 360 METR A | 36,31 | 167 | 1571 | 8,29 | 2653 / 10888 | 843 KB |
| 360 METR B | 35,98 | 180 | 1550 | 8,37 | 2605 / 9976 | 779 KB |
| Dino METR base | 35,82 | 198 | 1543 | 8,37 | 2320 / 10081 | 843 KB |
| Dino METR A | 35,85 | 196 | 1541 | 8,38 | 2322 / 10081 | 875 KB |
| Dino METR B | 35,78 | 202 | 1556 | 8,33 | 2140 / 8623 | 843 KB |

- A: los fps y los cuadros tarde no cambian (el TA y la CPU son los mismos); la VRAM que deja el coche (37-47 KB) va
  a la zona del curso (+32 / +64 KB) y en PALM el lejano sube un poco (3227 -> 3301, 3697 -> 3732). Capturas con la
  imagen congelada (`-DDIAG_CONGELA=1500`, el mismo cuadro pixel a pixel): en carrera no se distingue (diferencias de
  1-2 niveles de color en las ruedas); en la seleccion las llantas se ven algo mas blandas.
- B: el coche cuesta menos (-9 % y -14 % de vertices, -2 % y -7 % de CPU) y la imagen congelada en carrera es la misma
  (4 y 10 pixeles distintos, ninguno del contorno del coche), pero la carrera no va mejor: el TA que deja libre lo
  ocupa la etapa (el tope de la etapa sale de lo que gasto lo demas en el cuadro anterior), que cuesta mas CPU, y el
  salto adaptativo sube (PALM: lejano 3227 -> 2890 y 3697 -> 2990; METR igual). Con la reserva de antes
  (coche_teo_carrera = modelo_teo) sale lo mismo (360 PALM: lejano 2931, salto 4,67): no es la reserva. En la consola
  real, si el limite es el TA y no la CPU, podria ser al reves: sin medir.
- Recomendacion (pendiente del usuario): A si; B no en Flycast (el codigo de la demo no cambia nada sin el bit).

### Saltos en las etapas de noche (Rev 2.9.5, 2026-11-23)

Queja (Rev 2.9.4): "las fases de noche tienen elementos que aparecen y desaparecen". DESVIACION. Verificado en Flycast
(no en consola real: Flycast no da el tiempo del SH4 ni llena el TA como la consola). Medido con `-DPRUEBA_AUTO
-DPRUEBA_SIN_TIEMPO -DDIAG_MS -DDIAG_POPPING` (lineas POP: saltos por segundo de 8 px o mas / de 32 px o mas, por razon;
Flycast con la misma compilacion repite la carrera igual, asi que antes y despues se comparan en el mismo tramo).
- Causas, por razon de DIAG_POPPING:
  - "pvs" (GHOS: 12,4 apariciones y 12,1 desapariciones por s de 8 px o mas, igual con y sin faros): la PVS de
    tools/pvs.py, no un fallo de la demo. Sin lowpoly (`-DLOD_GRANDE_D=1e9f`) y sin `decor_reduce` (`-DSIN_DECOR_REDUCE`)
    sale lo mismo; con `-DSIN_PVS` desaparecen (y no cambian los fps en Flycast). Mirando la tabla de SEG09 (las
    mascaras de las 634 secciones), nodos enteros como el monte 1 (r 715 m) o los bloques de bosque 193 / 194 / 27
    (r ~100 m, 40-60 px a 500-900 m) se encienden en rachas de 3 secciones (una sola muestra del ojo los ve por un hueco
    entre los troncos, mas sus dos vecinas) y se apagan en rachas de 9-30 secciones (1,5-4 s a 200 km/h); la gracia
    (PVS_GRACIA, 15 cuadros) era mucho mas corta. Y como las muestras van cada 4 m a lo largo y a lo ancho y a 0,6 /
    3,3 / 6 m de alto, desde la camara entre muestras se ve a veces lo que la tabla da por tapado: aparece al entrar en
    la primera seccion que lo ve.
  - "lejos" (METR: 3,1 / 2,0 por s): la emergencia del salto adaptativo (nivel 6..9) bajaba el lejano a 600 m y de ahi
    subia y bajaba; de noche la franja de niebla del limite es debil (cielo mas oscuro que la niebla).
  - "tope" (GHOS +12,5 / -5,7, METR +5,6 / -1,7): el reparto del TA; el corte bajaba en el mismo cuadro en cuanto el
    objetivo, con el 95 % del tope, pedia una clase menos, y el tope cambia cada cuadro (coche, trafico y el nivel del
    salto, que se mueve un poco en cada cuadro de carrera). Muchas de GHOS a 1155 m son lo tapado 63/64 por la niebla
    (va a la ultima clase): en GHOS el horizonte es del color de la niebla y no se ven. Probado no tratarlo asi de
    noche (silueta clara contra el cielo negro): en METR no habia saltos ahi y el TA de mas quitaba cosas cercanas;
    descartado.
- Cambios (demo/main.c; cada uno con su `-D` para volver a lo de antes):
  - PVS_GRACIA 15 -> 240 cuadros (6 s): el nodo que la PVS apaga sigue dibujandose con su ultimo coste. Medido en GHOS:
    120 cuadros, desapariciones "pvs" 2,4 por s; 200, 1,3; sin coste de fps.
  - Ventana de la PVS (PVS_VENTANA 3, pvs_ventana / pvs_aplica_v / pvs_coste): ademas de la mascara de la seccion del
    ojo, las de s - 3 y s + 3 (como las rachas de la tabla son de 3 o mas, lo de toda la ventana, +-24 m); lo que va a
    verse ya esta dibujado (tapado) antes de verse. 2 mascaras y 8 bytes por nodo mas por modelo (~12 KB en GHOS). En
    el reparto, el mayor coste de los tres. La medida de las mallas de la PVS (POP "pvs" de despues de "falta") mira
    ahora lo que se dibuja (pvs_dibujo en pvs_recuerda) y no la mascara.
  - Histeresis del corte del reparto (REPARTO_SCHMITT, reparto_cabe): el corte de antes se queda mientras lo que dibuja
    quepa en el 100 % del tope (del TA de la etapa y de los vertices); el objetivo sigue con el 95 %. Lo que se envia
    sigue limitado por ta_cabe.
  - SALTO_LEJ_SUELO 1200 m: la emergencia baja el lejano de 2400 a 1200 y no a 600. Probado 1700 (lo que se pedia,
    1600-1800): en METR 0,2 saltos del limite por s pero mas del tope cerca (20,3 en total frente a 18,7 con 1200), en
    las demas igual y los mismos fps.
  - Focos de los faros: FOCOS_ANOTA_D 40 -> 30 m (el haz acaba a FOCOS_LEJOS, 28 m: misma imagen); FOCOSMS en METR
    713 -> 611 us por cuadro, en GHOS 784 -> 676. FOCOS_VSUB sin tocar (las divisiones son las del haz de cerca). Los
    faros siguen encendidos toda la etapa.
- Medido (Flycast, piloto, 140 s por prueba, tres a la vez; mismas medias de mide.py; saltos por s de 8 px o mas / de
  32 px o mas). "Antes" es la 2.9.4 (`-DREPARTO_SCHMITT=0 -DSALTO_LEJ_SUELO=600.0f -DPVS_GRACIA=15 -DPVS_VENTANA=0
  -DFOCOS_ANOTA_D=40.0f`):

| Etapa | fps antes / despues | tarde | lejano medio (min) | nivel | saltos 8 px / 32 px | pvs | tope | lejos |
|---|---|---|---|---|---|---|---|---|
| GHOS | 38,12 / 37,71 | 106 / 138 | 1578 (600) / 1600 (1200) | 7,17 / 7,98 | 31,3 / 9,1 -> 16,8 / 3,4 | +8,4 -7,5 -> +4,7 -0,5 | +9,0 -3,9 -> +8,0 -3,1 | +0,8 -1,3 -> +0,1 -0,0 |
| METR | 35,26 / 33,54 | 304 / 414 | 1552 (600) / 1909 (1200) | 7,46 / 7,54 | 23,3 / 7,8 -> 18,7 / 5,3 | +6,2 -2,1 -> +5,4 -0,1 | +6,5 -1,9 -> +8,4 -2,2 | +3,0 -1,6 -> +0,4 -0,4 |
| PALM | 38,17 / 38,10 | 96 / 104 | 1983 (600) / 1971 (1200) | 6,22 / 7,00 | 31,9 / 9,3 -> 22,8 / 5,9 | +4,9 -3,5 -> +2,2 -0,1 | +16,8 -4,3 -> +15,7 -4,0 | +0,9 -1,3 -> +0,3 -0,3 |
| INDU | 38,62 / 38,54 | 86 / 90 | 2260 (886) / 2149 (1200) | 5,53 / 6,27 | 29,8 / 12,1 -> 21,7 / 5,8 | +9,5 -6,6 -> +6,0 -0,1 | +9,4 -3,9 -> +10,6 -4,5 | +0,1 -0,1 -> +0,1 -0,3 |

  Los de 32 px o mas bajan a la mitad o menos en todas. METR pierde 1,7 fps (cada parte ~0,4-0,5: la ventana, la
  histeresis del reparto y el suelo del lejano; la gracia no cuesta): es la que ya no llegaba (la ciudad cercana,
  ~28 ms de CPU por cuadro en Flycast) y se eligio menos saltos (pedido). Lo que queda: apariciones del tope al
  acercarse (lo que entra por la clase de distancia o de LOD) y las mallas de la PVS dentro de nodos que ya se dibujan
  (METR ~40 por s de 8 px o mas, casi todas de cosas que salen de detras de otras). `-DPVS_VENTANA=0`,
  `-DREPARTO_SCHMITT=0`, `-DPVS_GRACIA=15`, `-DSALTO_LEJ_SUELO=600.0f`, `-DFOCOS_ANOTA_D=40.0f`: lo de antes.
- NO verificado en consola real (alli el TA y la CPU pesan distinto: la ventana y la gracia dibujan algo mas).

- RAM (comprobado en el GDI de la 2.9.5): la ventana de la PVS guarda los costes de sus dos secciones en 16 bits
  (unidades de 32 bytes; con 32 bits el menu perdia 54 KB). Aun asi el menu queda con ~383 KB libres (426 KB en la 2.9.3:
  los records de la 2.9.4 y la ventana) y al arrancar sale una vez "Out of memory ... diff 217088": es newlib, que pide
  al monton el bloque con su margen y no cabe; la peticion se sirve con lo que ya hay (SALIDA.DAT y lo demas se leen
  bien, nav3 llega a la carrera sin errores en el clasico y en el taxi). Hay menos margen: vigilarlo en la consola.
### 40 fps adaptativos y lejano 3800 (Rev 2.9.4, 2026-11-23)

Pedido: "que mantenga los 40 fps con frameskip progresivo adaptativo y que aumente la distancia de dibujado a 3800".
DESVIACION. Verificado en Flycast (no en consola real: Flycast no da el tiempo del SH4 ni llena el TA como la consola).
- LEJOS_MAX 3200 -> 3800 (demo/main.c): el arranque, el lejano de la carrera, lejos_guion y los topes de la meta, la
  franja de niebla del limite y el lowpoly de lo grande (LOD_GRANDE_FR 0,8: el completo hasta 3040 m). LEJOS_PS2 (4000)
  sigue para el proyector de los faros (40A950). Revisado: ninguna tabla depende del lejano; el cielo va a 1/w 1,2e-4
  (8333 m), mas alla; la niebla (129 entradas, a lejano / t) se rehace sola al cambiar el lejano.
- Salto progresivo ADAPTATIVO (SALTO_*; antes, 2.9.2.e/f, un contador de cuadros tarde con 4 niveles y el recorte por
  CPU aparte de 50 m cada 30 cuadros): un nivel continuo salto_q 0..9 que cada cuadro de carrera sigue al trabajo medio
  del cuadro (pres_trab: de pres_espera a pres_espera sin la espera al campo; media de 1/16) contra los 25 ms de un
  cuadro a 40 fps: sube por encima de 24 ms en proporcion a lo que falte (como mucho 0,03 por cuadro) y +0,08 por cuadro
  tarde; baja por debajo de 22 ms (como mucho 0,012 por cuadro) si no hubo ninguno tarde en 1 s; entre 22 y 24 ms no se
  mueve (histeresis). Leyendo del GD no se mueve. Tramos, lineales (sin escalones):
  0..3 el tope del TA de la etapa y de los vertices del reparto al 94 / 87 / 80 %; 3..6 el lejano 3800 -> 3200 -> 2800
  -> 2400 m; 6..9 emergencia, el lejano a 600 m (lo que hacia el recorte por CPU, 4 veces mas despacio). El lejano sigue
  al del nivel a 240 m/s al bajar y 160 m/s al subir, dentro de la franja de niebla del limite (que ya no salta de 0,85
  a 0,55: 0,85 hasta 2400 m y de ahi a 0,55 en 600 m, seguido). Al cambiar de etapa el nivel vuelve como mucho a 3 (el
  lejano entero). La logica sigue a 59,94 pasos por segundo. `-DSIN_SALTO`: sin el, con el recorte por CPU de antes.
- Por que primero el TA: medido en METR (mismos tramos de la carrera) con el lejano fijo, 3800 m 35,2 fps (355 cuadros
  tarde), 2400 m 35,6 (447) y 3800 m con el TA al 80 %, 36,5 (269): el reparto del TA ya limita lo que se envia y el
  lejano solo ahorra por debajo de ~1000 m. En PALM, 3800 m fijos sin recorte: 39,13 fps (51 tarde), lo mismo que 3200
  con el recorte de antes (39,12, 53).
- Medido (Flycast, piloto, -DDIAG_MS, -DPRUEBA_NODO=n, 150 s; medias de toda la etapa; "comun": solo los tramos de 500 m
  que recorren las dos pruebas):

| Etapa | 2.9.3 (3200, salto 2.9.2.f): fps / desv. / tarde / lejano | 2.9.4 (3800 + adaptativo): fps / desv. / tarde / lejano / nivel | comun fps / tarde |
|---|---|---|---|
| PALM | 39,12 / 2,73 / 53 (1,8 %) / 2565 m | 39,15 / 2,68 / 49 (1,7 %) / 3714 m / 1,39 | 38,90 / 53 -> 38,92 / 49 |
| INDU | 39,38 / 2,07 / 38 (1,1 %) / 2748 m | 39,59 / 1,46 / 17 (0,6 %) / 3800 m / 1,02 | 39,06 / 34 -> 39,25 / 17 |
| METR | 37,20 / 4,09 / 238 (5,7 %) / 2112 m | 37,27 / 3,51 / 194 (6,0 %) / 1664 m / 7,21 | 36,98 / 238 -> 36,99 / 194 |
| RUIN | 39,61 / 2,33 / 8 (0,2 %) / 3044 m | 39,80 / 0,57 / 4 (0,1 %) / 3577 m / 0,31 | 39,40 / 8 -> 39,49 / 4 |

  En PALM, INDU y RUIN el lejano queda en 3800 casi siempre con los mismos fps o mejores. METR (noche) no llega a 25 ms
  ni con todo recortado (la geometria cercana de la ciudad, ~10 ms de etapa con 600 m): el nivel acaba en la emergencia
  y el lejano baja mas que antes (de noche casi no se ve); sin la llegada (la escena de la meta, ~130 tarde en las dos),
  63 cuadros tarde frente a 109. Pendiente aparte: en PALM, tras leer SEG05, los objetos de la union (bo_dibuja) gastan
  14-18 ms durante ~2 s (32 fps con cualquier nivel); el tope de subida evita que ese pico se lleve el lejano al fondo.

### Rankings y records (Rev 2.9.4)

Tres cambios de paridad (SLES-53998, analisis estatico del ELF). Sin verificar en Flycast ni en consola real en esta
entrega: compilado; la logica nueva de comun/rksp.c probada en el anfitrion (insercion, RKTA <-> 16 bytes, RKU2 de ida
y vuelta). Datos de prueba nuevos (no van al repo): GOALT.DAT y TABLERO.DAT regenerados en /dev/shm/datos294/ (pasos
`fin` y `tablero` de tools/regenera.py, o `python3 -I tools/conv_fin.py <iso> <dir>` y
`python3 -I tools/conv_tablero.py <iso> <dir>`). Con los ficheros de antes todo sigue funcionando sin lo nuevo.

**1. Tiempo del fantasma en la meta de la Contrarreloj de una etapa** (demo/fin.c fin_tabla_*, demo/main.c
tabla_entra / modo 19, tools/conv_fin.py tabla()):
- Original: el GAD de la meta 0x19e6d0 (= 498670 del PC; modos 19 y 22; 0x19f2f8 en los 20 / 21) tiene, antes de la
  tabla de etapas, la rama 0x19e918 `if (v == 7 && 0x21cb28())` con 0x21cb28 = 48B350 = `[0x373e54] < 0x3C` (el codigo
  del curso): en la CONTRARRELOJ DE UNA ETAPA no hay tabla 0x1a05f8 ni panel 2C00AF, sino la escena 2B0063 ("MEJOR
  TIEMPO:", banco 0x2B, cuadros 0..0x46) y el tiempo del registro 2 del fantasma. Pasos [0x577c86] (0x3ef290): 0
  (0x19e960) 0x20a010(2) ('NHOT' en el registro 2; sin el, 0x1b4a10(0): el modo acaba) y 0x114d00(0x2B0063, capa 0,
  modo 1, 0, 0x46); 1 (0x19e9ac) mientras 0x115230(h) == 1 el temporizador a 0xB4; 2 (0x19e9cc) mientras suene la voz de
  GOAL (0x1b8e10 = 49A190) lo mismo, despues el efecto 0xB0 una vez y en cada paso 0x19fe08(0xCD, 0xF5, blanco)
  (= 497430: 466190(2) en ms, fuente 4, "% 2d'%02d\"%03d", 20 px por caracter) hasta 0x1b4a20() < 10; 3 (0x19ea24)
  0x114eb0(h). El registro 2 lo pone 467C60 en cada stage clear si la vuelta mejora: en la meta es min(record anterior,
  esta carrera).
- Correccion: la escena es la **2B0063**; la 2B0061 del PS2 es el rotulo "Posicion" del puesto (0x1c02d8). La 2B0061 del
  PC es otra numeracion. (Lo que decia la seccion del fantasma, Rev 2.9.2, queda corregido.)
- La DC ensenaba en la contrarreloj de una etapa la tabla de etapas y T_PANEL7: **fallo de paridad corregido (cambio
  visible)**. Ahora (fin_tabla_t.solo = variante 7 y codigo < 0x3C): GOALT.DAT trae el grupo T_MEJOR (2B0063, 71
  cuadros, una textura de 64 KB); fin_tabla_entra entra en el primer paso de la meta (no a los 0xB4), sube solo
  T_MEJOR y T_CH y dibuja la escena (cuadros 0..0x46) y, acabada, el mejor tiempo en (205, 245) en blanco con T_CH;
  fin_tabla_quita (paso 3) la quita cuando al modo le quedan 10 cuadros. mejor_ms = min(gh_rep_cab.total_ms, esta
  carrera) (gh_mejor), el equivalente del registro 2 (tambien cuando gh_meta no guardo por gh_lleno). Con solo, el HUD
  (hud_aviso("META")) no se baja al entrar sino a los 0xB4 cuadros, como antes. Las metas A..E, las del reves y los
  cursos seguidos (codigo >= 0x3C) y la variante 1 siguen igual.
- DESVIACION: sin la espera de la voz de GOAL (el tiempo sale al acabar la escena, quiza antes que en el PS2) ni el
  efecto 0xB0. Con un GOALT.DAT de antes (sin T_MEJOR) se ensena solo el tiempo y sale un aviso en el registro.
  SIN PORTAR: la copia de los modos 20 / 21 (GAD 0x19f2f8, fin del tiempo: PROBABLE que no se llegue nunca).
- tools/conv_fin.py comprueba el codigo del PS2 (0x19e96c / 0x19e984 y 0x19f57c / 0x19f594 = 2B0063, 0x46, 0xCD / 0xF5,
  fuente 4, formato 0x3ef3c8, 0x3C de 48B350, < 10 de 0x19ea10) y, con dir_previas, hace la maqueta tabla_mejor.png.
- Probar: `-DPRUEBA_AUTO -DPRUEBA_VARIANTE=7 -DPRUEBA_TA_CODIGO=0 -DPRUEBA_MILLAS -DPRUEBA_TABLA` con el GOALT.DAT nuevo:
  dos carreras (REINTENTAR); la primera ensena su tiempo, la segunda el menor; con `-DPRUEBA_TA_CODIGO=0x3c` (meta A)
  la tabla de siempre; con `-DPRUEBA_VARIANTE=1` sin cambios. En el registro: "FIN: Contrarreloj de una etapa, MEJOR
  TIEMPO n ms, VRAM libre nK".

**2. Lista de records en los tableros de la CONTRARRELOJ** (comun/menu.c tb_lista_*, tools/conv_tablero.py):
- Original: la lista 0x267120 (= 51E080 / 51C050 del PC; registros de 0x14A4 bytes): 0x267210 reset (modo 1: 5 filas a
  la vista y las flechas 0x440034 cuadros 0..0xa, capa 8, modo 3), 0x2674d8 anade, 0x269880 rellena, 0x268910 dibuja
  y 0x268d88 da el cuadro de cada columna con la tabla 0x41dcb8 (150 x {token, primero, ultimo}, = 5E70F8 del PC con
  los tokens del PS2) y las locales 0x41db18 (B30), 0x41db38 (B16) y 0x41db48 (D30). Tablero 0x26 (0x24f620 /
  0x250c80 / 0x250e2c): 10 filas de clase 6; 0x27 (0x2582d8..0x258388): 10 de clase 7. Cada fila en (99, i x 36 -
  100): fondo 0x440035 (0..10 de entrada, capa 4, luego el 10), puesto 0x440040, cambio 0x440037, bandera 0x440038,
  configuracion 0x440039, coche 0x44003a (D30), ruta 0x440041 (B16; cuadro 31 con el curso seguido); nombre (fuente
  0xe) en (316, 124 + 36 i) y tiempo "%2d'%.2d'%.3d" en (484, misma y). Datos sin red: las tablas 0x2804 / 0x9154 /
  0x4d84 de la partida (0x269b60 = 51D0F0).
- DC: TABLERO.DAT lleva el grupo de atlas 'L' (escenas 0x35, 0x40, 0x37, 0x38, 0x39, 0x3a, 0x41, 0x34: una textura VQ
  de 512x256) y, detras de 'TAB1', el bloque 'LST1' (la tabla 0x41dcb8, B30 / B16 / D30 y las constantes 99, -100, 36,
  124, 484, 316, 5 y 82, todas comprobadas en el codigo por el conversor). tb_abre lo usa si esta (con un TABLERO.DAT de
  antes: sin lista, como en la 2.9.3). TABLERO.DAT pasa de 959680 a 1023872 bytes (animacion recortada 134 KB); medir en
  Flycast el dbglog de tb_abre (tablas en el hueco de 256 KB y VRAM libre: antes 481..516 KB).
- Que lista: metas A..E (0x3c..0x45) de rkuj (las dos tablas de cambio mezcladas por tiempo, las 10 mejores);
  metas del reves, cursos seguidos y etapas de RKTA (ver 3). La lista sigue a la posicion del carrusel, a R y al juego.
- Control: abajo lleva el foco a la flecha de abajo y luego a la de arriba (cuadros 0xb / 0xc; 0xa sin foco), arriba lo
  deshace, A con el foco en una flecha pasa de pagina (puestos 1..5 / 6..10); con el foco en la lista el carrusel no se
  mueve y el cursor 0x4400f2 no se ve; B vuelve el foco al carrusel (PROBABLE). En la etapa 0 de 0x27, abajo sigue
  siendo la opcion B si esta desbloqueada; DESVIACION: abajo con la opcion B puesta la quita y lleva el foco a la lista
  (el original solo la alterna y las filas 6..10 de la etapa 0 no se podian ver).
- DESVIACIONES: el cambio mezclado (el PS2 tiene una tabla por meta con el cambio en el registro), nombre de 4
  caracteres, filas vacias con fondo y puesto sin texto, sin la fila resaltada (cuadro 11), sin el deslizamiento al pasar
  de pagina (51C220), columnas sin la entrada animada (solo el fondo); el tick de cada fila no esta desensamblado en el
  PS2 (se toma el 51DB70 del PC). Los textos de las filas (0x268a74..0x268b00 / 0x26b758): fuente 14 en (316 / 484,
  124 + 36 x fila) con las banderas 1 de 0x116aa8 (sin alinear, la y arriba de las letras; antes 9, centrado en
  vertical, y salian subidas y cortadas por el borde de la fila). La bandera de la fila se limita a 0..25 (0x41dcb8:
  26 cuadros de 0x440038; DESVIACION, como tarjeta()). SIN PORTAR: la barra de ayuda con lista, la lista con red (0x226df8 / 0x5f8598), "Total players", filtros,
  rotulos (60, 342) / (60, 82), las listas del tablero 0x25 (0x440065 / 0x4400a4) y la del OUTRUN (0x24).
- Probar: con el TABLERO.DAT nuevo, UN JUGADOR -> CONTRARRELOJ -> CURSO / ETAPA; mover el carrusel, R, abajo y A.
  `-DPRUEBA_TABLERO_TODO` desbloquea las ETAPAS y METAS.

**3. rk_un_jugador revisado y records de la CONTRARRELOJ por codigo** (demo/main.c, comun/rksp.c, comun/menu.c,
demo/nombre.c):
- Correccion de atribucion: 0x5f8598 / 0x2270a8 / 0x226df8 / 0x227310 son el cliente de los rankings EN RED (DNAS; una
  cola de 16 peticiones de 0x14 bytes que despacha 0x227128; 0x226fc0 acepta los id 0x21..0x74, uno por codigo de la
  Contrarreloj 0x00..0x53; 0x1bbcac..0x1bbfb8 sube el registro al acabar). Las tablas locales del PS2 son
  \Common\def_orhiscores.bin (0x3ef798), cargadas por 0x1a18e8 / 0x1a1910 en 0x577d44 (PROBABLE, formato sin leer).
  0x1a65c8 / 0x1a7180 (47EE70 / 47EF30) son los records por etapa de la variante 0 de OUTRUN2SP (QHOT), no los de la
  consola.
- rk_un_jugador: los corazones saturan a 0xfff (antes `& 0xfff` daba la vuelta pasados 4095; tambien en demo/nombre.c;
  DESVIACION: el original guarda 12 bits); sin la variante 2 con los presets 2 / 3 (pisaria las tablas de la variante
  0); el coche es el de la salida (rk_modelo = menu_modelo() junto a manual_; antes se leia al final y dependia del
  estado del menu); tras insertar marca la licencia con cambios sin guardar (menu_marca_cambios, +3F4 |= 2).
- RKTA (nuevo): los records de la CONTRARRELOJ de las etapas sueltas (0x00..0x3b), metas del reves (0x46..0x4f) y cursos
  seguidos (0x50..0x53): 74 tablas de 10 filas de 8 bytes (u32 ms en los bits 0..22, modelo << 23, bandera << 27;
  nombre de 4 con el cambio en el bit 7 del primero), insercion con < estricto. rk_un_jugador los registra al llegar a la
  meta (no con la opcion B de la etapa: corre la etapa normal). Las metas A..E siguen en rkuj.
- VMU, fichero version 4: 'RKSP' (igual), 'RKU2' (los records de UN JUGADOR compactos: solo las 42 tablas usadas,
  6720 bytes en lugar de las 0x2bc0 de 'RKUJ') y 'RKTA' (5924 bytes), los dos con u32 de tamano detras de la marca. Se
  siguen leyendo 'RKUJ' y los ficheros de antes; los lectores de la 2.9.3 paran en 'RKU2' (conservan 'RKSP', pierden
  los records de UN JUGADOR). Tamano con VMU.DAT: 66 bloques con todo (64 la 2.9.3, 42 la version 2).
- RANKINGS: con UN JUGADOR, R pasa por un cuarto modo CONTRARRELOJ ETAPA (izquierda / derecha: 15 etapas, metas del
  reves A..E y curso seguido; X el sentido; Y el juego) con el tablero de la entrada de nombre (nombre_t consulta 3 con
  tabla16 = rkta_fila16) y el fondo del registro de la etapa: solo distingue las metas (RANKSP.DAT 0x40d248: las
  etapas que no son meta llevan todas el 0x37000C / 0x37000D de la meta A), asi que la posicion va en un rotulo de la
  banda ("ETAPA n NORMAL / REVES", "META x REVES", "CURSO ...") y el titulo va sin la linea "META A". DESVIACION (el
  original los ensena en la lista de los tableros y en la red).
- Meta de la Contrarreloj de una etapa (tabla_solo): la escena MEJOR TIEMPO se abre una vez por carrera (tabla_hecha):
  antes se volvia a abrir en las millas (fin_tabla_sale la quitaba y con solo el umbral es el cuadro 0) y sin GOALT.DAT
  se reintentaba en cada paso; el HUD se baja a los 0xB4 cuadros tambien si la tabla no se pudo abrir. Medir en la
  consola: entre el cuadro 0 y el 0xB4 la VRAM libre baja a 45..57K (HUD y MEJOR TIEMPO a la vez).
- Probar: con licencia, `-DPRUEBA_VARIANTE=7 -DPRUEBA_TA_CODIGO=0 -DPRUEBA_MILLAS` sin `-DPRUEBA_AUTO` no registra con el
  piloto: correr con el mando una etapa (registro: "RANKING: contrarreloj codigo 0x00: n ms, puesto 1 (RKTA)"),
  guardar (Y), reiniciar y ver la fila en el tablero de ETAPAS y en RANKINGS -> UN JUGADOR -> R x 3. `-DPRUEBA_FIN=m`
  y `-DPRUEBA_SIN_TIEMPO` para acabar antes / llegar a la meta.

### Auditoria del usuario (Rev 2.9.3 / Mod 2.5.3, 2026-11-23)

Pedido: "audita revisa y corrige" (lista de fallos de la 2.9.2.f). Verificado en Flycast (no en consola real).
- Faros de noche que se encendian y apagaban solos: el salto progresivo (2.9.2.e) quitaba los faros y los focos desde el
  nivel 1, que sube y baja durante la carrera. Ahora no dependen del salto (demo/main.c faros_dibuja / focos_prepara).
- "Zonas azules" en INDUSTRIAL COMPLEX y "bloques blancos" en ANCIENT RUINS: la version lejana lowpoly de la 2.9.2.d (12 %
  de los triangulos, el color medio de cada textura) desde 900 m se veia como bloques planos oscuros (forzada de cerca:
  fachadas negras y gris azuladas) o, con la niebla, claros. LOD_GRANDE_FR (0,8): el completo hasta el 80 % del lejano
  (2560 m con 3200) y el lowpoly solo en el ultimo tramo, dentro de la franja de niebla. LOD_GRANDE_D (900) queda como
  minimo. El hueco blanco al fondo del desfiladero de RUIN sale igual sin lowpoly y sin franja: es el limite de 3200 m.
- CONTRARRELOJ sin el derrape de la salida: demo/meta.c callaba los efectos de la escena de salida (op 5) con la
  variante 6 / 7 tomando 450240 por la variante; 450240 es el estado del fin de la carrera ([7D38F0] del PC, 0 en la
  salida, 450230 lo pone al acabar): los efectos suenan en todos los modos.
- SALON, COLORES DEL COCHE: el coche que se ensena es el de 0x41ba30[fila] (0x25ce50: [+0x410] x 8); la DC sumaba el
  0x49 de la tabla de rotulos y ensenaba el coche y el color de 73 filas mas abajo. Comprobado fila a fila (DINO 246 GTS,
  365 GTS/4 DAYTONA, F50 en sus colores).
- SALON, cabecera: el original pone la de la clave 0x2B de 0x416B70 (0x23D5B8 -> 0x440077, "showroom"); la DC dejaba la
  de ELEGIR MODO. Va en SALON.DAT (grupo COMUN de tools/conv_salon.py; MENU.DAT lleva el arte de OUTRUNNERS hecho
  aparte y no se rehace); con un SALON.DAT de antes, la de ELEGIR MODO.
- Ecualizador de la radio (MUSICA): la escena 0x4400A7 es una sola pieza de un cuadro y los titulos (0x44002E..30)
  solo mueven las letras (que la DC ya animaba); el usuario ensena en un video del PS2 la rejilla de puntos de la radio
  encendida por columnas. No encontrado donde lo dibuja el PS2 (PROBABLE: la capa de 640x480 sin cuadros, n = 0, de la
  escena de la lista 0x4400C6, que rellena el codigo). DESVIACION (comun/menu.c eq_dibuja): la rejilla (20 columnas de
  3 px cada 4 desde x = 267, 5 filas de 1 px cada 2 desde y = 252, medida en la escena) se enciende en el azul de los
  titulos (66, 150, 204) con alturas al azar (mas altas en los graves), no sacadas de la musica.
- Bocina en X (pedido): el PS2 no tiene bocina; el PC si (boton 2, [82F23C] -> la acompanante dice la voz 0x163 en el
  modo OutRun, 489A90). DESVIACION: X ya no frena (B y el gatillo L si) y es la bocina: sonido propio sintetizado
  (comun/sfx.c sfx_bocina, dos tonos de 400 / 500 Hz en PCM de 16 bits en bucle mientras se pulsa) y en el modo OutRun
  la voz 0x163, que esta en HC.PAK del PS2 (tools/conv_sonido.py la anade a VOCES.DAT; tools/regenera.py rehace el
  sonido si VOCES.DAT no la tiene). Probado en Flycast (sin sonido en la prueba): 18 bocinazos en 90 s sin fallos.
- Todas las voces de la acompanante (pedido): asustada (al pasar rozando el trafico a mas de 120 km/h) y trompo (choque
  del estado 7), que el original solo dice en COAST 2 COAST, y 0x71 / 0x78 (en ORC.PAK sin uso en las tablas) en el
  sorteo de los derrapes. DESVIACION.
- CONTRARRELOJ de una etapa final (el "partenon encima de la bifurcacion", captura del usuario en CAPE): el registro de
  44D720 lleva la union 0 con hijos 0 / 0 (la bifurcacion) y el decorado de la meta de la etapa quedaba encima.
  DESVIACION (demo/main.c ta_meta_final): en una etapa que es final en algun arbol de CURSO.DAT / CURSOTA.DAT, su union
  de meta (CAPE: BK14 en vez de BK276), sin rehacer datos. Verificado en CAPE: la meta con el templo detras del GOAL.
  Las etapas no finales (1..12) acaban en la recta de la union 0 como antes; las metas A..E (meta C por el menu) sin
  cambios.
- INFARTO, corazones al adelantar: funcionan (prueba con el piloto: peticion de adelantar, +217 y +247 corazones, anillo
  y corazones volando). En el modo OutRun el original no da corazones.
- (Anadido en la Rev 2.9.4, ver Rankings y records) La Contrarreloj de una etapa no ensena la tabla de etapas en la meta
  (0x19e918 / 0x21cb28) sino 2B0063 "MEJOR TIEMPO:" y el tiempo del fantasma; los records de UN JUGADOR de la 2.9.3
  atribuian a 0x5f8598 / 0x2270a8 la base local y son los rankings en red; los corazones de INFARTO daban la vuelta
  pasados 4095.

### Franja de niebla en el limite y edificios lejanos (Rev 2.9.2.f / Mod 2.5.2.f, 2026-11-23)

Pedido: "se aprecian calvas al final de la distancia de dibujado ... como una niebla en el limite de dibujado" y "hay
algunos edificios que no renderizan ahora y antes si".
- Niebla del limite (niebla_pon): con el lejano en su maximo (LEJOS_MAX, 3200 m) no habia fundido (la condicion venia de
  cuando el maximo era el 4000 del original): el corte se veia seco. Ahora siempre una franja del NLIM_FRANJA (0,85) x
  lejano al lejano que lleva lo de la escena al color de la niebla de la etapa (acortado por la CPU, desde 0,55 como
  antes); de noche (cielo mas oscuro que la niebla) con la fuerza de la luminancia del horizonte / la de la niebla.
- Edificios: tools/lod_grande.py dejaba en la version lejana solo los triangulos opacos; en las etapas de ciudad las
  fachadas con ventanas van en la lista de recorte y de lejos el edificio se quedaba en el esqueleto o casi vacio (en
  NEWY 53 de 364). Ahora opacas y de recorte, con la textura reducida a la media de sus texeles opacos. En las 560
  etapas y uniones: 22836 nodos, 6.551.544 -> 466.476 triangulos.
- Salto progresivo mas suave: tope del TA al 94 / 87 / 80 % (antes 90 / 80 / 70 %), que en lo pesado dejaba fuera
  edificios lejanos.

### 40 fps fijos y salto progresivo (Rev 2.9.2.e / Mod 2.5.2.e, 2026-11-23)

Pedido: "frameskip progresivo e intentar que los fps se mantengan fijos a 40". DESVIACION. FPS_OBJETIVO 45 -> 40 (a 60 Hz
un cuadro cada 1 y 2 campos alternos; a 50 Hz 4 cada 5), CPU_MS_CORTA / ALARGA 21 / 17 -> 24 / 20 ms (cuadro de 25 ms).
Salto progresivo (demo/main.c, SALTO_*): cada 40 cuadros dibujados en carrera, con 2 o mas que empezaron tarde un nivel
mas, con 3 ventanas seguidas sin ninguno uno menos; nivel 1 sin los focos de los faros sobre la escena ni la luz del
suelo y el tope del TA de la etapa al 90 %, nivel 2 al 80 %, nivel 3 al 70 % y el lejano como mucho a 2400 m. La logica
sigue a 59,94 pasos por segundo (un cuadro tarde no frena el juego). Medido (Flycast, piloto, ~100 medidas):
PALM 41,8 fps (desviacion 3,80, 178 cuadros tarde) con 45 sin salto -> 39,2 (2,63, 53); LASV 41,4 (4,00, 196) -> 39,4
(1,41, 30). `-DSIN_SALTO`, `-DFPS_OBJETIVO=45`: como antes.

### Version lejana lowpoly de lo grande (Rev 2.9.2.d / Mod 2.5.2.d, 2026-11-23)

Pedido: "modelos lowpoly y texturas de baja resolucion para los elementos grandes como puentes, montanas, bloques de
edificios, ruinas, piramides ... cuando el coche este a 900 m cargara completo". DESVIACION. tools/lod_grande.py (detras
de lod_veg.py en conv_tramo / conv_union; en una carpeta ya hecha `regenera.py --interno lodgrande`, que el programa de
Windows clasico pasa al final y el del taxi sobre los datos de tu GD):
- candidatos: nodos de radio >= 40 m, al menos el 60 % de sus vertices en la lista opaca, >= 6000 bytes teoricos del
  TA, no carteles ni con version lejana de vegetacion;
- version lejana: solo sus triangulos opacos, agrupados en una rejilla que crece hasta el 12 % de los triangulos
  (24..400), en tiras voraces, sin textura: la textura reducida a 8x8 cocida en el color de los vertices (textura x
  color / 255, como MODULATE): 0 KB de VRAM;
- en el fichero como los lejanos de lod_veg.py (NODO_LEJ + NODO_LEJ3D 0x40, cartel -1, el completo con cartel = -2 -
  lejano; la PVS reindexada).
En las 560 etapas y uniones: 18725 nodos grandes, 5.500.137 -> 451.643 triangulos (8,2 %); como mucho 89 por fichero y
~100 KB de vertices mas (el hueco de la etapa crece: RAM libre en el menu 575 -> 444 KB, OUTRUN2SP 505 KB, sin "Out of
memory"). demo/main.c nodos_visibles: con NODO_LEJ3D el lejano si lo mas cercano del nodo esta a mas de LOD_GRANDE_D
(900 m) por delante, el completo al acercarse (histeresis 50 m). Medido (Flycast, piloto, OR2 rama derecha, PRUEBA_SALTO
1500): 5-11 grandes en lowpoly por cuadro en PALM; apariciones por el tope del reparto 1566 -> 1369 (-12,6 %); 42,7 fps
igual (lo que se ahorra de lejos lo gasta lo que antes no cabia). `-DLOD_GRANDE_D=m`: otra distancia.

### Little Bloste y lejano a 3200 m (Rev 2.9.2.c / Mod 2.5.2.c, 2026-11-23)

Pedido: quitar SH4ZAM y hacer las cuentas con una herramienta propia, Little Bloste, documentada; y el lejano maximo a
3200 m. comun/lbloste.h (docs/LBLOSTE.md): carga / guarda de la XMTRX con FRCHG y FMOV.S, FTRV y el producto de matrices
de filas, con su version en C (-DSIN_LBLOSTE). Los tres usos de antes (recorte de nodos, UV del entorno del coche,
matrices de los personajes) con las mismas cuentas. Autoprueba (-DLBLOSTE_PRUEBA, 4000 casos): XMTRX exacta al guardar
lo cargado, error maximo ~2e-7 de la suma de los terminos (como la cuenta en C). PALM: 41,50 fps (C: 41,31). LEJOS_MAX
3500 -> 3200 m. SH4ZAM fuera de tools/instala_entorno.sh y de windows/construye.sh (y su licencia del paquete).

### Lejano a 3500 m (Rev 2.9.2.b / Mod 2.5.2.b, 2026-11-23)

Pedido: "reducir la distancia de dibujado maxima de 4000 a 3500 de forma global". DESVIACION. LEJOS_MAX (3500) es el techo
del plano lejano en la carrera, la salida, la meta (el op 20 del guion de la meta, 4000, se recorta) y al volver de cada
escena; LEJOS_PS2 (4000) queda como el del original para el proyector de los faros (40A950) y las cuentas. El recorte
por CPU sigue igual (minimo 600 m, -50 m por cuadro por encima de 21 ms, +50 m por debajo de 17 ms). Medido en PALM
(Flycast, piloto, 95 muestras): lejano medio 2203 -> 1750 m, 41,5 -> 41,7 fps (dentro de lo que varia una prueba).
`-DLEJOS_MAX=4000.0f`: como antes.

### Fantasma simplificado, menos repetidos, menos trafico y la meta desde la vista 1 (Rev 2.9.2.a / Mod 2.5.2.a, 2026-11-23)

Pedido del usuario (todo DESVIACION salvo la meta):
- **Fantasma**: "que no tengan color, menos poligonos sin textura, que conserven su traslucidez" (con el coche entero el
  TA no daba para la etapa). tools/conv_coche.py `fantasma`: bloque GHO1 con las caras de las envolventes de los trozos
  convexos de los volumenes de sombra sin las interiores (182-566 triangulos segun el coche); demo/main.c gh_dibuja: sin
  textura, gris azulado con la luz de la normal, alfa 0,44, lista transparente. PALM (piloto): 42,8 -> 42,2 fps con el
  fantasma a la vista (con el coche entero de la 2.9.2: 41,2). Los COCHEnn.DAT sin GHO1 (de antes) no dibujan fantasma.
  GHO1 con los vertices en mm (i16, 20 bytes por triangulo) y COCHE_RAM 480 -> 496 KB: el mayor, COCHE13, pide 480 KB
  (con GHO1 en f32 no cabia en el bloque y la musica del menu daba "Out of memory").
- **Repetidos y segunda fila** (decor_reduce, todas las etapas, al leer): vegetacion 15 -> 30 % ("10 palmeras -> 7");
  objetos iguales repetidos fuera de la calzada (misma firma: vertices, texturas y medidas; no pegados a otro igual ni
  con algo grande encima) 1 de cada 3 ("3 carteles -> 2"); edificios tapados por otro del mismo lado mas cerca de la ruta
  (80 % de su tramo, 70 % de su alto) 1 de cada 3. Medido (OR2, rama derecha): PALM 37 de 109 repetidos y 1 de 3
  edificios, ALPI 1 / 1, DESE 10 de 39, GHOS 4 de 10, CAPE 2 de 6 y 1 de 3. Capturas del recorrido: sin huecos.
- **Trafico -5 %**: 1 de cada 20 aperturas del trafico de la calle (478B90) se gasta sin coche (TRAF_MENOS; no los
  puntos fijos ni los coches de mision).
- **Meta desde la vista 1** (fallo): 4A2BA0 solo usa la vista de carrera en el modo 16; en la salida, la meta y el fin
  del tiempo el coche va entero (coche_modo). Antes la escena de la meta desde la vista 1 dibujaba el habitaculo.
  Verificado en E (`-DPRUEBA_VISTA=1`, meta en IMPE): el derrape entero.

### Coche fantasma de la CONTRARRELOJ (Rev 2.9.2 / Mod 2.5.2, 2026-11-22)

El original (PC 465F30..4687C0; SLES-53998 GHOST.PS2 y la tarjeta de memoria) graba en la Contrarreloj el coche del
jugador (4671D0 / 466E50), en la meta guarda el mejor por curso (467880 / 467960) y en la carrera siguiente de ese curso
lo reproduce (467E30) con un coche translucido (4AE0F0: alfa +B68). En la Dreamcast (demo/main.c gh_*):
- Grabacion propia (DESVIACION del formato): una muestra de 12 bytes cada 8 pasos de logica en el marco de la carretera
  (etapa del arbol, etapa o union y rama, distancia desde el principio de esa parte, x lateral, rumbo, deriva, velocidad);
  se reproduce interpolando y se coloca como el trafico (coche_coloca / pon_coche). PALM entera: 533 muestras (6,4 KB).
- El mejor por curso (codigo del tablero + preset) en la RAM y en la VMU: fichero ORGHppcc (9 bloques una etapa; sin la
  imagen de presentacion), escrito en un hilo durante la escena de la meta si mejora el tiempo; al empezar una carrera
  de otro curso se lee de la VMU. comun/menu.c menu_vmu_guarda / menu_vmu_lee.
- Dibujo (DESVIACION): con el modelo del coche del jugador (un segundo COCHEnn.DAT no cabe en la RAM ni la VRAM), sus
  piezas animadas, en la lista transparente con alfa 0,5 (cabeceras recompiladas para esa lista; el alfa se pone en los
  vertices durante su dibujo para que vaya por la ruta directa). Medido en PALM (Flycast, piloto): 42,7 -> 41,2 fps con el
  fantasma a la vista (33,8 con el alfa por color_mul, descartado).
- Solo se ve en la parte de la ruta cargada (la etapa o union actual, la siguiente o la union anterior) y por la misma
  rama del jugador.
- Verificado en E (`-DPRUEBA_AUTO -DPRUEBA_VARIANTE=7 -DPRUEBA_TA_CODIGO=0 -DPRUEBA_MILLAS -DPRUEBA_GH_ADELANTO=15`,
  REINTENTAR con el mando): primera carrera graba y guarda ORGH0100 (releido igual), la segunda lo reproduce delante.
  `-DDIAG_GH`: donde lo pone; `-DGH_OPACO`: opaco.
- HECHO (Rev 2.9.9.c, ver Modos "sp" 3): el coche de la grabacion. SIN PORTAR: el fantasma de la red, la escena 2B0063 (no
  2B0061: esa es "Posicion" en el PS2) y el tiempo del fantasma en la meta (497430): portados en la Rev 2.9.4 (Rankings y
  records); la pantalla RANKINGS (siguiente revision).

### Vista interior y tabla de tiempos de la meta (Rev 2.9.1 / Mod 2.5.1, 2026-11-22)

Pedido: de la lista de lo que falta respecto a PS2, "1 y 2" (el fantasma y el ranking / la tabla de tiempos, y la vista
interior); numeracion 2.9.1 en adelante, 3.0 cuando este todo.

**Vista interior (vista 1).** El original (4A2BA0, PC driving/pc_camera.cpp) pone car+4 & 0xC000 segun la vista: 0x4000
(vm = 1, el coche entero) en las vistas 2.., 0x8000 (vm = 2) en la 1 y en la mezcla de vistas si el ojo esta dentro de la
caja del coche (482E80), nada en la 0. Con vm = 2 el dibujo del coche (46AE70 / 469600) no dibuja la carroceria ni las
ruedas: solo la pieza +04 de la disposicion del modelo (el habitaculo: salpicadero, cuadro de instrumentos y capo;
1224-4347 vertices segun el coche), con la matriz local identidad (sin car+F0), y los faros escamoteables +64; sin
pilotos, sombra ni la pareja (487880 solo con 0x4000). Antes la demo no dibujaba nada en la vista 1.
- tools/conv_coche.py: la pieza +04 entra en el modelo como pieza P_INTERIOR (13) del grupo G_INTERIOR (7): no tapa en el
  vaciado y no cuenta en el tope de vertices (va sola); `vacia_interior` le quita lo que no se ve desde el ojo de la
  vista interior (tabla CAM1 por modelo; camaras a +-0,15 m girando +-50 grados y de -25 a +15: 4347 -> 3588 vertices en
  el Dino). Texturas: 0-34 KB mas por coche. RAM: el bloque fijo del coche (COCHE_RAM) pasa de 424 a 480 KB (el mayor,
  COCHE13, pide 474 KB); antes de agrandarlo el coche se salia del bloque al monton y la musica del menu daba "Out of
  memory". Medido con nav3 y sp: libre en el menu 684 KB, 675-835 KB tras las lecturas, sin "Out of memory".
- demo/camara.c `cam_coche_modo` (4A2BA0, sin el desvanecido +EA0 / +E9C al pasar de la 1 a la 0); demo/main.c
  `coche_dibuja(l, mc, interior)`: con 1 solo PZ_INTERIOR y PZ_FAROS; con 0 todo menos PZ_INTERIOR.
- Verificado en E (Flycast, `-DPRUEBA_VISTA=1`, piloto en PALM): capo y salpicadero desde el asiento, sin "Out of memory".

**Tabla de tiempos de la meta.** El GAD de la meta (PS2 0x19e6d0 = 498670) en las variantes 0/1/7/8/9: cuando el
temporizador del modo 19 baja de 0x3FC (variante 1) y acaba la voz de GOAL, 0x1a05f8 (= 4979E0): una fila por etapa
cada 30 cuadros (5 a la vista, 38 px) con el fondo 2C000D, el rotulo "Etapa n" (0x3ef430), el icono y nombre de la etapa
(2C0047, cuadro = 0x35c7b0[descriptor de la zona]), la barra (imagenes 2 y 3 de spr_etc) y el porcentaje mientras se
llena y despues el tiempo de la etapa (4505A0: el crono al superarla menos lo de las anteriores, "% 2d'%02d\"%03d" con
la fuente 4); al acabar, el panel: variante 1 2C00AE con el tiempo total y la puntuacion amarilla deslizandose, variante
7 2C00AF con el tiempo. Detalle en demo/fin.c (fin_tabla_*) y tools/conv_fin.py (`tabla`: GOALT.DAT, con asserts de cada
constante del codigo de PS2 que la demo lleva escrita).
- demo/main.c: `etapa_acaba` en el stage clear (44FF20) y en GOAL (4513C0), `tabla_entra` en el modo 19 (variantes 1 y
  7 de la consola) a los 0xB4 cuadros; el HUD se baja de la VRAM al abrirla (el NAVI ya no dibuja).
- SIN PORTAR: los efectos 0xAD / 0x80AD (barra) y 0xB0 (fin de la tabla): estan en bancos de sonido de SOUND.PS2 que
  dependen de la etapa (sin localizar cual carga la meta); la espera de la voz de GOAL (49A190: comun/sfx.c no dice si
  suena); la variante 0 (records QHOT: el tiempo en verde y 2C00B7) y la 9.
- Siguiente (Rev 2.9.2): el coche fantasma de la CONTRARRELOJ y los rankings.

### El muro a la salida del tunel de ALAS (Rev 2.9 / Mod 2.5, 2026-11-22)

Queja: "un muro que se ve al final de la etapa de nieve en outrun2sp ... una zona de tunel, al final del tunel".
Reproducido en Flycast (piloto, arbol 1, nodo 8 = ALAS, SEG23, capturas cada 0,25 s): al salir del primer tunel el
hueco de la salida se tapaba con una pared con el ladrillo del tunel hasta pasarla. Es la malla 8147 del nodo 227
(textura 89): una placa de 24 x 8 m desde el asfalto que cruza la calzada en s 1100, cuya cara (la de la primera de su
tira) mira hacia delante: desde el tunel se ve por detras. La demo dibuja todo sin descartar caras (PVR_CULLING_NONE);
en el original no sale. No se borra: `tapas_busca` (al leer) marca las placas de una cara asi (4-8 vertices, planas,
verticales, que cruzan el centro de la calzada desde el suelo hasta mas de 3 m, con la normal a menos de ~37 grados
del sentido de la ruta, a mas de 8 m de su principio, no carteles ni versiones lejanas) y `tapas_paso` (cada cuadro)
solo las dibuja con la camara delante de su plano. En las 62 etapas y 498 uniones: SEG23 (esta), SEG26, SEG52 y SEG58
(estas tres no comprobadas en pantalla). `-DSIN_TAPAS`: como antes. Encontrada con tools/invasion.py y una busqueda de
placas perpendiculares a la ruta (docs/AUDITORIA_MOTOR.md).

### Huecos de RUIN y framerate de LASV (Rev 2.8 / Mod 2.4, 2026-11-22)

Pedido: texturas de los puentes y de RUIN a 8x8 / 16x16 de lejos y 32x32 de cerca ("ahi se crean zonas sin dibujar") y
mejorar el framerate de LASV. Medido antes de tocar nada (Flycast, piloto, RUIN 110 s): VRAM de sobra (793 KB libres,
0 texturas rebajadas) y el TA casi nunca lleno (1 % de media sin caber); de los 10 saltos grandes por s, 9,4 eran de la
PVS, de nodos de 84-288 m de radio (estructuras). Las texturas no eran la causa (bajarlas no cierra huecos; mipmaps
gastarian un tercio mas de VRAM): no se cambian.
- PVS_GRANDE (nodos_visibles): en las zonas de PVS_GRANDE_ZONAS ("RUIN") los nodos de 60 m de radio o mas no se
  recortan con la PVS (ni enteros ni por tiras) a menos de 800 m. RUIN: saltos grandes 10,0 -> 4,8 por s (PVS 9,4 ->
  3,3), 44,4 -> 43,7 fps. Para todas las etapas no: en LASV 41,3 -> 39,7 fps y lo que no cabe 3 -> 27 %; en MAYA los
  saltos por el tope 0,1 -> 3,4 por s; encenderla sola con holgura del TA o costaba en esas o se apagaba en RUIN.
- LASV (perfil del SH4 de noche: dibuja_etapa 25 %, envio de vertices 13 %, piloto y pareja 6 %, faros 5 %). Medido
  quitando cada cosa: agua 0, objetos 0, focos de los faros 0,65 ms, la luz de los faros en el suelo ~1 ms. La rejilla
  de la luz de los faros consulta la altura del suelo en 156 puntos por cuadro (1,34 ms): ahora la mitad de las lineas
  por cuadro, alternas, y las otras con la altura sobre el plano del coche del cuadro anterior (0,79 ms;
  `-DFAROS_TODAS` como antes); focos_anota descarta por distancia antes de los 5 planos. LASV 41,1 -> 41,7 fps, 20,3 ->
  19,8 ms de CPU. Bajar el tope del TA al 85 % en LASV (PRESUP_ZONAS) solo daba 40,1 -> 40,4 fps con el doble de saltos
  por el tope: queda apagado.
- NO verificado en consola real.

### Sin reflejo del entorno y con tiras de 6 (Rev 2.7 / Mod 2.3, 2026-11-22)

El usuario: "hay muchos tirones en etapas de noche como en las vegas, creo que el efecto reflejo ha sido
contraproducente, asi que vamos a deshacerlo ... y nos quedamos con T6". Medido en Flycast (piloto, LASV -> MAYA
150 s, mismas opciones): con el reflejo 38,5 fps de media, 22,1 ms de CPU y 607 cuadros tarde; sin el 41,4 fps,
20,1 ms y 386. Ahora:
- el reflejo solo con `-DCON_REFLEJO` (refl_busca no busca REFLEJO.DAT): la carroceria con el mapa fijo de cada coche,
  como la Rev 2.3; sin los ~118 KB de RAM de los mapas. tools/regenera.py y el programa del taxi ya no hacen
  REFLEJO.DAT (tools/conv_reflejo.py se queda para -DCON_REFLEJO);
- las tiras de la PowerVR en 6 por defecto (`-DTIRA_2`: como KallistiOS).

### Longitud de las tiras en las listas de objetos (prueba -DTIRA_6, 2026-11-22)

Pregunta: "si estamos poniendo el tamano de strip en 6". No: todas las cabeceras salen de pvr_poly_compile de
KallistiOS, que parte de PVR_CMD_POLYHDR (0x80840000): Strip_Len (bits 18-19) = 1, 2 triangulos por entrada de la lista
de objetos de cada tile. -DTIRA_6 la pone en 6 envolviendo pvr_poly_compile en el enlazado (demo/Makefile, como
pvr_scene_finish). Flycast no lo distingue (41,2 frente a 41,3 fps, PowerVR 7,5 ms, imagen igual): se mide en la
consola con la linea de diagnostico (Y en la pausa: fps, CPU, PowerVR y TA; "T2" / "T6" al final). Por defecto, 2.

### Un 5 % mas de decoracion de los margenes y sin deslumbramiento (Rev 2.6 / Mod 2.2 definitivas, 2026-11-22; DESVIACION pedida)

Pedido: "eliminar un 5 % de los arboles, arbustos, farolas, arboles con nieve, arbustos de desierto, palmeras, son
elementos que estan en los margenes de todas las etapas" (ademas del 10 % de la Rev 2.1) y quitar el deslumbramiento
("no esta logrando el resultado que espero"). Se publica con el mismo nombre, Rev 2.6 (y Mod 2.2).
- tools/decor_tabla.py: la vegetacion que el color no reconocia (nevada, de invierno sin hojas, de zonas secas,
  palmeras y arboles verdes oscuros) elegida a mano en una hoja de las texturas de recorte sin clasificar de las 62
  etapas (VEG_REF, por su contenido: tambien en sus copias), y las farolas (tipo 3, FAROLA_REF) elegidas por su silueta
  entre los nodos opacos finos (planta < 2,5 m, 5-16 m de alto) repetidos en cada etapa: de brazo, de doble globo, de
  farol. No: postes de vallas, troncos sueltos, rocas, mastiles ni columnas con algo encima. Tabla: 247 texturas de
  vegetacion (antes 213), 16 de farolas.
- demo/main.c decor_reduce: vegetacion 3 de cada 20 (15 %), farolas 1 de cada 20 (5 %, otros bits de la posicion), y
  con cada arbol quitado su tronco si va en otro nodo (no queda un palo solo).
- Medido (Flycast, piloto, PALM -> ALPI 150 s): PALM 86 de 575 de vegetacion fuera (antes 55) y 1 de 37 farolas; ALPI
  103 de 763 (antes 60 de 730). fps iguales (41,3 / 20,0 ms de CPU frente a 41,2 / 20,0: la escena va por el tope del
  TA); popping de PALM por el tope 2,1 -> 2,0 por s, presupuesto que no cabe 16 -> 15 % de media (155 -> 152 % el
  peor); ALPI igual.
- Deslumbramiento: fuera (`-DCON_DESLUMBRA` lo vuelve a poner).

### Lacado con brillos, deslumbramiento del sol y registro de la VRAM (Rev 2.6 / Mod 2.2, 2026-11-22; DESVIACION pedida)

Pedido: la opcion A del reflejo (mas contraste), "que los coches conserven su lacado con reflejo, aplicando brillos no
cambios de color, no quiero que los ferrari se vean mate", el efecto del original "cuando sale el sol y un haz se dirige
a la vision del jugador" sin bajar los fps, y el plan del material de investigacion de VRAM que paso el usuario.
- Reflejo (refl_compone): el mapa propio del coche a peso 1 (el lacado de la Rev 2.3) y del entorno solo lo que pasa de
  REFL_UMBRAL 150 de luminancia, como brillo blanco (cielo claro, horizonte, sol); lo medio y lo oscuro ya no tine la
  pintura. Textura de 32x32, horizonte nitido (x 1,5 arriba, x 0,25 abajo), sol cos^160 x 2, Fresnel 0,6 + 0,4
  (1 - |N.I|)^2 con la vista hasta cada vertice. Medido (pixeles rojos del Testarossa): saturacion 146-169 frente a
  108-131 del mapa fijo, brillo 115-134 / percentil 99 160-216 (mapa fijo 115-125 / 181-192): rojo mas vivo y brillos.
- Deslumbramiento (sol_deslumbra): en el video de referencia la imagen se blanquea, mas arriba que abajo, al ir hacia el
  sol aunque su disco quede por encima de la pantalla (el destello del original solo sale con el sol en pantalla, casi
  nunca con la inclinacion de las etapas). Un rectangulo de toda la pantalla en la lista translucida, blanco tenido del
  sol, alfa 0,45 x intensidad del sol x ((rumbo hacia el sol - 0,75) / 0,23)^2 x la altura (x 1 hasta 30 grados, x 0,3
  a 60) x lo que se ve del sol x el cielo (menos bajo techo); abajo x 0,3; suavizado (~0,2 s). Medido: PALM 27-33 s de
  carrera hacia el sol, alfa 0,19-0,35. -DSIN_DESLUMBRA.
- Rendimiento (Flycast, PRUEBA_AUTO, PALM y ALPI 150 s, media de cuadros > 30 fps): sin reflejo ni deslumbramiento 43,5
  fps / 18,0 ms de CPU; Rev 2.5 40,6 / 20,6; primer intento 35,3 / 24,8 (la mezcla casi cada cuadro con ~24000 divisiones
  enteras y 1024 powf); con tablas (sin divisiones), mezcla en pasos de 1/8, FSRRA y textura / vertices en cuadros
  alternos: **41,2 / 20,0** (PowerVR 7,5 ms en todos: el deslumbramiento no se nota). Mas rapido que la Rev 2.5.
- Registro de la VRAM (`-DDIAG_VRAMLOG`, docs/REGISTRO_VRAM.md): eventos alloc / free / fail / mark con el contrato del
  material del usuario. Captura PALM -> ALPI 150 s: 1020 eventos, 0 fallos, 0 avisos del analizador del usuario
  (AnalizarRegistroVram.ps1, PowerShell 7 en Linux); pico de reservas registradas 1643 KB en el cuadro 2583 (al leer
  SEG05 con SEG00 aun en la zona): pvr_mem fuera de la zona 662 KB (HUD 278, coche y modelos fijos 186, efectos 49...),
  etapas 473, trafico 322, uniones 148. La zona del curso (1280 KB) llego a 599 KB con SEG00 y 621 con SEG05: no se
  lleno. El libro reducido se reserva tal cual (demo/main.c sube_modelo, vram_pide; el dibujo apunta 2048 - 8 N antes).
- NO verificado en consola real.

### Reflejo del entorno en la carroceria (Rev 2.4 / Mod 2.0 y Rev 2.5 / Mod 2.1, 2026-11-22; DESVIACION pedida)

Peticion: volver a poner el reflejo en la carroceria, muy simplificado. Lo que no gusto de la Rev 1.3: "un cambio de
color muy abrupto y no se apreciaba bien el reflejo" (el mapa era solo el cielo x 0,3 sobre toda la pintura, con un
suelo inventado de borde duro, y el mapa del tunel entraba de golpe). Referencia: el video de OutRun 2 SP del usuario
(el original dibuja un cubo del entorno por cuadro con cs_ENV_<zona>: ver Reflejo del cielo y de los tuneles): bandas
suaves del color del cielo en lo que mira arriba y en los cantos (rejillas, tapa trasera, aleron, aletas), naranja al
atardecer, gris con luces en el tunel; de frente casi solo la pintura.
- Datos (`tools/conv_reflejo.py`, REFLEJO.DAT 'RFL2'): por etapa, un mapa de 16x16 (RGB565 twiddled, 512 bytes) cada
  12 puntos de su ruta (48 m) visto desde 1 m sobre ella: rayos (32 x 32, tools/c/reflejo.c) contra los triangulos de
  las listas 0 y 1 de la etapa a menos de 300 m (sin las versiones lejanas), cada uno con el color medio de su textura
  por el de sus vertices, y el cielo (el grupo 0) donde no chocan; desenfocado y reducido a 16x16, x 0,85. La misma
  proyeccion que antes (u = 0,5 + 0,4 dx / (1 + dy)). Sin tabla de tuneles: el techo de las galerias esta en los mapas.
- Demo (`refl_elige`, `refl_mezcla`, `coche_reflejo`): cada cuadro, la mezcla lineal de los dos mapas de alrededor
  (solo se rehace si cambia en 1/32), en el marco del mundo (cada mapa girado desde el de su etapa); en la union, del
  ultimo de la etapa al primero de la siguiente entre el 25 % y el final de lo andado. Dos texturas de 16x16 que se
  alternan (RGB565 y ARGB1555 para los cristales: 2 KB de VRAM), con una curva de contraste (REFL_GAMMA 2,2: el
  suelo oscuro, el cielo claro). Fresnel: el especular de cada vertice por 0,04 + 0,96 (1 - |N.I|)^3. Sin pasadas ni
  vertices de mas en el TA. Primer ajuste (0,15 + (1 - |N.I|)^2, sin curva): la tapa trasera salia lechosa, como un
  velo claro; con el de ahora el rojo queda profundo y el reflejo va en los cantos y costados (Flycast, PALM y las
  galerias de ALPI con el piloto: capturas scratchpad vt/cmp_refl2.png y vt/r2_gal.png).
- Rev 2.5 / Mod 2.1 (queja: "la carroceria tiene color opaco sin brillos"; medido en Flycast, brillo de los pixeles
  rojos del coche, media / percentil 99: el mapa fijo del coche 110-132 / 181-202, la Rev 2.4 58-82 / 92-140): el
  brillo de siempre lo daba el mapa propio de cada coche (esferico, visto desde la camara). Ahora la textura de 16x16
  se compone cada cuadro (`refl_compone`) con ese mapa (reducido a 32x32 al leer el coche, refl_fijo_lee; muestreado
  con la normal que refleja hacia cada texel, N ~ R - I) x 0,9 x la luz de lo de arriba en el entorno (1 a cielo
  abierto, hasta 0,3 bajo techo) mas el entorno x 0,6, con el destello del sol (la luz del entorno 2: cos^24 x su
  color) y la curva 1,5; Fresnel 0,5 + 0,5 (1 - |N.I|)^2. Medido: 99-130 / 153-186 (como el mapa fijo) y con el
  entorno encima (capturas scratchpad vt/cmp_refl4.png).
- INVENTADO: los pesos (0,9 / 0,6), la ganancia (0,85), el Fresnel, la curva, el sol, la altura del punto de vista
  (1 m) y el desenfoque.
- Coste: RAM, dos bufers del tamano de la etapa con mas mapas (malloc al buscar REFLEJO.DAT); CPU, la mezcla (256
  texeles, solo al cambiar) y el Fresnel por vertice de entorno. `-DSIN_REFLEJO`: el mapa fijo del coche.
- NO verificado en consola real.

### La pancarta CHECK POINT otra vez (Rev 2.3 / Mod 1.9, 2026-11-22)

Queja del usuario: "han desaparecido las marcas de checkpoint". La pancarta (objetos de la etapa, ver Checkpoint) esta
en z = 0 y el punto 0 de la ruta, 5,9..7,8 m mas adelante (medido en SEG01/03/05/16/38/49): con la regla de la Rev 1.4b
(DETRAS_ENTRADA: lo de la etapa siguiente con el centro detras de la entrada no se dibuja mientras se va por la union)
quedaba fuera hasta cruzar la entrada, ya debajo de ella; en todas las etapas. Ahora con un margen de DETRAS_MARGEN
40 m (lo de SEG29 que motivo la regla queda 400 m por detras). Comprobado en Flycast (PALM -> ALPI con el piloto):
la pancarta al llegar, antes no. Tambien los TRAFK*.DAT (trafico) pasan por tools/libros.py (el patron TRAF[0-9]* los
dejaba fuera): 17 texturas, 17 KB.

### Libros VQ reducidos (Rev 2.2 / Mod 1.8, 2026-11-22)

Plan de VRAM tras el estudio del flujo por tramo: las texturas de las etapas ya van casi todas en VQ (31,8 MB frente a
152 MB sin comprimir) y los libros son el 36 % de lo que ocupan: una VQ de 32x32 lleva 2 KB de libro y 256 bytes de
indices. La PowerVR acepta libros de N < 256 entradas (las ultimas; demo/main.c ya los dibuja: elige16 los hacia en
algunas), asi que `tools/libros.py` vuelve a codificar con pvrtex (utils/pvrtex de KallistiOS, ELBG de FFmpeg) las
texturas usadas, en VQ con libro de 256 y sin mipmaps: 64 entradas hasta 32x32, 128 hasta 128x64, si el error RMS contra
la imagen de ahora es <= 8 (de 255) y el alfa de recorte cambia en <= 1 % de los texeles. Comparado antes con 28
texturas de 6 etapas: con 128 entradas pvrtex da 5,2 de RMS y el k-means de tools/vq.py 5,9 (27 de 28 mejor).
- Datos: SEG16 (IMPE) 82 de 85 candidatas, 82 KB menos, RMS media 4,7 (maxima 7,5); a la vista, iguales
  (scratchpad lib/comparacion.png). Todas las etapas y uniones (los cuatro paquetes reempaquetados con pak_union.py) y
  los modelos sueltos (trafico, personajes, banderero, objetos: 381 KB menos; lo fijo de la carrera 617 -> 586 KB).
- VRAM libre en la zona del curso (tools/vram.py --zona 1248, 496 combinaciones): media 573 -> **624 KB** (+51; de +20
  a +96), peor caso BK242 + IMPE 219 -> **304 KB**; PALM 530 -> 614, BEAC 400 -> 454, LASV 495 -> 549.
- No cambia el popping por si solo: los saltos medidos son del tope del TA (el bufer de parametros), no de la VRAM. Es
  margen para lo siguiente (versiones lejanas de los edificios, soltar texturas al entrar en la union).
- `tools/regenera.py` lo hace despues de cada revq (uniones, uniones de la Contrarreloj y del OUTRUN, etapas y modelos
  sueltos). En Windows, `pvrtex.exe` compilado con mingw-w64 (windows/construye.sh, su fuente LGPL en el paquete).
  Sin pvrtex, sin cambios (lo dice). Determinista (las mismas entradas dan los mismos bytes).
- NO verificado en consola real.

### Menos vegetacion y publico con sus gradas (Rev 2.1 / Mod 1.7, 2026-11-21; DESVIACION pedida)

El usuario no quiere el fundido ni el velo reforzado de la Rev 2.0 ("se hace incomodo"): vuelven a como en la Rev 1.9
(fundido solo con `-DCON_FUNDIDO_ENTRADA`, NLIM_A 0,3). En su lugar pide bajar el coste: quitar el 10 % de las palmeras,
arboles y arbustos de decoracion de los margenes y simplificar el publico de las gradas para que cargue con ellas.
- tools/decor_tabla.py hace demo/decor_tabla.h (numeros de textura por SEGnn: 1 vegetacion, 2 publico) con el color y
  los bordes del alfa de cada textura de recorte, revisado a mano en todas las etapas (fuera, a mano: almenas de CAST,
  una verja de METR, la barandilla con jardineras de BEAC, el cartel a rayas de FLOR y una estructura y una palmera con
  tronco de EAST). Las 62 SEGnn.DAT del clasico y del GD-ROM del taxi son identicas: la misma tabla.
- decor_reduce (al leer la etapa): fuera uno de cada DECOR_VEG (10) nodos de vegetacion (planta < 25 m, 1,5-35 m de alto),
  elegidos por un hash de su posicion (siempre los mismos, repartidos), con su version lejana; el publico sobre una
  grada va al grupo de la grada y se queda con una de cada DECOR_PUBLICO (2) tiras. PALM: 55 de 575 plantas fuera y 30
  nodos de publico (3 por bloque de gradas) con su grada. El publico de pie en el arcen (BEAC, NIAG, LASV, SANF) no se toca.
- `-DSIN_DECOR_REDUCE`: como antes.
- Medido (`-DDIAG_POPPING`, Flycast, las mismas pruebas que la auditoria de la Rev 2.0): PALM, saltos por el tope del TA
  2,8 -> 2,3 por segundo y el reparto en su minimo (corte 1 m) el 44 -> 35 % del tiempo; CAST 3,0 -> 2,9; BEAC, SANF y
  LAKE igual. La vegetacion lejana ya va como impostor (lod_veg.py) y cuesta poco: lo que no cabe es sobre todo
  edificios y terreno.

### Auditoria del popping, fundido de entrada y velo del fondo (Rev 2.0 / Mod 1.6, 2026-11-21)

Auditoria (Rev 1.9, Flycast con el piloto, `-DDIAG_POPPING`, 5 etapas de cada modo; scratchpad auditoria.py): saltos de
nodos de 32 px o mas por segundo 3,5-12,3 segun la etapa; de ellos por el reparto del TA ("tope", los que se ven) 0,9-3,0
y por la PVS ("pvs") 1,0-9,3, que casi nunca se ven (el contador mira el centro del nodo en la pantalla, no si esta
tapado): dibujando los datos desde la carretera (pvs_audit.py) la PVS quita de lo visible 0,00 % de pixeles en BEAC, 0,01 %
en MACH, 0,11 % en SANF y 1,29 % en PALM, con dos huecos reales: SANF nodo 177 (terreno, s ~2370) y PALM nodos 493-510 y
569 (fachada, s ~4300) (PENDIENTE). Los saltos del reparto estan a 280-400 m de mediana y el 90 % antes de ~900 m; la
distancia de dibujo esta en 2650-3200 m: una niebla solo en el limite no los tapa (y la de media distancia la quito el
usuario en la demo 40).
Cambios (pedidos: "opcion 2" y "algo de niebla en el fondo"): el fundido de entrada vuelve por defecto
(`-DSIN_FUNDIDO_ENTRADA` lo quita; se quito tras la demo 35) y el velo del limite NLIM_A pasa de 0,3 a 0,45 en el mismo
tramo. Medido en SANF (61 s): los saltos son los mismos (12,9 frente a 12,5 por segundo: el fundido no los evita, los
suaviza) y el coste del fundido en el TA es pequeno (saltos por el tope 4,8 frente a 4,5 por segundo). Comprobado con
`-DDIAG_FUNDIDO` (lo que se funde, en magenta) que funciona con el envio en dos pasadas y la reserva por listas.

### Torres del puente de SANF y laminas en la union hacia MACH (Rev 1.8 / Mod 1.4, 2026-11-21)

Queja: en OutRun 2 SP, SANF y MACH, "elementos importantes que no cargan dejando huecos". Comparado con el video de
referencia y con `-DDIAG_POPPING -DDIAG_NODO=n` (por que se quita un nodo cada cuadro) y `-DDIAG_SIGM` (la malla mayor de
la etapa siguiente que se dibuja).
- SANF, el Golden Gate (CORREGIDO en la Rev 1.9): las torres (nodos 765 y 775 de SEG25, 323 m de alto) van en un grupo
  (tools/grupos_nodos.py) con piezas pequenas del puente y el grupo toma la clase de LOD mas alta de los suyos: el reparto
  del TA las quitaba ("tope") y se veian los cables colgando sin torres. La Rev 1.8 lo decidia cada cuadro por el tamano
  en la pantalla y ponia esos nodos en la clase 0 (la que no se quita): la clase 0 no cabia, el reparto se quedaba en su
  minimo (corte 1 m, clase -1) y lo de alrededor del umbral cambiaba de clase: el suelo y el puente parpadeaban (queja
  del usuario). Rev 1.9: al leer la etapa los nodos de radio >= GRUPO_RMAX 150 m salen de su grupo y llevan su propia
  clase. Medido con -DDIAG_POPPING en los primeros 18 s de SANF (puente): saltos de 32 px o mas por segundo 11,6 (Rev
  1.7), 16,4 (1.8), 12,1 (1.9); el corte del principio 45 m / clase 23 como en la 1.7 (1 m / -1 en la 1.8).
- Union hacia MACH (CORREGIDO): mientras crece la etapa siguiente (geo_vista) no se dibujan sus nubes (mallas enteras
  mas de SIG_NUBES 600 m por encima de lo mas alto de su carretera) ni sus cupulas de cielo (nodos de radio mayor que
  SIG_CUPULA_R 2000 m: el degradado azul de MACH, nodos 1069 / 1072 de SEG27, lista opaca). Aplastadas hacia la base de
  su marco eran laminas azul marino y siluetas palidas en el cielo de la union. Y los paneles lisos azul marino de las
  nubes de MACH (textura 43 de SEG27 / SEG59, 32x32, 17 tiras a 900-1650 m) no se dibujan nunca: el video del original
  no los saca; seran una mascara o un estado del GS que la demo no tiene (PENDIENTE: leerlo en el MIPS).
- Lo que queda en la union hacia MACH es la silueta de sus montes en la niebla con el cielo aun de la etapa anterior; el
  original tambien los ensena ahi.

### Edificios en esqueleto y palmeras sin hojas (Rev 1.7 / Mod 1.3, 2026-11-21)

Captura del usuario en la salida de PALM con el coche girado a la derecha: edificios sin fachada (solo forjados y
pilares) y palmeras sin hojas. Propuesta del usuario: quitar el esqueleto interior y dejar las fachadas. Los datos
(vista cenital y render de los datos con la camara de la captura, scratchpad) muestran que las fachadas y las hojas
estan en el disco: son de la lista de recorte (texturas con transparencia: ventanas, hojas), que va detras de la opaca
(forjados, pilares, troncos). Mirando de lado a toda la manzana lo opaco llenaba el tope del TA y la lista de recorte
no cabia. No se quita geometria (el esqueleto se ve a traves de las ventanas: el original es asi). CORREGIDO: cada lista
deja a las de detras lo que gastaron en el cuadro anterior (x 1,1, o x 1,5 si no cupieron), entre RESERVA_MIN 4 % por
lista y RESERVA_MAX 60 % en total (sustituye al RESERVA_L2 fijo de la Rev 1.6). Lo que se queda fuera es lo lejano de
la lista opaca (dibuja_etapa manda lo cercano primero). Prueba: `-DPRUEBA_CAM_FIJA -DPV_X=-14 -DPV_Y=3.2 -DPV_Z=-40
-DPV_DX=1 -DPV_DZ=0` (camara fija en el mundo) y `-DDIAG_VTX` (linea LISTAS: lo de cada lista).

### Carga, suelo de LASV, faros, agua y cascadas de NIAG (Rev 1.6 / Mod 1.2, 2026-11-21)

Queja: carga irregular de elementos en general; en LASV bajadas de cuadros con el agua de las ruedas y el suelo que
desaparece; faros de noche que no apuntan al suelo y demasiado brillantes; las cascadas de NIAG aparecen y desaparecen
y su agua parece una capa solida turquesa. Medido en Flycast (`-DDIAG_VTX`, `-DDIAG_TEX=n`, `-DDIAG_SIN_TOPE`).
- Suelo de LASV (CORREGIDO): la calzada mojada es translucida (textura 3 de SEG22, lista 2) sobre una capa opaca con
  los edificios reflejados. Las listas van en orden (0, 1, 2) y en los tramos cargados (s 450-650) lo opaco llenaba el
  tope del TA: la calzada no cabia y se veia el reflejo sobre negro. Ahora las listas 0 y 1 dejan RESERVA_L2 (15 %)
  del tope del TA y de vertices de la etapa a la translucida.
- Envio de lo cercano primero (dibuja_etapa, dos pasadas por lista): la clase de distancia <= PRIMERO_CD (unos 64 m)
  o la de LOD 0 y despues el resto: si un cuadro no cabe, se queda fuera lo lejano. Y un cuadro suelto que no cabe ya
  no baja el corte del reparto (prio_ajusta: dos seguidos si): antes un solo cuadro llevaba el corte de 4000 m a 5 m
  y los edificios volvian a golpes durante segundos (la "carga irregular").
- Esferas de los nodos (CORREGIDO): al leer cada etapa el radio de cada nodo crece hasta cubrir todos sus vertices
  (lee_modelo_en). En unos pocos nodos del disco la esfera no los contiene (NIAG 984, la cascada de la derecha, 85 m
  corta; LASV 2177, 432 m) y el recorte por la vista los quitaba enteros con sus tiras en la pantalla.
- Agua de las ruedas: como mucho NAGUA 128 gotas (el original 256), AGUA_TAM_MAX 1,2 m y AGUA_PANTALLAS 0,5
  pantallas de area entre todas (cada gota es un cuadrado translucido: la PowerVR paga por pixel y ordena por triangulo).
- Faros (DESVIACION pedida): la luz del suelo (FAROS.DAT) x FAROS_BRILLO 0,4 de alfa y los focos proyectados x
  FOCOS_BRILLO 0,55. Era un parche blanco delante del coche que en el video del original casi no se ve.
- Agua de NIAG (DESVIACION pedida): color por vertice entre el fondo (0x1A4470, mirando hacia abajo) y el cielo
  reflejado (0x6A9AC6, rasante), con (1 - cos)^3 del angulo con la vertical (agua_malla, agua_fresnel). Colores del
  video del original. Las demas zonas siguen con su color solido.
- NO verificado en consola real; los cuadros por segundo de Flycast no son los de la Dreamcast.

### Reflejo del cielo y de los tuneles (Rev 1.3, 2026-11-20; DESVIACION pedida)

- El original (SLES-53998 0x103dc8 -> 0x1040e0 -> 0x172f30 = 414340 / 414050 / 44CD30 del PC): cada cuadro, con el coche
  del jugador en marcha, dibuja el entorno desde el coche en un cubo de caras de 64x64 (proyeccion de 90 grados, cerca
  0,1 y lejos 2000; la cara 3 todos los cuadros y otra por turno, [0x448a60]: 0, 1, 2, 4, 5) con el modelo simplificado
  de la etapa (cs_ENV_<zona>: 8000-14000 vertices y 20-40 texturas, ~500 KB en el disco de PS2) y su cielo, y lo usa de
  mapa de entorno. CONFIRMADO en el MIPS (la cadena de llamadas y las constantes); no se ha leido 0x1047c8 (como pasa
  el cubo a la textura del coche).
- En la Dreamcast (peticion: "solo el cielo y los tuneles"): el cubo no cabe (una pasada mas por cuadro con un sistema
  de presentacion ajustado en consola, y ~250 KB de RAM por etapa para cs_ENV). Como el cielo de una etapa no cambia,
  `tools/conv_reflejo.py` calcula una vez por etapa un mapa de 32x32 de su cielo (el grupo 0 de SEGnn.DAT, en el marco
  del fichero; u = 0,5 + 0,4 dx / (1 + dy), v = 0,5 + 0,4 dz / (1 + dy)) y la tabla de puntos de la ruta bajo techo
  (geometria de la etapa entre 3 y 14 m sobre el centro de la calzada y a +-2,5 m, en 24 m seguidos o mas):
  REFLEJO.DAT, 2 sectores por etapa, leidos con la etapa (el hilo de carga). En carrera `refl_elige` cambia la textura
  de entorno del coche (tipos 2 y 3 de CEX2) por ese mapa, o por el del tunel bajo techo, y `coche_reflejo` pone las UV
  con el vector reflejado de cada vertice (R = I - 2 (N.I) N, I del ojo al coche) en el marco del mapa: el de la etapa
  (marco_et[3]) para el cielo, el del coche para el tunel. Sin vertices ni pasadas de mas: el mismo color de
  desplazamiento de antes; 8 KB de VRAM y 8 KB de RAM. Fuera de la carrera, o sin REFLEJO.DAT, el mapa fijo del coche.
- INVENTADO (no sale del disco): la intensidad del cielo en el mapa (x 0,30: a plena luz quemaba la pintura; el original
  atenua su cubo con [8A8BD4] / [8A8BD8], sin leer), el color del suelo bajo el horizonte (el del horizonte x 0,45) y el
  mapa del tunel (penumbra con dos filas de lamparas). El original reflejaria ahi cs_ENV (edificios, montes, la boveda).
- Verificado en Flycast (`-DPRUEBA_AUTO -DPRUEBA_SIN_TIEMPO -DDIAG_REFLEJO`, arbol 0): PALM a cielo abierto (mapa 1) y
  las dos galerias de ALPI (mapa 2 en s 3132-3348 y 3678-3945 de la etapa, y de vuelta al cielo); `nav3.sh` normal.
  NO verificado en consola real (el coste por cuadro es el de `coche_entorno` mas una division por vertice).

## Coches: montaje y luces (como el original)

Queja del probador: "diseno de coches mal". Se comparo cada coche con un render de referencia montado con el codigo de
la decompilacion portado a python (46A560 de platform/vehicle_model_draw.cpp: pila de matrices de fila del PC, cada
operacion multiplica por la izquierda; fuera del repositorio), leyendo el .DAT como la demo, y con Flycast.
Diferencias encontradas y su causa:

- Triangulos de union entre tiras (las "placas" que cruzaban cristales, capo y motor en todos los coches; la peor,
  el F40). Causa: el VU1 no dibuja el triangulo que cierra un vertice con la bandera ADC (bit 1 de la palabra de la y;
  tools/ps2/vif.py no_dibuja, regla del agente de graficos en tools/modelo.py); conv_coche.py la suplia con una
  heuristica (astillas: triangulos largos y finos) que dejaba muchas y podia quitar tiras finas de verdad. Ahora se
  usa la regla real y astillas() ya no existe. CONFIRMADO (microcodigo del VU1).
- F40: faltaba la pieza +14 de la disposicion (conv_coche.py la saltaba si tenia mas de 2000 vertices). Es una copia
  entera de la carroceria (6503 vertices de pintura, con asientos y salpicadero pintados) que 46A560 dibuja antes que
  la carroceria (+00); con ZTEST GEQUAL (D3DCMP_LESSEQUAL en el PC) la carroceria, dibujada despues, la tapa donde
  coinciden. Ahora va y el vaciado (abajo) resuelve los empates como el original: gana lo dibujado despues (antes
  ganaba lo primero: el F40 salia con el interior pintado). CONFIRMADO (render con y sin ella).
- Luces: antes solo +7C. El original (46A560/46AE70/469600, vm = 1): +80 en vez de +7C con car+4 & 0x300 (0x100
  noche, 0x200 tunel: CheckNightAndTunnel 4A25F0; no son los frenos), +84 con el pedal de freno (car+38 != 0; en el
  Enzo ademas 0x4001a, la luz del aleron, en +1C), +60 (luz de los faros) con noche y faros abiertos (0x800). Ahora
  van las cuatro con su grupo (bloque CEX2 de conv_coche.py) y la demo enciende las que tocan (coche_luces en
  demo/main.c: freno = yo.freno_ant > 0; noche y tunel = 0, la demo no tiene los bits de SCN_EFC; una malla apagada
  tiene cuenta 0 y dibuja_rango la salta). CONFIRMADO.
- Orden de las piezas: ahora el del original (puertas, +14, carroceria, volante, +18, faros, luces, ruedas: pinza, buje
  y disco). Las matrices salen identicas a las del port independiente en los 15 coches (reposo, freno, tunel).
- Sin diferencias (comprobado): posicion, giro y caida de cada pieza (puertas en su bisagra, volante girado en x +58,
  +18 en +1C, faros cerrados en +68, ruedas en +98 con la caida +A4 en z); no hay escalas negativas: las piezas del
  lado derecho (ruedas, pinzas, discos, puertas) son registros propios ya espejados; cada registro es un solo nodo
  (tabla 0) sin matriz (+1C = -1) que dibuja todas sus tiras (tabla 1: opacas y de mezcla): en los 360 registros que
  montan coches y trafico no hay jerarquia ni matrices de la tabla 6. Ruedas: apoyan en y = 0 (+-2 cm), radio
  0,310-0,358 m, via 1,35-1,67 m, batalla 2,34-2,65 m; el origen del modelo es el suelo y la demo lo pone en la altura
  de la calzada (coche_coloca): sin altura de mas. Suspension: el original pone las ruedas en car+130 (de la fisica) y
  la carroceria con la matriz car+F0; en reposo son +98 y la identidad (469FF0, el dibujo de los finales, usa +98).
- Vaciado: solo si lo que se dibuja a la vez pasa de 11000 vertices (10 de los 15; ver DREAMCAST.md). Desde las
  camaras del juego el render del .DAT sale igual que el del original entero: 0,00-0,09 % de pixeles distintos en la
  seleccion y 0,00-0,06 % en carrera (por la compresion VQ); en una vuelta baja a 4 m que no esta entre las camaras,
  0-0,94 %.
- Trafico: antes era la carroceria del LOD 2 (que ya lleva las ruedas pegadas) con otras cuatro ruedas puestas a ojo
  (posiciones inventadas: ruedas dobles y torcidas). Ahora como el original de cerca (4AE0F0 -> 4ADBC0, LOD 0 del
  registro de 4866C0 / PC 650500): carroceria, sombra de \COMMON\obj_vshadow_pmt.sz subida 0,1 m y las ruedas del
  descriptor en +3C/+4C/+5C/+6C (las del otro lado giradas pi en y); sin luces (+C, vacia en estos modelos). 823-1089
  vertices por coche (antes 472-794), VRAM 297 KB (antes 265).
- LOD del trafico (CONFIRMADO, 2026-11-08): el registro de 4866C0 tiene 5 descriptores (LOD 0..4); 4ADBC0 (0),
  4ADD80 (1) y 4ADF00 (2..4) dibujan carroceria, pilotos y sombra (subida 0,1 m solo en el 0), y la lista +9C y las
  ruedas solo en el 0 y el 1. El LOD sale de la z de la camara en 46DF50; en SLES-53998 es 0x1dd728 (localizado por el
  tope de poligonos 46E110 = 0x1dd9f8: addiu 0x2ee0 / 0x1388): lui 0xc302 / 0xc2aa / 0xc248 / 0xc220 = -130 / -85 /
  -50 / -40 (LOD 4 / 3 / 2 / 1), 0x40a0 = 5 (LOD 0 hasta ahi; detras, 4), y base lui 0xc20c = -35 en los modelos 0x1f
  y 0x27 y 0xc1c8 = -25 en el 0x28 (los umbrales del PC 5B4354..5B4360 son 40 / 50 / 85 / 130; 5B4358 = 50 ya estaba en
  race_traffic.cpp). Ahora `tools/conv_trafico.py` monta los 5 LOD (mascara de LOD por malla) y `traf_lod` de
  `demo/main.c` elige como 46DF50. Vertices por LOD 823-1089 / igual / 341-517 / 69-207 / 20-68: el trafico en carrera
  pasa de ~9500 vertices a ~1000-1500. No portado: 46E170 (baja el LOD de los mas cercanos si la suma de poligonos pasa
  del tope de 46E110), innecesario con el tope del TA. Verificado en E (hoja en el scratchpad, dist/hoja.png).
- Coche del jugador: el original no tiene LOD (46AE70 dibuja su disposicion entera; los LOD de 4866C0 de los modelos
  0..29 solo los usa 4AE0F0, el dibujo de los otros coches). Se queda entero.
- Luces de freno (2026-11-10). Jugador: 46A560 dibuja +84 en lugar de +7C con car+38 != 0 (el freno de
  OperationInput); la demo miraba yo.freno_ant, que con la fisica del original (fis_ok) coche_paso no toca: no se
  encendian nunca. Ahora fis.freno_op != 0 (event+0x38). El original no tiene luces de marcha atras: la disposicion
  del coche solo trae +7C / +80 / +84 / +60 (y la del aleron del Enzo); no hay pieza ni rama para la marcha R.
  Trafico: 4AD4D0 dibuja los pilotos +14 del descriptor en lugar de +C con car+38 > 0, y a car+38 del trafico solo
  lo toca su IA: 472450 lo pone a 0 cada cuadro y 4718D0 (471E3E) a 1 si +4 & 0x40000, que 46DD00 copia de 0x10000 |
  0x20000 tras 4F9450 (tocado por otro coche / contra el muro en ese cuadro): los pilotos del trafico se encienden
  mientras lo empujan por detras, no al frenar por su cuenta. TRAFn.DAT lleva los +14 (grupo en los bits 24..25 del
  campo del nodo, conv_trafico.py) y traf_dibuja elige; el 0x20000 del trafico (su choque con el muro, 504E70) no
  esta portado. CONFIRMADO (captura frenando: tools/prueba/frena.sh).
- Luces de noche (car+4 de 4A25F0, +80 / +60 del jugador, +8 / +10 del trafico): HECHO en la revision 42, ver Luces de
  noche y faros. PENDIENTE: direccion de las ruedas del trafico (car+32 del trafico sin localizar) y su car+F0 (4A2910
  con car+D34).

## Luces de noche y faros (revision 42)

Queja del usuario (Dreamcast real): "en las etapas de noche los coches no emiten luz por sus faros". La demo dejaba
car+4 a 0 (coche_ban): ni pilotos de noche, ni faros escamoteables, ni luz de los faros, ni luces del trafico. Ahora:

- Bits (CONFIRMADO, PC; PS2 igual en el dibujo): CheckNightAndTunnel 4A25F0 (cada cuadro, jugador y trafico: race_traffic
  4A25F0) pone en car+4 0x100 con el bit 3 del trabajo de SCN_EFC (4AFB60), 0x200 con el bit 4 (4AFB90) y 0x400 con la
  bandera 8 del +0x3C del descriptor de la etapa elegida. weather_44bd50 copia los bits 8 / 0x10 de ese +0x3C al trabajo
  de SCN_EFC (al empezar, 44CB00, y en el estado 0xD del cambio de entorno). CURSO.DAT ya los llevaba (`clima`): 0x18 en
  GHOS y METR, en ninguna otra. demo/main.c `noche_ban` (con la etapa del entorno como el sol: la nueva desde el
  fundido) -> `coche_ban` cada cuadro en coche_anima_paso ("LUCES: car+4 700" por la consola serie). El "tunel" 0x200 es
  de la etapa entera (no cambia al entrar en un tunel: el original no mira la posicion).
- Coche del jugador (SLES-53998 0x1886e0, = 46A560 / 469600): 0x188d58 sube los faros escamoteables con car+4 & 0x500
  (0xa000 << 27 del doble de car+0), 0x188ea8 dibuja +60 con & 0x900 (0x9000 << 28) y 0x188ef0 +80 en vez de +7C con &
  0x300 (0xc000 << 26): lo que ya hacia coche_luces / coche_anima_paso, ahora con los bits.
- Luz de los faros en el suelo (CONFIRMADO, PS2): 0x17c6b0 (= 49F400; banderas del suceso & 3) -> 0x188680 (= 4695C0):
  si car+2C8 <= 0 (fuera del choque; fis.t_choque) y car+4 & 0x900 == 0x900, en car+B0 llama a 0x106960 (= 416AA0):
  textura 0x30009 (banco 3 = spr_etc_xst, textura 9: el haz, 256x256), estados del GS por 0x169ce0: ABE, mezcla
  (fuente 2, destino 1) de la tabla 0x3590f0 = ALPHA 0x48 (Cs As + Cd), ZTST ALWAYS, ATE 0, niebla; y el paquete fijo
  [0x328a80] que hace 0x106a98 (= 4171F0 / 417550) al arrancar (0x106920): V2 = (-tan 0,56, 0, 0), V1 = (0, -tan 0,2618,
  0), V3 = (0, 0, 1) girados en x por -0,24; para i = 0..7, a = i / 8 y (i + 1) / 8, P = a V1 + V2 + V3 proyectado al
  suelo del coche (0x106eb0: P x 0,8 / P.y - (0, 0,8, 2,0)); 0x106f28: tiras de 8 vertices (x por 1, 2/3, 1/3, 0; ST =
  ((2 - k/3) / 2, 1 - a); color (0x50, 0x50, 0x48, 0x80)) y su espejo en x: 16 tiras, de 1,3 m a 80 m por delante
  (la ultima cruza el horizonte y acaba 39 m detras: se recorta en el plano cercano como todo; su textura ahi es casi
  negra). Constantes del PS2 0x328a68..0x328a7c (el PC usa otras: 0,365 / 0,2418 / 0,252 / 1,2 / 4,1 y v = a x 0,85).
  `tools/conv_faros.py` -> FAROS.DAT (16 mallas y la textura en VQ: 18 KB de VRAM, siempre cargada); demo/main.c
  `faros_carga` / `faros_dibuja` en la lista transparente con SRCALPHA / ONE, textura x color (0xFF9F9F8F) y la niebla
  de la tabla. DIFERENCIA: prueba de z GEQUAL con la profundidad un 1 % mas cerca (matriz x 0,99: la misma imagen) en vez
  de ALWAYS: en el PS2 el coche, dibujado despues, la tapa; en la lista transparente de la PowerVR ALWAYS la pondria
  encima del coche. Donde la carretera suba por delante del plano del coche mas de ese 1 %, la tapa (en el PS2 no).
- Trafico, rivales y camiones (CONFIRMADO, PC 4AD4D0 / second() de race_ghost_car.cpp): +8 (luz de noche) con car+4 &
  0x100; +14 (freno) con car+38 > 0, si no +10 con car+4 & 0x300, si no +C; remolque: +18 con el freno, si no +14 con &
  0x300. `tools/conv_trafico.py` anade +8 (grupo 4), +10 (grupo 2) y +14 del remolque (grupo 2) en los bits 24..26 del
  campo del nodo; las de mezcla aditiva en su estado del GS (tabla 4 -> 7: ABE y ALPHA 0x48; la +8 de los 9 coches de
  trafico y la +10, la +14 de los camiones; en los rivales casi ninguna) con una copia de su textura con 0x400 | 0x800
  (SRCALPHA / ONE, z GEQUAL; compila_cab). demo/main.c `traf_luces` (mascara de grupos) en lugar de traf_apagado. TRAFn,
  TRAFKn y RCmm crecen 1-3 KB (+1 textura de 1 KB en VRAM por modelo con luz aditiva). Los pilotos de dia (+C / +14) se
  quedan como estaban.
- Focos de los faros (revision 42b; DESVIACION respecto al PS2, del PC). En SLES-53998 0x20e100 (= 4AF5B0) y 0x20e1b8
  (= 4AF630) calculan todo con los parametros de las tablas 0x410688 / 0x410698 / 0x4106A8 (variables 0x5E0138 /
  0x5E013C / 0x5E0140): TEX POS 2, TEX HEIGHT 1,8 y TEX DIR -20. (El 0x410528 que citaba antes esta mal: es la tabla de
  SPOT POS / SPOT HEIGHT / SPOT DIR / SPOT INSIDE / SPOT OUTSIDE, cadenas 0x410398..0x4103D8.) La proyeccion va con
  cerca 0,1 (0x3DCCCCCD en 0x20E16C) y lejos 4000 (0x457A en 0x20E178; el PC usa 5000). Pero NO se ve nada
  (CONFIRMADO, desensamblado): el dibujo del suceso
  (0x20de10 -> 0x20e7b8) es un `jr ra`, igual que la lluvia 0x20e038 y la sombra 0x20e0b8; nadie lee work+0x170 / +0x1B0
  (los unicos lectores de [0x55c9dc] son los que ponen y quitan banderas), y las dos luces de foco (tipo 1, ids 0 / 1,
  entornos 1 y 2: 0x445360 + id x 0xD0 + entorno x 0x1A0) solo las lee 0x110100 -> 0x168e80, que descarta toda luz cuyo
  tipo (+0x60) no sea 3 (direccional; los focos son 2): no llegan al VU1. Lo unico que ilumina el PS2 de noche es la luz
  del suelo 0x106960 (arriba). El PC si los dibuja: 40C550 en las capas 5 y 6 (40BD80) proyecta la textura 0x30009 desde
  work+170 (vista de 40A900 x perspectiva de 40A950) y la suma (operacion 0x0C200B01 / 0xC00 del combinador: actual x 1 +
  textura x constante 0x80808080, decodificada con la tabla de modos de 40B800: PROBABLE) con borde negro; sin textura
  con car+2C8 > 0. Como el usuario quiere ver la luz de los faros delante del coche, se porta ese haz del PC (demo/main.c
  focos_prepara / focos_anota / focos_dibuja; el comentario de focos_prepara lleva el detalle): segunda pasada, en la
  lista transparente (ONE / ONE, z GEQUAL con 1/w x (1 + 2^-12)), de las mallas opacas de la escena dibujadas en el cuadro
  que tocan la piramide del proyector, con la UV proyectada en el SH-4 por vertice, el gris 0x80 x (1 - niebla de la
  tabla) y sin mandar los triangulos enteros fuera de la textura o en sus filas negras de abajo (la linea de corte del
  haz: desde la fila 200 de 256 en la textura de la DC, focos_v = 0,7754, mirado al cargar). En llano el haz va de 2 m
  a ~20 m por delante del centro del coche (la fila 200 a ~25 m) y ~56 grados de ancho; detras del proyector cae en el
  borde (negro). DIFERENCIAS: la PowerVR interpola la UV con la w de la camara y no con la zc del proyector (corregido en la
  revision 44 dividiendo los triangulos, ver abajo); sin pasada en lo de recorte (lista 1), lo transparente y
  los carteles; ni el coche ni el trafico (capas 5 / 6 del PC = la escena: PROBABLE). Los dos focos por vertice (SPOT /
  WIDE) no se portan: ni el PS2 los usa ni la escena de la DC lleva luz por vertice.
- Luz del suelo revisada contra el desensamblado (queja del usuario sobre faros_despues.png: "mancha blanquecina a los
  lados del coche"): 0x106a98 (V2 = (-tan 0,56, 0, 0), V1 = (0, -tan 0,2618, 0), V3 = [0x3bfab0] = (0, 0, 1)), el giro de
  0x1647e4 (VCALLMS 0x4D y MULAy / MADDx / MULAy / MSUBx sobre las filas 1 y 2 de la matriz del VU0: fila1 c + fila2 s,
  fila2 c - fila1 s = D3DXMatrixRotationX; 0x16481c da igual RotationY con la misma asignacion seno / coseno), 0x1649ac
  (x fila0 + y fila1 + z fila2, sin traslacion), 0x106eb0 (P x 0,8 / P.y - (0, 0,8, 2,0): rayos desde los faros, 0,8 m
  de alto y 2 m por delante del origen de car+B0, al suelo) y 0x106f28 (x s, 0, -z; S = (2 - k / 3) / 2, T = 1 - a en
  1/4096; los dos primeros vertices de cada tira con la bandera de no dibujar) coinciden con tools/conv_faros.py; el
  origen de car+B0 es el de mod_coche (en la calzada, z = -adelante). La zona clara de la textura (filas 100-180, alfa
  ~0,9) cae de 3 a 9 m por delante y hasta 3-4 m a cada lado: con la camara de carrera el coche tapa el centro y se ve a
  los lados del morro; asi lo pinta el PS2 (sin emulador de PS2 aqui no hay captura del original para comparar). Probado
  tambien con ZTST ALWAYS como el PS2 en vez de GEQUAL: la misma imagen. La ultima tira de cada lado (cruza el
  horizonte) usa las filas 0-32: alfa x color <= 5 / 255, no se ve. Nada que corregir; lo que faltaba delante del coche
  es el haz de arriba.
- Verificado (Flycast, `-DPRUEBA_AUTO -DDIAG_CONGELA=n -DDIAG_MS -DDIAG_SIN_CPU`, el mismo cuadro antes / despues):
  METR (`PRUEBA_NODO=11`, crono 2113 = el 0'35"099 de faros_despues.png): 44,8 / 44,8 fps, dib 15,17 -> 16,45 ms, 25153
  -> 25205 vertices (134 mallas anotadas, 52 vertices del haz: la calzada son triangulos grandes), focos_dibuja 0,46 ms;
  GHOS (`PRUEBA_NODO=9`, crono 900): 44,8 fps, dib 15,25 -> 16,60 ms, 29482 -> 29525 vertices, 0,69 ms. VRAM igual (la
  textura es la de FAROS.DAT). Con el proyector entero, sin el recorte de las filas negras, eran 1512 mallas, 5883
  vertices y 9 ms (35 fps). Capturas en el scratchpad capturas_r42/: focos_antes.png / focos_despues.png (METR),
  focos_antes_ghos.png / focos_despues_ghos.png, faros_corregido.png (= focos_despues.png, el punto de
  faros_despues.png), focos_sin_luces.png (`-DDIAG_SIN_FAROS -DDIAG_SIN_FOCOS`) y las diferencias faros_solo_dif.png /
  focos_solo_dif.png. Datos: ninguno nuevo.
- Verificado en Flycast (METR, PRUEBA_AUTO PRUEBA_NODO=11): luz delante del coche, pilotos de noche del jugador y del
  trafico; de dia (PALM) igual que antes (car+4 = 0: las mallas nuevas no se dibujan; sin "LUCES:" en la consola).
  Coste (METR, `-DDIAG_MS -DDIAG_SIN_CPU`, ~55 lineas MS): 43,8 -> 43,9 fps, dib 18,52 -> 18,45 ms, 28646 -> 28726
  vertices (los 128 de la luz y las luces del trafico); VRAM libre 1694K -> 1658K (FAROS.DAT 18 KB y las texturas
  aditivas del trafico); sin "Out of memory". Capturas: scratchpad capturas_r42/faros_antes.png y faros_despues.png.
  Datos: FAROS.DAT nuevo; TRAF0-8, TRAFK0-2 y RC00-29 hay que rehacerlos (paso trafico / camiones / rivales de
  regenera.py: borrarlos de la carpeta de datos); con los de antes la demo funciona igual que antes (sin grupos 2 y 4).
- Revision 44 (queja del usuario, Flycast y Dreamcast: "las luces delanteras en el trafico estan bien pero en los
  Ferrari salen raras, deformadas"). Reproducido de noche en METR (`PRUEBA_CURSO=189 PRUEBA_NODO=11`, crono 2113) y en
  FLOR (arbol 1, nodo 13 / arbol 193, nodo 1) con el Testarossa (faros escamoteables), el F50 y el 360, camara trasera
  y una de prueba delante del morro (`-DPRUEBA_FRENTE=5`; `-DPRUEBA_VISTA=n` elige la vista inicial). Pieza a pieza:
  - Piezas del modelo (+60 / +80 / +64): las matrices salen de llamadas() (= 46A560 / 0x1886e0: +60 y +80 con la matriz
    del coche, +64 en T(+68) Rx(car+2FC), los mismos que 0x188d58..0x188f20) y el estado del GS de cada malla es el de
    su tabla 4 / 7 (ninguna de las +60 de los 15 coches es aditiva: ALPHA 0x44 con o sin ABE; las +60 son el
    salpicadero, los pilotos encendidos, la luz de la matricula y los brillos del faro, ver tools/conv_coche.py). Se
    ven bien de cerca (capturas con PRUEBA_FRENTE). Lo que falta es lo de siempre: la capa 2 (0x48 aditiva en los
    cristales de los pilotos y faros, PENDIENTE en Suelos: capas 2). No es lo deformado.
  - Luz del suelo de 0x106960 (FAROS.DAT): tiras pequenas, sin cambio.
  - El haz proyectado del PC (focos_*): ESTO era lo deformado. La UV va proyectada por vertice (u = 0,5 + 0,5 xc / zc)
    y la PowerVR la interpola con la w de la camara; en los triangulos grandes de la calzada (decenas de metros) el
    haz salia como un rectangulo con alas o una cuna a un lado que cambiaba al moverse. Correccion (DIFERENCIA de la
    tecnica, no del resultado: el PC lo hace con la etapa de textura proyectada por pixel): el error en el punto
    medio de cada lado (la media de las UV frente a la UV del punto medio, (ub - ua) (za - zb) / (2 (za + zb)), en
    texeles de la textura de 256) se mira sin dividir (focos_bien); si pasa de 1 texel el triangulo se recorta a la
    piramide de lo no negro de la textura (columnas 13..242, filas 7..198: focos_a, mirado al cargar) y al plano
    cercano y se divide en n x n con n = techo(raiz(error)) (el error cae con el cuadrado del lado), como mucho 6 por
    lado y 300 vertices de division por cuadro (luego n = 1). La niebla de cada vertice nuevo se interpola. Borde: el
    recorte a la piramide da el borde exacto del haz. Ademas no se ilumina la cara que ve la camara si el proyector
    esta al otro lado de su plano (focos_cara: muros por detras, la bajada tras un cambio de rasante); sin sombra del
    haz (lo que este tapado al proyector pero lo mire se ilumina, como en el PC).
  - Coste (Flycast, `-DDIAG_MS -DDIAG_SIN_CPU`, cuadro congelado): METR crono 2113 Testarossa 45,4 -> 44,6 fps
    (dib 16,30 -> 16,84 ms, focos 0,46 -> 1,00 ms, 52 -> 333 vertices); F50 / 360 44,7 -> 44,7 / 44,8 fps (focos
    0,57 -> 1,07 ms); FLOR crono 2113 0,95 -> 2,0 ms, 44,7 -> 44,9 fps; peor caso medido, la salida de FLOR (crono
    200, 541 mallas anotadas): 2,9 -> 5,1 ms, 44,5 -> 43,1 fps. `-DFOCOS_NMAX=1` quita la division (medida),
    `-DFOCOS_SIN_CARA` / `-DFOCOS_SIN_ERR` las pruebas.
  - Capturas (scratchpad capturas_r44/): luces_testa / f50 / 360 _antes / _despues.png (METR, mismo cuadro) y
    luces_testa_flor / luces_360_flor _antes / _despues.png. Datos: ninguno nuevo.
- Revision 46 (queja del usuario, Dreamcast con la revision 44: "no esta arreglado, el haz de luz queda como un cono y
  no apunta al suelo"; en Flycast igual: una mancha blanca ancha junto al morro con un borde de arriba recto y nitido,
  sin luz alargada por la calzada). Reproducido en METR (`PRUEBA_CURSO=189 PRUEBA_NODO=11`, crono 2113) y GHOS
  (`PRUEBA_NODO=9`, crono 900) con la camara de carrera, una lateral baja (`-DPRUEBA_FRENTE=12`: 12 m delante, 4,2 m
  a un lado, 1,2 m arriba) y una nueva desde arriba (`-DPRUEBA_ARRIBA=14`: 14 m por encima de 3 m detras del coche
  mirando 12 m por delante), cada pieza sola (`-DDIAG_SIN_FOCOS` / `-DDIAG_SIN_FAROS`). Ninguna de las dos es
  geometria en el aire: las dos caen en la calzada (desde arriba se ve la mancha en el suelo). Lo que hacia el "cono":
  - El haz del PC (focos_*) estaba al reves en v (CONFIRMADO por las dos luces del suelo: la del PC, 417550 en
    outrun2-decomp pc_headlight_pool.cpp, pone v = a x 0,85 (K628084) con a = 0 lo cercano; la del PS2, 0x106f28,
    v = 1 - a; las dos con u = 1, 5/6, 2/3, 1/2: la textura 0x30009 del PC esta invertida en v respecto a la del PS2,
    y la DC lleva la del PS2, FAROS.DAT). Con la v del PC (0,5 + 0,5 yc / zc, sesgo +0,5 de 40C550) sobre la textura
    del PS2, lo cercano salia con las filas de arriba (tenues), los dos focos brillantes a 10-15 m y el corte negro
    (fila 200) a ~25 m: una pared de luz con el borde recto delante del coche y alas a los lados, que con la camara
    de carrera queda como una franja encima del capo. Ahora yc va con el signo cambiado (focos_prepara): el corte del
    haz queda a ~2,8 m (debajo del morro), lo mas brillante de 3 a 9 m y lo lejano se apaga poco a poco por la
    calzada. DESVIACION (el usuario: "que solo ilumine la calzada"; y el coste): la piramide del proyector se corta en
    los rayos que llegan al plano del coche a FOCOS_LEJOS = 28 m y el gris baja a 0 de FOCOS_FUNDE = 14 m a 28 m
    (focos_gris, por la v de cada vertice: sin borde). En el PC el haz sigue (filas 7..60 de esta textura, 8..30 / 255
    de gris) hasta pasar el horizonte del proyector (las fachadas del fondo, la pared de enfrente); entero anotaba ~485
    mallas en METR (5,6 ms, 43,0 fps) y 1097 en la salida de FLOR (12,6 ms, 31,7 fps); con 40 m, FLOR 8,8 ms.
  - La luz del suelo del PS2 (0x106960, FAROS.DAT): desensamblado otra vez 0x106a98 / 0x106eb0 / 0x106f28: los
    vertices van en el marco de car+B0 con y = 0 (0x106f28 escribe en la y el entero 2 en el primer par de cada tira y
    0 en los demas: la bandera; el VU1 solo los transforma con la matriz de 0x1643d8 / 0x16b230), es decir, un plano
    del coche de 1,3 a 80 m por delante, y el GS lo pinta con ZTST ALWAYS (0x169ce0): encima de lo ya dibujado sin
    mirar la profundidad, asi que en el PS2 siempre "se pega" a la calzada aunque el plano se meta debajo o quede en el
    aire (balanceo, cambios de pendiente, el bombeo). En la DC va con GEQUAL (con ALWAYS taparia el coche: la lista
    transparente va despues de lo opaco) y lo que caia bajo la calzada desaparecia (desde la camara de carrera solo
    quedaba la parte junto al morro). Ahora (faros_rejilla / faros_punto / faros_dibuja) las tiras 2..6 de cada lado
    se parten en una rejilla de 7 columnas (las 4 del original y sus puntos medios) y 12 lineas (faros_div = 1, 1, 1,
    2, 6 filas en las tiras 2..6) con la posicion y la UV exactas de los dos triangulos de cada cuadrilatero del
    original, y en cada cuadro cada punto se pone a la altura de la COLI bajo el (coli_altura modo 0x100, el de la
    camara y el trafico, con su propia prediccion; sin suelo debajo se queda en el plano del coche); 156 consultas
    (la columna del centro es la misma en los dos lados). No se dibujan las tiras 0 y 1 (1,3 a 2,6 m: filas 192-255 de
    la textura, alfa 0) ni la 7 (cruza el horizonte, <= 5 / 255; proyectada cubriria el coche): no se ve diferencia.
    `-DFAROS_PLANO` dibuja las tiras planas de antes.
  - Coste (Flycast, `-DDIAG_MS -DDIAG_SIN_CPU`, cuadro congelado, media de las lineas MS): METR crono 2113 44,9 ->
    44,8 fps, dib 16,72 -> 18,12 ms (FAROSMS: 1,5 ms la rejilla, casi todo las 156 consultas a la COLI, ~9,6 us cada
    una; FOCOSMS 1,0 -> 1,0 ms, 124 -> 128 mallas); GHOS crono 900 44,8 -> 44,8 fps, dib 17,25 -> 18,59 ms (rejilla
    1,3 ms, focos 1,36 -> 1,47 ms); peor caso, la salida de FLOR (`PRUEBA_CURSO=1 PRUEBA_NODO=13`, crono 200): 40,1 ->
    36,1 fps, dib 23,71 -> 26,36 ms (rejilla 1,3 ms, focos 4,2 -> 5,7 ms, 721 -> 880 mallas anotadas: ya iba por debajo
    de 44). `-DFOCOS_LEJOS=m` / `-DFOCOS_FUNDE=m` para medir otros cortes.
  - Capturas (scratchpad capturas_r45/): faros_cono_antes / _despues _metr_carrera / _lateral / _arriba.png (el mismo
    cuadro), faros_cono_antes_metr_*_solo_haz_pc / _solo_luz_suelo.png (cada pieza sola, antes) y
    faros_cono_antes / _despues _ghos_carrera / _lateral.png, faros_cono_antes / _despues_flor_carrera.png y la hoja
    faros_cono_antes_despues_metr_ghos_hoja.png (izquierda antes). Datos: ninguno nuevo.

## Tuneles: calzada negra y luces (revision 44)

Queja del usuario: "si el Ferrari entra en un tunel deben verse los efectos de iluminacion en la carretera, que ahora
se ve negra, y los coches tener los faros encendidos".

- Calzada negra (CORREGIDO, era del port): reproducido en el tunel de SNOW (`PRUEBA_CURSO=189 PRUEBA_NODO=8`, crono
  950; barrido con PRUEBA_AUTO de los arboles 1 y 189: tuneles en SNOW, LASV y MAYA). La calzada se dibuja (con
  `-DDEP_TEX` sale de color) pero encima va la sombra pintada del tunel: registro 0 de cs_CS_SNOW (tira 135, entrada
  0 de la tabla 4, material 1), textura 0 de 32x32 negra con alfa 0x50 en todos los texeles, ABE con ALPHA 0x44 (la
  mezcla normal), sin color de vertice: en el GS oscurece la calzada al 37,5 %. modelo.textura_dc toma el alfa casi
  constante por "no es transparencia" y la dejaba en RGB565: en la lista transparente de la PowerVR alfa 1, negro
  opaco. Ahora `modelo.formato_mezcla` (ajusta_texturas): en la lista transparente RGB565 solo si todo el alfa es >=
  240, ARGB1555 si es casi binario, si no ARGB4444. Afecta a 6 texturas del disco (todas de los cs_CS: escaneados
  todos los _pmt de STAGE.PS2 y OBJ.PS2): SNOW 0 (el tunel), INDU 11 (alfa 160-174), METR 57 (108-154) y 96 (0-38:
  salia opaca y casi no se ve), NIAG 108 (208-254) y BEAC_BT 35 (236-254), y las de sus etapas al reves. Datos
  rehechos (PYTHONHASHSEED=0, `regenera.py --interno seg` + `--interno revq`): SEG04, 06, 13, 21, 37, 39, 46 y 53
  (BEAC_BT / BEAC_BR, SEG19 / 51, no estan en ningun arbol); solo cambian esas texturas (565 -> 4444, comprobado byte a
  byte contra los de antes). Van sueltas (ORD7) en la carpeta de datos en vez del descriptor PAKD de SEGS.PAK (6
  ficheros, ~27 MB mas en el disco); un regenera.py entero las vuelve a empaquetar.
- Iluminacion del tunel en el original (CONFIRMADO, datos y PC): no hay luz propia del tunel. Las luces de
  scn_env_sun_<zona> son un registro fijo por etapa (salvo GRAN lista 1 y FLOR lista 2 de los coches, por tramos de
  posicion); las luces locales de maya_spl_<zona> (44A1D0) solo existen en CLOU (174 farolas de 0,3 de gris a los lados
  de la carretera) y FLOR_R; ninguna etapa tiene color de vertice oscuro en la calzada. Lo que se ve en el tunel es
  la geometria con su luz de etapa, las lamparas del techo (geometria) y la sombra pintada de arriba. Verificado:
  SNOW (tunel_snow_antes / despues.png), fuera del tunel de dia sin un pixel distinto (tunel_snow_dia_antes /
  despues.png, crono 500), LASV y MAYA ya se veian bien (tunel_lasv.png, tunel_maya.png; sus datos no cambian).
- Faros en los tuneles (como el original: NO se encienden de dia). car+4 0x200 ("tunel") y 0x100 (noche) de
  CheckNightAndTunnel 4A25F0 salen de los bits 0x10 / 8 del trabajo de SCN_EFC, y el unico que los pone es
  weather_44bd50 (race_area.cpp:236-243 de la decompilacion: 4AFB70 / 4AFB40 con los bits del +0x3C del descriptor de
  la etapa, al empezar y en el cambio de entorno); 4AFB70 no tiene otro llamador. No hay deteccion de tunel por
  tramo, superficie de COLI, volumen ni tabla. Solo GHOS / METR (y LASV / FLOR) llevan 0x18. Encender faros, pilotos y
  haz en los tuneles de dia seria una DESVIACION (habria que inventar la deteccion, p. ej. con la sombra pintada del
  tunel o geometria encima de la ruta): NO hecho, pendiente de que el usuario lo confirme sabiendo que el original
  no lo hace.

## Coches: piezas animadas (como 46AE70 / 469600)

Queja del probador: "las ruedas cuando haces el derrape no se ve como giran" y detalles del coche que no se animan.
Antes todo el coche era una malla fija en reposo. Ahora (tools/conv_coche.py llamadas(pose) / animadas, bloque ANI1
de CEX2; tools/conv_trafico.py bloque TAN1; demo/main.c coche_anima_paso / coche_anima_matrices / coche_dibuja /
traf_dibuja) cada pieza animada va en su marco (la inversa de su matriz de reposo; la pieza en el campo del nodo << 8)
y la demo le pone cada cuadro la matriz del original:

- Ruedas (46AE70, ruedas fuera de car+F0): T(car+130) Rz(caida +A4) Ry(rueda +32) -> pinza; Rx(rueda +30) -> buje y
  discos. +30 avanza -omega x 65536 / 2 pi / 60,2 por paso (advance_angle de pc_wheel_dynamics, 501190 / 4A0000):
  en la demo -fis.r[k].omega x dt, la velocidad angular de cada rueda de la fisica, asi que en el derrape las traseras
  giran a otra velocidad que las delanteras (medido en Flycast: 289 / 337 rad/s detras contra 198 / 213 delante).
  +32 = fis.r[k].ang (toe + car+32 en las delanteras: hasta 14 grados de contravolante en la prueba de derrape).
  car+130: x y z de la disposicion +98 (constructor 4A5830); y = car+134 de CopyCarWork 4A1140 (fis.rueda_y).
  CONFIRMADO (decompilacion del PC; mismo orden de operaciones que conv_coche.py, que esta comprobado contra 46A560).
- Cuerpo: el original dibuja el coche en car+14 = CG - R (0, +224, 0) con la R del cuerpo de la fisica; la demo lo
  ponia en la calzada (coche_coloca) sin la suspension. Ahora el coche (carroceria y ruedas) va ademas con la altura del
  CG (fis.alt), el cabeceo y el balanceo de ActionForce2 alrededor del CG, menos alt_cg. PROBABLE (car+14 de
  CopyCarWork; los desplazamientos x y z de work+220/+228 se toman 0). Consecuencia: la geometria de la suspension de
  PARAM.DAT es la misma en las 19 columnas (radio 0,314 / 0,329, anclaje 0,465), no la del modelo: en reposo el centro
  de la rueda queda a 0,316 / 0,328 m del suelo; con las ruedas de radio 0,345 del Enzo (+08 de 4866C0) hunden 1-3 cm
  y las de 0,308 del Testarossa quedan 1-2 cm por encima. Es lo que sale de las formulas del PC; sin comprobar en PS2
  (no se ha localizado CopyCarWork en SLES-53998).
- car+F0 (CheckReverseCar 4A2910, en GamePlCar_Ctrl por 4A7690): T(0, subida) Rx(cabeceo) Rz(balanceo) sobre el
  cuerpo. Subida = base + base x (0,5 + rand / 32768) x 0,6667 x sin(fase) x a / 2 con car+1F4 > 100 (base = +1C / 2
  del registro de 4866C0 = 5 mm); cabeceo = (x < 0 ? +20 t : +24 (1 - t)) x x^2 x 0,4053 acotado a [+20, +24] (x =
  car+1CC, media de 30 cuadros de 1000 x el cambio de velocidad, 4A2D70; t = car+1F4 / 480): el morro baja al frenar
  hasta 1,2-2,5 grados y sube al acelerar hasta 1,5-3; balanceo 0 en el jugador (solo con car+4 & 1 = 0). +1C..+2C de
  4866C0 leidos del ELF de PS2 (registros localizados por firma como en conv_trafico.py). CONFIRMADO (PC).
- Volante: T(+4C) Rx(+58) Rz(-car+204 x 2 pi / 65536 x 0,41667); car+204 = media de 30 cuadros de -car+32 acotado
  a +-5461 y x 6 (4A4710). Se ve por los cristales (en carrera la vista interior no se dibuja). CONFIRMADO (PC).
- Aleron del modelo 5 (+18 en +1C y la luz 0x4001a): Rx(roof_angle): x = car+1C4 x 216,72 x 0,72917; hasta 300 baja
  hasta -17,5 grados, de 300 a 350 vuelve a 0. Palanca de los demas: fija (el original no la mueve). CONFIRMADO (PC).
- Faros escamoteables (+64 en +68): Rx(car+2FC), +78 / +74 por cuadro con car+4 & 0x500 hasta +78 (entonces 0x800, que
  enciende la luz +60). Con los bits de noche de 4A25F0 (noche_ban, revision 42) se levantan en GHOS y METR; de dia
  cerrados (GTO: se levantan 55 grados en 1 s).
- Disco caliente (+94 de la rueda): en vez del +90 con car+38 > 200 y car+1F4 != 0 (grupos G_DISCO / G_DISCO_CAL de
  CEX2; coche_luces). CONFIRMADO (PC). En COCHE06 anade su textura: +6 KB de VRAM.
- Rueda "borrosa" a alta velocidad: el original no la tiene (46AE70 / 469600 no cambian de pieza ni de textura con la
  velocidad; la disposicion no tiene otra rueda). No se hace.
- Seleccion de coche: el original no anima nada (4A5B20 llama a 4A2910 y 4A4710 con el coche parado): todo en reposo.
- Trafico cerca (LOD 0 y 1, 4AD530): cada rueda T(+3C..) [Ry(pi) las del otro lado] Rx(-car+40 / -car+44, con el
  signo cambiado al otro lado); 47E780 suma a +40 y +44 +1C4 / radio (+08 de 4866C0) cada cuadro (traf_giro en la
  demo, con la velocidad del coche de trafico). Sin direccion (car+32 del trafico sin localizar).
- Vaciado (tope del TA): una pieza que se mueve destapa lo de detras, asi que un triangulo se queda si se ve desde
  alguna camara en alguna de 6 posturas (giro de rueda cada 60 grados, direccion y volante a +-30 grados y al centro,
  faros abiertos y aleron abajo en la mitad; margenes de vaciado, no datos del juego: 30 grados es el tope de car+32 en
  4A4710). Los dos discos no tapan y se vacian. Vertices dibujados 9374-11674 (antes 9320-11195): el 575 11674, el
  F40 11511, el F430 11288, el Enzo 11212 y el 550 11091 pasan del tope de 11000 (el TA lo reparte ta_cabe).
- Verificado: render de referencia (llamadas con una postura) contra el .DAT con las matrices de la demo (0,00-0,11 %
  de pixeles distintos en el Testarossa y el GTO, 1,5 % en el Enzo desde camaras bajas fuera del vaciado) y Flycast
  con PRUEBA_GUION (hoja en el scratchpad, ruedas/hoja.png).

## Coches: el piloto y la acompanante (2026-11-09)

Peticion del probador: "anadir en los coches descapotables los disenos del conductor y la chica rubia". Antes el coche
iba vacio. Ahora (`tools/conv_chr.py` -> CHR.DAT, `comun/robot.c`, `demo/main.c` robot_*) van los dos personajes de la
carrera, animados con la logica del original. Todo sale de SLES-53998 (direcciones abajo) contrastado con la
decompilacion del PC (race_robots.cpp, rob_disp.cpp, rob_motion*.cpp).

- Quienes: sucesos ROB01 / ROB02 (tabla de 0x14 bytes en 0x3eebe4: piloto init 0x201b10, control 0x201c40, dibujo
  0x202bf0; acompanante 0x202c58 / 0x202de0 / 0x204618; PC 4884D0 / 488DD0 / 4879B0 y 4885A0 / 489A90). Personaje
  (PS2 0x205518 = PC 487EE0 y 0x2055b8 = 487F50) = indice de la tabla de ficheros 0x36f7d8: fuera de la partida en red
  (bandera 0x59acac) el piloto es el 11, `CHR_DR_M00` (pelo castano, camisa naranja: Alberto), y en las variantes 1, 3,
  4 y 5 (modo OutRun y Coast 2 Coast) la acompanante el 9, `CHR_DR_L00` (la chica rubia: Jennifer). Son los modelos
  ligeros de carrera (1426 y 1886 vertices con las cuatro manos); `MAL`/`FAL`/`GAL` (con cara y pelo) son los de los
  finales. CONFIRMADO.
- Esqueleto y malla: bone.bin 3 (53 huesos) y 2 (50); piezas rigidas en su hueso y piezas con pesos de hasta 4
  matrices `inv(enlace) M(hueso)` (514280 / PS2 0x2076b8); los pesos van en la UNPACK que sigue a las normales segun la
  clave de la malla (ver ASSETS.md, Personajes); manos: puno o abierta (513A30 / 513BA0 = PS2 0x206758 / 0x2068d8) en
  los huesos de rob_disp_init (PS2 0x206390: piloto 0x15 / 0x20, acompanante 0x2D / 0x1D), el puno si el movimiento
  esta en la lista de 34 de PS2 0x36f658 (487940). Sin cadenas osage en estos dos (+1C = 0): el pelo no tiene fisica.
  CONFIRMADO (render de referencia).
- Colocacion: work+10 = T(asiento) car+F0 car+B0 (PS2 0x204f30 / 0x189f58 = PC 488AF0 / 46BC10), asiento = +108 (piloto)
  y +118 (acompanante) de la disposicion del modelo (la de conv_coche.py; p. ej. el 360: (-0,39, 0,64, 0,27) y (0,38,
  0,64, 0,27)). Solo en red se sube y por car+D18. Escala 1, sin LOD (el original no tiene otro modelo ni distancia).
- Cuando se dibujan: el dibujo (0x202bf0 / 0x204618) solo con car+4 & 0xC000 == 0x4000, las vistas exteriores de la
  camara (4A2BA0: 0x8000 la interior); no mira el modelo: van en todos los coches, tambien en los cerrados (se ven por
  los cristales; en el F40 apenas). En la seleccion de coche no (4A5B20 dibuja solo el coche): la demo tampoco.
- Motor (comun/robot.c, comprobado contra el de conv_chr.py: error < 1e-5 en las matrices): claves de Hermite por canal,
  IK de dos huesos y de apuntar (brazos, piernas, pecho y cabeza son cadenas), mezcla de 10 cuadros al cambiar de
  movimiento (SetMotionConnect) con la postura guardada (las variables 85B2xx valen 0: el hueso 0 mezcla su y dos
  veces, como el original).
- Logica (solo la variante 1, el modo OutRun):
  - Piloto: conduce con el movimiento de su modelo (tabla 0x40e638; p. ej. 0x6001d HANDOL_360) con el cuadro sacado del
    volante, (car+204 + 0x7FFF) / 65535 x 168 (PS2 0x201d00; el PC usa otras constantes); en los coches con tabla de
    mirar (0x40e728: los abiertos) tras 121 cuadros con el cuadro entre 83 y 87 (volante centrado) apoya el codo en la
    puerta (ida, mano en la puerta guiada por el volante, vuelta: HIZIOKI_*). Tras un choque, al recuperarse (car+2F8),
    uno de 0x40e938 / 0x40e918 segun el estado del choque (tabla 0x40e230) y las cuentas que avanza la acompanante;
    TIME OVER 0x60000. CONFIRMADO.
  - Acompanante: empieza con 0x25000c en bucle; quieta, a los 181 cuadros una de la lista de aburrida (maquillarse,
    el pelo, ...); en el choque 0x6005f y al recuperarse una de aterrizar o de salto; fuera del asfalto (car+248 &
    0x10F407C, la superficie bajo el centro) 0x220007 / 0x230003; giro brusco a mas de 100 km/h (|car+4E| entre 0x2000
    y 0x4000) solo en los abiertos; GOAL 0x60049 (cerrados 0x25000c) y TIME OVER 0x60036; al acabar cada reaccion
    vuelve a 0x6004b (sentada). Listas 0x40eb28 / 0x40eb68 segun 45B380 (PS2 0x1aa3a0: 1 en GTO, 512 BB, Enzo, F40,
    250 GTO y el modelo 12; el PC lo llama "convertible" pero son los de techo). CONFIRMADO.
  - PROBABLE: el estado 8 del piloto dura la cuenta atras (el original espera a que el temporizador de salida 49B2D0
    baje de 120); Calc avanza 1 cuadro por paso de logica.
  - Meta del piloto (estado 9): hecha (2026-11-10, ver Escena de la meta).
  - PENDIENTE (sin dato en la demo): el cambio de area 44BCE0 (car+184 y el
    suceso 0x549068 = 0x14; en el modo OutRun las tablas del piloto, 0x36f620, valen -1), asustada y trompo (car+4 &
    0x1000 / 0x2000, car+0xC), el choque de car+4 & 0x30000, las poses pedidas (+68 / +70), el peso de pelo de
    4F1E80 y la escala de la luz car+0x58 de 487880. En el checkpoint del modo OutRun el original no hace nada
    especial con los personajes (las tablas del cambio de area del piloto son -1).
- Luz: la de los coches (entorno 2, 40D840(2) en 487880) con el material de cada malla, como luz_vertices de
  tools/modelo.py (tabla de 33 valores de n.l por material y cuadro).
- Verificado en Flycast (hoja en el scratchpad, chr/hoja.png): 360 Spider conduciendo, codo en la puerta y
  maquillaje, derrape, choque contra el muro y la reaccion despues, META (la acompanante 0x60049), F40 (cerrado); y
  sin el coche (`-DDIAG_SIN_COCHE`) para ver las posturas, iguales al render de referencia de conv_chr.py. Coste en
  DREAMCAST.md.

## Coches: la pareja de la carrera (2026-11-11)

Antes la demo llevaba siempre la pareja de OUTRUN2SP (CHR_DR_M00 y CHR_DR_L00) tambien en el juego de la consola.
Ahora como el original (SLES-53998; PC race_robots.cpp 487EE0 / 487F50, native_runtime.cpp 488090):
- Quienes (PS2 0x205518 / 0x2055b8; indices de la tabla de ficheros 0x36f7d8): con [0x443510] (PC [78026C]) = 3 el
  piloto 0xC y, sin la consola, la acompanante 8: ese modo (las rondas de la demo de 49EED0) no esta en la demo de la
  Dreamcast (SIN PORTAR; no se convierten CHR_DR_MH00 ni CHR_DR_GH00_USA). Sin 43F860 (PS2 0x1b4878 = [0x59acac],
  OUTRUN2SP) el piloto 0xB y la acompanante 9 en las variantes 1, 3, 4 y 5 (tabla de saltos 0x40e620) y 6 en el resto.
  Con 43F860 (el juego de la consola) la licencia activa +0x20 (PS2 [0x588688] = PC [7C2400], la foto) pasa por la
  lista {0, 1, 2, 4, 3, 5} (0x40e210, copiada a la pila) a k; piloto = 0x36f928[k] = {11, 18, 17, 9, 5, 19}[k]
  (M00, S00, W00, L00, G00, H00); acompanante = 0x36f940[0x378190[3 k + n]] (505340 = PS2 0x28e888; la tabla
  0x36f940 tiene los mismos 6), n = 1 en la variante 2, 2 en la 9 y 0 en el resto: con piloto hombre (k 0..2) una de
  L00 / G00 / H00, con piloto mujer uno de M00 / S00 / W00. CONFIRMADO en el MIPS. La variante 5 (3 + 46C520 =
  [0x6162ac], PC [7F94D4]; con otro valor lee el indice 6, detras de la tabla: 0x879) no esta en la demo: SIN PORTAR
  (`robot_pareja` devuelve -3 y la carrera va sin personajes). Sin licencia activa: foto 0 (la de una nueva, 4471A0).
- Datos: `conv_chr.py --pares` hace una CHRppaa.DAT por pareja posible (28: la de OUTRUN2SP 11 / 9 y 11 / 6, y las de
  las 6 fotos con n = 0..2 y la variante 5) y CHRPAR.DAT con las tablas de arriba (`tabla_pares`); CHR1109.DAT es
  igual byte a byte que el CHR.DAT de antes. Esqueleto por personaje de la tabla de saltos 0x40e3f0 (0x205038): 5, 9,
  19 -> 2 (50 huesos), 11, 17, 18 -> 3 (53); manos de rob_disp_init 0x206390 (5, 9, 19: 0x2D / 0x1D; 11, 17, 18:
  0x15 / 0x20).
- obj_chr_dr_s00_pmt y obj_chr_dr_w00_pmt tienen registros de 64 bytes (8 tablas; el resto 72): con 72 las tablas de
  esos dos se cruzan y con 64 cuadra todo (la tabla de texturas apunta a la cabecera de justo detras). PROBABLE: el
  cargador mirado (0x118aa0) recoloca de 0x48 en 0x48; no se ha encontrado como los lee el original. Sus tablas 2, 4
  y 5 tambien son otras (12, 32 y 72 bytes por registro; los materiales en unidades de 1 y la potencia en +64: x 128
  quedan como los de los otros, el de S00 es el de M00 / 128) y tienen una sola textura (palabra 3 de la tabla 4 a 1
  en todas: PROBABLE "con textura", la unica): tools/conv_chr.py Pmt64. Sus mallas
  llevan claves 0x22n / 0x26n con las mismas UNPACK que 0x20n (n matrices) y 0x228 / 0x22a..0x22c con un V4-8 (0x6e)
  detras de las posiciones, (0, 0, 0, 127) en todos los vertices: se ignora (PROBABLE sin efecto). Su microcodigo es
  otro (tabla 0x3592b0: 0x202 -> 0x31edb0, 0x262 y 0x228 -> 0x315760; 0x203 / 0x223 / 0x243 el mismo 0x313db0): lo
  que cambie en la luz, SIN PORTAR (la demo los ilumina como los demas).
- Osage (pelo con fisica) de CHR_DR_G00 (y G00_USA): dos cadenas en el hueso 0x22 (3 y 1 nodos; huesos 0x8000..3 de
  las piezas). Portado en comun/robot.c de rob_osage (PC 514F60 / 515040 / 5229A0 / 522810 / 522620 / 522450 /
  514EC0 / 522560 / 401790 = PS2 0x208b58 / 0x208ca8 / 0x290890 / 0x290a60 / 0x290c28 / 0x290e60 / 0x209078 /
  0x291388 / 0x100968, mismo algoritmo comprobado instruccion a instruccion en lo que se uso), constantes del ELF:
  gravedad 0x328708 (0,0027222), +24 de los nodos 0x36fb08 (0,002), 20 pasos de colocar ([0x378368]), eje 0x422990
  (1, 0, 0), viento 0x36fb20 (0: nadie lo escribe), [0x36fb10] = 0; esferas de los personajes 5..8 (tabla de saltos
  0x40f680 -> 0x40f6b0: hueso 0x22 radio 0,15 (0,2 el ultimo nodo) en (0,02, 0, 0,08) y otra fija de 0,25 en (0,
  0,26, 0,37)). Cada paso, tras Calc (488450 con [0x36fb10] = 0); la primera vez coloca y da 20 pasos. Las matrices
  de los nodos van en M[n huesos + i]. El init (514F60) tambien coloca con el +24 de la plantilla (basura de la pila):
  el primer control lo vuelve a colocar, no queda nada.
- Carga: CHRPAR.DAT y un bloque fijo de 272 KB al arrancar (antes que los huecos: la mayor pareja sin texeles,
  CHR1919.DAT, 269 KB; antes CHR.DAT 219 KB: +53 KB de RAM fija); la pareja se lee en el hueco de la union, aun
  prestado, al empezar la carrera (`robot_carga`, solo si cambia); sus texturas a pvr_mem (de 7 KB a 85 KB segun la
  pareja; la de antes 22 KB) y se quedan entre carreras. CHR_MT 200 (CHR0517.DAT: 194 matrices).
- Voces: la lista de "aire" es la de los personajes 9 y 10 (489A90) si la acompanante es uno de ellos (voces.c
  `voces_acompanante`). El juego de voces (0x11f048) va por la variante, no por el personaje; con el idioma espanol
  todos los juegos de 0x3527a0 dan SPANISH: las voces son las mismas con cualquier acompanante. CONFIRMADO.
- Seleccion de coche y licencia: en la seleccion (4A5B20) el original solo dibuja el coche; la licencia muestra la foto
  (2D, U_FOTO de MENU.DAT, ya portada): ninguna de las dos usa estos modelos. Lo demas que mira el personaje: el
  esqueleto, las manos y el osage (hechos) y la lista de "aire" (hecha). El pelo de la acompanante (514B60 del PC: el
  objeto del hueso 0x22 de los personajes 9 / 10), CORREGIDO 2026-11-13 (queja del usuario: "el modelo de la chica no
  tiene pelo"): antes se daba por no dibujado en el PS2 mirando 0x206d80, pero el dibujo de la carrera (0x202bf0 /
  0x204618 -> 0x201948 -> 0x206110 -> 0x206a78) es la otra variante, que tras el cuerpo y las manos llama a 0x207078:
  por la tabla de saltos 0x40f400 [personaje - 5] el 9 (CHR_DR_L00) pone el objeto fijo 0xBF0028 (lui / ori de
  0x2070dc / 0x2070e8) y el 10 (CHR_DR_LH00) el de object_db_bin de KAM_DR_LH00 (0x40f3f0), en el hueso 0x22
  (0x20711c) con T(0x40f3e0) = (0, 0, 0). 0x206d80 (la variante sin pelo) es la de 0x201a18. tools/conv_chr.py pelo():
  pieza rigida mas en las parejas con el personaje 9 (CHR0509, 0905, 0909, 0911, 0917, 0918, 0919, 1109, 1709, 1809,
  1909: +9 KB). Su malla es de la clave 0xa01 (dos posiciones, dos normales y UV V2-16 o V2-32: la forma y la de la
  mezcla del VU1 con el peso global 406800, que en la carrera nadie pone: PROBABLE peso 1, la primera forma). La cara (0x206e88) no la ponen los personajes de carrera (rob_disp_init no la llena).
- PENDIENTE de comprobar en el original: con las fotos 3, 4 y 5 el piloto es una mujer (L00 / G00 / H00, esqueleto 2
  de bone.bin) y el acompanante un hombre (esqueleto 3), pero los movimientos son los mismos del puesto (conducir
  ORT_MAN_*: esqueleto 3 en la entrada del movimiento; acompanante ORT_WMN_*: esqueleto 2) y no se ha encontrado
  ninguna conversion: SetMotion (PS2 0x207fa8, PC 4F2280 / 520750) y los canales (522F70) van por el indice del hueso
  del esqueleto del personaje, SetBone (0x207e60 / 4F1CF0) toma el del personaje (487C30) y 0x40e638 (el de
  conducir) solo mira el modelo del coche. La demo hace lo mismo y con esas fotos los dos salen deformados (brazos
  y piernas estirados). Las fotos 0, 1 y 2 (piloto hombre, acompanante mujer) salen bien (Flycast: 11 / 9, 18 / 5,
  17 / 19 con -DDIAG_SIN_COCHE). Una licencia nueva tiene la foto 0.
- Prueba: `-DPRUEBA_FOTO=n` fuerza la foto de la licencia.

## Graficos: tiras, nodos, recorte y distancia (como el original)

Queja del probador: poligonos estirados que tapan la carretera, estructuras que se cruzan y distancia de dibujo
inventada. Causas y arreglo:

- Tiras del VU1 (CONFIRMADO, microcodigo): las mallas (tabla 1 / UNPACK de posiciones) son lotes de hasta 59
  vertices con varias tiras pegadas. El microcodigo que les toca (tabla de programas de SLES-53998 en 0x3592b0: 32
  bytes por entrada {inicio, fin, 0, 0, palabra 1 de la malla, palabra 0, paso}; p. ej. 0x10200/0x212 -> 0x31aae0) hace
  por vertice `MTIR vi7, y` (16 bits bajos de la y) `IAND vi7, vi7, 2`, `FCAND vi1, 0x3ffff` (CLIP de los 3 ultimos
  vertices), `IADDIU vi1, vi1, 0x7fff`, `IADD vi1, vi1, vi7` y `MFIR.w` a la W del XYZ2: el bit 15 (ADC) queda puesto
  si el bit 1 de la y vale 1 o si algun vertice del triangulo sale del volumen de recorte, y el GS no dibuja ese
  triangulo. Los mismos pasos estan en todos los programas mirados (0x31b1b0, 0x3162b0, 0x326880...). En los datos: los
  dos primeros vertices de cada tira la llevan (145 693 de 145 813 tiras de STAGE) y marca el empalme cada 8 vertices
  en las de calzada. En STAGE 1 856 418 de 5 180 389 triangulos (36 %) no se dibujan. Antes: cortes por "bordes
  anormales" (arista > 6 x mediana y > 15 m) y quitar "puentes" por normales opuestas: en PALM dibujaba 28 004
  triangulos que el original no dibuja (caras raras y placas) y quitaba 1 158 que si (LAKE 24 176 / 577, ALPI-2 19 940
  / 2 182). Ahora `vif.no_dibuja` y `modelo._tira` parten cada lote en las tiras que dibuja el GS, sin heuristicas
  (tambien coches, trafico, perfil y asfalto de conv_tramo).
- Nodos (CONFIRMADO, decompilacion del PC: pc_pmt_records.hpp PmtNode, walk_404d10, node_visible 404B00): la tabla 0
  es el PmtNode de 56 bytes (+00 banderas, +04 esfera, +14 sesgo de LOD, +1C matriz, +20 hijo, +24 hermano, +28 malla
  por LOD) y la tabla 1 la malla del nodo {n opacas, n de mezcla, primera opaca, primera de mezcla}. Antes se leia la
  tabla 1 como {cuenta, tipo, primera, una} y se perdian las de tipo 2 y 3 (opacas + mezcla): esas tiras salian sin
  la matriz de su nodo, en el origen del bloque (estructuras cruzadas); las tiras de nodos con matriz y "tipo 1" se
  tiraban por "sombras". Ahora `modelo.nodos` recorre el arbol como 404D10 (hijos antes que el hermano siguiente,
  matriz = m * padre, vectores fila) y solo se dibujan las tiras de nodos (las demas no las dibuja el original). En
  PS2 no hay jerarquia ni LOD 1-3 en las etapas (todos los nodos son hermanos con mesh[1..3] = -1).
- Carteles orientados a la camara (bandera 0x800 con matriz; face_camera de 404D10): ahora giran en y hacia la vista
  (`carga_cartel` en main.c, vertices en el marco del nodo). 571 en FORE (segmento 10), 10 en la bifurcacion.
- Registros que la etapa no dibuja: la tabla cs_CS_<zona>_bin (una parte por registro, una lista por seccion de
  carretera: u16 n y n pares (byte, mascara)) no nombra nunca los registros 8 y 9 de PALM ni el 8 y 9 de LAKE (y 5, 6
  vacios); PALM 8 repite en instancias la geometria del registro 2 (100 % de sus vertices) y el 9 un 35 % de la del
  4; LAKE 8 un 82 %. Ahora no se convierten. PROBABLE (que la tabla sea lo que manda en la PS2; en el PC
  race_area_geometry_44de10 recorre cada registro con la lista de la seccion del coche). Las listas por seccion en si
  NO cuadran como conjunto visible (los nodos de la calzada bajo el coche no estan en la lista de su seccion; ni el
  complemento ni diferencias acumuladas ni con la seccion 0 cuadran): formato de PS2 sin descifrar, no se usan.
  Comparadas (2026-11-09) con la visibilidad precalculada de tools/pvs.py en PALM (secciones de 8,16 m, 591; nodos de
  la tabla llevados a los del fichero, mismos centros): el 98 % de los nodos que nombra una seccion se ven desde ella
  segun el PVS (frente al 76 % de todos), pero son unos 68 de 2511 por seccion, en rachas de 1,4 secciones (cada nodo
  aparece en ~11 rachas), con la mitad detras de la camara; su XOR acumulado (947 nodos de media) deja fuera el 57 % de
  lo que se ve delante. No son el conjunto visible ni sus diferencias: SUPUESTO que sean otra cosa (p.ej. lo que hay
  que dibujar ademas de lejos, o los nodos con algun efecto); no se usan para la visibilidad.
- Visibilidad precalculada (2026-11-09, DIFERENCIA solo de coste, no de imagen): el original dibuja todo lo que pasa
  el LOD y el recorte de 404B00, se vea o no; la demo no envia las mallas que no se ven desde ningun punto de la
  seccion (tools/pvs.py, z-buffer por software desde muestras del ojo en toda la zona transitable y todos los rumbos;
  DREAMCAST.md seccion 2, Visibilidad precalculada). Capturas congeladas con y sin (PALM, LAKE, ALPI, CAST, FORE y la
  union de PALM, sin tope): 0 pixeles distintos. Vertices de la etapa en la misma posicion: -29 % PALM, -14 % LAKE,
  -51 % ALPI, -44 % CAST, -48 % FORE, 0 en la union; en CAST lo que dibujaria el original pasa de 88518 a 12469
  vertices y la distancia efectiva de 192 m a 4000. En marcha (mediana de la distancia del nodo quitado mas cercano):
  PALM 443 -> 527 m, LAKE 671 -> 926, ALPI 450 -> 4000 (nada quitado), CAST 257 -> 550, FORE 641 -> 4000.
- Entorno cs_ENV_<zona> y cs_ENVBK_PALM: en el PC solo los dibuja race_env_model_44cd30, llamado desde cube_scene
  (cubo del reflejo del coche, 414340), nunca en la escena. Antes se anadia cs_ENVBK_PALM a la union y su calzada
  repetia la de cs_BK_PALM a la misma altura (616 de sus 1162 triangulos a menos de 15 cm: el parpadeo de las rayas).
  Ahora no se dibuja (CONFIRMADO).
- Ramas de la union (race_area_stage_draw_44e290 = PS2 0x172ba0): registros 0 tronco, 1 rama izquierda, 2 derecha;
  el original dibuja la rama del coche (obj | 1 o | 2 segun 451350) pasada la posicion 0x1C y el tronco hasta la 0xE3
  (ver "Bifurcaciones: registros de la union", 2026-11-13). Ahora como el original (demo/main.c `uni_regs`).
- Recorte y distancia (CONFIRMADO): camara de carrera de PS2 en 0x178580: +0xBC = 0,1 (cercano), +0xC0 = 4000
  (lejano); en el PC camera_perspective_404310 con esos campos. LOD por tamano en pantalla de 404B00: un nodo se
  descarta si 127 - exponente(r / (z tan(fov_v / 2))) >= umbral, umbral 6 en la etapa (4044E0(6) en
  race_area_display_44f120) y 7 con la bandera 0x40 del nodo: se dibuja hasta z = r 2^(umbral - 1) / tan(fov_v / 2)
  (con la fov de la demo, 57 r o 114 r). Antes: plano cercano 0,5, distancia adaptativa 600-1000 m con "detalle" que
  quitaba mallas pequenas (clip(rad x 30, 150, 1000)) y 450 m para las instanciadas, todo inventado. Ahora cerca 0,1,
  lejos 4000, LOD del original por nodo (`nodos_visibles`) y recorte por esfera exacto (antes 1,3 r fijo, que con fov
  ancha quitaba mallas de los bordes). El VU1 no dibuja el triangulo con algun vertice fuera de su volumen (banda de
  guarda del GS); la PowerVR no la necesita: se recorta contra el plano cercano (`tri_recortado`) y no desaparece nada
  cerca de la camara. La distancia solo se acorta por seguridad si un cuadro no cabe (ver DREAMCAST.md).
- Presupuesto: lo que dibuja el original de la etapa (16000-47000 vertices en PALM) no cabe con el coche (~10000) y el
  trafico (~1000-1500 con su LOD). DIFERENCIA medida y documentada (DREAMCAST.md, seccion 2): se dibuja entero lo
  cercano (3/4 del tope de la etapa, por la distancia de la parte mas cercana de cada nodo) y de lo de mas alla se
  quita primero lo que el LOD de 404B00 quitaria antes (p = z / distancia de LOD del nodo: lo pequeno y lejano) y lo que
  la niebla del original ya tapa; cortes con histeresis. Antes (2026-11-08, `corte_d`) se quitaba todo lo que estuviera
  mas alla de una distancia que oscilaba. Distancia del nodo quitado mas cercano (Flycast, mediana): PALM 166 -> 224 m,
  LAKE 278 -> 660, ALPI 200 -> 387, CAST 185 -> 258, union de PALM 291 -> 655; vertices de la etapa ~5500 -> 11600-15700.
- Verificado (E, Flycast, PRUEBA_AUTO a 100 km/h, PALM, LAKE, ALPI, CAST y la union de PALM): compila sin avisos; hoja
  antes/despues en el scratchpad (graf/hoja.png): sin las placas y arcos translucidos de la salida de PALM ni los
  carteles retorcidos de ALPI, calzada completa; con `-DDIAG_SIN_TOPE` la vista entera del original (playa con
  windsurfistas y ciudad al fondo en PALM).
- PENDIENTE: saber que son las listas por seccion de cs_CS_<zona>_bin (en la PS2; el PC usa otro formato; no son la
  visibilidad, que ya se calcula en la conversion); medir en la consola real lo que gasta el TA por tira (`ta_f`) y el tiempo de CPU del cuadro; banderas
  0x4/0x8 de los nodos (sin uso conocido); 451350 (rama antes de elegir).
- Coordenadas (comprobado, sin cambios): posiciones V3-32 float sin STMOD; normales V3-16 /32768; UV V4-16 con STROW y
  STMOD 2 (sumas acumuladas) /4096; STCYCL 1/3 (paso de 3 qwords); W de la posicion sin escribir (el microcodigo no la
  lee). Matrices de la tabla 6: vectores fila, traslacion en la ultima fila (igual que el PC).

## Curso entero (arbol del modo)

- Arbol: SCRIPTS `\bin\csc_data_2.bin` (15 registros de 0x78 tras la cabecera: +0x04 clave, +0x1c descriptor de tramo,
  +0x20 de union, +0x2c/+0x30 hijos). Niveles: PALM; LAKE, ALPI; CAST, FORE, DESE; CLOU, INDU, SNOW, GHOS; metas TULI,
  METR, RUIN, IMPE, CAPE (STAGE segmentos 0, 3, 5, 8, 10, 11, 7, 4, 6, 9, 12, 13, 14, 16, 15).
- Encadenado: la union se coloca en su etapa con el +0x98 del descriptor de tramo (x, y, z, giro y; giros x/z 0 en
  todos); la etapa siguiente en el final de la rama (colision LBK/RBK_PALM). Cada fichero en su marco; la demo los
  coloca al leerlos y recentra el marco en el principio de cada union (traslacion; el giro se acumula).
- Uniones: bifurcacion = cs_BK_PALM (tronco y las dos ramas) + porticos obj_course_obj_bk_palm con los carteles BUNKI_TX de
  los dos destinos en sus 4 texturas hueco de 8x8 (asignacion por la posicion de cada malla: PROBABLE); meta =
  cs_BK_<zona>. Los cs_BK_/cs_ENVBK_/obj_course_obj_bk_<zona> de las demas zonas que nombra el ejecutable no estan en
  el disco: se usan los de PALM (SUPUESTO: que el original haga lo mismo).
- Niebla, cielo, luz y sol: ver la seccion Entorno.
- Fallo de la union (coche hundido y planos grises inclinados al entrar en ella), causa y arreglo: (1) la ruta de cada
  etapa acababa hasta 20 m antes del principio de la union (la de LAKE, 19,7 m) y la de la carrera daba ese salto en
  un paso de 4 m; ahora conv_tramo alarga la ruta en recto hasta el extremo de la calzada de su colision (quedan
  1-9 m) y ruta_anade rellena lo que falte con puntos interpolados. (2) cs_ENVBK_PALM (malla 8, muros de 5 m) une en
  una misma tira los muros de los dos lados de la calzada y el triangulo puente cruzaba la carretera a 0,4-2,9 m de
  altura entre 15 y 35 m dentro de la union (medido en el PC con la malla colocada: tools en el scratchpad); la PS2 no
  lo dibuja (bandera ADC, seccion Graficos); y cs_ENVBK_PALM ya no se dibuja: es del reflejo del coche, y su calzada
  repetia la de cs_BK_PALM a la misma altura (parpadeo). Las alturas de la malla, la colision y la ruta coinciden (0 m
  de diferencia en la calzada). Verificado en E (nodo 1, PRUEBA_FIN=900).
- Inventado que se ha quitado: las mallas sin textura de los tramos iban con un gris translucido fijo (0x60202020,
  `SINTEX` de modelo.py); ahora llevan el difuso de su material (tabla 5) con la luz por normal, opacas si su alfa es
  128. La ruta de TULI y METR sale de la colision (su SOO no da la carretera); el rotulo de bifurcacion usa nombres de
  zona escritos en main.c (no salen del disco).
- Verificado en E (Flycast, PRUEBA_AUTO, PRUEBA_NODO para salir en cada etapa): ruta A PALM-LAKE-CAST-CLOU-TULI y
  ruta B PALM-ALPI-DESE-GHOS-CAPE, mas GHOS-IMPE; las 3 metas probadas dan GOAL; una prueba larga encadena PALM -> LAKE
  -> CAST (dos uniones, recorte de la ruta y recentrado) sin perder RAM. No probados en E: FORE, INDU, SNOW, METR,
  RUIN (convertidos y con la misma mecanica); desde 2026-11-09 las cinco metas dan GOAL en E (ver Ultimo cambio
  verificado). Cifras de RAM, VRAM y cargas en DREAMCAST.md. Texturas de los 14
  tramos nuevos revisadas en hoja (sin desordenar).
- PENDIENTE: objetos animados de BK; probar en R.

## Entorno: cielo, niebla, luz, sol y clima

Del original (decompilacion del PC: pc_environment_blend.cpp, pc_scene_environment.cpp, pc_frame_render.cpp, race_sky.cpp,
race_course_passes.cpp 40CBC0, race_scene_effects.cpp, pc_render_flush.cpp 40D840 / 410810, vs_15 de
pc_shader_programs_generated.cpp; datos de SLES-53998: descriptores de tramo, scn_env_fog/sun, banco 0x57). Hecho el
2026-11-08 (conv_tramo.py pon_cielo, conv_union.py soles / cielo_de / transicion / sol_dat, modelo.py luz_vertices,
main.c "entorno del original", cielo_dibuja, sol_dibuja, coche_luz).

- Tres entornos a la vez (CONFIRMADO): 40D840(e) carga la niebla 7D3A10 + e 0x1C y las luces 899B98 + e 0xA0; lo elige
  cada capa del dibujo (449050, tabla 5A27B4 = {1,1,-1,0,1,1,2,2,2}): e 0 = capa 3 (el cielo), e 1 = la escena (y el
  cubo del reflejo), e 2 = los coches (46C140 / 46BD30 llaman 40D840(2)). Las listas 0, 1 y 2 de scn_env_fog_<zona> y
  scn_env_sun_<zona> son esos tres entornos, no dia/tarde ni tramos. Registros de 0xB0: posicion inicial y final de
  carretera; la niebla cambia por tramos dentro de la etapa (CLOU: 6 tramos de 0,0006 a 0,005, niebla espesa;
  INDU, IMPE, CAPE: 2), la luz no (un registro fijo por lista en las 15 etapas).
- Luz (CONFIRMADO el modelo; PROBABLE que la PS2 lo calcule igual): vs_15 del PC: color = difuso del material x difuso
  de la luz x max(n.l, 0) + ambiente del material x (+78 + (+6C - +78) (1 + n.l) / 2) + emision (c1A / c1B de
  410810). Las normales de PS2 apuntan hacia dentro (la calzada de PALM: n.y = -1; vs_15 las niega). Etapas y uniones:
  en los colores de los vertices sin color propio (antes una luz inventada 0,7 + 0,3 |n.L|), con el material de la
  tabla 5 (tambien las texturadas). Cielo: la luz del entorno 0 (CAST, INDU, IMPE: atardecer naranja). Coche del
  jugador: la del entorno 2 en la pintura (CEX2), por cuadro con su orientacion (coche_luz; difuso y ambiente del
  material 1: los de la pintura son 1 y 1,09-1,1, sin el brillo car+58). Valor >1 se satura (el GS llega a 2).
- Cielo (CONFIRMADO): descriptor +0x44 banco obj_course_obj_sky_<zona>; 4521C0 dibuja +0x4C/+0x50 (grupo 0, el que se
  ve siempre) y +0x54 (grupo 1, solo en el cambio); +0x48 es el del reflejo (451C20). Antes se dibujaban todos los
  registros del bloque (y en SNOW ninguno: no se reconocia). El original lo dibuja sin z; aqui va sin escribir z.
- Union (CONFIRMADO): descriptor +0x58 = {u16 100, s8 2} en las 15: al pasar la posicion 100 de la union (807 m, con la
  etapa siguiente elegida y leida) la niebla y las luces se mezclan en 120 cuadros (44A000 / 44AB10) del valor guardado
  al del principio de la etapa nueva, y el cielo cambia en tres tercios de 40 cuadros (451E90): viejo +0x4C -> viejo
  +0x54 -> nuevo +0x54 -> nuevo +0x4C. Antes: niebla por distancia en toda la union y cielo de golpe.
- Sol y destello (queja del probador de la demo 22, "falta efecto del sol deslumbrando", 2026-11-09): el pase ahora es
  el del PS2 (0x10f190), no el del PC. CAUSA de que apenas se viera: el valor de cada objeto (4056D0 / PS2 0x111998)
  SUSTITUYE el alfa del material y la demo lo multiplicaba por el (0x76 el sol de lejos 0x10, 0xAE el brillo 6, 0x30
  el destello 4: 2-5 veces mas flojos); ademas el sol solo esta en pantalla con |x| < 360 y |y| < 280 (PS2; el PC 470
  / 440) y con la luz de las etapas (inclinacion 30-60 grados, cam+358 = 1) casi siempre queda por encima de la
  pantalla: se ve en las etapas de 30 grados (CAST, DESE, INDU, IMPE) mirando hacia el o en las subidas. Constantes del
  PS2 distintas del PC: bordes 360 / 280 con (360 - |x|) / 160 y (280 - |y|) / 180 desde 100; brillo 6 con escala
  fov x 0,1; destellos y brillo de cara al sol con alfa x 0,75 (0x10fee0) y este de tamano 2,5 (PC 1,5); lejos a 1500
  m (aqui 1000); sin las excepciones de etapa del anillo. Parte visible: el PS2 la mide con la media del alfa de 8 x 8
  pixeles (0x1036c0..0x103af4, guardada en 0x448864 y leida por 0x103b38) x 0,5 / 255 sin tope: 0,251 con el sol
  entero (alfa 0x80 = opaco del GS; PROBABLE). Verificado en Flycast con `-DPRUEBA_SOL -DPRUEBA_SOL_X/Y` (capturas
  aves/sol_*.png del scratchpad).
- Oclusion del destello (DIFERENCIA aproximada, 2026-11-09): la PowerVR no deja leer la z ni tiene consultas. Cada
  cuadro, si el sol se va a dibujar, sol_prepara pone UN rayo desde el ojo hacia uno de 16 puntos (4 x 4) del disco
  que cubre el objeto de prueba del PS2 (0x10 a 1/4: radio angular 2,96 x 0,25 x spread); dibuja_rango lo prueba
  contra cada malla opaca o de recorte de la etapa, la union y la etapa siguiente que dibuja de verdad en ese cuadro
  (esfera y despues los triangulos de la tira; carteles girados como carga_cartel), y la parte visible es 0,251 x la
  fraccion de los ultimos 16 rayos que no chocan (el PS2 usa el cuadro anterior: aqui 16 cuadros, ~0,27 s, de
  suavizado). No tapan los coches, el piloto ni lo transparente; los arboles de recorte tapan con el rectangulo entero
  (el PS2 mira el alfa por pixel). Medido en Flycast (PALM, sol detras de un edificio: 16/16 tapados, destello fuera;
  en el cielo abierto 0-3/16): por cuadro ~2000 esferas probadas (las mallas que ya se dibujan), ~25 tocadas, 400-800
  triangulos; coste calculado en el SH4 < 0,5 ms (solo con el sol en pantalla). `-DSOL_SIN_OCLUSION` lo quita.
- Sol y destello (CONFIRMADO salvo lo dicho): bandera 2 de +0x3C (todas menos CLOU, GHOS y METR); intensidad que sube o
  baja en 120 cuadros en la union (4AFCC0); direccion -luz 1 con la inclinacion x cam+358; objetos 2, 3..0x10 del banco
  0x57 (SOL.DAT) en el espacio de la camara como 40CBC0 (disco, lejanos, brillo, 11 destellos y brillo de cara al
  sol), tenidos con el ambiente del entorno 0. DIFERENCIAS: sin consulta de oclusion (la PowerVR no la tiene): la parte
  visible sale de rayos contra la escena (ver arriba, Oclusion del destello; el PC topa en 0,22); los
  lejanos a 1000 m en vez de 1500 (PS2; PC 2500) (el cielo esta en la escena); mezcla aditiva (PROBABLE: texturas de fondo negro).
- Niebla en la PowerVR: una tabla para todo: la de la escena (lista 1); los coches no llevan la suya (lista 2: casi igual
  salvo ALPI, 0,0003 frente a 0,0009, y CAPE/RUIN; DIFERENCIA); el cielo lleva la tabla solo si su lista 0 tiene
  niebla (CLOU).
- Clima (CONFIRMADO): bandera 1 (lluvia de SCN_EFC, la unica del original) en ninguna de las 15 etapas; no hay nieve ni
  hojas como particulas (SNOW: bandera 6 como PALM). 8 / 0x10 (faros y luces del coche: noche) en GHOS y METR
  (portado en la revision 42: noche_ban, ver Luces de noche y faros). Tuneles: nada del entorno los oscurece (la luz es fija en la etapa; solo la niebla cambia por posicion); las luces
  locales de maya_spl_<zona> (44A1D0, puntos a menos de 500 m de la camara) no se portan (PENDIENTE).
- cs_ENV_<zona> / cs_ENVBK: solo en el cubo del reflejo (414340 -> 44CD30); la demo usa el mapa esferico fijo del coche:
  no se dibujan (cubo por cuadro: TA y VRAM que no hay).
- Verificado (E, Flycast, PRUEBA_AUTO): PALM, LAKE, ALPI, SNOW, DESE, METR y la union PALM -> LAKE con la mezcla a medias
  (hoja en el scratchpad: cielo/hoja.png); sol con PRUEBA_SOL (sol delante, solo prueba).

## VMU: icono, eyecatch y pantalla LCD (2026-11-09)

No es paridad con el PS2 (la tarjeta de memoria del PS2 lleva un icono 3D del coche, `SYSTEM.PS2 \VIEW.ICO`) sino lo
que la Dreamcast espera de un juego: todo sale de graficos del disco (`tools/conv_vmu.py` -> VMU.DAT, `comun/vmu.c`).
- Fichero OR2C2C: cabecera VMS de vmu_pkg con "OUTRUN 2006 C2C" / "Licencia OutRun", app id OR2C2C, icono de 32x32 y
  16 colores en 2 cuadros ("OutRun" con las palmeras y "2006", recortes del logo del titulo, k-medias con la paleta a
  ARGB4444; velocidad 30) y eyecatch de 72x56 a 256 colores (el logo entero). 9852 bytes = 20 bloques (antes 10).
  Se escribe y se lee en bruto (O_META) y se comprueba con vmu_pkg_parse; si la cabecera no vale (ficheros viejos) los
  datos se buscan por la marca 'OR2L' como antes.
- LCD (48x32) de todas las VMU con pantalla (MAPLE_FUNC_LCD): en el titulo y los menus el logo ("OutRun" y "2006",
  2,5 s cada uno); en carrera la velocidad "%03d" con las cifras del velocimetro del HUD reducidas a 18 pixeles y la
  caja de cambios (cifras de CAJA6 a 9 pixeles, 5 o 6 casillas) con un marco en la marcha actual. vmu_draw_lcd encola
  sin esperar (si la unidad esta ocupada se reintenta); se compone como mucho cada 250 ms y solo se envia a las unidades
  cuya ultima imagen es distinta. La imagen se gira 180 grados (la VMU va al reves en el mando).
- Verificado (E, Flycast con rend.FloatVMUs): `PRUEBA_VMU` lee la licencia del formato anterior (vmu_pkg_parse la
  acepta), guarda con la cabecera nueva y la relee ("cabecera VMS bien (2 iconos, eyecatch 2)"); LCD del logo en A1 y
  A2 en el titulo y velocidad / marcha en carrera (`PRUEBA_AUTO`); gestor de memoria de la BIOS con el icono y el
  eyecatch. Previas en el scratchpad (vmu/). Pendiente: R (VMU real en GDEMU).

## Modos de un jugador (2026-11-09)

Inventario de las opciones del original (menu UN JUGADOR del frontal SUMO_FE: COAST 2 COAST, OUTRUN, INFARTO, CONTRARRELOJ,
SALON). La opcion elegida pasa al juego por la palabra empaquetada del frontal (4EEC80 del PC: `runtime_route_mode_4eead0`,
selector 1..6 -> variante [780258] = 1 OutRun, 2 Heart Attack, 9, 7 Time Attack, 8, 10; selector 0 -> misiones de
Races.bin, variante 5 C2C o 6) y el preset del curso [78024C] (4EEA70). En SLES-53998 la variante es 0x443508
(reiniciada a 1 en 0x1b4898). El port: `menu_modo()` (comun/menu.c) da 1 / 2 / 7 y main.c la usa en toda la carrera.

| Modo / opcion | Original (evidencia) | Estado en la DC | Verificacion |
|---|---|---|---|
| OUTRUN (variante 1) | fila 0 de los tiempos (44DC70), trafico, puntos 4F2DF0, voces ORC/PERO | OK (lo que ya habia) | E |
| INFARTO (variante 2) | gestor de peticiones 464350/464D20 (PS2 0x1ae440 / 0x1a7f28), misiones 5A94D8 (PS2 0x3f0b98, 41 x 0x140), fila 1 de los tiempos, voces HC/PERH + HAM\<etapa> | PARCIAL: ver abajo; el juego de cursos del tablero 0x24 desde 2026-11-13 (ver Tableros de cursos) | E |
| CONTRARRELOJ (variante 7) | fila 2 de los tiempos (44DC70: 0 y 7 -> 2; 450CC0 pasa el 7 como 0), sin trafico (479080 y 47EC00 vuelven con la variante 0, 7 u 8), gestor 0x193 / funcion 0x22 (48B8F0..: curso por codigo [656234], fantasma 465F30..4687C0, GHOST.PS2) | PARCIAL: reglas de tiempo y sin trafico; desde 2026-11-13 el curso de los tableros (ver Tableros de cursos): una etapa, las metas A..E (y del reves) o los cursos seguidos | E |
| COAST 2 COAST (variante 6 misiones, 5 campeonatos) | variante 6: misiones de Races.bin / RaceAssignment.bin (gestor 0x20, evento 0x191: 495B90 / 496A30); variante 5: campeonatos Request_Course_SP / OR2 / MIX con peticiones Req_*.bin (gestor 7F9460, evento 0x190); rivales (47CF40) | PARCIAL (2026-11-13): categorias, mapa de misiones y detalle del original (banco 0x44, C2C.DAT); se corren las 76 misiones de carrera (tipo 0) con sus coches de carreras y su nota; sin los otros tipos ni los campeonatos (ver COAST 2 COAST: misiones de carrera y el rival) | E |
| SALON (showroom) | frontal 4D4230.. (PS2 0x25b338..0x25db20) | OK (2026-11-13): ver SALON | E |
| Dificultad (opcion de la licencia +E0) | difficulty_of de 44DC70 (PS2 0x1cdc98): 0 en el juego de la consola (43F860 = [780260] = 1, lo pone SUMO_FE 0x237e28); la licencia (+E0) solo en el de OUTRUN2SP (2 en la variante 4) | OK (2026-11-11; antes al reves: la licencia en los modos de la consola) | E (log `MODO:`) |
| Multijugador | red / LAN (variantes 3, 4) | no se hace (orden del usuario): la pantalla queda como estaba, con la accion bloqueada | - |

**Rival en las etapas.** No hay rival en OUTRUN, INFARTO ni CONTRARRELOJ: los coches de carreras (sucesos 8.. de
`racer_setup_47cf40`) solo los monta el gestor de misiones de C2C (mission_manager.cpp:136, registro de Races.bin) y
el de la variante 9 (race_variant_owners.cpp:252); 4F2DF0 solo cuenta adelantamientos de rivales si [8421C0] != 0
(4B00D0, el de esas misiones).
Comprobado de nuevo en el ELF (2026-11-12, queja "en las etapas falta el Rival: otro Ferrari que va por delante"): el
montaje de los coches de carreras 47CF40 es en SLES-53998 0x1ef900 (semilla 0xA5DE, "%s %d" en 0x40b5c0, registros de
0xA0) y solo lo llaman 0x1f9e4c (gestor de misiones, estado 3, si el tipo de la mision 495B20 = 0x1f83e0 no es 4:
47CF40(+0x1C del registro de Races.bin, 0), como mission_manager.cpp:136) y 0x1f0918 (47D950 de la variante 9, que solo
da el selector 3 de la palabra del frontal, 4EEAD0; no esta en el menu UN JUGADOR del inventario de arriba). Sin ese montaje
[80FB00] = 0 y 47DAC0 / 47EC00 no mueven coches de carreras. El "Ferrari de delante" es de las misiones de carrera de
COAST 2 COAST (tipo 0, 76 de las 94): p. ej. 1-1 = 5 coches de carreras (c4 107, c8 109, c10 25, c14 5, c1c 5 -> 6
sucesos de coche, lista de modelos 2, c28 55) con dos especiales de las clases 1 y 5 en las plazas 1 y 2 (+0x14 = 103 /
105, +0x1C = 1,25 / 1,23, +0x20 = 0,825); 3-1 (tipo 2, derrape contra el rival) y 9-1 (tipo 3) un solo rival con +0x30 =
0x13 / 0xB y tabla +0x44. Portarlo es portar el modo C2C: el mapa de misiones (variante 6), las reglas (496A30), el
montaje 47CF40 con las tablas del ELF (listas de modelos 64E090, pares 5B3C34 / 5B3C74, colores 5B3A80, perfil 64DEFC),
los coches de carreras del modulo del trafico (avance sin coche 47BC90 con el tiron 476480 / 4764D0 y los vaivenes
476CA0 / 476DF0 / 476F40, apertura como suceso 0x55 con 46E830, seguimiento 477090, puestos 477510 / 477C90, lider en la
bifurcacion 477E00), los modelos de los Ferrari como coches del trafico (registros 4866C0, LOD) y el HUD de puesto. NO se
pone un rival en OUTRUN: no lo hay en el original. En el INFARTO hay coches de la mision (45D470, tablas de 45D710, ver abajo) y los de la
peticion 14 (escoltas 45B5B0, +324 = 6: carrera contra ellos), que no esta en las misiones base 0..14.

**INFARTO** (comun/infarto.c, tools/conv_infarto.py -> INFARTO.DAT, VOCESH.DAT, HAHUD.DAT):
- Mision de la etapa: base = indice del descriptor de tramo de la clave (44DC50; claves 0..14 -> 0, 1, 3, 6, 8, 9, 5,
  2, 4, 7, 10, 11, 12, 14, 13); desde el nivel 2 la variante especial 0x0F..0x28 segun las notas de los niveles
  anteriores (45C2C0 / 45C160 = PS2 0x1aedb8 / 0x1aec08). Cada mision: acciones previas (seccion, -1..-19, ...) y
  peticiones (seccion, fin, tipo, p6, p8). Ejemplo PALM (mision 0): -5, -1 (sin trafico), -14 tabla 0 (coches), -3,
  -6 anuncio (tipo 2: voz 0xE7), (90..400) tipo 2 con p6 15 / p8 21, (535..570) tipo 0.
- Corazones 7F23C4 (PS2 0x58a530), 10000 por corazon; nota 45C990 (PS2 0x1b0780: 12, 20, 30, 45, 60, >79; tope 5 con
  golpe, -3 adelantado, -4 choque); corazones del resultado 45DCE0 (PS2 0x1b3708; tabla por byte de nivel 0x3f0908 =
  0, 20, 50, 100 y suma por clave 0x3f0920 x 30000 con nota >= 5, segun el numero de peticiones de la mision); voz de
  la nota 45DDC0 (PS2 0x1b3880, tablas 0x3f0960..0x3f09e0 por tipo, ids de los bancos HAM). Fuera de peticion: choque
  -50 (0x150), adelantado tras choque -20 (0x14C), golpe de un coche mas rapido -10 (0x14B) (PS2 0x1a8100..0x1a8250).
- Tipos portados: 0 (derrape: PS2 0x1a8d88 / 0x1a90a8), 2 y 22 (adelantar: 0x1aab18 / 0x1aad20), 6 (sin incidentes:
  0x1aaf18 / 0x1ab058, tope por nota 0x3f07f0); tablas de puntos 45E2F0 (0x3efb20, 0x3efd60, 0x3efd78, 0x3efda8 =
  {choque, adelantado, muro/cuadro, golpe, rueda fuera/cuadro, trompo}: 250000, 250000, 833/333/666/83, 150000,
  83/83/0/0, 0/0/666/333). Donde el PC y la PS2 difieren manda la PS2: evaluacion del tipo 0 = (q - 0,9) / 0,1 x
  300000 + mantiene x 400000 + 800000 (PS2 0x1a9134..0x1a91c0; el PC resta el segundo termino).
- Coches de la mision (45D710 / 45D7A0 / 45D470 = PS2 0x1b2e58 / 0x1b2b20): tablas de 0x320 bytes en 0x35c958, clase
  3/4 -> modelo de 0x3f0850 [id % 7], 2 -> 0x1F, <= 1 -> 0x2B (no esta en la DC: se gasta sin coche); se abren de 30 a
  50 posiciones antes de su seccion y no se cierran por delante (+C5C 0x40000) hasta el fin de la peticion (45D8E0).
  Acciones -1 / -2 (46E4A0, 64E08C): sin trafico normal (46CA60 cierra el de la etapa) / con el.
- Medidor (45FB80 = PS2 0x1ac580): anillo de hasta 19 corazones (resultado / 10000 + 1, x 0,25; radio 64, 0,33 rad,
  centro (176, -96)) con 2B0015 / 2B000E / 2B000F del banco 0x2B y el efecto 0xD7 al aparecer uno: es lo que se ve al
  adelantar en la peticion de adelantar (cada adelantamiento: 800000 / p6 de resultado, efecto 0x50, voz de
  animo 0xC6..0xCA y el contador de efectos 0xD5).
- Rotulos (2026-11-09, 460970 / 460AD0 / 462D80 = PS2 0x1ad998): marco de 0x3f0b40 (2B001E..20) y texto de 0x3f0aa8
  (37 tokens de 2B0021..3D) del banco 0x2B, los originales en espanol (p. ej. "Sigue adelantando a los coches"), en
  (176, 78): la accion -3 (si la peticion que viene se porta) abre el generico 2B003D y a los 30 cuadros pasa al de la
  peticion hasta su fin; la nota abre su texto (5..0xB, marco 0 o 1) 120 cuadros. HAHUD.DAT: un atlas VQ por rotulo
  (error RMS 0..8,7) que se sube solo mientras se ve; las capas con el bit 4 de +0x48 (resplandores de fondo negro) se
  dibujan sumadas. SIN la deformacion de entrada/salida ni el vaiven (7F8BB4).
- Bono de nivel 45D360 (PS2 0x1b2978) con las estadisticas de 4628D0 en la union (posiciones con +D36 > 0 y con
  ruedas en (+24C.. >> 8) & 0x3B sin derrapar; 0,8 / 0,7 x t / 5, tope 5, x 10; 0 si una peticion del nivel saco nota
  <= 5). Mas la parte de la velocidad (2026-11-09): v = [7D3838 + nivel * 4] (4505F0 = PS2 0x1cfdf0: media de +1C4 x
  216,72 desde la ultima etapa terminada, la guardan 44FF20 al terminar la etapa y 4513C0 en la meta; tope 0x57E3F
  pasos) x el parametro +2438 del coche; si v > 250 (PS2 0x1b2ab0, 0x437a0000) suma (int)(v - 250) x 2. Medido en E
  (piloto de prueba): media 250,8 / 238,5 / 246,8 / 277,9 x 0,6276 = 157..174, sin bono de velocidad (hace falta una
  media de mas de ~398, o sea ~250 km/h en el velocimetro); niveles 2 y 3: +20 corazones cada uno (por la union).
- Modelo 0x2B (clase <= 1 de las tablas de coches): solo lo piden las tablas 20 (1 coche, mision base 4), 21 (17,
  mision especial 24) y 22 (4, especial 20); la DC no lo tiene (trafico 30..38): se gasta sin coche (PENDIENTE).
  Estudiado (2026-11-09): el 0x2B es un camion con REMOLQUE: su lista de LOD (registro de 4866C0, PC 650500) lleva
  segundo descriptor en +4 (conv_trafico.py llamadas() se para: "remolque sin portar"). Para portarlo hace falta, ademas
  del modelo (TRAF9.DAT solo en el modo, VRAM fuera de la zona del curso), la cinematica del remolque: 470360 (PC
  race_traffic.cpp "shadow_470360": punto de enganche +C70 / angulos +C7C seguido 12 unidades por detras en el marco
  del coche, matriz +C90 con el desplazamiento del segundo descriptor +4; inicio en 1008/1010 con 43EB60), el angulo de
  enganche +C2C de 474990 (recorte del volante) y las cajas del remolque en los choques (4F9000 con ia == 1, +C90);
  dibujo en race_ghost_car.cpp second() (carroceria, pilotos +14/+18, luces +1C/+20, lista +68, sombra +10, ruedas
  4AD8F0). Es terreno del trafico: no se hizo para no inventar un remolque rigido. PORTADO (2026-11-13) para las
  carreras de OUTRUN2SP (ver OUTRUN2SP, Camiones); en el INFARTO (juego de la consola) el 0x2B se sigue gastando sin
  coche (los camiones no se cargan fuera de OUTRUN2SP).
- Listas de aterrizaje / salto de la acompanante del modo (PC 5C0FB0 / 5C0FD0 = PS2 0x40ea30 / 0x40ea40): iguales que
  las del OutRun (0x40eb10 / 0x40eb20) y con las mismas cuentas: CONFIRMADO (comun/voces.c). Sus listas de movimientos
  (PS2 0x40ea48 cerrado / 0x40ea88 descapotable, PC 5C0FD8 / 5C1018) SI son otras (0x6003d, 0x60025, 0x240000/1,
  0x60022, 0x6006d, 0x230001...) y CHR.DAT no lleva 0x6003d, 0x6006d ni 0x230001: PENDIENTE (con las reacciones).
- Voces: el juego de voces del modo se cambia en el AICA (VOCESH.DAT 740 KB por VOCES.DAT 207 KB) al empezar la
  carrera, leido en el hueco de la union; banco HAM de la etapa por clave (4 distintos: grupos 0,0,0,1,1,1,2,2,2,1,0,3,
  0,0,3; PROBABLE que el juego lo elija por la clave). La acompanante es el personaje 6 (487F50), sin comentarios de
  zona ni de derrapes (solo variante 1 / 1, 3, 4).
- HECHO (Rev 1.1, ver "INFARTO: tipos de peticion, hoja de resultados, panel de la nota y la acompanante"): tipos 8 y
  23, la hoja de resultados de la etapa 4603F0, el panel de la nota (45BDF0) y las reacciones de la acompanante a las
  peticiones (487D60 / 487D90). SIN PORTAR (PENDIENTE): tipos 7, 9, 11, 15, 16, 17, 19, 20, 21, 24, 25 (no empiezan,
  sin anuncio ni nota), 14 / 18 (escoltas). La cuenta de
  corazones y la nota ya no son texto bfont: ver INFARTO: medidor, efectos y contador (2026-11-10).

**INFARTO: medidor, efectos y contador (2026-11-10)** (queja: "no aparece la animacion de los corazones"):
- Causa de que no se viera nada: HAHUD.DAT se abre al empezar la carrera y hud.c pedia sus tablas en un bloque (208 KB
  con las piezas de 40 bytes) que no habia: "Out of memory" y `ha_hud` NULL (sin medidor ni rotulos). Ahora
  (hud.c lee_tablas) las tablas de un fichero parcial se leen a trozos de 16 KB, con las piezas ya a 24 bytes y cada
  grupo en su bloque: se pide lo justo y sin huecos grandes (HAHUD ~150 KB en bloques de 23 KB como mucho; MENU.DAT
  tambien). Verificado en E: sin "Out of memory" (RAM libre 66 KB tras BK00).
- Medidor 45FB80 = PS2 0x1ac580 entero: corazon del centro con el color de 0x3f0b20[corazones % 7] (2B000F..2B0015,
  bucle 0x1F..0x3C), rotura 2B000E del centro al perder (uno cada 20 pasos) y cambio de color; al ganar (salvo tipos
  0, 6, 8, 23 con 7F2510 > 0, 0x1ac944) efecto 0x52 y un corazon que vuela del centro al anillo (45B860 = PS2 0x1abde8:
  tipo 0x3f0b91 = 2, 0x3f0b20[5], velocidad 1); al cerrar la peticion (7F19F8 = 2, 0x1ace3c) con 7 o mas a la vista
  cada corazon vuela a (280, 200) (0x3efdd8) con el movimiento 0x3f0b90[k % 3] y velocidad (20 - k) x 0,3 + 0,5 (antes
  se rompian todos: inventado). Efectos 45B950 = PS2 0x1abee0 (16 x 0x38, tabla de estados 0x3efde0): espera, abre
  0x4C..0x5A moviendose (k1 = 1/15, k2 = 0,335; movimientos 0..3), abre 0x5B..0x69 y se cierra. Tinte del marco del
  rotulo con resultado negativo (0x1ac718: 280000 -> rojo). El anillo, como estaba (con el primer cuadro de cada
  corazon nuevo con la y al reves, como 0x1acdd0 / 0x1acd34).
- HUD del modo (NAVI_PUB 0x1bd440, variante 2 en 0x1bd544): panel 2C002A (Tiempo total / Tiempo / Corazones) en lugar
  de 2C0028 / 2C0029 y sin la puntuacion (1bf3e8) ni sus mensajes; 4BCD50 = PS2 0x1bda80: filas 2C0037 + i (cuadro 1
  en (60, 0); tabla 0x404418) de los niveles terminados y de la etapa con "%3d" en (0x21D, 0x53 + 21 i) (estilo 0xA,
  rojo si es negativo), contador "%4d" de 7F1C80 / 10000 en (0x206, 0x1B2), aviso 2B000E en (166, 182) mientras baja
  (efecto 0xD4). Las filas se acercan de 3 en 3 (final de 464350): 7F244C -> 7F2438 (empieza sin el bono del nivel),
  7F2464 -> 7F2460. HAHUD.DAT: un atlas para todos los corazones y la rotura (18 KB VQ: solo cambia el icono de 30x30),
  HA_C0..HA_C5 (desde el cuadro 0x1F), HA_PANEL y HA_FILA0..4; VRAM fuera de la zona: 75 -> 18 KB con todo subido.
- SIN PORTAR: el signo "-" de las filas negativas (fuente de texto 42CCE0, no esta en la DC: sale la cifra en rojo sin
  el signo); la silueta 2C0067 que se llena (cuadro corazones / 37, tope 99): es una pieza con mascara (bandera 8) de
  un degradado blanco -> amarillo -> rojo que sube; la DC solo hornea mascaras de alfa (hornea) y aqui cuenta el COLOR
  de la mascara (sin descifrar como lo combina el GS del PS2) y 100 cuadros horneados no caben en los 18 KB de VRAM
  que quedan: BLOQUEADO. Tampoco la proyeccion del ancla del centro con la camara +34A > 1 (5E0AC8) ni el vaiven del
  rotulo (7F8BB4), que mueve el centro del anillo.

- Objetos de las peticiones (estudio 2026-11-09, para portarlos; nada de esto esta aun en la demo):
  - Uso real en las 41 misiones (INFARTO.DAT): tipo 15 x16 (misiones base 1, 3, 4, 6, 7, 8...), 16 x15 (2, 3, 7, 8, 9,
    13...), 7 x13 (especiales 15, 20, 21, 32, 33...), 8 x8 (5, 10, 14...), 23 x5, 9 x3, 11 x4, 24 x4, 14 x3, 19 x1 (39),
    20 x1 (16), 25 x1 (37); 17, 18 y 21 no salen. Puesta en marcha / control / evaluacion por tipo (464350 tablas
    464CB8 / 4650E4 / 464C5C; B[] de race_traffic_heart_main.inc): 7 y 24 comparten (45E8A0 / 463150), 8 y 23
    (45EEB0 / 463250: sin objetos, distancia de deslizamiento con car+0x264..0x26C), 16 y 25 (461850 / 463F00).
  - Fichero de objetos de la mision (45C470 = PS2 0x1af428): mision >= 15 y [78024C] == 0 -> tabla de saltos PS2
    0x3f05c0 por mision - 15 (oso_qt_niag1, -, -, mach1, -, alas1, amaz1, -, lasv2, -, -, east1, maya1, prin1, -, newy1,
    east2, maya2, prin2, flor2, newy2, east3, maya3, prin3, flor3, newy3); mision < 15 -> tabla 0x3f06a0 por 44DC50 - 15
    = la mision base (-, sequ, niag, lasv, alas, -, sanf, amaz, mach, yose, -, newy, prin, flor, -); con [78024C] != 0
    la 0x3f0630 (oso_qt_4c..) o ninguno. Los nombres de ciudades de C2C son solo nombres: son los del curso de OutRun 2.
    CONFIRMADO en el codigo de PS2. Ficheros en OSO.PS2 ("\oso_qt_*.sz", zlib), u32 tamano y registros de 0x28 bytes
    (4F0660 / 4F0D10: hasta y < -10000) {f32 pos[3], f32 angulos[3], u32 clase +18, u32 tipo +1C (funcion de suceso, PS2 0x1d76f4), u32 +20 (tamano
    del trabajo: tabla 0x40ab50, PS2 0x1d6c54; el PC lee aqui las dos cosas), u32 +24}
    en el marco de la etapa (7D2DA0). Clases que salen: 2 (10..30 en cada fichero), 6, 9, 10 (0xA), 11 (0xB), 18 (0x12),
    56 (1, en flor*) y 57 (0x39).
  - Clase -> modelo y escala (PC 5DA0A8 / 5DA0AC = PS2 0x40a958, 8 bytes por clase, lo usa 0x1d6db8): 2 0x570019,
    6 0x57001f, 9 0x57001e, 10 0x57001c, 11 0x57002b (escala 3), 18 0x570030, 56 0x1010000, 57 0x57001d (banco 0x57 =
    \Common\obj_course_obj_common_pmt.sz, registro = los 16 bits bajos; 0x101 = obj_course_obj_cs_flor). Tipo ->
    funcion de suceso (PS2 0x40a918): 0x3d, 0x3f, 0x5d, 0x40, 0x41, 0x42, 0x5f, 0x60, 0x61, 0x3e, 0x63, 0x64, 0x4f,
    0x65, 0x66 (bases de los carriles en 0x40a900: 0x20 / 0xfa, 0x11a / 0x1a, 0x15c / 0xa). En la zona 0x5C.. la PS2
    numera una mas que el PC (PROBABLE por lo que hace cada clase): tipo 2 (clases 2, 6, 9) = PC 0x5C (cuerpo rigido
    4ABA80 / 4ACD50 / 4ABCE0: conos), tipo 6 (10, 57) = PC 0x5E (4AA560 / 4ABF20 / 4AA620: el corazon que se toca, radio
    [6282A4] en la 0xA), tipo 7 (11) = PC 0x5F (4AA7D0 / 4AC200 / 4AA920: la puerta de dos postes; +7C = 3 al pasar
    entre ellos, 1 / 2 al tocar uno), tipo 12 (56) = 0x4F (luz).
  - Logica que lee cada peticion: 15 (4617A0 / 463DF0): objetos 0xA y 0x39 con +7C != 0 y +80 == 8 (el jugador; 4AA140
    caja del coche +D28 / +70 / +338..+34C contra el radio +90, en XZ); 16 / 25 (461850 / 463F00): clase [7F1C74] (p6 =
    11) con +7C == 3 (25: solo derrapando, +D36 > 0); su colocacion es la accion -11 (4611D0 con la clase 16: anchos de
    la tabla 5AFB58 + p6 x 16 entre las secciones de la accion -5, 7F24D0 / 7F24D4); 7 / 24 (45E8A0): clase [7F1C90]
    (p6 = 9) golpeada (+3F8, +3FC == 8) menos las de la clase 6; 19 (461940): cajas 0x12 que caen. Alturas y matrices
    sobre la carretera: 45DB70 (clases 0xA / 0x22 / 0x39 a la altura del suelo + [5B0084]; 5..9, 0xB, 0xE, 0x16 con la
    orientacion de la carretera 45D950). Sprites 2C0000 / 2C0002 / 2C0006 (banco SPRANI 0x2C) y efecto 0xD6 al tocar.
  - Comprobado en el codigo de PS2 (2026-11-09, para portar los objetos; nada de esto esta aun en la demo):
    - Numeracion de las funciones de suceso CONFIRMADA (ya no PROBABLE): el abridor de objetos (4F0D10, PS2 0x1d76e4..
      0x1d7724) lee el tipo +0x1C de cada registro, toma la funcion de 0x40a918 (0x3d, 0x3f, 0x5d, 0x40, 0x41, 0x42,
      0x5f, 0x60, 0x61, 0x3e, 0x63, 0x64, 0x4f, 0x65, 0x66) y abre el suceso (0x19c870). Tabla de funciones de PS2 en
      0x3ee7a0, 20 bytes por funcion {inicio, control, 0x1d0e00 dibujo, 0x1d1e70, 0x1d0f38}: 0x5d 0x1d1ac8 / 0x1d1af0
      (cuerpo rigido, PC 4ABA80 / 4ACD50), 0x5e 0x1d1a98 / 0x1d1af0, 0x5f 0x1d3c68 / 0x1d3d48 (corazon: radio 3,0 en la
      clase 10, efecto 0xD6, 2C0000; = PC 4AA560 / 4ABF20), 0x60 0x1d41d0 / 0x1d42f0 (puerta: postes en x = +-3,0,
      2C0002; = PC 4AA7D0 / 4AC200), 0x61 0x1d4b60 / 0x1d4d80 (aro, PC 0x60; lo usan las cajas clase 18 tipo 8),
      0x62 0x1d4b30 / 0x1d4d80, 0x63 0x1d52b8 / 0x1d5368, 0x64 0x1d55b8 / 0x1d56d8. O sea: PS2 = PC + 1 en 0x5C..
    - Dibujo 0x1d0e00: modelo +0x64 (el de la clase) con la matriz del trabajo; la clase 56 siempre; las demas con +54
      bit 0 (visible) y sin el bit 1 (parpadeo); 0x1105e0(9) para 0x57001c y 0x1105e0(8) para el resto (SIN
      DESCIFRAR que es). Toque 0x1d3720 (= 4AA140): radio previo 3,0 (0x1ee0004: 20), medias cajas x 0,5; solo el
      jugador para 0x57001c..1f, 0x570025..27, 0x57002b, 0x570030, 0x1ee0004, 0x1ee000d (los demas, los 24 coches).
    - Corazon 0x1d3d48: altura 1,2 (clase 57: 1,0), giro +0,05 por paso; al tocarlo escala 0,3; luego 0x1d3fc8 (solo
      con la peticion 15): t += 1/60, y = coche + 25 t - 39,2 t^2, desde t > 0,319 la escala baja 0,015 por paso, giro
      +20; si hay otro subiendo y 0x1d77c8 < 6 desaparece. Puerta 0x1d42f0: cruce del segmento del coche (+16C/+174 ->
      +14/+1C) con el de los postes y y + 3,0 x escala > y del coche -> +7C 3 (peticion 25: 1 sin 0x1b3b10); si no,
      cada poste (y + 0,3, radio 0,3 x escala, efecto 0xCD) -> 1 / 2; despues 0x1d4508: giro -0,2 / +0,2 (1 / 2),
      t += 1/60, se cierra en t = 3,0 y parpadea ((int)(resto x 60) & 1) con resto <= 1,0.
    - Control de las peticiones (tabla 0x3ef9c0): 7 / 24 0x1a9290, 8 / 23 0x1a9af8, 9 0x1a9f68, 11 0x1aa3e8, 14
      0x1ab6a0, 15 0x1b0260, 16 / 25 0x1b0570, 17 0x1b0c58, 18 0x1b1948, 19 0x1b11c8, 20 0x1b1f18, 21 0x1b24e0. Peticion 15
      (0x1b0260, puntos 0x3f07c0): recorre 276 objetos, resultado = cogidos x (800000 / p8) - bono; evaluacion
      0x1b03e0: nota 7 -> 700000 si se cogieron todos los de clase 10 del tramo, si no en proporcion, + (cogidos - p8)
      x 0,5 x 10000; corazones del resultado con min(h, 80) x 10000; medidor h x 20 / 7.
    - Alturas y matrices 45DB70 = PS2 0x1b3458 (en el stage clear): clases 10 / 0x22 / 0x39 a la altura del suelo bajo
      (x, 9999,9, z); por la tabla 0x3f08c0, clases 5..9 con el marco de la carretera (0x1b3240) y el desplazamiento
      (0, 0,65, 0) de 0x3f08a8; 11, 14 y 22 con el marco de la carretera. Anchos de las puertas (accion -11 con 16,
      PS2 0x1afa48): tabla 0x3f7218 + tab x 16 {inicio, medio, final, reparto} x carriles de la seccion; media anchura
      3,0 x escala, minimo 2,5 (si no, escala 2/3); cuenta 0x1b00d0(desde, hasta, 11), bifurcacion 0x176e20.
    - Fichero por mision (0x1af428): mision >= 15 tabla 0x3f05c0; si no, tabla 0x3f06a0 por 0x176700(clave) - 15.
    - Modelos (banco 0x57): conos 0x19 / 0x1e / 0x1f (39 vertices, una malla), corazones 0x1c / 0x1d (69), caja 0x30
      (60), puerta 0x2b: malla 1 (250 vertices, transparente) en 4 nodos con banderas 0x860 (cara a la camara y
      animacion de vertices 8) y malla 2 (50 vertices).
    - BLOQUEADO para portarlo sin aproximar (falta portar del original, no se ha inventado nada): la consulta de suelo
      43EB60 / GetYPositionProg (rejilla de 24 m de la COLI y get_road_cond, driving/pc_course_query.cpp) que dan la
      altura de los corazones, la seccion de cada objeto (43F370) y la cuenta 45C8E0; el marco de la carretera 0x1b3240
      (43E570, el tramo de la seccion) y los carriles 43E790; los sprites 2C0000 / 2C0002 / 2C0006 del banco SPRANI
      0x2C (no convertidos); el modo de dibujo 0x1105e0; el cuerpo rigido 516E10 de los conos.
- Coches de la mision: parados (+1C4 = 0) y sin control (47E6C0: +C5C 0x40000 y 45D820 = 7F250C = 0) desde la
  accion -14 hasta la -17 (45D830: la velocidad de la carretera). SIMPLIFICADO: despues van a la velocidad de la
  carretera sin el camino Lead de 472450 (que en la DC los ponia a ~290 km/h por delante del jugador y no se podian
  adelantar).
- Verificado en E (2026-11-09, recorrido real del menu: titulo, UN JUGADOR, INFARTO, coche, cambio, musica; 120 s de
  acelerador): log `MODO: variante 2, dificultad 0, fila de tiempos 1`; PALM: voz 0x82 de salida, anuncio 0xE7,
  0xE8, peticion tipo 2 en la seccion 90 (90..400, p6 15, p8 21), coches de la mision de la tabla 0 abiertos 30..50
  posiciones antes (clase 3 modelos 33, 34, 30, 32...; clase 2 modelo 31), adelantamientos con 0x50 + animo + 0xD5,
  fin con resultado 102 corazones, nota 7, +187; peticion tipo 0 (535..570) nota 1; etapa terminada: 187 corazones,
  nota media 4; CHECKPOINT y etapa 2. El anillo del medidor sale lleno (19) con el resultado de la peticion.

- GOAL verificado en E (2026-11-09, menu real con `-DPRUEBA_PILOTO_MENU -DPRUEBA_SIN_TIEMPO`: conduce el piloto y el
  tiempo no se acaba; solo para llegar a la meta): PALM 237 corazones (nota media 5) -> ALPI 0 -> FORE +20 de bono ->
  SNOW 872 -> IMPE, `INFARTO: GOAL con 1129 corazones`. Observado: la peticion tipo 0 de FORE (mision 8, 575..615) no
  acaba en su etapa (la union empieza antes de la posicion 615 en la DC) y sigue en la siguiente, como haria el codigo
  del original (464350 no la cierra al cambiar de etapa), donde acumula 281 corazones de resultado. COMPROBADO con los
  datos del original: la posicion de carretera de cada poligono de coli_CS_FORE (seccion 5 de COLI0200, la tabla que
  lee 43F370 para car+0x64) llega como mucho a 610 (PALM 590, LAKE 676, ALPI 655, SNOW 680), asi que en el original
  esa peticion tampoco puede acabar en FORE y sigue en la etapa siguiente igual que en la DC.

**COAST 2 COAST** (estudio 2026-11-09; nada de las misiones esta aun en la demo):
- Flujo (PC 4EEC80 / 4EEB50, palabra empaquetada +0x208 del frontal): UN JUGADOR -> COAST 2 COAST -> pantalla de
  categorias (frontend_categories 4E8130..4E8C60: 7 grupos; 0-3 son misiones, 4-6 los campeonatos) -> grupos 0-3: mapa
  de misiones (frontend_missions 4E9160..4E9BB0: 5 / 9 / 11 / 15 categorias, nodo desbloqueado si un vecino tiene nota
  4..6; en el detalle hasta 5 carreras, la i+1 solo si la i tiene nota 4..6) -> coche -> carrera con la variante 6 y el
  par (clave, subclave) de Races.bin (4EEB50 selector 0..3, 4958A0); grupos 4-6: frontend_requests y la variante 5
  (selector 4..6: [7F94C4..] = {1,..,1} SP, preset 1 OR2, {3,..,2} MIX).
- PS2 (SLES-53998): las tablas de posiciones del mapa son las del PC (0x375430 resaltes, 0x375570 nodos, 0x3752e8 /
  0x3753e0 carreras y notas, vecinos 0x3756b0 / 0x375700, de las categorias 0x417a88, cuentas {5,9,11,15} y
  desplazamientos {0,5,14,25} 0x417df8); los tokens del banco 0x44 son OTROS que en el PC (tablas 0x417bc4..0x417df0):
  fondos 0x4400dd / ca / d9 / db, capas 0x4400de / cb / da / dc, tipos 0x4400d3 (cuadros 0,3,2,5,6,4,4), textos
  0x34e..0x352, botones 0x4400d5, pies 0x4400f0 (4..7), nodos 0x4400df, paneles 0x4400d2, carrera elegida 0x4400d4
  (30 cuadros por carrera), notas 0x4400e0 (6..0); categorias: seleccion 0x4400d0 (30 cuadros por grupo, 0x417a28),
  posiciones de los botones 0x417b04 (87/149/212/274, 303 / 365). Codigo: categorias 0x242340 (seleccion) ..
  0x242ac0 (dibujo), misiones 0x243000..0x2443xx. SIN LEER aun el dibujo del PS2 (que token lleva cada boton).
- Races.bin (94 registros de 0x44): +0 clave (categoria 1..40 de RACE_MAPPING_ARRAY), +4 subclave, +0x14 = 3 (4 seria
  sin trafico ni 47CF40), +0x18 curso (registros de 0x78 como csc_data: clave, descriptor de tramo +0x1C, de union
  +0x20, hijo izquierdo / derecho +0x2C / +0x30 con -1 en la rama que no se toma), +0x1C los rivales (47CF40), +0x20
  tipo, +0x28 segundos (tipos 1 y 6), +0x34 / +0x38 / +0x3C umbrales de nota. Tipos (496A30 / 495C20): 0 carrera
  (nota 6 - puesto, 0 si ultimo; 76 de los 94), 1 eliminacion por tiempo (3), 2 derrape contra el rival (7), 3
  conduccion limpia y rapida (5), 6 pillar a los rivales (3). Nota en la licencia +0x1D7 + 5 clave + subclave si
  mejora (495A20 = PS2 0x1f81cc: licencia 0x588668 + 477 + 5 (clave - 1) + subclave - 1, si la nueva es mayor o la
  guardada es 7; +3F4 |= 2). Recompensa del grupo 499730 (= PS2 0x1a0f18..0x1a0f7c: bit de +0x11C, +3F4 |= 2).
- BLOQUEADO para correr las misiones: 76 de los 94 cursos usan etapas que la demo no tiene: las 15 de OutRun 2 SP
  (BEAC, SANF, YOSE, NIAG, SEQU, GRAN, MACH, LASV, ALAS, AMAZ, EAST, MAYA, PRIN, FLOR, NEWY), las etapas al reves
  (*_R) o las variantes PALM_T / BEAC_T, y 68 usan la union 76 (meta en una etapa que no es final, sin convertir). Solo con etapas de
  OutRun 2 hacia delante hay 18 (2-1, 6-1 tipo 6, 8-1, 10-2, 15-1 tipo 2, 19-2, 20-2, 26-2, 26-4, 28-2, 29-1..5, 31-1,
  31-3, 38-2 las 15) y de ellas 6 piden la union 76 (2-1, 8-1, 15-1, 20-2, 29-1..3). Ademas faltan los rivales (47CF40 y los coches de carreras del modulo del trafico) y el arbol de curso por
  mision (main.c solo conoce el de csc_data_2).
- Etapas y cursos de C2C (2026-11-09, `tools/conv_union.py cursos`, `--arboles=`):
  - Las etapas al reves (*_R), PALM_T / BEAC_T (y PALM_BT / BEAC_BT / PALM_BR / BEAC_BR) son etapas propias de STAGE.PS2
    (segmentos 34..65, 1 / 2 / 18 / 19), con su colision, geometria, niebla, sol, SO y descriptor de tramo (+0 = su
    tipo 30..65, +0x98 su union); los 66 descriptores apuntan a ellas (indice 30 PALM_R ... 59 EAST_R, 60 PALM_T, 61
    BEAC_T, 62/63 *_BT, 64/65 *_BR). No es la misma etapa conducida al reves. Los descriptores de union de las etapas
    _R son los mismos que los de la etapa hacia delante (tabla 0x3693b0: union 30 = 0, 40 = 10...).
  - csc_data_cvt.bin = el arbol de OUTRUN2SP (15 etapas de OutRun 2 SP: BEAC u65, SANF, YOSE, NIAG, SEQU, GRAN, MACH,
    LASV, ALAS, AMAZ; metas EAST, MAYA, PRIN, FLOR, NEWY). csc_data_easy es otro (mezcla, uniones 60..75).
  - Union de las misiones (PC race_area_stage_44cf00, estados 7 y 8; special() = modos 3/4, 4957F0, 48B310 o
    495490): con algun hijo, el modelo del +0x10 del descriptor de union (cs_LRBK_PALM, recursos 0x47..0x55) y la
    colision LRBK_PALM (0x5A42F4); sin hijos, el modelo +4 y la colision +0xC. LRBK es la bifurcacion con la franja
    central cambiada (material 5 -> 10, z -290..-440, +2 poligonos): las dos ramas siguen abiertas. Union 76 (la
    ultima de 68 cursos): modelo 0x1E9 cs_BK_SUMOEND (el tronco de la bifurcacion, z 0..-703, con la meta) y colision
    coli_BK_PALM. Rama sin hijo (-1): next_area (44E590) deja el mismo registro (se vuelve a cargar la etapa).
  - Carteles de destino (PS2 0x3f9da0 = PC 5D4FD8, 16 bytes por tipo: recurso BUNKI, nombre izquierda, nombre
    derecha, icono; stage_tables_44ca00): por el +0 del descriptor del hijo (las _R y _T dan el BUNKI de su zona
    hacia delante); hijo -1 = tipo 0x42 = obj_OBJ_BLNK_BUNKI_TX textura 0. CONFIRMADO en el ELF.
  - CURSO.DAT: detras del arbol del modo OutRun (igual que antes) 'CURS' con mas arboles (1 OUTRUN2SP, 100 + i el
    curso del registro i de Races.bin); cada registro lleva su BKnn.DAT (f_bk; las 15 del OutRun conservan su clave,
    las nuevas desde 15) y su colision de union (f_col: COLBK, COLLR, COLBKnn). Hijos como indices del arbol (las
    claves de Races.bin no son consecutivas: 0,1,3,6,10). La demo: `curso_lee_id`, `curso_pedido` (PRUEBA_CURSO).
  - Coste en el GD: 262 uniones distintas (20 BIF, 194 LRBK, 48 metas) en todos los cursos de las misiones a ~1,4 MB
    = 334,5 MiB sueltas; empaquetadas (2026-11-10, tools/pak_union.py: UNIONES.PAK con los trozos distintos y un
    descriptor por BKnn.DAT, recompuesto byte a byte por comun/gd.c) 21,6 MiB. Todas en el disco: datos 854 MiB de 984.
    CURSO.DAT con los arboles 1 y 100..193. Verificado en E (PRUEBA_AUTO hasta GOAL): 144 (INDU_R -> BK135 -> LASV_R),
    128 (MACH -> BK100 -> CLOU) y 142 (GHOS_R -> BK132 -> AMAZ_R); menu real NTSC y PAL hasta la carrera. NO corren
    (RAM, ver DREAMCAST Espacio del GD-ROM): los arboles con SEG46 (125, 177, 187, 192) o con una union con el cielo de
    FLOR_R (BK145, 161, 166, 219, 225, 241, 253).
  - Datos en el disco (2026-11-10): las 47 etapas que faltaban (SEG/COL de los descriptores 15..61), las uniones de
    OUTRUN2SP (BK15..29) y de las misiones 0..7 (BK30..46, COLLR, COLBK25..29); CURSO.DAT con los arboles 1 y 100..107.
    Datos 887 MiB de 984 (despues, con las uniones empaquetadas: todas las misiones, 854 MiB). VRAM (tools/vram.py --zona 1344, todos los arboles): peor LASV + BK22 1286 KB, libres 58 KB.
  - Verificado en E con `-DPRUEBA_AUTO -DPRUEBA_SIN_TIEMPO -DPRUEBA_FIN=900`: arbol 1 (BEAC -> BK15 -> SANF -> BK16),
    100 (BEAC -> LRBK con BLNK a la derecha -> PALM_T -> SUMOEND, GOAL), 105 (PALM_R -> BEAC_R, GOAL) y 107 (ALPI_R ->
    YOSE_R, GOAL). El modo OutRun desde el menu real (variante 1, BK00) antes del cambio de main; tras el reinicio del
    contenedor el arranque llega al titulo y al menu, la navegacion automatica de Flycast no llego a la carrera.
  - Sin hacer: el menu de C2C (mapa de misiones -> curso de Races.bin, variante 6); las reglas de las misiones y los
    rivales. La entrada OUTRUN2SP: ver OUTRUN2SP (2026-11-11).
  - SIN PORTAR: los campos +0x60..+0x68 del descriptor de BEAC (0x1170003..5, objetos del cielo de mas?), el letrero
    de destinos del final de etapa (0x3fa1d0), los nombres largos de las zonas de SP en los avisos (sale el codigo).

**COAST 2 COAST: misiones de carrera y el rival** (2026-11-13):
- Menu (2026-11-13, antes una ventana provisional): UN JUGADOR -> COAST 2 COAST abre las pantallas del original con
  los graficos del banco SPRANI 0x44 (C2C.DAT de `tools/conv_c2c.py`: las 22 escenas 0x4400ca..0x4400f1 que usan, la
  animacion recortada a 43 KB con `conv_sprani.recorta`, 492 KB de VRAM con VQ; las tablas del ELF en el bloque C2C1;
  los glifos de la fuente 14 de sus textos) en el reproductor comun/sprani.c (comun/menu.c, pantalla P_C2C):
  - Categorias (suceso 0x3a; PS2 0x242340 / 0x242680 / 0x242ac0 = 4E8580 / 4E8780 / 4E8C60 del PC): los 7 botones
    0x4400d1 (0x4177e8 abierto en la capa 2, 0x417848 cerrado en la 1, posiciones 0x417b08), el cursor 0x4400d0
    (0x417a28: 30 cuadros por boton, capa 4), la foto 0x4400cf (0x241f60: 4 x ultimo grupo abierto + boton) o la del
    campeonato (0x417968 / 0x4179c8: 0x4400f1), el pie 0x4400ce (0x4178a8 / 0x417908), el texto del grupo (TEXT.PS2
    0x375358, "perfeccionado" 0x375378 con la nota 6; cerrado 0x375338) con la fuente 14 en (65, 210) y el rango
    (0x3753b8: E..AAA) centrado en (520, 363). Abierto (0x242290): los grupos 0 y 4; 1..3 con la nota del grupo
    anterior (0x2420a0) 4..6; 5 y 6 con la del 4 / 5. Flechas por los vecinos 0x417a88; A abierto -> sonido 0x40 y
    al mapa; cerrado -> sonido 3; B -> ELEGIR MODO.
  - Mapa y detalle (suceso 0x3b; PS2 0x242ff0 / 0x2431f0 / 0x243748 / 0x243b88 = 4E8F80 / 4E90E0 / 4E9360 / 4E95A0):
    fondo y encima del grupo (0x417bc8 / 0x417bf8), los nodos de sus 5 / 9 / 11 / 15 categorias (0x417df8, base
    0x417e08; 0x4400df en 0x375570.. por 0x375940) con su nota (0x417da0: 0x4400e0), el cursor 0x4400d8 (0..0x34) en
    0x375430.. (por 0x375930) o en la carrera (0x3753e0) con 0x4400d4 (0x417d60) en el detalle, las carreras de la
    categoria (hasta 5, 0x1f7fe0) con su boton (0x417ca0: 0x4400d5, cuadro 5 cerrada), tipo (0x417c28: 0x4400d3 por
    +0x20 de Races.bin) y nota (0x375408), el nombre 0x4400d6 (categoria + 0x1e en (237, 388)), el panel (0x417d30),
    el pie (0x417cd0) y el rotulo del tipo de la 1.a carrera (0x417c80 -> TEXT.PS2 0x34e..0x352) en el cuadro de texto
    (300, 330, 570, 370; fuente 14, lineas de 14 px, ajuste de linea de 48EB60). Nodo abierto (0x2431f0): el 0 y los
    que tienen un vecino (0x3756b0 / 0x375700 / 0x375790 / 0x375840 por 0x375950) con nota de categoria (0x241ed0)
    distinta de 7, es decir con todas sus carreras jugadas: asi el PS2; el PC (4E90E0) pide ademas nota 4..6 en el
    vecino (manda el de PS2). Carrera i + 1 abierta si la i tiene nota 4..6 (0x243a28; dibujo 0x243fa8). A: mapa ->
    detalle -> la carrera (root+0x208: categoria << 12, carrera << 18; suceso 0xa: coche, cambio, musica y
    MISION.DAT como antes, `-DPRUEBA_MISION=i` sigue igual); B: detalle -> mapa -> categorias; B en el coche vuelve al
    detalle. Las notas son las de la licencia (+0x1DD + 5 categoria + carrera; la clave de la categoria c en
    RaceAssignment.bin es c + 1: comprobado en conv_c2c.py, asi coincide con 495A20 = +0x1D7 + 5 clave + subclave).
  - DESVIACION: solo se corren las misiones de carrera (tipo 0, 76 de 94) cuyo arbol cabe en los huecos
    (`arbol_grande` de curso_huecos); las de los tipos 1, 2, 3 y 6 se ven en el mapa y en el detalle como en el
    original (boton, icono del tipo, nota, rotulo) pero A en ellas da el sonido de bloqueado. Los campeonatos (botones
    4..6, suceso 0x3c, variante 5): ver el punto siguiente.
  - Peticiones: el mapa de los campeonatos (Rev 1.2, 2026-11-20). A en los botones 4..6 abiertos abre la pantalla del
    original (PS2 0x240d68 inicio, 0x240e20 control, 0x241228 dibujo, 0x240bc8 cursor, 0x240948 resultados hechos =
    4EA330 / 4EA3C0 / 4EA610 / 4EA210 / 4EA030 del PC, outrun2-decomp platform/frontend_requests.cpp: el mismo codigo
    con los tokens de PS2 y los textos una posicion antes): fondo 0x4400cc / encima 0x4400cd (cuadro 1 fuera de la
    salida), los 16 nodos (cursor 0x4400d8 en 0x4170b0, vecinos 0x4171c8), el nodo 0x4400df y la nota 0x4400e0 de
    las etapas con resultado (media de sus resultados, 0x240748), las casillas del nodo (0x4172c8 por nodo: 0, 1, 1,
    1, 2, 2, 2, 3 x 4, 4 x 5; 0x4400d5 cuadro 3 hecha / 5 sin hacer, icono 0x4400d3 del curso, nota), el titulo
    0x4400d6 (campeonato / curso / lista), el pie 0x4400f0 (la chica) y el cuadro de texto (0x461 en la salida, 0x462
    con resultado, 0x380 sin el). Detalle (A en un nodo con resultado): el cursor en la casilla, la imagen de su
    lista (0x4400d4) y las peticiones de REQUEST_NORMAL / SPECIAL1..3 del guion del curso (Req_*.bin, registros de
    0x74; el rotulo de cada una por su tipo y subtipo, 0x240a08, textos 0x353..). Resultados: los cuartetos de la
    licencia +0x36E + 2 tipo de curso (0x2406b0). Datos: bloque REQ1 de C2C.DAT (tools/conv_c2c.py, con las
    direcciones comprobadas en el codigo). Verificado en Flycast: licencia nueva (Clarissa abierta, mapa, 15 nodos,
    textos, B a las categorias, el mapa de Flagman igual que antes) y con `-DPRUEBA_NOTAS_CAMP` (nodos con nota,
    casillas con icono y nota, detalle con las listas SOLICITUD / SOLICITUD ESPECIAL 1..3 y sus peticiones).
    PENDIENTE (lo que falta para correrlos): el gestor de peticiones de la variante 5 (PC 7F9460, evento 0x190; PS2
    0x22xxxx) no esta portado ni tiene fuente en la decompilacion del PC (alli corre el x86 traducido), asi que A en
    la salida o en una casilla da el sonido de bloqueado en vez de pasar al coche. La acompanante de la variante 5
    ya se elige como 487F50 (`robot_pareja` con `menu_campeonato`: 46C520 = la quinta palabra de 0x2a6b08: grupo 4
    -> CHR_DR_G00, 5 -> CHR_DR_L00, 6 -> CHR_DR_H00); sin carrera aun no se ve.
  - PROBABLE: el salto de linea de los textos de las categorias ('\n' en TEXT.PS2) baja el alto de la celda (17) como
    42C610 del PC (el printf de PS2 0x117640 no se ha seguido); el orden entre el texto y las capas de sprites (el texto
    va encima de todas). Sin el fundido de entrada / salida de los recursos (las escenas aparecen y se van de golpe).
  - Verificado en Flycast (pantalla 98, `MODO_JUEGO=0 C2C_RECORRE=1 nav.sh`, licencia nueva): categorias con Flagman 1
    abierto y su texto, los demas en negro, Clarissa con "???" y "Clarissa Ranking A para desbloquear!" (A: bloqueado),
    mapa de Flagman 1 con el nodo 1 abierto y el cursor, derecha sin moverse (nodo 2 cerrado), detalle con la 1-1 y
    la 2 y la 3 cerradas ("?"), B / B y de nuevo A / A, la 1-1 -> coche, cambio, musica -> `MODO: variante 6` y la
    carrera con sus 5 rivales; tablas de C2C.DAT 61232 bytes en el hueco, VRAM libre al abrirlo 486 KB; sin "Out of
    memory".
    Con notas (`-DPRUEBA_NOTAS_C2C`: notas en las categorias 0..4 de la licencia nueva): rango A de Flagman 1 en
    POSICION MEDIA, Flagman 2 abierto y la foto 4 (la del mapa de Flagman 2), los 5 nodos abiertos con su nota (AAA, A,
    B, A, A), derecha al nodo 2 y su detalle con las dos carreras (la 2.a abierta por el 4 de la 1.a; A y AA).
  - VRAM: al entrar en P_C2C y al salir, la VRAM del menu y el coche 3D fuera y lo pendiente liberado (como antes del
    frontal de recreativa): sin ello las escenas del banco 0x44 quedaban repartidas por los huecos y, al salir, el
    coche, el cambio y la musica no encontraban sitio seguido ("pvr_mem: ERROR -- out of PVR memory", revision 41).
    Flycast (`MODO_JUEGO=0 C2C_RECORRE=1 nav.sh`, con B del coche al detalle y otra vez A): sin errores de pvr_mem;
    VRAM libre en la pantalla de carga 1618 KB, la misma que por OUTRUN (`MODO_JUEGO=1`); en el coche 936 KB (578 KB
    por OUTRUN, que guarda las pantallas del menu ya subidas).
  - RAM: las tablas de C2C.DAT (~60 KB) van en el final del hueco de la union prestado (`menu_hueco` = hueco_fsp de
    main, como el frontal de recreativa) solo mientras se ve P_C2C; su VRAM se suelta al salir (spr_cierra).
- Carrera: variante 6 (menu_modo), el arbol 100 + i de CURSO.DAT (4965A0), los registros de 47CF40 (PS2 0x1ef900) con
  el registro +0x1C de Races.bin y las tablas del ELF (listas de modelos 0x36be70, familias 0x40b818, clases de los
  especiales 0x40b858 con la 0x1E -> 0x1B de 0x1d9ae8, colores 0x40b618 por familia, perfil 0x36bd00, c3c 0x36bcf8;
  constantes 17,99198 / 295 / 1,0431494 / 60 / 3612 / 1/65536 de 0x1ef900) y el largo del curso de 0x1724e0
  (descriptor +0x96 de PS2: PALM 590, uniones 241; mision 1-1 = 1436 posiciones). La posicion de carretera del curso
  entero (car+0x260) en la demo: la base de cada etapa del arbol mas (s - principio) / m_pos (rc_pos_curso).
- Coches de carreras (comun/rival.c y la parte "coches de carreras" de demo/main.c): 47CA40 (PS2 0x1ef230) y 47BC90
  (0x1ecef8): sin coche avanzan +40 += +44 x ritmo x arranque (escala 2, perfil +0x10 / +0x14, arranque cuadros / 1505;
  0x3f3cffe8, 0x44bc2000); entre -10 y +60 posiciones del jugador abren un coche del trafico (cada 0x5A cuadros por
  delante y por detras; los de la parrilla al montar, 47D8B0, en los carriles 2 / 3 parados con marcha 1) hasta c1c a la
  vez; el coche lo conduce 47A7C0 (PS2 0x1e5dc0: velocidad de su clase x +B78 x escala x ritmo + vaiven coseno, curva
  {radio, km/h} de 0x40bb68 con +BB4 (479830, PS2 0x1df668: puntos a v x 15 posiciones) o +B54 (479670), rebufo del
  jugador x 0,7 x su +1C4, minima +0xC, tope del modelo ((134C - 2,3) x -131,39 + 318) x 1,025 / (2438 x 216,72), x 0,05
  o d / 3 justo delante del jugador), la aceleracion de 472090 (PS2 0x1e43a8) con la tabla 0x40bc98 y por marcha (+208,
  0x40bcb0 x 0,002 x ritmo x +BCC) y el cambio automatico 502420 con los umbrales de su modelo (487570); se cierra a
  -15 / +70 posiciones (477F60) y el registro sigue solo (4763D0, 476480 / 4764D0). Clasificacion 477510 y nota 495C20
  (6 - puesto, 0 el ultimo), guardada al salir de la meta (49CFD0 -> 495A20); el fin del tiempo no la guarda.
- Modelo: el del original para los coches de carreras, el registro de 4866C0 del modelo 0..29 (obj_rc_<coche>, 5 LOD,
  1215-2155 vertices en el LOD 0, 22-30 KB de texturas VQ): RCmm.DAT con `conv_trafico.py --rivales=0,..,29` (formato
  de TRAFn.DAT; 'TCX9': los 8 colores de car+0x12 por clave de la tabla 0x36bc18). No se usan COCHE00-14: un segundo
  coche de jugador (10-11 mil vertices, 424 KB de bloque) por cada coche de carreras no cabria en el TA ni en la RAM.
  RAM: hasta RC_NMOD = 6 modelos distintos por mision (~60 KB cada uno en el monton, texeles en la VRAM, leidos en el
  hueco de la union prestado al empezar la carrera); 18 de las 94 misiones tienen mas (hasta 14): DESVIACION, los de los
  modelos que no caben se dibujan con el primero. Medido (1-1, dos modelos): RAM libre en la carrera 1301 KB (886 KB
  seguidos en la cima), VRAM libre 1323 KB antes de la zona del curso.
- Verificado en Flycast (pantalla 98): `-DPRUEBA_AUTO -DPRUEBA_MISION=0 -DPRUEBA_SIN_TIEMPO -DDIAG_MS`: los 5 coches
  de la 1-1 abren en la parrilla (+3..+23 posiciones), arrancan en GO con sus marchas, el piloto los adelanta (puesto
  6/6 -> 3/6) y alguno le vuelve a pasar; 43,9-45,7 cuadros por segundo, sin "Out of memory". Menu real
  (`-DPRUEBA_PILOTO_MENU -DPRUEBA_SIN_TIEMPO`): UN JUGADOR -> COAST 2 COAST -> ventana (hoy: categorias, mapa y detalle) -> 1-1 -> coche, cambio, musica ->
  `MODO: variante 6` -> la carrera entera hasta la META de PALM_T (media 44,7 cuadros por segundo) -> nota en la licencia
  (`LICENCIA: mision 1-1 nota ...`) -> titulo; RAM al volver 1086 KB libres (1088 al arrancar: los modelos RCmm sueltos).
- HUD de la mision (C2CHUD.DAT de `conv_hud.py --c2c`; comprobado en el ELF): NAVI_PUB 0x1bd4ac con la variante 6
  (0x1bd6f4, el 4BED13 del PC) dibuja en el modo 16 el panel 2C0046 en lugar de 2C0028 / 2C0029 (sin el crono 0x1bfa58
  ni la puntuacion), el avance 0x1bfd78 (icono del curso 2C0104 / 2C0105 / 2C0106 / 2D0001 / 2D0002 por la
  profundidad del arbol, tabla de saltos 0x404580; marcas 2C0107 del coche de 47DA80 y 2C0108 del jugador, cuadro =
  posicion / largo x 180, tabla 0x404578) y en los tipos 0 y 1 el puesto 0x1c02d8 (escena 2B0061 y las imagenes de
  42D280: cifras 0x404b30 del banco 0x2B, sufijo 0x404b58 (idioma 4: 0 el primero, 3 los demas, y 0x38), total
  0x404b08 de spr_etc y la barra 3000F; desplazamientos 0x369888 / 0x369878). demo: hud.c con `mision` y c2c_dibuja.
  Fuera el texto "PUESTO n/N". Flycast: "Posicion 6o/6" .. "3o/6", las marcas TU / rival en la barra Inicio-Meta.
- La puntuacion de OutRun no cuenta en las misiones: 4F2DF0 (PS2 0x28f8c8) sale si 0x1f7d80 (4957F0: mision elegida)
  y 4F2B20 no da el bonus de etapa (demo: pt_paso / pt_bonus_etapa con la variante 6).
- Rotulos del principio (496A30, PS2 0x1f8d0c, modo 16): con la cuenta 0x168 los elementos 2B0020 (capa 9) y 2B003D
  (capa 10), modo 3, en (170, 20) con escala (1, 4) hacia (170, -80) y (1, 1) en 0,2 s (465310 / 4653C0, paso de
  449E30: 12 cuadros; matriz de 4656B0 = hudd_escala); en 0xB4 la voz del tipo (0x40c6b8: 0x1E7, 0x219, 0x1E8,
  0x1EA, 0x218) y 2B0020 en modo 5 con el rotulo del tipo (0x36f3f8: 2B0065, 2C00FD, 2B0067, 2B0066, 2C00FC); la
  cuenta baja 2 por cuadro. Flycast: el bocadillo "Objetivo: Compite con tus rivales!" al GO. Voces: VOCESC.DAT
  (conv_sonido.py) con los bancos que carga el PS2 en la variante 6 (CONFIRMADO, 0x1b7fa4: 11ED40(0x89A0) PERR.pak y
  11ED40(0x869B) TRS.PAK de la tabla 0x352840, en lugar de PERO / ORC; en la meta 0x1b8e70: 0x889C TRE.PAK): TRS y TRE;
  PERR.pak SIN PORTAR (no tiene las tablas que lee conv_sonido.banco_tablas). La cola de 45BD30 se toma inmediata.
- Resultado de la mision (como el original: no hay otra pantalla): la meta (19) guarda la nota (495A20) y pasa al
  modo 34 (OutRun Miles; 4EF550 con la variante 6 y 44FF10: millas = 84B900[nota] - 84B900[nota de antes] >= 0, con
  84B900 = SETUP de OutrunMiles.bin: 100, 200, 400, 600, 800, 900, 1000, 0 para las notas 0..7) y al 35 con los tokens
  de la mision (0x1bc650: 2B0068, o 2B0069 si (tipo 1 o 495B80) y nota 4..6; despues 2B004F si la licencia ya tiene la
  mision superada (4959E0) con A = 495A60 (la subclave siguiente si la nota llega a +0x24 = 4) y REINTENTAR, el boton
  0x10 la misma y B al menu; si no 2B004A); FINC2C.DAT (conv_fin.py). El fin del tiempo: 34 y 35 sin nota. Fuera la
  vuelta directa al menu.
- HECHO (2026-11-27, ver COAST 2 COAST: misiones de tipo 1, 2, 3 y 6): los tipos 1, 2, 3 y 6 y los vaivenes. Lo de antes: SIN PORTAR los tipos 1, 2, 3 y 6 (los de su juego) (puntos 4BD230 / 4BD340, marcas
  495E90, notas por umbrales) no estan en la lista. Tambien 472E60 / 474080 / los vaivenes, la espera de 474D30, 43EB60
  al abrir, el bit 0 de +0xF, el boton 0x10 del PS2 (la X de la Dreamcast, PROBABLE), los campeonatos y la variante 9.

**COAST 2 COAST: glitches en las misiones (revision 42; queja de la revision 41: "edificios que se quedan en medio de la
carretera, poligonos que se cruzan")**:
- Barrido en Flycast (`-DPRUEBA_AUTO -DPRUEBA_MISION=i -DPRUEBA_SIN_TIEMPO -DDIAG_MS`, capturas cada 1-4 s con la s de la
  ruta): misiones 0 (1-1, BEAC -> LRBK -> PALM_T -> SUMOEND), 6 (LAKE_R -> SANF_R), 64 (PALM -> LAKE -> ALPI, las
  etapas del modo OutRun con uniones LRBK) y el arbol 1 de OUTRUN2SP (BEAC -> BK15) en el mismo sitio.
- Causa 1 (la de la queja): con la camara en la union la demo dibujaba la etapa siguiente ENTERA (pvs_paso no usa la
  PVS con el ojo a mas de PVS_FIN de una punta de la ruta). Las etapas que en el modo OutRun nunca van detras de una
  union (PALM_T, BEAC_T, las de salida) traen lo que hay detras de su parrilla: en la 1-1, PALM_T sacaba sobre la
  union de BEAC su monte y su mar (nodos de 700-800 m de radio, cruzados con los muros y la hierba de la union) y un
  edificio de 83 m pegado al borde derecho de la rama izquierda (x -1164..-1134, z -858..-812 de la union), que con la
  curva se ve en medio de la carretera. El original no dibuja eso: race_area_geometry_44de10 (PS2 0x172780) dibuja
  la etapa con la lista de nodos de una seccion; con el coche en la union (car+0x5C != 0) la seccion 0 de la etapa
  nueva si ya esta cargada ([7D3064] = 1, estado 0xB de race_area_control_44e590) y si no la ultima de la que se deja
  (43D470(0)). Las listas (cs_CS_<zona>_bin, u16 n y n pares (byte, mascara) por seccion) estan descifradas
  (SLES-53998 0x1112f8): mascara de nodos por recurso y registro (256 bytes en 0x4ce7a8 + registro x 0x110); para la
  seccion s se aplican las listas 0..s en orden y cada par ESCRIBE el byte i de la mascara (sb; al volver atras empieza
  de cero); 0x1112f8 pasa la mascara a una lista de u16 (nodo = 8 i + bit, -1 al final, en 0x70001000) que dibuja
  0x1114e0. (Antes se leian como conjuntos de nodos por seccion o diferencias por XOR y no cuadraban: son bytes que
  cambian.) En PALM la mascara de cada seccion lleva el 77 % de los nodos de menos de 40 m de radio que hay en los
  120 m de delante y casi nada de lo que queda 150 m detras.
  Arreglo: `tools/conv_viso.py` -> VISO.DAT (27 KB, las 62 etapas): por etapa la mascara de la seccion 0 y la de la
  ultima, por nodo del ORD7 (orden de modelo.Modelo.bloque, comprobado esfera a esfera; los objetos de la etapa, que
  el original dibuja aparte, siempre). demo/main.c `viso_lee` (las de las etapas del arbol, al leerlo) y `pvs_paso`:
  con el ojo antes del principio de la etapa (la siguiente, en la union) o pasado su final (la que se deja) solo esos
  nodos (`mo->viso`, en nodos_visibles); dentro de la etapa sigue la PVS. Vale para todos los modos (en el OutRun las
  etapas siguientes casi no tienen nada detras de la salida: no cambia lo que se veia).
  PALM_T en la seccion 0: fuera el monte y el mar de detras (ORD7 141 / 142) y el edificio 1137; quedan los dos
  edificios de 21 y 28 m a la derecha de la carretera (175, 1115), que el original tambien dibuja.
  Verificado en Flycast (1-1 con `-DPRUEBA_FIN=1500 -DPRUEBA_RAMA=0`, capturas en las mismas s de la ruta: 4900..6700):
  antes, en s 6300-6500, el monte de PALM cruzado con el muro izquierdo y un edificio oscuro sobre la carretera; despues
  la union limpia y los dos edificios a la derecha. Cuadros por segundo en la union con PALM_T ya leida: 31-33 antes
  (dibujaba toda la etapa), 44-45 despues; sin "Out of memory".
- Causa 2 (mision, no de la queja pero se ve en cada union): los objetos animados de la union (BO) se elegian con la
  fila de la variante 1 y la logica del modo OutRun: en las misiones salia el dragon (conjunto 11) pasando por delante
  de la camara (el cielo marron de las capturas). 0x174a48 con la variante 6 va por la rama de las variantes >= 5
  (0x174cd0) con la fila 6 de 0x35a738 = {9 barco, 0 globo, 5 avion de linea, 4 alas delta, 10 ovni, 8 avioneta, 3
  caza}: choque fuerte -> [0]; muro o fuera del asfalto >= 10 % -> [1]; fuera del asfalto -> [2]; contra otro coche ->
  [3]; roce (7D3084) >= 0x3C -> [4]; 7D30A4 < 0x12C -> [2]; roce -> [5]; si no [6]. 7D30A4 (PC 44EDE0, al final):
  cuadros con +E68 > 0,18 y |+162| < 0x5DC (el rebufo del HUD). demo: `bo_elige`, `bo_ca4`; conv_bo.py convierte los
  14 conjuntos (filas 1 y 6) y deja la fila 6 al final de BO.DAT ('VAR6', la demo de antes no la lee); BO00, 03, 04,
  05, 08, 09, 10 nuevos. DESVIACION: el caza (BO03, grupos 15 y 16) sin animacion de vertices: sus cuadros no casan
  malla a malla con el 0 (compacta_cuadros) y se queda el cuadro 0.
- Visto y SIN CAMBIAR (no son de las misiones): al final de BEAC (las misiones de BEAC y OUTRUN2SP igual) un edificio
  sin la parte de abajo en el cielo, sobre la carretera, a 200 m: nodos 451 / 453 del registro 3 (y 23..109 m), que
  la mascara del original tambien lleva en esas secciones; lo de debajo lo quitan la PVS o el reparto del TA (sin
  medir). Tomar la rama sin hijo (-1) de una LRBK vuelve a cargar la misma etapa como el original, pero la posicion
  del curso de la demo (rc_pos_curso) vuelve a la base de esa etapa: el puesto pasa de 1o a 6o (PENDIENTE). SIN
  PORTAR: los objetos 0x570018 / 0x570042 de obj_course_obj_common (x +-9, z -403, en la franja central de la LRBK) que
  0x172ba0 dibuja en las misiones segun la rama (451350).

**CONTRARRELOJ** verificado en E: `MODO: variante 7, dificultad 0, fila de tiempos 2`, sin un solo coche de trafico,
union y ruta leidas como en el OutRun. GOAL (menu real con el piloto y `-DPRUEBA_SIN_TIEMPO`, 2026-11-09): PALM -> ALPI
(+3000 cuadros) -> FORE (+3060) -> SNOW (+3270) -> IMPE (+3240) -> GOAL, sin trafico en todo el curso. SIN PORTAR: la eleccion de etapa / curso por codigo (48B550: una etapa con su meta GOAL_*_TA o el
curso entero), el fantasma (GHOST.PS2 gc_default_*, formato QHOT/NHOT, modulo 465F30..4687C0) y su record y la
pantalla de resultado 0x50 del modo 25.

## SALON (showroom: la tienda de las millas OutRun, 2026-11-13)

UN JUGADOR -> SALON abre el salon del original: se gastan las millas OutRun de la licencia (+0x24, float; las suma
4EF510 = PS2 0x1f41d8 al salir del modo 34) en desbloquear objetos. Codigo: comun/menu.c (pantalla P_SALON, `sa_*`),
datos: SALON.DAT (tools/conv_salon.py, paso `salon` de tools/regenera.py).
- Original: SLES-53998 0x25b338..0x25db20 (vtable 0x41adec: destructor 0x25b338, init 0x25b418, control 0x25b5d0,
  dibujo 0x25b8c0, suspender 0x25b998) = 4D4230.. del PC (outrun2-decomp platform/frontend_showroom.cpp). Misma
  estructura que el PC con otros tokens del banco 0x44 y otros desplazamientos (+0x40c estado, +0xc84 base de la
  pagina, +0xbdc comprado, +0xc80 refresco, +0xc88 / +0xc8c objeto y vuelta de la ventana, +0x2a74 / +0x2080 se
  puede / eleccion, +0xb45 tema). Tabla de filas 0x41ae20 (257 {token, primero, ultimo}; PC 5CAE00 tiene 252: las
  252..256 del PS2 son los iconos de la ventana, 0x44005c cuadros 0, 1, 2, 4, 6).
- Paginas (lista 0x24d430 en (-180, -50), 19 por fila, capas 1 / 2, 7 visibles con desplazamiento; tira de imagenes
  0x238798: objeto i en el cuadro paso x i + base, entra de paso x i + 1, sale hasta paso x (i + 1); titulo 0x4400bb /
  0x4400ba): principal (filas 0x28..0x2c: FERRARIS, ETAPAS, METAS, MUSICA DE FONDO, COLORES DEL COCHE; tabla 0x41ad10
  -> estados 3, 6, 15, 9, 12; tira 0x44007d), FERRARIS (28 registros de 0x41bda0 {modelo, bit}: bits de la seleccion
  de coche, 0x3759E0, comprobado en conv_salon.py; tira 0x44007e), ETAPAS (objetos 0x8a + fila, 34; 0x44007f),
  METAS (0xc1 + fila, 14; 0x4400c5), MUSICA DE FONDO (0xac + fila = el bit 172 + tema - 7 que lee la seleccion de
  musica; suena el avance de la pista 0x375dcc[i] + 0x21; 0x4400a9), COLORES DEL COCHE (0x1c + fila, 110; el coche del
  registro 0x41ba30[fila] con su color; 0x440080).
- Precio: 0x1f4198 (0x629ee0, que rellena 0x1f3c90: 10 x i y los pares de PRICES de \Scripts\bin\OutrunMiles.bin;
  p. ej. F50 7500, tema 7 2500), "%dM" (0x41ad00) con la fuente 14 en (480, 368) si el objeto no es suyo (blanco: el
  0x26ce50(-1) del dibujo; sale claro sobre el fondo, igual que en el codigo); millas "%dM" en (480, 87). Icono de
  comprado 0x440051 (filas 7..15).
- Compra (0x25d560 / 0x25d878 / 0x25da80): ventana "COMPRAR OBJETO" (0x3a4) en (330, 180, 239, 130) e icono de la
  fila 252 (SI) / 253 (NO) con izquierda / derecha; sin millas: fila 255 y sonido 3; los objetos de 0x41ad38 (26: 4,
  11, 19, 26, 169, 171, 189..192 y colores) no se venden en esta version: fila 256. A con SI: 0x1a1ba0 (sin el bit y
  con las millas truncadas >= precio: bit de la licencia +0x28 y millas - precio), +3F4 |= 2 y al cerrar 0x105f50
  guarda (aqui vmu_guarda). El coche 3D se quita mientras esta la ventana (28e750).
- Coche 3D en FERRARIS y COLORES (28e348 / 28e788): el del registro con su color (0x18a110: 0x3d4b80 por modelo) por
  el dibujo del coche de la seleccion (menu_coche3d / menu_modelo / menu_color).
- VRAM / RAM: como COAST 2 COAST, la VRAM del menu y el coche fuera al entrar y al salir (menu_libera_todo); tablas de
  SALON.DAT (287 KB: animacion recortada a las 18 escenas, 257 KB) en el hueco de la union (menu_hueco). Las tiras son
  escenas de hasta 44 texturas (ETAPAS: 1 MB con VQ): SALON.DAT va por tramos (comun/sprani.c, spr_por_tramos): al
  crear una instancia solo se suben las texturas de los cuadros de su tramo y, si no caben, se echan las de las
  escenas del banco que no usa nadie.
- Ventana: las imagenes 0x30040..0x3004f son del banco 3 = spr_etc_xst (0x117b88 = 42D300; las mismas que lee
  conv_hud para el HUD de C2C), dibujadas como 0x271ca8 (= 48C5F0 del PC, con otros indices: cuerpo 0x3004a, franja
  0x30049, cabecera 0x3004f + esquinas 0x3004e / 0x3004d, pie 0x30043 + 0x30042 / 0x30041, lados 0x3004c / 0x3004b);
  titulo con la fuente 14 en blanco en (x + 6, y + 5) (0x271900). Los modos 1 / 2 / 3 de 0x117b88 se dibujan estirando
  la imagen (PROBABLE: el detalle de 0x117b88 sin seguir; son franjas de un color).
- X (LICENCIA): la raiz (0x23c150) abre la licencia (pantalla 0x15) cuando la pantalla de encima lo permite (+0x2a4 de
  su nivel: el salon pone 1 en cada paso con 0x23bb20(raiz, 0, 1)); al volver el salon empieza de nuevo (0x25b418). Igual
  en la DC (a_licencia con vuelta al salon).
- Musica: 0x25bf28 la para al salir de la pagina principal (al entrar en una pagina y al salir del salon) y la de
  musica la para al salir; nadie la vuelve a poner en UN JUGADOR (solo el arranque de la raiz, 0x23b5b8, y la vuelta de
  la seleccion de musica, 0x24efec, ponen la pista 0x1e): tras el salon los menus quedan sin musica, como el original.
- Repeticion: 0x284e20 (= 48F4F0): la palanca analogica izquierda marca al pasar de 64 y repite cada 21 pasos
  mientras se mantiene; la cruceta da solo la pulsacion (0x101bd4), sin repeticion. Portado (joyx / joyy de la DC).
  No probado en Flycast (el teclado de las pruebas es la cruceta).
- BLOQUEADO / SIN PORTAR:
  - Camara del coche: 0x1788c8(1) pone [0x35a9f8] = 1, el preset de 0x3d2bf0 / 0x3d2c08 / 0x3d2c20 (ojo (-3, 7,15, 7),
    giro x -0,192, fov 35 grados; el 0, la seleccion de coche: (-1,55, 5,75, 2,75)) = 5BA9xx del PC (pc_camera.cpp,
    482E70). Falta donde coloca el frontal el coche (suceso 0x181 de 28e6d8 / 19c870): sin eso el ojo no se puede poner
    en la escena del port; se usa la camara de la seleccion de coche (que tampoco es la del original).
  - Coche bloqueado: 28e348 guarda el desbloqueo en +0x90 y 28e6d8 crea el coche con 19cfd8(modelo, color,
    bloqueado), que pone el bit 0x40 en las banderas del coche (0x4081 | 0x40). Solo afecta al primer registro al
    entrar en la pagina (28e788 pasa siempre 1 al moverse). Que hace ese bit al dibujar no esta seguido: el coche sale
    normal.
  - ETAPAS (bits 0x8a..0xab) y METAS (0xc1..0xce): los leen los tableros de cursos de la CONTRARRELOJ (y el 0x24 de
    OUTRUN, 0xcb / 0xcc): portados el 2026-11-13, ver Tableros de cursos. Lo comprado en el salon ya se puede elegir.
  - COLORES (bits 0x1c..0x89): los lee la seleccion de coche (0x245038, tabla 0x375A20, 110 colores): Y pasa por los
    colores con su bit; ya funciona con el arreglo de MNU2 de abajo.
- PROVISIONAL: la barra de ayuda (0x23bd70: ELEGIR, LICENCIA, ATRAS) con los botones de la Dreamcast compuesta en
  SALON.DAT (DESVIACION de la revision 42 de las barras).
- Arreglo de paso (seleccion de coche): comun/menu.c leia las tablas de coches de MNU2 dos bytes corridas (el bit de
  desbloqueo en +32 detras de los 30 modelos u8; conv_menu.mnu2 lo escribe en +30): cada ficha tomaba el bit, los bits
  de los colores y el color inicial de la ficha siguiente (p. ej. el F50, ficha 2, pedia el bit 1 en vez del 0: el F50
  comprado en el salon seguia bloqueado). Ahora en +30; MENU.DAT no cambia. Con ello el color inicial de cada ficha
  (0x375C00) coincide con el del modelo de 0x3d4b80 que usa el salon (Dino 2, F50 1). Esto corrige tambien la
  verificacion de Colores de los coches (2026-11-10), hecha con la tabla corrida una ficha.
- Prueba: `-DPRUEBA_SALON_MILLAS=n` (la licencia activa, si no tiene millas, con n al entrar en el salon; otro nombre
  que el PRUEBA_MILLAS de la demo de atraccion) y tools/prueba/salon.sh.
- Verificado en E (Flycast, `-DPRUEBA_SALON_MILLAS=20000`, tools/prueba/salon.sh): pagina principal con 20000M, FERRARIS
  con el F50 3D y su precio, ventana "COMPRAR OBJETO" (sin el coche), A con SI: "comprado el objeto 0 por 7500 millas
  (quedan 12500)", guardado en la VMU y "Ya tienes este objeto / VEND."; MUSICA DE FONDO (suena el avance 40) y compra
  del objeto 172 (2500); fuera del salon, la seleccion de coche deja elegir el F50 (antes del arreglo de MNU2 no) y la
  musica llega al tema 7. Segundo arranque con la misma VMU: el salon con 10000M, el F50 "Ya tienes este objeto" y
  elegible en la seleccion de coche. VRAM libre al abrir el salon 385-419 KB y con el coche 1170 KB; sin "Out of
  memory" ni "pvr_mem: ERROR". No probado en R.
  Segunda pasada (ventana con las imagenes 0x3004x, X, musica, `-DPRUEBA_SALON_MILLAS=20000`): la ventana de la compra
  con su cabecera roja, X en el salon abre ELEGIR CARNET y B vuelve a la pagina principal del salon; los mismos
  desbloqueos tras reiniciar; sin "Out of memory" ni "pvr_mem: ERROR".

### Articulo OUTRUNNERS (2026-11-16; el del taxi, retirado en la Rev 1.1, 2026-11-20)

- METAS vende OUTRUNNERS (objeto 0xcf) por 50.000 millas; la compra es la normal de `sa_compra` (pone el bit de la
  licencia +0x28, resta las millas, +0x3F4 |= 2 y guarda en la VMU). El nombre `ENTIRETY` de la licencia tambien lo
  desbloquea.
- El articulo TAXI CRAZY TAXI (objeto 0xd0), la ficha del taxi (que ocupaba la del F50: ficha 7, modelo 0), su turbo
  y su piloto se han quitado: la ficha 7 vuelve a ser el F50 del original, con su bit de la licencia, sus colores y
  su columna de PARAM.DAT. SALON.DAT conserva el sitio de la fila 0xfd y del precio 0xd0 (sin uso) para no cambiar
  el formato.

## Tableros de cursos de INFARTO y CONTRARRELOJ (2026-11-13)

UN JUGADOR -> INFARTO / CONTRARRELOJ ya no van directos al coche: pasan por los tableros de cursos del original, que
leen los desbloqueos ETAPAS y METAS del SALON. Codigo: comun/menu.c (pantalla P_TABLERO, `tb_*`; menu_ta_codigo,
menu_ha_juego), demo/main.c (el arbol de la carrera); datos: TABLERO.DAT (tools/conv_tablero.py, paso `tablero` de
tools/regenera.py), CURSOTA.DAT y las uniones BK262..BK469 en UNIONESTA.PAK (conv_union.py --curso=CURSOTA.DAT
--bk-desde=262, pasos `unionta*`).
- Original (SLES-53998): la seleccion de un jugador (0x25dd58) escribe el modo en los bits 5..7 de la palabra del
  frontal (raiz +0x208: C2C 0, OUTRUN 0x20, INFARTO 0x40, CONTRARRELOJ 0x80, SALON 0xc0) y va a la clave de la tabla
  0x375f58 (0x3a, 0x24, 0x24, 0x24, 0x2b). Fabricas de la raiz por clave (0x239eb8): 0x24 = 0x2528e8 (vtable 0x419f3c),
  0x25 = 0x2558e8 (0x41a24c), 0x26 = 0x24f340 (0x419a3c), 0x27 = 0x257e58 (0x41a51c); son la familia de 4D05E0 del PC
  (platform/frontend_rankings.cpp: claves 36..38), con otros desplazamientos y la clave 0x27 (las etapas) que el PC no
  tiene. Botones (0x2848f0 = 48F5F0): 0 A, 1 B, 3 / 5 izquierda / derecha, 4 abajo, 11 el gatillo R (bit 0x2000).
- 0x24, juego de cursos (init 0x252b50, control 0x2530a0): carrusel 0x419f70 (0x44003b, 2 posiciones: OutRun 2 /
  OutRun 2 SP; OUTRUN 0x419fb0 = 0x4400a5 con 4, los cursos seguidos con los bits 0xcc / 0xcb), posicion = bits 0..1.
  A: INFARTO -> el coche (clave 0xa) con los bits 0..1 = posicion; CONTRARRELOJ -> 0x25 con +0x208 |= 0x800 (0x2539c4).
- 0x25, CURSO / ETAPA (0x255e58): carrusel 0x41a280 (0x44003c) o 0x41a2c0 (0x4400a2) por el juego; A: 0x375e48[i]
  (0x26 con 0x800, 0x27 sin el; 0x256520).
- 0x26, metas (0x24f818): carrusel 0x251da0 por juego y R (+0x3f4 = 0x375e30): 0x419a70 / 0x419b10 (0x44003d /
  0x4400bf) o 0x419bb0 / 0x419c50 (0x440043 / 0x4400be); 6 posiciones: META A..E y "15 ETAPAS SEGUIDAS" (la derecha se
  para en la 5, 0x250d44). A (0x2502cc): bits 12..17 = posicion (+6 con R) y 0x800; candado (sonido 3): la 5 con 0xcc /
  0xce (juego 0, R) o 0xcb / 0xcd; las del reves 0xc2 + 2 i (juego 0) o 0xc1 + 2 i. R (0x250288) cambia de tabla en la
  misma posicion. B: 0x375e30 = 0 y los bits 12..17 a 0.
- 0x27, etapas (0x258610): carrusel 0x2581a0 por juego y R (+0x538 = 0x375f50): 0x41a550 / 0x41a718 (0x440036 /
  0x4400bd) o 0x41a8e0 / 0x41aa58 (0x440042 / 0x4400bc); 15 posiciones. A (0x259198): bits 12..17 = posicion (+15 con
  R; se escriben antes del candado, como el original), +0x20c bit 0 = opcion B; candado solo con R: 0x8b + 2 i (juego
  0) o 0x8a + 2 i, sonido 3. Abajo en la etapa 0 (0x259ea0): la opcion B (PALM_BT / BEAC_BT, del reves _BR) si esta el
  bit 0xa9 / 0xab (juego 0, R) o 0xa8 / 0xaa: animacion +0x348 (0x41a6c8 / 0x41a6f0 / 0x41a890 / 0x41a8b8: 0..0xf modo
  5 al ponerla, 0xf..0x1e modo 3 al quitarla, capa 1) y aviso 0x4400e4 (0x41abd0) cuando se puede.
- Comunes: cursor 0x4400f2 cuadro 1 (capa 4), candado 0x440066 cuadro 5 en (-363, -206) ("NO DISPONIBLE"), indicador
  0x4400f3 cuadro 0 / 1 ("AL CONTRARIO" / "NORMAL" con el icono R1 del disco, tal cual) en las metas y las etapas;
  barra de ayuda {4, 0x294}, {8, 0x295} (la de sin tarjeta de memoria, 0x25303c; la de MENU.DAT 'CAMBIO'); cabecera de
  UN JUGADOR. Sonidos 0x40 al elegir, 3 en un candado, 1 al mover (4249F0).
- Juego de cursos y codigo (0x2a6758 = 4EEC80 del PC, el mismo): preset 0x2a6978: juego 0 -> 1, 1 -> 0, 2 -> 3, 3 -> 2
  (1760c0 del PS2: preset 1 = csc_data_2 = OutRun 2, 0 = csc_data_cvt = OutRun 2 SP). CONTRARRELOJ: sin 0x800, una
  etapa: codigo (+15 si >= 15, +15 con preset 0); con 0x800: preset 0: 5 / 11 -> 0x52 / 0x53, si no + 0x41 (+0x45 > 5);
  preset 1: 5 / 11 -> 0x50 / 0x51, si no + 0x3c (+0x40 > 5). El codigo [656234] lo carga 48B550 del PC (PS2: nombres
  de 0x21cf40): < 0x3c una etapa (43D950: tablas izquierda / derecha, +0x1e la segunda mitad; PALM -> PALM_T 0x3c,
  BEAC -> BEAC_T 0x3d), 0x3c.. OR2_goals, 0x41.. SP_goals, 0x46.. OR2_reverse_goals, 0x4b.. SP_reverse_goals (GOAL_A,
  GOAL_B_TA, GOAL_C_TA, GOAL_D_TA, GOAL_E en la Contrarreloj), 0x50..0x53 csc_data_2_ren / csc_data_cvt_ren
  (*_ren_course_TA). En la DC: el arbol 200 + codigo de CURSOTA.DAT (conv_union.arbol_ta, de esos .bin de SCRIPTS).
- Etapa sola: el registro de 44D720 con 635F34 (clave 0, union 0 = la bifurcacion, hijos 0 / 0: vuelve a si misma); la
  meta es el stage clear (44FF20 -> 48B3A0: codigo < 0x3c y 451350(0) == 1 -> 44FEF0(1)): en la DC el checkpoint del
  principio de la etapa siguiente se convierte en GOAL (demo/main.c, CUR_SIGUIENTE -> CUR_META).
- INFARTO: el juego 1 (OutRun 2 SP) corre el arbol 1 (csc_data_cvt); el 0 el de antes (arbol 0, csc_data_2).
- Uniones nuevas (208: las etapas solas con su bifurcacion y las de los arboles de las metas con un lado -1, cartel
  BLNK; conv_union.py las numera detras de las 262 de antes, que no cambian): 285,5 MiB sueltas, 17,3 MiB en
  UNIONESTA.PAK (pak_union.py --pak=UNIONESTA.PAK; comun/gd.c lee el paquete que nombra cada descriptor). Los datos
  pasan a 903 MiB de 984 (TABLERO.DAT 0,9 MB, CURSOTA.DAT 0,2 MB).
- Prueba: `-DPRUEBA_TABLERO_TODO` (la licencia activa con todos los bits 0x8a..0xce al entrar en un tablero) y
  `-DPRUEBA_TA_CODIGO=n` (con -DPRUEBA_AUTO -DPRUEBA_VARIANTE=7: la Contrarreloj del codigo n con el piloto);
  tools/prueba/tablero.sh.
- Verificado en E (Flycast, tools/prueba/tablero.sh con -DPRUEBA_SALON_MILLAS=20000): CONTRARRELOJ -> el juego de
  cursos (OutRun 2 / OutRun 2 SP), CURSO / ETAPA, las metas (META A..E, "15 ETAPAS SEGUIDAS" con "NO DISPONIBLE" y A
  bloqueado: sonido 3), R: las del reves con candado; ETAPA: las 15 etapas, R: PALM BEACH del reves con candado y A
  bloqueado. En el SALON, ETAPAS: compra del objeto 139 (0x8b, 3500 millas): de vuelta, PALM BEACH del reves sin
  candado y su carrera (codigo 30 -> arbol 230, PALM_R). Segundo arranque con la misma VMU: sigue sin candado; INFARTO
  con OutRun 2 SP: el coche y la carrera del arbol 1 (BEAC, HUD de corazones). Con el piloto
  (-DPRUEBA_AUTO -DPRUEBA_VARIANTE=7 -DPRUEBA_SIN_TIEMPO): codigo 0 (PALM_T sola) -> GOAL en el stage clear y la escena
  de la meta; codigo 0x46 (OR2_reverse_goals GOAL_A: TULI_R, CLOU_R, CAST_R, LAKE_R, PALM_R) -> 4 checkpoints y GOAL
  en PALM_R; con los datos empaquetados (UNIONESTA.PAK), codigo 0x41 (SP_goals GOAL_A): BEAC, SANF (el piloto toma
  la rama -1 de SANF y la etapa se vuelve a cargar, como el original), NIAG, MACH, EAST y GOAL. Sin "Out of memory" ni
  "pvr_mem: ERROR"; TABLERO.DAT 124 KB de tablas en el hueco, VRAM libre al abrir el
  tablero 481..516 KB (1009 KB de vuelta del salon). No probado en R.
- SIN PORTAR / BLOQUEADO:
  - La lista de records de cada curso (0x267210 / 0x2674d8 / 0x269880) y el foco hacia abajo: PORTADOS en la Rev 2.9.4
    (Rankings y records). Siguen sin portar los textos de (60, 342) / (60, 82), el titulo de filtro 0x4400b6 / 0x4400de
    y la barra de ayuda con lista.
  - La opcion B de la etapa 0 (PALM_BT, BEAC_BT, PALM_BR, BEAC_BR: descriptores 0x3e..0x41, bandera 830395 = +0x20c
    bit 0): el tablero la deja poner (con su bit), pero sus etapas no estan convertidas (SEG de los descriptores
    62..65): la carrera corre la etapa normal (aviso en la consola serie). PENDIENTE.
  - OUTRUN por la clave 0x24 (4 posiciones, los cursos seguidos con 0xcc / 0xcb): PORTADO en la revision 44 (Orden
    de las etapas). El preset de los juegos de la consola y el arbol del frontal de OUTRUN2SP: CORREGIDOS el 2026-11-13, ver
    Preset del curso.
  - INFARTO en OutRun 2 SP: corre el arbol 1 con el preset 0; las misiones, los coches y las misiones base por el
    preset / arbol estan HECHOS (2026-11-13, ver Preset del curso). PENDIENTE: los objetos (OSO, 45C470 con [78024C])
    y los bancos de voz HAM.CVT de ese curso (HECHOS en la Rev 2.9.9.c, ver Modos "sp" 2).
  - La escena de la meta de los arboles de la Contrarreloj: la letra sale del codigo (GOAL_A..E; PROBABLE: 450380(8)
    lee la clave del registro, que en los del reves no es 10..14); la etapa sola usa la de la meta A.

## Fin de la carrera: millas OutRun, continuar y GAME OVER (2026-11-11)

Se quita el panel de texto provisional (resultados(): "GOAL! / TIEMPO AGOTADO / TIEMPO TOTAL / PUNTOS / MAX km/h", con
su caja y el fundido a negro, y la salida al menu con START o a los 600 cuadros, inventados). Ahora, con la consola
(43F860 = 1), como el original (PC race_end_modes.cpp, SLES-53998; tabla de modos 0x3f8e50, {init, control, salida}):
- **Rutas**: meta -> modo 19 (0x4B0 cuadros, la escena de la meta) -> 33 (sin red: directo) -> 34; fin del tiempo ->
  modo 20 (0x1b9990: 0xF0 cuadros, 0x258 en las variantes 2 y 5; 0x1b9aa0: 43F860 -> 0x22) -> 34; REINTENTAR /
  ABANDONAR de la pausa (0x249c98 / 0x24a140) -> 34 con 450230(6) / (7) y la carrera suspendida (estado FIN de la
  demo: sin coche, trafico ni efectos del motor, 49A130). La licencia suma el crono al salir de 19 / 20 (como antes).
- **Modo 34, OutRun Miles** (0x1bc270 / 0x1bc3d8 / 0x1bc618 = 49B470 / 49B570 / 49B7C0): millas de 4EF550 (PS2
  0x1f3e90): variante 1 / 9 puntos / 3000, 2 [7F1C80] / 10000 / 10 (float); + 84BCF8 (0x36d2e4: por etapa 4EF460 +25 /
  +10 / +10 / +25 / +15 / +15 por cada estadistica limpia y 4EF440 40 por segundo por debajo de 60, desde 4F2B20 =
  PS2 0x290568 / 0x290584); + (car+E78 / 60) x 5 (car+E78: cada vez que CheckSlipStream encuentra un coche mejor por
  encima de 0,18 y |+162| < 1500, PS2 0x186494 = las marcas +B72 = 180 de la demo; el constructor lo pone a 0,
  0x239e84) y car+E78 = 0. Escena 2B004C (panel "MILLAS OUTRUN:", modo 5) y, cuando acaba, las 8 cifras cuentan a la
  vez hasta digito x 6 (cuadro de la escena 2B004B, 6 por digito), dibujadas por el GAD (0x1a0c98 = 497FA0) en
  (540 - 320 - i x 32, 210 - 240) (floats 0x35c8b8..c0); al pararse, el efecto 0x217 (muestra de \MILES.pak, ahora en
  SFX.DAT). Sale al acabar su temporizador (0xF0) o con A / B (1b4548 bits 4 / 8: cruz / circulo): 450240 = 6 ->
  modo 30 (REINTENTAR), 2 -> 35, 7 -> 27, 1 -> 35 (sin red y sin el Time Attack de una etapa: el port corre el curso
  entero). Al salir, 4EF510 (0x1f41d8): licencia +0x24 (float) += millas, +3F4 |= 2; se guarda en la VMU al volver a
  los menus (menu_suma_millas, comun/menu.c).
- **Modo 35, continuar** (0x1bc650 / 0x1bc700 / 0x1bc8c8 = 49B7E0 / 49D9C0 / 49DAE0): 2B004D ("VOLVER A
  INTENTARLO?", modo 5); al acabar, 2B004A (REINTENTAR / ABANDONAR); al acabar esta, A -> modo 30 (la misma carrera
  con el mismo coche y lo elegido, desde la salida: reintenta de demo/main.c, el de la pausa) y B -> modo 28 (al
  frontal), los dos con el efecto 0x40. Los iconos de los botones del PS2 de 2B004A se cambian por A / B como en la
  barra de ayuda de los menus.
- **Modo 27, GAME OVER** (0x1bafc8 / 0x1bb120): 0xF0 cuadros con 2C000B ("Fin de la partida", modo 1) y la voz 0x954
  (0x154 de PERO / PERH), despues al frontal.
- **HUD** (NAVI_PUB 4BEB00 = PS2 0x1bd4ac): fuera del modo 16 solo el panel 2C0029 y la puntuacion en la meta de la
  variante 1 hasta que el temporizador baja de 0x3FC (4B8E30(0) de 498670); en la meta de las demas variantes y en el
  fin del tiempo, nada (4B8E20); en los modos 34 / 35 / 27, nada (428600 borra los sprites, tambien GOAL / TIME OVER).
- Datos: tools/conv_fin.py -> FIN.DAT (millas y continuar: 4 texturas VQ, 529 piezas, 67 KB) y GOVER.DAT (GAME OVER:
  4029 piezas; aparte para no tener ~97 KB de tablas en la RAM fuera de ese modo); se abren al entrar (solo tablas) y
  se sueltan al salir. demo/fin.c. Prueba: -DPRUEBA_MILLAS (la demo automatica tambien acaba en las millas).
- **SIN PORTAR**: la bandera 84BCFC (primer puesto en una tabla de records: 4EF430) y su suma (OutrunMiles.bin +
  puntos / 1000, corazones / 10000 / 2,5): las tablas de records no estan en el port; la tabla de tiempos por etapa y
  la voz de 498670 (como antes); las variantes 4 (red) y 6 (misiones de C2C: otra suma en 4EF550, tokens 2B0066..69 /
  2B004F en el modo 35); la espera de la voz (427700 / 49A190: el temporizador del modo 20 se rearma mientras suena
  TIME OVER) y de la muestra 0x1a1 (modo 34 desde la pausa): sin consulta de "sonando" en comun/sfx.c; el bloqueo de
  botones del modo 35 (+0x5E0 del objeto 0x375068, PC +0x518 de 4035F0: sin localizar quien lo pone); la suspension de
  los sucesos del fin del tiempo (modo 20, 440A10) que no son coches (0x186, 0x187, 0x20..0x119, 0x11A..0x133,
  0x16A..0x17E: la demo no tiene esos sucesos sueltos; los coches, 8..0x1F, HECHO en la demo 40: el del jugador y el
  trafico se quedan quietos, sin fisica ni giro de ruedas; hasta la demo 39 el coche seguia con el freno a 0,6,
  PROVISIONAL, y seguia andando tras el fin del tiempo); la pantalla
  de carga del modo 30 / 28 (0x480000); START en los modos 19 / 20
  (43F980 -> 43F990(0x1D): sin localizar la peticion; antes la demo volvia al menu con START, inventado).
- 1001d8(0, 0xF0, 0.0) del modo 27 (0x1baff0; PC 42E020, el volumen de la musica): en el PS2 solo llama a 0x100110(0)
  (1 si suena el canal 0: [0x443b84] != 0) y devuelve eso; no cambia el volumen. Igual en sus otras 15 llamadas (el
  frontal de recreativa 0x2189a4.., 0x1b5c8c, 0x249698...): no hay fundido que portar. La salida del modo 27 (49AED0)
  para la musica: 401030(0) y (1) (fin_sale de demo/fin.c).
- Efectos del coche: 49A130 (PS2 0x1b8d80: 4278C0 de 0x300, 0x400, 0x100 y 0; 0x800 / 0x700 / 0x600 en las variantes
  4..6) en la entrada de los modos 19 (0x1b8f00) y 20 (0x1b99e8): la Dreamcast calla el motor, el derrape y el golpe
  (sfx_para) en META, AGOTADO y FIN (antes solo en FIN: en la meta y el fin del tiempo seguian sonando).
- Millas: START (0x1b6130) con la consola (1b4878 != 0) pone a 0 las millas y las de las etapas (0x1f3e68: 0x36d2e0 /
  0x36d2e4); fin_carrera_empieza(consola). Antes solo se ponian a 0 al sumarlas a la licencia: las de la demo de
  atraccion y las de OUTRUN2SP se sumaban en la siguiente carrera de la consola.
- Verificado en E (Flycast): fin del tiempo en PALM -> millas (0: 1740 puntos) -> continuar -> B -> titulo; pausa ->
  ABANDONAR -> millas -> GAME OVER -> titulo; INFARTO, pausa -> REINTENTAR -> millas -> salida otra vez con el HUD;
  `-DPRUEBA_AUTO -DPRUEBA_MILLAS -DPRUEBA_NODO=13 -DPRUEBA_FIN=400 -DPRUEBA_SIN_TIEMPO`: GOAL en IMPE -> 1991 millas
  (65990 / 3000 + 1970 de las etapas) contando cifra a cifra -> continuar -> A -> la misma carrera desde la salida ->
  GOAL otra vez. Sin "Out of memory"; VRAM libre al entrar 50-70 K (536 K en el INFARTO, con el HUD fuera).

## Preset del curso: OutRun 2 / OutRun 2 SP (2026-11-13)

El preset [78024C] (PS2 [0x44350c] = [gp-0x7b64], gp = 0x44b070 en 0x1012f8) decide el curso y sus reglas. La DC lo
tenia al reves (0 = OutRun 2): en el juego de la consola corria el curso de OutRun 2 con las reglas de OutRun 2 SP, y
en OUTRUN2SP cada lado del gatillo R corria el curso del otro. CONFIRMADO en el MIPS de SLES-53998 (y en el PC:
outrun2-decomp r075_course_name de 44DA00, el mismo reparto):
- 0x1760c0 (= 44DA00): 1 / 4 -> "csc_data_2" (0x3cb4d8: PALM.., OutRun 2), 0 / 3 -> "csc_data_cvt" (0x3cb4e8:
  BEAC.., OutRun 2 SP), 2 -> csc_data_easy, 5 / 7 / 8 / 10 -> csc_data_cvt_ren, 6 / 9 -> csc_data_2_ren.
- START (0x1b63a0, variantes 0 / 1 / 2 de [0x443508]): 0x1760c0(preset 0 -> 0, 1 -> 1, 2 -> 5, 3 -> 6, segundo
  argumento 1). El preset 0 es el de OutRun 2 SP y el 1 el de OutRun 2.
- Quien lo escribe (0x1b4a88 = 43F950; la puesta a cero 0x1b48d8 lo deja en 0): el juego de la consola al salir del
  menu (0x23ab54 -> 0x2a6758 para todos los modos, tambien COAST 2 COAST y antes de OUTRUN2SP): 0x2a6978 (= 4EEA70)
  con los bits 0..1 de la raiz +0x208 (el juego de cursos de los tableros 0x24..0x27): 0 -> 1, 1 -> 0, 2 -> 3, 3 -> 2
  (el juego 0 del tablero es "OutRun 2"); la CONTRARRELOJ de los cursos seguidos ademas 2 / 3 (0x2a6884 / 0x2a68e8).
  Las misiones de COAST 2 COAST (0x2a6ad4: variante 6) no lo cambian: se quedan con el de esos bits (0 si nunca se
  entro en un tablero: preset 1); los campeonatos (0x2a6b08..: variante 5) 1 o 0 por su peticion. El frontal de
  OUTRUN2SP (0x215fa4 = el final de 4C3DC0): el +27 del lado (0 "Recorrido OutRun2SP", 1 "OutRun2"; 2 / 3 las filas de
  15 recorridos) y 0x1760c0 del preset. La demo de atraccion (0x2878a8): preset 1 y csc_data_2. El menu de la
  consola lee csc_data_2 al entrar (0x237de4: 0x1760c0(1)).
- Confirmacion independiente por los nombres de los recursos: con el preset 1 / 3 el banderero es CHR_AUT04 y con el
  0 / 2 CHR_AUT04_CVT (0x2046f0); los bancos de voz del INFARTO (0x173a6c: 0x1b3be8 / 0x1b3c70) son HAM\1A.. (entradas
  0x43.. de 0x352840) con el preset 1 y HAM.CVT\1A.. (0x1a..) con el 0.

| Juego | Antes (DC) | Ahora (DC) = original |
|---|---|---|
| OUTRUN de la consola | preset 0, arbol 0 (csc_data_2) | preset 1, arbol 0 con el juego 0 (desde la revision 44 pasa por el tablero 0x24: juegos 1..3 -> preset 0 / 3 / 2, arboles 1 / 290 / 291; ver Orden de las etapas) |
| INFARTO, juego 0 (OutRun 2) | preset 0, arbol 0 | preset 1, arbol 0 |
| INFARTO, juego 1 (OutRun 2 SP) | preset 0, arbol 1 | preset 0, arbol 1 (igual) |
| CONTRARRELOJ | preset 0 (el codigo ya salia del juego: menu_ta_codigo) | 0x2a6978 del juego (1 OutRun 2, 0 SP) |
| COAST 2 COAST (misiones) | preset 0 | el del ultimo juego de cursos de los tableros (1 sin tocarlos) |
| OUTRUN2SP lado 0 "Recorrido OutRun2SP" | preset 0, arbol 0 (el curso de OutRun 2) | preset 0, arbol 1 (BEAC..) |
| OUTRUN2SP lado 1 "OutRun2" (gatillo R) | preset 1, arbol 1 (el de OutRun 2 SP) | preset 1, arbol 0 (PALM..) |
| Demo de atraccion | preset 0, arbol 0 | preset 1, arbol 0 |

Lo que lee el preset (todas las lecturas de [0x44350c] del ELF revisadas) y como estaba la DC:
- Solo numeracion (el efecto ya era el del original o no cambia nada): la fila de tiempos (0x176018: TIM1 del preset;
  las filas 0 y 1 de la tabla base son iguales, los tiempos no cambian); el aviso "<- FACIL" (0x1c50c0: presets 0, 1 y
  4); CARGASP0..3 (0x1b8228, por el numero de preset: cada lado ya tenia su mapa), el fondo / columna del final de
  recreativa (0x19bae4 / 0x209428: +5 con 0 / 2), los rankings RKSP (por preset) y la escena del GAME OVER: indexadas
  con el preset del frontal, que era el bueno; lo que cambia en OUTRUN2SP es el curso que se corre con ellos.
- Dos errores que se compensaban: la entrada de nombre (demo/nombre.c) tomaba los descriptores del arbol 0 con el
  preset 0 (desc[preset & 1]); con el arbol del preset (preset 1 -> desc[0] = csc_data_2) sigue dando el fondo de la
  etapa que se ha corrido.
- Efecto equivocado de verdad, CORREGIDO:
  - Arbol de OUTRUN2SP (`curso_pedido = preset`): ahora `arbol_de_preset` (0x1760c0).
  - Juego de la consola con el preset 0: ahora `preset_de_juego` (0x2a6978) del juego de cursos (`menu_juego_cursos`).
    Para el jugador, en OUTRUN / INFARTO de OutRun 2 / COAST 2 COAST: choques entre coches con la restitucion de
    OutRun 2 (0x191d20: 1,0 / 0,9 / 0,7 en vez de 0,8), trafico de la clase 4 en ciclo de 6 (0x1E, 0x20..0x24: sin el
    38) y clases 0..2 0x27 / 0x29 / 0x28 (camiones: en la consola siguen sin cargarse), el banderero CHR_AUT04
    (BAND1.DAT) en vez del de OutRun 2 SP, las escenas de la meta del preset 1 (1..5 en vez de 3, 33, 3, 35, 34).
  - INFARTO de OutRun 2 corria las misiones y los coches del preset 0 (los de OutRun 2 SP): 0x1a7bb8 da 0x3f0b98 con el
    preset 0 y 0x3f3ed8 con los demas; 0x1b2e58 la tabla de coches 0x362718 con el preset 1 y 0x35c958 con los demas;
    0x1b2ca4 los modelos de la clase 3 / 4 de 0x3f0850 % 6 con el preset 1 (% 7 con los demas) y 0x3f0870 % 7 en la
    etapa del descriptor 0x1a (NEWY). Y la mision base de cada clave (45C160 = 0x1aec08: 176700, el descriptor de la
    clave en el arbol cargado, > 14 resta 15) salia siempre del arbol 0: mal en el INFARTO de OutRun 2 SP. Ahora
    INFARTOP.DAT (tools/conv_infarto.py, paso `infarto` de regenera.py) lleva las misiones y los coches del preset 1 y
    las bases de los dos arboles; comun/infarto.c los elige con infarto_preset (INFARTO.DAT no cambia).
  - Trafico de los presets 0 / 2 en NEWY (0x1db4dc: 176700 de la clave del jugador == 0x1a, SEG30): la lista de 9 de
    0x36bf90 (0x25 x 6, 0x21, 0x22, 0x23) en vez de la de 7. No se veia antes (el preset 0 nunca corria el arbol 1
    salvo el INFARTO de OutRun 2 SP del 2026-11-13).
- SIN PORTAR (lecturas del preset de partes que la DC no tiene): los bancos de voz HAM.CVT del INFARTO de OutRun 2 SP
  (0x173a6c: HECHO en la Rev 2.9.9.c, ver Modos "sp" 2), los sonidos 0x841a / 0x8443 de 0x173cbc, el
  recurso 0x12b / 0xc3 de 0x1736b0 / 0x1b7ac8 / 0x206cac / 0x287e98 (el personaje del banderero, que la DC lee con
  BAND / BAND1), el mapa de ruta del HUD (0x1be240 / 0x1bfb60 / 0x1c36f8 / 0x1cc0d0: tablas por preset, presets 0 y 1
  iguales), la camara de 0x17c018 / 0x17e860 (presets 0 / 2 frente a 1 / 3: sin portar), la lista de 7 de 0x22d0a0, los
  records de la Contrarreloj (0x21cc74 / 0x21f7a0 / 0x1fe3c8 / 0x20012c) y 0x28d0c8. La clase 4 de los presets 1 / 3
  con red o rivales ([0x443514] != 0) va por la lista de 7 / 9: la DC no lo mira.
- OUTRUN de la consola: PORTADO el tablero 0x24 de 4 posiciones (revision 44, Orden de las etapas). COAST 2 COAST con
  el juego que quedo del ultimo tablero: CONFIRMADO (0x2a6770 llama a 0x2a6978 sin condicion).
- Pruebas: tools/prueba/sp.sh con SP_LADO=1 (por defecto, el gatillo R: preset 1, arbol 0) o SP_LADO=0 (preset 0, arbol
  1); final.sh y resultados.sh usan SP_LADO=0 (las metas de OutRun 2 SP: FLOR es la clave 13 del arbol 1).
- Verificado en Flycast (display 95): sp.sh SP_LADO=0 ("Recorrido OutRun2SP"): `juego OUTRUN2SP, preset 0`, `CURSO:
  arbol 1, 15 etapas, salida BEAC`, CARGASP0, `BAND: preset 0` (BAND.DAT), tiempo inicial 99 (5999 cuadros); SP_LADO=1
  (gatillo R, "OutRun2"): `preset 1`, el arbol 0 (PALM, sin leer otro), CARGASP1, `BAND: preset 1` (BAND1.DAT); sin
  "Out of memory" ni "pvr_mem: ERROR" (RAM libre en carrera 926K / 908K). nav.sh MODO_JUEGO=1 (OUTRUN): `juego consola,
  preset 1`, PALM, `BAND: preset 1` (el banderero CHR_AUT04), tiempo inicial 99. nav.sh MODO_JUEGO=0 (COAST 2 COAST,
  mision 1-1): `variante 6, ..., preset 1`, arbol 100. conduce.sh MODO_JUEGO=2 (menu.sh pasa por el tablero 0x24;
  TB_JUEGO=1 el juego 1), 150 s: juego 0 `INFARTO: preset 1, misiones INFARTOP (0x3f3ed8), coches INFARTOP (0x362718),
  bases 0 1 3 ...`, la peticion 2 de la mision 0 en las secciones 145..410 y coches de la mision 33, 34, 30, 32, 35, 36
  en ciclo de 6 (sin el 38); juego 1 `preset 0, misiones INFARTO (0x3f0b98)`, arbol 1 (BEAC), bases 0 6 9 ..., la
  peticion 2 en 90..400 y el ciclo de 7 con el 38. Sin "Out of memory" ni "pvr_mem: ERROR" en ninguna (RAM libre en
  carrera 996K / 1028K / 1027K).

## OUTRUN2SP (2026-11-11)

Flujo del original (SLES-53998 y la decompilacion; PC native_runtime.cpp 43F870, race_end_modes.cpp "OUTRUN2SP route",
arcade_attract.cpp):
- Menu principal, opcion 2 (commit 0x23ab30, tabla de saltos 0x416af0 -> 0x23aba0 = PC 4E2970 modo 2): 0x1b4888(0)
  pone [0x59acac] = 0 (PC [780260], 43F860 = PS2 0x1b4878; el menu de la consola lo pone a 1 al entrar, 0x237e28) y
  +21C = 7. El modo 32 (SUMO_FE) pasa al 7 (48AE40 / 49F360 / 49F370: suelta el mundo del frontal) -> 9 (48AE50:
  variante -1) -> 10 ATTRACT (48B210 / 48AE90): el frontal de recreativa de OUTRUN2SP (suceso 4, funciones 1..12 de
  la tabla 59BE78, secuencia 5C8BE0; PS2 0x210fbc..0x218748 y 0x28efec..: los mismos tokens 0x2E00xx). Banco SPRANI
  0x2E = SELECTOR_CVT (spr_sprani_selector_cvt_[s]xst, 96 escenas, 238 texturas): "Elige modo de juego" (OutRun,
  Infarto, Contrarreloj con los cursos OutRun 2 / OutRun 2 SP; 4C3DC0: fila 0 -> variante 1, 1 -> 2, 2 -> 0; lado +3E
  -> preset 0 / 1, filas 3 / 4 -> preset 2 / 3; 43F940 / 43F950 y 44DA00(0 / 1 / 5 / 6)), "Elige coche" (con el coche
  3D, 4A7080 / 475670), cambio, "Elige la musica" (5C8D00), "Shortcut Features" (SI / NO), "Modo controvertido" (coche
  preparado), carrera (funcion 0xB 4C05F0 -> START).
- Carga: en el juego de OUTRUN2SP (0x1b8228, solo con [0x59acac] = 0, variantes 0..2 y 6..9: tabla 0x3f9720) la
  pantalla "Cargando" del banco 0x2F (LOADING_CVT) con el logo y el mapa del curso del preset: preset 0 -> 2F0002, 1 ->
  2F0003, 2 -> 2F0000, 3 -> 2F0001, capa 2, modo 0 (en bucle), cuadros 0..0x77, creada al acabar de leer el curso en
  START (0x1b64ac). HECHA (2026-11-11): tools/conv_sp.py -> CARGASP0..3.DAT (uno por preset, grupo CARGA_Pn; sus
  tablas en el hueco de la union, que START no usa hasta la primera union), demo/sp.c. La demo carga
  de un tiron: la escena se dibuja al empezar la carga y entre sus pasos con el cuadro del reloj de la logica (60000 /
  1001 por segundo) y la pantalla se queda con el ultimo cuadro mientras dura cada paso; sus texturas (VQ, detras de las
  del HUD) salen antes de reservar la zona del curso. Se quita al empezar la escena de la salida (el original la
  deja hasta que START la borra: sin localizar en que estado; PROBABLE el mismo momento). Desde la demo 41 (DESVIACION
  pedida por el usuario: Menu principal sin MULTIPLAYER y pantallas de carga) se pone al salir del frontal, con toda la
  carga de la carrera por debajo, y dura como poco 3 s; se quita al acabar la carga.
- Fin: GOAL (modo 19) -> 33 -> 24 (final de recreativa AUTOSCENE \AS\as_E_xx, 485470, fondos END_*_STAGE) -> entrada
  de nombre (modo 26, race_name_entry.cpp, bancos NAME_CMN / NAME5A..E) y rankings de OUTRUN2SP (sp_rankings.cpp);
  sin licencia ni Miles (49CFD0 solo registra con 43F860 != 0). Tiempo agotado: modo 20 -> 0x19 (no 0x22). Pausa:
  REINTENTAR -> 30 (0x249ca8), ABANDONAR -> 27 GAME OVER (0x24a150; banderin 0x34001F). Salida (modo 28, 49AF90): al
  frontal de recreativa (modo 10) salvo [65994C] (-> 32, el menu de la consola); el frontal vuelve al menu de la
  consola con su funcion 0xB cancelada (4BF510: modo 32).

Reglas que cambian con el preset del curso (todas CONFIRMADAS en el MIPS; preset 0 = OutRun 2 SP / csc_data_cvt, 1 =
OutRun 2 / csc_data_2, ver Preset del curso: hasta el 2026-11-13 esta tabla decia "OUTRUN2SP / preset 1" con el arbol
1, que era el preset equivocado para ese arbol):
| Que | Original | DC |
|---|---|---|
| Arbol del curso | 0x1b63a0 en START: 0x1760c0(preset 0 -> 0, 1 -> 1) = 44DA00: 0 csc_data_cvt, 1 csc_data_2 | `arbol_de_preset`: preset 0 -> arbol 1, 1 -> arbol 0 de CURSO.DAT (`curso_pedido`) |
| Tabla de tiempos | 0x176018: preset 0 / 1 / 2 / 3 -> fila 0 / 1 / 5 / 6 de la tabla base (TIM1[preset]); k636904 se suma (43F960 = 0) | `TIM_PRESET` = `preset_curso` |
| Dificultad | 0x1cdc98: licencia +E0 (2 en la variante 4); 0 en la consola | `tim_dif` |
| Trafico: modelos | 46CE30 = 0x1db2a0: clase 4 con preset 1 / 3 -> 0x1E, 0x20..0x24 en ciclo de 6 (0x1db49c..0x1db578), con 0 / 2 la lista de 7 (0x36bf70) o, en la etapa del descriptor 0x1a (NEWY), la de 9 de 0x36bf90 (0x25 x 6, 0x21, 0x22, 0x23); clase 0 0x27 / 0x29 (1 / 3) o 0x29 / 0x2B, 1 / 2 0x28 o 0x2B | `traf_modelo_46ce30` (con el dado de la clase, ver abajo; la lista de NEWY desde el 2026-11-13) |
| Trafico: tablas de aparicion | las del descriptor de cada etapa (+0x80 / +0x84) y sus puntos fijos (4EF880) | TAPD.DAT por segmento, copiadas al leer el arbol (antes la demo usaba las del arbol 0 por indice en todos los arboles) |
| Choques entre coches | 0x191d20 / 0x191d70: con preset 1 / 3 (OutRun 2) umbrales 0x35b9d8 (80, 280: iguales) y restitucion 0x35b9f0 = 1,0 / 0,9 / 0,7 (normal 0x35b9e0: 1,0 / 0,9 / 0,8) | bloque CHA1 de PARAM.DAT, `or2_choques_preset` |
| Banderero de la salida | 0x2046f0: personaje 3 = CHR_AUT04 (recurso 0xC3, tela del recurso 0xC3: 0x100d40) con preset 1 / 3 (OutRun 2), si no 4 = CHR_AUT04_CVT (OutRun 2 SP); hueso de la tela 0x11; movimientos y posiciones de la fila del preset | BAND1.DAT (`conv_chr.py --banderero-sp`), el resto ya iba por preset en robot.c |
| Escena de la meta | 0x3f99b0 + (meta + preset x 5) x 4: preset 0 -> 3, 33, 3, 35, 34; preset 1 -> escenas 1..5 | `meta_escena(variante, preset_curso, meta)`, metas A..E = claves 10..14 del arbol |
| Piloto / acompanante | 487EE0 / 487F50 sin 43F860: 0xB (CHR_DR_M00) y 9 (CHR_DR_L00) en las variantes 1, 3, 4, 5; 6 (CHR_DR_G00_USA) en el resto | CHR1109.DAT / CHR1106.DAT (`robot_pareja`, ver Coches: la pareja de la carrera) |
| Voces | 427DB0: juego 4 (SPRACE); con el idioma espanol el directorio es SPANISH en todos los juegos | sin cambio |
| Mapa de ruta del HUD | 0x1be288 / 0x1bfc68 / 0x1c38a0: presets 0 y 1 igual (5 niveles) | sin cambio |

Clase del coche de trafico (46CE30, PS2 0x1db2a0..0x1db3b4; antes la demo daba siempre la clase 4, CORREGIDO para
todos los arboles): la primera clase con peso (+6..+0xA del registro de aparicion; sin ninguna, la 4); si hay, desde
la de 800D9C (PS2 0x5c7740) se tira el dado de la clase (80A668 + c = PS2 0x5c7738: b = b x 5 + 1, b x 100 / 255)
hasta que sale por debajo de su peso, pasando a la siguiente; 800D9C + 1 al abrir el coche (0..4). Valores iniciales
46C950 (PS2 0x1d9c40): 0x78, 0x67, 0x69, 0x5E, 0xD4 y cuentas a 0. En el arbol 0 todos los registros son 0,0,0,0,100
(sale la 4 como antes); en OUTRUN2SP SANF (0,0,0,3,97), YOSE (0,0,0,5,95)... dan la clase 3 (0x1F, el autobus).
En el INFARTO (variante 2) la clase 0 pasa a la 4 salvo en el tipo de peticion 0x16 (45C440 = PS2 0x1af3f8: [0x589b08],
el tipo de infarto.c; 0x1db3c0..0x1db3ec), con la cuenta de la clase que salio (CORREGIDO 2026-11-11: antes siempre 4). Los modelos 39, 40, 41 y 43 (clases 0..2: camiones) salen en las carreras
de OUTRUN2SP (HECHO 2026-11-13, ver Camiones abajo); en el juego de la consola se siguen gastando sin coche, como los
puntos fijos de esos modelos.

Estado en la DC (verificado en E, tools/prueba/sp.sh: OUTRUN2SP desde el menu, coche, cambio, musica y 90 s de
carrera por BEAC -> SANF; `MODO: variante 1, dificultad 0, ..., juego OUTRUN2SP, preset 1`, `CURSO: arbol 1, 15
etapas, salida BEAC`, BAND1.DAT en el hueco de la union; RAM libre 1419..1490 KB, 901 KB seguidos; 0 "Out of"):
- HECHO: la entrada del menu principal, el arbol 1, preset 1 y [780260] = 0 con todas las reglas de la tabla de
  arriba. Fin: sin millas ni continuar (los de la consola, demo/fin.c, no son de este juego: 49CFD0 / 49D850 con
  43F860 = 0); tras la escena de la meta (EAST: escena 1 del preset 1, musica 66, verificado con el piloto desde el
  menu real) o el fin del tiempo suma el crono a la licencia (0x1b96bc / 0x1b9b58) y pasa al GAME OVER del modo 27
  (HECHO 2026-11-11): el GAD (0x1a04f0) con 1b4878 = 0 y 450240 != 7 crea la escena 34001F (banco 0x34 ROUTE_CVT,
  "Fin de la partida" sobre las palmeras, capa 0, modo 1; tools/conv_sp.py -> GOVSP.DAT, 240 cuadros = los 0xF0 pasos
  del modo), la voz 0x954 una vez y al acabar el modo 28 -> el menu. El original pasa antes por el final de recreativa
  (24, solo tras la meta), los resultados (25: suceso 0x188, funcion 0x15 / 0x50, banco 0x34) y la entrada de nombre
  (26, solo con 450240 = 1: el fin del tiempo): el 25 y el 26 HECHOS (ver abajo), el 24 HECHO en parte (2026-11-13,
  ver Final de recreativa de OUTRUN2SP abajo); en la pausa
  REINTENTAR es el modo 30 (la salida otra vez) y ABANDONAR el modo 27 GAME OVER (HECHO 2026-11-11, fin_game_over de
  demo/fin.c: la carrera suspendida y, con 450240 = 7, la escena 2C000B de la consola, 0x1a051c; 0xF0 pasos y la voz
  0x954) y despues al menu (el frontal de recreativa, modo 10, SIN PORTAR).
- Banderero del preset 1 (BAND1.DAT: (-4,5, 0, -21) girado 180 grados, movimiento 0x170004) que no se veia en la
  salida de BEAC: CORREGIDO 2026-11-11, era la salida del coche. 0x2046f0 lo pone en el mundo sin marco (T(0x40ef88 +
  preset x 12), Rz Ry Rx de 0x40f008), 5 m por delante del coche, que 4A6ED0 pone en (0, 0, -16) mirando a -z. La demo
  tomaba el paso de 0,25 m de la ruta donde (0, -16) queda de lado y el rumbo de la ruta alli: en PALM la ruta va recta
  por -z (igual), pero en BEAC llega en curva y el coche quedaba en (1,9, -23,8) torcido 14 grados, ya pasado el
  banderero. Ahora el cruce se afina entre los dos pasos (biseccion) y el coche sale mirando a -z (yo.a = -rumbo de la
  ruta, or2_coloca con rumbo 0 en el mundo; salida_calcula / salida_a_ en demo/main.c).
- Frontal de recreativa (SELECTOR_CVT), HECHO 2026-11-11 (comun/fsp.c, comun/sprani.c, tools/conv_sprani.py,
  conv_sp.py tablas_fe -> SPFE.DAT): ver "Frontal de recreativa de OUTRUN2SP" abajo. Sustituye a las pantallas de
  coche, cambio y musica de la consola que se usaban (PROVISIONAL) en este juego.
- Vuelta al frontal de recreativa, HECHA 2026-11-11: el modo 28 (49AF90 estado 3) sin la consola (43F860 = 0) y sin
  [65994C] pide el modo 10: tras el GAME OVER (meta, fin del tiempo y ABANDONAR de la pausa) la Dreamcast vuelve al
  frontal (menu_entra_sp) en vez de al titulo de la consola.
- Resultados (modo 25), HECHO 2026-11-11 (demo/fin.c fin_sp_resultados; tools/conv_sp.py rutas -> RUTASP.DAT: el
  banco 0x34 ROUTE_CVT entero para comun/sprani.c y el bloque RSP1 con las tablas del mapa; MUS28.ADP). Tabla de modos
  del PS2 0x3f8e50 (16 bytes por modo: 24 = 0x1ba4d8 / 0x1ba5f0 / 0x1ba6a0, 25 = 0x1ba6c0 / 0x1ba748 / 0x1ba910, 26 =
  0x1ba968 / 0x1baaf8 / 0x1babe8), comprobado instruccion a instruccion con race_end_modes.cpp:
  - 25: temporizador 0xB4, banco 0x34, musica 100050(0, 0x1C, 1) = sprintf("\music\%02d.ps2", 0x1C) en bucle (MUS28),
    en la variante 0 0x5a2030 = 1a7180(preset); con el banco el suceso 0x188 con la funcion 0x15 (1b4a90 = preset 2 / 3
    -> la 0x50, resultado de los 15 recorridos: HECHO en la Rev 2.9.9.c, ver Modos "sp" 1); A o START (210c58:
    210988(1) / (4)) o el mapa acabado ([0x370dc8]) y despues el temporizador o A / START: 26 con 450240 = 1 o en la
    variante 0 con el record ([0x5a2030]), si no 27 (las variantes 3 / 4 piden antes el 27). Sin el 26: al 27. La
    animacion de carga del banco 0x33 (288d20(1) mientras se lee el banco) no sale: la Dreamcast lo lee de un tiron.
  - Mapa (tabla de funciones 0x3ee7b4, fila 0x14: 0x209208 / 0x209518 / 0x2099f0 / 0x2099f8 = PC 4AE750 / 4AE960 /
    4AED20; trabajo 0x5df960 = PC 842048): coche del mapa = [0x40fe68 + ([0x40fc10 + modelo x 4] - 1) x 0x78 + modelo x
    4] (modelo 0x1bcfb0 = 48B140, color 0x1bcfd0 = 48B160; sin color 340017); 12 puntos a -10000, el primero y el
    segundo de 0x40fd78 / 0x40fd88, y por cada nivel 0..3 con rama (451350: 1 -> siguiente 1 del nodo y tramo 2, 0 ->
    siguiente 0 y tramo 1, si no -1 y se acaba) los puntos de y a del nodo (nodos 0x40fc88 de 16 bytes, puntos 0x40fd70
    de 8); escenas 34000E / 34001A (presets 0 / 2) o 34000D / 340019 en las capas 1 y 5 (modo 0), 34001D en la 6 (modo
    1) y el coche en la 4 (modo 0, cuadros 0x5B..0x96). Cada paso: si el punto siguiente esta a menos de -9000, el
    mapa acaba (el ultimo tramo 34001B queda quieto, 115020(h, 2)); con el nivel de 4AEEF0 se para en su razon; 14
    pasos antes del final de un tramo el coche gira hacia el siguiente (cuadros 0x3D..0x4B / 0x97..0xA5, velocidad +-1);
    el coche en x, -y del punto interpolado (0x28 pasos por tramo); al empezar cada tramo (de los 9 primeros) 34001B en la
    capa 2 (modo 1: 1..0x28 etapa, 0x29..0x50 izquierda, 0x51..0x78 derecha) y en los de etapa salvo el primero la marca
    34001C (capa 3); al acabar el tramo el siguiente (cuadros 0x5B..0x95 / 1..0x3C / 0xB5..0xF0). -10000 y -9000 en el
    codigo (lui / ori de 0x2092c4 y 0x209554).
  - En la Dreamcast la rama de cada nivel se guarda al decidirse (rama_nivel de demo/main.c: 451350) y 4AEEF0 al
    acabarse el tiempo: nivel de la etapa, 0 / 1 en la etapa / union (car+5C) y la razon fila / filas del tramo (tr_fila:
    las filas de la union o de la etapa son las de la ruta de la Dreamcast, s_union - s_etapa entre m_pos; 30000 si aun
    no se sabe: PROBABLE igual a 43D470).
  - Verificado en E (Flycast, tools/prueba/resultados.sh): OUTRUN2SP preset 1, fin del tiempo en SANF (etapa 2, rama
    izquierda): `FIN: modo 25 (450240 = 1), ... ramas 0 2 2 2, nivel 1 lado 0 razon 0.848`, el mapa del recorrido con el
    coche por INICIO -> izquierda y parado en la segunda etapa, el GAME OVER y la vuelta al frontal; 0 "Out of memory".
    VRAM libre al entrar 613 KB (el banco 0x34 sube solo las texturas de las escenas creadas).
- Entrada de nombre (modo 26) y rankings de OUTRUN2SP, HECHO 2026-11-11 (demo/nombre.c, comun/rksp.c; tools/conv_sp.py
  nombre -> RANKSP.DAT: el banco 0x3D RANKING_CVT entero para comun/sprani.c y el bloque NOM1 con las tablas; NOM5A..E.DAT:
  los bancos 0x37..0x3B NAME5A..E, el fondo 0xC / 0xD; NOMTX.DAT: los glifos de las fuentes 3 y 4 del PS2 (= 2 / 3 del
  PC) y la banda 0x150010 de spr_ranking_xst). Tras los resultados (25) con 450240 = 1 (fin del tiempo); tras la meta el
  original no la ensena (0x1ba810: 26 solo con 450240 = 1 o en la variante 0 con un record [0x5a2030]).
  - PS2 (comprobado instruccion a instruccion con race_name_entry.cpp): modo 26 = 0x1ba968 / 0x1baaf8 / 0x1babe8; suceso
    0x189 funcion 0x16 = fila 0x15 de 0x3ee7b4: 0x1fc560 / 0x1fcd30 / 0x1fd1e8 (tablero 0x200398) / 0x1fd528. Modulo en
    0x5da108 (PC 8423C0, el mismo orden). Tablas del ELF: fondo por descriptor 0x40d248 (= el banco de la tabla de saltos
    0x3f9800 del modo), tabla y titulo 0x40daa0 por [0x40e040 + (cambio + config x 2 + variante x 4) x 4] + preset x 6,
    filas 0x40da88, coches 0x40da10, configuracion 0x40cfc8, filas 3D0016 0x40e028, posiciones 0x40dc80 / 0x40ddb0,
    estilos de cifras 0x40cfd8, nombre 0x40c838, conjuntos 0x40c840 / 0x40c848, "----" 0x40c860, palabras 0x36f4b8,
    textos 0x443..0x445 (en el PC 0x444..0x446), fuentes 3 / 4 (0x117320; el PC 2 / 3). 0x1ff740 (4B1680), 0x2002b8 (el
    filtro de 4B1F40) y los registros por defecto (0x1fa3b0) se ejecutan en la conversion con tools/ps2/mips.py (ampliado
    con mult / div / madd / desplazamientos de 64 bits / lb / lh...).
  - Rankings: 0x5d7548..0x5da108 (0x2BC0 bytes; v1 presets 0 / 1, v2, v0 presets 0 / 1, v1 presets 2 / 3, v0 presets 2 / 3;
    PC 84DF40.. en otro orden), registros de 16 bytes con el nombre en +0xB (el PC en +0xC); insercion 0x1fae90 (4F3010),
    accesos 0x1fb970 / 0x1fb9f0 / 0x1fbad0, guardado del registro 0x1fbb20 / 0x1fbbf0 / 0x1fbd18 (coche de la licencia
    +0x1C en los bits 12..16). Por defecto: AM2 / OR2 / R&D, 100000 puntos, 4'50"000 / 14'40"000.
  - VMU: el PS2 guarda los rankings con los ajustes (0x106288: 0x2D20 bytes de [0x35a9e8]) y despues la licencia
    (0x1a67e0 / 0x1a6840 si [0x443508] = 0) en los estados 7..0xB del control; la Dreamcast los guarda en el fichero de
    las licencias (comun/menu.c, version 2: 'OR2L' 2, la licencia activa, las 4 licencias, 'RKSP' y las tablas; los de la
    version 1 se leen igual y los rankings quedan por defecto).
  - Textos: 0x1175a8 -> 0x116fa0 / 0x116e10 (sin metricas: la celda entera, avance = ancho + espaciado; espacio = ancho;
    >= 0x80 no se dibuja en el PS2: "PUNTUACI N"; DESVIACION pedida por el usuario en la revision 41: la Dreamcast dibuja
    el glifo 0xB1 de la fuente 4, que si esta (Ó), y sale "PUNTUACIÓN"; NOMTX.DAT lleva los glifos >= 0x80 de esos textos) en la capa 4 (0x117558(4) -> 0x117708). La banda 0x150010 (117d28:
    la escala que se le pasa es la capa, 1).
  - PROBABLE: el color 0xFFFFFF00 de los nombres como ARGB (amarillo, el del PC); el orden dentro de una capa (escenas y
    despues textos / banda); 451180(0) = 0 tras el fin del tiempo ([0x5c60f0] = -1 sin acabar); sin llegar al nivel 4,
    44DC50(-1) da el descriptor de [0x548f6c] (BSS, sin localizar): el del primer registro (mismo fondo NAME5A).
  - SIN PORTAR: el deslizamiento de las filas (0x1fe780 / 4B0970: no se ve sin el teclado), el banco 0x36 y los de
    sprites 0x12 / 0x14 (solo el teclado de letras, que la entrada de nombre no ensena: [0x424] = 0), los records de la
    Contrarreloj de cada etapa (47EE70 / 0x1a65c8 y 47EF30: el 26 en la variante 0 con record y la lista de etapas), la
    consulta de rankings fuera de la carrera (0x1ff950 = tablero con [0x424] = 1; la llama 0x287a60, funcion 0x13 del
    suceso del modo de atraccion con el banco 0x32 ADV_CVT, sin portar) y el frontal de rankings del PC
    (frontend_rankings.cpp: el menu de la consola del PC, distinto del PS2). DESVIACION: fuera del ranking el original
    guarda el nombre en el registro de antes de la tabla (0x1fb970 con el puesto -1); la Dreamcast no.
- Final de recreativa (modo 24): HECHO (2026-11-13), ver Final de recreativa de OUTRUN2SP abajo; sin las luces
  del final ni el humo KEM.
- Etapas al reves (descriptores 30..59; SEG34..SEG65), HECHO 2026-11-11: sus tablas de aparicion y de velocidad (+0x80
  / +0x84) estan en el ELF; lo que el ELF deja en la BSS son sus listas de puntos fijos (4EF880: 0x5d6028..), y por eso
  conv_trafico.py daba el registro entero vacio (sin trafico). Las rellena la rutina 0x1f2110 (la llama la puesta a cero
  del trafico, 46C950 = PS2 0x1d9e30): copia las listas de .data (0x36c020..) a la BSS y despues, para cada etapa s de 0
  a 0x41, pone a 0 el usado (+0x14) y, si 30 <= s < 60, da la vuelta a sus puntos: posicion = 1 + [descriptor + 0x96]
  (1772E8 = 0x1772e8) - posicion, carril = 5 - carril. conv_trafico.py la localiza (la funcion que llama a 4EF880) y la
  ejecuta con tools/ps2/mips.py (interprete minimo del R5900): TAPD.DAT con las tablas de las 32 etapas al reves. Sus
  puntos fijos conservan la etapa de ida (0..29) en el campo +0: 478B90 solo toma los de la etapa actual (44DC50, el
  descriptor) salvo con la red o los rivales (F12), asi que sin red no salen (0 en TAPD: PROBABLE que 44DC50 de 30..59
  en esas etapas, como da ti en las de ida).
- Camiones (modelos 39, 40, 41 y 43; clases 0..2), HECHO 2026-11-13 (antes SIN PORTAR: se gastaban sin coche). En
  el arbol 1 los piden 46CE30 (preset 1 / 3: clase 0 0x27 / 0x29, clases 1 / 2 0x28; solo hay pesos de las clases
  0..2 en SEG23, la etapa 8 ALAS: 3, 3, 3 de 100) y los puntos fijos de 478B90 de SEG23 (40, 43, 39, 41, 43), SEG24
  (etapa 5: 43), SEG25 (etapa 1: 39) y SEG26 (etapa 9: 41); el 42 no lo pide nadie.
  - Modelos (tools/conv_trafico.py --camiones -> TRAFK0.DAT 39 y 40, TRAFK1.DAT 41, TRAFK2.DAT 43): registro de 4866C0
    (PC 650500; SLES-53998 0x3d6f38 / 0x3d6f68 / 0x3d6f50 / 0x3d6f98), cada LOD {descriptor, segundo descriptor}; el 39
    y el 40 (y el 41 y el 42) comparten la cabeza (descriptor del LOD 0 0x3d8128 / 0x3d8358): el 40 es la cabeza sola
    y usa los datos del 39. Cabeza como los coches (4ADBC0 / 4ADD80 / 4ADF00); remolque como second() de
    race_ghost_car.cpp en su marco: +0 carroceria (con el color), +18 freno (car+38 > 0), +14 pilotos solo con car+4 &
    0x300 (noche / tunel, apagados como los de la cabeza), +1C / +20 intermitentes (apagados), +68 lista (cerca), +10
    sombra (subida 0,1 m en el LOD 0, como la del resto del trafico) y ruedas de 4AD8F0 (+24 / +34 / +44 / +54, las del
    otro lado giradas pi en y, todas con el giro de car+40). Las mismas tiras y los mismos 5 LOD del original
    (vertices por LOD: 39 1852 / 1852 / 902 / 245 / 207, 41 1902 / 1902 / 977 / 262 / 140, 43 2118 / 2118 / 994 / 296 /
    151; LOD de 46DF50 con base -35 en el 0x27 y -25 en el 0x28); texturas en VQ como el resto (13 de 15, 13 de 15 y 17
    de 18: 191 -> 46, 201 -> 46 y 272 -> 64 KB). La pasada de sombra 4AD380 (d0+98 / d1+64 del LOD 0) es None en estos
    modelos: nada que dibujar.
  - Seguimiento del remolque: 470360 (PC race_traffic.cpp shadow_470360, en el control 47E780 tras mover el coche;
    demo/main.c traf_remolque): el enganche del cuadro anterior (+C70, mundo) al marco de la cabeza (T(+14) Ry(+2E)
    Rx(+2C) Rz(+30)), angulos de 449800 {-atan2(y, sqrt(z z + x x)), atan2(x, z), 0} (+C7C), el enganche nuevo a 12 m
    en esa direccion y car+C90 = T(+4 del segundo descriptor: (0, 1,4, 1,8) en el 39, (0, 1,45, 0) en el 41, (0, 1,35,
    3,2) en el 43) Ry Rx Rz; +CD0 la del cuadro anterior. Al abrir el coche (46E830, traf_remolque_nace) el enganche a
    (0, 0, 12) de la cabeza en el suelo (43EB60 con 0x100). El dibujo: car+B0 x car+C90 (second()).
  - Choques: la segunda caja de 520950 (la del remolque, en el marco de car+C90: 39 (1,244, 2,906, 11,335, -1,244,
    -1,4, 1,2), 41 (1,497, 2,118, 9,911, -1,497, -1,45, 2,005), 43 (1,376, 3,059, 11,182, -1,376, -1,35, -0,2)) en
    4F9000 con ia / ib = 1 (+C90 ahora y +CD0 en el cuadro anterior); la pasada de 4F9450 prueba cada caja de uno
    contra cada caja del otro (jugador y trafico entre si) con la cercania de los centros (+14) y el radio de 4866C0
    (+18: 13,82 en los de remolque, 5 en el 40) y separa por cada par que toca (formas_traf / contacto_cajas).
    SIMPLIFICADO: en el plano, como la caja del propio coche, sin el cabeceo del remolque. Efectos del golpe de la
    familia camion (0x27..0x2B) de 4F65D0 / 4F67A0 como ya estaban.
  - Recursos: solo en las carreras de OUTRUN2SP (camion_carga al empezarla, con el hueco de la union aun prestado para
    leerlos: lo que no son texeles va a un bloque del monton de su tamano y las texturas detras del HUD, antes de la
    zona del curso; se sueltan al volver al menu). Ver docs/DREAMCAST.md (Camiones de OUTRUN2SP).
  - PENDIENTE: en el juego de la consola (puntos fijos de SEG04 / 05 / 06 / 11, los 0x27 / 0x28 / 0x29 de 46CE30 y el 0x2B del
    INFARTO) siguen gastandose sin coche (no se cargan: decision de recursos); 470570 (la camara dentro de una caja del
    camion, alfa 0,5).
  - Verificado en Flycast (sp.sh, display 97; -DPRUEBA_CAMION: cada coche al azar es un camion, 39 / 41 / 43 / 40 por
    turno; -DDIAG_CAMION: angulos del remolque y choques): los cuatro modelos en BEAC y SANF con el remolque en su
    sitio, ruedas en el suelo y sombra; en las curvas de BEAC (radio ~400 m) la guinada del remolque sigue a la
    curvatura (1,1..2,1 grados, 12 / R = 1,7) y vuelve a 0 en las rectas, el enganche siempre a 12,00 m; choques del
    jugador con la caja 1 (roce a lo largo del remolque del 39, sin cambio de velocidad) y con la caja 0 del 40. Sin
    PRUEBA_CAMION (-DPRUEBA_NODO=8, ALAS; -DDIAG_TRAF): 46CE30 abre un 40 (clase 1) con el jugador en la posicion 16 y
    478B90 el 40 del punto fijo de la posicion 50 (carril 0), los dos 50 posiciones por delante.
- Encontrado de paso (juego de la consola): 487EE0 / 487F50 con 43F860 != 0 eligen el piloto y la acompanante por la
  licencia (+0x20, tablas 6549B0 / 6AF268 / 6549C8). CORREGIDO (2026-11-11, ver Coches: la pareja de la carrera).

### Frontal de recreativa de OUTRUN2SP (2026-11-11)

Fuente: arcade_attract.cpp (PC 4BEE60..4C4880, suceso 4) y race_end_modes.cpp (modo 10, 48B210 / 48AE90) linea a
linea, con las tablas del ELF (bloque FSP1 de SPFE.DAT; tools/conv_sp.py tablas_fe comprueba cada una):
| PC | PS2 | que |
|---|---|---|
| 5C7170 | 0x410c30 | 564 filas {token, primero, ultimo} (todas del banco 0x2E) |
| 5C8BE0 | 0x4126a0 | secuencia {0xB, 5, 6, -1, 4, 2, 8, 9, 0xA, 0} |
| [6898D8] | [0x373654] = 0x4126c8 | tabla de sonidos |
| 5C8D00 | 0x4127c0 | 16 registros de la musica {tema, pista del avance (0x21 + tema), 4 filas} |
| 5C91DC / 5C9204 / 5C922C | 0x412800 / 0x412828 / 0x412850 | coches {modelo, filas}, sus colores {argb, nombre, indice}, rotulos |
| 5C70A8 / 5C9408 | 0x410b68 / 0x410950 | color de partida, sonido de cada coche |
| 689B28 / 689C68 | 0x373938 / 0x373a78 | configuracion del coche de la Contrarreloj (2E002B..2E0040) |
| 689DA8.., 689E50.. 689E7C | 0x373bb8.., 0x373c60..0x373c8c | modo de juego (2 lados x 0x54), cursores, posiciones de las listas |
| 689F58 | 0x373d70 | filas del fondo 2E0045 |
| 5C9334 | 0x412920 | alturas del panel de la derecha (u16) |
| 689AAC.. 689AFC | 0x373890..0x3738e0 | tokens de la lista de coches, posicion, modelos y ranuras |
| 689AA0 / A4 / A8, 689FC4 / FBC / FC0, 84A9FC | 0x373860 / 64 / 68, 0x373874 / 7c / 78 / 79 | 5, 10, 100, 1, 3, 40, 0 |
| 689FA0 / FA4 / FB8 / FB4 | 0x373dbc / c4 / d0 / d4 | palanca: 0,2, 5, -40, 40 |
| 6898D0 | 0x37364c | velocidad 3 de los cambios de lado |
| 5C9434 / 30 / 38, 6281E8 | 0x213540..0x213590 (inmediatos) | barra del color: x = n x 4,37 + 563, y = 406 - n x 12 |
Constantes del PC que el ELF da en el codigo: 628064 = 0,5, 6280B0 = 2 (0x2110c0), 6280C4 = -1 (neg.s de 0x211684).
Escenas: el banco 0x2E entero se evalua en cada cuadro (comun/sprani.c, como 429460 / 429350 / 48B970; el grupo de
instancias de 427F70 / 4282B0 con sus modos y su estado; 4288C0 pone el cuadro: PS2 0x115290 escribe +0x50) en vez de
hornearlo (~290000 piezas). Texturas en atlas por pantalla con VQ (1910 KB en total), mascaras horneadas (318) por
(escena, cuadro, capa). La animacion (744 KB) va en el final del hueco de la union mientras dura el frontal.
Recorrido con un jugador (la secuencia; el codigo c es la funcion c + 1 de la tabla 0x3ee7b4):
"Elige coche" (funcion 6: lista de 10, Y sostenido el color con arriba / abajo, el coche 3D girando +0x64 por paso
como 475670) -> "Elige transmision" (7, 2E004F) -> "Elige modo de juego" (5: OutRun / Infarto / Contrarreloj; el
gatillo R cambia de lado: lado 0 rotulo "Recorrido OutRun2SP" -> preset 0 (csc_data_cvt), lado 1 "OutRun2" -> preset 1
(csc_data_2; 0x215fa4 -> 0x1760c0(preset)); variante
fila 0 -> 1, 1 -> 2, 2 -> 0) -> solo en la Contrarreloj "Elige configuracion del coche" (3) -> "Elige la musica" (9;
Y sostenido el balance musica / motor 2E0050) -> la carrera (11). B vuelve atras; B en el coche, al menu de la consola
(4C2C60 -> 4BF510: modo 32). La funcion 12 ("Single Play.", 2E004E) no sale con un jugador (el paso 0xB la salta).
La carrera usa lo elegido: coche, color, AT / MT, variante (0 = Contrarreloj), preset -> arbol de CURSO.DAT (0 -> 1, 1 ->
0 desde el 2026-11-13; antes 0 -> 0 y 1 -> 1: cada lado corria el curso del otro con sus reglas), tema.
Verificado en Flycast (tools/prueba/sp.sh): las cinco pantallas y la carrera con preset 1 (gatillo R), 0 "Out of
memory", RAM del menu igual que antes (939 KB libres).
- Mandos (el PC lee 4536F0 / 4536C0; el PS2 los filtra en 0x210988 / 0x210a00): 8 = B, 1 / 4 = START / A, 0x10
  sostenido = Y, 0x400 / 0x800 = cruceta arriba / abajo, 0x40000 = gatillo R. Eje 453720(0) = palanca X; la cruceta
  izquierda / derecha cuenta como palanca a tope (PROBABLE: 0x210a00 devuelve 0 para 0x40 / 0x80); 453750(0) = la X del
  paso anterior (PROBABLE).
- Constantes del PS2 (2026-11-11, antes SUPUESTAS): 5A29E0 / 5C9404 (casi cero de la velocidad de la lista, 4BEFE0 =
  PS2 0x211328) = 1e-4 / -1e-4 (lui / ori 0x38d1b717 / 0xb8d1b717; antes 1e-6); 5B4350 / 5B4354 (topes del suavizado de
  la palanca, 4C0BD0 = PS2 0x218a88: el suavizado 0x218a68 con 0x373dbc = 0,2 y el valor en 0x373dc0) = -40 / 40 (lui
  0xc220 / 0x4220 en 0x218c18 / 0x218ba0: si queda por debajo de 40, 40; antes 0). Los dos casi cero de dentro de la
  lista (4C0EB0) usan la misma constante.
- PROBABLE: el orden entre las escenas del grupo y las inmediatas (429530) de una misma capa; la lista (4C0EB0) es la del
  PC: la del PS2 (0x2113a0) difiere en detalles (pone 3 en 84A28F / 84A290 en los saltos de borde) y no esta comprobada
  linea a linea.
- 453320 = [0x54c108] (PS2 0x178478, su unica lectura 0x214e00): lo copia 0x178338 del byte +0x13 de los ajustes de
  [0x35a9e8] = 0x5493d0 (0x178108); 0x177310 / 0x177440 (al arrancar, 0x1077c4) lo ponen a 2 y nada mas lo escribe: en
  el PS2 sale 2 -> 8449E0 = 2, 844944 = 1 y la lista de 5 filas con los 15 recorridos continuos (presets 2 / 3: arboles
  csc_data_cvt_ren / csc_data_2_ren, 0x1760c0). HECHO en la Rev 2.9.9.c (ver Modos "sp" 1): las 5 filas, los
  arboles 291 / 290 y el resultado de la funcion 0x50 del suceso 0x188 (4AED50).
- Balance musica / motor (+21, 48B1B0 = PS2 0x1bd020 -> 0x5a2079, leido por 0x1bd030): en las carreras (modos 13 / 0x10,
  0x11f148 / 0x11f570) elige el registro de mezcla 0x3c15d0 + balance x 0x1C (7 floats: 1,0 0,5 0,5 0,8 1,0 0,8 0,7 /
  0,9 0,7 0,7 0,8 1,0 0,8 0,7 / 0,8 1,5 1,0 0,8 1,0 0,8 0,7; fuera de carrera 0x3c15b0: 0,9 0,7 0,7 0,7 1,0 0,8 0,7) que
  se copia a 0x531890: +0x18 x 127 a los comandos 0xA004 / 0xA010 del IOP (0x11f3e0), +4..+0x14 escalan el volumen por
  la clase del sonido (0x11f438, tabla de saltos 0x3c1270, 10 clases), +0 en 0x11f4f4. SIN PORTAR: falta saber que
  clase de 0x3c1270 es cada sonido de la Dreamcast (motor, derrape, voces, musica) para aplicarlo en sfx.c / musica.c.
  Localizado (2026-11-11): la clase es el grupo de canales del primer argumento de 0x11be88(grupo, parametro, valor)
  (con parametro 0, el volumen, pasa por 0x11f438): grupos 0 / 1 -> +4, 3 -> +8, 8 / 9 -> +0xC, 6 -> +0x10, 2 / 4 / 5 /
  7 -> +0x14 (tabla 0x3c1270); el frontal (0x11f570 desde 0x216fec / 0x217010) copia el registro del balance y pone
  los grupos 0 / 1 (parametro 2 = 0x73, volumen 0x13: el motor de muestra) y la musica con +0 (0x11f4d0, mas el ajuste
  por tema de 0x3c1628). Falta seguir que sonidos de la carrera piden cada grupo (71 llamadas a 0x11be88 desde
  0x11c8d0 / 0x11cc70 / 0x11d3a8 / 0x11d870 / 0x11dd40 / 0x11e338 / 0x11e994 / 0x11ebc8) y que hace +0x18 en los
  comandos 0xA004 / 0xA010 del IOP: SIN PORTAR.
- Textos de ayuda: en el PS2 son la barra de botones del objeto 0x5e08f8 (0x238f28 = 446D90 del PC) con sus pares
  {boton, texto}: 0x212048 (4 0x294, 8 0x295, 0x10 0x32C), 0x2165f4 (4 0x294, 8 0x295, 0x10 0x32D, 0x20 0x422) y 0x213604
  / 0x215068 / 0x21608c; sus metodos de aparecer / quitarse (PC 447000 / 447090 / 4470F0 / 446CF0 / 446A50) sin
  localizar en el PS2: SIN PORTAR.
- Cruceta: 0x210988 y 0x210a00 (los filtros de botones del frontal) devuelven 0 para 0x40 / 0x80 (CONFIRMADO); que la
  cruceta mueva la palanca (0x210a78 -> 0x1b4578: el eje del mando, [0x58a7d0 + mando x 0x70 + eje x 16 + 4]) depende de
  quien escribe ese eje (sin localizar): sigue PROBABLE.
- El fundido de volumen de la musica (4BF970 -> 42E020) es en el PS2 0x1001d8, que no cambia el volumen (ver Fin de
  la carrera): nada que portar.
- Vuelta desde el GAME OVER (modo 28 -> 10): el frontal se abre en cuanto el hueco de la union queda libre (tras la meta
  la union de la meta sigue alli unos cuadros, hasta curso_al_menu); fsp_entra no usa nunca el monton (las tablas de
  SPFE.DAT, ~850 KB, solo en el final del hueco de la union; sin el, no se entra). Las voces del modo (VOCES.DAT /
  VOCESH.DAT) se leen en el hueco sin ese final.
- SIN PORTAR: el mundo y la camara del coche (4BF2E0 / 4BF340, suceso 0x181 funcion 0xD; la
  Dreamcast usa la camara que gira del menu de la consola, PROVISIONAL); los textos de ayuda (84A320, 446D90); las
  voces 6..0x3F (4BEE60: solo con el idioma 0; en la PAL espanola no suenan); la configuracion del coche de la Contrarreloj en la carrera (+23, 48B190 =
  PS2 0x1bd000 -> 0x5a2078; la leen por 0x1bd010 0x176754, 0x18d7d4, 0x19b590 / 0x19b64c, 0x1a4938, 0x1b7254 / 0x1b72bc,
  0x1b9710, 0x1bbc40, 0x1c29c4, 0x1fc68c, 0x1fd684, 0x200174 y 0x21cc18 / 0x21ccc0: sin seguir); el balance (arriba);
  689C68 ([7C24BC]);
  red, varios jugadores y la pagina 84A218.

### Final de recreativa de OUTRUN2SP (modo 24, 2026-11-13)

Tras la meta del juego OutRun de OUTRUN2SP (19 -> 33 -> 24 -> 25): demo/final.c (logica y guion) y final24_* de
demo/main.c (RAM, modelos y dibujo); datos de tools/conv_final.py: FINAL.DAT, FINE0..9.DAT, FINPROP.DAT y STAFF.DAT
(nombres nuevos; regenera.py paso `final`). Fuente: race_ending.cpp / race_autoscene.cpp linea a linea, contrastados
en SLES-53998:
| PC | PS2 | que |
|---|---|---|
| 49A660 / 49A710 / 4527A0 | 0x1ba4d8 / 0x1ba5f0 / 0x1ba6a0 (tabla de modos 0x3f8e50) | init, control y salida del modo 24 |
| 452B10 | 0x19b6d0 | temporizador [0x5a2020] = 8 |
| 4527F0 | 0x19bae0 | columna = (450380(8) - 0xA) % 5, + 5 con los presets 0 / 2; variante 1: id = [0x3e9048 + columna x 4], juego de movimientos 1 |
| 5A4830 + id x 0x4C | 0x3e7910 + id x 0x4C | +0 escena AUTOSCENE, +8 15 recursos, +0x44 grupo de movimientos (4F2020), +0x48 coche |
| 638DF0 | 0x35c5c8 | escena 2D de cada final (banco 0x43 STAFFROLL_CVT: los creditos), capa 7, modo 1 (0x114b68) |
| 5A46A0 | 0x3e7780 + columna x 16 | el sitio: recurso y nombre (END_5A_STAGE.. de object_db_bin) |
| 4524E0 | 0x19b740 ([0x556f18], tabla de saltos 0x3e76a0) | estados 0..6 (ver demo/final.c) |
| 717D68 + escena x 12 | 0x3b5680 + escena x 12 | filas AUTOSCENE {1, ruta, 0} (\AS\as_e_05..as_e_11, as_SPE_*) |
| 4B6BD0 | 0x28baa8 | reloj del guion (1 / 60 = 0x3c888889; fin en cuadros / ritmo de la cabecera) |
| 4B5180 / 483E10 | 0x28bce0 / 0x17b540 | camara del guion: ojo, mira, balanceo, FOV = 2 atan(([pista] x 0,5) / (focal x 0,03937)) a +E0 / +EC / +130 / +A4 |
| 4B5670 / 4B5750 | 0x28c198.. | canales de los nodos (9 valores y la visibilidad) y su dibujo T(+10) Rz Ry Rx(+1C) |
| 4B5FF0 / 4B6320 / 448CD0 | 0x422438.. / object_db_bin | tokens de los nodos: piezas del coche BD0000.., ojos BD001C.., KEM BD0020, objetos (0xD0nnnn: obj_end_property) |
Lo que se ve (Flycast, tools/prueba/final.sh con un GDI de -DPRUEBA_PILOTO_MENU -DPRUEBA_NODO=13 -DPRUEBA_FIN=300
-DPRUEBA_SIN_TIEMPO; OUTRUN2SP preset 1, meta D en FLOR): GOAL y la escena de la meta (19) como antes, despues la plaza
de San Pedro de END_5E_STAGE (columna 3, final 6, guion 0x19 = as_e_07, 1200 cuadros) con la camara del guion, el
coche del jugador (F50) colocado por el nodo CAR1_BODY con sus ruedas, los creditos de 0x430001 (la imagen de la
camara de video: FINE, la bateria, AUTO, los nombres) en la capa 7 y la musica de la meta (MUS65) que sigue sonando;
al acabar los creditos el modo 25 (el mapa de la ruta), el 27 y la vuelta al frontal. 0 "Out of memory".
  Con la pareja (2026-11-13, la misma prueba): CHR_FAL y CHR_MAL con su cara, pelo, ojos y manos, de pie junto al F50
  con los movimientos de mot_E07 (planos de la cara de cerca, los dos andando y posando, los creditos encima); RAM al
  arrancar 1028 KB libres (la pareja del final va entera en el hueco de la etapa: FINCHR0D.DAT, sus motores de 128
  huesos y fin_morf); al entrar en el 24 el HUD sale de la VRAM (428600) para el sitio, la pareja y los creditos.
RAM y VRAM: el curso sale entero al empezar el 24 (curso_fuera; la etapa tambien: mundo_fuera, nada del curso se
dibuja ni se consulta hasta volver al menu); el sitio, sus objetos y FINAL.DAT en el hueco de la etapa (FINE3: 1003 KB
de 8116 tras subir las texturas), las tablas de STAFF.DAT en el hueco de la union prestado; VRAM libre 507 KB con el
sitio subido. Sin texeles en la RAM (van a la VRAM al leer).
Diferencias y SIN PORTAR:
- La pareja del final, HECHA 2026-11-13: FINCHRgg.DAT (gg = grupo de la fila +0x44) con CHR_MAL (0x10, bone.bin 5: 115
  huesos + 6 nodos de osage; ROBOT_HUESOS sube a 128 y los huesos con mas de 6 hijos llevan la lista detras, hijo() de
  comun/robot.c) y CHR_FAL (0xD, osage con la lista de amortiguacion por nodo de 0x290fd8: 0x36fb30 / 0x36fbb0 con las
  esferas 0x40f730 / 0x40f7b0, bloque OSA0 / OSA1), los movimientos de sus sucesos 1 (mot_E05.. de ANIMS.PS2) y los
  objetos de rob_disp: cara (tabla 0x40f478 / 0x40f4f0: hueso 0x19, FACE_*_DEFORUT_*), ojos (EYE_*_L / R con su posicion
  y giro, los nodos BD001C..F del guion: 514880; L / R en el orden de las cadenas de 0x4225c0), manos (HAND_*_DEFORUT_L
  / R en los huesos 0x27 / 0x28 y 0x38, y las de los sucesos 7: el tulipan, el ramo...). demo/final.c: 4B65B0 (robots),
  4B5B20 (poses con ranuras de nodo, canal 9 los oculta), sucesos 1 (robot_fin_mov: 4F2280 / 4F1D30 / 4F2520 / 4F1E30
  / 4ED890 / 4F1E60 y 514F60 al cambiar), 6 (514830: la cara y la de antes con sus cuadros) y 7 (514800), y cada paso
  5147D0 y 4F2520 + 515040 (robot_fin_calc). Nombres de los sucesos 6 / 7 a tokens en la conversion (4B5AF0: la letra
  de 0x378170 "FGG" + "AL" en +5 salvo CHR_MAL). Dibujo (demo/main.c robot_matrices / robot_dibuja con el robot en su
  work+10): la cara con la mezcla 405450 (posicion = w a + (1 - w) b, w = 1 - (+8 / +4) del 406800; la normal la de a:
  PROBABLE) entre las mallas del mismo numero en las dos caras (tabla MORF del RBT1); los ojos T(+20) Rz(+40) Rx(+38)
  sobre el hueso; el ramo (clave 0x208) con su color por vertice (multiplica la luz: PROBABLE). RAM: la pareja en el
  hueco de la etapa detras de FINAL.DAT; la de la carrera se vuelve a leer en la siguiente carrera.
- El fondo de los presets 0 / 2 (4522D0, [0x3e7820 + columna x 24]: {recurso, token, x, y, z, alfa}), HECHO
  2026-11-13: el registro 2 del cielo de la etapa (obj_course_obj_sky_east / maya / prin / flor / newy) en FINSKY5..9.DAT
  dibujado en T(0, -100, 0) antes del sitio (alfa 1 / 0,9: opaco en 452080, alfa >= 0,9). PROBABLE: con la prueba de z
  (el original lo dibuja sin ella, render state 7 = 0); no visto en Flycast (la prueba usa el preset 1).
- Del guion: el fundido / paso del tiempo (registro de tipo 5: 4AF520 pone el paso de los movimientos [842118] del
  cuadro siguiente; no es un fundido de imagen), los sonidos (sucesos 2 / 4: 4249F0), las puertas (canales de
  CAR*_DOOR_*) y las piezas animadas del coche: ninguno de los 10 guiones de la variante 1 (as_e_05, 07, 08, 09, 11,
  as_SPE_S01, S03, S04, S05, M09) los trae (contado en la conversion): nada que portar con estos datos.
- SIN PORTAR: las luces del final (4523E0: tres luces de 5A6AB8 por final y columna, 407CF0 / 407DD0 / 408390 /
  4083C0 / 407F40 / 408030 / 407D60: la pareja y el coche usan la luz de los coches de la demo, el sitio la de
  tools/modelo.py en los vertices), el humo KEM (4208A0, solo en as_SPE_S01: columna 5), las luces del coche del final
  (car+330 con los bits de las piezas visibles) y el bloqueo 0x10000 del final 6 a partir del cuadro 0x460.
- PROBABLE: las ruedas car+40 / 42 / 44 / 46 (giro x de TIRE_FL / FR / RL / RR) como el giro de las ruedas 0..3 de la
  demo y car+32 (giro y de BRCL_FL) como la direccion; K 0x6282c0 = 65536 / 2 pi. Los objetos de obj_end_property se
  convierten por registro sin la jerarquia de sus nodos (cajas, MOAI, PILLAR, OBENTO: un nodo).
- Las variantes 0 y 2 (Contrarreloj e INFARTO: 0x3e9070 / 0x3e9098 y 45BF30) no salen: con ellas la meta pasa directa
  al 25 como antes (final_columna).
- Los estados 0 / 1 (object_db y las cargas) se cumplen en el mismo cuadro: la Dreamcast lee de un tiron al entrar.

## Ultimo cambio verificado

Tiempo de juego de la licencia (2026-11-09): la licencia activa suma +0x10C += el crono total de la carrera (4505D0 =
[7D39DC]; SLES-53998 0x1cfdc0 = [0x5c5f48], licencia en 0x588668) al salir de la meta (modo 19, PS2 0x1b96bc), del
fin del tiempo (modo 20, 0x1b9b58) y al confirmar REINTENTAR / ABANDONAR en la pausa (0x249c5c / 0x24a110), con +3F4
|= 2; al volver a los menus se guarda en la VMU si hay cambios (4457B7 -> 416420). El crono total ya tiene el tope del
original (0x57E3F). No se suma en la demo de atraccion. Verificado en E (Flycast, menu real, OutRun, pausa ->
ABANDONAR -> si): `LICENCIA: tiempo de juego +863 cuadros` (el HUD marcaba 0'14"335 = 863 / 60,2), guardado en A1;
en el siguiente arranque otra carrera +259 = 1122 y la tarjeta muestra T.CONDUC. 0''00'18. Las millas OutRun (+0x24,
4EF550 / 4EF510 y la pantalla del modo 34) ya estan: ver Fin de la carrera (antes: SIN PORTAR; dependen de la
puntuacion (4B99D0 = [8447DC + coche x 4]), de 84BCF8 (4EF460 / 4EF440 desde race_traffic_score), de la bandera 84BCFC
(4EF430) y del contador car+0xE78; su codigo del PS2 no esta localizado (0x1a38f0 es el reinicio de la licencia y
0x1a41b4.. otra copia de +0x24, no la suma; la suma es 0x1f41d8).

Suelo del original (2026-11-09, ver Suelo del original: colision COLI0200): la altura del coche, del trafico y de la
camara sale de 43EB60 sobre la colision de la etapa y de la union (tools/conv_coli.py, demo/coli.c) en vez del perfil
de la ruta. Flycast: TULI (`-DPRUEBA_NODO=10 -DPRUEBA_FIN=4800`, s 440-530) antes el coche iba por debajo del puente
empedrado (perfil -2,3 m, calzada +1,8 m), ahora va por encima (capturas iguales con `-DSIN_COLI` salvo ahi); GHOS ->
BK09 -> IMPE -> meta BK13 y otra carrera (`-DPRUEBA_NODO=9 -DPRUEBA_RAMA=0 -DPRUEBA_FIN=400 -DPRUEBA_SIN_TIEMPO`): las
dos raices, el recentrado y la meta dan la altura de la calzada (diferencia con el perfil < 1 cm salvo donde el perfil
falla). Con el menu real (nav.sh): titulo, menus y carrera bien; al empezar la carrera 450 KB libres con las
colisiones (sus huecos solo existen en la carrera: reservados al arrancar dejaban sin RAM a MENU.DAT).

Memoria de video del curso (2026-11-09, ver DREAMCAST.md, Memoria de video del curso y del HUD): la meta BK13 no
cabia tras GHOS -> IMPE (VRAM troceada por `pvr_mem`: 253 KB libres en huecos, BK13 219 KB) y la carrera se paraba.
Ahora las etapas y uniones van en una zona propia de la VRAM (etapas desde abajo, uniones desde arriba; sin huecos),
la meta usa las texturas iguales de su etapa, el HUD de carrera sube con paleta de 4/8 bits las texturas de 256
colores o menos (sin perdida: mismos valores de texel; -254 KB) y la VRAM del curso se devuelve al volver al menu. Si
algo no cabe, la carrera sigue (fuera de la zona, echando del HUD lo que no se dibuja o con la textura rebajada).
Peor combinacion: de 29-31 KB libres a 290 KB (BK08/BK09 + IMPE) y 382 KB (IMPE + BK13). Lo que se ve no cambia
(capturas congeladas antes/despues: HUD identico). Medida en host: `tools/vram.py`. Prueba nueva: `-DPRUEBA_SIN_TIEMPO`
(el tiempo no se acaba, para llegar a GOAL por cualquier ruta) y `-DPRUEBA_VRAM_KB=n` (zona del curso pequena, para
probar el ultimo recurso).
Verificado en E (Flycast, COCHE06, PRUEBA_SIN_TIEMPO): las cinco metas hasta GOAL, GHOS -> IMPE -> BK13, SNOW -> IMPE,
CLOU -> TULI, INDU -> METR (BK11), SNOW -> RUIN y GHOS -> CAPE, dos o tres carreras seguidas cada una (con vuelta al
menu), sin texturas rebajadas; y el ultimo recurso con `-DPRUEBA_VRAM_KB=700` (GOAL con texturas rebajadas). Compila
sin avisos. Sin datos regenerados (no hacia falta cambiar ningun .DAT).

Piloto y acompanante (2026-11-09, ver Coches: el piloto y la acompanante): CHR_DR_M00 y CHR_DR_L00 con su esqueleto,
pesos y movimientos del original en el coche del jugador (todos los coches, vistas exteriores). Verificado en H (motor
en C contra el de Python: error < 1e-5) y en E (Flycast: 360 Spider, F40, derrape, choque, META; posturas iguales al
render de referencia; hoja en el scratchpad, chr/hoja.png); compila sin avisos; coste medido en DREAMCAST.md. CHR.DAT
regenerado en el dir de datos.

Suelos (2026-11-09, ver Suelos): lista, mezcla y z de cada malla de la escena por el estado del GS (tabla 7), mezcla
aditiva y alfa de vertice en lo que mezcla, mipmaps de las texturas cuadradas del suelo. Verificado en E (Flycast,
PRUEBA_AUTO, PALM, LAKE, ALPI, CAST, DESE, SNOW, METR y la union de PALM; hoja antes/despues en el scratchpad,
suelo/hoja.png): calzada de METR opaca (antes translucida y azulada, ahora recibe la sombra del coche), suelo lejano de
PALM sin centelleo de las rayas, arboles de PALM en la lista de recorte; compila sin avisos; VRAM del peor caso medida
(DREAMCAST.md, Suelos). Datos regenerados con conv_union.py.

Golpeados y vibracion (2026-11-09, ver Choques entre coches: Golpeados y Vibracion): el coche golpeado (476040) se
ve entero 30 cuadros, parpadea a 30 Hz (4AE5F0, +B53) y a los 120 cuadros se cierra (478340) y libera la plaza; los que
aparecen parpadean 60 cuadros (+B62 de 4704A0); vibracion del mando de SLES-53998 (0x1d0420 / 0x1023a0) en el Puru Puru.
Ademas el trafico ya no se recorta por la vista con un margen fijo de 4 m (el autobus asomaba y desaparecia en el borde):
el margen es medio coche + 1,5 m (el original no recorta por la vista). Verificado en Flycast (capturas en el scratchpad
fade/): `-DPRUEBA_JUGADOR -DPRUEBA_TRAFICO -DDIAG_CHOQUE -DPRUEBA_FADE=60` (golpe 17 a un coche a 14 m, el jugador
parado): DIAGFADE entero de b64 120 a 90, parpadeo (oculto 1/0 alterno) de 89 a 2, cerrado en el cuadro 120;
capturas: trompo entero a +10 cuadros, oculto/visible alternos y ausente a +124 (parpadeo_trompo.png).
`-DPRUEBA_GUION -DPRUEBA_EMBISTE` con `-DEMB_DESDE=2500 -DEMB_CONG=...` (congela tras el golpe al autobus, estado 19):
el autobus se ve entero en las congelaciones de +1..+15 sin desaparecer en el borde. Vibracion con el Puru Puru de
Flycast (`device1.2 = 3`): DIAGVIB golpe a otro coche grande 0,75 durante 5..12 cuadros (diferencia de velocidades x 25),
aterrizaje del estado 7 0,75 / 15 cuadros, cambio de marcha 0,5 / 4, arranque 0,3 / 10; se manda nivel 5 / 3 / 2 y 0 al
acabar o en la congelacion. Compila sin avisos.

Choques entre coches (2026-11-08): auditoria completa (ver Choques entre coches): cajas 520950, esquinas contra bordes,
reparto con la clase 5 del jugador, roces, estado 7 solo con cierre > 80 (o pesados), lado del original, choques
trafico-trafico, recuperacion 473F30 y vuelta 473D10, efectos SE del original. Verificado: banco (cuenta a mano),
demo sin avisos, Flycast.

Fisica (2026-11-08): auditoria completa contra el PC y SLES-53998 (ver Fisica: auditoria): error de los coeficientes
de ass_compulsive_move, suspension y cargas del original, volante del mando del original, cambio manual completo, curva de
par del manual (PAR2), ayudas de CheckWanderer y del muro, redondeo hacia cero. Verificado: banco, demo sin avisos, Flycast.

Distancia de dibujo (2026-11-08): LOD del trafico del original (46DF50 / PS2 0x1dd728), presupuesto del TA por tira
(formato de los parametros), reparto de la etapa como lo veria el jugador (cercano entero, luego por el LOD de 404B00,
con histeresis) y reserva ajustada. Verificado en E (DREAMCAST.md seccion 2, PARIDAD.md seccion Graficos).

Graficos (2026-11-08): tiras con la bandera ADC del VU1, nodos y matrices como 404D10, carteles orientados, sin
cs_ENV/cs_ENVBK (reflejo), planos 0,1/4000 y LOD de 404B00, recorte de la etapa por presupuesto desde lo mas lejano.
Verificado en E (ver seccion Graficos).

Trafico: densidad del original (2026-11-08). Tablas del ejecutable de PS2 (tools/conv_trafico.py `aparicion`,
bloque TAP1 de TRAFC.DAT), localizadas por el codigo que las usa:
- AREA estado 0xF (PS2 0x173bf4: `jal 4EFB50; lw a0, 0x80(s1); jal 4EFAF0; lw a0, 0x84(s1)`): el descriptor de tramo
  lleva la tabla de aparicion en +0x80 (PC +0x68; hasta 8 registros de 12 bytes) y la de velocidad en +0x84 (PC +0x6C;
  5 de 0x46). 4EFD20 (PS2 0x1f3a70) no se usa en el curso (+0x64 de csc_data_2 = 0); su rama de la union da el
  registro por defecto 6A5DC4 (PS2 0x36bfe8): 2 coches, separacion 20, carriles 2 y 3 prohibidos.
- Registro de aparicion: u16 posicion, +4 coches como mucho, +5 separacion (min 3), +6..+0xA pesos de clase que
  4EFB90 deja siempre en 0,0,0,0,100 (lo confirma PS2 0x1f3a3c), +0xB carriles prohibidos. Por etapa del curso (clave
  0..14: coches/separacion por tramo): PALM 5/15, 5/15, 3/15, 0/15, 4/15; LAKE 12/12, 7, 5, 2, 3; IMPE 12/15 al
  principio; las demas 3..10 coches con separacion 14..20.
- Velocidad: +0xC = 100 km/h del modelo en todas; 4F0030 pisa con constantes las velocidades por clase (+0xE = 250,
  250, 250, 290, 290, 310; +0x1C = 270,9 / 309,6 / 309,6 / 387 / 408,5 / 473 x 1 o 0,8) y los porcentajes (+0x2A..);
  +4 = 7 si el registro de aparicion deja 10 coches o mas, si no 10 (PS2 0x1f1ed8).
- Puntos fijos 478B90: tabla por etapa de 4EF880 (PC 6A6CE0, PS2 0x36d1d8), en PS2 0x18 bytes por punto (u32 etapa,
  u32 tipo de lugar, u16 posicion, i8 carril, u32 modelo +0xC, i8 color +0x10, usado +0x14; fin en etapa 0x42; el
  PC tiene 0x14). Un coche cuando el jugador esta justo en esa posicion. Modelos 30..43 (trafico, funcion 0x59) y
  0..29 (coches de carreras, funcion 0x55, como mucho 2): la demo solo tiene 30..38; los demas se gastan sin coche.
- Modelos al azar: 46CE30 con la clase 4 siempre: la lista de 7 (PC 64E198, PS2 0x36bf70) = 30, 32, 33, 34, 35, 36,
  38 en orden; carril de 478430 con los pesos por clase de 46CC10 (PC 5B3DE4, PS2 0x40b9c8: 4, 2, 3, 1, 1, 1, 1).
- Sin trafico en sentido contrario: todos los coches van con la orientacion de la carretera del sitio (470240);
  ninguna tabla lleva sentido. Dificultad: el trafico no lee la licencia (7C24C0) ni la dificultad; solo la variante
  780258 (3/4 red: densidad 5B3DF0 y x0,8 las velocidades; 2 Heart Attack) y el preset 78024C (modelos de 46CE30).
  La demo es la variante 1 (OutRun), preset 1 (0 hasta el 2026-11-13, ver Preset del curso): nada que cambiar con
  menu_dificultad().
- TRAFC.DAT no se cargaba: la demo leia registros de 52 bytes y son de 48 (sizeof trafc_t); ahora se leen bien las
  clases, cajas, aceleraciones (ACL1) y el modelo para el LOD.
Verificacion (Flycast, `-DPRUEBA_AUTO -DVER=1 -DDIAG_TRAF`, 80 s de carrera con el piloto, `DIAGTRAF` cada 1 s):
PALM antes 11 coches en la ruta todo el rato (media 10,4; dibujados 9,8), despues media 3,2 y como mucho 7 (dibujados
3,0; abre uno cada 15 posiciones, el punto fijo de la 170 con el modelo 31 en el carril 5 y el de la 390); LAKE (58 s
hasta agotar el tiempo) antes media 9,8 y siempre 11, despues media 3,9 y como mucho 10 (los 9 puntos fijos del
principio). PENDIENTE: el camino "Lead" de 472450 (470B30, 471E70, 471F90, 472320: los coches por delante del
siguiente al jugador ajustan su velocidad a la del jugador, con suelos de 250..310 km/h del modelo), 478640 y 46CDA0
en la union, los camiones 39..43 en el juego de la consola (en OUTRUN2SP HECHO: ver OUTRUN2SP, Camiones) y coches de
carreras de los puntos fijos.

Auditoria de inventos y checkpoint del original (ver arriba): ACEL 1, posicion de carretera m_pos (puntuacion, IA y
grua), rumbo adelante a 16 posiciones, ampliacion en el principio de la etapa con 44DC70 completo (KTE1), aparicion y
cierre del trafico del original. Verificado: compila sin avisos, banco de fisica, Flycast (checkpoint LAKE y DESE).

## Proxima tarea

Camara de carrera del original (sistema visible en todo momento y hoy escrito a mano).

## Graficos: agua, cielo de la union, z y tiempos (2026-11-09)

Probado con el piloto automatico (`PRUEBA_AUTO`, `PRUEBA_NODO`, `PRUEBA_SIN_TIEMPO`) y `DIAG_FONDO` (fondo magenta:
los huecos de la escena se ven):

- Agua: en LAKE (y en CLOU, DESE, TULI, METR, IMPE) se veia el color de fondo en el lago (bajo el puente y a la
  izquierda de la salida). Causa: faltaba el plano del agua. El descriptor de etapa del PS2 lleva en +0x5C un objeto
  del mismo recurso obj_course_obj_cs_<zona> que el de la pancarta (+0 de la tabla 0x3f9b90): LAKE 0x590003 (122
  vertices a y = -99, 3,8 x 3,3 km), CLOU 0x5d0003, DESE 0x610005, TULI 0x620006, METR 0x630007, IMPE 0x660005, las
  demas 0xFFFFFFFF; todos son planos a la altura del agua. `conv_union.objetos_etapa` lo anade a la etapa (SEG03, 07,
  11, 12, 13 y 16 reconvertidos; SEG13 +10 KB: hueco de etapa 7842 KB). Comprobado: sin pixeles de fondo en las
  seis. CORREGIDO (2026-11-11, ver Agua: el reflejo del original): ese plano no pinta color en el original (solo el
  alfa) y en la Dreamcast salia negro; ya no se dibuja. No era la PVS, ni la tabla de secciones, ni el recorte por nodos, ni el tope del TA
  (probado quitando cada uno), ni los registros 8 y 9 de cs_CS_LAKE que no se dibujan.
- Cielo en el fundido de la union: la lista transparente de la PowerVR ordena por la z de cada triangulo y los
  triangulos grandes de los dos cielos (y las nubes) se cruzaban: trozos rectangulares de un cielo sobre el otro. El
  original los dibuja sin z y en orden (4521C0). Ahora cada malla del cielo en la lista 2 va con una z fija creciente
  en el orden de dibujo, detras de toda la escena (`cielo_malla_orden`); con niebla en el cielo (CLOU) se deja la z
  (manda en la tabla de niebla). En la consola real esa z (1e-5, 100 km) quedaba detras del plano de fondo
  (ISP_BACKGND_D = 1e-4 de KallistiOS, `pvr_set_zclip`) y la PowerVR no dibujaba esas mallas: el cielo que se funde
  desaparecia a tercios (Flycast no recorta por el plano de fondo y no se veia). Ahora 1,2e-4 x (1 + i / 1024): entre el
  plano de fondo y la escena (lejos <= 4000 m: 1/w >= 2,5e-4) (2026-11-10, ver abajo Cielo en la consola real).
- z de lo coplanario (edificios, marcas de la calzada): 1/w salia de FSRRA en las tiras enteras delante del plano
  cercano (`dibuja_malla_directa`) y de una division en las demas (`envia`); con GEQUAL una calcomania y su pared o su
  asfalto por caminos distintos daban z distintas en el ultimo bit y parpadeaban. Ahora las dos con FSRRA.
- Niebla para cortar lo lejano: la niebla ya es la del original (scn_env_fog por etapa, mezclada en la union). Con sus
  densidades llega a 255/256 a: PALM 6,0-11,8 km, LAKE 5,9-7,8, ALPI 2,6-3,9, DESE 2,0, CLOU 1,1-5,5, INDU
  1,3-2,2, SNOW/TULI 3,7-6,7, GHOS 1,5, METR 4,7, RUIN 7,9-12, IMPE 2,1-6,6, CAPE 17 (exp2: x = 2,355 / d; exp: 5,545 / d); CAST y FORE no tienen. El
  cielo va sin niebla salvo en CLOU, asi que lo que queda entero en la niebla se ve como silueta del color de la
  niebla contra el cielo: quitarlo cambiaria la imagen. En CLOU el cielo (a ~1100 m de z en la esquina) no llega a
  255/256 antes que la escena. Sin corte por la niebla (probado: no cortaba nada).
- Tiempos (`DIAG_MS`: cada 30 cuadros MS, PRF por partes y MAL): coche y trafico enteros delante del plano cercano
  (esfera del modelo) van por `dibuja_malla_directa`; el recorrido de las mallas salta antes las de nodos no visibles
  (con prefetch); sin recargar globales por vertice. Misma imagen. Flycast (CPU de dibujo, media de 45 s desde la
  salida, `DIAG_SIN_CPU`): PALM 28,7 -> 25,8 ms, LAKE 25,7 -> 23,2, INDU 29,4 -> 26,9, METR 33,5 -> 30,7, CAPE
  25,4 -> 23,1 (solo indicativo: Flycast no cuenta la cache). Lo que queda: el coche (10 440 vertices, ~5,7 ms), la
  etapa (~11-13 k vertices, ~13 ms), el piloto (2,6 ms).

## Cielo en la consola real y niebla del limite de dibujado (2026-11-10)

Queja del probador (demo 30, consola real): "sigue habiendo glitches en el cielo y en algunas texturas, y me gustaria
que aplicaras en el limite de dibujado tenue niebla para que no se aprecie tanto el popping".

- Cielo del fundido de la union (consola real; ver arriba Cielo en el fundido de la union): las mallas del cielo que
  van a la lista transparente (el grupo que se funde en cada tercio de 40 cuadros) llevaban z fija 1e-5, detras del
  plano de fondo de KallistiOS (1e-4): la PowerVR no dibuja lo que tiene 1/w menor y en la consola ese cielo
  desaparecia. Ahora 1,2e-4.. (8,3 km), entre el plano de fondo y la escena. Flycast no recorta por el plano de fondo
  (por eso se vio bien al verificarlo).
- Cielo en carrera (consola real): el cielo rodea la camara (707-1938 m del ojo en las 15 etapas y sus uniones) y
  sus triangulos que pasan por detras se recortaban en el plano cercano de la escena (0,1 m): vertices a millones de
  pixeles (1500 m / 0,1 m x 426). La PowerVR calcula en coma flotante los planos de z, color y UV de cada triangulo
  desde sus vertices y con esas cifras pierde la precision (bandas, texturas del cielo rotas); Flycast rasteriza con
  la GPU del PC y no lo muestra. Ahora el cielo se recorta en w = 50 m (`CIELO_CERCA`, `cielo_malla`): lo que se ve del
  cielo esta a w >= ~700 m, asi que no cambia la imagen (Flycast: PALM congelado igual salvo 5 pixeles) y los
  vertices quedan por debajo de ~16000 pixeles. La escena sigue con 0,1 m (sus triangulos son de pocos metros).
- Texturas: sin rebajas ni grises en el arbol del modo OutRun (el unico que se juega: `curso_pedido` solo cambia con
  PRUEBA_CURSO): `tools/vram.py` con el coche de mas texturas (COCHE06, zona 1312 KB) deja >= 185 KB libres en todas
  las combinaciones (peor BK08 / BK09 + IMPE) y las cargas de Flycast dicen 0 rebajadas; la VRAM es la misma en la
  consola. PENDIENTE (no se juegan aun): 14 combinaciones de los arboles de las misiones pasan de la zona (peor BK241
  + LASV, 1598 KB: 286 KB de mas, mas que los 87 KB de fuera de la zona: iria con texturas rebajadas); entre -3 y -51
  KB caben fuera de la zona. Las texturas de las etapas: mipmaps todos cuadrados, UV pequenas (|u|, |v| <= 8), VQ
  con libro reducido en direcciones multiplo de 8: nada que la consola trate distinto de Flycast.
- Niebla del limite de dibujado: DESVIACION pedida por el usuario (no esta en el original). Ademas de la niebla del
  original (scn_env_fog, sin cambios), un velo tenue en el ultimo tramo de la distancia de dibujo de la demo, en la
  misma tabla de niebla de la PowerVR (`niebla_pon`): de 0 a NLIM_A = 0,3 (suave) entre NLIM_INI = 0,9 x lejos y
  lejos, la distancia de dibujo real (4000 m del original, LEJOS_PS2, o la del guion de la salida y la meta). Delante
  de 0,9 x lejos la tabla es solo la niebla del original: lo cercano y lo medio no cambian. DESVIACION actualizada en
  la demo 40 (el usuario: el velo "es molesto", llevarlo al limite de la distancia de dibujado): hasta la demo 39 iba
  de 0,65 x nlim_d a nlim_d, con nlim_d siguiendo (`nlim_paso`, ~0,3 s) a la distancia desde la que el reparto del TA
  quita nodos (prio_d, en la demo 39 la del reparto a 0,75 del tope: nlim_prio), y velaba lo de media distancia;
  `nlim_paso` y `nlim_prio` ya no estan. Color: el de la niebla de la etapa; en CAST y FORE (niebla del original negra y sin
  densidad) el del horizonte de su cielo (`horizonte_mira`: media de color del vertice x texel de los vertices del
  grupo 0 entre -5 y 14 grados; tambien es el color al que va la niebla del original en el fundido de la union hacia
  esas etapas, antes hacia el negro). Fuerza x luminancia del horizonte / luminancia de la niebla (hasta 1): en las
  etapas de noche (METR, GHOS: niebla clara, cielo oscuro) casi no hay velo (aclaraba los edificios contra el cielo
  negro). `-DSIN_NIEBLA_LIMITE` lo quita (para comparar).

## Fundido de entrada de lo que aparece (2026-11-11)

DESVIACION pedida por el usuario (no esta en el original): "para evitar el popping quiero probar una cosa: aplicar un
sombreado a la textura que va a cargar para que no aparezca de golpe, como una transicion". QUITADO por defecto
(2026-11-11, el usuario tras probar la demo 35 en la consola: no le gusta, "hasta el popping se veia mejor"): el codigo
sigue, solo con `-DCON_FUNDIDO_ENTRADA` (demo/main.c `fe_paso`, `fe_pon`, `fe_cab`); sin el, la imagen y el TA son los
de antes del fundido (sin la cabecera con especular ni sus 4 bytes por vertice, sin los 1,375 bytes por nodo).

De donde sale el popping (2026-11-11, medido en Flycast, PALM con el piloto desde la salida, `-DDIAG_MS -DDIAG_VTX`,
70 lineas de 30 cuadros; el tiempo de Flycast no es el de la consola, pero si la cuenta del TA y de los vertices):
- El limite de dibujo `lejos` (el lejano del original, 4000 m) lo acortaba el recorte de seguridad por CPU: 50 m cada
  30 cuadros si dibujo + logica pasan de 15 ms. PALM gasta 15,5 + 2,1 ms (mediana; 55,5 cuadros por segundo), asi que
  bajaba solo: mediana 2200 m, 1100 m a los 30 s. Lo de mas alla aparecia de golpe al acercarse (con la niebla del
  limite tapandolo a medias). Es lo que el usuario ve como poca distancia de dibujo.
- El reparto del TA (`nodos_visibles_todos`): la etapa tiene 630-710 KB de los 1728 del bufer de parametros (el coche
  ~600 KB, el trafico, lo 2D); se dibuja entero hasta `prio_d` (mediana 181 m) y mas alla por la clase de LOD del
  original (lo pequeno y lejano se quita antes). Es el tope: la etapa queda en 13000-14000 vertices (VTX_ETAPA_MAX
  20000 no llega a cortar). Subirlo pide VRAM: el bufer va doble (2 x 1792 KB) y la zona del curso ya va justa (peor
  BK08 / BK09 + IMPE: 185 KB libres en el modo OutRun, 19 en el INFARTO); no se toca.
- El LOD de 404B00 y la visibilidad precalculada son los del original / no cambian la imagen.
Cambio: el recorte por CPU acorta por encima de 22 ms (unos 45 cuadros por segundo, lo que pide el usuario: 40-50 en la
consola) y alarga por debajo de 18 (`CPU_MS_CORTA` / `CPU_MS_ALARGA`, antes 15 / 11,5). Flycast, PALM, misma prueba:
`lejos` 4000 m todo el rato (antes mediana 2200), 27100 vertices por cuadro (igual: los limita el TA), 56 cuadros por
segundo (antes 55,5), dibujo 15,5 ms (igual). En la consola, si el cuadro pasa de 22 ms la distancia baja como antes.
El fundido de entrada costaba ~410 ns por vertice que se fundia y 4 bytes mas por vertice en el TA.

- Que se funde: los nodos de la etapa, la union y la etapa siguiente que pasan de no dibujarse a dibujarse dentro de la
  pantalla: los que el reparto del TA vuelve a poner al acercarse (`prio_d` / `prio_p` de `nodos_visibles_todos`), el
  LOD del original (404B00), el limite de dibujo (`lejos`), la visibilidad precalculada al cambiar de seccion
  (`pvs_paso`) y los de una etapa o union recien subida en carrera. Por nodo (`fe_v`, 255 al aparecer, baja con el
  tiempo real: 0,4 s, `FE_US`, igual a 30 que a 60 cuadros por segundo).
- No se funde: lo que entra por el borde de la pantalla al girar (estaba fuera de la vista, `fe_f`: no salta), lo que
  aparece a menos de 12 m (`FE_CERCA`: la carretera de delante del coche), y lo que aparece todo a la vez: el primer
  cuadro tras el menu, la carga o REINTENTAR (mas de 0,1 s sin cuadros o ningun modelo que ya se dibujara) y los
  cortes de camara de la salida o del cambio de vista (mas de 1/4 de lo visible es nuevo). Ni las mallas sueltas de un
  nodo ya visible (el plano lejano de cada malla, la mascara de mallas del PVS: lo que quita no se ve), ni los objetos
  de la union (pajaros), el trafico o el cielo.
- Tecnica: desde el color de la niebla, no con transparencia (sin ordenar en la lista transparente ni ver a traves):
  las mallas del nodo llevan el color del vertice x (1 - k) y el color de desplazamiento
  (especular de la PowerVR, la cabecera de su textura con el bit del especular) = color de la niebla x k:
  textura x color x (1 - k) + niebla x k; la niebla de la tabla sigue encima pixel a pixel, asi que lo lejano lleva la
  suya y con k = 0 la imagen es la de siempre (sin salto al acabar). Se descarto la niebla por vertice (PVR_FOG_VERTEX):
  sustituye a la de la tabla (habria que calcularla por vertice y en los triangulos grandes no es la misma) y al acabar
  el fundido se veria el cambio. Color: el de la niebla de la etapa, o el del horizonte del cielo si es mas oscuro
  (noche de METR y GHOS); lo aditivo de la lista transparente sale del negro.
- Por que 0,4 s: a 200 km/h son ~22 m; lo que aparece a cien metros o mas ha acabado mucho antes de llegar, y por
  debajo de ~0,25 s se ve como un parpadeo y no como una transicion. Es tambien lo que tarda la niebla del limite en
  seguir al corte del reparto.
- Coste: las mallas que se funden van por el mismo bucle en ensamblador que las demas (`sq_vtx_fe`: el de
  `dibuja_malla_directa` con el color x (1 - k) y el desplazamiento puestos en cada vertice, los mismos valores que
  `dibuja_malla`); solo las de carteles y las que cruzan el plano cercano van por `dibuja_malla`, como las que no se
  funden. En el TA 28 bytes por vertice en vez de 24 (la cabecera con especular; `ta_cabe` / `ta_teo_tab[1]` lo cuentan:
  el presupuesto se respeta). Por cuadro, `fe_paso` solo mira los nodos nuevos y los que se estan fundiendo (mascara
  `fe_m`). RAM: 1,375 bytes por nodo (~4,3 KB por etapa, 3094 nodos en la mayor). VRAM: nada (la cabecera se copia al
  enviarla). Medida en Flycast (2026-11-11, relativa: el tiempo de Flycast no es el de la consola; PALM conduciendo,
  `-DDIAG_FUNDIDO -DDIAG_MS -DFE_US=2000000u`, el reloj en ns alrededor de cada malla que se funde en `dibuja_etapa`):
  ~750 ns por vertice que se funde con `dibuja_malla`, ~410 con `sq_vtx_fe` (con el reloj dentro); los vertices enviados
  son identicos bit a bit (comparados los 32 bytes de cada uno con los de `dibuja_malla` en mas de un millon).
- Prueba: `-DDIAG_FUNDIDO` (con `-DFE_US=2000000u` para verlo mejor) funde desde el magenta y escribe cada segundo
  "FE: <nuevos> nodos nuevos, <n> fundiendose": Flycast, PALM conduciendo 60 s (tools/prueba/conduce.sh): 30-135 nodos
  nuevos por segundo en marcha; en las capturas lo que aparece
  a lo lejos (edificios, palmeras, carteles, gradas) sale del magenta y la carretera de delante no. Sin "Out of memory".


## Cambios de etapa: cielo del fundido (2026-11-11)

Queja del probador (demo 31, consola real): "en algunos cambios de etapa hay glitches en el cielo y alguna
ralentizacion". Capturas cuadro a cuadro en Flycast (`-DDIAG_TRANS`: camara lenta alrededor de cada suceso del cambio
de etapa; tools/prueba/README) de PALM -> LAKE, con el GD normal y frenado (`-DDIAG_GD_LENTO`). Los tirones, en
docs/DREAMCAST.md (Cambios de etapa: tirones).

ENCONTRADO Y ARREGLADO EN EL CODIGO
- El fundido se cortaba al llegar a la etapa nueva. El fundido (cielo en tres tercios, niebla, luz y sol; 44A000 /
  451E90) no puede empezar hasta tener la etapa siguiente (su cielo y sus texturas) y en el original esta siempre.
  Aqui se lee del GD al soltar la etapa anterior: con el tiempo de lectura que emula Flycast (~1,4 MB/s) el fundido
  de PALM -> LAKE empieza en la posicion del original (s 5629); con un lector lento (`-DDIAG_GD_LENTO=1000` encima:
  ~0,5 MB/s) SEG03 (LAKE) llega en s 6296-6341, y SEG08 (CAST, 6,6 MB) a 69 m del principio de la etapa: el fundido de 120
  cuadros se quedaba en ~50 y al entrar en la etapa el cielo, la niebla y la luz saltaban de golpe a los nuevos
  (`niebla_paso` ponia `ent_fase` a 0), y el sol se quedaba a medias toda la etapa (solo cambia en la union). Ahora
  el fundido que no ha acabado sigue en la etapa nueva hasta su final (`ent_cruza`): el cielo viejo es el de la union
  (no se suelta hasta 300 m despues: 2 s de fundido son 160 m a fondo), la luz vieja la de la etapa que se dejo con el
  giro de su marco y el sol termina su cambio. DIFERENCIA que queda con un GD lento: el fundido empieza mas tarde que
  en el original (no hay cielo nuevo que fundir hasta leer la etapa); con la etapa leida a tiempo es el de antes.
- Orden entre capas del fundido (`cielo_grupo`, `cielo_lmax`): una malla transparente de una capa de abajo salia por
  encima de la capa de arriba mientras esta iba entera (alfa >= 0,9) en la lista opaca de la PowerVR. Las mallas de
  una capa suben a la lista mas alta de las capas de debajo. Solo cambia con cielos que lleven mallas transparentes:
  de las 62 etapas, SEG32 (arboles de las misiones, aun sin jugar); en el modo OutRun todos los cielos son opacos y
  la imagen es la misma.

ENCONTRADO, ARREGLADO EN EL CONVERSOR, FALTA REGENERAR LOS DATOS
(Rev 2.9.9.c: tools/regenera.py detecta los datos de antes de este arreglo y los rehace; ver "Rev 2.9.9.c: bandas en el
cielo en las bifurcaciones")
- Franjas lisas en la cupula del fundido (lo que se ve en las capturas: rectangulos y arcos de un color en el cielo
  intermedio de los tercios 2 y 3, en todos los cambios desde o hacia una etapa con la cupula de PALM: SEG00, 01, 03,
  17, 18, 26...). No es del dibujo: dos de las cinco tiras de esa cupula (grupo 1 del cielo, objeto +0x54) salian del
  conversor con UV (0, 0) en todos sus vertices. Sus UV vienen en diferencias de 16 bits (STMOD 2) y, cuando una
  diferencia no cabe, el fichero trae un STROW nuevo y otro UNPACK del mismo tipo que sigue en la direccion siguiente
  (la tira de 79 vertices: 16 + 1 + 62 UV). `ps2/vif.py tiras()` solo guardaba el primero, la cuenta no cuadraba con
  la de los vertices y `modelo._trozo` dejaba la tira sin UV: un color liso (el texel de la esquina). Ahora junta los
  UNPACK seguidos de un mismo flujo.
- No es solo el cielo: en datos/ hay 28965 mallas con textura y todas las UV a 0 en las 62 etapas y 262 uniones (PALM
  1372, IMPE 2686, SEG25 2794...). SEG00 regenerado con el arreglo: 846 de sus 1372 recuperan las UV (5000 vertices
  cambian; mallas, posiciones y colores identicos; el tamano del ORD7 no cambia). Son trozos del decorado que hoy
  salen de un color liso.
- HAY QUE REGENERAR etapas y uniones (tools/regenera_datos.sh: pasos seg*, union*, curso y los dos paquetes; horas,
  por la PVS) y rehacer la demo. Los huecos de RAM y la VRAM no cambian (mismos tamanos). Comprobado solo con SEG00 y
  SEG03 sueltos (sin PVS) en una copia de datos/: la cupula del cielo nuevo (tercio 3) sale lisa y sin franjas; la del
  cielo viejo sigue con ellas porque viene de la union (BK00, sin regenerar).

SOLO SOSPECHADO (sin tocar)
- Las mallas del cielo que van a la lista transparente llevan z fija (el orden de dibujo): la PowerVR interpola las
  UV con esa z y la textura queda afin, no en perspectiva, mientras la capa se funde (alfa < 0,9). En PALM no se
  aprecia salto al pasar de 0,9 (cupula fina); en cielos de triangulos grandes (LAKE: mallas de 8 vertices del
  horizonte al cenit) puede verse la textura desplazarse unos pixeles durante el fundido. Arreglarlo exacto pide
  z = k / w por malla con k en escalones de mas de 3x (hasta ~1e-11) y bajar el plano de fondo: sin consola para
  probarlo no se ha tocado lo que la demo 31 dejo bien.
- Texturas liberadas o sin subir con la PowerVR dibujando: repasado y no hay caso. La etapa se suelta 300 m dentro de
  la union y su zona de VRAM no se vuelve a escribir hasta leer la siguiente (segundos); la union igual; el cielo no
  se dibuja de un modelo sin tablas (`tv`), tampoco a medio subir. Las cabeceras guardadas del cielo (`cie_n`) se
  borran en cada `tex_suelta`. Color de fondo, plano lejano y tabla de niebla no cambian en el cuadro del cambio
  (la niebla de la etapa nueva en su posicion 0 es la del final del fundido).

## Agua: el reflejo del original (2026-11-11)

Queja del probador (consola real): "hay zonas que deberian mostrar agua y se ven negras" (lagos y agua bajo los
puentes). Reproducido en Flycast (`-DPRUEBA_AUTO -DPRUEBA_NODO=1`): el lago de LAKE bajo el puente y a la izquierda de
la salida, negro.

Causa: el plano del agua que se anadio el 2026-11-09 (descriptor de etapa +0x5C, `conv_union.objetos_etapa`) se
dibujaba con su textura del disco: 32 x 32, casi negra (media 12 / 255, unas rayas grises) y modulada por la luz.
En el original esa textura no se ve nunca. El agua es un efecto propio del PS2 (el PC no tiene estos campos: su
descriptor sigue en +0x5C con lo que en el PS2 va en +0x72), CONFIRMADO en SLES-53998:
- Descriptor de etapa: +0x5C el plano (obj_course_obj_cs_<zona>), +0x60 / +0x64 / +0x68 las tres capas del reflejo
  (registros 3, 4 y 5 de obj_course_obj_sky_<zona>: la media esfera de abajo, y de -1320 a +48, con el paisaje
  reflejado; las tres iguales salvo la V, desplazada 0,06 y 0,09), byte +0x6C el alfa de las capas, +0x6E / +0x70
  (s16) las posiciones de carretera en las que se dibuja. 38 de las 62 etapas lo tienen (las del lago, los rios y el
  mar: LAKE, CLOU, DESE, TULI, METR, IMPE, BEAC, NIAG, ALAS, GRAN, SANF, AMAZ, YOSE, MAYA, NEWY, PRIN, FLOR, EAST y sus
  vueltas de las misiones).
- Al cargar la etapa (0x11a1a8): 0x119ea8 cambia el material del plano (la textura por la de agua procedural del
  0x530780 +0xC: 64 x 64 PSMT8H con paleta, hecha cada cuadro sumando tres mascaras de ondas de 64 x 64 del ELF
  (0x344dd0 / 0x348dd0 / 0x34cdd0, en los bits 7, 6 y 5 del alfa) que se desplazan 0,003 por cuadro (0x11a250);
  ALPHA Cs, TEX1 0) y 0x119fb8 el de las capas (alfa del material (+0x6C >> 1) / 255, ALPHA 9 = Cs + Cd x As, TEST
  con la prueba del alfa de destino DATE / DATM 1).
- Cada cuadro, al final de la geometria de la etapa (0x172b68 -> 0x11a2e8): en la etapa con la posicion entre +0x6E y
  +0x70, en la union (car+5C != 0) pasada la posicion 0xC7 si +0x6E <= 0. El plano se dibuja con FBMSK 0x00FFFFFF
  (tabla 0x32b3b0[8], via 0x112330): solo escribe el alfa de la pantalla, la mascara de ondas (paleta: 0x80, 0x20 o
  0x40). Despues cada capa, alrededor de la camara como el cielo, sin z (ZTST ALWAYS) y solo RGB (FBMSK 0xFF000000),
  sale donde el alfa de destino tiene el bit 7; entre capa y capa 0x11a800 relee la pantalla como textura PSMT8H con la
  paleta de 0x530780 +0x10 y rota ese alfa (0x80 -> 0x20, 0x20 -> 0x40, 0x40 -> 0x80; paquetes del ELF 0x3128c0 y
  0x312990). Cada pixel del agua ensena una de las tres capas: el paisaje reflejado ondulado.

Arreglo (tools/conv_union.py `agua_de` / `agua_campos`, tools/conv_tramo.py `pon_agua`, demo/main.c `agua_dibuja`):
- El plano del +0x5C ya no se anade a la etapa (no pinta color en el original).
- La primera capa del reflejo va en el SEGnn.DAT de la etapa como grupo 2 del cielo (-258 en el campo del nodo), con
  la luz del entorno 0 del cielo (PROBABLE) y As = (+0x6C >> 1) / 255 en el alfa de sus vertices (PROBABLE: que el
  material de 0x119fb8 vaya en unidades de 1 y no de 128). CURSO.DAT lleva por etapa las posiciones +0x6E / +0x70, el
  alfa y si tiene agua (8 bytes mas por registro, antes de la union).
- Se dibuja cuando lo dibuja el original (posiciones y union como arriba; en la union, el de la etapa que se deja
  mientras siga cargada, PROBABLE: que el registro del curso no cambie hasta la etapa siguiente), en la lista
  transparente tras el cielo, con su z fija detras de toda la escena, mezcla ONE / SRCALPHA (Cs + Cd x As) y sin niebla
  (PROBABLE).
- DESVIACION (la PowerVR a 16 bits no guarda el alfa de destino ni lo prueba): sin la mascara del plano, el reflejo se
  ve donde no hay nada delante, recortado en el horizonte (y <= 0 en el marco del cielo; el plano siempre esta por
  debajo del ojo): el agua que cubria el plano y, si lo hay, lo de debajo del horizonte que no tapa la escena (en el
  original, el cielo o el fondo). Lo que hay bajo el plano del agua (orillas hundidas) se ve sin el reflejo encima
  (en el original Cs + Cd x As sobre ello). Solo la primera capa: sin la mascara animada no hay ondas (las tres capas
  son la misma imagen con la V algo desplazada).
- Datos: los 38 SEGnn.DAT con agua y CURSO.DAT regenerados; las uniones no cambian (el agua es del descriptor de
  etapa).

Cambio (2026-11-11, tras la demo 35 en la consola real): DESVIACION pedida por el usuario. El reflejo sin la mascara
"no queda logrado" (el paisaje reflejado entero, sin ondas, encima de lo que deja ver la escena); prefiere "una capa
azul como el mar" (el de BEAC, que dio por bueno). Ahora:
- En lugar del reflejo va el plano del agua del descriptor +0x5C (el registro de obj_course_obj_cs_<zona>, en el
  marco de la etapa: LAKE 122 vertices a y = -99, BEAC 368 a y = -51, SANF 214...) como grupo 2 de la lista 3 de
  SEGnn.DAT, sin textura (tools/conv_union.py `agua_de`, tools/conv_tramo.py `pon_agua`). demo/main.c lo coloca y lo
  mueve con la etapa (`modelo_coloca` / `modelo_desplaza`: el resto de la lista 3, el cielo, sigue alrededor de la
  camara) y `agua_dibuja` lo pinta en la lista opaca tras la geometria de la etapa (como 0x172b68 -> 0x11a2e8), con la
  z, la niebla de la tabla y la sombra del coche como el resto de la escena, en las mismas posiciones de carretera que
  el original (+0x6E / +0x70; en la union pasada la 0xC7 si +0x6E <= 0).
- Se ve justo donde se ve el agua del original: lo que el plano deja ver de la escena (en el original esos pixeles
  llevan el reflejo; lo que tapa el plano, las orillas hundidas, tampoco se ve alli). Lo que queda delante (las
  mallas del lago de LAKE, a -95..-98) se ve como siempre.
- Color: el original no tiene un color del agua (el plano no pinta color, FBMSK 0x00FFFFFF, y el reflejo es una
  imagen distinta por zona: por ejemplo noche en METR, desierto en DESE). AGUA_COLOR (demo/main.c) = 0x4064A3, el
  azul del mar de BEAC con el reflejo en la demo 35 (mediana de su superficie en la captura de Flycast: 64, 100,
  163), el que el usuario aprobo; la niebla de la etapa lo aclara con la distancia como a lo demas.
- Datos: los 38 SEGnn.DAT con agua (el plano en lugar de la capa del reflejo; sin la textura de esta) y SEGS.PAK;
  CURSO.DAT no cambia. SEG03 regenerada con estas herramientas: igual que la anterior salvo el grupo 2 (31 tiras, 122
  vertices, sin textura) y 4 texturas menos (las del reflejo); misma ruta, nodos, carteles y visibilidad. Flycast: el
  lago de LAKE a la izquierda de la salida y bajo el puente, azul y sin negro, 0 Out of memory.

### Agua: ondas y tinte por zona (2026-11-16)

El color plano de AGUA_COLOR se veia igual en todas las etapas: el lago de LAKE (reflejo del original 42,58,69), el
de METR de noche (20,19,15) y el de DESE (218,219,207) salian con el azul del mar de BEAC, y el agua no se movia.

- Textura: `demo/main.c agua_tex_datos` genera al arrancar una 64x64 ARGB4444 twiddled con mipmaps (11 KB de VRAM,
  0 bytes en el GD): tres senos de frecuencia entera (4/2, 9/-5 y 14/11 sobre u,v) y ruido de 1 texel. Al repetir no
  tiene costura. Se sube una vez con `agua_prepara` (fuera de la zona del curso: el peor caso BK09 + IMPE, 185 KB
  libres, queda en 174 KB).
- UV: `agua_escala` mide el plano de la etapa (122-1751 vertices) y elige la escala para una vuelta cada 36 m, porque
  las UV del original van de 0 a 15 en 5 km (en NIAG, 0 a 1 en 3 km). El desplazamiento es `crono * 0,006` en u y
  `crono * 0,0035` en v: unas olas de unos 15 m/s, sin coste de geometria (los mismos vertices y triangulos).
- Color: `agua_tinte_de` usa la media de las 4 texturas del reflejo de la zona en el descriptor +0x60.. del original
  (PSMT8/PSMCT32 de 32x16 a 128x64, alfa 254), aclarada x1,4 y con tope 240 porque en el PS2 el reflejo se suma a la
  escena (ALPHA 9 = Cs + Cd x As) y aqui sustituye al color plano. Valores: LAKE 3B5161, BEAC 4064A3 (se queda el
  azul aprobado por el usuario; el reflejo medido es 141DB3), METR 1C1B15,
  DESE F0F0F0, CLOU F0F0F0, IMPE F0C0A0, NIAG 6C92BE, ALAS 27537B, GRAN 3E4553, SANF C4C1E4, AMAZ 7677A0,
  YOSE 233F6F, MAYA 2B2923, NEWY C4B9BA, PRIN 9DCAE4, FLOR 5C829B, EAST 4786AF, CAPE ABDFE8, PALM B98F6D.
- Comprobado en Flycast (LAKE, `-DPRUEBA_AUTO -DPRUEBA_NODO=1 -DDIAG_CONGELA=180`, con A): el agua pasa de banda
  lisa azul (mediana 106,134,181; desviacion de luma 12,2) a agua con ondas y tinte de lago (93,107,116; 15,7). En el
  mismo cuadro no hay costuras de tiling; el patron se apaga con la niebla como el resto.
- Lo que no se toca: el reflejo real (3 capas del cielo y la mascara de alfa de destino) sigue fuera: la PowerVR a 16
  bits no guarda ni prueba el alfa de destino. Las mallas de agua de la escena (lagos, cascadas: 64x64 a 512x256,
  VQ y mipmaps) y el brillo aditivo del mar de PALM (64x64, alfa 47/255, 8 mallas) se siguen dibujando como antes.

### Agua: color solido por zona (2026-11-17, revision 4)

El usuario pide un unico color plano por zona, sin textura, UV, scroll, niebla, reflejo, espuma, destellos ni tinte por
vertice. Se sustituye el color unico de la revision 3 por la tabla `agua_color_de` de `demo/main.c` (una entrada por
zona; lo que no este en la tabla usa el azul marino 0xFF0E2248). PALM y BEAC conservan el azul aprobado 0xFF4064A3.

| Zona | Nombre | Color | Zona | Nombre | Color |
|---|---|---|---|---|---|
| PALM | (aprobado) | FF4064A3 | BEAC | (aprobado) | FF4064A3 |
| LAKE | jeans | FF3E5A78 | NIAG | aguamarina | FFA8E8E0 |
| METR | noche | FF131A2E | IMPE | cobalto | FF3B4A9C |
| DESE | cerceta | FF0E6B66 | CAPE | acero | FF6B7C8A |
| CLOU | resto | FF0E2248 | TULI | resto | FF0E2248 |
| ALAS | resto | FF0E2248 | GRAN | resto | FF0E2248 |
| SANF | resto | FF0E2248 | AMAZ | resto | FF0E2248 |
| YOSE | resto | FF0E2248 | MAYA | resto | FF0E2248 |
| NEWY | resto | FF0E2248 | PRIN | resto | FF0E2248 |
| FLOR | resto | FF0E2248 | EAST | resto | FF0E2248 |

- Implementacion: `agua_color_de` devuelve el color de `etapa_t.zona`; `agua_malla` lo escribe en el color de todos los
  vertices (`pw[i].c`). La cabecera `agua_cab` sigue siendo `pvr_poly_cxt_col`, `PVR_FOG_DISABLE`,
  `PVR_DEPTHCMP_GEQUAL`; no hay textura, UV, scroll ni mezcla por vertice.
- PALM normal no lleva plano de agua en CURSO.DAT (`agua=0`; su mar son las mallas aditivas de la escena). La entrada
  PALM queda para sus variantes. CAPE normal (SEG15) tampoco; las misiones con SEG48 tienen un unico plano de 4
  vertices a -887,9 m, fuera de la ruta (no se ve desde la carretera).
- Comprobado en Flycast con las pruebas automaticas directas (`-DPRUEBA_AUTO -DPRUEBA_SIN_TIEMPO -DPRUEBA_CURSO=...
  -DPRUEBA_NODO=...`, y `-DPRUEBA_AGUA_S` para empezar en la franja de agua). GDI de prueba con `_trabajo/datos` (el
  juego con los planos de agua); `datos_r44` y `gdi_unificado` no se han tocado. Pixeles exactos del color en las
  capturas: LAKE 13.946, BEAC 4.225, NIAG 8.949, METR 1.521, IMPE 642, DESE 542, CLOU (resto) 8.157.
- Capturas y tabla final: `_trabajo/agua_zonas/AGUA_ZONAS_CAPTURAS.png` y `_trabajo/AGUA_ZONAS_INFORME.md`.


## Bifurcaciones: franja a escalones sobre el horizonte (2026-11-11)

Queja del usuario (demo 35, consola real): "en el cielo de algunas bifurcaciones hay texturas que se mezclan y no se
ven bien" (foto: en una union, a la izquierda, una franja azul morada oscura a escalones encima de los montes, con el
resto del cielo claro). Reproducido en Flycast al final de LAKE (`-DPRUEBA_AUTO -DPRUEBA_NODO=1 -DPRUEBA_FIN=900`,
hacia la union de CAST / FORE).
- Que es: no es el cielo (el de LAKE y el de su union BK01 son la misma cupula de nubes de 128 x 128 y el gradiente de
  32 x 32 del fundido, sin montes) ni el fundido entre cielos (en ese tramo aun no ha empezado), sino los montes de
  fondo de LAKE: 3 tiras del nodo 230 (registro 1, nodo 11 del original: esfera de 906 m) con la textura de 256 x 32
  de la cordillera (roca gris azulada con nieve; el cielo lila de abajo es alfa 0), a 0,5-1,4 km de la carretera. Es
  de la lista de recorte en el original: TEST_1 del GS con ATE, ATST GEQUAL y AREF 0x40 (0x80 = 1), sin mezcla; la
  conversion la lleva igual (ARGB1555 VQ, el alfa de 1 bit identico texel a texel al del original).
- Por que se veia a escalones: la PowerVR compara el alfa filtrado con PT_ALPHA_REF y KallistiOS no lo pone (queda el
  de arranque, 0xFF; Flycast igual): solo pasaba el alfa 1 despues del filtro bilineal, asi que el borde de cualquier
  textura de 1 bit seguia la rejilla de texeles (escalones de un texel, medio texel hacia dentro) en lugar del contorno
  suave que da el GS con el alfa filtrado >= 0,5. En la cordillera, estirada 1,4 km en 256 texeles, eran escalones de
  varios pixeles: la franja a bloques.
- Arreglo (demo/main.c, tras `pvr_init`): PT_ALPHA_REF = 0x80 (0,5), el AREF 0x40 del original. Todos los materiales
  con prueba de alfa de las etapas tienen ese mismo estado (888 en PALM, LAKE, INDU, SNOW, METR y BEAC: GEQUAL 0x40),
  asi que vale para toda la lista de recorte (hojas de los arboles, vallas, carteles). Flycast: la cordillera junto a
  la bifurcacion de LAKE con su silueta de montes (antes una franja a escalones) y las hojas de los arboles sin bloques.
- Lo que no cambia: la cordillera se sigue viendo alli (azul gris y oscura por la luz de la etapa, 0xC2D2F4 en sus
  vertices), como en el original si la dibuja en ese tramo. PENDIENTE: la lista de nodos por seccion del PS2
  (cs_CS_LAKE_bin) nombra ese nodo en 28 secciones sueltas (74..519 de 677) y su formato sigue sin descifrar (la demo
  usa la visibilidad precalculada propia, que no quita lo que se ve).

## Bifurcacion: el rotulo <- FACIL (2026-11-11)

Queja del usuario (demo 35): "falta el letrero de dificil al elegir ruta". Buscado en SLES-53998:
- El unico rotulo de la bifurcacion es la escena 2B0000 del banco 0x2B (GAME_CVT): una sola capa de footage (textura
  93 de 256 x 64 con "<- FACIL" en espanol, "<- EASY" en el XST ingles) con su eco que se desvanece, bucle de 60
  cuadros. No hay otra: ni en las 124 texturas del banco 0x2B ni en las 135 del 0x2C (ETC_CVT) hay un "DIFICIL" /
  "HARD"; las otras escenas del banco 0x2B que crea el ELF (jal 0x114b68: 2B000E, 2B0040/41/45/4A/4C/4D/50/5C) no son
  de la bifurcacion; los textos de TEXT.PS2 con DIFICIL / HARD son los de la dificultad de las opciones (11, 12). Los porticos de la union llevan los nombres e iconos de los destinos (BUNKI), sin facil / dificil.
- Cuando sale: 4BB890 (PC) = PS2 0x1c50c0, llamada solo desde NAVI_PUB en la variante 1 (OutRun) y el modo 16
  (0x1bd534): con el preset [0x44350c] 0, 1 o 4; en el nivel 0 (car+0x68) se crea al llegar car+0x102C a 0,65 y se
  suelta en 0,72; en el nivel 1, 0,70 y 0,75 (constantes 0x3f266666 / 0x3f3851ec / 0x3f333333 / 0x3f400000, c.ole.s);
  ni en los niveles 2..4 ni en el INFARTO o la Contrarreloj. car+0x102C la pone 4A8330: fila / (filas del tramo de
  43D470 + 1 + 0xF2) en la etapa y (filas + 1 + fila de la union) / lo mismo en la union.
- La Dreamcast lo sacaba en todas las variantes y bifurcaciones desde 450 m antes de la separacion hasta elegir rama
  (PROVISIONAL). Ahora como el original (demo/main.c `fac_on`, con `tr_fila` para la fila y las filas del tramo). No
  se anade un "DIFICIL" que el original no tiene.

## Bifurcaciones: texturas que se cruzan en el cielo (2026-11-12)

Queja del usuario (consola real): "en las bifurcaciones hay texturas en el cielo que se cruzan y no deberian" (tras
fbfe7b6, que arreglo la cordillera de LAKE).
- Cielo: repasado contra 4521C0 / 452080 / 451B40 / 451DD0 / 451E90 / 44C420 (PC) y los descriptores de SLES-53998.
  En las 30 etapas de los arboles 0 y 1 el descriptor da +0x48 = registro 1 (reflejo), +0x4C = ninguno, +0x50 =
  registro 2 (grupo 0) y +0x54 = registro 0 (grupo 1) de obj_course_obj_sky_<zona>; los registros 3..5 son la media
  esfera de abajo (agua) y no se dibujan. Las BK00..BK29 llevan en su lista 3 el mismo cielo que su etapa (mismas
  mallas, texturas y lista), todo opaco salvo FLOR (BK28 / SEG32: capa transparente del grupo 0). Orden de capas y
  tercios del fundido como 4521C0 (nueva 0, nueva 1, vieja 1, vieja 0; capa 1 = registro viejo 7D31DC, capa 0 =
  7D3188). Sin cambios.
- Lo que no era como el original: la geometria de las etapas en la union. El original solo dibuja la de una etapa,
  la del registro 7D3188 (44DE10 = PS2 0x1725d0), con el factor 7D3184 (44C530 = PS2 0x176bd0, mismas constantes
  0x84 / 0xF0 / 0,2 / 0,08): al pasar el coche la posicion 0x18 de la union ya en su rama (4515B0: bandera 0x20 con la
  0x80 de la colision; 44E590 = PS2 0x173480 cambia de registro y pone 7D3184 = 0) la etapa que se deja deja de
  dibujarse; la siguiente, leida, sale con 0 hasta la posicion 0x84, crece desde el suelo de su marco (escala y) en
  los 200 m siguientes (car+0x2B0: velocidad por cuadro x 0,001, km; 4A1140) y es 1 desde 0xF0; < 0,1 no se dibuja.
  La demo dibujaba la etapa que se deja hasta soltarla (300 m dentro de la union: sus montes de fondo, la cordillera
  de LAKE, seguian en el cielo de la union) y la siguiente en cuanto estaba subida (sus montes salian de golpe en el
  cielo de la union, con el fundido de los cielos a medias o antes de el). Arreglo: demo/main.c `geo_paso`,
  `geo_visible`, `geo_vista` (tambien para el plano del agua, que el original dibuja dentro de 0x1725d0).
- Aproximacion que queda (la de siempre de la demo): la posicion de la union es (s - s_union) / m_pos (traf_lugar);
  en la bifurcacion BK_PALM las secciones son de ~8,03 m (pos 0x18 a 199 m, 0x84 a 1063 m, 0xF0 a 1929 m de la
  colision) y m_pos de la etapa anda por 7,9-8,1: unos metros de diferencia. car+0x2B0 se lleva con la velocidad del
  coche (yo.v / 60,2 por paso).
- La union (cs_BK_PALM) por registros como 44E290: portado el 2026-11-13 (seccion siguiente).

## Bifurcaciones: registros de la union (2026-11-13)

Repaso de la queja "texturas en el cielo de las bifurcaciones que se cruzan" (tras fbfe7b6 y 42f41e8) contra
race_area_stage_draw_44e290 (PC) = PS2 0x172ba0 (llamada desde 0x172410 = 44F120, antes o despues de 0x1725d0 segun
car+0x5C, como en el PC). Lo que dibuja de la union (registro [7D2E78] << 16 = PS2 [0x5491600], matriz 7D3190):
- En la union (car+0x5C != 0, s3 = car+0x64): el tronco (registro 0) si la posicion es < 0xE4; la rama (registro 1 si
  451350 = PS2 0x1cfa40 da 0, 2 si da 1; nada con 2, sin rama aun) si la posicion es >= 0x1D. Antes de la 0x1D no hay
  ninguna rama.
- En la etapa (car+0x5C = 0): con la posicion < +0x72 del descriptor (PS2; +0x5C en el PC) la rama tomada en la union
  de antes (451350 del nivel anterior, v - 1) y no el tronco; entre +0x72 y +0x74 nada; pasada la +0x74 (PC +0x5E) el
  tronco de la union que viene y los porticos de los carteles (0x670000). Valores de SLES-53998: arbol 0 PALM 0 / 500,
  LAKE 100 / 563, ALPI 100 / 575, CAST 90 / 570, FORE 64 / 535, DESE 100 / 550, CLOU 100 / 550, INDU 100 / 550, SNOW
  100 / 625, GHOS 100 / 530, TULI 100 / 570, METR 50 / 600, RUIN 100 / 580, IMPE 60 / 580, CAPE 70 / 538; arbol 1 BEAC
  100 / 530, SANF 30 / 595, YOSE 70 / 550, NIAG 100 / 570, SEQU 100 / 550, GRAN 0 / 570, MACH 100 / 550, LASV 50 / 600,
  ALAS 100 / 525, AMAZ 100 / 500, EAST 60 / 600, MAYA 100 / 620, PRIN 100 / 580, FLOR 70 / 530, NEWY 100 / 520.
- La demo dibujaba la union entera en cuanto estaba subida (2500 m antes del final de la etapa: el tronco desde la
  posicion ~410 de PALM en vez de la 501), las dos ramas hasta elegir (la eleccion de la demo cae en la posicion
  ~10-18 de la union) y, desde el principio de la etapa siguiente hasta soltar la union (300 m), otra vez el tronco y
  las dos ramas (cur_rama vuelve a -1 al cambiar de etapa). Arreglo: CURSO.DAT lleva +0x72 / +0x74 de cada etapa
  (tools/conv_union.py union_pos, registro de 684 bytes) y demo/main.c `uni_regs` decide los registros de cada cuadro
  (nodos_visibles: bits 8-9 del nodo, tools/modelo.py rama).
- PROBABLE: los porticos de los carteles (0x670000, obj_course_obj_bk_palm, tools/conv_union.py carteles) van en el
  fichero sin registro propio y la demo los dibuja con el tronco; el original los dibuja en toda la union (bp == 0) y
  en la etapa pasada la +0x74. Solo cambia pasada la posicion 0xE3 de la union, con los porticos ya muy detras del
  coche.
- Aproximacion que queda: la demo suelta la union 300 m dentro de la etapa siguiente (UNION_LIBERA; memoria); el
  original dibuja la rama hasta la posicion +0x72 (~400-800 m), detras del coche.
- Barrido (Flycast, PRUEBA_AUTO con PRUEBA_RAMA, una captura cada 2 s): las 20 bifurcaciones con sus 40 ramas de los
  arboles 0 y 1, con la union por registros: en todas la etapa que se deja deja de dibujarse en la posicion 0x18-0x1A,
  la siguiente no sale antes de la 0x84, los registros de la union y el fundido de los cielos (tres tercios) como el
  original; sin texturas grises, rebajadas ni "Out of memory". Lo que se ve en el cielo de algunas uniones es geometria
  de la etapa siguiente que ya sale desde su seccion 0: las crestas de SNOW desde la union de DESE y el techo de roca
  de MAYA (nodos 194 / 195 de cs_CS_MAYA, radios de 1013 y 2436 m) desde la de LASV; sigue igual en la etapa y con la
  PVS de la seccion 0 de la etapa siguiente desde la union (como la lista de nodos de la seccion 0 que usa 44DE10 en la
  union, probado y quitado: no cambia nada).

## Edificios palidos en la salida de PALM (2026-11-13)

Demo 38 (Flycast Android, salida de PALM): los edificios de la derecha calle abajo, de unos 100 m en adelante, salian
palidos, azul y blanco, como sin textura. No era la niebla: ni el velo del limite de dibujado (igual con
`-DSIN_NIEBLA_LIMITE`) ni la del original (scn_env_fog de PALM: densidad 0,0002, nada a 130 m); ni la VQ (8283e33), ni
texturas rebajadas o grises (0), ni el LOD de 404B00 (igual sin el), ni un LOD del original con otras texturas (las
tiras de PS2 solo traen el LOD 0). Eran huecos: con el ojo en la seccion 1 de la PVS de SEG00, que como las dos
primeras y las dos ultimas de cada fichero llevaba todas las mallas, la etapa entera estaba en la vista (71131 vertices
para ~16000 del tope de la etapa) y el reparto del TA (DREAMCAST.md, Presupuesto) quitaba lo de mas de 76 m que no
fuera grande: los bajos y las tiendas de esos edificios (nodo 548, opaco). Sus fachadas con ventanas (nodo 550, lista
de recorte: las ventanas llevan alfa 0 y el original las dibuja asi, se ve lo de detras) se quedaban flotando con el
cielo a traves y por debajo. Arreglo (tools/pvs.py y pvs_paso, DREAMCAST.md, PVS: puntas de la ruta): sin secciones
llenas (la ruta prolongada 32 m por cada punta) y las fachadas de recorte tapan donde su textura tapa; la salida queda
en 56506 vertices en la vista y los edificios salen enteros como en el original. Sigue siendo mas de lo que cabe:
alli el reparto aun quita (lo pequeno y lejano primero).

## Revision 44: pausa, reintento, cielo y fotos de la consola real (2026-11-13)

Quejas del usuario sobre la revision 43 (video de Flycast en el PC, COAST 2 COAST, y fotos de la Dreamcast real).

- Pausa: "A y B descentrados" (CORREGIDO). La ventana de 48C5F0 del PC = SLES-53998 0x271ef0..0x272210: texto ELEGIR
  (0x294) en (x + 45, y + h - 38), ATRAS (0x295) acabando en x + w - 45 (widget +0x960 con +0x480 = 2, a la derecha) y
  los iconos (0x2720f8 / 0x272188, tabla 0x41f368 de {escena, primer cuadro, ultimo}: ATRAS la entrada 1 = 0x48000a
  cuadro 25; ELEGIR la entrada [+0x1314] = 5 en la pausa = 0x480009 cuadro 25) CENTRADOS en (x + 27, y + h - 30) y
  (x + w - 27, y + h - 30) (0x41d80000 = 27, 0x41f00000 = 30, menos 320 / 240: la escena va centrada). Las escenas son
  de 29 x 29 / 29 x 30 (banco 0x48, las mismas de la barra de ayuda, conv_menu.py iconos_dc). Antes: B a la IZQUIERDA
  de ATRAS (en el original va a la derecha), los dos 5 px hacia dentro y de 19 px; ahora pz_boton es del tamano del
  boton_dc de la barra de ayuda (radio 13, borde 1,6) en el punto del original. Lo usan la lista, AUDIO (solo ATRAS) y
  "Estas seguro?"; el SALON y la compra usan la ventana sin franja de botones (flags 0). Capturas pausa_antes /
  pausa_despues.
- REINTENTAR (CORREGIDO un caso; el del video no se ha reproducido tal cual): en las misiones la rama sin hijo vuelve
  a cargar la MISMA etapa (next_area de 44E590: con [rec + rama * 4 + 0x24] = -1 el registro no cambia; en CURSO.DAT
  las 94 misiones tienen una rama -1, p. ej. 1-1: BEAC izq 1 der -1) y esa copia se lee con el marco de despues de la
  union (marco_sig, y su colision con ese marco). Con REINTENTAR desde ella, curso_reinicia la reaprovechaba como la de
  la salida (etapa_nodo igual al nodo 0, los datos aun en el hueco) y salida_calcula buscaba (0, -16) del mundo en una
  ruta movida y girada: el coche salia fuera de la calzada y torcido (lo del video: costa con gradas, encima del
  quitamiedos). Lo mismo con una carrera nueva del mismo arbol desde el menu (curso_al_menu no suelta la etapa de la
  RAM): "a veces el coche comienza descentrado". Ahora `etapa_movida` (la etapa del hueco es la siguiente de una union
  o se recentro) obliga a leerla otra vez en el origen. La consola serie escribe en cada salida "SALIDA: arbol .. etapa
  .. s .. x .. a .. coche .." (con "(REINTENTAR)"), y tools/prueba/reintenta.sh compara la primera salida con la del
  reintento. Verificado en Flycast: C2C 1-1 con REINTENTAR desde BEAC (8 y 40 s de gas) y desde PALM (tras la union,
  con -DPRUEBA_PILOTO_MENU): las dos salidas iguales (s 16,00, x 0,02, coche (0, 0, -16)). El paso por la rama -1 no se
  ha podido completar con el piloto: con -DPRUEBA_RAMA=0/1 fue a PALM; con -DPRUEBA_RAMA_FIJA=1 (nuevo: fuerza la rama
  derecha, la -1) se vuelve a leer BEAC (SEG17) pero el piloto no llega a la etapa repetida antes del tiempo (REINTENTAR
  desde la union: salida bien). La causa del video es PROBABLE (encaja: costa con gradas = BEAC vista desde la copia
  movida).
- Cielo, "lineas que separan zonas" (dos causas):
  - Consola real (fotos de PALM: una costura vertical recta de otro tono de arriba abajo): el recorte del cielo a
    w = 50 m dejaba vertices a ~16000 px y la PowerVR pierde precision en los planos de UV y color de cada triangulo
    (cada uno con un error distinto: dos tonos a los lados de una arista). Ahora el plano de recorte es el mas lejano que
    no quita nada de la pantalla: 600 x fx / sqrt(fx^2 + 320^2 + 240^2) (437 m con la focal de siempre; el cielo esta a
    >= 707 m del ojo). Flycast no lo muestra (rasteriza en la GPU del PC): sin verificar en la consola.
  - Flycast y consola: escalones de color del degradado. La textura del cielo del original es de 8 bits con paleta de
    32 (obj_course_obj_sky_palm: 128 x 128, 256 colores; azul de la parte alta de R 119..121, B 205..214 en pasos de 1)
    y la de la Dreamcast es RGB565 (VQ): R 119 -> 115 y 121 -> 123, un salto de 8 justo donde el original cambia en 1 y
    el cielo casi liso queda en zonas con una linea entre ellas (en la captura de PALM, (115,146,206) / (123,146,206) /
    (123,146,214)). No hay formato de textura con mas bits que filtre (la paleta ARGB8888 de la PowerVR es global y no
    filtra igual); queda PENDIENTE (YUV422, que tiene 8 bits de luminancia, o tramar la textura al convertir: cambian
    los datos).
  - La textura del cielo es continua en u (los bordes del original y de la DC coinciden: diferencia media 3) y las
    tiras comparten vertices y v: no hay costura de geometria ni de borde (sin CLAMP en el original no hace falta).
- Fotos de la consola real (PALM, modo OutRun) y la de INFARTO: ver "Pendiente de la revision 44" abajo.

### Pendiente de la revision 44 (investigado; corregido en la segunda parte, abajo)

- Cajas oscuras sin textura (edificios lejanos de PALM que se texturizan al acercarse) y trozos de torre "flotando"
  sin su base al final de PALM: solo en la consola real. En Flycast el TA nunca se llena (ta_f no sube: Flycast no
  actualiza TA_ITP_CURRENT) y el reparto de nodos_visibles_todos dibuja todo; en la consola ta_f sube y el reparto
  quita nodos por clases de distancia y de tamano en pantalla, nodo a nodo: la base de un edificio (otra clase) se
  quita y la parte de arriba no (flotante). Reproducible en Flycast forzando ta_f (TA_F_MIN mayor). Arreglo pendiente:
  quitar por edificio (agrupar los nodos de un mismo objeto del registro) en vez de por nodo. Las cajas lisas: mallas
  sin textura del original (sin_textura del material: 0x60202020 por defecto en modelo.bloque) o niveles pequenos de
  los mipmaps en la PowerVR real; sin verificar. -> Corregido abajo (grupos de nodos): eran las dos cosas el reparto.
- INFARTO (foto: el coche parado sobre el plano azul del agua, 6c8658f): el plano de agua_dibuja se pinta con z en la
  lista opaca y tapa lo que quede por debajo de el; con el coche fuera de la calzada (o la etapa movida, ver
  REINTENTAR) se ve como un suelo azul. Sin reproducir: hace falta la etapa y el sitio (tiempo 14, Etapa 1). -> Abajo:
  NEWY por el arbol equivocado y el agua con la z mal por los vertices fuera de la pantalla.

### Revision 44: fallos de la consola real (2026-11-13, segunda parte)

Lo pendiente de arriba, con las fotos de la Dreamcast PAL (PALM en el modo OutRun y la de INFARTO).

- **Edificios sin base y cajas oscuras lisas (PALM)**. Causa: el reparto del TA (`nodos_visibles_todos`), que en la
  consola quita nodos (alli ta_f sube con lo que mide `TA_ITP_CURRENT`; Flycast no lo lleva y ta_f se queda en 1,75).
  Quita nodo a nodo por su clase de distancia y de tamano en la pantalla, y los nodos de un mismo edificio caen en
  clases distintas: en PALM las torres del final son una caja de 12 vertices (nodo 260: la base y el cuerpo) y dos
  capas de ventanas de 36 y 52 (562, 568) con la misma planta; la caja (pocas caras grandes) se queda y las ventanas
  (caras pequenas: caras por pixel altas) se van, o al reves. Las dos fotos son lo mismo: trozos de arriba sin su base
  (ed6aefe0, 75d97d85) y la caja sin las ventanas, oscura y lisa, que al acercarse vuelve a tenerlas (77d6267b ->
  34b1edd7). No son mallas sin textura (en PALM solo 37 tiras / 119 vertices sin textura, todas pequenas) ni los
  mipmaps (las mismas texturas se ven bien de cerca y no hay textura a medias: ver abajo).
  Arreglo (DECISION DEL PORT: el original no tiene tope del TA ni grupos; walk_404d10 prueba cada nodo por su cuenta):
  `tools/grupos_nodos.py` junta al convertir los nodos de un mismo objeto (los dos de menos de 80 m en planta, las
  plantas solapadas en al menos la mitad de la menor, la mayor como mucho 4 veces la menor, a menos de 1 m en altura,
  de la misma rama de la union; las clases de esa relacion) y escribe en los bits 16-31 de sus banderas el jefe del
  grupo + 1; `nodos_visibles` da a todos los del grupo que se ven la clase de distancia y la de LOD MAS ALTAS de ellos
  (las de lo que antes se quita) y cuenta alli el coste de todos: `nodos_quita` los deja o los quita a la vez. Asi,
  cuando no cabe, el edificio se va entero, como lo pequeno y lejano en el LOD del original, en vez de quedarse a
  medias. PALM: 209 grupos, 861 de 2512 nodos (hasta 17 por grupo, 1985 vertices el mayor; la torre del final: 260,
  562 y 568 juntos; los bajos, las fachadas de recorte y el tejado del edificio de la salida, 548..551, juntos); en las
  62 etapas y 470 uniones. RAM: 2 bytes por nodo (detras de vis); CPU: una pasada mas por los nodos visibles.
  Probado y descartado: la clase MAS BAJA del grupo (si se ve un trozo cercano, todo el grupo es cercano) y sin el
  tope de proporcion (un trozo de suelo con algo encima encadenaba manzanas enteras): el coste de los grupos pasaba a
  lo cercano, el reparto ya no cabia y en la salida de PALM se quitaba todo lo de mas alla ("clase -1": solo la
  calzada y la playa).
  Datos: `conv_tramo.convierte` y `conv_union.py` lo hacen al escribir; en una carpeta ya hecha,
  `tools/regenera.py --interno grupos datos/ [trabajo/]` (recompone, cambia y vuelve a empaquetar SEGS.PAK,
  UNIONES.PAK y UNIONESTA.PAK; solo cambian 2 bytes por nodo agrupado).
  Verificacion en Flycast (`-DTA_F_MIN=576`, una consola que gasta 2,25 veces la cuenta teorica; `-DPRUEBA_AUTO`, PALM;
  aqui decia 768 por error, corregido en el lote 1: las capturas *_tope576 y DREAMCAST.md son de 576):
  sin los grupos, en la salida de PALM (0'14-0'16) los edificios quedan a medias: cajas lisas sin sus capas de ventanas
  y fachadas con el cielo a traves (capturas_r44/cajas_antes_tope576.png, edificios_antes_tope576.png); con los grupos
  salen enteros o no salen (cajas_despues_tope576.png, edificios_despues_tope576.png; referencia sin el tope:
  edificios_referencia_sin_tope.png; el edificio negro de la derecha es asi en el original, cristal oscuro con
  ventanas). Sin -DTA_F_MIN (lo de siempre en Flycast) la imagen no cambia.
- **Medida del TA en la consola**: Flycast no lleva el registro del TA (lo gastado de verdad sale 0) y la unica medida
  de la consola sigue siendo la de la demo 12c (1,34 veces la cuenta teorica en el cuadro que se colgo; DREAMCAST.md,
  El tope). Para medirlo sin cable serie: en la pausa, Y (la pantalla de medidas de siempre) escribe ahora en la
  primera linea `TA <real>K/<teorico>K x<real / teorico> max<el mayor desde el arranque> f<ta_f>` (en Flycast "TA sin
  registro"): una foto de esa linea tras una vuelta por PALM da el factor de la consola, que es el que pone ta_f (real
  / teorico x 1,2, nunca por debajo de TA_F_MIN = 1,75). PENDIENTE: la foto del usuario.
- **Textura no subida con el GD lento**: comprobado que no pasa. Las etapas y uniones se suben a la VRAM a trozos
  (`sube_trozo`) y el modelo no lleva sus tablas (`tv`) hasta acabar: `geo_visible` no lo dibuja. Con
  `-DDIAG_GD_LENTO=1000 -DDIAG_TEX_VRAM` (medida nueva: cuenta por segundo las mallas con textura que se dibujan sin
  ella en la VRAM o con la gris de cuando no cabe nada; linea "TEXVRAM:"), salida en LAKE con el piloto, 140 s por LAKE,
  su union y la etapa siguiente (CAST): 93 lineas, 3500-9400 mallas con textura por cuadro, 0 sin subir y 0 grises.
  Las cajas lisas son lo de arriba (el reparto), no texturas a medias ni mallas sin textura.
- **Cielo con escalones (todas las etapas y el fundido de cielos)**. La VQ del cielo cuantificaba el libro a RGB565
  sin mas (el azul de PALM de R 119..121 a 115 / 123). Ahora (`vq.elige_cielo`, `modelo.cielos`: las texturas de las
  mallas de la lista 3 de los grupos -256 y -257, el cielo y el del fundido, en SEGnn.DAT y BKnn.DAT) el k-means va
  sobre los 8 bits del original como siempre, pero cada entrada del libro se cuantifica con un tramado ordenado de 2x2
  dentro de su bloque (umbrales 1/8, 7/8, 5/8, 3/8) y los bloques se asignan por la entrada sin cuantificar: la media
  de cada bloque guarda el color con 1/4 del paso del formato. En la pantalla el texel del cielo mide ~2 px (PALM:
  0,28 grados por texel) y el bilineal deja la trama casi lisa. Mismos bytes que antes (libro e indices): la VRAM no
  cambia (SEG00: 506240 bytes de texturas antes y despues). Medido sobre la textura de PALM (media de 2x2, lo que deja
  el bilineal, frente a los 8 bits del original): error RMS 1,88 -> 1,23 (16 bits tramados sin VQ: 1,06, pero 5 veces
  la VRAM); en la imagen realzada (x2 de contraste) desaparecen las curvas de nivel del azul (capturas_r44/
  cielo_tex_antes_despues.png: original, antes, VQ tramada, 16 bits tramados). Datos: las conversiones de ahora ya
  lo hacen; en una carpeta ya hecha, `tools/regenera.py --interno cielo <iso> datos/` (tools/cielo_tramado.py: busca
  la imagen de 8 bits de cada textura del cielo entre las de todos los bancos obj_course_obj_sky_* por el menor error
  y la vuelve a codificar; 0 sin encontrar en las 62 etapas y 470 uniones).
- **INFARTO: el agua como un suelo azul con un triangulo enorme** (foto d8983416). Sitio: INFARTO del juego de la
  consola, etapa 1 = NEWY (SEG30; "SALIDA: arbol 0 etapa 0 NEWY"), el parque (la colina de rocas y el edificio de
  ladrillo, la valla amarilla: en Flycast entre 0'47 y 0'59 del modo OutRun a toda velocidad). El agua de NEWY es una
  cuadricula de 12 cuadrados de 282 m a y = -12,4 bajo el parque (x -539..307, z -1202..-71), 12 m por debajo de la
  calzada; las demas etapas con agua tienen triangulos de hasta 550 m (BEAC: la ciudad entera a -51,1, la calzada a
  -50). En la foto el coche esta encima del azul con su sombra (el volumen de la sombra solo oscurece el suelo de
  alrededor del coche): el agua sale DELANTE de la calzada, es decir, con la z mal, en un triangulo de la cuadricula.
  Es el mismo fallo de precision que el cielo (arriba): `agua_dibuja` solo recortaba contra el plano cercano y con la
  camara a pocos metros del plano los vertices de esos triangulos caian a cientos de miles de pixeles de la pantalla;
  la PowerVR real saca de los vertices los planos de 1/w de cada triangulo y con esas coordenadas la z sale mal (el
  triangulo entero por encima de lo que tiene delante). Flycast (la GPU del PC en coma flotante) no lo muestra.
  Arreglo: cada triangulo del agua se recorta contra el plano cercano y los cuatro lados de la pantalla con 32 px de
  margen (`agua_tri`): ningun vertice del agua llega a la PowerVR a mas de 32 px de la pantalla. La zona de dibujo
  (posiciones +0x6E..+0x70 del descriptor, la union pasada la 0xC7) es la del original y no cambia. Verificado en
  Flycast (que no tenia el fallo): el lago de LAKE sale igual, azul solo donde no tapa el terreno, con las orillas y
  los arboles delante (capturas_r44/agua_lake_recortada.png; la foto: agua_infarto_antes_foto_usuario.jpg); NEWY con
  el piloto (arbol 283) sin agua a la vista desde la calzada, como antes. Sin verificar en la consola.
- **El curso equivocado (la foto de INFARTO en NEWY)**: `curso_huecos` (al arrancar: el mayor fichero de todos los
  arboles para los huecos de RAM) recorre CURSO.DAT y despues CURSOTA.DAT copiando cada arbol en curso[], y solo
  volvia a leer el arbol 0 tras CURSO.DAT: curso[] se quedaba con el ultimo arbol de la Contrarreloj (283, el curso
  seguido de OutRun 2 SP del reves: NEWY .. BEAC, union BK457) con curso_id = 0, y `curso_reinicia` no lo cambiaba
  (el pedido era el 0). El modo OutRun, el INFARTO del juego OutRun 2 (arbol 0) y la demo de atraccion salian en NEWY
  en vez de PALM (Flycast: "SALIDA: arbol 0 etapa 0 NEWY"; la foto del usuario: "Etapa 1" en el parque de NEWY). Desde
  f330617 (los tableros). Ahora el arbol 0 se lee al acabar los dos ficheros.
- **Salida del jugador** ("a veces el vehiculo comienza descentrado"): `tools/prueba/salidas.sh` (nuevo) hace la salida
  tres veces en el mismo modo (la primera, REINTENTAR y ABANDONAR + carrera nueva) y compara las lineas "SALIDA:".
  El original pone el coche en (0, 0, -16) del marco de la etapa de salida (4A6ED0 / 5C2560). Flycast, con el arreglo
  del arbol de arriba: INFARTO (juego OutRun 2, arbol 0) las tres "SALIDA: arbol 0 etapa 0 PALM s 16.00 x 0.00 a 0.0000
  coche 0.00 0.00 -16.00"; CONTRARRELOJ (tablero: OutRun 2, CURSO, la primera meta, codigo 60 -> arbol 260) las tres
  iguales en PALM (0, 0, -16); OUTRUN2SP (lado R, preset 1, arbol 0) la salida y el REINTENTAR iguales en PALM, y tras
  ABANDONAR el frontal (sin el R) eligio el preset 0: arbol 1, BEAC, "s 16.00 x 0.02 a 0.0018 coche -0.00 -0.01
  -16.00" (la ruta llega en curva: el cruce se afina a 0,25 m). Siempre en (0, 0, -16) como el original. El "a veces
  descentrado" del usuario era el caso de la primera parte (la etapa movida al reintentar) y, en los modos del arbol
  0, la salida en NEWY (el arbol equivocado). Antes del arreglo del arbol el INFARTO salia en NEWY (s 11,82).
- Visto de paso, sin investigar: en NEWY (arbol 283 con el piloto, Flycast) el cielo del atardecer tiene zonas
  naranjas de bordes rectos entre 0'38 y 0'58 (las mismas antes y despues de estos cambios; no es el tramado).
- Datos regenerados en esta parte (en la carpeta de datos compartida): SEGS.PAK, UNIONES.PAK, UNIONESTA.PAK y los
  descriptores SEGnn.DAT / BKnn.DAT (532), SEG13.DAT y SEG46.DAT sueltas, con `tools/regenera.py --interno grupos` y
  `--interno cielo` sobre los de la revision 43 (PYTHONHASHSEED=0). VRAM y tamanos de las etapas iguales.

## Arena, grava y polvo (2026-11-13)

Peticion del usuario: "en la arena de Palm Beach deberia saltar grava y el coche frenarse cuando pase por zona de
arena". Lo que hace el original (SLES-53998 y el PC) y como queda:

- **Que es la arena de PALM en la COLI**. Medido en la colision de PALM (SEG00, COLI0200; poligonos secundarios por su
  poligono de carretera mas cercano): el tipo 3 (clase 0x8) es la franja de arena entre las dos calzadas desde la
  salida (desvio +11 m, posiciones 0..479) y trozos a los lados; el tipo 2 (0x4) los arcenes de las posiciones 357..590
  (+-12,5..14 m); el tipo 7 (0x80) va de 16 a 22 m, detras de los muros (tipos 5 y 14). En BEAC el tipo 3 ocupa
  1246 poligonos. La demo ya clasificaba bien cada rueda (sup_rueda = 1 << tipo del poligono de la COLI bajo la
  rueda, en el orden del original: 0 del. izq.): no habia nada que corregir en la clasificacion.
- **Frenado (get_road_mu 500B30 + running_resistance 502120)**. Sin el servicio de red (fuera de linea) GetRoadMu da
  0 a la clase 1, 0,7 a 4, 8, 0x10 y 0x8000 y 1 a las demas (PC 5C403C / 62806C); SetRunningResistance resta en cada
  rueda sobre 0x801C (tipos 2, 3, 4 y 15), o en las cuatro si el centro (+248) lo esta, |cos(deriva de la rueda)| x
  0,175 x 0,0225 x kmh^2 kgf contra su velocidad (6AE4E4..6AE4F8: 0,0225; 0,2; 1; 0,9; 0,175; 0,175). La demo hace
  las dos cosas igual (or2_mu_superficie, resistencia_tierra) y la fuerza entra en el cuerpo (+94 sin par). Medido en
  Flycast (`tools/prueba/arena.sh`, -DPRUEBA_X=10.5f -DPRUEBA_ARENA=10.5f -DPRUEBA_V=200 -DPRUEBA_ARENA_GAS=1
  -DDIAG_PARTICULAS=6: lanzado a 314 km/h y sin gas, por la arena y por el asfalto, x = 0): en la arena las cuatro
  ruedas con clase 8, mu 0,70 y resistencia -11,9 kgf a 314 km/h por rueda (la formula da -11,8 por rueda... x 4 x
  9,81 / 1290 kg = 11,8 m/s2); deceleracion sin gas en la arena / en el asfalto / diferencia / formula (m/s2): 286
  km/h -14,3 / -5,7 / -8,7 / -9,8; 237 km/h -12,6 / -6,8 / -5,8 / -6,8; 176 km/h -10,5 / -7,5 / -3,1 / -3,7;
  130 km/h -6,8 / -5,1 / -1,7 / -2,0. La diferencia es el 85-90 % de la resistencia de la formula (el resto: otra
  marcha, la pendiente y el freno motor de cada recorrido). Con gas a fondo por la arena (sin soltar) el coche llega a
  233 km/h en 290 m desde la salida donde en el asfalto pasa de 300. Coincide con el original: la arena frena
  (hasta 1,5 g a 300 km/h sin gas). El tipo 7 de detras de los muros no frena (mu 1, sin resistencia), como el
  original.
- **Particulas (race_particles, PART_EFC)**: portadas grava, hierba y el humo / polvo del jugador (demo/main.c,
  "particulas de race_particles"; las direcciones y constantes en los comentarios): grava = PC 41DE20 / 41DF00 /
  41E190 / 41E230 = SLES-53998 0x10bd18 / 0x10bdd0 / 0x10c088 / 0x10c178; hierba = 41E360 / 41E440 / 41E780 / 41E820 =
  0x10c2a8 / 0x10c360 / 0x10c670 / 0x10c758; humo = 41C420 / 41C570 / 41CB30 / 41D1E0 / 41D330 / 41D180 = 0x109eb0 /
  0x10a0d8 / 0x10a708 / 0x10af70 / 0x10b178 / 0x10ad98; los registros de las ruedas de MakePlcarParam 41BD50 =
  0x109830; el generador y el nacimiento 0x1086f8 / 0x108648 (los de las chispas). Constantes del PS2 0x32b17c..0x32b2d0
  y 0x3bfe60..0x3bff2c (iguales a las del PC donde el PC las tiene; el PS2 tiene 64 humos, el PC 512, y como mucho 15
  humos por rueda y cuadro). Texturas: grava 0x57002a, hierba 0x57002b, humo y polvo 0x57002d (las tres de 64x64 de
  obj_course_obj_common; las cargan 0x10bd9c, 0x10c32c y 0x109f70). La 0x2f (una mancha de arena) NO es de las
  particulas de las ruedas: la usa 0x1dedd8 (mision, modo 9) y la tabla 0x40a9b0.
  - Que sale en cada superficie (por la clase de la COLI bajo cada rueda): grava con las traseras sobre los tipos 4 y
    21 (0x200010); hierba con cualquier rueda sobre el tipo 2 (0x4); polvo (el humo con la tabla de colores de su
    clase, 0x32b250) en el tipo 4 (clase 1, marron), en el tipo 3 (clase 2: 0x80ECDFCB, el polvo de arena; en BEAC
    entre las posiciones 0xE7 y 0x176 solo en los cuadros impares), en el 25 (clase 4, blanco) y en el 23 (clase 5),
    con la rueda movida mas de 0,15 m en el cuadro; en el asfalto de DESE y RUIN (descriptores 9 y 0xC) fuera de la
    union, polvo de clase 3 en los cuadros impares; humo blanco en asfalto con el derrape. **En la arena de PALM (tipo
    3) el original no echa grava: echa polvo de arena** (y marcas de la clase 2). La grava salta en los tipos 4 / 21
    (p. ej. los arcenes de tierra de ALPI, SEG05 posiciones 309..459, y SEG20) y la hierba en los arcenes de hierba.
  - Dibujo: grava y hierba como 418CF0 (cuadrados de cara a la pantalla de medio lado 0,05 / 0,075 m x proyeccion,
    trozo de 1/4 x 1/4 de la textura al azar, color de su fuente); el humo como 419DC0 (carteles de medio lado
    tamano x 0,5 con los cuatro colores en las esquinas y u = 1 a la izquierda). En la lista translucida con niebla y
    sin escribir z. TA: grava y hierba como mucho 256 + 256 cuadrados (4 vertices, ta_cabe); el humo con el tope de
    area de antes (HUMO_PANTALLAS = 2 pantallas por cuadro, del mas pequeno al mas grande). VRAM: 24 KB (tres texturas
    de 64x64 de 16 bits) en lugar de la mancha de 2 KB de antes, cargadas con EFECTOS.DAT al arrancar (fuera de la
    zona del curso).
  - PROVISIONAL / PENDIENTE: (a) la prueba del derrape del humo en asfalto (deslizamiento +E0 y agarre +E4 de la
    rueda contra 0x32b1e8; la fisica de la demo no los da): las traseras mientras dura el derrape, como las marcas;
    (b) car+58 (la luz del carril de CalcOfsLeftLane 4A45F0, que aclara el humo) = 1; (c) la meta con el estilo 82E7C0
    echa el humo de los rivales (41CED0 / 41D270 / 41D5C0 = 0x10aad0 / 0x10b050 / 0x10b450): aqui el del jugador;
    (d) el tope del tamano de 418CF0 (74F7BC / 74F7C0 del PC; en el PS2 el microprograma de la VU1): sin tope;
    (e) el alfa de los colores (0x80 de las tablas) se toma como 128 / 255 (el microprograma de la VU1 del dibujo
    sin leer: PROBABLE); (f) el contacto de cada rueda es el punto sobre el plano del coche (no el suelo de la rueda,
    +3C) y su direccion la del coche girada por angulo_rueda.
- **Marcas**: efectos_paso leia la superficie de la rueda del otro lado (sup_rueda va en el orden del original y
  el bucle de las marcas en el de la demo, i ^ 1): CORREGIDO.
- **Vibracion de la superficie** (0x1d04e4..0x1d0880 con 0x1d0138, PS2; el PC no tiene vibracion): portada en
  vib_paso con las intensidades por clase (arena 0,85: motor grande v x 0,85 x 0,3, 10 cuadros), los bordes
  +268 / +264 y el humo; PENDIENTE el termino de +D34 (derrape medio de 21 cuadros: +C2C no esta en la fisica de la
  demo) y las banderas +8 0x40 / 0x100. Ver Vibracion del mando. Medido en Flycast por la arena de PALM
  (-DDIAG_CHOQUE: lineas DIAGVIB): a v = 1,03 m/cuadro grande 0,26 y pequeno 0,07, 10 cuadros (0,85 x 0,3 x v y
  0,85 x 0,1 x v x 0,75).
- Verificacion en Flycast (display 94; capturas en el scratchpad, capturas_r44/): arena_palm_frente_antes /
  arena_palm_frente_despues(2) (-DPRUEBA_FRENTE=7: la camara delante del coche; antes sin nada, despues el polvo de
  arena detras de las ruedas), arena_palm_polvo_despues (camara de carrera), grava_alpi_despues(2) (ALPI, el arcen de
  tierra: -DPRUEBA_NODO=2 -DPRUEBA_ARENA=-9.5f -DPRUEBA_ARENA_S=2400: grava saltando y el polvo marron de la clase 1),
  hierba_alpi_despues, y los registros arena_palm_sin_gas.txt / asfalto_palm_sin_gas.txt / grava_alpi.txt (lineas
  PART). Vivas: polvo 58-64 (el tope de 64 del PS2), grava 35-69 a 300 km/h (mueren a 15 m del coche), hierba hasta
  246. VRAM: 22 KB menos libres (1801 / 1357 K frente a 1823 / 1379 K; fuera de la zona 77 K).
- **Sonido de la superficie**: el original lo tiene (PlayRoadCondition 425A40 del PC: dos canales con el sonido de la
  primera de las 10 entradas de la tabla 623F38 {mascara, sonido, ganancia} que casa con la superficie de las ruedas
  +24C y +250 (CarSound +54 / +6C de 4256F0), volumen por el deslizamiento de las cuatro ruedas y la velocidad).
  PENDIENTE (no esta en la demo: hacen falta su tabla en el PS2 y sus muestras en SFX.DAT).
- Datos: EFECTOS.DAT regenerado (el paso 'efectos' de tools/regenera.py: `PYTHONHASHSEED=0 tools/conv_efectos.py <iso>
  EFECTOS.DAT`; bloques nuevos 'PAR1' con
  las tres texturas y 'DSC1' con el descriptor de tramo de cada SEGnn, detras del 'ATR1'; lo de antes igual byte a
  byte, un ejecutable anterior lo sigue leyendo) y copiado a la carpeta de datos compartida.

## Orden de las etapas en todos los modos (revision 44, 2026-11-13)

Queja del usuario sobre la revision 43: "el tema del orden de las etapas ahora esta raro" (tras el preset 682b799 y
los tableros). Revisado contra las tablas del original (SCRIPTS de SLES-53998: csc_data_2, csc_data_cvt, *_ren,
OR2 / SP_goals y sus _reverse, Races.bin; codigo 0x1760c0, 0x1b63a0, 0x2a6758, 0x2a6978, 0x1bd2a8, 0x1be240).
- **La causa de la revision 43**: curso_huecos dejaba en curso[] el ultimo arbol de CURSOTA.DAT (283, NEWY .. BEAC del
  reves) con curso_id 0: OUTRUN, el INFARTO de OutRun 2 y la demo salian en NEWY y seguian por FLOR, PRIN... o
  repetian NEWY (su rama izquierda es -1). Corregido en 526de98 (Revision 44, segunda parte); comprobado ahora con la
  prueba de abajo.
- **Los arboles de la DC son los del original**: los 180 arboles de CURSO.DAT / CURSOTA.DAT comparados registro a
  registro con los del disco (script del scratchpad r44orden/compara.py): hijos (la clave de +0x2c / +0x30 buscada
  como en runtime_apply_course_data_44d720 -> +0x24 / +0x28), nivel (la profundidad de 44C850, en profundidad y solo
  hacia claves mayores; el nivel de la DC es en anchura y sale igual en todos) y metas: sin diferencias. Las claves
  son los indices salvo en la mision 108 (claves 0, 1, 3, 6, 10; la DC guarda indices). Una rama sin hijo (-1) vuelve
  a cargar la misma etapa (next_area de 44E590), como ya hacia la DC.
- **Mapa de ruta del HUD: estaba mal desde la primera version** (CORREGIDO). La DC encendia la casilla numero NIVEL
  (en PALM -> ALPI se encendia LAKE; en CAST, la de ALPI; parpadeando, inventado) y siempre el mapa de 5 niveles.
  Original: NAVI_PUB 0x1bd2a8 (= 4BCC40 del PC), al cambiar la clave de la etapa del coche (0x1cf9a8), pone en la
  casilla de su nivel (0x177270 = 44C940: +8 del registro) la clave o, si el registro lleva +0x74, +0x74 - 1 (solo
  las metas del reves de la Contrarreloj, arboles 270..279: la casilla del mapa del derecho) y esa es la actual
  ([0x5a3ce8]); la init (4BC9E0) las pone a -1. 0x1be240 dibuja (en (-241, -124), floats 0x369504) el fondo en la capa
  2, la actual en la 3 y las de los niveles anteriores hasta la primera sin llegar o la actual (ultimo nivel
  0x1bd098: 4, o 14 con los presets 2 / 3), sin parpadeo; mapa de 5 (2C0072 / 70 / 6F) con los presets 0, 1, 4 y de
  15 (2C006E / 6D / 6C) con 2 / 3 (tabla 0x4044a0); nada en las misiones ([0x36f3a4] sin la variante 4) ni en la
  Contrarreloj de una etapa (0x21ca78 y codigo < 0x3c, 0x21cb28). CURSO*.DAT llevan ahora el +0x74 en el byte libre
  detras de trans_seg (conv_union.crudos; 0 en todos los arboles de CURSO.DAT, que no hace falta rehacer).
- **OUTRUN pasa por el tablero 0x24** (PORTADO): la pantalla de UN JUGADOR (0x25dd58) lleva C2C / OUTRUN / INFARTO /
  CONTRARRELOJ / SALON a la clave de 0x375f58 = 0x3a, 0x24, 0x24, 0x24, 0x2b; con el modo 0x20 la init de 0x24
  (0x252cf4) usa el carrusel de 4 posiciones 0x419fb0 (OutRun 2, OutRun 2 SP y los dos cursos seguidos, candados 0xcc
  / 0xcb del SALON, 0x2538ac) y A va al coche con los bits 0..1 = la posicion. La DC ya tenia esa rama del tablero
  (comun/menu.c) pero llevaba OUTRUN directo al coche con el juego 0; ahora va al tablero y B desde el coche vuelve a
  el. Juegos 2 / 3 -> presets 3 / 2 (0x2a6978) -> START 0x1760c0(6 / 5) -> csc_data_2_ren / csc_data_cvt_ren con la
  categoria "%s_course" (0x3cb528, comprobado en el MIPS: las de sin _TA, 15 etapas con los dos hijos iguales):
  arboles 290 / 291 de CURSOTA.DAT (conv_union.ARBOLES_REN) y sus 28 uniones (los carteles con el mismo destino a
  los dos lados) BK470..497 en UNIONESR.PAK (7,1 MiB; aparte, UNIONESTA.PAK no cambia). La meta (CAPE / NEWY, clave
  14) usa la escena de la meta E (450380(8) - 0xA). Con estos presets: mapa de 15, fila de tiempos TIM1 2 / 3,
  banderero BAND1 / BAND, choques del preset.
- **CONTRARRELOJ de curso seguido**: el original pone el preset 3 (0x50 / 0x51) o 2 (0x52 / 0x53) (0x2a68e8 /
  0x2a6884); la DC le dejaba el del juego (1 / 0). CORREGIDO.
- **El juego elegido se queda** (raiz +0x208 bits 0..1): 0x2a6758 llama a 0x2a6978 con esos bits sin condicion
  (0x2a6770) al salir del menu en todos los modos, tambien COAST 2 COAST (que no pasa por el tablero), y solo B en el
  tablero 0x24 los borra (0x253e80). En el original, tras elegir OutRun 2 SP (o un curso seguido) en un tablero, las
  misiones corren con ese preset (las reglas: choques, trafico, banderero, tiempos; el curso es el de Races.bin) y,
  al volver a INFARTO, el tablero de 2 posiciones se queda en la 0 si los bits son 2 / 3 (0x252e80). La DC hace lo
  mismo (menu_juego_cursos). OUTRUN ya no hereda nada: lo elige en su tablero.
- **Nombre de la etapa**: zona_nombre (el rotulo de texto de la DC) solo se ve sin HUD; sale de curso[cur_sig], la
  etapa que se carga. CARGASP (solo OUTRUN2SP) va por el preset del lado, que ya da el arbol (Preset del curso). SIN
  PORTAR: el rotulo de la etapa siguiente del original (0x1bea00 = 4B9450 del PC: con car+0x5c y la clave elegida
  distinta de la del coche, el token de la tabla 0x369518 (30 escenas 0x2c0048..0x2c0065, por el tipo del descriptor
  de la etapa elegida) en la capa 5, parpadeando con el bit 0x10 hasta 0x78 cuadros y despues fijo; x -340, o
  [0x369510] - 340 en las misiones): HUD.DAT no lleva esas escenas. Detalle (2026-11-14, MIPS y outrun2-decomp
  stage_banner_4b9450): se ve con el coche en la union (car+0x5c != 0; fuera, el estado [0x5a3d36] vuelve a 0) y la
  clave del registro elegido ([0x549048]+4) distinta de la de la etapa del coche (0x1cf9a8): con una rama -1 (la misma
  etapa otra vez) no sale. El indice de 0x369518 es el +0 del descriptor del registro ([[rec+0x14]], que
  44D720 pone del +0x1c: el numero de tramo, el mismo de dsc_seg / conv_union.tablas: 0 PALM .. 29 EAST). Estados:
  0 -> 1 (contador [0x5a3d34] = 1); 1: dibuja si contador & 0x10, al llegar a 0x78 pasa a 2; 2: siempre. Punto:
  (w + x - 320, h + 5 - 240) con (w, h) la mitad del tamano de la escena (0x1156c0) y x = -340 ([0x369510] = -42
  mas con [0x36f3a4] y la variante != 4: misiones). Cada escena son dos piezas: el nombre (126 x 38, texturas 89..98
  del banco 0x2c, 128 x 128 de 256 colores, tres nombres por textura) y un rotulo comun (82 x 22, textura 74, 16
  colores). PENDIENTE por la memoria: en 16 bits los 30 nombres son 280 KB (en los avisos residentes de HUD.DAT, con
  el tope de 350 KB, no caben sin VQ) y C2CHUD.DAT (misiones) los necesita tambien; hay que decidir si van en los
  avisos con VQ o subidos solo mientras se ven (hudd_sube_uso, con la RAM de HUD.DAT).
- Prueba: `tools/prueba/orden.sh` (GDI con -DPRUEBA_AUTO -DPRUEBA_ORDEN -DPRUEBA_SIN_TIEMPO -DPRUEBA_RAMA=r -DPRUEBA_RAMA_FIJA=r
  -DPRUEBA_FIN=600 -DPRUEBA_SALTO=600): el piloto corre la lista PO de demo/main.c (OUTRUN juegos 0..3, CONTRARRELOJ
  5 / 0x3e / 0x43 / 0x46 / 0x4d / 0x50, INFARTO de OutRun 2 SP, misiones 0, 84, 16 y 22, OUTRUN2SP sin y con el
  gatillo R) siempre por la rama r, cada etapa desde 600 m antes de su final (PRUEBA_SALTO; en la bifurcacion el coche
  pasa al carril 1 (izquierda, carriles 0..2) o 4 (derecha, 3..5) de la rama fijada: el piloto sigue el asfalto mas
  cercano y se quedaba en la otra calzada, fuera de la ruta: en la primera pasada "atascado" en MAYA del arbol 291) y
  la tercera vez seguida en la misma etapa acaba la carrera; `tools/prueba/orden.py` compara las lineas SALIDA /
  RECORRIDO / CHECKPOINT / MAPA / GOAL con la tabla del original leida de la ISO (etapas, a donde lleva cada rama, meta,
  nivel, casilla y mapa) y el arbol cargado con el del modo (misiones: Races.bin, 100 + i; CONTRARRELOJ: 200 + codigo;
  si no el preset (OUTRUN2SP: el del lado; consola: 0x2a6978 del juego) y 0x1b63a0 -> 0x1760c0(0 / 1 / 5 / 6):
  csc_data_cvt, csc_data_2, csc_data_cvt_ren, csc_data_2_ren = arboles 1, 0, 291, 290). menu.sh: MODO_JUEGO=1 pasa
  por el tablero (TB_JUEGO=n). Las filas de 15 recorridos del frontal de OUTRUN2SP (+27 = 2 / 3) no estan en la lista
  (dan los presets 2 / 3, los mismos arboles 291 / 290 que los juegos 3 / 2 de la consola).
- **Resultado (2026-11-14)**: rama 0: 18 carreras, todas BIEN (OUTRUN PALM LAKE CAST CLOU TULI / BEAC SANF NIAG MACH
  EAST / los 15 de 290 hasta CAPE y de 291 hasta NEWY; Contrarreloj DESE, metas 0x46 TULI .. PALM; INFARTO; mision 0
  BEAC PALM; OUTRUN2SP lado 0 BEAC .. EAST, lado 1 PALM .. TULI; las ramas -1 repiten la etapa). Rama 1: 18 carreras,
  todas BIEN (OUTRUN PALM ALPI DESE GHOS CAPE / BEAC YOSE GRAN AMAZ NEWY; los seguidos 290 / 291 igual que con la 0;
  mision 84 CAPE GHOS DESE ALPI PALM, mision 22 ALPI SNOW ALAS y la repeticion; OUTRUN2SP lado 0 BEAC .. NEWY, lado 1
  PALM .. CAPE). Nada cruzado en los arboles, las ramas, las metas, el mapa ni el arbol de cada modo. Capturas:
  scratchpad/capturas_r44/orden_rama0_resultado.txt, orden_rama1_resultado.txt, orden_r0_mapa15_seguido.png.
- Tablero 0x24 de OUTRUN (CORREGIDO): al entrar, el original pone el carrusel en el juego guardado (0x26e6e8 con la
  raiz +0x208 & 3) y despues deja los bits 0..1 en 0 / 1 (0x252d78..0x252e34: 0 y 2 -> 0, 1 y 3 -> 1); la DC no los
  tocaba. El de INFARTO (0x252e38..) solo pone el carrusel si los bits son 0 / 1 y no los cambia (como la DC).
- Datos: CURSOTA.DAT (arboles 290 / 291 y +0x74), UNIONESR.PAK y BK470..497.DAT: paso `unionren` de
  tools/regenera.py (`conv_union.py <iso> uren/ --arboles=<ARBOLES_TA> --bk-desde=470 --curso=CURSOTA.DAT` y
  `pak_union.py --pak=UNIONESR.PAK`; `unionta` lleva ahora --bk-hasta=470), PYTHONHASHSEED=0.

## Barrido grafico con el disco lento y el TA de la consola (revision 44, 2026-11-14)

Solo busqueda (sin cambios en el juego). Flycast con `-DPRUEBA_AUTO -DPRUEBA_SIN_TIEMPO -DDIAG_GD_LENTO=750
-DTA_F_MIN=576 -DDIAG_MS -DDIAG_POPPING -DDIAG_TEX_VRAM` (el GD lento ya existia: comun/gd.c, 120 ms por salto + n x
2048 / KB/s): arboles 290 y 291 enteros (`-DPRUEBA_JUEGO=2 / 3`: las 30 etapas y las 28 uniones BK470..497), arboles
0 y 1 por la rama izquierda y la Contrarreloj, INFARTO de OutRun 2 SP y las misiones 0 / 84 por la derecha
(`PRUEBA_ORDEN`, FIN / SALTO 1500), tools/prueba/tablero.sh con el GD lento, y controles con el TA de siempre (448) sin
GD lento. Informe y capturas: scratchpad capturas_r44/barrido/ (w4.md). Nota: la verificacion de la segunda parte de
la Revision 44 dice `-DTA_F_MIN=768`, pero sus capturas son *_tope576 y DREAMCAST.md (El tope) dice 576; con 768 la
imagen se rompe (abajo).

PENDIENTE (encontrado):
- **Uniones con ta_f 2,25: desaparece la escena 3-4 s** (grave si la consola gasta eso). En casi todas las uniones
  (290: PALM->LAKE, CAST->FORE, DESE->CLOU, CLOU->INDU, SNOW->GHOS, RUIN->IMPE; 291: NIAG->SEQU, SEQU->GRAN,
  GRAN->MACH, ALAS->AMAZ, PRIN->FLOR; arboles 0 / 1: BK00, 01, 03, 06, 15, 16; INFARTO igual) el reparto no cabe ni en
  la clase -1 ("POP: corte 1 m clase -1, falta 500-900 %", "tope mallas 12000-22000" en 34-46 cuadros) y ta_cabe tira
  lo que no cabe en el orden de envio: la calzada, los muros y el cielo (queda el color de fondo y el plano del agua),
  33-35 fps. Con 448 no se pierde nada pero RUIN->IMPE dibuja 30000 vertices a 28 ms (31 fps). Causa probable: etapa
  que acaba + union + etapa siguiente en el mismo tope sin prioridad para lo cercano / la calzada.
- **Con TA_F_MIN=768 (3 x) PALM sale sin calzada ni edificios** desde la salida (clase -1 siempre, 28000 mallas por
  segundo fuera; el coche sobre el agua). El factor de la consola sigue sin medir (la linea "TA" de la pausa).
- **Tirones al entrar en la etapa con el GD lento**: a 750 KB/s SEG08 (CAST) tarda 16,0 s, SEG13 (METR) 16,1, SEG15
  (CAPE) 15,4, SEG22 (LASV) 14,9 y la union no dura tanto: curso_paso espera (`carga_espera` a 60 m del final) 1-2,6 s
  (ALPI->CAST "2130 ms 31 cuadros", TULI->METR 2617 / 37, EAST->MAYA 2372 / 20; CAST fps 13,5, METR 11, MAYA 10,7), y
  antes el final de la union da al vacio (la etapa siguiente aun no esta). Al entrar, 6000-14000 mallas fuera del
  tope en los primeros cuadros.
- **"lejos" no se recupera tras la noche**: la CPU de METR baja la distancia a 1850 m (CAPE 1900) y sube 50 m cada 30
  cuadros: al principio de RUIN sigue en 2550-2700 y bajo el arco de roca se ve el blanco del horizonte en vez del
  terreno (con 4000 m, en el control, no).
- Visibles / menores: caja gris lisa sobre el agua al salir de BEAC (291, solo con 576 + GD lento); EAST, antes de la
  union: la parte de arriba del acantilado flotando sin la de abajo ("tope -20/20/9"); GHOS: el haz de los faros con
  un cuadrilatero brillante de borde recto delante del morro; el texto "< X  X >" de la bifurcacion se ve con el HUD en
  las carreras con el piloto (auto_: condicion del rect_txr de txt_cen); NEWY (291) sigue con las zonas naranjas lisas
  del cielo del atardecer.
- Sin fallo: texturas sin subir o grises (TEXVRAM 0 en todo), tuneles, agua de LAKE / BEAC / GRAN, HUD de los modos,
  GOAL y meta, menus / tableros / SALON con el GD lento; los porticos en blanco de las misiones son el BLNK del
  original (rama -1). Las "torres grises" de la entrada de SANF son los anclajes del puente (igual en el control).

## Barrido de jugabilidad (revision 44, 2026-11-14, w5)

Solo busqueda (informe y capturas: scratchpad capturas_r44/barrido/w5.md y w5_*.png). Flycast sin GD lento.
- Muros y suelo con `-DPRUEBA_MURO` (muro.sh): IMPE (ROMA) del arbol 0 desde la salida 300 s, y RUIN -> IMPE -> CAPE
  del arbol 290 420 s: carroceria dentro de un muro 0, ruedas hundidas 0, PlWrecker 0, trafico hundido 2 / 1 (-0,13 m,
  0x100). Los saltos de la camara (0,6-2,9 m en un cuadro; 36 con 90 choques y 58 con 355) son todos en choques contra
  el muro; la camara nunca pasa de 2,8 m sobre el coche. La "camara que salta en ROMA" y los quitamiedos atravesados no
  se reproducen en Flycast.
- Carreras seguidas (seguidas.sh con -DDIAG_PVRMEM, en COAST 2 COAST): RAMMAPA de carrera 12716K y zona del curso 992K
  en las 6, sin errores de RAM ni de VRAM; la salida del REINTENTAR igual a la primera.
- INFARTO, CONTRARRELOJ (meta A, arbol 260), OUTRUN con cambio manual (marchas 1..5 arriba / abajo, derrape, pausa
  REINTENTAR / ABANDONAR), OUTRUN2SP fin del tiempo -> mapa -> nombre -> rankings -> GAME OVER -> frontal, VMU (guardar
  y leer tras reiniciar), SALON (compras, guardado, segundo arranque) y la musica de cada pantalla: sin fallos nuevos.
- **FALLO (visible): C2C, MISION SUPERADA (modo 35, escena 2B004F)**: salen "B REINTENTAR" y "ABANDONAR B" (dos B) y
  REINTENTAR se pulsa con X. En la ISO el icono de REINTENTAR de 2B004F es el del bit 0x10 (escena 0x480005 de la
  tabla 0x374a60); tools/conv_fin.py iconos_ps2 solo carga los iconos 4 y 8 y boton_de lo empareja con el de B (error
  < 40). En los menus el 0x10 es Y (conv_menu.BOTON_DC, RANDOM de comun/menu.c), y demo/fin.c (0x1bc700, mis_seg) lee
  CONT_X. PENDIENTE: iconos_ps2 con (4, 8, 0x10), regenerar FINC2C.DAT (paso FIN de tools/regenera.py) y CONT_Y en fin.c.
- La pareja deformada con la foto 3 (05 / 18) sigue como en "Coches: la pareja de la carrera" (PENDIENTE).
- Pruebas: menu.sh con MODO_JUEGO=3 pasa ahora por los tableros 0x24 / 0x25 / 0x26 (conduce.sh / reintenta.sh en la
  CONTRARRELOJ). Desfasados, sin tocar: seguidas.sh sin MODO_JUEGO va por C2C (PRUEBA_NODO no cuenta en las misiones y
  el recorrido se desfasa: usar MODO_JUEGO=1) y salon.sh (OUTRUN pasa por el tablero 0x24 desde la revision 44: sus
  capturas sa_8 / 9 / 10 son el tablero y espera MUS33 en vano).

## Correccion de los barridos, lote 2 (revision 44, 2026-11-14, w61)

Capturas de antes / despues: scratchpad capturas_r44/w61/. Flycast con `-DPRUEBA_AUTO -DPRUEBA_SIN_TIEMPO
-DDIAG_GD_LENTO=n -DDIAG_MS -DDIAG_POPPING -DPRUEBA_JUEGO=2 -DPRUEBA_NODO=k -DPRUEBA_FIN=2800` (arbol 290).
- **Tirones de carga al entrar en la etapa con el GD lento** (CORREGIDO, dos causas):
  1. La medida exageraba: `DIAG_GD_LENTO` (comun/gd.c) esperaba n x 2048 / KB/s + 120 ms ademas del tiempo de
     lectura que ya emula Flycast (~1,4 MB/s, DREAMCAST.md "Cambios de etapa: tirones"): con 750 el lector iba en total
     a ~490 KB/s (SEG08, 6,6 MB recompuesto + COL08, 16,4 s). Ahora la espera es solo lo que falta para el tiempo de
     un lector de esa velocidad (medido el tiempo de la lectura de Flycast): 750 es 750.
  2. La etapa que se deja se soltaba a UNION_LIBERA (300) m dentro de la union; el original la deja de dibujar en la
     posicion 0x18 de la union con la rama elegida (race_manager 4515B0 bandera 0x20; 44E590 cambia de registro; la DC
     ya lo hacia en geo_paso y ya no la dibujaba). Ahora se suelta ahi (UNION_LIBERA_POS, 191 m con m_pos 7,95) y la
     etapa siguiente se pide 109 m antes. La rama se decide a 64-65 m de la union (RECORRIDO, que ahora escribe
     "union + n m") en ALPI, TULI y METR del 290. El trafico de atras se cierra a TR_CIERRA_ATRAS posiciones.
  Medido (ALPI -> CAST): antes (750 de antes) SEG08 en 16,44 s, leido en s 7120 y "POP: ALPI 2251 ms 7 cuadros"
  (parada en carga_espera); con la etapa soltada en 0x18 y el mismo lector (490 nuevo) 17,43 s y parada de 1982 ms;
  con 750 de verdad 12,93 s, leida en s 6844 (350 m antes de CAST, s 7193) y sin parada. TULI -> METR: SEG13 en
  12,16 s, 425 m antes; METR -> RUIN: SEG14 en 10,38 s. Ningun POP de mas de 1300 ms. El final de la union ya no da
  al vacio (alpi_cast_hoja_antes_arriba_despues_abajo.png). PENDIENTE: no hay medida de la consola (GDEMU / GD-ROM);
  por debajo de ~550 KB/s las etapas de 6-8 MB vuelven a no llegar (es el tiempo de lectura: la memoria no deja leer
  la etapa siguiente antes de soltar la anterior).
- **"lejos" arrastrado de METR a RUIN** (CORREGIDO): el recorte de seguridad por CPU (`lejos`, 50 m cada 30
  cuadros; no es del original, que dibuja a 4000 m: 0x178580) era de la geometria de la etapa que se dejaba y solo
  volvia a subir con la CPU por debajo de CPU_MS_ALARGA. Ahora al entrar en la etapa siguiente (CUR_SIGUIENTE) vuelve a
  LEJOS_PS2 y se mide otra vez. Medido: METR acaba a 2250 m y RUIN empieza a 4000 (3850-4000 despues); bajo el arco de
  roca el terreno en vez del blanco del horizonte (ruin_arco_tras_metr_despues_lejos4000.png; antes
  capturas_r44/barrido/ruin_tras_metr_lejos2650_0430.png).
- **Texto "< X  X >" de la bifurcacion con el HUD en las carreras con el piloto** (CORREGIDO): el rotulo de texto de la
  DC (txt_cen) se dibujaba con el HUD si `estado == CARRERA && auto_ && t_estado >= 50`; ahora solo sin HUD, como dice
  "Nombre de la etapa" arriba.
- **seguidas.sh** (CORREGIDO): MODO_JUEGO=1 (OUTRUN, arbol 0) por defecto; en COAST 2 COAST PRUEBA_NODO no cuenta.
- **menu.sh con MODO_JUEGO=3**: ya arreglado en d4d267c (tableros 0x24 / 0x25 / 0x26).
- PENDIENTE: **BEAC (291) caja gris lisa sobre el agua**: reproducida con `-DPRUEBA_JUEGO=3 -DTA_F_MIN=576
  -DDIAG_GD_LENTO=750` (gd.c nuevo) y no es de un cuadro: se ve de 0'03"7 a 0'05"4 de crono
  (capturas_r44/w61/beac_caja_gris_hoja_0303_0515.png), y faltan tambien las torres altas de la derecha. Las lineas POP
  de BEAC dan "clase 8-18, falta 170-318 %" desde la salida (con 448 la clase no baja asi): es el reparto del TA
  (nodos_visibles_todos / ta_cabe) cortando los nodos del edificio y dejando una pieza sin textura; TEXVRAM 0 (no es una
  textura gris). No es del GD lento. No tocado: el reparto del TA lo esta cambiando a la vez el otro lote (cambios sin
  commit en ta_cabe / nodos_visibles_todos / pop_mide); queda para despues de ese.
- PENDIENTE: **pareja deformada con las fotos 3..5**: comprobado en el MIPS que el original tampoco convierte nada.
  El esqueleto sale del personaje (0x205038: [s0 + 8] = personaje, tabla 0x40e3f0: 5..10 y 19 -> esqueleto 2; 11, 12,
  17, 18 -> 3; 3 / 4 -> 4; 13..15 -> 0; 16 -> 5) y va a SetBone 0x207e60 con bone.bin + 16 x esqueleto; SetMotion
  0x207fa8 no mira el esqueleto del movimiento (0x2081e8 solo guarda el id y devuelve 1); 0x28b190 guarda el u16 del
  esqueleto del movimiento en +8 sin usarlo y 0x28b3d8 empareja los canales por el indice de hueso del robot. Los
  esqueletos 2 (50 huesos) y 3 (53) tienen otro orden (2: hara, kosi, piernas, mune...; 3: hara, mune, kubi, brazos,
  kosi, piernas) y en ANIMS.PS2 todos los de conducir (HANDOL, OCVT_3x, grupos 4, 6, 0x20) son del esqueleto 3: no
  hay movimiento de conducir para el 2. Las tablas confirmadas: piloto 0x36f928 = {11, 18, 17, 9, 5, 19}, lista
  0x40e210 = {0, 1, 2, 4, 3, 5}. Queda por ver quien llama a 0x205038 para el piloto (si algun camino cambia el
  personaje o el asiento con las fotos 3..5) o si el original sale igual de mal.

## Correccion lote 1 de los barridos (revision 44, 2026-11-14)

Los fallos del barrido grafico (w4) y del de jugabilidad (w5) de arriba, por orden de gravedad. Capturas en el
scratchpad capturas_r44/lote1_*.png. Medidas nuevas con -DDIAG_POPPING: la linea POP lleva el TA de la etapa (tope),
lo que no es la etapa y prio_alfa, y la linea POPTA el desglose por cuadro de lo que no es la etapa (agua, objetos de
la union, coche, pareja, trafico, efectos); "d" es el corte de distancia (prio_d, -1 abajo).

- **Uniones con ta_f 2,25: la escena desaparecia 3-4 s** (CORREGIDO). Reproducido en Flycast (`-DPRUEBA_AUTO
  -DPRUEBA_SIN_TIEMPO -DTA_F_MIN=576 -DDIAG_POPPING -DPRUEBA_JUEGO=2`, PALM -> LAKE): no era la calzada ni las tres
  etapas en el mismo tope, sino los objetos de la union (bo_dibuja, 4A9DF0: la bandada, los halcones, el dragon...):
  POPTA "bo 468-638K coche 782K pareja 192K trafico 58-99K", lo que no es la etapa 1620-1730 KB de los ~1790 del 3D y
  el tope de la etapa 0 KB ("TA etapa 0K/0K"): ta_cabe tiraba todo lo de la etapa, el cielo (que va primero) incluido,
  y solo quedaban el coche, el trafico y los pajaros sobre el color de fondo y el plano del agua (capturas
  lote1_union_palm_lake_tope576_antes_*.png). bo_dibuja contaba en la reserva de "lo que no es la etapa" del cuadro
  anterior (ta_otros_ult), que se aparta entera antes del reparto.
  Arreglo (DECISION DEL PORT: el original no tiene tope del TA): los objetos de la union van despues de la etapa con su
  propio tope (`ta_bo_tope`: lo que deja la etapa sin tocar la reserva del coche, la pareja, el trafico y los efectos)
  y de la etapa solo se les aparta lo que pidieron en el cuadro anterior hasta la mitad de lo libre (`TA_BO_PARTE`);
  cada nodo de bo_dibuja (un pajaro, el dragon) va entero o no va (su coste se mira antes de enviarlo). Con 448 (lo de
  siempre) los objetos caben como antes (~300 KB de los ~950 libres) y nada cambia. Despues, con 576: PALM -> LAKE
  "TA etapa 250-290K, bo 313-383K, tope mallas 0" (antes 14000-18000 mallas por segundo fuera), la calzada, los muros,
  el cielo y los pajaros en todos los cuadros, 37-38 fps (antes 33-35); LAKE -> ALPI y ALPI -> CAST (la bandada
  cruzando) y CAST -> FORE sin mallas fuera (lote1_union_*_despues_*.png). No probadas una a una las demas uniones de
  la lista (CLOU, INDU, SNOW, RUIN, 291, arboles 0 / 1, INFARTO): el mecanismo es el mismo.
- **TA_F_MIN=768: PALM sin calzada desde la salida** (CORREGIDO lo que se puede). Con 3 veces la cuenta el coche
  (1043 KB) y la pareja (256-379 KB) se llevan casi todo y a la etapa le quedan 140-174 KB (~1800 vertices): ni lo que
  rodea la camara (la clase 0 de distancia, que el reparto dibujaba siempre entera) cabe, y ta_cabe tiraba en el orden
  de envio (el de las texturas: la calzada se iba y se quedaba el mar lejano). Ahora (DECISION DEL PORT) el reparto
  baja un escalon mas: si ni la clase 0 de distancia cabe, prio_d = -1 y todo se reparte por la clase de LOD (lo
  enorme en la pantalla, la clase 0, siempre); y como ultimo recurso, si aun asi algo no cabe con el reparto en su
  minimo, cada malla de la etapa mas alla de `ta_lej_tope` (z - r) no se envia: baja un 20 % en cada cuadro en que algo
  no cupo y sube un 2 % en los demas hasta soltarse (mas alla de lejos). Con 768: la calzada, los quitamiedos y lo
  cercano (el cartel de Shell, los edificios de la salida, el autobus) en todos los cuadros de los primeros 30 s; lo
  lejano (las playas, el mar) sale y entra con el corte (lote1_palm290_tope768_despues_*.png; antes:
  barrido/palm290_tope768_*.png). Sigue siendo una imagen muy pobre: si la consola gastara 3 veces la cuenta, el coche
  y la pareja solos llenarian el 80 % del bufer (no se tocan: son los del original). Con 576 el ultimo recurso no se ha
  visto actuar en las uniones medidas (prio_d -1 si, sin mallas fuera).
  Documentacion: la verificacion de los grupos de nodos (Revision 44, segunda parte) decia `-DTA_F_MIN=768`; era 576
  (las capturas *_tope576 y DREAMCAST.md), corregido alli.
- **C2C, MISION SUPERADA: REINTENTAR con el icono B y la tecla X** (CORREGIDO). `tools/conv_fin.py` iconos_ps2 carga
  ahora los iconos 4, 8 y 0x10 de la tabla 0x374a60 (banco 0x48, cuadro 25): la pieza de REINTENTAR de 2B004F se
  empareja con el 0x10 y lleva el boton Y de la Dreamcast (conv_menu.BOTON_DC 0x10 = Y, como RANDOM de los menus);
  demo/fin.c (0x1bc700, rama mis_seg de 4959E0: "0x10 la misma") lee CONT_Y. FINC2C.DAT regenerado con el paso fin de
  tools/regenera.py (PYTHONHASHSEED=0) y copiado a la carpeta de datos; FIN.DAT y GOVER.DAT salen iguales byte a byte.
  Verificado en la maqueta de la escena (c2c_mision_superada_iconos_despues_maqueta.png: A SIGUIENTE RONDA, Y
  REINTENTAR, ABANDONAR B); sin pasada en Flycast hasta una mision superada.
- **salon.sh desfasado** (CORREGIDO): tras elegir OUTRUN pulsa A en el tablero 0x24 (espera "TABLERO: clave 0x24"),
  en los dos arranques. Verificado en Flycast (GDI con -DPRUEBA_SALON_MILLAS=20000): las dos compras y sus guardados,
  sa_8 / sa_10 la seleccion de coche en el F50 y sa_9 la musica (MUS33), sin errores (lote1_salon_sa7_8_9_10_despues.png).
- **EAST antes de la union: la parte de arriba del acantilado flota sobre la carretera** (INVESTIGADO, PENDIENTE). No es
  el reparto del TA, ni el PVS, ni el LOD: sale igual (EAST desde la salida, `-DPRUEBA_JUEGO=3 -DPRUEBA_NODO=10`, 1'05"
  a 1'09" de la etapa) con 576, sin tope (`-DTA_F_MIN=128`), sin PVS (`-DSIN_PVS`), sin reparto ni tope
  (`-DDIAG_SIN_TOPE`, que ahora tambien quita el reparto, y `-DCPU_MS_CORTA=1000`: lejos 4000) y con el LOD de 404B00
  16 veces mas lejos (`-DREPARTO_LOD_MAS=4`): siempre la masa de roca con el borde de abajo recto y el cielo debajo
  (lote1_east_roca_sin_reparto_hoja.png, lote1_east_roca_sin_reparto_lod16_hoja.png). Sin la etapa (`-DDIAG_SIN_TRAMO=0`,
  medida nueva: no dibuja la etapa (0), la union (1) o la siguiente (2)) la roca no esta (lote1_east_roca_sin_etapa_
  hoja.png): es de la etapa EAST (el tramo 29 de conv_union.tablas) y todo lo que tiene ya se dibuja. Queda por
  ver si el original tambien la ve asi desde la calzada (la base podria estar en un registro de la union que el original
  no dibuja aun: uni_regs / +0x72..+0x74 = 60 / 600 de EAST) o si falta una malla en la conversion (tools/conv_tramo.py):
  buscar en su SEGnn.DAT las mallas del acantilado del final y su nodo.
- **GHOS: el haz de los faros con un borde recto delante del morro** (INVESTIGADO, sin cambio). Sin la luz del suelo
  (`-DDIAG_SIN_FAROS`, lote1_ghos_sin_luz_suelo_solo_haz_hoja.png) el borde recto sigue: es el haz proyectado del PC
  (focos_*), cuya textura (0x30009, spr_etc_xst 9) corta en seco en la fila 200 de 256 (la linea de corte del haz; el
  PC la proyecta igual, con borde negro) y se suma a la luz del suelo (0x106960, aditiva) donde coinciden: de ahi la
  zona mas clara. El PS2 no dibuja ese haz (Luces de noche y faros: lo pidio el usuario, del PC); suavizarlo seria otra
  desviacion. La luz del suelo tiene ademas la DIFERENCIA ya anotada (z GEQUAL con 1 % en vez de ALWAYS): donde la
  calzada sube por delante del plano del coche la tapa con borde recto; no es lo de las capturas.
- **RUIN -> IMPE a 31 fps con 448** (REPRODUCIDO, PENDIENTE): es el dragon (conjunto 11 de los objetos de la union,
  forzado con `-DPRUEBA_BO=11`, `-DPRUEBA_NODO=12` del arbol 290): POPTA "bo 418-475K" (~9000 vertices por cuadro, no
  los ~2300 de "Objetos animados de las uniones"), 30000-32000 vertices en total, dib 29-31 ms, 29-31 fps durante ~4 s
  (sin el dragon la misma union va a 43-45 fps con 30000 vertices y 18-19 ms). Lo caro es la CPU: bo_dibuja_malla
  mezcla en la CPU dos cuadros de cada vertice y lo manda por dibuja_malla. Por mirar: si 4044E0 / 4F0E40 eligen un
  solo nivel de detalle del dragon (aqui se dibujan todos los nodos del registro que pasan la esfera y el umbral 8) y
  cuantos objetos del conjunto 11 salen a la vez.
- **NEWY: zonas naranjas lisas de bordes rectos en el cielo** (SIN RESOLVER, PENDIENTE). Reproducido con `-DPRUEBA_JUEGO=3
  -DPRUEBA_NODO=14` (NEWY desde la salida, 0'22"-0'24": lote1_newy_cielo_naranja_hoja.png): una zona naranja lisa con
  un borde vertical recto encima del parque. Con `-DSIN_NIEBLA_LIMITE` (sin el velo del limite, NLIM_A 0,3 desde 0,9 x
  lejos) no se vio en las capturas de 0'23"-0'26" (otra posicion: no concluyente). Hipotesis por comprobar: edificios
  lejanos del fondo enteros en el color de la niebla (la del original, scn_env_fog, o el velo) contra un cielo mas
  oscuro; o mallas del cielo de NEWY ordenadas por la z (con niebla en el cielo la z manda: "Bifurcaciones: texturas
  que se cruzan en el cielo"). Siguiente paso: `-DDIAG_CONGELA` / el mismo cuadro con y sin el velo y con
  `-DDIAG_SIN_TRAMO=0` (si la zona sigue sin la etapa, es el cielo).

Pendiente del lote 1: EAST (la roca de la etapa), NEWY (el cielo), el dragon de RUIN -> IMPE (CPU), las uniones de la
lista no repasadas una a una con 576 (CLOU, INDU, SNOW, RUIN y las del arbol 291, arboles 0 / 1 e INFARTO) y una pasada
de una MISION SUPERADA de C2C en Flycast. Datos regenerados: FINC2C.DAT (paso fin de tools/regenera.py,
PYTHONHASHSEED=0), copiado a la carpeta de datos compartida.

## Verificacion independiente de la revision 44 (2026-11-14, w7)

HEAD limpio (8f1b2aa, `git archive` en /dev/shm/w7, 0 avisos al compilar), datos de la carpeta compartida sin tocar,
Flycast en el display 94. Capturas en el scratchpad capturas_r44/w7/ (hojas de contactos y `*_analisis.txt`: cargas,
POP de mas de 1300 ms, cuadros con la etapa a 0 KB, fps minimos por etapa y mallas fuera del tope).

Pruebas del menu (GDI sin defines salvo donde se dice): nav.sh con MODO_JUEGO 0..3 (C2C 1-1 BEAC, OUTRUN arbol 0 por el
tablero 0x24, INFARTO arbol 0, CONTRARRELOJ arbol 260 por 0x24 / 0x25 / 0x26), sp.sh SP_LADO 0 (preset 0, arbol 1) y 1
(preset 1, arbol 0), resultados.sh, vmu.sh, vmu_diag.sh ("releer: BIEN", restaura 0), reintenta.sh en C2C y en OUTRUN
(las tres lineas SALIDA iguales), salon.sh y tablero.sh (-DPRUEBA_SALON_MILLAS=20000: compras, guardados, arbol 230 y
arbol 1), final.sh y seguidas.sh (-DPRUEBA_PILOTO_MENU -DPRUEBA_NODO=13 -DPRUEBA_FIN=300 -DPRUEBA_SIN_TIEMPO
-DDIAG_PVRMEM; en OUTRUN las 7 carreras con RAMMAPA 12546-12555K y la zona del curso 1248K en todas). En ninguna
"Out of memory", "pvr_mem: ERROR" ni "PVRMEM: sin / no pedido". Tres fallos de las pruebas (no del juego), arreglados:
- sp.sh SP_LADO=1: "CONGELADO a los 95 s": con el lado 1 (PALM) el tiempo se acaba antes de los 120 s y la pantalla de
  rankings queda quieta. Ahora para al ver "CURSO: tiempo agotado".
- tablero.sh salia con 1 sin mensaje: al_menu prueba START cada 3 s con espera_log, que pone ERROR=1 en cada intento
  sin la musica del menu. Ahora los intentos no cuentan como error.
- final.sh: "no sale 'FIN: modo 27' / 'FSP: frontal'": el final 18 dura ~20 s y los modos 25, 27 y 28 salen durante
  las fotos fi_00..14; cada espera mira ahora desde el modo 24. El juego paso por los modos 24, 25, 27, 28 y el frontal.

Fallos de los barridos (barridos repetidos con `-DPRUEBA_AUTO -DPRUEBA_SIN_TIEMPO -DTA_F_MIN=576 -DDIAG_GD_LENTO=750
-DDIAG_MS -DDIAG_POPPING -DDIAG_TEX_VRAM` y `-DPRUEBA_JUEGO=2` (arbol 290 entero hasta CAPE), `=3` (291 hasta NEWY),
`=1` (arbol 1) y `=0` (arbol 0)):
- Uniones con ta_f 2,25 (576): CERRADO. Ningun cuadro con "TA etapa 0K/0K" en los cuatro arboles (las 14 uniones del
  290, las 14 del 291, BK00/02/04/07/12 y BK15/17/20/23/28); calzada, muros y cielo en todas las hojas; fps minimos
  33-44 (PRIN 33,4, PALM 34,9; medios 44-45). No repasado: INFARTO con 576.
- Tirones de carga con el GD lento (750): CERRADO. Ninguna linea POP de mas de 1300 ms en ningun arbol; SEG08 12,70 s,
  SEG13 12,11, SEG15 12,24, SEG29 (MAYA) 13,06, SEG22 (LASV) 11,81, todas leidas antes del final de la union.
- TA_F_MIN=768: CERRADO solo en PALM; SIGUE en LAKE. En PALM la calzada y lo cercano estan en todos los cuadros (lo
  lejano entra y sale y hay trozos de colina sueltos). Al pasar a LAKE con trafico (POPTA: coche 1043K, pareja 256K,
  trafico 220-321K) queda "TA etapa 0K/0K" en muchos segundos y el coche va sobre el plano del agua sin calzada
  (t768_lake_sin_calzada_b0028.png, 1'25"-1'28"): ni el reparto en la clase -1 ni ta_lej_tope dejan nada a la etapa.
  Sigue sin medir el factor real de la consola.
- "lejos" de METR arrastrado a RUIN: CERRADO (POP: METR acaba a 1650 m y RUIN empieza a 4000). PERO la zona blanca bajo
  el arco al principio de RUIN SIGUE con 576 (b290_tope576_ruin_arco_blanco_lejos4000.png): no es "lejos" sino el
  reparto del TA (POP de RUIN "clase 12-16 d0" con 576; con 448 "clase 20-27 d19" y la zona blanca es mucho menor;
  ruin_nodo12_tope576_hoja.png / ruin_nodo12_tope448_hoja.png, desde la salida de RUIN sin METR delante). Con 448 queda
  un resto pequeno tambien en la captura del lote 2. PENDIENTE: el terreno lejano visto por el arco cae en una clase de
  distancia que el reparto quita.
- EAST antes de la union (roca que flota): SIGUE (b291_east_roca_hoja.png, 14'07"-14'10"), como dice el lote 1.
- BEAC caja gris sobre el agua: SIGUE (b291_beac_caja_gris_hoja.png, 0'04"9; BEAC 25155-25209 mallas fuera del tope en
  los primeros segundos, "falta 280-318 %").
- C2C MISION SUPERADA: CERRADO en Flycast. seguidas.sh con MODO_JUEGO=0 (s1/s2/s5_35) y la prueba c2c_y (scratchpad
  w7/c2c_y.sh): "A SIGUIENTE RONDA, Y REINTENTAR, ABANDONAR B"; X no hace nada y Y da "FIN: modo 30 (REINTENTAR)" y la
  misma SALIDA (c2c_35_hoja.png, c2c_y_hoja.png).
- Pareja deformada con la foto 3: SIGUE (foto3_pareja_hoja.png: brazos y torso retorcidos en toda la pasada de PALM).
- GHOS, haz de los faros con el cuadrilatero de borde recto: SIGUE (b290_ghos_haz_hoja.png), sin cambio a proposito.
- Texto "< X  X >" con el HUD y el piloto: CERRADO (no sale en las hojas miradas: antes de PALM -> LAKE, EAST -> MAYA,
  METR -> RUIN, GHOS -> TULI y FLOR -> NEWY).
- NEWY, zonas naranjas lisas de bordes rectos en el cielo: SIGUE (b291_newy_cielo_naranja.png, 18'52" y 19'22").
- seguidas.sh sin MODO_JUEGO: CERRADO (va por OUTRUN, 7 carreras, recorrido completo).
- salon.sh: CERRADO (OUTRUN por el tablero 0x24, sa_8 / sa_9 / sa_10 la seleccion del F50 y MUS33).
- menu.sh con MODO_JUEGO=3: CERRADO (nav3: tableros 0x24 / 0x25 / 0x26 y arbol 260).
Sin regresiones vistas: la etapa que se deja se suelta en la union con la rama decidida a 64-66 m ("union + n m") en
todas las uniones; RAM y VRAM igual en las carreras seguidas; TEXVRAM "sin subir 0, gris 0" en las 1785 lineas.

## Revision 45: el edificio volando de BEAC (tabla 2 de los registros) y el recorte en la PVS (2026-11-14)

Foto del usuario (revision 44, COAST 2 COAST 1-1: BEAC -> LRBK -> PALM_T, puesto 6/6, tiempo 80): sobre el mar, a
la izquierda de la carretera de la costa, "un trozo de edificio volando" (capas de ventanas y una base marron sin la
caja). Reproducido en Flycast con el TA de siempre (448; `-DPRUEBA_AUTO -DPRUEBA_MISION=0 -DPRUEBA_SIN_TIEMPO
-DDIAG_MS -DDIAG_POPPING`): BEAC s 1125-1357 (no al final de BEAC: en la primera recta de la costa, con la barra de
Inicio-Meta al principio), capturas_r45/edificio_antes.png. No es el reparto del TA, ni VISO, ni los grupos.

- **Causa (CORREGIDO, conversion)**: la tabla 2 de cada registro del modelo (16 bytes por entrada) no va en paralelo con
  la tabla 3 (las filas de 24 bytes de los paquetes del VIF): cada entrada es (material, material, filas, primera
  fila de la tabla 3), y la tabla 1 (la malla de cada nodo: n opacas, n de mezcla, primera opaca, primera de mezcla)
  cuenta ENTRADAS de la tabla 2. PS2 0x110700 (el dibujo de la malla de un nodo, desde 0x1114e0): recorre las
  entradas de la tabla 2 que da la tabla 1 (+0 cuenta, +8 la primera; de 16 en 16 bytes), de cada una +4 (ya el
  puntero a la entrada de la tabla 4, cuya +0xC es la textura), +8 filas y +0xC la primera fila de la tabla 3, y
  dibuja esas filas de 0x18 en 0x18. En 1187 de los 1204 registros de los segmentos cada entrada es (1, i) y da igual;
  en 7 bloques de etapa (los unicos de los 403 _pmt de la ISO) alguna entrada lleva 2 filas: cs_CS_BEAC (SEG17,
  registro 3: 968 filas para 967 entradas), BEAC_T (SEG18), BEAC_BT (SEG19, sin uso), GHOS_R (SEG42, registro 2:
  470 / 467), BEAC_R (SEG50, 553 / 548), BEAC_BR (SEG51, sin uso) y EAST_R (SEG65, registro 7: 312 / 311).
  tools/modelo.py leia la tabla 1 como filas de la tabla 3 y la textura de la fila i en la entrada i: desde la
  primera entrada doble, cada tira iba al nodo anterior al suyo y con la textura de otra entrada. Tiras fuera de la
  esfera de su nodo: SEG17 3001 de 24992 (727 nodos), SEG18 2981 (720), SEG42 6177 (643), SEG50 2574 (405), SEG65
  12281 (429); las demas etapas 0-24 sueltas (mallas un poco fuera de la esfera del original). La demo recorta cada
  nodo por su esfera (fuera de la vista, LOD, PVS): en BEAC la fachada de una torre (textura 93) iba con la esfera
  del nodo 494, a 700 m (`-DDIAG_EDIF`: "razon detras" con la malla en la pantalla), y lo que si se dibujaba llevaba
  texturas de otro: el "edificio" de la foto eran trozos con la textura de fachada de otras mallas; las "cajas
  negras" sobre el agua de la misma captura eran los carteles (AMD, Shell, Bridgestone, Vodafone, SEGA) con otra
  textura.
  Arreglo: `modelo.filas_t3(blq, reg)` ((k, filas) por entrada de la tabla 2; en paralelo si la tabla no tiene ese
  formato) en Modelo.bloque (con y sin nodos), texturas_de_carteles, conv_tramo.puntos_asfalto y conv_bo
  (tiras_de_malla / cuadros_de: entradas de la tabla 2); modelo.nodos cuenta hasta las entradas de la tabla 2.
  Rehecho, tiras fuera de su nodo: 0 en SEG17. conv_tramo avisa ahora ("AVISO: n tiras fuera de la esfera de su
  nodo") si vuelve a pasar.
  En el original: la mascara de cs_CS_BEAC_bin (0x1112f8, conv_viso.mascaras) en las secciones 140-170 (s 1125-1357)
  lleva los nodos 493..517 y 912 (las torres de ese tramo): el original dibuja los edificios enteros, como ahora.
  Verificado en Flycast (448): el mismo sitio sin nada volando y con los carteles en la orilla
  (capturas_r45/edificio_despues.png, edificio_antes_despues.png). Arbol 291 con `-DTA_F_MIN=576` (BEAC_R): la "caja
  gris lisa sobre el agua al salir de BEAC" de los barridos (w4, lote 2, w7) era esto y ya no esta
  (beac291_caja_antes_tope576.png / beac291_caja_despues_tope576.png, hojas beac291_hoja_*). La roca de EAST antes de
  la union (291) NO: el 291 usa EAST (tramo 29, SEG33, sin entradas dobles) y sigue igual
  (east291_roca_antes_*.png / east291_roca_sin_cambio_*.png); sigue PENDIENTE como dice el lote 1.
  Etapas afectadas por modo: SEG17 (BEAC) en el arbol de OutRun 2 SP (csc_data_cvt) y las misiones 0, 4, 8, 16, 36,
  38, 50, 60, 72, 88; SEG18 la 80; SEG42 (GHOS_R) 27, 42, 49, 84, 92; SEG50 (BEAC_R) 5, 24, 46, 48, 61, 81, 83, 85,
  93; SEG65 (EAST_R) 33, 70, 78, 81, 86, 93 (y los arboles de los cursos seguidos que las usan).
- **PVS y el recorte (CORREGIDO, herramienta)**: tools/c/pvs.c dejaba tapar a una malla de la lista de recorte (prueba
  de alfa) un pixel entero si el texel del CENTRO del pixel era opaco; el raster del PVS va a K = 128 pixeles por
  unidad de tangente (unas 4 veces menos que la pantalla), y lo lejano (texeles menores que el pixel) tapaba por los
  huecos de las ramas y las ventanas lo que se ve a traves. Ahora solo tapa si TODOS los texeles del pixel (el
  rectangulo de las UV de sus cuatro esquinas con la perspectiva, por la tabla de sumas de la mascara erosionada) son
  opacos. Medido en BEAC (puntos 0-400 de la ruta): 8113 de 25139 mallas se ven en alguna seccion mas (+1,3 % de los
  pares malla-seccion; ninguna se pierde). Para el edificio de la foto no era esto (el nodo 516 queda fuera tambien
  sin el recorte: su geometria estaba a 1400 m de su esfera). Datos: aplicado a las cinco etapas rehechas; las demas
  etapas y uniones siguen con la PVS de antes hasta rehacerla (`python3 tools/pvs.py SEGnn.DAT ...` sobre las
  recompuestas y volver a empaquetar; unas 5-10 min por etapa).
- **Grupos de nodos, VISO y la union (SIN CAMBIO, pregunta)**: los grupos (tools/grupos_nodos.py) ya se aplican en la
  etapa siguiente y en la union (nodos_visibles de los tres tramos; cada objeto esta en un solo fichero). Las mascaras
  del original (VISO.DAT: seccion 0 y ultima de cs_CS) dejan a medias 388 grupos en la seccion 0 y 318 en la ultima de
  las 62 etapas (p. ej. SEG58: de un grupo de 17 nodos, 6 fuera): el original dibuja solo esas partes desde la union. No
  se ha cambiado (seria dibujar mas que el original). PREGUNTA al usuario: si se quiere, VISO puede encender el
  grupo entero cuando el original enciende alguno de sus nodos (DESVIACION).
- Medidas nuevas: `-DDIAG_EDIF` (con `-DDIAG_POPPING`; DIAG_EDIF_S, DIAG_EDIF_X0/X1/Y0/Y1): al pasar por la s dada,
  cada malla de los tres tramos con el centro en ese rectangulo de la pantalla: nodo, lista, textura, razon del nodo
  (POPR), bit de la PVS y la esfera del nodo.
- Datos (PYTHONHASHSEED=0): SEG17, SEG18, SEG42, SEG50 y SEG65 rehechas con `tools/regenera.py --interno seg <iso>
  <dir> <ti>` (ti 15, 61, 37, 45, 59), la PVS nueva (`tools/pvs.py` sobre cada una) y `tools/revq.py` (0 KB: ya
  salen con la segunda pasada), copiadas SUELTAS (ORD7 enteras, como las 8 que ya iban sueltas) a la carpeta de datos
  en lugar de sus descriptores; SEGS.PAK no se toca (sus trozos de las cinco viejas quedan sin uso, 254 MB; volver a
  empaquetar todas con pak_union.py no da los mismos bytes que la carpeta compartida en 6 descriptores y SEGS.PAK).
  Las cinco COLnn.DAT salen iguales byte a byte y VISO.DAT (conv_viso.py) tambien (los nodos no cambian). En una
  regeneracion completa (regenera.py) salen ya bien y empaquetadas.

## Revision 45: las palmeras que aparecen de golpe (2026-11-14)

Queja del usuario (Dreamcast real, revision 44): "sigue habiendo popping en las palmeras". Detalle y cifras en
docs/PLAN.md (Popping, 7. Las palmeras).
- **No es el original**: el LOD de 404B00 quita las palmeras a 3,75 px de radio (umbral 7, bandera 0x40 en 170 de las
  174 de PALM; 7,5 px los arboles con umbral 6): en PALM a una mediana de 1126 m (457 m el 10 % mas pequeno) con la
  camara de carrera (fx ~380). Sin cambiar: el LOD del original no hace saltar vegetacion de 8 px o mas (0,03-0,07 por
  segundo, en su umbral).
- **Es el reparto del TA del port** (DECISION DEL PORT, `nodos_visibles_todos`; DIAG_POPPING, linea POPVEG nueva):
  6,5 apariciones por segundo de vegetacion de 8 px o mas en PALM con el TA de Flycast (448; 0,6 de 32 px o mas) y 11,9
  (5,0 de 32 px o mas, a 12-150 m) con TA_F_MIN=576; BEAC 3,3 / 5,4. La PVS 0,8-1,8; grupos de nodos, niebla y velo
  nada. Las palmeras son lo mas caro por pixel (7,3 KB teoricos del TA cada una; la vegetacion de PALM, 1453 KB, mas
  que todo el tope de la etapa) y el coste por vertices del reparto (REPARTO_COSTE) las quita antes que los edificios.
- Probado y descartado (Flycast, PLAN.md): sin coste para los vertices de recorte, el recorte a la mitad, histeresis
  12 / 6, REPARTO_CERCA 0,40. Lo que ganan las palmeras lo pierde el resto (mas saltos de 32 px o mas y mas pantalla sin
  dibujar) o ni eso; el reparto se queda igual. Unir las hojas en tiras largas (idea del usuario): +63 % de TA por
  palmera (los trozos de 6 triangulos llevan su cabecera; los degenerados anaden vertices); las hojas ya usan el alfa
  de recorte de su textura y no hay geometria de silueta que quitar. Sin cambio de datos.
- Capturas congeladas (scratchpad capturas_r45/palmeras_*): crono 2400 (s 3605, la playa de PALM): sin tope y con 448
  estan las palmeras del borde derecho de la carretera; con 576 faltan 6-8.
- PENDIENTE (del usuario): la linea `TA ... x..` de la pantalla de medidas de su consola tras una vuelta por PALM (dice
  si su consola es la de 448 o la de 576) y decidir si se busca sitio en el TA (el coche del jugador gasta 608 KB, el
  45 % del bufer con la pareja; mas bufer pide VRAM).
- Nota: las pruebas de BEAC usaron la carpeta de datos compartida durante el cambio de la tabla 2 de los registros
  (otro trabajo, mismo dia); las de PALM no dependen de el.

## Revision 46: palmeras lejanas en version de bajo coste (DESVIACION pedida, 2026-11-14)

Peticion del usuario tras la revision 45: "No quiero recortes; en tal caso, las palmeras mas lejanas que se vean pero
en version low-poly, y cuando te acerques que se vean bien". **DESVIACION pedida**: el original no tiene LOD de la
vegetacion (los nodos de las etapas de PS2 solo traen la malla 0; 404B00 solo quita por tamano, 3,75 / 7,5 px) y la
dibuja entera hasta ahi; la Dreamcast no puede (revision 45: el reparto del TA las quitaba de lejos). Detalle y medidas
en docs/PLAN.md (Popping, 8).
- **Version lejana** (tools/lod_veg.py, en la conversion: conv_tramo.py y conv_union.py tras la PVS; en una carpeta ya
  hecha, `tools/regenera.py --interno lodveg datos/ trabajo/`): para cada nodo de vegetacion de recorte (el criterio de
  POPVEG: mas de la mitad de sus vertices en la lista de recorte) que cuesta al menos 1500 bytes teoricos del TA y que
  se ve igual desde cualquier lado (en planta no alargado, alto, silueta abierta, muchas piezas, no azulado: palmeras,
  arboles, arbustos, pinos; tambien unas farolas, la cabina del teleferico y ruinas pequenas; no barandillas, vallas,
  setos, macizos de flores, fachadas ni carteles), un impostor: un cuadrilatero orientado a la camara alrededor de la
  vertical (como los carteles del original, face_camera de 404D10: carga_cartel) con la imagen del propio nodo dibujada
  por software desde su geometria, sus texturas y el color de sus vertices tal y como estan en el fichero (la luz del
  original cocida; nada de arte nuevo): vista desde la direccion por la que llega la camara (la ruta hacia atras),
  4 x 4 muestras por texel, el recorte de la PowerVR (alfa >= 0,5). Los nodos del mismo modelo (las mismas tiras con
  otra matriz) comparten imagen por rumbo (8). Imagenes de hasta 62 texeles de lado en atlas VQ ARGB1555 (lista de
  recorte). Coste en el TA: 108 bytes teoricos por impostor frente a ~7300 de una palmera de PALM (x 1,75 = 189 frente
  a 12800 bytes contados): **62-94 veces menos** (PALM 1334 KB -> 18 KB teoricos para sus 179 nodos).
- **En la demo** (`nodos_visibles`, LOD_LEJ_PX 48 px de radio en la pantalla de 640x480, histeresis LOD_LEJ_HIST 1,25):
  por debajo del umbral se dibuja el impostor; la visibilidad, el LOD del original (sigue quitando lo mismo por debajo
  de 3,75 / 7,5 px), la PVS y la clase de distancia son las del nodo completo; el impostor va en la clase de LOD 0 del
  reparto (con lo que no se quita: ~22 por cuadro, 4 KB). VISO.DAT compara con los nodos del original (`nn0`).
- **Medido en Flycast** (PRUEBA_AUTO 240 s, por 45 cuadros; antes = el mismo fichero con LOD_LEJ_PX=0): apariciones
  por el tope del TA de la vegetacion con version lejana (>= 2 / 8 / 32 px por s): PALM con 448 2,76 / 2,58 / 0,24 ->
  **0,01 / 0,01 / 0,01**; BEAC con 448 2,05 / 2,00 / 0,39 -> **0,10 / 0,10 / 0,10**; con TA_F_MIN=576 PALM 2,44 / 2,44 /
  2,14 -> 1,37 / 1,37 / 1,37 y BEAC 2,55 / 2,54 / 2,15 -> 1,04 / 1,04 / 1,04 (las que quedan son palmeras completas de
  25-50 m y 60-130 px que no caben en la salida: el reparto quita lo cercano). Pantalla sin dibujar por el tope (PALM
  448) 6,3 -> 1,9 %; fps iguales (43,3 / 43,2; dib 18,13 -> 18,21 ms). Captura congelada crono 2400 (playa de PALM) con
  576: antes 1 palmera del borde derecho, ahora las 8 (capturas_r45/palmeras_lod_c2400_zoom.png: 576 antes, 576 ahora,
  todo completo sin tope, todo lejano sin tope).
- RAM: el hueco de la etapa sube 44 KB (SEG46 8116 -> 8160 KB; la mayor). VRAM: hasta 44 KB por etapa (atlas; PALM 40).
  Sin "Out of memory". Las uniones casi no tienen vegetacion asi (4 BK de 498 cambian).
- Lo que sigue apareciendo de la vegetacion por el tope es la que no tiene version lejana (barata, menos de 1500 bytes,
  o alargada: setos y macizos de flores) y, con 576, palmeras cercanas completas.
- Datos (PYTHONHASHSEED=0): `tools/regenera.py --interno lodveg` sobre una copia de la carpeta compartida
  (scratchpad/dcwork/datos2): 40 SEG y 8 BK cambian; SEGS.PAK, UNIONES.PAK, UNIONESTA.PAK y UNIONESR.PAK
  reempaquetados (pak_union.py, comprobado byte a byte) y 370 ficheros (paquetes y descriptores) copiados a datos2: 914
  MB (antes 955: SEGS.PAK ya no lleva los trozos sin uso de las cinco etapas de la revision 45). Probado con esa carpeta
  (PALM y BEAC, 448): las mismas cifras, hueco de la etapa 8160 KB, 0 "Out of memory". En una regeneracion completa
  conv_tramo / conv_union ya lo hacen.

## Rev 2.9.9.b: las voces de la copiloto otra vez (correccion)

El "Out of memory ... diff 217088" que desde la 2.9.5 se daba por inofensivo en estas notas NO lo era: 217088 es
VOCES.DAT (216990 bytes) redondeado a 4 KB. Al arrancar ya no cabia seguido en el monton, `voces_inicia` fallaba,
`voz_ok` quedaba a 0 y en la carrera no sonaba ninguna voz de la copiloto (lo noto el usuario). Arreglo: VOCES.DAT se
lee en el hueco de la union (como ya hacia `sfx_voces_juego` con VOCESH/VOCESC), y si al arrancar no se puede, se
reintenta al empezar la carrera (`voces_inicia_en`). Medido en Flycast: "VOZ: VOCES.DAT, 35 muestras, 210 KB en el
AICA" al arrancar y ningun "Out of memory". Las notas anteriores que llaman "conocido" a ese aviso estan equivocadas.

## Rev 2.9.9.c: huecos al entrar en las etapas (el crecimiento espera a ver la etapa de frente)

Queja: "corregir el crecimiento de las etapas para que no se aprecien la falta de elementos (huecos) como en la
montana del gran canon". Todo medido en el Flycast de aqui (NO en la consola ni en Android).

### Causa (medida)

- No falta nada por cargar. Con el modelo entero de SEG24 dibujado fuera de la consola (todas las mallas, sin PVS, sin
  reparto y sin recorte) desde la camara del juego en la union YOSE -> GRAN, sale el mismo trozo: es toda la geometria
  que hay. (La camara se saco del cuadro congelado, `-DDIAG_CONGELA=4400 -DDIAG_FALTA`, y luego de la linea `GEOC:`
  de `-DDIAG_GEO`: la camara en el marco de la etapa siguiente.)
- Las dos ramas de la bifurcacion (la misma geometria en todas las uniones de los arboles 0 y 1: medido en 16 entradas)
  llegan a la etapa siguiente por una curva de 90 grados. Entre las posiciones 133 y ~215 de la union el coche va
  PERPENDICULAR al eje de la etapa siguiente, de 770 a 100 m a un lado y ~300 m antes de la entrada. El original
  (44C530) hace crecer la etapa entre la posicion 0x84 y ~157: desde ahi hasta la entrada (~posicion 243, unos 700 m)
  la etapa, entera, se ve DE LADO Y POR FUERA. Su decorado solo esta hecho para verse desde su carretera.
  - GRAN: las paredes del canon vistas por detras, acabadas de golpe donde acaba la etapa, encima de los muros de la
    union; y lo de arriba de la meseta (SEG24 nodo 60, 31 mallas de recorte entre 219 y 484 m de alto) visto por
    debajo: una losa oscura (cortada, ademas, por DETRAS_ENTRADA en el plano de la entrada).
  - MACH (arbol 1, rama izquierda desde NIAG): sus fondos de cielo (SEG27 nodos 1069-1073, cilindros azules de 2,1 a
    2,7 km) vistos a medias: una raya vertical que parte el cielo en dos (el de la union a un lado, azul liso al otro)
    y una silueta palida gigante. Es la queja de las "bandas dividiendo el cielo en las bifurcaciones" en esa union.
- SUPUESTO: el original ensena lo mismo en GRAN (la lista de VISO.DAT da la misma imagen, medido por el agente
  anterior; no hay video de esa union para compararlo).
- Lo que NO era: texturas sin subir, reparto del TA, lejano, LOD ni la PVS de la seccion 0 (rellenar los nodos
  enteros, `PVS_ANTES` del WIP, no cambiaba la imagen y costaba TA: 526K -> 570K en GRAN; QUITADO).

### Arreglo (`demo/main.c`, todo DESVIACION)

- `GEO_RUMBO` (1): el factor del crecimiento (7D3184, `geo_paso`) no pasa del que da el rumbo del coche a un punto 200 m
  dentro de la etapa siguiente por su eje (`geo_rumbo_f`): 0 con mas de 32 grados, 1 con menos de 14, y no baja. En la
  bifurcacion la etapa empieza a verse en la posicion ~183 (antes ~135) y esta entera en la ~209, unos 280 m antes de
  la entrada (antes en la ~157): crece en ~210 m, como los 200 del original, pero cuando el coche ya gira hacia ella.
  Mientras tanto el cielo de la union queda limpio. En una union recta el rumbo es ~0 y queda el factor del original.
  `-DGEO_RUMBO=0`: lo de antes.
- `SIG_PANO_R` / `SIG_PANO_ALTO`: con la camara antes de la entrada no se dibujan los "techos" de la etapa siguiente:
  nodos de 600 m de radio o mas, todo de recorte, cuya esfera pasa por detras de la entrada y con todos sus vertices
  150 m o mas por encima de ella. En las 30 etapas de los arboles 0 y 1 solo cae GRAN 60 (comprobado con los datos). Desde
  la entrada, mirando por el eje, ninguna de sus mallas toca la pantalla: al pasarla no salta nada. (La regla del WIP,
  sin la altura, cogia tambien fondos que si se ven desde la entrada: YOSE 233-235, CLOU 1469, SNOW 182 con 617 mallas,
  IMPE 1208 / 1226...; estrechada.)
- `SIG_CUP_F`: tampoco salen hasta pasar la entrada los fondos de cielo que rodean la etapa por delante (nodos de mas de
  2000 m de radio con el centro medio radio o mas por delante de la entrada: solo MACH 1069-1073 y SNOW 176). Una
  primera version los dibujaba enteros (sin el corte de DETRAS_ENTRADA): la raya seguia (DESCARTADA).
- `DETRAS_X` / `DETRAS_Z`: DETRAS_ENTRADA usa en los carteles el centro de su nodo (sus mallas van en el marco del
  nodo y su centro no dice donde estan: en SANF 96 nodos-cartel a 1,6-3,1 km por delante de la entrada se quitaban en
  toda la union).
- `-DDIAG_GEO` escribe ademas `GEOC:` (camara, rumbo y factor en el marco de la etapa siguiente).

### Medida

Flycast de aqui, tres a la vez, piloto automatico. "Antes" = 467dd28 (Rev 2.9.9.b). Capturas (antes | despues) y
registros en el scratchpad, `m29c/huecos/`.

- Lo que se ve:
  - YOSE -> GRAN (`gran_1m13..1m17_antes_despues.jpg`): antes, de 1'12" a 1'17", el trozo de pared y la losa oscura en
    el cielo; despues el cielo limpio hasta 1'15", el monte creciendo detras del muro de 1'15" a 1'16"4 y el canon
    entero a 1'17"4, igual que antes.
  - NIAG -> MACH (`mach_1m19..1m21_antes_despues.jpg`): antes, la raya vertical y el azul liso de 1'19" a 1'21"5;
    despues no salen.
  - Otras 14 entradas (arbol 0: ALPI, DESE, GHOS, CAPE, LAKE, CAST, CLOU, TULI; arbol 1: SANF, NIAG, EAST, YOSE, AMAZ,
    NEWY), con una captura por segundo en la union: sin trozos sueltos antes ni despues; despues la etapa sale ~1,5 s
    mas tarde. (AMAZ y NEWY solo se miraron con el WIP.)
- fps y cuadros tarde, sin los -D de diagnostico (`-DDIAG_MS -DDIAG_POPPING` solo; el emulador es determinista: dos
  tiradas iguales dan las mismas cifras):

| Tramo | fps antes | fps despues | tarde antes | tarde despues | segundos con el reparto hundido (corte < 1000 m) antes / despues |
|---|---|---|---|---|---|
| YOSE entera (hasta GRAN) | 39,12 | 39,18 | 47 | 44 | 3 / 0 |
| YOSE, la union (s >= 6000) | 38,11 | 38,51 | 12 | 10 | |

- Con `-DDIAG_GEO -DDIAG_FALTA -DPRUEBA_SALTO=3000` (los del barrido de las uniones; con ellos la logica cuesta 2-3 ms
  mas en la union por las lineas que escribe, y "despues" escribe el doble): PALM 33,90 -> 33,66 fps (tarde 322 -> 328),
  LAKE 36,21 -> 36,15 (146 -> 147), CAST 36,13 -> 35,91 (167 -> 188), CLOU 32,87 -> 32,90 (275 -> 265), ALPI 37,14 ->
  37,19 (114 -> 111), DESE 37,46 -> 37,25 (111 -> 118), GHOS 35,27 -> 35,04 (200 -> 209). Segundos con el reparto
  hundido: PALM 18 -> 13 y 20 -> 15 (dos tiradas), ALPI 7 -> 4, DESE 8 -> 4, GHOS 2 -> 0, CAST 21 -> 16, CLOU 13 -> 7,
  LAKE 7 -> 8. SUPUESTO: la diferencia de "tarde" de estas tiradas es de las lineas de diagnostico (en la tirada
  limpia de YOSE no la hay); no se repitieron limpias.
- RAM: "VOZ: VOCES.DAT, 35 muestras" y ningun "Out of memory" en todas las tiradas. Lo nuevo son 16 int32 y 4 int de bss.
- Pendiente / no medido: las uniones de las misiones y de C2C (otros arboles), la meta, y la consola real. En la
  entrada de MACH no se ha mirado cuadro a cuadro si los fondos de cielo saltan al pasarla (en las capturas, una por
  segundo, no se nota).

## Rev 2.9.9.c: bandas en el cielo en las bifurcaciones (2026-11-27)

Queja del usuario (Flycast de Android): "fallos graficos en el cielo (aparecen bandas dividiendo el cielo) en las
bifurcaciones". Todo lo de aqui esta medido en el Flycast de Linux; nada en la consola ni en Android.

LO QUE SE HA MIRADO (9 cruces de union con capturas cada 0,5-0,7 s: PALM -> LAKE, YOSE -> GRAN, GRAN -> AMAZ, LAKE -> FORE,
CAST -> INDU, CLOU -> METR, INDU -> RUIN, SNOW -> IMPE, AMAZ -> NEWY; y GRAN -> AMAZ y AMAZ -> NEWY cuadro a cuadro con
`-DDIAG_TRANS`)
- Datos: los cielos de la carpeta de datos de las pruebas ya estan hechos con el conversor arreglado (ps2/vif.py, ver
  "Cambios de etapa: cielo del fundido"). En las 62 etapas y las uniones no queda ninguna malla del cielo con textura
  y todas las UV a 0 (antes: dos de las cinco tiras de la cupula del fundido de PALM y las etapas con su cupula). En
  todo el modelo quedan 6286 mallas con UV a 0 (eran 28965); no son del cielo y no se han mirado.
- PERO tools/regenera.py no rehacia unos datos viejos: es idempotente y lo que ya esta en datos/ no se repite. Quien
  convirtio su ISO antes del arreglo del conversor sigue con las franjas lisas en la cupula del fundido, que solo se
  ve en las uniones (los tercios 2 y 3 del cambio de cielo): es la causa mas probable de lo que ve el usuario
  (SUPUESTO: no hay captura suya ni se sabe la fecha de sus datos). Ahora regenera.py lo detecta (`cielo_liso`: alguna
  malla del cielo de SEG00.DAT o de BK00.DAT, sueltos o en su paquete, con textura y todas las UV a 0) y, si es asi,
  avisa, quita las etapas (SEGnn.DAT, SEGS.PAK, VISO.DAT, REPSEC.DAT) o las uniones (BKnn.DAT, los tres UNIONES*.PAK y
  los intermedios de trabajo/) y sus pasos vuelven a la lista. Es Python con numpy, igual en Linux y en Windows.
  Probado con los datos de las pruebas (no los toca) y con una copia de SEG00 / BK00 con las UV de una malla del cielo
  a 0 (los detecta y quita lo dicho). La conversion entera detras de eso (horas) NO se ha vuelto a pasar.
- En los 9 cruces, con los datos buenos, no ha salido ninguna banda: el fundido va liso cuadro a cuadro. Lo unico
  parecido es del original: la cupula del fundido de NEWY (SEG30, grupo 1) lleva el color en los vertices (naranja a un
  lado, blanco al otro, gajos de 18 grados) y durante el tercio 3 (0,7 s) se ve un borde inclinado entre los dos tonos.
  Sin tocar.
- Mallas del cielo que no caben en el TA (`-DDIAG_CIELO`, linea "CIELO:"): 0 en todos los cruces, antes y despues.
- Fondo de la PowerVR a la vista (`-DDIAG_FONDO`, magenta, en PALM -> LAKE, YOSE -> GRAN y GRAN -> AMAZ): como mucho 217
  pixeles de 307200 en una captura (GRAN, en la etapa) y 115 dentro de las uniones (rendijas y trozos sueltos). Con la union quitada
  (`-DDIAG_SIN_TRAMO=1`) si se ve: por debajo del borde de la cupula (1,6 a 18 grados bajo el horizonte segun el cielo)
  quedaba el azul fijo del fondo con el borde recto del horizonte.

CAMBIOS EN EL CODIGO (DESVIACION los dos; son prevencion: en el Flycast de Linux la imagen de los cruces es la misma)
- CIELO_TOPE 0 (`-DCIELO_TOPE=1`: como antes): el cielo no obedece el tope de TA de la etapa (`ta_en_cielo` en
  `ta_cabe`), solo el del cuadro (`ta_lim`). La capa que se funde va entera a la lista transparente, la ultima, y
  llegaba con el tope de la etapa gastado por las listas anteriores; ademas `corte_nodos` aparta para el cielo lo que
  costo en el cuadro anterior (`cn_cielo`) y lo que no cabia no se contaba, asi que no se curaba solo. Ahora
  `cn_cielo` cuenta tambien lo que no cupo. No se ha visto fallar (0 mallas fuera en las medidas); en la consola
  `ta_f` y los topes son otros. `-DDIAG_CIELO` (o `-DDIAG_TRANS`) escribe "CIELO: ..." cada 30 cuadros si alguna
  malla del cielo no cupo.
- FONDO_CIELO 1 (`-DFONDO_CIELO=0`: el azul fijo): el color de fondo de la PowerVR es el del horizonte del cielo del
  momento (`fondo_pon`: `horizonte_de`, y en el fundido de la union mezclado del viejo al nuevo como la niebla). Donde
  el escenario no tapa lo de debajo de la cupula, o en las rendijas, ya no sale azul claro bajo un cielo naranja o de
  noche.
- Prueba: `-DPRUEBA_S0=<m>` empieza la carrera a esos metros del final de la etapa (para llegar a la union en 20 s).

NO HECHO / DUDAS
- Android: no se ha podido mirar. Si con los datos rehechos siguen las bandas hace falta una captura.
- En el fundido cada capa pasa de la lista opaca a la transparente al bajar de alfa 0,9 y deja de dibujarse por debajo
  de 0,1 (como el original, 452080): dos escalones del 10 % por tercio. No se ha tocado.
- Las mallas del cielo enteras por debajo del horizonte llevan niebla (cielo_grupo, por el mar de BEAC). En 43 de las
  62 etapas hay alguna; en varias (ALPI, CLOU, CAPE, GRAN...) es una sola malla de 12 vertices que cubre un sector, y
  el resto de esa altura va en mallas que cruzan el horizonte, sin niebla. Solo se veria con el horizonte al
  descubierto; no se ha visto en los cruces y no se ha tocado.

## Rev 2.9.9.c: optimizacion del dibujo (CPU y fps sin quitar nada) (2026-11-27)

Pedido: "optimizacion de codigo + FPS sin perder elementos en pantalla". La 2.9.9 costo -0,85 fps de media. Todo lo de
aqui esta medido en el Flycast de Linux (tres a la vez); nada en la consola ni en Android.

OJO CON LA MEDIDA: el tiempo de CPU de Flycast sale de contar instrucciones (un ciclo por instruccion, mas 2 por cada
uno de los tres primeros accesos a memoria de cada bloque; sh4_cycles.cpp). No cuenta fallos de cache. Lo que aqui gana
es quitar instrucciones y bloques cortos con lecturas; en la consola real la ganancia sera otra (SUPUESTO: del mismo
signo, porque todo lo hecho es hacer menos trabajo).

PERFIL (`-DDIAG_PERFIL`, PALM y METR en carrera, antes de tocar nada)
- `dibuja_etapa` 31-32 % del cuadro; de eso solo un tercio es enviar vertices (el bucle en ensamblador). Las mallas
  son tiras cortas (5-6 vertices de media): manda el trabajo por malla y por tramo, no el vertice.
- El recorrido de los tramos (dos pasadas por todos los tramos de la lista, miles, para dibujar unos cientos): 6 %.
- El coche y el trafico (`dibuja_indices` + `ta_cabe` + `dibuja_malla_directa`): 6 % sin contar sus vertices.
- La pareja (`robot_dibuja`): 7-8 % para 2898 vertices. `nodos_visibles`: 5 %. `pvs_recuerda`: 2 %.

CAMBIOS (`demo/main.c`; cada uno con su -D para volver atras; no cambian que se dibuja)
- `TRAMO_LISTA` (1): `tramo_sig` salta en un bucle corto los tramos de nodos sin dibujar, y la primera pasada de
  `dibuja_etapa` apunta (`dif`, 1024 en la pila) los tramos visibles de la segunda, que solo recorre esos. Mismas
  mallas en el mismo orden. Si no caben, la segunda pasada es la de antes (no ha pasado: maximo visto 282).
- `INDICES_FINO` (1): el coche y el trafico por un bucle con las cuentas del TA en variables locales, tambien cuando
  el modelo no esta entero delante (prueba de la esfera por malla, como antes).
- `RECORTE_TIRAS` (1): en una malla que toca el plano cercano, los triangulos seguidos que estan enteros delante van
  como un trozo de tira en vez de uno a uno. Mismos triangulos; menos vertices (PALM congelado: 16949 -> 16415) y menos
  TA. UNICA DIFERENCIA DE IMAGEN: hasta 35 pixeles de 307200 cambian 1 nivel de 255 (el orden de los vertices del
  triangulo cambia el redondeo de Flycast). Con `-DRECORTE_TIRAS=0` la imagen es identica pixel a pixel.
- `ETAPA_FINO` (1): fuera del bucle de las mallas lo que es igual para el tramo o el modelo (pvs_ent del nodo, el rayo
  del sol, DETRAS_ENTRADA).
- `PVS_RACHAS` (1): `pvs_recuerda` copia de una vez los tramos seguidos cuyo nodo sigue en la PVS (`pvs_copia`).
- `demo/Makefile`: `comun/menu.o` con `-Os`. No es por velocidad: lo de arriba anade 3,3 KB de codigo, que son RAM
  del monton (con los -D de medida, la RAM libre al arrancar bajaba de 261K a 253K). Los menus por tamano dan 19 KB.
  Compilacion normal: codigo 786488 -> 769120 bytes, RAM libre al arrancar 309K -> 333K (seguido en la cima 115K ->
  139K). Menus comprobados con tools/prueba/nav3.sh antes y despues (titulo, licencia, menu principal, modo, mision,
  coche, cambio, musica, carrera): iguales.
- Medida: la linea "MAL:" lleva ademas el mayor `dif` y las llamadas de `dibuja_indices` por cada bucle.

PROBADO Y QUITADO (no ganaba)
- Saltar en la pareja la matriz que no pesa en ningun vertice de la malla: +0,33 ms (peor).
- Ahorrar la prueba de la esfera de cada malla si la del nodo esta entera dentro de la vista: solo el 22 % de los
  nodos contienen las esferas de sus mallas (PALM 614 de 2740) y 28 de 373 tramos estaban dentro: +0,14 ms (peor).
- `-fno-math-errno`: 23,58 frente a 23,51 ms (nada).
- No tocado: el bucle de vertices (ya esta en 27 instrucciones), `nodos_visibles`, la pareja, `coche_entorno`.

MEDIDA 1: el mismo cuadro congelado (`-DDIAG_CONGELA=1500`, con `-DDIAG_FALTA`), ms de dibujo por cuadro (mediana)

| Etapa | antes | despues | triangulos | vertices antes / despues | pixeles distintos (de 307200, 1 nivel) |
|---|---|---|---|---|---|
| PALM | 15,95 | 13,37 | 10989 = 10989 | 16949 / 16415 | 35 |
| CLOU | 16,24 | 14,23 | 10752 = 10752 | 17322 / 17258 | 0 |
| GHOS | 24,11 | 21,96 | 17780 = 17780 | 25856 / 25572 | 24 |
| METR | 25,98 | 23,51 | 15721 = 15721 | 26261 / 26239 | 9 |

Cada cambio por separado (misma compilacion con su -D a 0; ms del cuadro congelado):

| Cambio | PALM | METR |
|---|---|---|
| TRAMO_LISTA | etapa 3,56 -> 2,20 (-1,36) | etapa 10,74 -> 9,30 (-1,44) |
| INDICES_FINO | coche 2,68 -> 2,08, trafico 0,98 -> 0,85 (-0,73) | coche 2,89 -> 2,23 (-0,66) |
| RECORTE_TIRAS | etapa 2,79 -> 2,20 (-0,59) | sin medir aparte |
| ETAPA_FINO | etapa 2,23 -> 2,20 (-0,03) | etapa 9,66 -> 9,30 (-0,36) |
| PVS_RACHAS | en carrera: vis 3,90 -> 3,77 (-0,13), +0,08 fps | sin medir |

MEDIDA 2: carrera de 160 s con el piloto (`-DPRUEBA_AUTO -DPRUEBA_NODO=n -DPRUEBA_SIN_TIEMPO -DDIAG_MS -DDIAG_POPPING
-DDIAG_FALTA`), en el tramo de s que recorren las dos. "Antes" = 239cc8d. cpu = dibujo + logica.

| Etapa | fps antes | fps despues | dif | tarde antes / despues | cpu ms antes / despues | grandes que faltan antes | despues |
|---|---|---|---|---|---|---|---|
| PALM | 34,08 | 35,82 | +1,74 | 311 / 207 | 27,86 / 25,45 | 2,72 (tope 0,96, tramo 1,66, entrada 0,10) | 3,73 (tope 0,99, tramo 2,64, entrada 0,11) |
| CAST | 36,52 | 37,61 | +1,09 | 207 / 143 | 25,35 / 23,49 | 1,56 (tope 0,68, tramo 0,88) | 1,61 (tope 0,74, tramo 0,87) |
| CLOU | 33,34 | 34,72 | +1,38 | 391 / 310 | 28,90 / 26,81 | 5,18 (tope 2,84, tramo 2,28, entrada 0,05) | 4,73 (tope 2,80, tramo 1,87, entrada 0,06) |
| INDU | 35,45 | 37,30 | +1,86 | 273 / 153 | 26,40 / 23,94 | 2,03 (tope 0,02, tramo 2,01) | 2,31 (tope 0,02, tramo 2,29) |
| GHOS | 35,34 | 36,59 | +1,25 | 255 / 185 | 27,10 / 24,86 | 0,96 (tope 0,33, tramo 0,63) | 1,29 (tope 0,33, tramo 0,97) |
| METR | 29,85 | 31,94 | +2,10 | 627 / 497 | 33,81 / 30,76 | 0,79 (tope 0,79) | 0,57 (tope 0,57) |
| **Media / suma** | 34,10 | 35,66 | +1,57 | 2064 / 1495 (-28 %) | 28,24 / 25,89 | | |

- La tabla es del codigo antes de los dos ultimos retoques (el bucle de reserva de `dibuja_indices` y el `-Os` de los
  menus). PALM con el codigo final: 35,91 fps, 199 tarde, y el mismo cuadro congelado (13,37 ms, 35 pixeles).
- "tope" (lo que no cabe) queda igual. "tramo" (la etapa siguiente a la vista antes de estar lista) sube en PALM, INDU
  y GHOS y baja en CLOU: son 2-3 muestras por segundo en la ventana entre leer la etapa siguiente y tenerla subida.
  La etapa siguiente queda lista en el mismo sitio (PALM: "SEG05.DAT en 6,49 s (s 5565)" antes, "6,53 s (s 5570)"
  despues). SUPUESTO: es ruido del muestreo, no una falta nueva; no se ha medido con mas muestras.
- RAM: "VOZ: VOCES.DAT, 35 muestras" en todas las tiradas. "Out of memory": 0 en carrera. En METR, al llegar a la meta
  y volver al menu con los -D de medida, sale un "Out of memory" y "MENU: no se pudieron leer las piezas de MENU.DAT"
  ANTES Y DESPUES (ya estaba; no se ha mirado con la compilacion normal). Con `-DDIAG_PERFIL` (64 KB de bss) tambien.

NO HECHO / DUDAS
- Consola real y Android: sin medir.
- HUD, sombras y trafico (su logica): no salen en el perfil por encima del 1 %; sin tocar.
- La pareja (2,5 ms por cuadro) y `nodos_visibles` (1 ms) son lo siguiente que queda.

Ficheros: m29c/opt del directorio de trabajo (res/ las consolas, cap/ las capturas antes | despues, py/ y sh/).

## Rev 2.9.9.c: medida final A/B en las 15 etapas (2026-11-27)

Todo en el Flycast de aqui (NO en la consola ni en Android). A = 467dd28 (Rev 2.9.9.b), B = f0fc01c (Rev 2.9.9.c: huecos
+ cielo + optimizacion). Los dos con los mismos datos (lgdat2: la 2.9.9.c no trae ficheros de datos nuevos). Flags:
`-DPRUEBA_AUTO -DPRUEBA_NODO=n -DPRUEBA_SIN_TIEMPO -DDIAG_MS -DDIAG_POPPING`, 140 s de piloto, paso 10; mide.py y pop.py
solo con las lineas de la zona. Tres Flycast a la vez (:90 :91 :92), como la medida final de la 2.9.9.
- Pasada 1, una ronda por etapa: A0 y B0 sin el contador, Bf = B con `-DDIAG_FALTA`.
- Pasada 2: Af = A con `-DDIAG_FALTA`, tres etapas distintas a la vez.

| Etapa | fps A0 | fps B0 | B-A | tarde A0 / B0 | salto A0 / B0 | lejano A0 / B0 | 8 px+ A0 / B0 | 32 px+ A0 / B0 | grandes que faltan Af / Bf | fps Af / Bf |
|---|---|---|---|---|---|---|---|---|---|---|
| PALM | 35,63 | 37,24 | +1,61 | 242 / 146 | 8,15 / 7,86 | 3800 / 3800 | 9,6 / 8,6 | 0,9 / 0,8 | 2,22 / 3,78 | 34,23 / 35,92 |
| LAKE | 37,86 | 38,65 | +0,79 | 130 / 73 | 6,04 / 4,34 | 3800 / 3800 | 3,2 / 2,2 | 0,2 / 0,1 | 1,14 / 1,80 | 36,97 / 37,69 |
| ALPI | 38,76 | 39,03 | +0,27 | 69 / 51 | 3,91 / 0,72 | 3800 / 3800 | 7,0 / 6,6 | 1,2 / 1,2 | 1,34 / 2,06 | 37,85 / 38,55 |
| CAST | 37,93 | 38,98 | +1,05 | 120 / 55 | 6,94 / 3,48 | 3800 / 3800 | 14,3 / 15,2 | 2,8 / 3,1 | 0,82 / 1,05 | 36,98 / 37,50 |
| FORE | 39,32 | 39,43 | +0,11 | 29 / 21 | 2,33 / 0,26 | 3800 / 3800 | 9,8 / 10,4 | 2,0 / 2,1 | 0,37 / 0,00 | 38,84 / 38,97 |
| DESE | 38,63 | 38,97 | +0,34 | 83 / 56 | 3,35 / 1,40 | 3800 / 3800 | 5,6 / 4,3 | 1,8 / 1,7 | 1,91 / 3,55 | 37,88 / 38,45 |
| CLOU | 34,50 | 36,40 | +1,90 | 321 / 188 | 8,07 / 7,45 | 3800 / 3800 | 20,0 / 21,2 | 2,1 / 2,0 | 4,57 / 5,01 | 33,37 / 34,70 |
| INDU | 37,16 | 38,63 | +1,47 | 151 / 59 | 6,85 / 5,24 | 3800 / 3800 | 10,3 / 11,1 | 3,9 / 4,3 | 0,26 / 0,02 | 35,73 / 37,20 |
| SNOW | 38,92 | 38,97 | +0,05 | 60 / 56 | 3,82 / 0,53 | 3800 / 3800 | 3,2 / 3,2 | 0,7 / 0,7 | 3,68 / 3,84 | 38,16 / 38,39 |
| GHOS | 36,71 | 37,82 | +1,11 | 167 / 93 | 8,10 / 7,12 | 3800 / 3800 | 14,3 / 15,4 | 2,4 / 2,7 | 0,80 / 0,83 | 36,01 / 36,79 |
| TULI | 37,43 | 38,45 | +1,02 | 138 / 70 | 7,84 / 5,64 | 3573 / 3693 | 21,0 / 21,1 | 2,7 / 2,7 | 0,10 / 0,10 | 36,37 / 37,29 |
| METR | 32,68 | 34,49 | +1,81 | 407 / 285 | 7,76 / 7,01 | 3680 / 3776 | 13,6 / 13,8 | 4,7 / 4,6 | 0,70 / 0,52 | 30,83 / 32,96 |
| RUIN | 38,94 | 39,29 | +0,35 | 50 / 28 | 4,83 / 0,76 | 3565 / 3722 | 9,5 / 11,8 | 1,4 / 2,1 | 4,44 / 4,41 | 38,21 / 38,90 |
| IMPE | 38,00 | 38,89 | +0,89 | 101 / 45 | 6,56 / 4,33 | 3618 / 3717 | 19,0 / 22,3 | 5,5 / 5,5 | 0,21 / 0,13 | 36,98 / 37,89 |
| CAPE | 38,89 | 39,18 | +0,29 | 54 / 35 | 4,08 / 0,95 | 3705 / 3761 | 16,6 / 11,3 | 6,4 / 3,8 | 0,01 / 0,00 | 38,18 / 38,80 |
| **Media / suma** | 37,42 | 38,29 | +0,87 | 2122 / 1261 | 5,91 / 3,81 | 3743 / 3778 | 177,0 / 178,5 | 38,7 / 37,4 | 22,6 / 27,1 | 36,44 / 37,33 |

"tarde": cuadros que empiezan tarde. "salto": nivel medio del reparto adaptativo (salto_q). "8 px+ / 32 px+": apariciones
+ desapariciones por segundo. "grandes que faltan": nodos y mallas de 24 px o mas en la vista que no se dibujan por
razones reales (tope, tramo, entrada; no rama, pvs ni pvsm), suma por muestra.

**Medido**
- fps: +0,87 de media sin el contador (37,42 -> 38,29) y +0,89 con el (36,44 -> 37,33). Suben las 15 etapas en las dos
  pasadas. Las cargadas ganan mas: CLOU +1,90, METR +1,81, PALM +1,61, INDU +1,47. La 2.9.9 habia costado -0,86.
- Cuadros tarde: 2122 -> 1261 (-41 %) sin el contador; 2871 -> 2017 (-30 %) con el. Bajan en las 15.
- La desviacion de los fps baja en las 15 sin el contador (PALM 3,98 -> 3,01; IMPE 2,54 -> 1,41).
- El nivel del reparto ("salto") baja de 5,91 a 3,81 y el lejano medio sube de 3743 a 3778 m: se recorta menos.
- Saltos de 8 px o mas: 177,0 -> 178,5 por s (+0,8 %); de 32 px: 38,7 -> 37,4. Con el contador 170,8 -> 169,6 y
  35,6 -> 35,1. Sin cambio apreciable. La razon "tramo" desaparece (2,0 / 0,9 -> 0).
- Grandes que faltan: 22,6 -> 27,1. "tope" igual (14,55 -> 14,05), "entrada" 0,38 -> 0,14, "tramo" 7,62 -> 12,91.
- RAM: "VOZ: VOCES.DAT, 35 muestras" en las 60 tiradas y 0 lineas "Out of memory". Con los -D de medida la RAM libre
  al arrancar es 262K en A y 278K en B.
- Menus + carrera con la compilacion normal de B (tools/prueba/nav3.sh, sin -D): "VOZ: VOCES.DAT, 35 muestras, 210 KB
  en el AICA", "VOZ 0x1e7" en carrera, 0 "Out of memory", EFECTOS 3/1/1/1/1/1, RAM libre al arrancar 333K (seguido en
  la cima 139K). Las 14 pantallas salen.
- YOSE -> GRAN (arbol 1, nodo 2, rama 1), una captura por segundo: en A el trozo de pared suelto y la losa oscura
  sobre el muro de 1'12"6 a 1'17"3; en B cielo limpio hasta 1'14"5, la pared crece de 1'15"5 a 1'16"4 (cortada por
  la derecha ~1 s) y a 1'17"3 esta como en A sin la losa. Despues de la entrada, igual en las dos.
- PALM -> ALPI (arbol 0, la rama que coge el piloto), una captura por segundo: sin bandas en el cielo, sin trozos
  sueltos y sin huecos, en A y en B (hojas alpi_01 a alpi_03, de 1'07" a 1'15"; las otras cuatro sin abrir).
- Compila con los mismos 4 avisos que A (los -D de medida); 1 en la compilacion normal.
- Cuelgues: ninguno. Las 60 tiradas escriben lineas MS hasta el final (80-112 por tirada).

**Regresiones (B frente a A)**
- Grandes que faltan por "tramo" (la etapa siguiente esta en la vista y aun no se dibuja): 7,62 -> 12,91. Sube en PALM
  (1,09 -> 2,67), DESE (1,75 -> 3,54), LAKE (0,84 -> 1,63), ALPI (1,31 -> 2,06), CLOU (1,24 -> 2,08) y CAST (0 -> 0,23);
  baja en FORE e INDU. Es el arreglo de los huecos (GEO_RUMBO): la etapa siguiente espera a que el coche gire hacia
  ella. En DESE las lineas con "tramo" van de s 5968 a 6611 en B y de s 6000 a 6288 en A.
- RUIN: saltos de 8 px 9,5 -> 11,8 y de 32 px 1,4 -> 2,1, igual en las dos pasadas (9,4 -> 11,5 y 1,3 -> 2,1). Toda
  la diferencia es la razon "detras" (+1,7 / 0,7): cuatro nodos (6, 8, 795, 796; 71 m de radio, a 42 m) que aparecen
  al cruzar la meta (s 5053). En A no salen en RUIN en ninguna de las dos pasadas; en METR, IMPE y TULI salen tambien
  en A. Causa sin determinar.
- IMPE: 8 px 19,0 -> 22,3 sin el contador ("lado" +2,3); con el contador 18,7 -> 19,6. INDU: 32 px +0,3 o +0,4 en
  las dos pasadas (3,9 -> 4,3 y 3,7 -> 4,0).
- Saltos "pvs": 117,9 -> 124,9 de 8 px por s (+6 %) sin el contador y 116,5 -> 120,0 (+3 %) con el.
- Ninguna etapa pierde fps ni gana cuadros tarde.

**Supuesto (no comprobado)**
- Android y la consola. Flycast cuenta instrucciones y no los fallos de cache: la ganancia sera del mismo signo y de
  otro tamano.
- Que el "detras" de RUIN sea el cambio de camara de la meta cayendo en otro cuadro, y no algo que falte antes. No
  hay captura de ese momento.
- Que el +3-6 % de "pvs" venga de que B recorta menos (lejano mas alto) y no de PVS_RACHAS.

**Avisos sobre la medida**
- En los mismos 140 s cada tirada emula un tiempo distinto (65-71 s de etapa en INDU), asi que B0 recorre entre un 9 % menos y
  un 2 % mas de etapa que A0; Bf y Af quedan a +-8 %. Las medias son de tramos algo distintos.
- PALM -> LAKE con `-DPRUEBA_RAMA_FIJA=0`: el piloto se atasca en la bifurcacion (s 5257) en A y en B. Sin capturas
  de esa union; se cambio por PALM -> ALPI.
- Las capturas de las uniones van emparejadas por la s del registro, que llega con retraso: mirar el crono del HUD.
- Fuera de la tarea: en ALPI (s ~6700, a la izquierda) hay un tronco de conifera sin copa. Sale igual en A, en B y
  en B con `-DRECORTE_TIRAS=0` (cuadro congelado 4410): no es de la 2.9.9.c.

Ficheros: m29c/final/ del directorio de trabajo (final.md, tablas.md, res/ las consolas y medida_<n>.txt, cap/ las
capturas A | B de YOSE -> GRAN, PALM -> ALPI y los menus, sh/ y py/).

### Saltos por s sumados en las etapas medidas, por razon (8 px+ / 32 px+, apariciones + desapariciones)
| razon | A0 | B0 | Af | Bf |
|---|---|---|---|---|
| detras | 3,0 / 2,2 | 4,8 / 2,6 | 3,6 / 2,1 | 2,9 / 1,7 |
| lado | 13,4 / 5,4 | 11,4 / 3,5 | 9,7 / 3,7 | 9,4 / 3,1 |
| lejos | 0,0 / 0,0 | 0,0 / 0,0 | 0,0 / 0,0 | 0,0 / 0,0 |
| lod | 0,2 / 0,0 | 0,2 / 0,0 | 0,4 / 0,0 | 0,4 / 0,0 |
| pvs | 117,9 / 26,1 | 124,9 / 27,7 | 116,5 / 25,7 | 120,0 / 26,5 |
| rama | 3,6 / 1,4 | 3,9 / 1,6 | 3,4 / 1,4 | 3,8 / 1,5 |
| tope | 36,1 / 2,5 | 33,2 / 1,8 | 35,0 / 2,0 | 33,1 / 2,2 |
| tramo | 2,0 / 0,9 | 0,0 / 0,0 | 1,9 / 0,8 | 0,0 / 0,0 |

### Grandes que faltan (24 px+, suma por muestra de las etapas medidas), por razon
| razon | Af | Bf |
|---|---|---|
| entrada (real) | 0,38 | 0,14 |
| pvs | 321,42 | 333,13 |
| pvsm | 433,60 | 441,09 |
| rama | 158,47 | 158,10 |
| tope (real) | 14,55 | 14,05 |
| tramo (real) | 7,62 | 12,91 |


## Rev 2.9.9.c (revision): repaso del diff 467dd28..adc6c06 (2026-11-27)

Repaso de todo lo de la 2.9.9.c leyendo el codigo; sin tiradas nuevas (lo medido sale de los registros de la medida
final). No ha hecho falta cambiar codigo.

Comprobado leyendo (no medido):
- TRAMO_LISTA: el bucle de tramos de dibuja_etapa no tiene salidas a medias, asi que la lista `dif` de la primera pasada
  lleva todos los tramos de la segunda; `it` cabe en 16 bits (tramos_nodo no hace la tabla con mas de 65535 mallas). Los
  2 KB de `dif` van en la pila del hilo principal (64 KB en KallistiOS).
- ETAPA_FINO: en un tramo de nodo todas las mallas son de ese nodo (tramos_nodo), asi que mirar pvs_ent una vez por
  tramo da lo mismo que por malla.
- INDICES_FINO: las mismas cuentas que ta_cabe fuera de la etapa y de la reserva (ta_prim no toca ta_vb con las
  cabeceras). El coche, el trafico y el fantasma no se dibujan con ta_en_etapa ni ta_en_bo.
- RECORTE_TIRAS: los trozos de tira van por envia_tira, el mismo camino que ya usaban las mallas enteras delante; no hay
  culling en ninguna cabecera. Si un trozo no cabe en el TA se queda fuera entero (antes, triangulo a triangulo).
- PVS_RACHAS: los mismos bits que el bucle de antes (misma mascara, mismo padre del nodo lejano).
- CIELO_TOPE: el cielo sigue bajo el tope del cuadro (ta_lim, que ya descuenta lo 2D): no puede desbordar el TA. Lo
  que puede pasar es que en la lista transparente se coma parte de la reserva del coche y el trafico.
- sig_pano_busca solo se llama con la etapa siguiente (rama `mo == &sig` de pvs_paso) y en el hilo principal, con el
  modelo ya leido. SUPUESTO para la consola: con un candidato que es todo de recorte (GRAN 60) recorre todas las mallas
  una vez (1 MB de malla_t): un cuadro largo en la union, que Flycast no cuenta.
- Las opciones de vuelta atras compilan todas juntas (TRAMO_LISTA, INDICES_FINO, RECORTE_TIRAS, ETAPA_FINO, PVS_RACHAS,
  GEO_RUMBO y FONDO_CIELO a 0, SIG_PANO_R=0, SIG_CUP_F=1e9, CIELO_TOPE=1).
- Datos: el juego no abre ningun fichero nuevo; vale con los datos de la 2.9.9 / 2.9.9.b. tools/regenera.py solo usa
  numpy y tools/pak_union.py (ya estaban en el paquete de Windows), sin rutas fijas ni programas nativos nuevos;
  cielos_viejos da (False, False) con los datos de ahora y, si no puede leer, no borra nada.
- AVISO: con datos de antes del arreglo de las UV, la herramienta (tambien la de Windows, que siempre pasa por
  regenera) borra etapas o uniones y las rehace: horas. Para mirar el cielo lee SEGS.PAK entero (235 MB) una vez.

La "regresion" de la meta de RUIN, explicada con los registros de la medida final (medido):
- Los cuatro nodos (6, 8, 795, 796) no son carteles; salen con la razon "detras" (detras de la camara) en el segundo
  del cambio de camara de la meta, junto con otros 120 ("detras +124/121/50"): es el corte de camara.
- En A no cuentan porque en ese momento el reparto esta hundido: A llega a la meta con el nivel de salto a 3,2 y tras
  el corte de camara queda "corte 1-4 m, falta 8-16 %" durante unos 3 s (la escena de la meta sin parte de la etapa). B
  llega con el nivel a 0 y dibuja todo ("corte 3444 m, falta 0,0 %").
- No es una falta nueva: en B la escena de la meta sale entera. SUPUESTO: en la consola, si se llega a la meta con el
  salto alto, pasara lo de A (ya pasaba en la 2.9.9.b).

Mod taxi: el cherry-pick de los cuatro commits sobre origin/rev-1.0 (46908c7), simulado con git merge-tree, entra sin
conflictos y compila desde git archive con el mismo aviso de siempre (or_buf).
## Trafico -10 % (tools/conv_trafico.py --ligero, DESVIACION pedida)

Menos triangulos en los coches del trafico (TRAF0..8.DAT) y en los camiones de OUTRUN2SP (TRAFK0..2.DAT) sin que se
note, para dejar margen de TA. No toca el coche del jugador ni los rivales (RCmm.DAT). El formato del fichero no cambia
y la demo no cambia: solo el conversor (`--ligero[=tanto por ciento]`, por defecto 10) y `tools/regenera.py`, que lo
pasa en los pasos `trafico` y `camiones` (el programa de Windows llama a regenera.py: genera lo mismo). Unos datos ya
hechos no se rehacen solos: hay que borrar TRAF0-8.DAT / TRAFK0-2.DAT de la carpeta de datos y volver a llamarlo.

- Por LOD (cada uno es un modelo aparte del original): 1) vaciado: fuera los triangulos que no ganan ningun pixel desde
  ninguna camara (z-buffer de tools/c/visible.c; anillos alrededor del coche de 0,45 a 8 m de alto, de cerca, a media
  distancia y con el coche entero; ruedas en 4 giros, remolque en 4 guinadas); 2) hasta el tanto por ciento, colapso
  de aristas de `conv_coche.simplifica` (el del coche ligero de la 2.9.7) solo en lo opaco de carroceria, piezas,
  ruedas y remolque, en los LOD 0 a 2, con el error acotado (LIG_DESV = 2,5 cm sobre 8 caras medias) y las UV con peso
  3 (LIG_UV). Luces, cristales, recortes y sombra no se tocan. Los LOD 3 y 4 solo se vacian.
- **El vaciado da poco**: estos modelos ya no traen bajos ni interior oculto (0 a 12 triangulos de 530-700 por coche,
  2 % como mucho). Casi todo sale del colapso, y por eso **no se llega al 10 % en todos**: con el error suelto
  (el VIS_TOPE del coche) se llegaba al 10 % pero se notaba de cerca (la calandra del modelo 35 deformada por una
  textura en espejo, la banda del paragolpes trasero mas alta); con el error acotado, lo que da cada modelo:

| fichero | triangulos LOD 0/1 | vertices LOD 0/1 | tiras LOD 0 | triangulos LOD 2 | bytes |
|---|---|---|---|---|---|
| TRAF0 | 576 -> 521 (-9,5 %) | 897 -> 832 (-7,2 %) | 126 -> 104 | 191 -> 189 | 87904 -> 85664 |
| TRAF1 | 701 -> 631 (-10,0 %) | 1089 -> 1007 (-7,5 %) | 153 -> 134 | 255 -> 249 | 83328 -> 80384 |
| TRAF2 | 578 -> 524 (-9,3 %) | 880 -> 839 (-4,7 %) | 112 -> 105 | 275 -> 259 | 77152 -> 75520 |
| TRAF3 | 532 -> 509 (-4,3 %) | 887 -> 867 (-2,3 %) | 138 -> 121 | 240 -> 227 | 82048 -> 80448 |
| TRAF4 | 672 -> 640 (-4,8 %) | 999 -> 962 (-3,7 %) | 134 -> 120 | 270 -> 257 | 94976 -> 91968 |
| TRAF5 | 543 -> 500 (-7,9 %) | 823 -> 809 (-1,7 %) | 115 -> 101 | 250 -> 234 | 73984 -> 72704 |
| TRAF6 | 534 -> 481 (-9,9 %) | 876 -> 796 (-9,1 %) | 126 -> 113 | 274 -> 254 | 74496 -> 71104 |
| TRAF7 | 576 -> 542 (-5,9 %) | 886 -> 863 (-2,6 %) | 109 -> 103 | 287 -> 270 | 61792 -> 60736 |
| TRAF8 | 594 -> 538 (-9,4 %) | 943 -> 892 (-5,4 %) | 142 -> 118 | 242 -> 226 | 90976 -> 88384 |
| TRAFK0 | 1147 -> 1084 (-5,5 %) | 1828 -> 1816 (-0,7 %) | 320 -> 295 | 450 -> 441 | 127808 -> 128160 |
| TRAFK1 | 1147 -> 1128 (-1,7 %) | 1878 -> 1899 (+1,1 %) | 328 -> 314 | 545 -> 536 | 133856 -> 135456 |
| TRAFK2 | 1332 -> 1253 (-5,9 %) | 2094 -> 2075 (-0,9 %) | 348 -> 322 | 510 -> 494 | 159904 -> 159008 |

  Los 9 del trafico: triangulos del LOD 0 5306 -> 4886 (**-7,9 %**), vertices 8280 -> 7867 (-5,0 %); LOD 2 -5,2 % y
  -3,4 %. Triangulos = distintos y con area de lo que se dibuja de dia. Lo que cuenta en el TA de la PowerVR son los
  vertices: bajan menos porque las tiras nuevas (`conv_coche.tiras` con `giro`: sigue la tira repitiendo un vertice en
  lugar de empezar otra) cambian vertices por mallas. Sin el giro salian menos vertices (-8,4 % en la carrera) pero un
  10 % mas de tiras y la demo tardaba un 3 % MAS en dibujar el trafico; con el giro, menos tiras que el original.
  En los camiones casi no hay ganancia (cajas grandes ya minimas; el TRAFK1 sube 21 vertices).
- Comprobacion offline (visor con z-buffer y texturas, 8 camaras por modelo, LOD 0 y 2, antes / despues / diferencia):
  sin agujeros ni siluetas rotas; pixeles que cambian mas de 24 niveles: 0,6 % como mucho (trasera del TRAF3 y del
  TRAF7: la textura se desplaza menos de un texel), el resto por debajo de 0,45 %.
- **Medido en Flycast** (PRUEBA_AUTO, modo OutRun desde la etapa 1, la de mas trafico; 146 medidas de 30 cuadros hasta
  s 10500, mismo recorrido): vertices del trafico por cuadro 1678 -> 1594 (**-5,0 %**; maximo 5240 -> 4968), tiempo de
  dibujo del trafico 800 -> 754 us (-5,8 %); **fps iguales** (38,15 / 38,21) y dib 20,43 / 20,34 ms: el trafico es el 6 %
  de los vertices del cuadro (27860), asi que el margen ganado es de un 0,3 % del cuadro. "trafico hasta" del TA al
  arrancar: 1101 -> 1019 vertices. Sin "Out of memory". OUTRUN2SP (arbol 1) 200 s con los TRAFK nuevos: carga sin
  errores y sin cuelgues (no se vio ningun camion en las capturas miradas).
- Solo probado en Flycast, no en la consola.
- Entrega (Rev 2.9.9.c): `--ligero` solo en el paso `trafico` (TRAF0..8). Los camiones (TRAFK0..2) se quedan como
  estaban: casi no ganaban y no se llego a ver ninguno en el juego con los datos nuevos.

## La pareja del final de recreativa otra vez (2026-11-27)

Los personajes de los finales (FINCHRgg.DAT, FINGgg.DAT, FINHgg.DAT) iban en los datos de las revisiones desde el
2026-11-23 como descriptores de ACTORES.PAK (repiten modelo: 46 MB sueltos, 6,4 MB empaquetados), pero comun/gd.c solo
recomponia BKnn y SEGnn: el final de recreativa de OUTRUN2SP salia sin la pareja ("FINAL: sin FINCHR11.DAT (-2)").
`es_union` acepta ahora tambien FINC* / FING* / FINH* (un FIN suelto solo cuesta leer un sector). Verificado en E con
tools/prueba/final.sh (preset 0, meta D, final 18): "FINAL: FINCHR2C.DAT, 4219 mallas, 35 texturas, 177 piezas", la
pareja en las capturas, modos 25 / 27 / 28 y vuelta al frontal. Datos sin cambios. PENDIENTE: que tools/regenera.py
produzca ACTORES.PAK (hoy los datos regenerados desde la ISO, y los del programa de Windows, llevan los FIN sueltos, que
tambien funcionan).
## Modos "sp" (Rev 2.9.9.c): 15 recorridos de OUTRUN2SP, voces HAM.CVT del INFARTO y coche de la grabacion del fantasma (2026-11-27)

### 1. Los 15 recorridos continuos de OUTRUN2SP (presets 2 / 3)

- Original: [0x54c108] = 2 (0x177310 / 0x177440 al arrancar; nadie lo cambia) -> 8449E0 = 2 y la lista de 5 filas del
  frontal ("Modo OutRun / Modo contrarreloj 15 recorridos continuos"; 4C3DC0 filas 3 / 4: +1F = 1 / 0, +27 = 2 / 3 con
  el lado 0 / 1 del gatillo R) -> START 0x1760c0(5 / 6) con "%s_course": csc_data_cvt_ren / csc_data_2_ren = arboles
  291 / 290 de CURSOTA.DAT (los de los cursos seguidos del OUTRUN de la consola, revision 44, ya en el disco y
  comprobados con orden.sh). La Dreamcast tomaba 0 (3 filas): HECHO como el original (comun/fsp.c paso4;
  `-DFSP_FILAS3` vuelve a las 3 filas).
- Resultado (modo 25): con los presets 2 / 3 (1b4a90 en 0x1ba7b4) el suceso 0x188 lleva la funcion 0x50 (PS2 0x209ad8 /
  0x209cc8 / 0x209d08 = PC 4AED50 / 4AEEC0 / 4AEEE0), no la 0x15: el mapa de 15 (340001 / 34000C con el preset 2, si no
  340000 / 34000B; capas 1 y 5), 34001D (capa 6) y el coche del modelo ([0x410048 + 48B140 x 4]) en la capa 4, modo 1,
  de su cuadro 0 al de la posicion: meta (0x1cff70(0xE)) 0x1FE; si no, 0x1E por nivel acabado (0x3C en los niveles 5 y
  10, 0x209a40) y en el siguiente razon x 23 (46) en la etapa, 23 + razon x 7 (46 + razon x 14) en la union y razon x 30
  en la etapa 14 (0x209a68 / 0x209aa0 / 0x41f0). Acaba cuando el coche llega a su ultimo cuadro. demo/fin.c ta_init /
  ta_control; tablas en el bloque 'TA50' detras de RSP1 en RUTASP.DAT (tools/conv_sp.py tablas_ruta: cada constante
  comprobada en el codigo; con el RUTASP.DAT de antes sale el mapa de la 0x15).
- Entrada de nombre (modo 26) con los presets 2 / 3 (0x200400..0x2004d4): la tabla 3D001D / 3D001E (cuadros 0..0x14)
  en lugar de t + 0, sin la linea de la meta ([0x360] = 6) (demo/nombre.c tablero). El fondo, 44DC50(450560(4)): el
  descriptor de la etapa del nivel 4 de su arbol (desc4p de demo/main.c: RANKSP.DAT solo trae los de los arboles 0 /
  1). Los rankings de esos presets ya estaban en comun/rksp.c.
- Ya iban por preset sin cambios: CARGASP2 / 3, tiempos TIM1 2 / 3, mapa de 15 del HUD, banderero, choques, la meta E
  (clave 14) y el final de recreativa.
- Espacio del GD y presupuestos: los 15 recorridos usan las SEGnn / COLnn de los arboles 0 / 1 y las uniones
  BK470..497 (UNIONESR.PAK, 7,1 MiB) que ya estaban en el disco; ninguna es SEG46 ni de las uniones con el cielo de
  FLOR_R (las que no caben en los huecos): caben todos. Las 28 uniones frente a las 30 bifurcaciones de los arboles 0 / 1
  (tools/vram.py lee_dat y la cabecera ORD7 de los LGDAT2, script scratchpad modos/sp/scr/uni.py): texturas media 179 /
  max 221 KB frente a 172 / 209 KB; vertices del modelo media 41306 / max 42295 frente a 41141 / 41591 (+0,4 % / +1,7 %:
  los mismos LRBK con otros carteles). El TA no sube (el tope del TA es global, demo/main.c ta_*, y la etapa es la misma
  SEGnn); la VRAM, como mucho +12 KB en una union, dentro de la zona del curso. Medido en Flycast (arbol 290): huecos
  8142K / 1584K al arrancar, RAM libre en carrera 331..463K, VRAM libre de la zona 558..1003K, SEG03 en 7,09 s, BK470 /
  471 en 1,7..1,9 s; modo 25 con 3154..4572K de RAM fija.
- Verificado en E (Flycast, display 97; tools/prueba/resultados.sh con SP_FILA=n (pulsaciones cortas a la derecha) y
  SP_CONF=1 para la configuracion de la Contrarreloj):
  - "Modo contrarreloj 15 recorridos", lado 0: `FSP: carrera (... variante 0, preset 2 ...)`, `CURSO: arbol 291, 15
    etapas, salida BEAC`, fin del tiempo en SANF -> `FIN: modo 25, funcion 0x50 (15 recorridos, preset 2): coche 340009
    hasta el cuadro 53` (mapa de 15 con el coche en la segunda etapa), `NOMBRE: modo 26, variante 0 preset 2` (tabla de
    la Contrarreloj de 15), GAME OVER y vuelta al frontal.
  - "Modo OutRun 15 recorridos", lado 1 (gatillo R): `variante 1 ... preset 3`, `CURSO: arbol 290, 15 etapas, salida
    PALM`, funcion 0x50 hasta el cuadro 52 (30 + 0,969 x 23), la tabla de OutRun de 15 (PUNTUACION 75680), GAME OVER y
    frontal. 0 "Out of memory", 0 "pvr_mem: ERROR".
  - Capturas: scratchpad modos/sp/cap/t15/ (re_8c la lista de 5 filas, re_2 el mapa de 15, re_n* el ranking) y
    modos/sp/cap/o15/ (re_8c, re_3, re_n1).
- Recorridos enteros hasta la meta desde el frontal (Flycast, :97, GDI con -DPRUEBA_PILOTO_MENU -DPRUEBA_SIN_TIEMPO,
  scratchpad modos/sp/scr/s15.sh = final.sh con espera larga; capturas modos/sp/cap/s15o/ y s15t/):
  - OutRun de 15 (lado R, preset 3, arbol 290): las 14 uniones BK470..483 leidas (1,7..2,4 s), GOAL en CAPE, meta E
    (escena 5, musica 63), final de recreativa (modo 24, FINE4), modo 25 con la funcion 0x50 hasta el cuadro 510 (0x1FE,
    la meta), GAME OVER y vuelta al frontal. RAM libre minima en carrera 235K (BKD.DAT detras de BK481).
  - Contrarreloj de 15 (lado 0, preset 2, arbol 291): BK484..497 (BK497, la de la meta, 14,4 s en Flycast con
    META.DAT), GOAL en NEWY, meta E (escena 34), modo 25 hasta el cuadro 510, GAME OVER y frontal (sin modo 24: la
    Contrarreloj no tiene final en la DC, final_columna). RAM libre minima 249K (META.DAT con la union de la meta).
  - Sin cuelgues, sin "Out of memory", "pvr_mem: ERROR" ni "No se pudo". Aviso visto, que no es de estos cambios:
    "FINAL: sin FINCHR11.DAT (-2)": en los datos de prueba (LGDAT2) los FINCHRgg.DAT son descriptores PAKD de un
    ACTORES.PAK que rev-1.1 no recompone (comun/gd.c es_union solo BK / SEG): el final sale sin la pareja.
- Los records de la Contrarreloj de 15 tras la meta (26 en
  la variante 0 con record, [0x5a2030]) siguen como antes (SIN PORTAR en todas las Contrarrelojes de OUTRUN2SP).

### 2. Voces del INFARTO: HAM.CVT (preset 0) y HAM_SR

- Original (SLES-53998): el cargador de la etapa (0x173a54..0x173aa4, variante 2) pide con 11ed40 un banco por etapa:
  con el preset 1 ([0x44350c], 0x173a6c) 0x1b3be8(clave), si no 0x1b3c70(clave); id = base[c] + clave con c =
  0x1af710(clave) = la clase de la mision de su nivel (0x1aeff0: < 15 -> 0; 15..40 por la tabla de saltos 0x3f06e0 -> 1,
  2 o 3) y base 0x8443 / 0x844f / 0x8458 / 0x845d (HAM\1A.., HAM_SR\3A1.., HAM_SR\4A2.., HAM_SR\5A3..; el byte bajo es la
  entrada de la tabla de bancos 0x352840) o 0x841a / 0x8426 / 0x842f / 0x8434 (HAM.CVT y HAM_SR.CVT). La Dreamcast
  llevaba solo HAM\ (la clase 0 del preset 1), tambien con el preset 0, y sin HAM_SR (misiones >= 15, niveles 2..4).
- Conversion (tools/conv_infarto.py vocesh): VOCESH.DAT (preset 1: HAM y HAM_SR, 26 bancos distintos) y VOCESHC.DAT
  (nuevo, preset 0: HAM.CVT y HAM_SR.CVT, 35 bancos distintos). Cada muestra distinta (por id y datos) una sola vez con
  la mascara de los bancos que la llevan (bloque 'VHG1' detras de los datos: mascaras u64, grupo por clase y clave,
  clase de cada mision 0..47); las de HC.PAK / PERH.pak como siempre. HAM.CVT\2A, \3A y HAM_SR\5D2 llevan las tablas
  al principio del sector siguiente (banco_tablas_ham; conv_sonido.banco_tablas no las veia y salian vacios). AICA:
  VOCESH.DAT 740 -> 511 KB (75 muestras), VOCESHC.DAT 653 KB (90). Tres muestras de HAM_SR.CVT pasan de 65535 (el
  largo maximo del AICA) y no van: suena la de HC / PERH si la hay (0xd8 de 5D3; 0x16e y 0xdc de 5E3).
- Demo: comun/sfx.c lee VHG1 (sin el, con los ficheros de antes, id + 0x200 (grupo + 1) como hasta ahora) y
  sfx_voz_grupo_ham(mision, clave); comun/infarto.c voz_banco (al empezar y en cada stage clear, con la mision de
  mision()) e infarto_voces() (VOCESHC.DAT con el preset 0 si esta en el disco); demo/main.c pone el preset antes.
- DESVIACION que queda: el PS2 elige el banco al cargar la etapa (la mision de 1aeff0 queda fijada entonces); la DC en
  el stage clear, con la mision que ya elegia alli.
- Verificado en E: OUTRUN2SP lado 0, Infarto (sp.sh SP_FILA=1 SP_LADO=0): `VOZ: VOCESHC.DAT, 90 muestras, 653 KB en el
  AICA (bancos HAM por clase y clave)`, `INFARTO: voces del banco 0 (clave 0, mision 0, preset 0)` y las voces de la
  copiloto (0xe7, 0xe8, 0xc8, 0xd7...). INFARTO de la consola (nav3.sh MODO_JUEGO=2, preset 1): `VOZ: VOCESH.DAT, 75
  muestras, 511 KB` y el banco 0. Con el piloto (-DPRUEBA_AUTO -DPRUEBA_VARIANTE=2
  -DPRUEBA_JUEGO=1 -DPRUEBA_SALTO=600 -DPRUEBA_RAMA=1, preset 0, arbol 1): el banco cambia en cada stage clear como en
  la tabla de VHG1 (clave 0 -> 0, 2 -> 2, 5 -> 5, 9 -> 8, 14 -> 10). En esa carrera todas las misiones fueron < 15
  (clase 0): los HAM_SR (clases 1..3) no se han visto elegir en Flycast.
- PENDIENTE: oirlo (Flycast va sin audio en las pruebas: se comprueba por la consola serie que la voz pedida existe).

### 3. El fantasma de la CONTRARRELOJ con el coche de la grabacion

- Original: 467E30 / 4AE0F0 dibujan el fantasma con el modelo grabado. La Dreamcast lo dibujaba siempre con la forma
  del coche del jugador. Ahora (demo/main.c gh_*, comun/gho.c):
  - ORGH guarda en `modelo` 0x80000000 | color << 8 | ficha del coche que grabo (mismo formato ORGH 'ver' 1; las
    grabaciones de antes llevan 0 y se ven con el coche del jugador, como antes; una version vieja de la demo lee las
    nuevas igual).
  - Si la ficha grabada no es la del jugador, gho_lee saca del GD el bloque GHO1 (la forma simplificada sin textura de
    la Rev 2.9.2.a) de su COCHEnnL.DAT (el ligero; si no esta, COCHEnn.DAT) sin leer el fichero: la cabecera ORD7 da
    donde empieza CEX2 y se recorren CEX2 / SVL1 / ANI1 sector a sector (un sector de cache) hasta GHO1. Va en gh_buf
    (el bufer del fichero de la VMU, libre en carrera; gh_meta vuelve al coche del jugador antes de escribir alli):
    0 KB de monton y 0 KB de VRAM (sin texturas). COCHE00L: 182 triangulos (3,6 KB).
  - Translucido como antes (gris azulado, alfa 0,44). Pedido del usuario (menos poligonos, <= 0,3 fps): solo las caras
    que miran al ojo (GHO1 son envolventes convexas con las caras hacia fuera; el ojo pasado al marco del coche): de
    182 triangulos / 546 vertices por cuadro a ~68 / ~205 (medias de 300 cuadros: 64..72 triangulos, 194..218
    vertices). `-DGH_DOS_CARAS` las dos caras como antes.
  - Medido en Flycast (PALM_T, codigo 0, piloto con -DPRUEBA_FICHA=5 contra la grabacion de la ficha 0,
    -DPRUEBA_GH_ADELANTO=15 -DDIAG_MS, 77..78 medidas de 30 cuadros, misma carrera): sin dibujar el fantasma
    (-DPRUEBA_GH_OCULTO) 39,21 fps / dib 19,83 ms / 27059 vtx; con el fantasma de una cara 39,16 fps / 20,03 ms /
    27402 vtx (**-0,05 fps**); con las dos caras 38,98 fps / 20,22 ms / 27664 vtx (-0,23 fps). Flycast, no la consola.
- Verificado en E: primera carrera con la ficha 0 (`FANTASMA: guardado ORGH0100 (407 muestras, 54102 ms)`), segunda
  arrancada con la misma VMU y la ficha 5: `FANTASMA: coche de la grabacion ficha 0 (color 1): GHO1 de COCHE00L.DAT,
  182 triangulos`, la forma gris del F50 delante del F430 del jugador (scratchpad modos/sp/cap/ghc/gh_b*.png y
  modos/sp/cap/ghc_zoom.png; con las dos caras gh/ y gh_zoom.png). Sin "Out of memory".
- DESVIACION que queda: el fantasma usa el cuerpo de la fisica del jugador (ani_m[0], unos cm), como antes; sin el
  color de la grabacion (guardado, pero el fantasma sigue sin pintura por la peticion de la 2.9.2.a).

### Medidas generales

- RAM: binario + BSS +5,3 KB (text 769120 -> 772892, bss 2174628 -> 2176100: 1,3 KB de las mascaras de las voces);
  al arrancar "en uso 13021K, libre 342K" (nav3.sh), "VOZ: VOCES.DAT, 35 muestras"; ninguna prueba con "Out of memory".
- Datos nuevos / cambiados (scratchpad modos/sp/dat): RUTASP.DAT (bloque TA50, +192 bytes), VOCESH.DAT (526 KB),
  VOCESHC.DAT (672 KB, nuevo; regenera.py paso `infarto`).
## Tarjeta SPLASH WAVE REMASTER en el selector de musica (2026-11-27; DESVIACION pedida por el usuario)

Una tarjeta mas, la ultima de la lista 2 (las remezclas): SPLASH WAVE REMASTER de SUPNEO, tema 28, siempre
disponible. La cancion y la imagen son del usuario y no van al repositorio (como los datos del juego): se anaden con
`tools/menu_musica.py` solo si estan.
- **Por que pantalla pasa la musica en cada modo**: COAST 2 COAST (misiones -> coche -> cambio -> musica), OUTRUN (tablero
  0x24 con los juegos OutRun 2 y OutRun 2 SP -> coche -> cambio -> musica), INFARTO y CONTRARRELOJ pasan todos por la
  misma pantalla de musica de la consola (P_MUSICA de comun/menu.c, 4C9C20 del PC): la tarjeta sale en los cuatro
  (INFARTO y CONTRARRELOJ comparten el selector, no se puede tener en unos si y en otros no sin inventar). El OUTRUN2SP
  del menu principal no pasa por ella: va al frontal de recreativa (comun/fsp.c, "Elige la musica", funcion 9, lista
  0x2E0028 de 8 filas por lado con los registros 5C8D00 {tema, avance, 4 filas}; ver Frontal de recreativa de
  OUTRUN2SP). Alli va como una fila mas: HECHO, ver "Fila SPLASH WAVE REMASTER en Elige la musica de OUTRUN2SP" abajo
  (lo que sigue era la propuesta). Como hacerlo sin inventar pantallas: una fila mas en esa lista
  (un 17.o registro de 5C8D00 en el bloque FSP1 de SPFE.DAT con el tema 28 y el avance 71, la lista de 9 filas en
  lista_4bee90 / lista_4c0eb0 de ese lado y las escenas de la fila nueva compuestas como aqui sobre las del banco 0x2E
  de SPFE.DAT, con un parche como menu_musica.py); el banco 0x2E se evalua en vivo (comun/sprani.c), asi que hay que
  anadir cuadros a sus escenas, no piezas horneadas: trabajo aparte. Mientras, quien quiera el tema en los cursos de
  OUTRUN2SP lo tiene en OUTRUN de la consola con el juego "OutRun 2 SP" del tablero (verificado abajo).
- **Datos** (`tools/menu_musica.py <MENU.DAT> <cancion> <imagen> <salida/>`): MENU.DAT no se puede regenerar (lleva el
  arte de OutRunners), asi que se parchea el que hay: se lee entero (HUD1 + MNU1 + MNU2), se anade una textura de
  512 x 128 en 16 bits (128 KB, sin VQ) al final de las de MUSICA2 (las de las pantallas de detras se corren; los
  indices de sprites, grupos y usos no cambian), seis sprites al final, las piezas de los cuadros 499 y 500 del carrusel
  MUSICA_L2 y 1320..1429 del titulo MUSICA_T2, y los tramos TEMA12 (500, quieto; titulo 1320..1429 en bucle) y
  MUEVE1112 (499 -> 500: un movimiento de 0 cuadros no avisa al menu de que ha acabado y no sonaba el avance). Marca
  `SWR1` en el primer campo libre de MUSICA2 en MNU1: el menu solo ofrece la tarjeta con ella. Repetible: si ya hay
  una tarjeta de mas (la suya o la ALL I WANT, cuyo arte seguia en el MENU.DAT de los datos aunque el codigo ya no la
  usaba: 6 texturas, cuadro 500) la quita antes; dos pasadas dan el mismo fichero byte a byte.
- **La tarjeta**, en los sitios de la del tema 11 (NIGHT FLIGHT INSTRUMENTAL): rotulo "SPLASH WAVE" (234x31) y tira
  inferior "SPLASH WAVE - REMASTER BY SUPNEO -" con la fuente 14 del propio MENU.DAT (grupo FUENTE + metricas de MNU2,
  engordada y espaciada para parecerse al rotulo del original; sin fuentes del sistema), colores medidos en las piezas
  del tema 11; etiqueta "REMASTER BY SUPNEO" en una pastilla azul como las de genero; el icono de la lista 2 (el mismo de
  sus 12 temas); visor de la radio en azul de puntos alternando "SPLASH WAVE" / "(REMASTER BY SUPNEO)" cada 55 cuadros
  como los del original; portada = la caja de CD de la portada del tema 11 con la imagen del usuario (recortada al
  centro y escalada, sin retocar) en el sitio del dibujo; 13 puntos de lista (uno mas, a 16 px) con el encendido en el
  13. El rotulo de la lista (MUSICA DE FONDO: REMIX 1) se repite del cuadro del tema 11.
- **Audio**: el mp3 se decodifica con el ffmpeg de imageio-ffmpeg (tools/nativo.ffmpeg) a PCM a 44 100 Hz y
  `tools/c/musica.c` (entrada nueva "pcm") lo pasa a ADPCM de Yamaha estereo como los MUSn.ADP. 44,1 kHz y no 48: la
  demo reproduce todas las pistas a 44 100 Hz (musica.c, el pitch 0xEB3 del original); a 48 kHz sonaria un 8 % lenta.
  MUS70.ADP = el tema entero (2:59, 7,6 MB); MUS71.ADP = el avance (26 s desde el segundo 30, fundidos de 1,5 s; los del
  original son trozos de ~25 s). Pistas 70 / 71 porque la 28 (final de recreativa, fin.c) y la 61 (0x21 + 28,
  resultados de OUTRUN2SP) ya son del original. comun/musica.h: MUSICA_TEMA_EXTRA, musica_pista_tema /
  musica_avance_tema / musica_tema_pista.
- **Codigo** (comun/menu.c, diff pequeno): TEMAS_LISTA[2] = 13 con la marca; pon_musica pide avance y tema con las
  funciones de musica.h; tema_libre: el 28 siempre; menu_pista devuelve la pista 70 y menu_entra la vuelve a tema 28
  (demo/main.c sin tocar: NPISTAS sigue en 28, el tema de mas no depende de el). **ALEATORIO (Y)**: igual que el
  original, `(rand & 0x1F) * 27 >> 5` = 0..26: la tarjeta nueva no sale nunca (tampoco el 27 en el original); se
  decide no tocarlo para no cambiar lo que hace el boton del original. Programa: +236 bytes de .text.
- **Paso opcional**: tools/regenera.py, despues de lo demas, si en `extras/` (por defecto al lado de datos/; `--extras
  DIR`) estan `splash_wave_remaster.mp3` y `supneo.jpg` (menu_musica.EXTRAS) parchea el MENU.DAT de datos/ y deja
  MUS70.ADP / MUS71.ADP; no lo repite si ya esta y no son mas viejos que los ficheros de extras/. El programa de Windows
  (windows/outrun2dc.py) usa `<carpeta>/OutRun2006_DC/extras/` (LEEME.txt); construye.sh ya lleva menu_musica.py y
  musica.exe. Sin esos ficheros, nada cambia.
- **VRAM**: MUSICA2 800 KB con la tarjeta (808 en el MENU.DAT de antes con el arte de ALL I WANT sin usar; 274 + 800
  con la comun MUSICA). RAM: +2 piezas en las tablas de MENU.DAT (las de ALL I WANT se quitan), nada mas.
- **Verificado en Flycast (E)**, recorrido real del menu con una licencia nueva (herramienta de prueba en el
  scratchpad: modos/musica/herr/musica.sh; audio de Flycast con SDL_AUDIODRIVER=disk): la tarjeta se ve (capturas
  mu_tema12), sin candado; el avance MUS71.ADP suena al llegar a ella (correlacion con MUS71 0,98 frente a 0,06 /
  0,13 con MUS60 / MUS27); al elegirla suena MUS70.ADP en la carrera: OUTRUN juego OutRun 2 (0,67 / 0,61 frente a
  0,06 de MUS27, con motor y voces), OUTRUN juego OutRun 2 SP (arbol 1, preset 0: 0,67 / 0,58 frente a 0,05) y COAST 2
  COAST (con -DPRUEBA_MEZCLA=6, solo la musica: 0,98 frente a 0,08). La grabacion de Flycast en carrera lleva silencios
  donde la emulacion va mas lenta que el audio: se quitan antes de correlacionar. "VOZ: VOCES.DAT, 35 muestras", RAM al
  arrancar 345 KB libres (la base no se midio aparte; el programa crece 236 bytes), sin "Out of memory"; nav3.sh
  (COAST 2 COAST) igual. INFARTO y CONTRARRELOJ usan la misma pantalla (no
  probados aparte; despues si, ver abajo). No probado en R.

### Fila SPLASH WAVE REMASTER en "Elige la musica" de OUTRUN2SP (2026-11-27; DESVIACION pedida por el usuario)

El frontal de recreativa (comun/fsp.c, funcion 9, PC 4C4880) lleva su propia lista: la escena 2E0028 del banco 0x2E
(SPFE.DAT, evaluada en vivo por comun/sprani.c), 8 filas por lado (7 temas y ALEATORIO; lado 0 OutRun 2, lado 1 las de
1986 / 1989 con el gatillo R), 90 cuadros por fila (t = (aa4 + aa0 + x2) x 2 = (10 + 5 + 30) x 2), con los registros
5C8D00 {tema, avance, 4 filas}. Ahora tiene 9 filas por lado: la novena, SPLASH WAVE REMASTER (MUS70 / avance MUS71),
al final, despues de ALEATORIO, en los dos lados.
- **Datos**: `tools/fsp_musica.py <SPFE.DAT> <MENU.DAT> <imagen> <salida/>` parchea el SPFE.DAT que hay (SPFE.DAT si se
  regenera desde la ISO con conv_sp.py; el paso opcional de extras de regenera.py lo parchea despues del MENU.DAT). Solo
  anade al final de la animacion y cambia unos pocos campos (apuntados en el bloque para deshacerlos): repetible, dos
  pasadas dan el mismo fichero byte a byte. Las capas de las 8 filas del original no se tocan.
  - 2E0028: la raiz se alarga (lado 0: 30..870 en vez de 30..780; cambio de lado, lado 1 y vuelta corridos 90 / 180
    cuadros; duracion 1780). Por cada capa de la raiz que dibuja un carrusel hay otra con la misma animacion y una
    componente con la carta 9 (copias de las capas de la carta 8, ALEATORIO, a +61 px y +90 cuadros: carta pequena,
    sombra, carta grande, marco y destello; la de "detras", la lista del otro lado, igual). DESVIACION de colocacion:
    todo el carrusel va 24 px a la izquierda para que quepa la novena (las 8 cartas del original, mismas y en el mismo
    orden, 24 px mas a la izquierda).
  - 2E0029 / 2E002A (la eleccion, 30 cuadros por fila, salida 240..255): la carta 9 quieta en el carrusel y su eleccion
    en 256..285 (copias de las capas de la raiz con su animacion de 210..239, el rotulo nuevo y el ecualizador de la
    fila 0).
  - 2E0027 (el visor): 1600..1689 con "SPLASH WAVE REMASTER" hecho con las letras de los rotulos del propio visor
    (SPLASH WAVE + R, E, M, A, S, T de PASSING BREEZE / MAGICAL SOUND SHOWER / ALEATORIO) y el ecualizador de la fila 0.
  - 2E0013 (el icono de la musica en el panel de la derecha, un cuadro por registro): el cuadro 16.
  - Arte: las cartas e icono de SPLASH WAVE del original con la imagen del usuario en el disco y "REMASTER" (fuente 14 de
    MENU.DAT) en la carta; una textura de 512 x 128 en 16 bits (128 KB de VRAM mientras se ve la lista).
  - FSP1: detras, el bloque `MUSX`: filas 9, desfase del lado 1 830 (740 + 90), cuadros del cambio de lado en 2E0028
    (870..889 y 1700..1719; en 2E0027 siguen los de antes), un registro por lado {70, 71, (2E0027 1600..1614),
    (1615..1674), (2E0029 / 2E002A 256..285), (1675..1689)} y lo necesario para deshacer el parche.
  - Tablas en la RAM (el hueco de 960 KB del frontal): 10 204 bytes de FSP1 + MUSX y ~15 KB mas de animacion
    (874 KB en total, caben).
- **Codigo** (comun/fsp.c): sin MUSX todo igual que antes. Con MUSX: lista_4bee90 con 9 filas y el desfase 830; la
  posicion p (fila + lado x 9) va al registro del original (lado x 8 + fila) o al de MUSX (mus_idx / mus_campo /
  mus_fila); ALEATORIO sigue siendo la fila 8 de cada lado y sortea entre los 7 temas del original (como el original:
  la nueva no sale); la voz del tema (idioma 0, no suena en la PAL espanola) no se pide para la nueva; el icono del
  panel, cuadro 16; el cambio de lado de 2E0028 con los cuadros de MUSX.
- **Verificado en Flycast (E)**, OUTRUN2SP desde el menu principal (scratchpad modos/musica/herr/fsp.sh): lado 0 y lado
  1 (R), las 9 filas (capturas: las 8 del original con sus rotulos y avances de siempre, MUS33..39 y MUS42..48, ALEATORIO
  MUS30) y la novena con su visor y avance MUS71 (correlacion 0,99 frente a 0,08 con MUS33); A en ella: la eleccion, el
  icono en el panel y la carrera con MUS70 ("FSP: carrera ... musica 70"; correlacion 0,67 frente a 0,08 con MUS2, con
  motor). ALEATORIO sigue sorteando entre los 7 (salio MUS2). "9 filas de musica" en el registro; sin "Out of memory".
- **INFARTO y CONTRARRELOJ** (consola, la misma pantalla de musica que OUTRUN y C2C): la tarjeta se elige y suena
  (avance MUS71 y carrera con MUS70; correlacion 0,65 y 0,72 frente a 0,05 / 0,06 de MUS27). No probado en R.
## COAST 2 COAST: misiones de tipo 1, 2, 3 y 6, vaivenes de los rivales (agente c2c, 2026-11-27)

Se juegan las 18 misiones que faltaban (tipos de +0x20 de Races.bin: 1 eliminacion por tiempo, 3 misiones; 2 derrape,
7; 3 rebufo, 5; 6 pillar al rival, 3) con sus reglas, su HUD, su nota y su resultado. Las 94 misiones abren desde el
mapa del menu (antes A en ellas daba el sonido de bloqueado); solo siguen cerradas las de `arbol_grande` (no caben).

**Gestor (comun/mision.c, nuevo).** 496A30 de SLES-53998 (PS2 0x1f8d0c, el mission_manager_control_496a30 del PC,
comprobado en el ELF de PS2 tipo a tipo: 0x1f9050 tipo 3, 0x1f90d0 tipo 2, 0x1f9144 tipo 6, 0x1f91b0 tipo 1, 0x1f9428 la
escala) y lo que llama: 477B60 (0x1f13c8), 477430 (0x1f0da8), 495C20 (0x1f85d0), 495E90 (0x1f8930), 4961F0 y 495A20. Se
movieron aqui los rotulos del principio (antes en demo/main.c, c2c_paso).
- Tipo 1: el reloj [836388] = segundos (+0x28) x 60; baja 1 por paso; en los ultimos 5 s el pitido 0x518D cada segundo;
  a los 0x168 cuadros el rotulo 2B0064 ("¡KO! en 5, 4.."); con el jugador ultimo entre 0x3D y 0x168 la voz 0x1F2; en 0:
  si el ultimo es el jugador, mision fallada (450230(2): el fin del tiempo, modo 20) con su nota guardada; si no, sale el
  ultimo coche de carreras (477430: fuera de la clasificacion, su coche cerrado, los puestos de detras suben uno), el
  rotulo del eliminado (0x36f418 por su +0x76: 2B0056..) y la voz 0x1F0; con 2 en carrera, ganada (meta sin camara).
- Tipo 2: con el derrape (+D36 > 0) a mas de 10 km/h, en el asfalto y sin toque reciente (+DA4), 10 puntos por paso, 20
  por encima del 95 % del tope del coche ((+134C - 2,3) x -131,39 + 318). Tipo 3: con rebufo (+E68 > 0,18 y |+162| <
  0x5DC) lo mismo (20 por encima del tope); un choque con otro coche (+4 0x10000 y 0x4000000) quita 2000 y pone +DA4 =
  0x1E. El rival suma 10 / 20 (bit 3 de +0x70) en 477B60 con su bandera (0x200 derrape, 0x100 rebufo).
- Tipo 6: con el jugador ultimo baja su reloj (segundos x 60); en 0 fallada. El rival en el ultimo puesto gasta su +0x98
  (el mismo tiempo, puesto en el montaje 47CF40) y en 0 esta pillado (rotulo 0x36f430: 2B0050..; con 2 en carrera,
  ganada y su rotulo en vez de GOAL en la meta, 498670).
- Tipos 1 y 6: car+0xDBC = [83638C] / 600 x 0,17 + 1 (sube con el jugador detras: ActionForce2 escala el avance; demo:
  `jug_escala` en coche_paso).
- Nota (495C20): 0 y 1 por el puesto; 2 y 3: por encima de los puntos del rival 4, 5 o 6 (umbrales +0x38 / +0x3C), si
  no por la razon con 0,3 / 0,5 / 0,7 / 1 (PS2 0x36f450); 6: (reloj + 5) / 6 contra los umbrales, 0 si acaba ultimo. Se
  guarda en la licencia (+0x1D7 + 5 clave + sub, menu_nota_mision) al salir de la meta (49CFD0) y en los finales de la
  mision (495A20): la VMU la escribe como las de carrera.
- Marcas (495E90): tipos 2 / 3 sobre el coche (495D90: el punto de la tabla PS2 0x40c370 por modelo, x - 0,7; con la
  vista interior en (234, 108)) la nota de lo ganado desde la ultima (2B000F + (puntos x 0,014), cuadro 0x1F), a los 500
  la grande 2B0015 (cuadros 0x3D..0x69, velocidad 2) que vuela a (210, -160) en 0,25 s con el efecto 0x52 y con el
  choque (-2000) 2B000E; tipo 6: 2B000E cada 70 cuadros de reloj perdido.
- Fin: `mision_fin()` 1 pasa a la meta (estado META sin la escena de la meta y con el coche quieto, `c2c_susp`, como el
  suspend_race de 49C9C0 con 8363A4); 2 acaba el tiempo (estado AGOTADO).

**HUD (NAVI_PUB 0x1bd6f4).** Panel 2C0029 en los tipos 2 / 3 / 6 (0x1bd748); puntos 4BD230 (0x1bf108: "%07d" del jugador
en (0x1E1, 0x49), estilo 1 o 0xD fuera del asfalto / con +DA4; el rival en (0x1E1, 0x67) y su imagen 2C024F con el
desplazamiento de 0x369838 / 0x36983c) y 4BD340 (0x1bf260: "%04d" de (reloj + 5) / 6, 2C00F1 delante, el estilo del que
va primero); cifras de 0x1c3c68 (estilos de 0x404818: 1 = escena 2C00B9, 0xD = 2C00C0, cuadro = cifra, 0x10 px).
Datos: tools/conv_hud.py --c2c escribe C2CHUD.DAT (lo de todos, 187 KB de VQ) y por tipo C2CH1.DAT (eliminados y
2B0064, 110 KB), C2CH23.DAT (notas, solo sus cuadros dibujados, 32 KB) y C2CH6.DAT (pillados, 108 KB): solo se abre el
del tipo de la mision. C2C_VRAM_RESERVA 196 KB + el del tipo. Las texturas se suben de una en una (antes todas: pedian
~190 KB seguidos del monton para leer sus sectores).
- SIN PORTAR: las etiquetas sobre los rivales (4BAD20 = PS2 0x1c4158: 2C00C5 en los tipos 2 / 3 / 6, 2B0046..48 o cifras
  en los 0 / 1; la escena 2C00C5 ya esta en C2CHUD.DAT) y la lista de puestos 4BA8C0 de los tipos 1 / 6.

**Rivales (comun/rival.c).** Vaivenes de los registros sin coche 476CA0 / 476DF0 (PS2 0x1ee8b0 / 0x1eea68: a, b, c de
+0x44 o 1/3, 1, 7 y 0,4, 2, 6; rachas de +28 hasta +24 = (b + azar x (c - b)) x 60 con la bandera de +0x70) y 476F40 (con
+0x48: ninguna mision la tiene), llamados como 47C8A0 / 47CA40 (cfg +0x30 & 8 / & 0x10). Con coche (477090): bit 0 del
rebufo (el del jugador de 47A7C0 y 474080), bit 1 del derrape y bit 3 de 4960A0 (rapido) en `rc_objetivo`.
- 474080 (rebufo del coche de delante, cfg +0x30 & 8): SIMPLIFICADO, PROBABLE: un coche delante a 4..60 m y menos de 2 m de
  lado mas rapido que 0,7 x el suyo: x (v x 0,7) como el del jugador (las constantes 0x628138 / 0x628244 del PC sin buscar
  en la PS2).
- 474990 / 474D30 (el derrape del coche de carreras): SIMPLIFICADO, PROBABLE. Las zonas de derrape son del original
  (MISION.DAT 'MIX2', de la lista del primer puntero de +0x44: nodo, etapa / union, secciones y sus 6 factores; 5
  misiones, 66 zonas); en ellas, en curva (+B54 < 300 m) y a mas de 80 km/h la carroceria se cruza hacia dentro hasta
  0,25 rad x factor y pone la bandera. Sin el +162 del coche, sin las constantes 0x5b43fc.. (no localizadas en la PS2) y
  sin la espera de 0x78 cuadros (+C26) de 474D30.
- SIN PORTAR: 472E60 (ritmo por los coches de alrededor, +BE4). La variante 9 (4B02B0): no aplica (solo el selector 3 de
  la palabra del frontal, que el menu UN JUGADOR no da).
- RAM: las misiones de 5 rivales de modelos distintos (tipo 1, y 18 de las 94 en total) troceaban el monton y se quedaban
  sin colisiones ("COLI: sin RAM", tambien en 059fc97 con -DPRUEBA_MISION=21) y sin HUD. rc_modelos_carga solo carga el
  segundo y siguientes con 906 KB libres (colisiones 406 + HUD 180 + modelo 80 + la lectura del curso 240, medido); los
  demas se dibujan con el primero (DESVIACION, como ya pasaba con mas de 6). 12-1: 3 modelos, colisiones y HUD bien.
- Poligonos (peticion del usuario): RCmm.DAT con `conv_trafico.py --rivales --ligero` (DESVIACION pedida, la de los
  coches del trafico: LOD 0 -6..-10 % de triangulos; tools/regenera.py la pasa). No se dibujan mas coches que antes (el
  tope c1c de la mision y los 16 del trafico).

**RAM al arrancar.** mision.o y rival.o con -Os; los registros de los coches de carreras (rival.c: 100 x 0x7C y la
clasificacion) pasan de la BSS al monton solo mientras hay mision (calloc en 47CF40): el programa ocupa 4 KB menos que
059fc97 (text + data + bss 2946596 frente a 2950676). Medido en Flycast: "RAM: al arrancar ... libre 376K" con
`-DPRUEBA_AUTO -DPRUEBA_MISION=21` (059fc97 igual: 372K); sin defines (nav3.sh, MODO_JUEGO=1): libre 356K, "VOZ:
VOCES.DAT, 35 muestras", sin "Out of memory", carrera OutRun 647K libres. En la carrera: 12-1 (tipo 1) libre ~390 KB,
tipos 2 / 3 / 6 ~480-640 KB tras las colisiones; VRAM libre tras el HUD 85-105 KB.

**MISION.DAT** (tools/conv_mision.py): detras de las millas el bloque 'MIX2' (punto de las marcas 0x40c370 por modelo y
las zonas de derrape por mision); lo anterior igual byte a byte (la demo de antes lo lee igual).

**Verificado en Flycast (E, pantalla :93, `-DPRUEBA_AUTO -DPRUEBA_MISION=i -DPRUEBA_SIN_TIEMPO`):**
- 12-1 (i 21, tipo 1; `-DPRUEBA_ESCALA=1.4f` para que el piloto no sea ultimo): "¡KO! en 5..1", eliminada Clarissa
  ("¡KO! CLARISSA"), la posicion pasa de 5/6 a 5/5; tres eliminaciones seguidas cada 40 s; sin la escala, el piloto
  ultimo a los 40 s: "mision fallada, nota 1" y el fin del tiempo.
- 3-1 (i 5, tipo 2, `-DPRUEBA_PUNTOS`: derrape forzado a ratos, el piloto no derrapa): puntos del jugador y del rival
  ("Rival", 7 cifras), notas sobre el coche y la grande que vuela a los puntos; el rival gana puntos con sus rachas; la
  nota pasa de 3 a 4 cuando sus puntos pasan los del rival.
- 9-1 (i 15, tipo 3, `-DPRUEBA_PUNTOS`): puntos, rebufo, estilo de las cifras cambiado fuera del asfalto.
- 6-1 (i 9, tipo 6, `-DPRUEBA_ESCALA=1.4f`): corazones del jugador y del rival, el del rival baja con el detras, pillado:
  "¡PLANTADO! WOLF" en lugar de GOAL con el coche quieto, nota 6.
- Sin "Out of memory" en ninguna (059fc97 con la 12-1: sin colisiones). Capturas (fuera del repo, en el scratchpad de la sesion:
  modos/c2c/cap/): tipo1_hud, tipo1_ko_cuenta, tipo1_eliminado, tipo2_derrape, tipo3_rebufo, tipo6_pillado.
- Cuadros por segundo y poligonos (`-DDIAG_MS`, lineas MS, 60 medias de 30 cuadros, la misma mision con 059fc97 y lgdat2
  frente a esta rama, el piloto igual): 12-1 (tipo 1) 39,25 -> 39,23 fps, triangulos por cuadro 13459 -> 13805 (+346),
  vertices 20330 -> 20708 (+378: el HUD de la mision y los rotulos; 059fc97 ademas sin colisiones); 3-1 (tipo 2, con los
  puntos y las marcas) 37,69 -> 37,61 fps (55 medias), triangulos 18814 -> 18490 (-324), vertices 29374 -> 29090 (-284: los
  RC ligeros compensan el HUD). Menos de 0,1 fps en las dos.
- PENDIENTE de ver en Flycast: la carrera entera hasta la meta de un tipo 2 / 3 y el modo 35 desde el menu real (el flujo
  de la meta es el del tipo 0, sin cambios), la marca 2B000E del choque del tipo 3.

**Sin hacer: los campeonatos (variante 5).** El gestor de peticiones (PC 7F9460, evento 0x190) no tiene fuente en la
decompilacion (corre el x86 traducido) y en la PS2 esta hacia 0x22xxxx sin seguir; es un modulo entero (peticiones de
Req_*.bin por curso, su HUD y sus resultados). El mapa de los campeonatos ya estaba (Rev 1.2); A en una casilla sigue
dando el sonido de bloqueado.

### C2C, segunda tanda (agente c2c, 2026-11-27): verificacion por el menu, 472E60, 474080 y la espera de 474D30

**Verificado en Flycast por el menu real** (licencia nueva con `-DPRUEBA_NOTAS_C2C`, el piloto conduce:
`-DPRUEBA_PILOTO_MENU -DPRUEBA_SIN_TIEMPO -DPRUEBA_PUNTOS`, salida cerca de la meta con `-DPRUEBA_NODO` / `-DPRUEBA_FIN`):
- Tipo 2 entrando por el menu: UN JUGADOR -> COAST 2 COAST -> categorias -> mapa (dos a la derecha) -> detalle "3
  PRINCIPIANTE" ("¡¡¡Derrapa para ganar puntos!!!") -> coche, cambio, musica -> `RIVAL: mision 3-1 tipo 2` -> META ->
  `LICENCIA: mision 3-1 nota 3 (guardada 3)` -> modo 34 -> modo 35 "¡MISION FALLIDA!" (nota < 4: 2B0068) con REINTENTAR /
  ABANDONAR.
- Tipo 3 (9-1, `-DPRUEBA_MISION=15` con el menu real) hasta la meta: nota 4, `LICENCIA: mision 9-1 nota 4 (guardada 4)`
  (antes 7, sin jugar), 830 millas, modo 35 "¡MISION SUPERADA!" con SIGUIENTE RONDA / REINTENTAR / ABANDONAR.
- La marca 2B000E del choque del tipo 3 (el corazon roto que sale sobre el coche al restar 2000): vista con el choque
  forzado cada 15 s de `-DPRUEBA_PUNTOS` (que el piloto choque de verdad con otro coche no se ha visto).
- Capturas (scratchpad, modos/c2c/cap/): menu_tipo2_meta_35, menu_tipo3_meta_35, tipo3_marca_2B000E; registros
  log_menu_tipo2.txt / log_menu_tipo3.txt / log_472b60.txt.

**472E60 / 472B60 (PS2 0x1e55f8 / 0x1e5188), HECHO con lo que falta marcado.** El coche de carreras no pasa de la
velocidad que le deja el de delante (+314, tr_del) ni el de delante de ese: con la tabla de la clase del de delante
(MISION.DAT 'PAC1', del ELF: 0x40bcc8 los del trafico, 8 clases, y 0x40bde8 los coches de carreras, 4; 6 filas
{distancia, angulo, factor} y un limite), a menos de la distancia de una fila y con el angulo hacia el menor que el de
la fila (x (1 - |sen(derrape / 2)|)), su velocidad x el factor (con factor < 1: x 0,99 con el jugador, x 0,999 con los
demas); pasados 360,3 cuadros, nada si el de delante va mas deprisa; no con el jugador delante cambiando de carril.
PROBABLE: la clase (+320 del original) es la +324 de la demo; sin el termino de +BB8 ni el empuje de lado +BE4 (3,5 m, que
este trafico no lee). Medido en la 12-1: limita (229 veces en ~80 s, `472B60 n` en la linea RIVAL), 39,23 fps como antes
(triangulos por cuadro 13536, 059fc97 13459).

**474080 (PS2 0x1e8090): constantes del original.** Sin rebufo con |+162| > 15 grados (0x3bb40000 = 360 / 65536); por
los coches de delante en orden hasta 60 m; si el de delante va a mas de 0,7 x su +1C4 (> 1) y esta a menos de 5 de lado: a
mas de 10 el rebufo (x 0,7 x +1C4, +C 0x40000) y si no nada; los de 5 de lado o mas (a mas de 15) solo apuntan el carril
libre (+C08, que este trafico no usa). Sigue PROBABLE la condicion de entrada (+C 0x20000, tomada por cfg +0x30 & 8).

**La espera de 474D30 (PS2 0x1e96a0): no ocurre en el original.** Tras subir de marcha (+208 > +1D8) y con mas de 0x384
cuadros, con azar / 65535 < +BC8 (y sin +8 0x10000) +C26 = 0x78 y durante esos cuadros no se llama al cambio 502420. Pero
+BC8 no lo escribe nadie al abrir el coche de carreras (0x1ed360..0x1ed3dc copia del registro +B78..+B94, +BD0, +BCC y
+C36..+C38, nada en +BC8; en el PC tampoco hay quien lo escriba fuera de su borrado): vale 0 y la espera no salta. No se
porta (seria codigo muerto).

**RAM:** ver abajo (la medida final).

**Campeonatos (variante 5): investigado, NO portado.** Lo averiguado en SLES-53998:
- Es un suceso del juego como el gestor de misiones: la tabla de descriptores de sucesos (5 palabras: inicio, control,
  dibujo, 0, fin) en 0x3ee900..; el de las misiones (0x191) en 0x3eea1c = {0x1f7ce0, 0x1f7d00, 0x1f7d20, 0, 0x1f7d40} y
  justo antes (0x190) el de los campeonatos {0x22db28, 0x22db50, 0x22db70, 0, 0x22dbb0}: envoltorios que llaman a los
  metodos de un objeto en la BSS 0x616238 (0x229e88 inicio, 0x22a048 control, 0x22a2b0 dibujo, 0x22a5b8, 0x22ac00,
  0x22b470 fin).
- Tamano: 0x229e88..~0x234000, unas 108 funciones (~40 KB de codigo MIPS, ~10 000 instrucciones), con su propio lector
  de guion (0x21ba68, 60 llamadas: las peticiones de Req_*.bin), la cola de voces (0x1ac3d8), los coches (0x1e1910) y el
  HUD de sprites (0x164xxx). En el PC (7F9460) no hay fuente: corre el x86 traducido.
- Pantallas: el mapa de campeonatos ya portado (Rev 1.2, frontal 0x240d68..); en carrera las peticiones del guion del
  curso (REQUEST_NORMAL / SPECIAL1..3) con su HUD; el resultado va a los cuartetos de la licencia (+0x36E).
- Portarlo es un modulo del tamano del INFARTO (comun/infarto.c): no cabe en esta tanda. A en una casilla del mapa de
  campeonatos sigue dando el sonido de bloqueado.

**RAM al arrancar (medida otra vez, nav3.sh sin defines, MODO_JUEGO=1, los mismos datos):** 059fc97 "libre 352K"; esta
rama "libre 357K". Para no bajarla: el punto de las marcas, las zonas de derrape y las tablas 'PAC1' van en el monton solo
con la mision (calloc en rival_carrera, ~2,7 KB, fuera con rival_fuera) y fin.o, final.o y nombre.o (pantallas del fin,
no de la carrera) se compilan con -Os como menu.o (text + data + bss 2941256; 059fc97 2950676). Comprobado despues el
camino del fin con esos -Os por el menu real: 6-1 (tipo 6) fallada con el piloto (el jugador ultimo hasta acabar su
tiempo): "Se acabo el tiempo", `LICENCIA: mision 6-1 nota 0 (guardada 0)`, modo 34 y modo 35 "¡MISION FALLIDA!".

## COAST 2 COAST: los campeonatos (variante 5), agente c2c2 (2026-11-27)

Los campeonatos (botones 4..6 de las categorias: Clarissa, Jennifer y Holly) se corren desde su mapa (Rev 1.2): A en
la salida (nodo 0) o en una casilla hecha del detalle de una etapa ya no da el sonido de bloqueado, pasa al coche, el
cambio y la musica y la carrera es la variante 5 con el gestor de peticiones portado (comun/campeonato.c, -Os, sus datos
en el monton solo durante la carrera).

**Lo que es (SLES-53998, averiguado en esta tanda).** El campeonato es un curso de 15 etapas (5 niveles, como el modo
OutRun) con las peticiones de la acompanante en cada etapa; el mapa del menu es ese arbol (nodo i = registro i - 1).
- Cursos: Request_Course_SP / _OR2 / Req_Course_MIX_PS2 (registros de 0x78 como csc_data). Comprobado en la conversion:
  el de Clarissa (grupo 4) es el arbol de OUTRUN2SP (csc_data_cvt, el 1 de CURSO.DAT: BEAC..NEWY, BK15..29) y el de
  Jennifer (grupo 5) el del modo OutRun (csc_data_2, el 0); solo cambian las metas (hijos 0 / -1 en vez de -1 / -1,
  PROBABLE que el 0 no se use: la carrera acaba en la meta). El de Holly (grupo 6, etapas del reves y la union 76)
  necesita 11 uniones nuevas: SIN HACER (ver abajo).
- Configuracion (4EEB50 del PC = la palabra +0x208 del frontal): {curso, forzada = nodo != 0, etapa = nodo - 1, lista =
  casilla, 46C520}. Desde la salida: la carrera entera desde la primera etapa; desde una casilla: la salida en esa etapa
  (nodo_salida en demo/main.c) con su lista en todas.
- Gestor (suceso 0x190, tabla 0x3ee900 {0x22db28, 0x22db50, 0x22db70, 0, 0x22dbb0}, objeto 0x616238; los
  desplazamientos de comun/campeonato.c son los suyos). 0x21ba68 NO es un lector de guion: es el `new` del C++ (60
  llamadas = los 24 objetos de los tipos de peticion de la fabrica 0x22a930, tabla 0x4151f0, y las listas). Las clases
  de los tipos estan fuera del modulo (0x292788..0x2a4800, ~70 KB mas), cada una con su vtable (0x423110 tipo 2,
  0x423718 tipo 1...): +0x24 el control (cada paso, los 24), +0x2c el dibujo, +0x34 y +0x3c.
- Guion (0x22ac00 / 0x22b600): Req_<etapa>.bin (nombres 0x374010 por el tipo del descriptor) y su lista +0x1ff8
  (REQUEST_NORMAL / SPECIAL1..3, 0x374138); peticiones de 0x74 {s16 inicio, s16 fin, ..., +0x58 parametros}; los
  parametros son bloques de 0x88 con nombre propio en el guion {tipo 1..24, subtipo, +0x58 / +0x5c punteros a otras
  categorias del guion (coches y objetos), +0x60.. enteros: +0x7c la velocidad del tipo 1, el % del tipo 12, +0x80 /
  +0x84 los coches del tipo 3}.
- Lista de la etapa siguiente (0x22e850, al entrar en la union): forzada, la del mapa; si no por el histograma de notas
  de las etapas anteriores (0x22e808: ninguna nota menor que n): nivel 1 -> 2: SPECIAL1 sin notas < 4 en los niveles 0
  y 1; nivel 2 -> 3: SPECIAL1 sin < 3, SPECIAL2 si venia de la 1 y sin < 4; nivel 3 -> 4: igual y SPECIAL3 si venia de
  la 2 y sin < 5. Por eso las etapas de nivel k tienen k casillas (0x4172c8) y sus cuartetos se llenan corriendo bien.
- Peticion en curso (0x22b7d0): la primera con inicio - 32 < seccion < fin + 30 (ninguna en la union). Fases
  (0x232988): 1 anuncio (desde inicio - 30: 0x22f9d0 el rotulo, 0x2a6358 la voz), 2 espera, 3 empieza (voz 0x8e8),
  4 en curso (velocidad media), 5 acaba (0x22e670 nota, 0x22e728 corazones), 6 a la etapa (0x22e498) con el rotulo de
  la nota (0x22f7a0) y su voz (0x2a65a8, tabla 0x423df0 por la fila +0xf0 del gestor: PROBABLE = 46C520), 7 hecha.
- Medidor +0x28 (0..80, lo llena cada tipo con el avance del coche mientras cumple) menos la penalizacion +0x2c
  (0x22c818: choque, golpe de uno mas rapido, golpe, muro, ruedas fuera y roce por paso con la fila de 0x374240 que
  elige 0x2323e8; tope 100): nota = el mayor k con medidor - penalizacion >= {0, 10, 20, 30, 45, 60, 80} (0x374190),
  tope 4 con un golpe, -3 con uno mas rapido, -4 con un choque (0x22e6d8). Corazones (0x22e728): los del medidor
  (tope 80, 0x3741c8) y con nota >= 4 {0, 20, 50, 100}[lista] (0x3741d0) + {0, 10, 20, 30, 50}[dificultad] (0x3741e0,
  la DC pasa 0: PROBABLE) + v media x 0,2; con nota 6 cada tipo su bono (vtable +0x24 en la fase 5). Fuera de las
  peticiones: choque -50 (voz 0x950), golpe de uno mas rapido -20 (0x14c), golpe -10.
- Etapa: media de las notas de sus peticiones (0x22ebb0) en la casilla de su lista si se hicieron todas; al terminar
  la etapa (0x1cec50 en 0x22c000) a los cuartetos de la licencia +0x36E + 2 tipo si mejora (0x22edd8, +3F4 |= 2) con
  millas[nota] x (lista + 1) (0x3741f8: 100..1000) a la licencia. El desbloqueo de los grupos 5 y 6 y el porcentaje
  salen de esos cuartetos como ya hacia el menu (0x242290 / 0x2420a0).
- HUD (NAVI_PUB 0x1bd6f4, caso de la variante 5: el panel 2C002A y 0x22a5b8): las filas 0x1bd920 por nivel y el
  contador "%4d" de los corazones en (0x206, 0x1b2): los del INFARTO (HAHUD.DAT); los rotulos del anuncio (marco
  2B0020, el generico 2B003D y a los 30 cuadros el del tipo) y de la nota (2B001E / 2B001F y 2B002D..31), los de HAHUD.DAT.

**Datos (tools/conv_camp.py -> CAMP.DAT, 20,8 KB):** las tablas del ELF de arriba (cada una comprobada por el codigo
que la lee), las 371 peticiones de los 60 guiones (inicio, fin, tipo, subtipo, +0x60..+0x84), el rotulo y la voz del
anuncio de cada una sacados ejecutando en la conversion los switch del original (0x22faec y 0x2a636c con ps2/mips.py),
los textos de la nota (0x22f8b0) y los tipos de las etapas de los tres arboles. CURSO.DAT no cambia.

**Tipos portados (su control de la vtable +0x24):** 1 pasar de una velocidad (0x29c828: voces 0xf0 / 0xf1, animo cada
0x6f cuadros), 2 derrapar (0x298360: voz 0x8c6, el medidor en el 90 % de la peticion), 3 adelantar coches (0x29f038:
80 / p80 por coche hasta p84, efecto 0xd8 del convoy), 4 no chocar (0x29ec18: el 90 %; los sucesos penalizan y topan
la nota; con un suceso el gestor llama a 0x22e9a0, que calcula la nota y llena un HUD de 0x3d88: SIN PORTAR), 12 ir al
rebufo (0x2a4198: p7c % de la peticion). SIN PORTAR (se anuncian con el rotulo generico y acaban con
nota 0): 5 los colores, 6 conos, 7 monedas, 8 arcos, 9 (otra clase de derrape: se trata como el 2), 11 chocar con los
coches, 13 alimentar a la chica, 14 el total, 15 la pelota, 16 los minijuegos (secuencia, sonido, suma, regalos,
ovejas, coches), 19 meteoritos, 20 lineas, 21 OVNIs, 22 la bocina, 23 patos, 24 fotos: necesitan los objetos de las
categorias del guion (+0x58 / +0x5c), los coches de la peticion (0x22d088 / 0x22d530 / 0x22d7a0, 0x1e1910) o sus
sprites del banco 0x46 (no convertido). Tampoco: los rotulos del banco 0x46 (los de los tipos 1, 12 y los minijuegos:
DESVIACION, sale el generico), el letrero 3D de inicio / fin de la peticion (0x232e60 con 0x1a7c80 / 0x1a7cb0), la
silueta 2C0067 / 0x460064 que se llena (como en el INFARTO), las reacciones de la acompanante (0x205288 / 0x2a66a8) y
la vibracion (0x2052f8). -> HECHO despues: 6, 7, 8, los rotulos del banco 0x46 y las reacciones (ver "tercera tanda"),
15, 19 y 5 (ver "cuarta tanda"), 11 (subtipos 0 y 1) y las voces de las peticiones (ver "quinta tanda"), 20, 21, 14
  (PARCIAL) y el letrero 3D de inicio / fin (ver "sexta tanda").

**Holly (grupo 6, Req_Course_MIX_PS2):** `tools/conv_union.py --arboles=...,302` ya sabe hacer su arbol (ARBOLES_REQ):
EAST_R / BK262, LASV_R / BK263 ... PALM_R / BK95 (comprobado: los demas arboles de CURSO.DAT salen igual byte a byte),
pero sus 11 uniones nuevas toman los numeros 262..272, que son los de las uniones de la Contrarreloj (CURSOTA.DAT,
--bk-desde=262): hay que numerarlas detras de esas y meterlas en UNIONES.PAK (tools/regenera.py). Sin el arbol 302 en
CURSO.DAT, A en su mapa da el sonido de bloqueado (menu_camp_puede).

**Verificado en Flycast (E, pantalla :93; capturas y registros en el scratchpad de la sesion, modos/c2c/cap/camp_*):**
- Por el menu real (licencia nueva, `-DPRUEBA_PILOTO_MENU -DPRUEBA_SIN_TIEMPO`): UN JUGADOR -> COAST 2 COAST -> abajo
  (Clarissa) -> su mapa ("RECORRIDO OUTRUN2SP", camp_mapa_clarissa) -> A en la salida -> coche, cambio, musica ->
  `MODO: variante 5`, `CURSO: arbol 1`, BEAC; el HUD del INFARTO (Tiempo total / Tiempo / Corazones, filas "Etapa n" y
  el contador); las cinco etapas BEAC -> SANF -> NIAG -> LASV -> PRIN hasta la META con sus peticiones de la lista
  NORMAL (BEAC 11/0 y 3/0; SANF 11, 15 y 4; NIAG 3, 15 y 4; LASV 11 x 3; PRIN 15 y 3); el anuncio generico (la bombilla,
  camp_anuncio_generico) en los tipos sin portar y el del tipo ("¡Sigue adelantando a los coches!", camp_anuncio_tipo3),
  el rotulo de la nota (camp_nota0; camp_tipo4_nota2_filas: no chocar en SANF, medidor 81 - 25, nota 2 por el tope
  del choque, 56 corazones en la fila "Etapa 2"), y en cada etapa `LICENCIA: campeonato etapa tipo t casilla 0 nota n`
  con sus millas (100 / 400: 800 en total), camp_meta_prin. Sin "Out of memory"; en carrera ~500 KB libres.
- Salida forzada (`-DPRUEBA_AUTO -DPRUEBA_CAMP=1 -DPRUEBA_CAMP_NODO=5 -DPRUEBA_CAMP_LISTA=1 -DPRUEBA_CAMP_PASA`: el
  nodo 5 del mapa con la casilla 1, y un adelantamiento cada 1,5 s y medio derrape de prueba porque el piloto apenas
  adelanta): sale en el registro 4 (tipo 16) con la lista SPECIAL1 ("¡Choca con el fantasma!", 11/1, y 3/0;
  camp_forzada_nodo5_lista1), el tipo 3 llena el medidor (133 - 0, nota 6, 80 + 126 corazones, la cara de corazones,
  camp_tipo3_nota6), etapa nota 3 en la lista 1 y la siguiente (LASV) otra vez con SPECIAL1.
- Jennifer (grupo 5, `-DPRUEBA_AUTO -DPRUEBA_CAMP=0 -DPRUEBA_CAMP_PASA`, que ademas da rebufo y +60 km/h la mitad del
  tiempo): `CURSO: arbol 0`, PALM con 12/0 (145..410) y 1/0 (475..560) del guion de Palm_Beach; el rebufo llena el
  medidor (91 - 3, nota 6, 80 + 34 corazones; camp_jennifer_palm_tipo12_nota6), la velocidad (pasar de 265 km/h) solo 8
  (el piloto no llega), etapa nota 3. El +0x7c del tipo 12 es un flotante (lwc1 en 0x2a41f8): leido como entero el
  medidor no subia (corregido).
- El paso a SPECIAL1..3 por las notas sin forzar (0x22e850) no se ha visto: el piloto no saca notas de 4 o mas en todas
  las peticiones de dos etapas seguidas. Tampoco se ha visto entero el camino de Jennifer desde el menu (se abre con
  Clarissa en rango A: ver el desbloqueo arriba) ni B en el coche de vuelta al mapa de las peticiones.

**RAM.** Al arrancar no se reserva nada del campeonato: CAMP.DAT (20,8 KB) y el estado (~1 KB) van en el monton desde que
sale del menu hasta que vuelve (camp_inicia / camp_libera). campeonato.o con -Os (5,9 KB). Para no bajar la RAM del
arranque (el codigo nuevo de main.c / menu.c / campeonato.c son ~8 KB) se compilan tambien con -Os modulos que no son
el dibujo ni la fisica de la carrera: infarto, fsp, vmuesc, sprani, rksp, modo_video, registro, voces, vmu, sfx, gd,
musica, pcarga, meta y sp (text + data + bss 2935336; 5b392f5 2941256). Medido con tools/prueba/nav3.sh (MODO_JUEGO=1,
sin defines, los mismos datos): 5b392f5 "libre 357K", esta rama "libre 364K", "VOZ: VOCES.DAT, 35 muestras", sin "Out
of memory" (el libre del arranque va a saltos: con 2 KB menos de programa salio 356K, por como queda la cima del monton).
En el campeonato, ~500 KB libres en carrera.

## INFARTO: tipos de peticion, hoja de resultados, panel de la nota y la acompanante (Rev 1.1, 2026-11-27)

Rama rev-1.1 (059fc97). Todo verificado en Flycast (nivel E), no en la consola. Ficheros nuevos de la conversion:
INFARTOX.DAT (tools/conv_infarto.py `extra`, tambien con `--solo-x`), CHRH.DAT (tools/conv_chr.py `--infarto`);
HAHUD.DAT cambia (grupos nuevos). regenera.py los saca en el paso `infarto`.

**Uso real de los tipos** (las 41 misiones de cada preset, INFARTO.DAT / INFARTOP.DAT): preset 0 (juego OutRun 2 SP)
0 x16, 2 x9, 6 x9, 7 x13, 8 x8, 9 x3, 11 x4, 14 x3, 15 x16, 16 x15, 19 x1, 20 x1, 22 x1, 23 x5, 24 x4, 25 x1; preset 1
(juego OutRun 2, el de por defecto del tablero) solo 0 x52, 2 x9, 6 x8, 7 x23, 8 x21, 9 x1, 11 x6. 17, 18 y 21 no los
pide ninguna mision de ningun preset. Los rotulos de cada tipo (2B00xx de 0x3f0aa8, mirados en PNG): 8 "Circula por
el azul" (p6 = 1) / "por el rojo" (p6 = -1), 23 "Derrapa y circula por el azul / rojo", 7 "Golpea el cono azul", 24
"Derrapa y golpea el cono azul", 9 "Cruza la linea" (entre dos coches), 11 "Atrapa el corazon", 14 "Supera a ese
coche", 15 "Recoge monedas", 16 "Pasa por el arco", 25 "Derrapa y pasa por el arco", 19 "Recoge las estrellas", 20
"Choca con el fantasma".

**HECHO: tipos 8 y 23** (comun/infarto.c; los mas pedidos sin objetos: 29 en los dos presets + 5):
- Puesta en marcha 464350 caso 4 (PS2 0x1ae800): 7F2530 / 7F2532 secciones, 7F2538 = p6 (el lado), 7F253C = -1.
- Control 45EEB0 (PS2 0x1a9af8, comprobado instruccion a instruccion): puntos de 45E2F0 con la tabla 0x3efcb8
  {150000, 150000, 333, 0, 333, 0}; cuenta las secciones mientras p6 x car+0x26C > 0 (el lado de la carretera) y
  +0x264 x +0x268 < 0 (dentro de los bordes) y, en el 23, derrapando (0x1b3b10: +D36 > 0); con ello la cara 5 de la
  acompanante (487D90) y 487DD0; resultado = secciones / ((fin - inicio) x 0,9) x 800000 (0x1a9aa0), el mayor.
  En la DC: +26C = fis.ofs, +268 = vib_izq, +264 = -vib_der (los que ya usaban la vibracion y 476040).
- Evaluacion 463250 (PS2 0x1a9c70): nota 7 -> 80 corazones + min(1, secciones / (fin - inicio) / 0,1) x 300000 +
  mantiene(v inicial - v) x 400000 (0x1b3b70), contador (h + h / 2) x 10 / 7 y el panel de la nota 0x78 cuadros.
- E (Flycast, PRUEBA_HA_MISION=15 preset 1, piloto del menu): "peticion tipo 8 (250..315, p6 -1)" con el rotulo
  "Circula por el rojo"; el piloto iba por la derecha: 0 secciones, nota 1; la segunda (584..622, p6 1) cuenta
  secciones (k8 6/38 en el registro DIAGHA) y sigue en la etapa siguiente como el original (464350 no la cierra).
  El 23 es el mismo codigo con el derrape (solo comprobado al compilar: ninguna prueba paso por la mision 16..32 del
  preset 0).
- SIN PORTAR: los carriles pintados de azul / rojo de esas misiones (45ECA0 / 45E9B0 goal_models: registros de
  obj_course_obj_cs_<zona> por mision, p. ej. 0x5e0003 / 0x5e0004 de CAST en la mision 0x0F del preset 1, 0xfb0001
  de NIAG en la 0x0F del preset 0) y las marcas de principio y fin de la accion -5 (45B000 / 45B2A0, modelos por
  idioma 64A9E8 / 64AA00). Sin ellos el jugador solo tiene el dibujo del rotulo (mitad azul a la derecha).

**HECHO: la hoja de resultados de la etapa** (4603F0 = PS2 0x1ad004, comprobada en el ELF; comun/infarto.c
`hoja_paso` / `infarto_dibujos`, demo/main.c `ha_dibujos`): mientras el jugador esta en la union (7F8B34: cuadros en
ella, a 0 en el stage clear) y no en el nivel 4: a los 0x78 el efecto 0xA9, el titulo del nivel (0x3eff60:
2C0093..97), las peticiones del nivel (2C0090..92) y una fila por peticion cada 0x3C cuadros (y de 0x3eff98) con su
nota (0x3eff78, primero corriendo por (t >> 2) & 7) y su voz (0x3f7308) y sus corazones "%3d" en (0x19D, 0x3effb0);
a los 0x168 "Ranking en la etapa" 2C0081 con su animacion (0x3effc8); a los 0x1A4 la nota media: voz, rotulo
2B0007..0D (0x3effe8, entero) y el movimiento de la acompanante (0x3f0b50 / 0x3f0b70); de 0x21C en adelante, con la
rama ya elegida, la clase de la mision especial de la etapa siguiente (45C7E0 = 0x1af710, tabla 0x3f06e0) con 0xDB
y 2B003F..41 a los 0x276 y las voces 0x1CC.. a los 0x294. Sprites: 429530 + 4294C0 (PS2 0x1156c0: la mitad del
tamano de la escena) -> el uso de HAHUD.DAT lleva (w / 2 - 320, h / 2 - 240) y la demo pasa la x / y del original.
E: "Etapa 1 Resultado", filas 1 y 2 con la nota corriendo y luego E, "Ranking en la etapa" y el rotulo "Ranking E"
(capturas cap/t9/h_00xx). No visto: el rotulo de la mision especial (la union de la prueba acabo antes de 0x21C).

**HECHO: el panel de la nota que entra** (45BDF0 = PS2 0x1ad6f4): 2C0080..7C (0x3f0010) por la nota 7F8B2C, 7F8B28 =
0x78 al cerrar la peticion (baja 1 por paso en 45FB80, PS2 0x1acf48); x = (1 - (t - 0x5A) / 30) x 384 - 0x9C mientras
t > 0x5A, luego 0xE4; y 0x5D. E: "Ranking E" entra por la izquierda bajo el tiempo (cap/t10).

**HECHO: las reacciones de la acompanante** (comun/robot.c `robot_pide`, `robot_extra`): el work +68..+7C de 489A90
(PS2 0x203760..0x20385c y 0x203f6c..0x204028, comprobado): +68 un movimiento (487D60: +78 = 2), +70 una fila de caras
(487D90: +78 = 0, +7C = 1; filas {entrada, bucle, salida} de 0x40eac8; cerrado -> la fila 1), +7C = 0 (487DD0);
estado Pose hasta el fin del movimiento con +78 = 2. Llamadas de las peticiones como el original: tipo 0 la cara por
(7F8AA0 - 7F24E8) / 80000 (0x1a8e50, tabla 0x3efb40) y 487DD0 al acabar el impulso; 2 / 22 el animo (0x1aac60:
coche 9 -> 0x6003D; si no rand x 2^-31 < 0,5 -> 0x6003F / 0x6003D, si no 0x6003E); 6 la cara 0 sin sucesos; 8 / 23
la cara 5; al cerrar cada peticion 45DED0 (0x1b3988: tablas 0x3f0a68 / 0x3f0a88 por nota; el coche 9 con nota 5
nada) y la nota media de la hoja. Los 35 movimientos que no lleva la pareja (0x6003D, 0x6006D, 0x230001, las filas de
caras 0x25xxxx / 0x26xxxx / 0x27xxxx...) van en CHRH.DAT (52 KB), leido al empezar el modo y buscado detras de los de
la pareja (las CHRppaa.DAT no: CHR_RAM va al limite con la mayor). E: registro DIAGHA "acompanante: movimiento
0x60048 (esta)", "cara 5 (0x25000d, esta)", "movimiento 0x230003 (esta)". PROBABLE: al volver de la pose a reposo no
se pone 0x6004B (el original sale con Exit::Reset, sin descifrar si eso lo hace).

**RAM** (Flycast, menu real hasta el INFARTO): arranque igual (343 K libres, "VOZ: VOCES.DAT, 35 muestras"); menu al
salir 262 K (antes 314: CHRH.DAT), carrera del INFARTO 289 K (antes 341), lecturas del curso 241..314 K, sin "Out of
memory". En el AICA nada nuevo (VOCESH.DAT igual). Codigo +5 KB. HAHUD.DAT: 640 -> 1147 KB en el disco (sus tablas
en la RAM crecen poco: solo los grupos nuevos); las texturas de cada grupo se suben al verse y se bajan al dejar de
verse (`ha_dibujos`).

**SIN PORTAR (PENDIENTE), con lo averiguado:**
- Tipos con objetos de OSO.PS2 (7 / 24 conos, 15 monedas, 16 / 25 arcos, 19 estrellas en cajas que caen, 17 / 21):
  siguen sin empezar (ni anuncio ni nota). Lo que falta esta en "Objetos de las peticiones" (arriba): ficheros por
  mision (0x3f05c0 / 0x3f06a0), clase -> modelo 0x40a958 (la misma tabla que usan los objetos de la union, BO.DAT: el
  formato de registro de 0x28 es el mismo), alturas 45DB70 y toque 0x1d3720. Se puede apoyar en el sistema de BO
  (conv_bo.py / bo_dibuja) para los modelos del banco 0x57. Tablas de puntos ya en INFARTOX.DAT (PTS2: 15 0x3f07c0,
  16 / 25 0x3f07d8, 19 0x3f0810). Controles y evaluaciones en el PC (race_traffic_heart*.inc) = PS2 0x1a9290 (7/24),
  0x1b0260 (15), 0x1b0570 (16/25), 0x1b11c8 (19), 0x1b0c58 (17), 0x1b24e0 (21).
- 9 (pares de coches 46D250 / 800AF8 del modulo del trafico), 11 (anillo de marcas 46DB60 / 8037C8), 14 (escoltas
  45B5B0 tabla 64AAD8 y la carrera 45F7F0 con el contador 2B0005 / 2B0006), 20 (coches fantasma: tabla de coches con
  0x40, +C5C 0x80000, corazones 2C00E5 sobre ellos 45D190), 18: sin portar (tablas de puntos de 9, 11, 14 y 20 ya en
  PTS2). Prioridad por uso: 11 (10), 9 (4), 14 (3), 20 (1).

## INFARTO: objetos de las peticiones, tipos 7 / 24 y 15 (Rev 1.1, 2026-11-27)

Sobre la seccion anterior. Datos nuevos: HAOSO.DAT y HAOBJ.DAT (tools/conv_infarto.py `objetos`, tambien con
`--solo-obj`; regenera.py paso `infarto`). Codigo: comun/haobj.c (objetos), comun/infarto.c (tipos), demo/main.c
`ha_obj_dibuja` / `ha_suelo` / `ha_seccion` (dibujo, suelo y seccion de un punto). Verificado en Flycast (E).

**Objetos (45C470 / 4F0D10 / 45DB70):**
- Fichero por (preset, mision) de 45C470 = PS2 0x1af428 (comprobado): preset 0 y mision >= 15 tabla 0x3f05c0, preset 1
  y mision >= 15 tabla 0x3f0630, mision < 15 solo con el preset 0 (tabla 0x3f06a0 por la mision base). 45 ficheros
  oso_qt_* (2983 objetos, el mayor 249). Clase -> modelo y escala 0x40a958 (conos 0x57001e / 0x1f / 0x19, monedas
  0x1c / 0x1d, arco 0x2b, caja 0x30). Las clases de otro recurso (la luz 56 de FLOR, la 3) no van.
- Colocacion: el marco de la etapa (el mismo que el de la geometria: el coche sale en (0, 0, -16)); clases 0x0A / 0x22 /
  0x39 a la altura del suelo bajo (x, 9999,9, z) con la consulta del suelo de la demo (coli_altura 0x100 = 43F2F0);
  clases 5..9 + 0,65 en y; las monedas (tipo 6) a 1,2 (la 57 a 1,0) y girando 0,05 por paso (0x1d3d48).
- Toque: solo el jugador; la caja del coche 2,3 x 1,1 m mas 0,3 (conos) / 0,8 (monedas) contra el punto del objeto
  (PROBABLE: la caja +D28 del original no esta leida). Un paso (+3F8 / +3FC == 8).
- SIMPLIFICADO (DESVIACION): los conos tocados salen volando (velocidad del coche, 5 m/s arriba, girando) y se quitan a
  los 2 s, sin el cuerpo rigido 516E10; las monedas suben con el coche y se encogen como 0x1d3fc8. Las clases 5..9 sin
  el marco de la carretera 45D950 (los angulos del fichero). La seccion de un objeto (43F370) sale del punto de la
  ruta mas cercano (APROXIMADO: el original lee la seccion del poligono de la COLI).
- Dibujo: HAOBJ.DAT (ORD7 de conv_bo.modelo_conjunto, registros 0x19, 0x1c, 0x1d, 0x1e, 0x1f, 0x30: 56 mallas, 315
  vertices, 21 KB de texturas, 30 KB) leido al empezar la carrera del modo (texturas fuera de la zona, sube_tramo);
  como mucho HA_OBJ_MAX = 24 objetos, los mas cercanos al ojo dentro de 160 m (HAOBJ_LEJOS), sin los de detras de la
  camara, con el tope de los objetos de la union (ta_bo_tope). Por objeto (el modelo del original no tiene LOD; no se
  simplifica): cono 19 triangulos / 39 vertices (10 tiras), moneda y corazon 63 / 69 (3 tiras), caja 20 / 60.
  Por cuadro, medido (registro DIAGHA objetos): conos hasta 17 dibujados, 171 triangulos / 351 vertices; monedas 24
  dibujados, 756..1260 triangulos / 828..1380 vertices.
- fps (Flycast, DIAG_MS, SEQU con la peticion 15 cargada: monedas en la linea del coche, 11 muestras de 30 cuadros):
  con objetos 39,85 fps, dib 16,73 ms, 15997 tri / 23627 vtx; la misma pasada sin dibujarlos (DIAG_SIN_HAOBJ) 39,86 fps,
  dib 15,72 ms, 15036 / 22537. O sea +961 triangulos, +1 ms de dibujo y -0,01 fps (Flycast; en la consola el dibujo
  cuenta mas: PENDIENTE medirlo alli).
- RAM: al arrancar 333 K libres (antes 344: el codigo crece ~11 KB; el monton en uso igual, 13021 K; nav3.sh sin "Out of
  memory" y con "VOZ: VOCES.DAT, 35 muestras"). Los objetos de la mision en el monton (~7 KB como mucho) y HAOBJ.DAT (~30
  KB) en la carrera del modo. Medido
  (carrera del INFARTO con DIAG): 311 K libres al empezar; sin "Out of memory" ni "sin VRAM".

**HECHO: tipos 7 y 24** (45E8A0 = PS2 0x1a9290 y 463150 = 0x1a9470, comprobados): cuenta los toques de la clase p6 (el
24 solo derrapando) con la cara 3 de la acompanante, menos 3 por cada cono de la clase 6; resultado 800000 x
buenos / p8 (800000 con p8 o mas); nota 7 + 700000 x mantiene(v). E: mision 16 (preset 1) en la etapa de la clave 4
(PRUEBA_NODO=4), 14 conos azules y 2 de la clase 6: (14 - 6) / 20 x 80 = 32 corazones, nota 4; capturas
cap/o1/h_00*.png con los conos en la carretera y volando al tocarlos.

**HECHO: tipo 15** (45C950 = PS2 0x1ae8a8, 4617A0 = 0x1b0260 con la tabla 0x3f07c0, 463DF0 = 0x1b03e0): las monedas
0x0A y 0x39 tocadas (efecto 0xD6, cara 4), resultado cogidas x 800000 / p8; nota 7: 700000 x cogidas de la 0x0A / las
del tramo (45C8E0) y + (cogidas - p8) / 2 corazones. E: mision 1 (preset 0, juego OutRun 2 SP, clave 4), 15 de 15
monedas del tramo (capturas cap/o2/h_00*.png: suben y se encogen; el medidor recibe los corazones). La nota salio 1
porque el piloto de prueba iba por la tierra (los puntos de 45E2F0 por rueda fuera del asfalto), no por las monedas.

**Pruebas** (solo con defines; no cambian el juego): PRUEBA_HA_MISION=m (la mision de todas las etapas), PRUEBA_NODO=k
con PRUEBA_PILOTO_MENU (la etapa de salida), PRUEBA_HA_TOQUE=clase (los objetos de esa clase, -1 todas, se ponen en la
linea del coche a 25..40 m: el piloto no apunta a ellos), DIAG_HA (registro), DIAG_SIN_HAOBJ (sin dibujarlos).

**SIN PORTAR (PENDIENTE):** 16 / 25 (arcos: modelo 0x2b de 4 nodos de 174 triangulos con animacion de vertices y
carteles + los postes de 42; la anchura por carril de la accion -11, 4611D0 con la tabla 0x3f7218, y el cruce
0x1d42f0), 19 (cajas que caen y sueltan estrellas: 461940, tabla 64AAA8), lo visual de 8 / 23 (carriles pintados
45ECA0 / 45E9B0 y marcas de la accion -5) y 11, 9, 14, 20.

## INFARTO: poligonos de los objetos, RAM del arranque y arcos 16 / 25 (Rev 1.1, 2026-11-27)

**Poligonos (DESVIACION pedida por el usuario: los objetos del modo, reducidos):**
- Moneda y corazon (0x1c / 0x1d, un disco de 16 lados, 63 triangulos): tools/conv_infarto.py `aligera_monedas` los
  rehace como un disco de 8 lados con el mismo radio, grosor, textura, UV y color: canto 16 + caras 6 + 6 = 28
  triangulos / 34 vertices; y una version lejana (registro + 0x80) solo con las caras, 12 triangulos / 16 vertices, que
  la demo usa desde 40 m (`ha_obj_dibuja`). En las capturas (cap/mon2) no se nota el octagono a la distancia de juego.
- Arco (0x2b): `aligera_arco` deja los postes (nodo 5, 42 triangulos) y uno de los 4 nodos de destellos (cuadro 0, sin
  la animacion de vertices ni la cara a la camara, y de sus tiras de 4 vertices solo las 10 primeras): 42 + 144 = 186
  triangulos (antes 738); desde 40 m solo los postes (42).
- Medido otra vez (SEQU, peticion 15 cargada, 11 muestras de 30 cuadros, Flycast): objetos 234..352 triangulos por
  cuadro (antes 756..1260); media de la escena 15423 tri / 22960 vtx con objetos frente a 15036 / 22537 sin dibujarlos
  (+387 tri, +423 vtx; antes +961 / +1090), dibujo 16,52 frente a 15,72 ms (+0,8), 39,85 frente a 39,86 fps.
  Arcos (LASV, peticion 16): 213..624 triangulos de objetos por cuadro con 10..17 a la vista.

**RAM del arranque:** -Os tambien en infarto.o, haobj.o y en lo que no es de la carrera o no pesa en ella (fsp.o,
rksp.o, vmuesc.o, vmu.o, registro.o, modo_video.o del frontal / VMU, y final.o, nombre.o, pcarga.o, sp.o: el final de
recreativa, el nombre del ranking, la pantalla de carga y el frontal de OUTRUN2SP) y la lista de dibujo de los objetos
en la pila: el programa queda en 769 KB de texto (antes de la Rev 1.1, 770 KB). nav3.sh: "RAMMAPA arranque: libre
350K" (antes de la Rev 1.1 344, con los objetos sin esto 333), "VOZ: VOCES.DAT, 35 muestras", sin "Out of memory".

**Tipos 16 y 25 (arcos)** (461850 = PS2 0x1b0570 con la tabla 0x3f07d8; evaluacion 0x1b0810, la misma cuenta que la
del 7): cuenta los arcos de la clase p6 (0x0B) cruzados (el 25 solo derrapando) con la cara 4; resultado 800000 x
cruzados / p8.
- Accion -11 (4611D0 con la clase 0x10 = PS2 0x1afa48): los anchos de la tabla 0x3f7218 (INFARTOX.DAT ARCO) {inicio,
  medio, final, reparto} x los carriles del primer arco (+0xA4: aqui los carriles distintos de la ruta en ese punto,
  PROBABLE), los n del tramo de la accion -5 repartidos; si 3 x ancho < 2,5, 0,8333 (0x3f555555).
- Colocacion: la clase 0x0B con el marco de la carretera (45DB70 / 45D950): aqui el giro de la ruta en el punto mas
  cercano (APROXIMADO: sin la inclinacion de la carretera).
- Cruce (0x1d42f0): el paso del coche (posicion anterior -> actual, xz) contra el segmento de los postes en +-3 x ancho
  y la altura del arco + 3 x ancho por encima del coche; despues el arco gira y se quita a los 3 s, parpadeando el
  ultimo (0x1d4508). Sin el toque de los postes (1 / 2, efecto 0xCD) ni el sprite 2C0002.
- E: mision 3 (preset 0) en la clave 7 (LASV): "20 arcos en 100..150, anchos 1,50" (4 carriles x 0,374), 2 cruzados
  con PRUEBA_HA_TOQUE (el piloto no apunta), +7 corazones (cap/arc). PROBABLE: la colocacion se ve a los lados de la
  carretera en las capturas; sin poder compararla con el original no se da por verificada.

**SIN PORTAR (PENDIENTE):** 19 (cajas que caen y sueltan las estrellas: 461940, tabla 64AAA8), lo visual de 8 / 23
(carriles pintados 45ECA0 / 45E9B0 con registros de obj_course_obj_cs_<zona> por mision; marcas de la accion -5
45B000 / 45B2A0 con los modelos por idioma 64A9E8 / 64AA00) y los tipos 11, 9, 14 y 20. -> HECHO el 19, los carriles,
las marcas y el toque de los postes: ver "INFARTO 2" (abajo); siguen sin portar 11, 9, 14 y 20. -> HECHO: ver "INFARTO 3".

## INFARTO 2: estrellas (tipo 19), carriles pintados, marcas de la accion -5 y postes de los arcos (Rev 1.1, 2026-11-27)

Sobre las tres secciones INFARTO anteriores (rama de 6fd5886). Todo verificado en Flycast (nivel E), no en la consola.
Datos nuevos o cambiados (tools/conv_infarto.py): INFARTOX.DAT con las secciones EST1, MARC y META; HAOBJ.DAT con la
estrella lejana y las dos marcas; HAMpqq.DAT (24 ficheros, `--solo-metas`; tambien en la conversion entera).

**HECHO: tipo 19, "Recoge las estrellas"** (comun/haobj.c `haobj_est_paso`, comun/infarto.c `est_*`; solo lo pide la
mision 39 del preset 0, FLOR). Lo averiguado (PS2, comprobado instruccion a instruccion):
- Lo que el PC llama "cajas" son estrellas: la clase 0x12 es el modelo 0x570030 del banco 0x57, una estrella de 5 puntas
  en bipiramide (20 triangulos sueltos); los objetos de oso_qt con clase 0x12 llevan el tipo 8 = suceso 0x61 (inicio
  0x1d4b60, control 0x1d4d80, = PC 4AADC0 / 4AC440).
- Inicio 0x1d4b60: reposo +A4 = el suelo bajo el punto (0x18dfc8 modo 0x100); +CC la seccion, +CE el carril; D = el eje
  lateral de la seccion (0x190028: +24 - +30), y = 0, normalizado, y = -0,5 - rand x 2^-31; la mitad de las veces x / z
  al reves; normalizado; +B0 = 6 D; la estrella sale de reposo - 200 D (llega de un lado y desde arriba). 0x1d4a58: +50 =
  1, +90 = 1,5 (radio del toque), +78 = 0xD6, +94 = 1, +BC = 1, +C0 = 0, +C4 = 1, sprite 2C0007.
- Control 0x1d4d80: +6C + 0,1 por paso; se dibuja en pos + (0, 2, 0) con escala +50 x 1,5; al tocarla (0x1d3720) +7C = 1,
  escala x 0,7 y control 0x1d50e0 (sube con el coche: y = coche + 25 t - 39,2 t^2, desde t > 0,319 la escala - 0,03, giro
  + 20).
- Peticion 0x1b11c8 (461940; puntos 0x3f0810; tambien como llamada [7F2528] fuera de la peticion desde la accion -15):
  cada estrella sin +C0 se activa si las activas (0x589d74) < el tope (0x589d60) y seccion del jugador + 0x28 >= +CC (la
  primera: movimiento 0x220011 de la acompanante); con +BC avanza +B0 por paso hasta el suelo (alli y = reposo + 0,05,
  +BC = 0); despues bota y = reposo + 0,05 + 20 t - 39,2 t^2 (t + 1/60) y al caer suelta 5 trozos de la tabla 0x3f7348
  (0x368630 por el carril: los 7 apuntan a la misma; {desvio, angulos, escala 0,3}) en los objetos 0x134 + base + j
  (base 0x589d6c, a 0 desde 40), cada uno en el suelo de su punto + 0,05, botando igual. Estrella tocada (+7C == 1 y +80
  == 8): rotas + 1 (0x589d66) y movimiento 0x260009; trozo tocado: cogidos + 1 (0x589d68). Resultado = 800000 / n x
  rotas - bono.
- Puesta 0x1b1150 (464350 y la accion -15 = 0x1b0030 con n 0x28 y c 0x32): n = p6, c = p8, rotas = activas = 0, tope =
  45C8E0 de la clase 0x12 en el tramo. Accion -10 (0x1af790): abre los 40 trozos (suceso 0x61); -13 (0x1af990) los
  cierra; -16 (0x1b00b8) quita la llamada.
- Evaluacion 0x1b1778: nota 7 -> 800000 + (rotas >= c ? (rotas - c) x 10000 + 0x46 : (int)(70 / (c - n) x (rotas - n)))
  + cogidos x 10000 x 0,1 (el PC resta; la PS2 suma); si no, el tope de corazones de la nota (0x1b07f0); medidor h x 20 / 7.
- SIMPLIFICADO / PROBABLE: el toque con la caja aproximada del coche (2,3 x 1,1 m + 1,5) de haobj.c; el eje lateral por la
  direccion de la ruta de la demo (ha_dir); la seccion de cada estrella se calcula al vuelo (4 por paso) con ha_s.
  SIN PORTAR (-> HECHO, ver "INFARTO 3"): el sprite 2C0007 (la estela de la estrella al llegar: 0x1d4d80 entre +A4 / pos y +B0, cuadro + 2 por paso,
  efectos 0x216E / 0x10DC del PC) y el 2C0000 de la que sube.
- Poligonos (DESVIACION pedida): la estrella cercana es la del original (20 triangulos / 60 vertices, ya ligera); desde
  25 m x escala (37 m la grande, 11 m los trozos) una plana de 8 triangulos / 20 vertices (las 5 puntas y el pentagono:
  tools/conv_infarto.py `aligera_estrella`); como mucho 24 objetos (los mas cercanos, HA_OBJ_MAX) dentro de 160 m; 70
  estrellas y 40 trozos como mucho.
- E: mision 39 en la clave 13 (FLOR, PRUEBA_HA_MISION=39 PRUEBA_NODO=13, TB_JUEGO=1): "estrellas 100..555: 70 en el
  tramo, n 40 c 50", las estrellas llegan de lado, botan y sueltan 5 trozos (capturas cap/e19a/h_0030, h_0035,
  cap/e19b/h_0040: estrellas en la carretera y subiendo sobre el coche al cogerlas); con PRUEBA_HA_TOQUE=0x12 y
  PRUEBA_SIN_TIEMPO: "29 grandes (n 40, c 50) y 77 trozos", fin de la peticion con nota 1 (el bono del piloto por salirse
  del asfalto se come el resultado; cap/e19c).
- Corregido de paso: tools/conv_infarto.py `aligera_monedas` ordenaba la lista de mallas sin sus vertices (varias
  mallas de HAOBJ.DAT se quedaban con los vertices de otra); ahora van juntas.

**HECHO: los carriles pintados de las misiones** (45ECA0 / 45E9B0 = PS2 0x1a9a48: [0x44350c] == 0 -> 0x1a9850, si no
0x1a95b0). Cada caso de las tablas de saltos 0x3efc60 (preset 0, misiones 15..36) y 0x3efbf0 (preset 1, 15..40, solo las
14 de la lista de 0x1a95d0) pasa a 0x1118b8 uno o dos registros de obj_course_obj_cs_<zona>, dibujados en el marco de la
etapa + (0, 0,1, 0). La conversion los lee del codigo (`sigue_dibujos`: lui / ori de a0, jal y b con su hueco) y no de
las tablas del PC, que no coinciden en varios (PS2: 0x5a0006 / 0x5a0003, 0xfa0005, 0x100000b...). 24 ficheros
HAMpqq.DAT (p = 0 / 1, qq la mision), sin texturas (el color de su material), de 50 a 1199 triangulos.
- DESVIACION (poligonos): las tiras van en trozos de 18 vertices (`trocea`) y la demo dibuja solo los trozos a menos de
  150 m del ojo y no detras (main.c `ha_meta_dibuja`), con el tope de los objetos de la union. Se lee al empezar la
  carrera del modo y en cada etapa (`ha_meta_carga`, el nombre de `infarto_metas` con la seccion META de INFARTOX.DAT).
- SIN PORTAR: las misiones < 15 del preset 0 (0x1a9998: por la etapa de 0x176700, 0x14 -> 0x1020001, 0x19 -> 0xfa0002,
  0x1d -> 0x1020003).
- E: mision 29 del preset 0 en FLOR (1010003 + 1010004): el carril rojo de "Derrapa y circula por el rojo" pintado a la
  izquierda de la carretera (cap/m29/h_0080, h_0085).

**HECHO: las marcas de principio y fin de la accion -5** (463950 caso 14 = PS2 0x1af13c, 45B000 = 0x1a8648, 45B2A0 /
45B200 = 0x1a8840): los modelos por idioma de 0x1a7c80 / 0x1a7cb0 (tablas 0x3684d8 / 0x3684f0 por 0x1f6548; el espanol,
indice 4: 0x570035 "INICIO" y 0x570024), una pancarta de 16 x 2,7 m (2 triangulos), en el centro de la seccion + 4 m
(0x1a868c) con su direccion, + sen(a) en y con a + 0,2094 por paso (0x1a7f78, [0x58a144]). En HAOBJ.DAT (registros de
MARC); la demo las pone con `punto_sec` (main.c `ha_punto`). PROBABLE: la orientacion (con el eje +z del modelo por la
carretera el texto salia al reves en las capturas: va girada 180 grados, cap/arc2/h_0004 antes, cap/arc3/h_0005 despues).
SIN PORTAR: las marcas de la etapa siguiente (0x176e48: 7F24D8 / 7F24DC cuando la seccion cae en otra etapa).

**HECHO: el toque de los postes de los arcos** (0x1d42f0 / 0x1d4434): sin cruce, cada poste (+-h, y + 0,3) con radio
0,3 x escala y efecto 0xCD -> +7C 1 (el de -h) / 2 (el de +h) -> 0x1d4508 con giro -0,2 / +0,2 por paso. Corregido: el
arco cruzado (+7C = 3) no gira (+94 = 0 desde 0x1d41d0; antes giraba 0,2). E: mision 3 en la clave 7 con
PRUEBA_HA_TOQUE=0x0b y la prueba nueva PRUEBA_HA_POSTE (un poste en la linea del coche): "poste 1 del arco 24 tocado
(giro -0.2)" (cap/arc3). SIN PORTAR (-> HECHO, ver "INFARTO 3"): el sprite 2C0002 (0x1d4508 con +94 == 0: 5 destellos alrededor del coche, puntos de
la tabla del PC 681CAC en el marco de car+D28 por la escala (6 - cuadro) x K + 1,5, proyectados, capa 5, cuadro + 1 por
paso hasta el final de la escena): necesita un grupo de HAHUD.DAT del banco 0x2C, la proyeccion en main.c y el dibujo con
escala en hud.c.

**Medidas** (Flycast, -DDIAG_MS -DDIAG_HA, piloto del menu, muestras de 30 cuadros en el mismo tramo de las dos pasadas):
- Estrellas (mision 39, FLOR, s 830..4420, 91 muestras): 39,52 fps, dib 19,55 ms, 13255 tri / 20884 vtx por cuadro;
  sin dibujar los objetos (DIAG_SIN_HAOBJ) 39,63 fps, 18,36 ms, 13128 / 20613: -0,11 fps, +127 triangulos y +271
  vertices de media; los objetos, 23 dibujados de media, 137 triangulos (como mucho 299) y 359 vertices (755).
- Carriles (mision 29, FLOR, s 3700..4824): 39,73 fps frente a 39,69 sin ellos (DIAG_SIN_HAMETA), +85 triangulos de
  media; los carriles: 79 triangulos de media (como mucho 142) y 90 vertices (164).

**RAM:** al arrancar 357 K libres (nav3.sh, "VOZ: VOCES.DAT, 35 muestras", sin "Out of memory"; la punta de partida,
350 K con los mismos datos). El .init del programa va alineado a 8 KB: el codigo solo baja la RAM al pasar de un multiplo
a otro. Para compensar lo nuevo (+5 KB de codigo y +0,6 KB de bss) y bajar un escalon: -Os tambien en fin.o, meta.o,
sprani.o, rival.o, camara.o, voces.o, sfx.o, gd.o, musica.o y hud.o (demo/Makefile) y en lee_modelo_en (main.c): el
programa queda en 702 KB de .text (antes 712). Coste de ese -Os en la carrera (la misma pasada de las estrellas con
todo el -Os nuevo, hud.o incluido): 39,42 fps y dib 19,65 ms frente a 39,52 / 19,55 (-0,10 fps, en el ruido de Flycast;
PENDIENTE medirlo en la consola). Carrera del INFARTO con DIAG_HA: 258 K libres (mision 39) y 264 K (mision 29), sin
"Out of memory" (no cambia con el escalon del arranque).

## COAST 2 COAST: campeonatos, tercera tanda (agente c2c3, 2026-11-27)

Sobre "COAST 2 COAST: los campeonatos (variante 5), agente c2c2". Todo verificado en Flycast (nivel E), no en la consola.

### Holly (grupo 6, Req_Course_MIX_PS2): HECHO

- tools/conv_union.py: las uniones nuevas de los arboles de ARBOLES_REQ (el 302) se numeran desde `BK_REQ = 498`
  (detras de las de CURSOTA.DAT, 262..497) con su propio contador: EAST_R / BK498, LASV_R / BK499, METR_R / BK500,
  DESE_R / BK501, NEWY_R / BK502, ALAS_R / BK503, ALPI_R / BK504, YOSE_R / BK505, GRAN_R / BK506, INDU_R / BK507 y la
  meta CAST_R / BK508 (union 76, cs_BK_SUMOEND); las demas del arbol son de antes (BK42, BK231, BK133, BK95).
- tools/pak_union.py: los BK desde `AL_FINAL = 498` van al final del paquete, asi UNIONES.PAK solo crece (22 750 247 ->
  23 467 226 bytes, +0,7 MB por los trozos repetidos) y sus primeros 22 750 247 bytes y los 262 descriptores de antes salen
  identicos byte a byte (comprobado: recompuestas las 262 de lgdat2 y reempaquetadas con las 11 nuevas, `cmp`).
- tools/regenera.py: el paso `union` convierte ARBOLES + 302 (ARBOLES_CURSO; la Contrarreloj sigue con ARBOLES_TA sin el
  302, CURSOTA.DAT no cambia) y se esperan 509 descriptores (NBK).
- CURSO.DAT: los 96 arboles de antes identicos registro a registro (h2/cmpcurso.py), el 302 detras.
- demo/main.c `curso_tiene`: leia 8 KB del principio de CURSO.DAT, pero el indice de los arboles va detras de los 15
  registros del modo OutRun (12 + 15 x 684 = 10272 bytes): nunca encontraba el 302 ("el campeonato 2 no se puede
  correr"); ahora lee 16 KB y comprueba el arbol contra el tamano del fichero.
- GD-ROM: datos de prueba 476 947 sectores (931,5 MiB; lgdat2 476 393): quedan 27 171 sectores (53 MiB) menos el
  1ST_READ.BIN (~3 MB).
- E (`-DPRUEBA_AUTO -DPRUEBA_CAMP=2 -DPRUEBA_SIN_TIEMPO`, pantalla :93): `CURSO: arbol 302, 15 etapas, salida EAST`, las
  cinco etapas EAST_R -> LASV_R -> DESE_R -> ALPI_R -> BEAC_R por BK498, BK499, BK501, BK504 (leidas del paquete en ~2,1
  s) hasta `CURSO: GOAL en BEAC`, con las peticiones de sus guiones (16, 13, 14, 22: sin portar, nota 0). Sin "Out of
  memory"; "VOZ: VOCES.DAT, 35 muestras". Capturas: modos/c2c/cap3/holly (r_*: una cada 15 s; r_225 en la union).

### Merge de infarto2 y RAM del arranque

- `git merge infarto2` (comun/haobj.c: los objetos de las peticiones del INFARTO, con sus modelos de HAOBJ.DAT y
  HAOSO.DAT). Conflictos resueltos: demo/Makefile (la union de las dos listas de -Os, SIN camara.o, hud.o ni sfx.o, que
  corren en cada cuadro; sfx.o vuelve a -O2), demo/main.c (los enganches de los dos modos), docs (las dos secciones).
- Para la prueba, los datos de modos/infarto/dat copiados a modos/c2c/dat_inf (HAOBJ.DAT, HAOSO.DAT, INFARTOX.DAT,
  HAHUD.DAT, CHRH.DAT, VOCESH.DAT, HAMnnn.DAT) y enlazados en datos2 (h3/datos2.sh).
- RAM: con el merge el arranque bajaba a 345K. Se recupera con el FRIO de infarto3 (efe7f03, cherry-pick: infarto.o
  con sus tablas al monton y `FRIO` = -Os en la carga de main.c), FRIO tambien en tapas_busca, rk_un_jugador,
  curso_huecos, focos_carga, robot_carga, hueco_fin, final24_chr, camp_preparado y curso_tiene (solo corren al cargar
  o al acabar), y el bufer del fichero del fantasma de la CONTRARRELOJ (gh_buf, 28 KB de BSS) al monton solo mientras
  se lee o se guarda en la VMU. nav3.sh (MODO_JUEGO=1, sin defines): "RAMMAPA arranque: libre 381K", "VOZ: VOCES.DAT, 35
  muestras", sin "Out of memory". Leer el fantasma comprobado en la CONTRARRELOJ (MODO_JUEGO=3: "FANTASMA: curso 013C,
  sin fantasma"); guardarlo en la meta (malloc en gh_hilo) NO se ha visto: PROBABLE.

### Tipos 6 (conos), 7 (monedas) y 8 (arcos): HECHO

- Objetos (un sistema propio de C2C, tabla 0x61e8a0 de 250 x 0x2c, distinto del del INFARTO pero con los mismos
  objetos del banco 0x57): cada tipo lee de sus parametros una lista de filas (0x293968: {cuantos, clase, +8, relativa,
  seccion inicial, final, lateral inicial, final}: cuantos repartidos sin llegar a la final) y una de puntos (0x2938d0,
  0x2c bytes); campos por tipo: 6 +0x74 / +0x78, 7 y 8 +0x78 / +0x7c (15 +0x70 / +0x74 y 19 +0x78 / +0x7c tambien se
  convierten). La clase (0..14) da el control (0x297e20: 0..1 conos, 2..3 moneda / corazon, 4 arco...) y la clase del
  banco (tabla 0x423080: 6 / 5 conos rojo / azul, 0x0a moneda, 0x39 corazon, 0x0b arco).
- tools/conv_camp.py: CAMP.DAT lleva detras de las peticiones 'OBJ1' con 335 filas (28 884 bytes en total).
- comun/campeonato.c `pon_objetos` en la fase 1 (el anuncio, como el original) y comun/haobj.c `haobj_pon` (nuevo):
  el objeto ya en el mundo; demo/main.c `camp_obj_`: la seccion -> s de la ruta y el lateral como fraccion del asfalto
  (SUPUESTO: 0 es el borde izquierdo; con el piloto los conos azules quedan en su linea), a la altura de la calzada y
  girado con la carretera. Los toques y los cruces de arcos de haobj_paso llegan a cada tipo en camp_coche_t.
- 6 (0x295a28 / 0x295d48): azul +80 / (azules x 0,99) al medidor (derrapando si +0x70 == 1), rojo 3 veces eso a la
  penalizacion; nota 6: +70 x 0x22f090. 7 (0x2953b8 / 0x295740): moneda o corazon +80 / p70 (derrapando si +0x74 == 1);
  nota 6: 70 x cogidas / todas + (cogidas + corazones - p70) / 2. 8 (0x29a330 / 0x29a670): arco cruzado +80 / (p70 x
  0,99); nota 6: +70 x 0x22f090. SIMPLIFICADO: la voz de animo (0x2a6240) por cuenta de toques y velocidad, no con el
  contador exacto 0x3b5528.
- E (Jennifer, `-DPRUEBA_CAMP=0 -DPRUEBA_CAMP_NODO=7`, Cloudy Highland): lista 1 (8, 7, 8): 15 arcos cruzados solo con
  el piloto, "peticion 8 hecha: 93 - 0, nota 6, 80 + 119"; monedas 28 - 3, nota 2 (con `-DPRUEBA_HA_TOQUE=-1`, que pone
  los objetos en su linea: 76 de 75 + corazones, nota 5); lista 2 (6, 6, 6): conos azules tocados por el piloto 8 / 31
  / 40, notas 0, 3, 3; la cara de la chica y el contador de corazones suben. Capturas cap3/obj_toque (arcos, monedas),
  cap3/conos.
- Poligonos y fps (los modelos ya aligerados del INFARTO; como mucho 24 objetos a menos de 160 m; DIAG_MS, las mismas
  pasadas con y sin dibujarlos `DIAG_SIN_HAOBJ`, cap3/medidas): conos 94 triangulos de media (190 como mucho), 39,74
  frente a 39,78 fps (-0,04); arcos y monedas 156 de media (464 como mucho), 37,87 frente a 38,03 (-0,16).

### Reacciones de la acompanante (0x205288 / 0x2052f8 / 0x2a66a8): HECHO

- Tipos 1 y 2 (0x29c828 / 0x298360): al empezar a cumplir (pasar de la velocidad, derrapar) 0x205288(1, fila) con la
  fila por la nota (switch 0x4236f0 / 0x4230f0: 0..1 -> 0, 2..3 -> 5, 4 -> 3, 5..6 -> 4; aqui la nota de ese momento
  sacada del medidor, PROBABLE: el original lee +0x24); al dejar de cumplir 0x2052f8 (para).
- Fase 6 (la nota, 0x232de0 -> 0x2a66a8): para y el movimiento de la tabla 0x423e60 por la nota, o 0x423e80 con los
  modelos de coche que elige el switch 0x423ec0 (por modelo - 2). tools/conv_camp.py: bloque 'CHI1' detras de 'OBJ1'
  (las dos tablas y la eleccion por modelo). Los movimientos (0x60048, 0x260009, 0x6003f, 0x260003, 0x26000d) ya estan
  en CHRH.DAT del INFARTO (comprobado: conv_chr.py con ellos da el mismo fichero byte a byte).
- comun/robot.c robot_pide (el del INFARTO) desde demo/main.c `camp_chica_`; CHRH.DAT (~52 KB del monton) se lee al
  empezar cada etapa del campeonato y se suelta al acabarlo.
- La pareja no salia en las pruebas `-DPRUEBA_CAMP` ("CHR: sin pareja (-3 ...)": robot_pareja leia el campeonato del
  menu); ahora lo lee del elegido en camp_preparado (camp_g; con el menu es el mismo).
- E (Jennifer, `-DPRUEBA_CAMP=0 -DPRUEBA_CAMP_PASA -DDIAG_HA`): rebufo nota 6 -> "DIAGHA acompanante: movimiento
  0x26000d (esta)", velocidad: "cara 0 (0x26000a, esta)" y nota 0 -> 0x60048; capturas cap3/chica (la acompanante se
  mueve en el coche tras la nota). En carrera 303K libres con la pareja y CHRH.DAT, sin "Out of memory".

### Rotulos del banco 0x46 (tipos 1, 12 y los demas): HECHO

- tools/conv_camp.py hace CAMPHUD.DAT (196 KB, 19 texturas VQ de ~64 KB sin VQ cada una): los 19 rotulos del anuncio
  del banco SPRANI 0x46 que piden las peticiones (460040 rebufo, 460041 velocidad, 460042 colores, 46002C, 46003E...
  46005C), cada uno su escena entera (conv_hud.grupo_escena) en el grupo CA_Tkk; la peticion lleva el rotulo 0x40 + k.
  Ya solo el tipo 20 se anuncia con el generico (no tiene rotulo propio en 0x22faec).
- demo/main.c: CAMPHUD.DAT se abre con HAHUD.DAT en la carrera del campeonato (solo sus tablas; la textura de un
  rotulo se sube mientras se ve y se baja despues, `hud_rotulo_uso`, como los HA_Tnn).
- E (Jennifer, PALM): "¡Ve al rebufo de los coches!" con su dibujo (tipo 12) y "¡Excede la velocidad!" con el
  velocimetro (tipo 1), cap3/rotulos (r_033, r_063, la ampliacion c2c3_rt_z.png).
- SIN PORTAR: el letrero 3D de inicio / fin de la peticion (0x232e60 con 0x1a7c80 / 0x1a7cb0). -> HECHO (ver "sexta tanda").

### Medidas finales y lo que queda

- RAM al arrancar (nav3.sh, MODO_JUEGO=1, sin defines, datos de prueba datos2): "RAMMAPA arranque: libre 373K" (4106ba2:
  364K), "VOZ: VOCES.DAT, 35 muestras", sin "Out of memory" (cap3/log_nav3.txt). En la carrera del campeonato con la
  pareja, CHRH.DAT y los objetos: ~300K libres.
- Por el menu real (`-DPRUEBA_PILOTO_MENU -DPRUEBA_SIN_TIEMPO`, h3/camp.sh, Clarissa): mapa -> coche -> BEAC con la
  peticion 11 anunciada con su rotulo del banco 0x46 y la reaccion de la acompanante a la nota; cap3/menu.
- SIN PORTAR (por orden de lo que piden los guiones): 16 los minijuegos (104 peticiones, casi todas de Holly; clase
  0x29f520 de 0x1860 bytes con 8 subtipos), 13 alimentar a la chica, 22 la bocina, 11 chocar con los coches de la
  peticion (necesita los coches propios 0x22d088 / 0x1e1910), 5 colores (las franjas pintadas de +0x58 de los
  parametros, 0x1f3a70, y el lateral 0x232768), 15 la pelota y 19 los meteoritos (sus objetos ya estan en CAMP.DAT
  OBJ1, falta su control: 0x293190 / 0x29da88 y los objetos de tipos 0xb / 0xd / 0xe que haobj no mueve), 14, 20, 21,
  23, 24; el letrero 3D de inicio / fin (0x232e60) y la silueta 2C0067 / 0x460064. -> HECHO: 15, 19 y 5 (ver "cuarta
  tanda"); 11 subtipos 0 / 1 (ver "quinta tanda").
## INFARTO 3: tipos 11, 9, 14 y 20, sprites 3D de los objetos y RAM del arranque (Rev 1.1, 2026-11-27)

Sobre "INFARTO 2" (rama infarto2, f0a330a). Todo en Flycast (nivel E), no en la consola. Datos nuevos o cambiados
(tools/conv_infarto.py): INFARTOX.DAT con ANI1, ESC1, ESC2, ARC2 y FAN1; HAOBJ.DAT con el corazon 0x2a y su version
plana; HAHUD.DAT con HA_RK0 / HA_RK1 (2B0005 / 2B0006) y HA_S06 / HA_S02 / HA_S07 / HA_S00 (2C0006 / 2 / 7 / 0, todos sus
cuadros). Codigo: comun/haobj.c (anillo, pares, sprites 3D de los objetos), comun/infarto.c (tipos, `infarto_autos`,
`infarto_spr3d`), demo/main.c (enganches: `traf_infarto` pasa las plazas del trafico a `infarto_autos`, `ha_spr3d`
dibuja los sprites 3D, la clase del coche de la mision en `ha`, la marca `ha_m` de la plaza). Pruebas con
PRUEBA_HA_MISION / PRUEBA_NODO / TB_JUEGO=1 (preset 0) como antes; scripts en $S/modos/infarto/c_*.sh.

**HECHO: tipo 11, "Atrapa el corazon"** (10 peticiones: mision 4 del preset 0 y 28, 29, 33, 34, 38, 39 del preset 1).
- Puesta 464350 = PS2 0x1ae854: 0x1aa358 (+4 / +6 tramo, +8 = p6, +C = p8, +A = 0) y 0x1df060 (el anillo vacio).
- El anillo 0x5d51f8 (PC 8037C8, 32 registros de 0x5C; 46DB60 = PS2 0x1dcaf8, el registro libre 0x1dca70 desde el cursor
  0x5d5d78): mientras la peticion es la 11, cada coche vivo sin +C5C 0x20000 recibe un registro con los dos puntos de
  0x40b9a0 (INFARTOX.DAT ANI1: (+-2,6, 1, 0) en el marco del coche) y la marca; cada paso los puntos al mundo y la edad
  +58 + 1; sin coche o sin puntos, fuera. Aqui la marca es `ha_m` de la plaza (un numero de serie: una plaza reusada no
  hereda el registro).
- Control 0x1aa3e8 (= PC 45F180): puntos de 45E2F0 (0x3efce8); cada punto a dx^2 + dz^2 <= 16 del jugador se coge
  (+4C = 0), movimiento 0x260003 de la acompanante (salvo los coches "callados" de 45B380), +A + 1 y el efecto 0x52 si
  pasa de p6; resultado = +A > p6 ? 800000 : +A / p6 x 800000.
- Evaluacion 0x1aa780 (y la del 9, 0x1aa1e0, la misma cuenta): nota 7 -> 80 corazones + 400000 si no hubo sucesos ni
  trompos (7F24EC / 7F2504) + (+A > p6: +A >= p8 ? 300000 + (+A - p8) x 50000 : (+A - p6) / (p8 - p6) x 300000);
  contador (h + h / 2) x 10 / 7.
- Dibujo 0x1aa5b8 (solo con la peticion 11): el modelo 0x57002A (en la PS2; el PC usa 0x2F) en cada punto con y +
  (sen(a) x 0,5 + 1), a = edad x 0x444 (2 pi / 65536), girando a sobre la vertical y con escala (1 / s, 1, 1 / s).
- DESVIACION (poligonos): el corazon es de 44 triangulos en 6 tiras; desde 30 m una version plana con su silueta
  (tools/conv_infarto.py `aligera_corazon`: 10 triangulos / 14 vertices, el color medio) y solo hasta 100 m
  (HAOBJ_ANI_LEJOS), con el tope de HA_OBJ_MAX objetos (los mas cercanos).
- E: mision 28 del preset 1 (PRUEBA_HA_MISION=28): corazones a los dos lados de los coches (cap/t11b/h_0022, h_0024), 3
  cogidos de 6, nota 3, +20 corazones.
- Medido (Flycast, DIAG_MS, el mismo tramo s 580..2250, 31 / 30 muestras): 38,71 fps, dib 18,87 ms, 16204 tri con los
  corazones; sin dibujar objetos (DIAG_SIN_HAOBJ) 38,67 fps, 18,78 ms, 16055 tri: +149 triangulos de media, +0,09 ms,
  sin perdida de fps medible; corazones dibujados: 2,6 de media, como mucho 152 triangulos.

**HECHO: tipo 9, "Cruza la linea"** (4 peticiones: 17, 20, 31 del preset 0 y 24 del preset 1).
- Pares 0x5d4fc0 (PC 800AF8, 10 x 0x38; 46D250 = PS2 0x1dc3c0): los abiertos se sueltan si falta un coche, se separan
  mas de 80 m o la peticion ya no es la 9; luego, por la cadena del jugador (+314: aqui las plazas por distancia, < 250
  m), cada coche del tramo (+4..+6 de la peticion) sin +C5C 0x18000 se empareja con el vivo mas cercano a menos de 80 m
  del tramo y sin 0x10000 (los dos: +C5C 0x8000). Las clases y puntuaciones del par (+30..+36) solo las usa el dibujo
  del PC: en la PS2 el dibujo de la peticion 9 (0x1aa1d8) es `jr ra`, sin nada.
- Control 0x1a9f68: puntos (0x3efcd0); la linea entre los dos coches (+ 1 m en y) en el marco del jugador contra su caja
  (0x1978d8 con +338; aqui 2,3 x 1,1 m en xz, PROBABLE): el par se suelta (0x1def90: +C5C 0x10000), movimiento (coche
  9: 0x6003D; si no rand < 0,5 ? 0x6003F : 0x6003E), +A + 1 (efecto 0x52 si pasa de p6) y el resultado del 11.
- SIN PORTAR: el registro de aparicion que la puesta 0x1a9e08 instala con 0x1f20e8 (como el del 22).
- E: mision 24 del preset 1: pares formados en el tramo 268..530 (registro DIAGHA "par n: plazas a y b"), 3 lineas
  cruzadas, nota 1 (cap/t9a; otra vez cap/t9b tras corregir c_act).

**HECHO: tipo 14, "Supera a ese coche"** (3 peticiones: 19, 22, 25 del preset 0).
- Accion -12 (0x1ab3c8 = 45B5B0): hasta 8 escoltas de la tabla 0x368528 + w4 x 0x78 (INFARTOX.DAT ESC2: {seccion desde
  w6, carril, modelo, +8, +B74}), clase 6 (+324), +C5C 0x40000, +B58 = +B5C = 0x12, parados. Aqui se abren por la misma
  via que los coches de la mision (`infarto_coche_pide` con la clase 6). DESVIACION: la tabla pide coches del jugador
  (modelos 2, 5, 8: sin cargar en el modo); van modelos del trafico de la lista de las clases 3 / 4.
- Accion -18 y la puesta (0x1ab1d0 = 45B520): los escoltas a la velocidad del jugador, como poco 0x3fb12fea (1,384 m por
  cuadro, 300 km/h), y 7F250C = 1. PROBABLE: despues su IA de carreras (sin portar) -> aqui vuelven a la de la
  carretera (v_des), y mientras dura la peticion no se cierran por detras (main.c, la regla de los coches de carreras de
  477F60 sin el -15): sin eso el jugador los adelantaba, se cerraban y contaban como "por delante" (+98).
- Puesta 0x1ab2d8: la lista de los escoltas vivos (+40..), puesto = n + 1. Control 0x1ab6a0 (puntos 0x3efdc0): puesto =
  los que faltan (+98) + los que van por delante (+260: aqui la seccion) + 1; si sube, el efecto 0x5C y se guarda 15
  pasos (+B6); resultado = 0x3f7328[puesto - 1] (ESC1: 800000, 600000, 450000, 300000, 200000, 160000, 120000, 0);
  acompanante: al subir 0x220000 / 0x220003 (rand), si no y no va primero 0x220011 (con 487DF0 = `robot_fin(1)`, nuevo
  en robot.c). Evaluacion 0x1abb30: nota 7 -> 80 + (sin sucesos 200000) + (60 - trompos) / 60 x 200000 + (60 - ruedas
  fuera) / 60 x 300000. Dibujo 0x1abca0: 2B0005 (cuadro 0) y 2B0006 (cuadro puesto - 1) en (0, 30), capa 1.
- E: mision 19 del preset 0 (TB_JUEGO=1): "7 escoltas de la tabla 0", el puesto en pantalla de 8/8 a 1/8
  (cap/t14g/h_0042..h_0057), fin con el puesto 1, nota 6, +231 corazones. PROBABLE: la posicion del rotulo del puesto
  (sale encima del bocadillo de la peticion).
- Corregido: `c_act` (el coche del jugador para las acciones) apuntaba a la pila de `infarto_paso`; ahora es una copia.

**HECHO: tipo 20, "Choca con el fantasma"** (1 peticion: 16 del preset 0).
- La tabla de coches de la -14 con w4 & 0x40 (0x1b2e04: [+E0] & 0x40) abre fantasmas (+C5C 0x80000; aqui la marca
  HA_FANT de la plaza).
- Puesta 0x1b1e10 (tramo, p6, p8; +B66 = 0). Control 0x1b1f18 (puntos 0x3f0828): cada fantasma a 7 secciones por
  delante: un efecto de 0x3f0840 (FAN1: 0xD1..0xD3, rand % 3) cada 60 pasos; golpeado (+B6C y +2F4 == 0): +10 + 1,
  resultado +10 / p6 x 800000; acompanante 0x260007 / 0x60025 / 0x250003. PROBABLE: aqui un golpe por coche (el
  original cuenta los pasos con esa condicion). Evaluacion 0x1b2348: nota 7 -> 80 + (+10 > p6: +10 < p8 ? (+10 - p6) x
  700000 / (p8 - p6) : 700000 + (+10 - p8) x 50000); contador h x 20 / 7.
- Dibujo 0x1b2190: el sprite 3D 2C0006 (4295D0 = PS2 0x115870) en +14 de cada coche vivo sin golpe a menos de 10
  secciones, cuadro +B66 (+ 1 por cuadro, tope el ultimo), escala 1,68 hasta el cuadro 25 y luego - 0,2 por cuadro.
  main.c `ha_spr3d`: el punto con la camara de la demo, px = escala x foco / z / 32 (4295D0: escala x a1 / -z / 32),
  hudd_escala + hudd_dibuja encima de la escena (SIN el sesgo de profundidad). SIN PORTAR: como se ve el coche fantasma
  (si el original lo dibuja distinto por 0x80000).
- E: mision 16 del preset 0: fantasmas abiertos, 3 golpeados, nota 1; el sprite sobre el coche (cap/t20a/h_0024).

**HECHO: los sprites 3D de los objetos** (haobj.c `haobj_spr3d`, por `infarto_spr3d`):
- 2C0002 del arco cruzado (0x1d4508 con +94 == 0): 5 destellos en los puntos de 0x36b9b0 (ARC2: un semicirculo de radio
  3 en (x, y) del coche) x (cuadro < 7 ? (6 - cuadro) x 0,1 + 1,5 : 1,5), 3 m por delante del coche, escala 0,7, cuadro
  +A8 + 1 por paso hasta el ultimo. E: cap/t5a/h_0012 (LASV, mision 3, PRUEBA_HA_TOQUE=0x0b).
- 2C0007 de la estrella que llega (0x1d4d80): en su punto (volando) o en su reposo, escala 3, cuadro +98 + 2 por paso
  hasta el ultimo. SIN el giro por la direccion del vuelo en la pantalla (0x2bb0a0) ni los efectos 0x216E / 0x10DC.
- 2C0000 de la que sube (0x1d50e0): sprite 2D (429530, sin escala) en el punto proyectado, cuadro + 1 por paso.
  E: cap/t5b/h_0050 (FLOR, mision 39, PRUEBA_HA_TOQUE=0x12).
- Coste: quads 2D (5 por arco, 1 por estrella o coche): sin medida aparte (dentro del ruido de las de arriba).

**RAM** (nav3.sh, build sin defines): al arrancar 355 K libres, "VOZ: VOCES.DAT, 35 muestras", sin "Out of memory".
- hud.o, sfx.o y camara.o otra vez a -O2 (corren en cada cuadro de la carrera): +7,7 KB de codigo.
- Para compensarlo y lo nuevo (~5 KB de codigo): las tablas grandes de infarto.c (la tabla de coches de 4 KB, los
  niveles, los sprites del medidor y sus efectos: ~9 KB) en el monton solo mientras dura el modo (`hm`, infarto_inicia
  / infarto_libera); el mapa de 8 KB de `pal_colores` (hud.c) en el monton solo dentro de `pal_reparte` (la carga del
  HUD); y FRIO (`__attribute__((optimize("Os")))`, demo/main.c) en las funciones que solo corren al cargar:
  decor_reduce, curso_reinicia, etapa_precalienta, efectos_inicia, ruta_anade, curso_lee_id, coche_carga, camion_carga,
  faros_carga, rc_modelos_carga, final24_entra, bo_carga_datos, traf_tablas, camion_lee, final24_dibuja, tabla_entra,
  band_carga, refl_fijo_lee, rc_abre, viso_lee, final24_chr, robot_carga, focos_carga, curso_huecos, hueco_fin,
  tapas_busca, rk_un_jugador, fisica_carga, coche_ani_lee, ruta_prepara, vram_ordena, compila_cab, camion_suelta_ram,
  coche_cab_cex, salida_calcula, ta_meta_busca, faros_rejilla, coche_cex. El .init va alineado a 8 KB y el codigo queda
  0x450 bytes por encima de un escalon: lo que el siguiente agente quite de codigo hasta ahi vale 8 KB del arranque.
- Carrera del INFARTO (DIAG_HA, mision 19): 259 K libres al empezar, sin "Out of memory" ni "sin VRAM". Los grupos
  nuevos de HAHUD.DAT se suben a la VRAM solo mientras se ven.

**SIN PORTAR (PENDIENTE):** la IA de carreras de los escoltas del 14 (+B58 = 0x12) y sus modelos de coche del jugador; el
registro de aparicion de la puesta del 9 (0x1f20e8) y del 22; la accion -19 (0x1abd10: [0x589d04] = w4, sin leer quien lo
usa); el aspecto del coche fantasma; el giro de la estela 2C0007; los tipos 17, 18 y 21 (no los pide ninguna mision).

## COAST 2 COAST: campeonatos, cuarta tanda (agente c2c4, 2026-11-27)

Sobre "campeonatos, tercera tanda (agente c2c3)" con `git merge infarto3` (0375cc8). Todo en Flycast (nivel E), no en la
consola. Datos de prueba: $S/modos/c2c/dat2 (CAMP.DAT, CAMPOBJ.DAT nuevos) y dat_inf (actualizado con modos/infarto/dat:
HAHUD.DAT e INFARTOX.DAT de infarto3), carpeta montada con h4/datos2.sh; scripts en $S/modos/c2c/h4 (mk.sh, gdi.sh,
auto.sh con INI=, medir.sh / mide.py para los fps con y sin dibujo, donde.py: en que grupo / nodo / lista sale un tipo).

### Merge de infarto3
- Conflictos: comun/haobj.h (las dos API), demo/main.c (hud_rotulo_uso de C2C y ha_spr3d del INFARTO), demo/Makefile (la
  lista de -Os de c2c3, que ya era la de infarto3 mas mision.o y campeonato.o; hud, sfx y camara a -O2), docs. infarto.c
  queda igual que en infarto3. Arranque: 379K.

### HECHO: tipo 15, empujar la pelota (0x293190 / 0x2934e8)
- La pelota es la clase 0x11 del banco de objetos = registro 1 de obj_tour_pmt (recurso 0x1ee, tabla 0x40a958; no es del
  banco 0x57, por eso faltaba en HAOSO.DAT). tools/conv_camp.py: CAMP.DAT 'CLS1' (clase -> registro de CAMPOBJ.DAT y
  escala) y CAMPOBJ.DAT (ORD7 como HAOBJ.DAT: la pelota 150 triangulos, el meteorito 159; 27 KB, se lee en la carrera del
  campeonato). haobj_clase / HAOBJ_TOUR: haobj dibuja esos registros con el modelo de CAMPOBJ.DAT.
- Control: fase 1 los objetos (0x293390 / 0x293448), +0xc = 80 / (largo x 0,85); fases 2..5 (0x2934e8) por cada pelota:
  si se ha quedado 5 secciones atras y el jugador va antes de la mitad - 10, otra en la mitad (lateral 0,5, una vez); con
  +3FC == 8 (tocada) el medidor suma lo que ha avanzado desde el toque anterior x +0xc (PROBABLE: 4 si no avanzo; el
  original suma 4 con +3F8, que pone 0x1d1af0 / 0x1d2608 con el modelo 0x1ee0001); en la 4, pasado el final, lo ultimo
  y hecha; nota 6: + 70 x 0x22f090. Efectos 0x422cd0[rand % 7] (CAMP.DAT 'SND1').
- SIMPLIFICADO (DESVIACION): sin el cuerpo rigido 516E10 del original (control 2, el de los conos): la pelota (radio 1 x
  escala 2, apoyada en el suelo) sale con la velocidad del coche + 5 m/s, rueda con 5 m/s2 de frenada y gira al rodar.
- E (Clarissa nodo 8 lista 1, `-DPRUEBA_HA_TOQUE=0x11`): 5 toques (secciones 104..136), "peticion 15 hecha: 92 - 0, nota
  6"; cap4/t15/pelota.png. fps (DIAG_MS, con / sin dibujar objetos): 35,72 / 35,74; 61 triangulos de media, 150 como mucho.

### HECHO: tipo 19, los meteoritos (0x29da88 / 0x29e018)
- Clase 0x3d = registro 4 de obj_tour_pmt (control 0x0d, objeto de 10 m de radio x escala). Fase 1: objetos y la voz 0x7f;
  fases 2..4 (0x29e018): caen (+C0 / +BC / +B0, como la estrella del INFARTO: desde 200 m, 6 m por paso; empiezan a 40
  secciones por delante, PROBABLE: el original mira [0x55cf38] & 3 == 2) y al llegar al suelo el efecto 0x423830; golpe
  (+7C y +80 == 8, una vez por meteorito, +D0): penalizacion + p[5] (+0x74, flotante: 14 / 15 / 16...) y, sin choque, la
  reaccion 0x182d38 con la velocidad x 0,9 (camp_golpe: solo la velocidad); en la 4 el medidor con el avance / (largo x
  0,9); la 5 no da bono. SIN PORTAR: 0x22df58 (vibracion), el efecto 0x22c6c8 de la llegada y el HUD de 0x22e9a0.
- E (Clarissa nodo 6 lista 1, `-DPRUEBA_HA_TOQUE=0x3d`): 10 golpes, penalizacion 100, nota 0 (cap4/t19/meteoritos.png);
  sin forzar, 39,81 / 39,78 fps con / sin dibujarlos, 159 triangulos de media (318 como mucho).

### HECHO: tipo 5, ir por la zona de color (0x2a3db0)
- La zona NO esta en el guion (+0x10..+0x1c a cero; 'ZON1' de CAMP.DAT lo comprueba): la pone 0x2323e8 por el subtipo:
  0 -> lateral 0..0,5 (color 2), 1 -> 0,5..1 (3), 2 -> 0,25..0,75 (4); 0x232768 la mira (fraccion del ancho de borde a
  borde) entre el inicio y el final. Control: dentro (y derrapando si +0x78 == 1) el medidor con el avance / (largo x
  0,9), animo por la velocidad; nota 6: el bono de 0x22f090 con 0,9.
- 0x1f3a70 (los +0x58 de los parametros, en 0x2937f0 para todos los tipos) NO pinta franjas: copia una tabla de 12 bytes
  por seccion a 0x5d5da8, la de la aparicion del trafico (0x1f3818). No esta portado (el trafico de la peticion).
- DESVIACION: como lo marca el original sobre la carretera no esta averiguado; aqui una tira translucida del color del
  rotulo (rojo, azul, verde) de 2 a 150 m por delante, <= 50 triangulos (camp_zona_dibuja). El lateral: 0 el borde
  derecho (PROBABLE, el dibujo del rotulo "Circula por el azul" lleva el azul a la derecha; los objetos de c2c3 usan el
  lateral al reves, SUPUESTO alli).
- E (Jennifer nodo 5 lista 1: 5/1, 5/0, 5/2): notas 2, 5 y 6 con el piloto; cap4/t5_azul.png. fps 39,16 / 39,17 con / sin.

### HECHO: el fantasma de la CONTRARRELOJ
- gh_rec / gh_rep (2 x 28,8 KB de BSS) al monton solo en la CONTRARRELOJ (gh_empieza: malloc de 57,6 KB al empezar la
  carrera con un codigo de curso; gh_suelta al volver al menu o al empezar otro modo; si gh_hilo esta guardando en la VMU,
  al acabar el hilo; gh_gen + 1 en cada suelta). El fichero de la VMU (GH_BUF = 12 + 28,8 KB) va al monton solo mientras
  se lee (gh_empieza) o se guarda (gh_hilo), como en c2c3.
- E (`-DPRUEBA_VARIANTE=7 -DPRUEBA_TA_CODIGO=0`): GOAL en PALM -> "guardado ORGH0100 en a1 (5948 bytes, 12 bloques) ...
  releido igual"; con la misma VMU otro arranque: "FANTASMA: curso 0100, leido de la VMU (398 muestras, 52890 ms)";
  697K libres en la carrera. cap4/fantasma/log_vmu.txt.

### RAM
- Arranque (nav3.sh, MODO_JUEGO=1, sin defines): 426K libres (379K tras el merge; 370K con 15 / 19 antes del fantasma),
  "VOZ: VOCES.DAT, 35 muestras", sin "Out of memory". FRIO en camp_*, c2c_paso y carga_hilo. Ningun modulo de cada cuadro
  a -Os.

### SIN PORTAR (lo averiguado)
- 16, minijuegos (clase 0x29f520, 0x1860 bytes): fase 1 0x2a0d08 reparte los registros de 0x2c del guion de +0x7c
  ({+0x20 clase 0 aviso / 1 puerta, +0x24 simbolo o carriles, +0x26 / +0x28 secciones, +0x2a cuantos}, +0x80: relativo
  al inicio) en la lista A (+0xc, +0x70 cuantos) y B (+0x8a4, +0x72), y el subtipo (tabla 0x4239f0) los prepara; cada paso
  (0x423a10) por el subtipo. Subtipo 2 (el mas usado, 22): avisos con la voz 0x423c30[simbolo] (0x1a7..0x1ad) y puertas de
  n carriles con un simbolo cada uno (el bueno al azar, 0x2a1ba0); al pasar la seccion de la puerta 0x29fb28 mira el carril
  (fraccion de borde a borde, k / n < f < (k + 1) / n): bien, + 88 / puertas y el destello 2C0002; mal, efecto 0xd4. El
  dibujo (0x2a0138) son sprites 3D (0x115870, como ha_spr3d) del banco 0x46: tablas 0x423ae0 (subtipo 0), 0x423b10 (1),
  0x423b68 (2), 0x423af8...; hay que convertirlos a CAMPHUD.DAT. Las voces de los minijuegos y de casi todos los tipos
  estan en los bancos HAM_MIS\*.PAK (tabla 0x352840, entradas 54..77: SOUND*, MEMORY, MATH, CARS, FOOD*, GHOST, DUCK,
  GATES, CONEHIT...), que no estan en VOCESH.DAT (solo HC.PAK, PERH.pak y los HAM de etapa): sin ellas las voces de los
  tipos del campeonato no suenan.
- 11 (0x294a90), 9, 22: los coches de la peticion salen del registro de aparicion 0x1f20e8 con la tabla de 0x46 bytes de
  +0x10 de los parametros (0x1f1a88) y 0x22dac0; el mismo que el INFARTO dejo sin portar (9 / 22). 13, 14, 20, 21, 23, 24:
  sin empezar. El letrero 3D (0x232e60) y el rotulo del 20: sin empezar. -> HECHO 20, 21, el letrero y el rotulo del 20; 14
  PARCIAL; 23 no lo pide ningun guion (ver "sexta tanda").

## Integracion (rama integra): SP + musica + INFARTO 3 + campeonatos c2c4 (2026-11-27)

- Fantasma de la CONTRARRELOJ: c2c4 quito el gh_buf fijo (la VMU se lee y se guarda en un malloc temporal) y el coche
  de la grabacion de los modos "sp" guardaba alli su GHO1. Ahora el GHO1 va a `gh_gb` (memalign de 16 KB recortado a
  20 B por triangulo con realloc), que suelta `gh_suelta_modelo()`: al cambiar de ficha, al soltar la grabacion
  (`gh_suelta`, fuera de la CONTRARRELOJ) y cuando el fantasma pasa a ser el coche del jugador (`gh_meta`).
- Voces: variante 2 (INFARTO) con el banco de la etapa (`infarto_voces()`, modos "sp"); variante 5 (campeonato)
  con VOCESH.DAT tal cual, hasta que haya bancos HAM_MIS propios.
- Verificado en Flycast con los datos juntos (lgdat2 + modos infarto, sp, musica, c2c dat/dat2; VOCESH.DAT el de sp):
  nav3.sh (MODO_JUEGO por defecto) "VOZ: VOCES.DAT, 35 muestras", "RAMMAPA arranque: libre 425K", sin "Out of memory",
  carrera de COAST 2 COAST; fantasma: ficha 0 graba (`guardado ORGH0100, 407 muestras, 54102 ms`), segundo arranque
  con la misma VMU y la ficha 5: `coche de la grabacion ficha 0: GHO1 de COCHE00L.DAT, 182 triangulos`, 64..73
  triangulos por cuadro, la forma gris delante del F430; RAM en carrera 688K. Datos 940,9 MB, 43,7 MB libres en el GD
  clasico (el Mod sin medir).
## COAST 2 COAST: campeonatos, quinta tanda (agente c2c5, 2026-11-27)

Sobre "campeonatos, cuarta tanda (agente c2c4)" (rama c2c4, b7958da). Todo en Flycast (nivel E), no en la consola.
Datos de prueba: $S/modos/c2c/dat3 (VOCESM.DAT nuevo y CAMP.DAT con 'C111' detras de lo de antes, que sale igual byte a
byte; CAMPHUD.DAT y CAMPOBJ.DAT salen iguales que los de dat2), carpeta montada con h5/datos2.sh (lgdat2 + dat + dat_inf
+ dat2 + dat3); scripts en $S/modos/c2c/h5 (los de h4 para este worktree; gdi.sh con GD= para tener varios GDI a la
vez; py/bancos.py lista la tabla 0x352840 con los id de cada banco, py/peor.py lo que pide cada etapa en el AICA).
Capturas y registros en $S/modos/c2c/cap5.

### HECHO: las voces de las peticiones (bancos HAM_MIS de 0x352840)

Lo averiguado (SLES-53998):
- El metodo de la etapa del gestor (0x22ac00, maquina de estados +0xe8): estado 0, por cada peticion de la lista el
  switch 0x415280 (tipo - 1) da el banco 0x80nn de la tabla 0x352840 (las entradas 0x6c..0x96 y 0xa5 son HAM_MIS\...;
  las "54..77" de la cuarta tanda son HAM_SR\ y HAM\ de las etapas): 1 MATCH, 2 DRIFT, 3 PASS, 4 COLLIDE, 5 ZONE, 6
  CONEHIT, 7 COIN, 8 GATES, 9 ninguno, 10 NITRO, 11 HCARS (subtipo 0) / GHOST (1), 12 SLIP, 13 FOOD1..6 (rand % 6 a +8
  de los parametros), 14 MTOTAL, 15 BALL, 16 por el subtipo (0x415300: 0 MEMORY, 1 ODD_ONE, 2 SOUND si la etapa solo
  tiene esa peticion (0x22bd40 == 0x22bd60) y si no SOUND2..8 con rand % 7 (0x415320), 3 MATH, 4 ITEMS, 5 SHEEP, 6 CARS
  con rand % 3 a +8), 17 HOLDLINE, 18 KEEPCONT (no estan en el disco en espanol; ningun guion los pide), 19 METEOR, 20
  CUTLINE, 21 UFO, 22 HHORN, 23 DUCK, 24 PHOTO. Cada banco va al hueco 0x600 / 0x700 / 0x800 de 0x374180 si no lo lleva
  ya otro (+0xf4) y 0x11ed40 los carga. Estado 1: +0xf0 = rand % 4 (0x22b148): la fila de las voces de la nota
  (0x2a65a8) es al azar en cada etapa (la segunda tanda la tenia por PROBABLE = 46C520: corregido en camp_etapa).
  Estado 2: el banco RANK1..4 de 0x415268[+0xf0] en el hueco 4 (las voces de la nota 0xb3..0xc7 y 0x151..0x153, que
  tampoco sonaban).
- La variante 5 carga al empezar COMMON.pak (hueco 1) y PERH.pak (hueco 9) (0x1b7f74; el INFARTO: HC.PAK y PERH).
  COMMON es HC.PAK sin 0x14a / 0x14b / 0x163 y con 0xc6..0xcb (0x8c6, la voz del derrape del tipo 2, no sonaba).
- Las tablas de algunos bancos (RANK3, ITEMS, PERR) van detras de un relleno de ceros hasta el siguiente multiplo de 2048
  (tools/conv_camp.py `tablas_banco`; conv_sonido.banco_tablas no lo sabe: PERR.pak sigue SIN PORTAR en VOCESC.DAT).
- Los id de voz son de 10 bits (UFO.pak 0x23f): sfx.c busca los de la etapa con 10 (VOCESH.DAT sigue con 9).

Conversion (tools/conv_camp.py `voces_mision`, tambien solo con `--solo-voces`; regenera.py paso camp): VOCESM.DAT
(1,9 MB en el disco, 47 bancos: COMMON 6 voces 34 KB, RANK 37..43 KB, los de las peticiones de 5 a 138 KB) y la tabla
(tipo, subtipo) -> (banco, cuantos al azar). Fuera (DESVIACION): tres muestras de mas de 65535 muestras que un canal del
AICA no toca de una vez: PASS 0xd8 (el efecto del convoy del 3/1, 3,4 s), NITRO 0x17d (6,7 s) y DUCK 0x1e1 (5,8 s).

Codigo: comun/vozmis.c (-Os: solo corre al cambiar de etapa) y el enganche `camp_bancos` (campeonato.c lo llama en
camp_etapa con las peticiones de la lista; main.c `camp_bancos_` abre VOCESM.DAT la primera vez: ~2 KB de indice en el
monton durante el campeonato). Por etapa: suelta el bloque anterior del AICA, pide uno con COMMON + RANK + los bancos de
las peticiones sin repetir, y un hilo (thd_create) lo lee de 32 KB en 32 KB (malloc solo mientras lee) y lo sube con
spu_memload; al acabar, sfx_voz_extra pasa las filas a sfx.c, que las busca antes que las de VOCESH.DAT (por el canal de
voz y por el de efectos: camp_se tambien). La variante 5 sigue con VOCESH.DAT (sfx_voces_juego), sin infarto_voces().

AICA: con VOCESH.DAT quedan ~294 KB libres (824 KB con VOCES.DAT; VOCESH ocupa 740 KB, VOCES 210 KB); lo que mas pide
una etapa de los tres arboles con el peor azar (py/peor.py) son 280 KB (tres 13/0 con FOOD distintos); las etapas de
las pruebas piden 83..123 KB. Si no cabe: "VOZM: sin sitio" y esa etapa sin esas voces (no se ha visto).

E (Flycast; el Flycast de las pruebas va con audio:backend=null: verificado por el registro "VOZ 0x... (peticion)", no
escuchado; PROBABLE que el volumen sea el de la consola, la tabla SE del IRX como en VOCESH.DAT):
- Jennifer entera hasta la meta (`-DPRUEBA_AUTO -DPRUEBA_CAMP=0 -DPRUEBA_SIN_TIEMPO`, cap5/voces/jennifer_goal.txt):
  cinco etapas, cinco juegos de bancos (p. ej. "VOZM: etapa con 5 bancos (COMMON.pak RANK1.pak GATES.pak ZONE.pak
  COIN.pak), 20 voces, 123 KB en el AICA" y "20 voces de la etapa en el AICA" poco despues), 43 voces de peticion
  tocadas: anuncios (0x197 SLIP, 0x177 MATCH, 0x195 GATES), animo de COMMON (0xc6..0xca), nota de RANK (0xb8, 0xbe,
  0x152) y de los tipos (0xf1); "CURSO: GOAL en IMPE", sin "Out of memory".
- Clarissa BEAC (11/0 y 3/0): HCARS + PASS y la voz 0x1a6 del anuncio del 11 (cap5/t11c/flycast.txt).
- Sin probar: los bancos al azar (FOOD, SOUND2..8, CARS) en una carrera (ningun piloto paso por 13 / 16 en esta tanda).

### HECHO (SIMPLIFICADO): tipo 11, chocar con los coches de la peticion (subtipos 0 y 1)

Lo averiguado: el control 0x294a90 (vtable 0x422ec0 +0x24). Fase 1: copia a +0x10 de los parametros la tabla de
aparicion de 0x46 bytes que da 0x1f1a88, pone en +0x1e..+0x2a los 7 modelos (+0x64, o 0x96 con el subtipo 0 / 0xa5 con
el 1 si +0x64 == -1) y 0x1f20e8(+0x10) la deja en [0x5d5fd8], que 0x1f1a88 (el generador del trafico, tabla 0x5d5e78
de 5 x 0x46) usa mientras no sea 0; con el subtipo 1 prepara el sprite 2C0006 (0x115ae0); +0x74 = 80 / p78; 0x22dac0
llena 24 enteros con rand % 0x19 (o % p66). Fase 4: por las plazas 1..23 del trafico (0x556f28) con [0x55cf38 + 8 + i] &
3 == 2: con el coche 7 secciones por delante del jugador y 60 pasos desde el anterior, un efecto de 0x422e00 (0xd1..0xd3,
de GHOST.pak; CAMP.DAT 'C111'); golpeado (+0xb6c y +0x2f4 == 0), medidor + p74 y +0x10 + 1. Fase 5: 0x1f20e8(0); con
nota 6, +0x10 > p78 ? (< p7c ? (n - p78) / (p7c - p78) x 70 : (n - p7c) x 5 + 70) : 0. El subtipo 2 (0x295298) no lo pide
ningun guion.

Portado (comun/campeonato.c `camp_autos` / `camp_spr3d`; main.c traf_aparece y ha_spr3d en la variante 5):
- SIMPLIFICADO / DESVIACION: sin 0x1f1a88 (el generador con la tabla de la peticion): los coches del trafico que
  aparecen por delante del jugador entre el anuncio y el final son los de la peticion (marca HA_CAMP de la plaza), con
  los modelos del trafico (0x96 / 0xa5 no son modelos que tenga la DC; no se ha averiguado a que coche corresponden, asi
  que tampoco se usa RCmm.DAT). El golpe: b6c && !n2f4, una vez por coche (PROBABLE, como el 20 del INFARTO).
- Dibujo: 11/1 el 2C0006 de HAHUD.DAT (HA_S06) sobre el coche, como el 0x1b2190 del INFARTO; 11/0 (DESVIACION: sin su
  modelo) el icono del corazon del medidor (HA_C0) a escala 2 sobre el coche, a menos de 10 secciones. Un quad por coche
  (no son triangulos de la escena).
- E (`-DPRUEBA_AUTO -DPRUEBA_CAMP=1 -DPRUEBA_SIN_TIEMPO -DPRUEBA_C11`; PRUEBA_C11 cuenta como golpe pasar por la seccion
  del coche, porque el piloto no apunta a ellos): BEAC 11/0 (90..290): "coche de la peticion golpeado (1 de 10), medidor
  8" ... 4 de 10, "peticion 11 hecha: 32 - 0, nota 3"; el corazon sobre un coche de la peticion en cap5/t11c/z42.png
  (ampliacion de r_042). NO verificado: golpes de verdad (sin PRUEBA_C11) ni el 11/1 en pantalla.

### Medidas

- Tipo 11 (BEAC 11/0, la misma pasada con y sin los sprites `-DDIAG_SIN_C11`, DIAG_MS, 21 muestras de 30 cuadros entre
  el anuncio y el final; cap5/medidas/c11_con.txt y c11_sin.txt): 38,69 frente a 38,89 fps (-0,20), dibujo 19,68 frente
  a 19,59 ms, 17822 frente a 17818 triangulos de media (+4: un quad por coche marcado, 1..2 a la vez). Los coches son los
  del trafico (ninguno de mas).
- Voces: nada por cuadro; al cambiar de etapa un hilo lee 83..123 KB (las pruebas) del GD; sin tirones apreciables en
  Flycast (no medido aparte).
- RAM al arrancar (nav3.sh, MODO_JUEGO=1, sin defines, datos3; cap5/nav3_final.txt): "RAMMAPA arranque: libre 426K"
  (igual que c2c4), "VOZ: VOCES.DAT, 35 muestras", sin "Out of memory". El programa crece 4,3 KB (2 869 688 -> 2 874 128
  de text + data + bss) sin pasar de escalon de 8 KB (queda a ~1,1 KB del siguiente); vozmis.o y lo nuevo de
  campeonato.c a -Os, sfx.c sigue a -O2 (corre en cada cuadro). En la carrera del campeonato: ~2 KB del indice de
  VOCESM.DAT y las filas de la etapa (< 2 KB); 32 KB mas solo mientras el hilo lee.

### SIN PORTAR (lo averiguado en esta tanda)

- 0x1f1a88 / 0x1f20e8 (el generador del trafico con la tabla de la peticion): el 11 va SIMPLIFICADO (arriba); 13 y 22
  lo necesitan de verdad (los coches que se acercan / a los que se toca la bocina). Tampoco el 9 / 22 del INFARTO
  (0x1a9e08). Lo que falta: el formato de los 0x46 bytes (hay 5 en 0x5d5e78, de 0x158 bytes cada 5; +0xe..+0x1c los 7
  modelos) y que son los modelos 0x96 / 0xa5.
- 22 la bocina (0x29b3d0, clase de 0x38): fase 1 0x29b798 (con +0x78 == 1) y por la clase +0x14 (vtable de 0x29b330);
  fase 3 0x29bfc0(+0x7c) y [+0x1ec] / [+0x1e8] del gestor; fase 4 +0x70 += el avance y 0x29c058; fase 5 el efecto 0x1bc
  (HHORN.pak, que ya suena con VOCESM.DAT) y con nota 6 30 x min(1, (medidor - pen - 80) / (80 / 0,9 - 80)) + 40 x
  0x22f090.
- 13 (FOOD: el azar del banco ya queda en pet_t.rnd, el +8 que lee el tipo), 14, 16 (los minijuegos: sus bancos de voz
  ya se cargan), 20, 21, 23, 24; el rotulo propio del 20; el letrero 3D de inicio / fin (0x232e60): sin empezar en esta
  tanda (el tiempo se fue en las voces y el 11).
- (c2c5 en integra) Jennifer entera hasta la meta con los datos juntos (VOCESH.DAT de sp, 511 KB en el AICA, y
  VOCESM.DAT; `-DPRUEBA_AUTO -DPRUEBA_CAMP=0 -DPRUEBA_SIN_TIEMPO`): las 5 etapas cargan sus bancos (81..123 KB),
  43 voces de peticion tocadas, ningun "VOZM: sin sitio", "CURSO: GOAL en IMPE", sin "Out of memory"; al arrancar 444K
  libres. GD clasico 41,9 MB libres; Mod (lgmod2 + lo mismo) 15,6 MB libres, sin los mipmaps.

## Rev 2.9.9.d: arreglo de FPS (culling en bucles cortos, sin quitar nada) (2026-11-27)

Pedido: subir los fps (sobre todo METR, CLOU y PALM) y bajar los cuadros tarde sin perder nada en pantalla. Todo en el
Flycast de Linux, una sola instancia; nada en la consola ni en Android. Flycast cuenta instrucciones (1 ciclo, emparejadas
si son de unidades distintas, +2 por cada uno de los tres primeros accesos a memoria de cada bloque) y no fallos de cache.

PERFIL (`-DDIAG_PERFIL`, METR, cuadro congelado 1500): dibuja_etapa 21-30 %, dibuja_indices (coche y trafico) 11 %,
robot_dibuja 9 %, nodos_visibles 8 %, coche_entorno 3 %. En dibuja_etapa el envio de vertices es un tercio; el resto es
la prueba de cada malla: el ojo, los ejes y las constantes (19 floats) no cabian en los registros y se releian de la pila.

HECHO (`demo/main.c`, cada uno con su -D; no cambian que se dibuja)
- `ETAPA_CULL` (1): `etapa_mallas_vistas` elige en un bucle corto las mallas de un tramo de nodo que pasan la PVS y la
  vista, 32 por llamada; el centro va a la pantalla con un ftrv de la vista ya cargada (X = 320 W + fx xv,
  Y = 240 W - fx yv, W = zv) y las pruebas de siempre multiplicadas por fx. No en los carteles, ni en la etapa siguiente
  antes de la entrada o creciendo (alli la vista va escalada), ni pasado el tope de vertices (ahi sigue el camino de
  siempre desde la malla siguiente, lo mismo que hacia).
- `NODOS_CULL` (1): `nodos_candidatos` hace las tres primeras pruebas de cada nodo de nodos_visibles (lejos, detras,
  lado) en un bucle corto, con el mismo ftrv y las mismas marcas (FF, POPR); el bucle de siempre solo ve los que pasan.
- `FOCOS_LOTE` (1): las mallas dibujadas se apuntan y `focos_lote` las mira para los focos al final, en el mismo orden.
- `demo/Makefile`: `comun/vmuesc.o` con -Os (como en la rama de integracion): devuelve la RAM del codigo nuevo.
- Medida: `-DDIAG_CULL_CMP` hace en cada cuadro las pruebas de siempre al lado de las nuevas y escribe "CULLCMP:".

PROBADO Y QUITADO
- Dos vertices por vuelta en los bucles de la SQ (sq_vtx, sq_vtx_ofs, sq_pt): METR congelado coche 2,27 -> 2,49 ms,
  etapa 9,30 -> 9,71 (peor en Flycast).
- La primera version de ETAPA_CULL, una llamada por malla visible: etapa 9,30 -> 9,50 (la entrada y salida de la
  funcion costaba lo que ahorraba).
- Los bucles por vertice de robot_dibuja en funciones aparte: 2,49 -> 2,51 ms (nada).

MEDIDA 1: cuadro congelado (`-DDIAG_CONGELA=1500 -DDIAG_FALTA`), ms (mediana); imagen comparada pixel a pixel con la base

| Etapa | dib base | dib nuevo | vis | etapa | pixeles distintos |
|---|---|---|---|---|---|
| METR | 23,56 | 22,21 | 3,17 -> 2,36 | 9,30 -> 8,76 | 0 |
| PALM | 13,29 | 12,42 | 2,25 -> 1,55 | 2,20 -> 2,06 | 0 |
| CLOU | 14,25 | 13,49 | 2,37 -> 1,80 | 3,46 -> 3,31 | 0 |

(PALM y CLOU sin FOCOS_LOTE.) Por separado (METR): ETAPA_CULL etapa 9,30 -> 8,84; NODOS_CULL vis 3,23 -> 2,36;
FOCOS_LOTE etapa 8,84 -> 8,76. Las mismas mallas recorridas y dibujadas (linea MAL) y la misma lista de focos (FOCOSMS).

MEDIDA 2: `-DDIAG_CULL_CMP` con el piloto (METR, CLOU, SNOW -> IMPE, INDU -> FORE; 140 s): de 30,4 millones de mallas
miradas, 2 distintas en la vista y 1 en seguro (el redondeo de una esfera que roza el borde de la vista); de 20,1
millones de nodos, 0 distintos.

MEDIDA 3: A/B en las 15 etapas. A = 4b1c25b, B = ca8c459 (ETAPA_CULL + NODOS_CULL; sin FOCOS_LOTE, que es de despues).
`-DPRUEBA_AUTO -DPRUEBA_NODO=n -DPRUEBA_SIN_TIEMPO -DDIAG_MS -DDIAG_POPPING`, 140 s de piloto, paso 10, una sola
instancia (la 2.9.9.c se midio con tres a la vez: las cifras de A no son las de su tabla). Sin -DDIAG_FALTA: la columna
de grandes que faltan queda vacia (no dio tiempo a la segunda pasada).

| Etapa | fps A | fps B | B-A | tarde A / B | cpu ms A / B | salto A / B | lejano A / B | 8 px+ A / B | 32 px+ A / B | grandes que faltan A / B |
|---|---|---|---|---|---|---|---|---|---|---|
| PALM | 37,30 | 37,99 | +0,69 | 146 / 93 | 24,00 / 23,09 | 7,86 / 6,77 | 3800 / 3800 | 8,2 / 8,8 | 0,8 / 0,8 | - / - |
| LAKE | 38,73 | 38,75 | +0,02 | 72 / 71 | 21,65 / 21,13 | 3,93 / 2,76 | 3800 / 3800 | 2,3 / 2,5 | 0,1 / 0,1 | - / - |
| ALPI | 38,99 | 38,99 | +0,00 | 53 / 55 | 20,33 / 20,00 | 0,56 / 0,27 | 3800 / 3800 | 6,6 / 6,5 | 1,2 / 1,1 | - / - |
| CAST | 38,71 | 38,99 | +0,28 | 83 / 53 | 21,96 / 20,89 | 3,20 / 2,24 | 3800 / 3800 | 13,1 / 14,7 | 2,4 / 2,6 | - / - |
| FORE | 39,00 | 39,41 | +0,41 | 58 / 20 | 20,05 / 19,37 | 0,29 / 0,30 | 3800 / 3800 | 8,3 / 11,9 | 1,6 / 2,3 | - / - |
| DESE | 38,91 | 39,19 | +0,28 | 37 / 31 | 20,10 / 19,01 | 1,22 / 1,18 | 3800 / 3800 | 6,8 / 5,7 | 2,5 / 2,2 | - / - |
| CLOU | 36,00 | 36,61 | +0,61 | 237 / 208 | 25,35 / 24,61 | 7,61 / 6,91 | 3800 / 3800 | 18,9 / 18,4 | 1,8 / 1,8 | - / - |
| INDU | 38,29 | 38,70 | +0,41 | 53 / 31 | 23,01 / 22,15 | 4,27 / 2,72 | 3800 / 3800 | 13,6 / 14,4 | 5,1 / 5,1 | - / - |
| SNOW | 39,36 | 38,91 | -0,45 | 25 / 64 | 20,17 / 19,97 | 0,59 / 0,29 | 3800 / 3800 | 3,8 / 9,2 | 0,9 / 1,3 | - / - |
| GHOS | 37,60 | 37,74 | +0,14 | 143 / 126 | 23,76 / 23,33 | 6,99 / 6,55 | 3800 / 3800 | 11,9 / 12,4 | 2,1 / 2,2 | - / - |
| TULI | 38,52 | 38,74 | +0,22 | 69 / 54 | 22,20 / 21,72 | 5,78 / 4,21 | 3598 / 3694 | 19,6 / 21,2 | 2,6 / 2,7 | - / - |
| METR | 35,32 | 35,76 | +0,44 | 221 / 207 | 27,00 / 26,25 | 6,84 / 6,58 | 3800 / 3800 | 12,2 / 12,2 | 3,2 / 3,3 | - / - |
| RUIN | 39,28 | 39,31 | +0,03 | 37 / 31 | 19,00 / 18,77 | 0,81 / 0,29 | 3576 / 3573 | 8,7 / 9,1 | 1,2 / 1,3 | - / - |
| IMPE | 38,92 | 39,01 | +0,09 | 43 / 28 | 21,46 / 20,77 | 4,33 / 3,09 | 3717 / 3800 | 15,8 / 17,5 | 3,4 / 2,8 | - / - |
| CAPE | 39,13 | 39,28 | +0,15 | 28 / 24 | 20,20 / 18,96 | 1,34 / 0,31 | 3800 / 3800 | 12,6 / 11,8 | 4,0 / 3,7 | - / - |
| **Media / suma (15)** | 38,27 | 38,49 | +0,22 | 1305 / 1096 | | 3,71 / 2,96 | 3766 / 3778 | 162,4 / 176,3 | 32,9 / 33,3 | 0,00 / 0,00 |

tiradas con 'VOZ: VOCES.DAT, 35 muestras': 30 de 30; lineas 'Out of memory': 0

Saltos por s sumados en las 15 (apariciones + / desapariciones -, 8 px / 32 px), por razon:

| razon | A +8/32 -8/32 | B +8/32 -8/32 |
|---|---|---|
| lado | +6.3/1.7 -0.0/0.0 | +5.4/0.8 -0.0/0.0 |
| lod | +0.1/0.0 -0.0/0.0 | +0.1/0.0 -0.0/0.0 |
| pvs | +104.3/27.2 -18.3/1.1 | +109.8/28.3 -20.0/1.3 |
| rama | +2.7/1.0 -0.0/0.0 | +2.6/1.1 -0.0/0.0 |
| tope | +19.8/1.1 -10.7/0.3 | +23.7/1.2 -13.6/0.4 |
| tramo | +0.3/0.1 -0.0/0.0 | +1.3/0.6 -0.0/0.0 |

- fps: +0,22 de media; las peores suben (PALM +0,69, CLOU +0,61, METR +0,44); SNOW -0,45 (25 -> 64 cuadros tarde).
  Cuadros tarde 1305 -> 1096 (-16 %). El nivel del reparto baja (3,71 -> 2,96): se recorta menos.
- Saltos de 8 px: 162,4 -> 176,3 por s (+8,5 %), sobre todo "pvs" (+5,5: FORE, IMPE, CAST) y "tope" (+3,9: SNOW).
  SUPUESTO: es la variacion entre tiradas (el piloto no hace dos veces la misma carrera; en la 2.9.8 dos tiradas iguales
  daban hasta 9 por s de diferencia): el cuadro congelado sale identico pixel a pixel con las mismas mallas, y la
  comparacion en cada cuadro (MEDIDA 2) no encuentra diferencias que cambien lo que se dibuja. No se ha repetido la
  tanda para separarlo del ruido.
- RAM (compilacion normal, tools/prueba/nav3.sh): libre al arrancar 352K en la base y 352K con todo (sin el -Os de
  vmuesc.o, 344K). "VOZ: VOCES.DAT, 35 muestras" en las 30 tiradas y en las nav3, 0 "Out of memory", guardado en la VMU.

Variante -O2 de la rama de integracion (8e2a856, pedida): mision.o, rival.o, campeonato.o, infarto.o y haobj.o fuera de
la linea -Os. Codigo 809024 -> 822296 bytes. RAM libre al arrancar (nav3): 416K -> 408K. C2C (`-DPRUEBA_MISION=0`, BEAC,
140 s): 37,52 -> 37,51 fps, 122 -> 123 tarde. INFARTO (`-DPRUEBA_VARIANTE=2`, PALM): 37,26 -> 37,13 fps. Sin ganancia
medible en Flycast; VOZ 35 muestras y 0 Out of memory en las dos. En rev-1.1 (4b1c25b) esos ficheros ya van con -O2
(solo menu.o va con -Os) y haobj.o no existe.

NO HECHO / QUEDA
- La pareja (2,5 ms por cuadro para 2898 vertices) y el coche (2,3 ms): sin tocar (lo probado no ganaba).
- Cuadros tarde en cambios de seccion y cargas, salto_q, tiras unidas con degenerados: sin estudiar.
- Store queues y prefetch: sin cambios.
- Consola real y Android: sin medir.

Ficheros: modos/fps/ del directorio de trabajo (res/ las consolas, cap/ las comparaciones, sh/ y py/).
  tanda (el tiempo se fue en las voces y el 11). -> HECHO 20, 21, el rotulo del 20 y el letrero; 14 PARCIAL (ver "sexta
  tanda").

## COAST 2 COAST: campeonatos, sexta tanda (agente c2c6, 2026-11-27)

Sobre "campeonatos, quinta tanda (agente c2c5)" (7e5b270). Todo en Flycast (nivel E), no en la consola. Datos de prueba:
$S/modos/c2c/dat4/CAMP.DAT (lo de antes igual byte a byte salvo el rotulo de las 9 peticiones del tipo 20, y detras 'LET1'
y 'C141'; CAMPHUD.DAT, CAMPOBJ.DAT y VOCESM.DAT salen iguales que los de dat2 / dat3), carpeta montada con h6/datos2.sh
(lgdat2 + dat + dat_inf + dat2 + dat3 + dat4 en modos/c2c/datos4); scripts en $S/modos/c2c/h6 (los de h5 para este
worktree; corre.sh: compila con los defines, GDI propio, auto.sh y copia registro y capturas a cap6/<nombre>; correw.sh lo
mismo con otro arbol, para la base c2c5). Capturas y registros en $S/modos/c2c/cap6.

### Lo que piden los guiones (CAMP.DAT, h6/donde.py)
- 14: 7 peticiones (Holly); 20: 9 (Clarissa); 21: 8 (Clarissa); 24: 6 (Holly); 23: NINGUNA (ningun guion de los tres
  arboles la pide: no se porta). 13: 10 y 22: 7 (Holly).

### HECHO: tipo 20, cruzar la linea entre dos coches (0x296120) y su rotulo
- Lo averiguado: la clase (0x474 bytes) lleva 20 pares de 0x38 en +0xc {plaza a, plaza b (0x19a ninguna), punto a + 1 m,
  punto b + 1 m, ...}. Fase 1: como el 11 (0x1f1a88 / 0x1f20e8 con los modelos +0x64 o 0x104; 0x22dac0); +0x70 de los
  parametros = 0. Fases 2..4 (0x296560): suelta los pares con un coche que falta; empareja los coches de la peticion
  ([0x55cf38 + 8 + i] & 3 == 2) de la cadena +0x314 del jugador (< 250 m) dentro de [inicio, fin] con el mas cercano a
  menos de 6 secciones que no este emparejado ni cruzado (+C5C 0xc000 << 33). Fase 4 (0x296f58): la linea en el marco del
  jugador contra su caja (0x1978d8 con +0x338, como el 9 del INFARTO); cruzada: 0x296cc0 la suelta, movimiento 0x6003d
  (coche 9) o 0x6003e / 0x6003f (rand < 0,5), +0x70 + 1 y el medidor + 80 / p74 (0x374190 + 0x18). Fase 5: suelta los 20 y
  0x1f20e8(0); nota 6: (+0x50 == 0 ? 40 : 0) + min(1, (n - p74) / (p78 - p74)) x 30 si n > p74, + (n - p78) x 5 si n > p78.
  Dibujo (0x296da0, fases 2..4): los modelos 0x57002f y 0x57002a en los pares (SIN PORTAR, ver abajo).
- El rotulo: el switch 0x4155b0 lleva el 20 a 0x22fe08, que es 2B0039 ("¡Cruza la linea!" con los dos coches, el HA_T17 de
  HAHUD.DAT; con a2 = 0, el del banco 0x2B). tools/conv_camp.py paraba alli el interprete como si fuera el caso 0 (sin
  rotulo): ya no, y las 9 peticiones del 20 salen con su rotulo.
- Portado: comun/campeonato.c (case 20, camp_autos, camp_lineas) con los pares del INFARTO (comun/haobj.c haobj_pares /
  haobj_pares_cruza, con la mascara nueva haobj_pares_mascara(HA_CAMP): solo los coches de la peticion y sin emparejar uno
  ya emparejado). DESVIACION (sin 0x1f1a88): como el 11, los coches del trafico que aparecen por delante desde el anuncio son
  los de la peticion, y a cada uno main.c le abre otro al lado (`camp_pares_pide`: dos carriles mas alla si los hay, para
  dejar uno libre entre los dos); 10 pares como mucho (los del INFARTO), con 80 m en vez de 6 secciones.
- Dibujo (DESVIACION, sin los modelos 0x57002f / 0x57002a): una cinta translucida amarilla de 0,3 m entre los dos coches
  (+1 m), 2 triangulos por par (main.c camp_lineas_dibuja).
- E (Clarissa nodo 4 lista 1: 20/0 90..240, 21/0 348..490, 20/0 565..640; `-DPRUEBA_AUTO -DPRUEBA_CAMP=1
  -DPRUEBA_CAMP_NODO=4 -DPRUEBA_CAMP_LISTA=1 -DPRUEBA_SIN_TIEMPO`): el rotulo "¡Cruza la linea!" (cap6/t20d/mos.png), los
  pares (registro "HAOBJ: par 0: plazas 4 y 3 a 10.4 m") y la linea amarilla entre dos coches (cap6/t20d/z42.png). El
  piloto no pasa entre ellos: con `-DPRUEBA_C20` (un par a 3 secciones cuenta como cruzado) "linea cruzada (1 de 6),
  medidor 13", la reaccion 0x6003e y la nota 1 (cap6/t2021p). NO verificado: un cruce de verdad conducido.

### HECHO (SIMPLIFICADO): tipo 21, los OVNIs (0x292840)
- Lo averiguado: fase 1 como el 11 con los modelos +0x64 o 0xb4 y 0x22dac0(10); fase 3 el sonido 0x1d8, fase 5 0x81d8 (lo
  para). Fase 4: +0x70 += el avance; con mas de 100 km/h, dentro de la carretera (0x2325a0(0, 1)) y sin OVNI encima (+0x18
  == 0) el medidor + avance x 80 / (largo x 0,85) y cada 0xb4 pasos la voz (0x2a6240 o la de 0x422c70 alternando, 0x3b5520 /
  0x3b5522). Fases 2 y 4, 0x292d28: por los coches de la peticion con +0xd14 == 4 y sin +C5C 0x8000 el toque 0x1d3a10 (radio
  4); tocado: +0x18 = el coche, efecto 0x23f (UFO.pak), 0x2052f8 y el movimiento 0x60048; despues 0x1f pasos que lo acerca
  (+0xd18 del jugador), 50 de giro (+0xc, angulo al azar) en los que cada 10 la penalizacion + 3, y lo suelta (+C5C 0x8000 y
  +0xb64 = 0x3c). Nota 6: (sin roces, ruedas fuera ni muro 30) + 40 x 0x22f090.
- Portado (case 21 y camp_autos): el medidor y el bono como el original; el toque a 5 m del coche (PROBABLE) o al
  golpearlo; la penalizacion de 3 cada 10 pasos en los 30 ultimos (3 veces), la voz 0x23f, la reaccion 0x60048 y camp_golpe
  (v x 0,9) al empezar. DESVIACION: el jugador no se acerca ni gira con el OVNI; sin el sonido continuo 0x1d8; sin el modelo
  del OVNI (0xb4: no esta en la DC), los coches de la peticion con el sprite 2C0006 encima (el de 11/1, HA_S06).
- E (la misma etapa): "peticion 21 hecha: 94 - 0, nota 6, 80 + 90 corazones" (con c2c5, nota 0). NO verificado: el toque
  (ni con `-DPRUEBA_C21`, que lo da a 8 secciones: los coches de la peticion van por delante y el piloto no los alcanza).

### PARCIAL: tipo 14, la suma (0x29ccb8)
- Lo averiguado: fase 1 +0x70 = +0x72 = 0 y 0x29d410: los coches de la lista +0x5c de los parametros (registros de 0x14
  {+8 s16 seccion desde el inicio, +0xa s8 carril, +0xc = 3, +0x10 el valor}, 0x298268 los cuenta hasta uno negativo, 100
  como mucho); valor: con rand < 0,2 uno de 0x3b55d0 + 0x28 x nivel entre los 0x3b55c0[nivel] primeros (negativos: -10,
  -5...), si no de 0x3b5548 / 0x3b5538 (5, 10; 2..10...); nivel = +0x74 (byte) de los parametros; objetivo +0x70: por rondas
  (hasta 10) cada valor con 0,7 (positivo) / 0,4 (negativo), hasta pasar de la suma de todos / 8. Fase 4: por los coches con
  +0xd14 == 3 el toque 0x1d3a10 (radio 3, o 4 si |valor| >= 10): +0x72 += valor (no baja de 0), medidor = (objetivo -
  |objetivo - suma|) / objetivo x 80 x 1,15, +0x3d78 / +0x3d74 (mas cerca / mas lejos) y 0x232290 (el HUD); el que se queda
  atras sale (0x19c990). Nota 6: (sin ruedas fuera ni muro 30) + 40 x 0x22f090. HUD 0x231fe0 (objetivo) / 0x232290 (suma).
- tools/conv_camp.py: 'C141' (las tablas y los coches de las 7 peticiones: 68, 20, 25, 32, 15, 27 y 26).
- Portado: el valor de cada coche y el objetivo como 0x29d410, el toque (radio + 1,5 m, PROBABLE), la suma, el medidor y el
  bono; main.c pone los coches de la lista 40 secciones antes de la suya en su carril, con el modelo de un coche del trafico
  (DESVIACION: no se sabe que modelo y velocidad les da el original; aqui los del trafico). DESVIACION: el numero sobre el
  coche con las cifras del HUD (rojo el negativo, sin signo) y la suma y el objetivo bajo el contador de corazones, sin el
  panel del original; el nivel de un guion con basura en +0x74 (0x87, Las Vegas) se topa a 2.
- E (Holly nodo 2 lista 0: 16/0, 14/0 380..590, `-DPRUEBA_CAMP=2 -DPRUEBA_CAMP_NODO=2 -DPRUEBA_CAMP_LISTA=0
  -DPRUEBA_C14`, que da por tocados la mitad de los coches a 1 seccion): "suma: 32 coches, nivel 0, objetivo 155", los 23
  primeros puestos en sus secciones y carriles, "suma 10 (+10) de 155, medidor 5", el rotulo "¡Consigue el total!" y el
  objetivo en pantalla (cap6/t14b/mos.png). NO verificado: los coches y sus numeros en pantalla (el registro dice que se
  dibujan a 25..75 m, pero en las capturas de LASV no se ve ningun coche en la carretera: PENDIENTE).

### HECHO: el letrero 3D de inicio / fin de la peticion (0x232e60)
- Lo averiguado: el dibujo del gestor (0x22a2b0) por cada peticion de la lista de la etapa: en su inicio el modelo de
  0x1a7c80 y en su fin el de 0x1a7cb0 (salvo los minijuegos, tipo 0x10), mientras la seccion del jugador esta entre la del
  letrero - 100 y + 2; 0x232e60 lo pone en el centro de la seccion (0x22c500) + 4 m + [+0x18c] (con 0x22b880), con escala
  (1, 1), si el jugador esta entre - 50 y + 2 (y en la etapa siguiente si cae alli: 0x176e20 / 0x176e48, SIN PORTAR). Son
  los mismos modelos por idioma que las marcas del INFARTO (0x570035 "INICIO" y 0x570024 en espanol).
- CAMP.DAT 'LET1' (los dos registros, conv_infarto.marcas); comun/campeonato.c `letrero` con haobj_marcas (el vaiven del
  INFARTO en vez de [+0x18c]: PROBABLE) y el punto de main.c (ha_punto, `camp_punto`). 2 triangulos por letrero.
- E: "HAOBJ: 1 marcas (registros 0x35 / 0x0)" al anunciar cada peticion y 0x24 al acabar; INICIO sobre la carretera
  (cap6/t14b/mos.png, cap6/t20d/mos.png) y FIN (cap6/t14/mos2.png). PROBABLE: la posicion (en una curva de NIAG sale al
  lado de la carretera, como en el INFARTO).

### Medidas (Flycast, DIAG_MS, la misma pasada con y sin `-DDIAG_SIN_C20 -DDIAG_SIN_C11 -DDIAG_SIN_LET`; cap6/m_con, m_sin)
| elemento | tramo | fps con / sin | triangulos de la escena con / sin | suyos (media / max por cuadro) |
|---|---|---|---|---|
| 20, lineas | 20/0 90..240, 19 muestras | 39,76 / 39,76 | 16394 / 16388 | 3,25 / 4 |
| 21, sprites | 21/0 348..490, 21 muestras | 38,57 / 38,52 | 17434 / 17430 | quads 2D (fuera de la escena) |
| letreros | la etapa entera, 113 muestras | 38,66 / 38,66 | 15872 / 15865 | 2 / 2 |
| 14, numeros | sin medir (cifras 2D del HUD; los coches de la lista ocupan plazas del trafico, ninguno de mas) | | | |

### RAM
- Arranque (h6/nav3.sh, MODO_JUEGO=1, sin defines, compilado limpio, datos4; cap6/nav3_final.txt): "RAMMAPA arranque:
  libre 418K" (c2c5: 426K), "VOZ: VOCES.DAT, 35 muestras", sin "Out of memory". El programa crece 5,3 KB (2 874 128 ->
  2 879 452 de text + data + bss) y pasa un escalon de 8 KB del .init: de ahi los 8 KB. Nada nuevo en el monton al arrancar
  ni en la carrera (los pares y el 14 van en el estado del campeonato, ~0,3 KB mas del monton solo en el campeonato; 'C141'
  + 'LET1' 1,2 KB de CAMP.DAT). Ningun modulo de cada cuadro a -Os (campeonato.o ya lo estaba).
- Pruebas nuevas (solo con defines): PRUEBA_C20 (un par a 3 secciones cuenta como cruzado), PRUEBA_C21 (un OVNI a 8
  secciones cuenta como toque), PRUEBA_C14 (la mitad de los coches con numero a 1 seccion cuentan como tocados);
  DIAG_SIN_C20 / DIAG_SIN_LET (medidas sin las lineas / los letreros).

### Lo que queda (con lo averiguado)
- Ajeno a esta tanda: "Out of memory ... diff 57344" al pasar de NIAG a LASV en la prueba de Clarissa nodo 4: sale igual con
  la base c2c5 (cap6/base_n4b), la carrera sigue (PENDIENTE: que lectura es).
- 24, las fotos (0x2a48a0; 6 peticiones): fase 1 0x2a5fd8 lee la lista +0x70 de los parametros (registros de 0x20 hasta
  +0x18 == -1, +0x74 relativa al inicio: los objetivos), fase 4 0x2a4fa8 (la camara: 0x2bb0a0 proyecta el objetivo; el
  disparo 0x2a60a0), fase 5 suelta (0x21bb48) y nota 6: min(1, (medidor - pen - 80) / (80 / 0,9 - 80)) x 30 + 40 x
  0x22f090. HECHO (SIMPLIFICADO; ver "campeonatos, septima tanda (agente c2c7)"): los objetivos en CAMP.DAT y un visor.
- 23 (0x2987a0, los patos con el sistema de objetos 0x61e8a0): ningun guion lo pide.
- 20: los modelos 0x57002f / 0x57002a del dibujo; 21: el acercamiento y el giro del jugador (0x292d28) y el sonido 0x1d8;
  14: verlo en pantalla, el panel 0x231fe0 / 0x232290 y las voces +0x3d74 / +0x3d78.
- 0x1f1a88 / 0x1f20e8 (el generador del trafico de la peticion, la tabla de 0x46 bytes) y con el los tipos 13 y 22: sin
  empezar en esta tanda. El 14 no lo necesita: sus coches son la lista +0x5c del guion ('C141'); PROBABLE que el 13 y el 22
  tambien lleven su lista +0x5c (mirarlo con h6/objs.py).
- 16 (minijuegos): sin empezar en esta tanda.
- (FPS y c2c6 en integra, 2026-11-27) nav3.sh con los datos juntos (CAMP.DAT de c2c/dat4): "VOZ: VOCES.DAT, 35
  muestras", "RAMMAPA arranque: libre 408K", sin "Out of memory", carrera de COAST 2 COAST. PENDIENTE (c2c6, tambien en
  la base c2c5): "Out of memory ... diff 57344" en carrera al pasar de NIAG a LASV (Clarissa nodo 4).

## Mipmaps en lo lejano (2026-11-27; DESVIACION del port)

Peticion: "arreglo de mipmapping". Hasta ahora solo llevaban mipmaps de la PowerVR las texturas cuadradas del suelo con
MXL > 0 en el original (ver Suelos: estado del GS, mipmaps y capas); lo demas (paredes, edificios, vallas, vegetacion,
carteles, el agua, la hierba y las texturas rectangulares, tambien las de la calzada) centelleaba de lejos. El original
de PS2 tampoco los lleva en esas texturas (su LOD del GS es por distancia, TEX1 K / L): esto es una mejora del port.

**Diagnostico** (`tools/remip.py --info [-v]`, datos de lgdat2; KB de VRAM; `cuad_*`: cuadradas sin mipmaps):

| Fichero | con mip | cuadradas VQ sin | cuadradas 16 bits sin | rectangulares (sin) | cielo |
|---|---|---|---|---|---|
| PALM SEG00 | 28 / 107 | 44 / 93 | 5 / 2 | 91 / 242 | 2 / 7 |
| LAKE SEG03 | 23 / 129 | 53 / 92 | 5 / 2 | 64 / 158 | 2 / 7 |
| INDU SEG04 | 15 / 94 | 71 / 190 | 2 / 1 | 71 / 178 | 2 / 67 |
| ALPI SEG05 | 18 / 68 | 58 / 93 | 22 / 11 | 65 / 234 | 2 / 67 |
| SNOW SEG06 | 11 / 122 | 24 / 89 | 0 | 59 / 224 | 2 / 67 |
| CLOU SEG07 | 11 / 73 | 12 / 68 | 0 | 40 / 260 | 2 / 19 |
| CAST SEG08 | 8 / 62 | 32 / 82 | 9 / 4 | 41 / 183 | 2 / 67 |
| GHOS SEG09 | 10 / 65 | 24 / 75 | 0 | 51 / 309 | 2 / 67 |
| FORE SEG10 | 14 / 166 | 23 / 117 | 2 / 1 | 43 / 241 | 2 / 67 |
| DESE SEG11 | 20 / 86 | 23 / 51 | 2 / 1 | 27 / 103 | 2 / 67 |
| TULI SEG12 | 28 / 140 | 41 / 98 | 3 / 2 | 65 / 202 | 3 / 8 |
| METR SEG13 | 13 / 79 | 61 / 128 | 1 / 0 | 58 / 164 | 2 / 7 |
| RUIN SEG14 | 7 / 31 | 15 / 54 | 0 | 33 / 197 | 2 / 67 |
| CAPE SEG15 | 31 / 141 | 35 / 102 | 0 | 59 / 150 | 2 / 67 |
| IMPE SEG16 | 15 / 80 | 43 / 168 | 1 / 0 | 97 / 405 | 2 / 67 |
| uniones BK00 / BK06 / BK09 | 5 / 21 | 8 / 34 | 0 | 13 / 88 | 2 |

Por que no tenian: las rectangulares (la mitad de la VRAM de cada etapa: 128x64, 128x32, 64x32, 256x64...) porque la
PowerVR solo admite mipmaps cuadrados; las cuadradas porque `modelo.ajusta_texturas` solo los pone en el suelo con MXL
> 0; el umbral de la VQ (9) casi no cuenta (2 de 338 en la primera prueba de 4 etapas, 32 en todas). Casi todas se
reducen dentro de la distancia de dibujo: con la focal de la carrera (426,7 px) un texel de 2-10 cm baja de 1 pixel
a 9-45 m (`--info -v`: "reduce desde"). En las capturas (Flycast, cuadro congelado `-DDIAG_CONGELA=2400`, PALM / CLOU /
GHOS / METR) lo que mas centellea: los arboles y vallas de recorte (GHOS, CLOU), las fachadas con ventanas (PALM,
METR), el grano del asfalto, el agua y la hierba (PALM, CLOU), el empedrado de la plaza (METR) y los carteles.

**Arreglo** (`tools/remip.py --datos`, sobre las etapas y uniones ya convertidas; `tools/regenera.py` lo pasa al final,
una vez):

- Cuadradas: niveles por promedio de 2x2 desde la imagen de ahora (la VQ decodificada) y VQ con un libro para todos los
  niveles (`vq.codifica_mip`), aceptada con error RMS <= 9 frente a esa imagen y sus niveles (media ~5). Mantener el
  libro y los indices de la base y codificar solo los niveles nuevos daba error 14 de media (48 en las de recorte): no.
  Si la VQ no pasa, sin mipmaps (en 16 bits costarian 2,7 veces mas). Las de menos de 32x32 en 16 bits con niveles.
  En las de recorte (ARGB1555) el alfa de cada nivel se corta por el umbral que guarda la cobertura de la base (si no,
  vallas y hojas se vacian de lejos).
- Rectangulares: estiradas al lado mayor (interpolacion lineal entre centros de texel; las UV normalizadas no cambian
  y de cerca se ve igual). Al lado menor perdian 6-40 de RMS (tienen detalle en los dos ejes): nunca se usa (umbral
  `PERDIDA` 3). Estirar un eje hace que la PowerVR elija el nivel por ese eje y el otro se vea un nivel mas borroso
  (medido en Flycast: las fachadas de PALM quedaban lisas). Se compensa con el **D adjust** de la PowerVR por textura
  (bits 24-27 del formato, `demo/main.c` compila_cab -> `mipmap_bias`): D = 2^-(niveles que se adelanta), con el
  aspecto del texel en el mundo medido en la geometria (`remip.sesgo`): 0,5 en las 2:1, 0,25 en las 4:1, 1,0 si el
  eje estirado era el mas grueso. PROBABLE: que la D de la consola multiplica la medida del nivel (documentacion de la
  PowerVR); Flycast la aproxima con un sesgo de -4 / -2 / -1 niveles para 0,25 / 0,5 / 0,75 (rend/TexCache.cpp), asi
  que las capturas se hicieron con `-DMIP_SESGO_FLYCAST` (pide a Flycast los niveles que pediria la consola).
- Sesgo general y filtro: las demas van con D 1,0 y bilineal (nivel mas cercano, como MMIN del GS). Trilineal no: en la
  PowerVR son dos pasadas por poligono (filtros TRILINEAR1 / 2 sumados), el doble de vertices de la escena en el TA
  (24-28 mil por cuadro, medidos con DIAG_MS), y el bufer del TA es lo que limita la distancia de dibujo; Flycast
  ademas no lo emula ("we ignore that for now", rend/gles/gldraw.cpp).
- Orden y topes: por detalle fino (lo que quita el primer nivel) x area / coste, cada fichero hasta TOPE_ETAPA 570 KB /
  TOPE_UNION 220 KB y sin que ninguna pareja de CURSO.DAT y CURSOTA.DAT (etapa + su union, union + la etapa de cada
  rama) pase de 944 KB, el peor caso de antes: la zona del curso no empeora en ningun modo. Los topes salen del espacio
  del GD (presupuesto: <= 8 MB en el Mod taxi): con 720 / 240 KB crecia 10,9 MB y con 640 / 225 8,6 MB. IMPE (748 KB
  con BK198) no gana nada por el tope de pareja. El mayor SEG y el mayor BK no crecen (los huecos de la RAM son su
  tamano). Determinista por contenido: la meta y su etapa siguen compartiendo texturas.

Despues (mismas columnas, con los topes de la entrega):

| Fichero | con mip | cuadradas VQ sin | cuadradas 16 bits sin | rectangulares (sin) |
|---|---|---|---|---|
| PALM | 107 / 387 | 11 / 29 | 4 / 2 | 46 / 146 |
| LAKE | 127 / 512 | 2 / 6 | 1 / 0 | 15 / 41 |
| INDU | 64 / 263 | 34 / 93 | 2 / 1 | 59 / 146 |
| ALPI | 103 / 307 | 5 / 29 | 17 / 8 | 38 / 159 |
| SNOW | 46 / 287 | 10 / 52 | 0 | 38 / 164 |
| CLOU | 38 / 373 | 2 / 20 | 0 | 23 / 158 |
| CAST | 82 / 398 | 1 / 1 | 1 / 0 | 6 / 76 |
| GHOS | 24 / 186 | 15 / 42 | 0 | 46 / 276 |
| FORE | 19 / 170 | 20 / 115 | 2 / 1 | 41 / 239 |
| DESE | 67 / 446 | 0 | 0 | 5 / 15 |
| TULI | 107 / 413 | 2 / 20 | 0 | 28 / 130 |
| METR | 121 / 517 | 3 / 6 | 0 | 9 / 40 |
| RUIN | 45 / 415 | 1 / 2 | 0 | 9 / 80 |
| CAPE | 94 / 419 | 3 / 11 | 0 | 28 / 73 |
| IMPE | 15 / 80 | 43 / 168 | 1 / 0 | 97 / 405 |
| BK00 | 20 / 131 | 2 / 8 | 0 | 4 / 60 |
| BK06 | 18 / 116 | 4 / 20 | 0 | 4 / 60 |
| BK09 / BK02 | 13 / 51 | 4 / 20 | 0 | 9 / 80 |

En todo: 2637 texturas nuevas con mipmaps en las 62 etapas y 5169 en las 498 uniones.

**Medida** (Flycast; nada en la consola):

- VRAM de la zona del curso (`tools/vram.py`, COCHE06; Flycast coincide a 1 KB: PALM + BK00 y GHOS + BK09): peor
  combinacion de todos los arboles **944 KB antes y despues** (BK242 + IMPE/49); modo OutRun 928 -> 937 KB (BK09 +
  IMPE). Zona 1312 KB (1280 con COCHE06): 336-368 KB libres en el peor caso; INFARTO 92 KB menos; misiones de COAST 2
  COAST con dos rivales: zona de 1056 KB (nav3.sh, BEAC), 112 KB libres en el peor caso, como antes. Lo demas de la
  VRAM (bufer del TA 2 x 1920 KB, OPB, imagenes, HUD, coche, trafico) no cambia. Una primera version con 780 / 240 KB
  por fichero dejaba 36 KB en esas misiones: por eso el tope por pareja.
- RAM: los huecos no cambian (etapa 8142 KB, union 1584 KB, colision aparte 220 KB: SEG13 pasa a usarlo, 208 KB);
  nav3.sh: "VOZ: VOCES.DAT, 35 muestras", sin "Out of memory".
- GD: SEGS.PAK +3,83 MB, UNIONES.PAK +0,68, UNIONESTA.PAK +0,58, UNIONESR.PAK +0,20, etapas sueltas y descriptores el
  resto: **+6,30 MB (6,01 MiB)** en la clasica (930,6 -> 936,6 MiB) y **lo mismo en el Mod** (sus SEGS / UNIONES*.PAK y
  sus SEG / BK son identicos byte a byte a los de la clasica: 956,9 -> 962,9 MiB). Presupuesto: <= 8 MB en el Mod.
- FPS (`-DPRUEBA_AUTO -DPRUEBA_SIN_TIEMPO -DDIAG_MS`, 150 s desde la etapa, media de las lineas MS sin las 3 primeras),
  antes / despues: PALM 38,9 / 38,9; INDU 39,4 / 39,4; CLOU 37,1 / 37,7; GHOS 38,4 / 38,4; METR 37,3 / 34,7; IMPE 39,7 /
  39,7. Las ventanas no son iguales (la carga tarda distinto: 58-112 medidas antes, 100-163 despues, la de METR llega
  mas lejos en la etapa) y en Flycast los mipmaps no cuestan CPU ni el emulador cuenta la cache de texturas de la
  PowerVR: lo que ganaria la consola (de lejos lee niveles pequenos) no se puede medir aqui. PROBABLE: igual o mejor.
- Capturas (cuadro congelado, mismo binario, datos de antes y de despues; energia de alta frecuencia del tercio lejano,
  media del laplaciano): PALM el agua, la hierba, las rocas y las fachadas lejanas sin ruido (25,2 -> 19,4); METR el
  empedrado de la plaza y las fachadas (22,9 -> 15,8); CLOU los montes y la hierba (29,1 -> 27,9; el grano del asfalto
  sigue: su textura no entro con el tope del GD, con 720 KB si entraba y bajaba a 25,6); GHOS casi igual (25,0 -> 24,9):
  lo que mas centellea alli son los arboles y vallas de recorte y Flycast no aplica mipmaps en la lista de recorte
  (GL_LINEAR, rend/gles/gldraw.cpp); en la consola si (PROBABLE que mejoren, con la cobertura del alfa guardada). De
  cerca, igual (22,9 / 22,4; 21,5 / 21,5).

Codigo: tools/remip.py (nuevo), tools/regenera.py (paso remip), tools/vram.py (curso con CURSOTA.DAT), demo/main.c
(`mip_sesgo` y una linea en compila_cab). Queda: los objetos de las uniones (BOnn), el trafico y los coches sin tocar;
IMPE sin mipmaps nuevos (no cabe sin empeorar el peor caso de la zona); la calzada de CLOU y otras rectangulares 4:1
caras no entran con el tope del GD (con mas sitio en el GD: subir REMIP_TOPE_ETAPA); verificar en la consola el D
adjust y los de recorte.
- (mipmaps en integra, 2026-11-27) remip.py --datos sobre la carpeta integrada (incluye las uniones nuevas de Holly,
  BK498..508): 62 SEG (2637 texturas, +6044 KB de VRAM sumados) y 509 BK (5287 texturas); salida en scratchpad
  modos/mip/dint_out (575 ficheros), +6,03 MB en el GD: clasico con 35,9 MB libres. nav3.sh: "VOZ: VOCES.DAT, 35
  muestras", libre 408K al arrancar, zona del curso en BEAC con 371K libres (igual que sin mipmaps), sin "Out of memory".
## COAST 2 COAST: campeonatos, minijuegos (agente c2c16, 2026-11-27)

Sobre integra (658f329). El tipo de peticion 16 (104 peticiones, casi todas de Holly). Todo en Flycast (nivel E), no en la
consola. Datos de prueba: $S/modos/c2c/dat16 (CAMP.DAT con 'M161' detras de lo de antes, cuyos 30 518 primeros bytes salen
iguales byte a byte; MJHUD.DAT nuevo), carpeta montada con h16/datos2.sh (enlaces a $S/dint + dat16 en modos/c2c/datos16);
scripts en $S/modos/c2c/h16 (los de h6 para este worktree, pantalla :94; py/r16.py lista los registros de +0x7c de cada
peticion del 16, py/ver46.py pinta escenas del banco 0x46). Capturas y registros en $S/modos/c2c/cap16.

### Lo averiguado (SLES-53998, clase 0x29f520 de 0x1860 bytes, vtable 0x423a80)
- Lo que piden los guiones (py/r16.py): subtipo 2 x 22, 1 x 20, 0 x 16, 6 x 15, 3 x 13, 4 x 9, 5 x 9 (no hay subtipo 7: las
  tablas 0x4239f0 / 0x423a10 / 0x423a30 tienen 7 entradas y un 0).
- Control (+0x24, 0x29f7a0). Fase 1: 0x2a0d08 reparte los registros de 0x2c de +0x7c de los parametros ({+0x20 clase 0
  aviso / 1 puerta, +0x24 simbolos / carriles, +0x26 / +0x28 secciones, relativas al inicio si +0x80, +0x2a cuantos: con 2 o
  mas repartidos de +0x26 a +0x28}) en la lista A (+0xc, +0x70) y B (+0x8a4, +0x72) de 50 x 0x2c ({+0..+0x14 seis simbolos,
  +0x18 por pasar, +0x1c el carril bueno, +0x1d el simbolo bueno, +0x24, +0x26 la seccion}); las secciones de las puertas a
  +0x113c (+0x74) y por puerta 0x2a3778 pone en los n + 1 bordes de sus n carriles objetos de la clase 6 del banco (los
  conos; 0x1d68f8 / 0x1d7748, +0x18 = 6); despues el subtipo (0x4239f0). Fases 4..6: el paso del subtipo (0x423a10) y cada
  600 pasos (contador global 0x6213a0) una voz de animo 0x1ac3d8(id, 0x3c, 0): 0x4239c0[k % 2] (subtipos 0 / 1), 0x4239c8[k
  % 3] (2), 0x4239d8[k % 3] (3..6). Fases 2..6: +0x1204 cuenta 0..30 (el vaiven). Sin bono de nota 6 (la fase 5 pasa por el
  paso del subtipo, que solo actua en la 4).
- La puerta (0x29fb28(n, k)): el jugador en k / n < f < (k + 1) / n (f de borde a borde en la seccion de la puerta); bien: el
  medidor + 80 x 1,1 / puertas, +0x3d78 = 1 y 0x114b68(0x2c0002, 5, 3) (el destello); mal: el efecto 0xd4.
- Dibujo de sprites (+0x34, 0x29fe00 -> por subtipo 0x423a30 -> 0x2a0138 por registro entre la seccion del jugador - 2 y
  + 50): filas = +0x24 / columnas en los laterales (fila + 0,5) / filas, 0,1 de fila hacia fuera del centro; columnas: +-2,5 m
  (0x423b88) y altura (0x423bb0: la del medio + 4 m con 3); a 2,5 m del suelo + sin(+0x1204 / 30 x pi) (puertas y los avisos
  de 0, 1, 3, 4); sprite 3D 0x115870 a escala 1,5 de la tabla del subtipo: 0 0x423ae0 (6 frutas), 1 0x423b10 (9 frutas y 12
  animales, cuantos en 0x423ad8), 2 0x423b68 (7 animales, solo en las puertas), 3 numeros con las cifras de 0x423bdc
  (0x2a0838).
- Dibujo 3D (+0x2c, 0x29fd50 -> 0x29fef8 con 1): en cada puerta el modelo de 0x1a7ce0 (tabla 0x368508 por idioma: 0x1ee000b,
  el registro 11 de obj_tour_pmt, un quad con textura) con 0x232e60 a escala (5, 1,5, 2,5). SIN PORTAR.
- Subtipo 2, el sonido (0x2a1ba0 / 0x2a1e00): por puerta el carril bueno rand % n y el animal +8 de los parametros (el azar
  del banco SOUND2..8 de la quinta tanda: SOUNDn solo trae la voz 0x1a7 + n - 2) o rand % 7 (SOUND.PAK, una sola peticion en
  la etapa), el mismo en el aviso k y en los demas carriles otros distintos; al pasar el aviso su voz 0x423c30[animal]
  (0x1a7..0x1ad, prioridad 0xb) y 0x232350 (el 2B003D en +0x3e6c del gestor).
- Subtipo 1, el distinto (0x2a1618 / 0x2a1948): por puerta una clase (rand % 2): el bueno de ella, los demas distintos de la
  otra; el aviso k igual que la puerta (0x2a18a4 copia los simbolos); el aviso se da a 20 secciones (solo 0x232350).
- Subtipo 0, la memoria (0x2a1150 / 0x2a13f8): el aviso k con +0x24 frutas rand % 6; la puerta k con 2 x +0x24 en dos filas,
  la buena (rand % 2) igual que el aviso y la otra con al menos una distinta en su sitio; se mira en 2 (0x29fb28(+0x24 de la
  puerta / +0x24 del aviso k)).
- Subtipo 3, la suma (0x2a2080 / 0x2a24d8): el aviso con un numero por nivel (+0x24: 0x3b5648 pares {de, hasta}; con rand
  < 0,5 de..hasta, si no 1..hasta; nivel 1 negativo con rand % 5 == 0, nivel 2 con rand & 3 == 0); en la puerta el carril
  bueno lleva la suma de los avisos desde la puerta anterior y los demas la suma - 5 + rand % 10, distintos. SIN PORTAR (las
  cifras 0x423bdc y 0x2a0838).
- Subtipo 6, los coches (0x2a3300 / 0x2a3510): el trafico de la peticion (0x1f1a88 / 0x1f20e8 con la tabla de +0x10, +0x64 o
  0x104 los modelos, 0x2a3bd0 con la lista +0x5c, 0x22dac0), el bueno es la cuenta +0x7a (la lleva otro codigo) y los demas
  bueno - 1 + rand % 3; SIN PORTAR -> HECHO (ver "campeonatos, novena tanda (agente c2c9)"). 4 y 5: SIN EMPEZAR.

### HECHO: subtipos 2, 1 y 0 (comun/minijuego.c, -Os)
- tools/conv_camp.py `minijuegos`: el bloque 'M161' detras de CAMP.DAT (las voces 0x423c30, el animo 0x4239c0.., los 34
  sprites de los subtipos 0..2 como grupos MJ_kk, 0x423ad8, 0x423b88 / 0x423bb0 y por peticion sus registros con las
  secciones de la etapa) y MJHUD.DAT (los 23 sprites distintos del banco 0x46, conv_hud.grupo_escena, 149 KB); regenera.py
  paso camp. CAMPHUD.DAT, CAMPOBJ.DAT y VOCESM.DAT salen iguales.
- Enganches: campeonato.c (camp_inicia -> mj_inicia, camp_libera -> mj_libera, case 16 -> mj_paso), main.c (MJHUD.DAT con
  CAMPHUD.DAT, `camp_mundo_` el punto de camp_obj_ con dx, ha_spr3d tambien con mj_spr3d y los grupos MJ_ de MJHUD.DAT).
- Lateral: la fraccion de camp_obj (0 el borde -iz, SUPUESTO de c2c3) para los conos y los sprites, y 1 - c->lat para mirar
  el carril: las dos con el mismo criterio (en cap16/mos_t2a el elefante bueno a la derecha y el coche a la izquierda: "mal").
- DESVIACION / SIMPLIFICADO: sin el cartel 3D de la puerta (0x1ee000b) ni el 2B003D del aviso (0x232350); el destello
  0x114b68(0x2c0002, 5, 3) es el HA_S02 de HAHUD.DAT sobre el coche a escala 0,7 (la de los arcos; SUPUESTO que 5 sea el
  coche); los conos son los de la clase 6 de HAOSO.DAT (salen amarillos y negros); sin las bifurcaciones (0x176e20: dos
  carreteras, no las hay dentro de una etapa de la DC).
- E (`-DPRUEBA_AUTO -DPRUEBA_CAMP=2 -DPRUEBA_CAMP_NODO=7 -DPRUEBA_CAMP_LISTA=1 -DPRUEBA_SIN_TIEMPO`, Alpine al reves, 16/2
  160..225 y 16/1 300..645): "aviso 0 en la seccion 170: animal 6", "VOZ 0x1ad (peticion)", el perro y el elefante sobre sus
  carriles entre los conos (cap16/mos_t2a), el piloto por el otro: "mal", nota 0 (cap16/t2a). Con `-DPRUEBA_MJ` (la puerta
  cuenta como pasada por el carril bueno; el piloto no elige carril): "bien", "peticion 16 hecha: 88 - 0, nota 6" (t2b); el
  16/1 con sus 3 puertas de 3 (la bebida, la naranja y el mono en cap16/mos_t1) "88 - 34, nota 4" y la etapa nota 5. 16/0
  (nodo 2 lista 0, Las Vegas al reves, 135..320, 3 avisos y 3 puertas): las frutas en dos filas de 2 (cap16/mos_t0b), "88 - 0,
  nota 6". NO verificado: un carril bueno elegido conduciendo; la voz no escuchada (Flycast sin audio, por el registro).
- Medidas (DIAG_MS, la misma pasada con y sin `-DDIAG_SIN_MJ`, cap16/medidas): 16/2 39,56 / 39,77 fps (-0,21), 12 muestras;
  16/1 39,39 / 39,57 (-0,18), 41 muestras. Triangulos: los conos 10 de media (57 / 76 como mucho, los modelos del INFARTO);
  los sprites son quads 2D fuera de la escena (como mucho 8 a la vez).
- RAM al arrancar (h16/nav3.sh, MODO_JUEGO=1, sin defines, compilado limpio, datos16; cap16/nav3_flycast.txt): "RAMMAPA
  arranque: libre 408K" (igual que integra), "VOZ: VOCES.DAT, 35 muestras", sin "Out of memory". El estado (1,3 KB) va al
  monton solo en el campeonato; las tablas de MJHUD.DAT tambien, y sus texturas se suben al verse.
- Datos nuevos: CAMP.DAT + 3,1 KB ('M161'), MJHUD.DAT 149 KB.
- Pruebas nuevas: PRUEBA_MJ (la puerta por el carril bueno, y un registro de los sprites cada 60 cuadros), DIAG_SIN_MJ (sin
  sprites ni conos).

### Lo que queda
- 3 (la suma): falta el numero con las cifras 0x423bdc (0x2a0838) y su conversion; la logica esta arriba.
- 6 (los coches): necesita el generador del trafico de la peticion 0x1f1a88 / 0x1f20e8 (sin portar desde la quinta tanda).
- 4 (los regalos, ITEMS: 0x2a26d0 / 0x2a2be0, dibujo 0x2a2d30) y 5 (las ovejas, SHEEP: 0x2a2de8 / 0x2a2e80, dibujo 0x2a3280;
  la fase 3 no dice la voz 0x8e8): sin empezar.
- El cartel 3D de la puerta (0x1ee000b de obj_tour_pmt: un quad, 6,4 KB mas en CAMPOBJ.DAT) y el 2B003D del aviso.
- (c2c16 en integra, 2026-11-27) minijuegos (tipo 16, subtipos 0, 1 y 2) con los mipmaps: nav3.sh "VOZ: VOCES.DAT, 35
  muestras", libre 408K al arrancar, sin "Out of memory"; CAMP.DAT de c2c/dat16 (bloque M161) y MJHUD.DAT (149 KB);
  clasico con 35,7 MB libres. OJO al juntar c2c7: CAMP.DAT tiene que salir de tools/conv_camp.py con los bloques de
  las dos tandas (minijuego.c busca M161 por su firma).
## COAST 2 COAST: campeonatos, septima tanda (agente c2c7, 2026-11-27)

Sobre la rama integra (658f329). Todo en Flycast (nivel E), no en la consola. Datos de prueba: $S/modos/c2c/dat5/CAMP.DAT
(lo de dat4 igual byte a byte y detras 'C241' y 'C221'), carpeta montada con h7/datos2.sh ($S/dint + dat5 en
modos/c2c/datos7); scripts en $S/modos/c2c/h7 (los de h6 para este worktree; corre.sh con FOTOPAT: una captura cuando sale
esa linea; serie7.sh / serie8.sh las medidas). Capturas y registros en $S/modos/c2c/cap7. El tipo 16 lo lleva otro agente.

### HECHO: "Out of memory ... diff 57344" al cambiar de etapa en carrera (NIAG -> LASV, Clarissa nodo 4)
- Causa (DIAG_MALLOC, nuevo: el Makefile envuelve malloc / memalign / realloc / calloc y se apunta cada peticion que
  falla o de 32 KB o mas con quien la pide; cap7/oom1): `decor_reduce` (demo/main.c) al leer SEG22.DAT (LASV, 2955
  nodos) pedia diez tablas temporales de 102 bytes por nodo (301 KB); con 325 KB libres pero troceados, `fr` (59 100 B)
  no cabia: el aviso de KOS y se saltaba la quita de repetidos / edificios tapados de esa etapa (sin "DECOR: SEG22.DAT
  repetidos" en el registro; no se colgaba).
- Arreglo (703f461): `ct` se suelta en cuanto se clasifica y `bb` en cuanto `ub` esta hecha, antes de pedir `fr` (que cabe
  en su hueco): 70 bytes por nodo en el pico, mismo resultado. Sin cambiar la RAM del arranque. No toca gh_* ni las voces.
- E: cap7/oom2 (la misma prueba: sin "Out of memory", "DECOR: SEG22.DAT repetidos 39 (11 fuera)"); otra transicion de
  campeonato, Holly nodo 5 (NEWY -> GRAN -> CAPE, cap7/n5_oom): sin aviso; CONTRARRELOJ (PRUEBA_VARIANTE=7, PALM -> LAKE ->
  CAST, cap7/ta_oom): sin aviso. NO verificado: el fantasma en esa CONTRARRELOJ (el piloto no pasa por el tablero:
  ta_codigo < 0 y gh_empieza no graba ni lee; DESVIACION de la prueba).
- PENDIENTE -> HECHO (ver "campeonatos, octava tanda (agente c2c8)"): al llegar a la META de la ultima etapa del campeonato (Holly nodo 15, cap7/t22a y
  cap7/fin_oom) "Out of memory ... diff 40960": `hudd_sube` (comun/hud.c) no consigue su bufer temporal de 40 KB para leer
  del GD las texturas de un grupo del HUD (vuelve -3: ese grupo no se sube en ese cuadro). PROBABLE que ya estuviera en la
  base (no se ha corrido alli).

### HECHO: tipo 14, los coches con numero se ven (PENDIENTE de c2c6)
- Lo que pasaba: los coches se dibujaban (563 triangulos, sin tope del TA, cap7/t14e) pero en ese tramo de LASV (Holly nodo
  2) los carriles del trafico van por la otra calzada, 20..32 m a la derecha de la ruta (registro C14L), y el piloto de
  pruebas iba por la de la ruta: los numeros (2D, encima de todo) salian sobre el muro. Ademas con la velocidad del trafico
  (35..97 % de la del jugador, 472450) no se les alcanzaba dentro de la peticion.
- Arreglo (7b3d540): los coches del 14 van a la velocidad de la carretera, como los de la mision del INFARTO (PROBABLE;
  no se sabe la que les da el original); PRUEBA_C14V lleva al piloto por los carriles del trafico y hacia el coche con
  numero mas cercano. 3c75848: como mucho 4 vivos a la vez (DESVIACION por el polycount, ver medidas) y los que se quedan
  atras sin abrir se saltan.
- E (cap7/t14m, cap7/t14k congelado con un coche a 44 m): los coches con su numero en la carretera, toques de verdad
  conducidos: "suma 10 (+10) ... suma 10 (-10) ... suma 75 (+5) de 155, medidor 44", nota 3.

### HECHO (SIMPLIFICADO): tipo 24, las fotos (0x2a48a0)
- CAMP.DAT 'C241' (tools/conv_camp.py): por peticion los objetivos de la lista +0x70 {f32 x, y, z en el marco de la etapa
  del PS2, s16 seccion desde, hasta; relativas al inicio con +0x74}: 10 peticiones, 21 objetivos, 384 bytes.
- Lo averiguado: 0x2a60a0 activa (+0x10) el primero cuya seccion inicial se ha pasado (voz 0xf4 / 0xf3 al azar) y lo da
  por perdido al pasar la final; 0x2a4fa8: con 0x2bb0a0 dos veces el angulo entre la camara y el objetivo; si es menor
  que 0,50265 rad la nota de la foto es (1 - angulo / 0,50265) x 10 + 0,5 (+0x2c); el disparo es 0x1b4548 con 0x10 u 8
  segun la configuracion (el mismo boton que lee el 22 para la bocina: aqui X, PROBABLE); efecto 0x174, oculta el HUD y al
  acabar el disparo el medidor + nota / 10 x 80 / (n x 0,9). Nota 6: bono(0,9) como c2c6.
- Portado: todo lo de arriba con el rumbo del coche en vez del de la camara (SIMPLIFICADO), el medidor al disparar (sin la
  espera +0x18) y el objetivo del marco de la etapa al mundo con marco_et (camp_marco). DESVIACION: sin el HUD de la camara
  (0x19d358 / 0x19d318 ocultan piezas del HUD): un visor de cuatro esquinas y una marca sobre el objetivo, amarilla dentro
  del angulo (main.c camp_visor; 12 quads 2D).
- E (Holly nodo 5 lista 1, New York, `-DPRUEBA_C24`: dispara a menos de 10 grados; cap7/t24b): "foto: 10 grados del
  objetivo, nota 7, medidor 62", nota 5 y 44 corazones; la marca cae sobre un edificio de Times Square (cap7/t24b/q_072.png:
  el marco de los objetivos es el bueno, PROBABLE). El segundo 24 (un objetivo a -62 grados todo el rato) sin foto.

### HECHO (SIMPLIFICADO): tipo 22, la bocina (0x29b3d0)
- CAMP.DAT 'C221': por peticion las ventanas de la lista +0x7c (filas de 0x18 {s16 desde, hasta (+ el inicio), f32 +8, +0xc,
  i32 +0x10} hasta -1): 23 peticiones, 99 ventanas, 1 684 bytes.
- Lo averiguado (0x29c058, fase 4): la ventana en curso (+0x24) entre inicio + desde + f8 e inicio + hasta + fc de la
  posicion 0x182b40, o la siguiente (+0x28); al pisar la bocina (0x1b4518) el efecto 0x1bc y la ventana objetivo (+0x2c);
  mientras se pisa, en su ventana y en la carretera el medidor + 80 x avance / +0x14 x 1,25 y fuera la penalizacion x
  0,25; al soltarla 0x81bc. +0x14: PROBABLE la suma de los largos (0x29bfc0). Nota 6: bono(0,9).
- Portado lo de arriba (case 22). DESVIACION: sin los coches de la clase +0x14 (0x29b798 / el generador 0x1f1a88) ni el
  +0x10 de la ventana (+0x1058 del jugador); sin el 0x81bc (el 0x1bc es corto) ni HUD propio.
- E (Holly nodo 15 lista 2, Palm Beach, `-DPRUEBA_C22`: la bocina en cada ventana; cap7/t22a): "bocina: 5 ventanas, 64
  secciones", las tres "peticion 22 hecha: 96 / 96 / 87, nota 6".

### SIN PORTAR: tipo 13 (0x2991b8) -> HECHO (SIMPLIFICADO), ver "campeonatos, octava tanda (agente c2c8)"
- Lo averiguado: la clase (0x44 bytes, vtable 0x423298, control +0x24 = 0x2991b8) en su fase 1 llama a 0x1f1a88 con la
  posicion del jugador (+0x5c del coche, no de los parametros) y copia los 0x44 bytes que devuelve: necesita el generador
  del trafico. Su lista +0x5c del guion existe (registros de 0x14 como el 14, con +0xc = 4 / 3), pero el camino corto de
  c2c6 no basta. Sin empezar el puerto.

### Medidas (Flycast, DIAG_MS; dos pasadas con y sin el elemento, misma prueba)
| elemento | tramo | fps con / sin | triangulos de la escena con / sin | suyos |
|---|---|---|---|---|
| 24, visor | 24/0 160..260 (cap7/m24_con, m24_sin) | 39,13 / 39,15 | 16826 / 16811 | 12 quads 2D |
| 14, todos los coches | 14/0 380..590 (m14_con, m14_sin) | 35,47 / 37,66 | 18662 / 17840 | 563 por coche (LOD 0) |
| 14, tope de 4 | 14/0 380..590 (m14b_con, m14b_sin) | 38,13 / 37,86 | 18244 / 17934 | media +310, max 4 x 563 |
| 22 | sin geometria | | | 0 |
(14: las dos pasadas no conducen igual (los toques frenan): diferencia dentro del ruido con el tope.)

### RAM
- h7/nav3.sh (MODO_JUEGO=1, sin defines, compilado limpio, datos7; cap7/nav3): "VOZ: VOCES.DAT, 35 muestras", "RAMMAPA
  arranque: libre 408K" (igual que integra), sin "Out of memory". Nada nuevo en el monton al arrancar; en el campeonato
  ~0,1 KB mas del estado y 2 KB mas de CAMP.DAT ('C241' + 'C221'). Ningun modulo de cada cuadro a -Os.
- Datos nuevos: solo CAMP.DAT (30 518 -> 32 586 bytes, +2 KB en el GD).
- Pruebas nuevas (solo con defines): DIAG_MALLOC, PRUEBA_C14V, PRUEBA_C24, PRUEBA_C22, DIAG_SIN_C24, DIAG_SIN_C14.

### Lo que queda
- 13 y el generador 0x1f1a88 (arriba); 22 sin sus coches; 24 sin el HUD de la camara ni la espera del disparo.
- El "Out of memory ... diff 40960" de hudd_sube en la META final del campeonato (arriba).
- El fantasma de la CONTRARRELOJ sin probar en esta tanda (arriba).
- (c2c7 en integra, 2026-11-27) CAMP.DAT unico con los bloques de c2c7 (C241, C221) y de c2c16 (M161): conv_camp.py
  de integra sobre la ISO, 35 678 bytes; los 30 518 primeros iguales a los de antes; CAMPHUD, CAMPOBJ, MJHUD y VOCESM
  identicos (scratchpad modos/c2c/dat_int). nav3.sh: "VOZ: VOCES.DAT, 35 muestras", sin "Out of memory", libre **400K**
  al arrancar (justo el minimo: el codigo paso un escalon de 8 KB). PENDIENTE: recuperar RAM antes de anadir mas; el
  "Out of memory ... diff 40960" de hudd_sube en la META de la ultima etapa del campeonato (c2c7).

## COAST 2 COAST: campeonatos, minijuegos 2 (agente c2c16b, 2026-11-27)

Sobre integra (88630c1), continua "campeonatos, minijuegos (agente c2c16)". Todo en Flycast (nivel E), no en la consola.
Datos de prueba: $S/modos/c2c/dat16b (CAMP.DAT con 'M162' detras de 'M161', cuyos 33 610 primeros bytes salen iguales
byte a byte; MJHUD.DAT con 29 sprites mas detras de los 23 de antes: MJ_00..MJ_22 los mismos), carpeta montada con
h16b/datos2.sh (enlaces a $S/dmip + dat16b en modos/c2c/datos16b). CAMPHUD.DAT, CAMPOBJ.DAT y VOCESM.DAT salen iguales.
Scripts en $S/modos/c2c/h16b (los de h16 para este worktree; py/nodos.py: la etapa de cada nodo de los tres arboles,
py/oveja.py: los cuadros de una escena del banco 0x46; serie4.sh: las medidas y nav3). Capturas en $S/modos/c2c/cap16b.

### Lo averiguado (SLES-53998, clase 0x29f520)
- 0x2a0838 (el numero): cifras de |v| (1..3, la de mas peso primero); de lejos (d al coche > 125: f = (d - 125) / 100
  hasta 1) las filas 1 / 2 de la puerta se apartan -/+ 1,5 f x cifras por el eje x de la camara, suben 2 f y el paso de
  las cifras (escala x 1,25) crece x (1 + f) (x (1 + 2 f) si la escala < 2,6); ancho w = cifras (+ 1 con signo) x paso;
  el signo (0x423bdc el +, 0x423bd8 el -) en -w / 4 y la cifra k en paso x (k + signo) - w / 4, todo 0,5 m mas arriba;
  cifras 0x423be0[d] (rojas) si v < 0 y 0x423c08[d] (verdes) si no. Signo: siempre en los avisos, en las puertas solo
  los negativos.
- Por subtipo en 0x2a0138: 3 el aviso 3 m mas arriba a escala 3, la puerta a 3 (2,5 si sim[0] o sim[1] tiene 3
  caracteres); 4 el aviso con los regalos 0x423af8[sim] a 1,3 (5 nada) y la puerta con numeros a 3,5; 5 solo la puerta,
  a 3 (2,5 con >= 100). El vaiven en las puertas y en los avisos de 0, 1, 3 y 4.
- 3 (0x2a2080 / 0x2a24d8), 4 (0x2a26d0 / 0x2a2be0) y 5 (0x2a2de8 / 0x2a2e80): ver la cabecera de comun/minijuego.c.
  El paso de la puerta es el de los demas (0x29fb28 y 80 x 1,1 / puertas); el 4 cuenta +0x76 (puertas pasadas).
- 0x231fe0 (el regalo a contar del 4): objeto 2D del gestor (+0x2650) con 0x423af8[perm[0]] en el panel (+0x3e6c /
  +0x3e70), crece a 0,5 en 0,35 s; 0x2320d8 cuando acaba el panel lo lleva a (180 - [0x374238], -110) a 0,75 y alli se
  queda. 0x2322a0 (el 5, con la fase anterior +4 == 3): zzz 0x460027 en el panel (capa 5, escala 1); 0x232330 lo quita
  en la fase 5. Las ovejas: 0x228e60(0x460028, capa 9, modo 3 -> una vez, (0, -50), 0,55, velocidad 1 + 0,1 x rand % 10):
  la escena (100 cuadros, 78 distintos) ya lleva el salto de izquierda a derecha.
- Las peticiones del tipo 16 solo estan en los guiones "_r" (los del arbol MIX, Req_Course_MIX_PS2). Del 4 no hay
  ninguna en las etapas del arbol MIX (Deep_Lake_r, Snow_Mountain_r, Ghost_Forest_r, Imperial_Avenue_r, Niagara_r,
  San_Francisco_r y Maya_r no estan en el): PROBABLE que el juego no las use, salvo otra forma de elegir la etapa que no
  he visto.

### HECHO: subtipos 3, 4 y 5 (comun/minijuego.c, -Os)
- tools/conv_camp.py: el bloque 'M162' (64 bytes, cada tabla comprobada por el codigo que la lee: 0x3b5648, 0x423bd8 /
  0x423bdc / 0x423be0 / 0x423c08, 0x423af8, 0x460028, 0x460027, 0x188 y 0x423a70) y los 29 sprites nuevos en MJHUD.DAT.
  N del 4 (+0x78 de los parametros) se lee de las peticiones de CAMP.DAT (p[6]); la fase anterior (+4) la lleva
  minijuego.c (sin tocar campeonato.c).
- main.c (ha_spr3d): un sprite con esc < 0 se dibuja en la pantalla en (p[0], p[1]) desde el centro a escala -esc (las
  ovejas, zzz y el regalo del HUD); 16 grupos a la vez (antes 8: las cifras) y 48 sprites; PRUEBA_MJ_TIPO=t (las
  peticiones de la etapa t, 0..59, en la primera etapa: para probar el 4 sobre Alpine al reves).
- E (PRUEBA_AUTO PRUEBA_CAMP=2 PRUEBA_CAMP_NODO=7 PRUEBA_SIN_TIEMPO PRUEBA_MJ, Alpine al reves):
  - 3 (lista 2, 100..340): 12 avisos de nivel 0 (1..5, suma 39), la puerta "37 / 39" (el bueno el 1); cap16b/t3a
    (r_045: el "+3" del aviso), zoom_t3a.png (37 y 39 entre los conos, de lejos separados), "88 - 2, nota 6".
  - 4 (lista 0, PRUEBA_MJ_TIPO=55: Maya al reves 100..265, N = 2): 7 avisos de 3 huecos, la puerta "10 / 9" (9 los
    regalos plateados, contados a mano en el registro); mos_t4a / mos_t4b: los regalos plateados y azules en la
    carretera, el plateado arriba a la derecha en el HUD, "88 - 2, nota 6". Sin PRUEBA_MJ (medida m4_con) el piloto paso
    por el carril bueno por azar: "bien" con el carril de verdad.
  - 5 (lista 0, 160..380): 16 ovejas (efectos 0x188 x 16 y 0x173 / 0x1db / 0x1dc, "VOZ ... (peticion)"), la puerta
    "16 / 12"; mos_t5b: las ovejas saltando por la pantalla, zoom_t5zzz: zzz en el panel; "88 - 2, nota 6".
- Medidas (DIAG_MS, la misma pasada con / sin -DDIAG_SIN_MJ, sin PRUEBA_MJ, cap16b/medidas, h16b/mide.py de "peticion
  16/n" a "peticion 16 hecha"): 3 39,51 / 39,60 fps (-0,09), 35 muestras; 4 39,67 / 39,73 (-0,06), 26; 5 39,45 / 39,63
  (-0,18), 30. Triangulos: los conos 4..7 de media (57 como mucho, los de antes); los sprites son quads 2D (2 triangulos)
  fuera de la escena: como mucho ~12 cifras / 8 regalos / 3 ovejas a la vez.
- RAM al arrancar (h16b/nav3.sh, MODO_JUEGO=1, sin defines, compilado limpio): sobre integra 88630c1 el codigo nuevo
  (+4 KB de .text) pasaba .init (alineado a 8 KB) al bloque siguiente: 408K -> 400K. Recortado minijuego.c (otros() para
  los carriles malos de 1..5, numero() mas corto, el puntero local Q, las probabilidades con enteros y MJLOG: los
  registros de cada aviso / puerta / peticion solo con PRUEBA_AUTO) hasta 0xba030 de .text: .init vuelve a 0x8c0ca000
  (408K, medido sin el merge solo por el tamano, PROBABLE). Tras `git merge integra` (a0b2d19, con c2c7): .text 0xbac70,
  .init en 0x8c0cc000 como integra sola; nav3 (cap16b/nav3_final.txt): "RAMMAPA arranque: en uso 13021K, libre 400K",
  "VOZ: VOCES.DAT, 35 muestras", sin "Out of memory": igual que integra (400K segun el jefe; no medido aqui el tip de
  integra solo). OJO: unos ~3 KB mas de .text en cualquier rama vuelven a saltar .init a 0x8c0ce000 (-8K).
- Tras el merge (CAMP.DAT de tools/conv_camp.py: sus 35 678 primeros bytes iguales a $S/modos/c2c/dat_int/CAMP.DAT, y
  'M162' detras de 'M161'; MJHUD.DAT igual): h16b/serie5.sh repite 16/3 (37 / 39), 16/4 (10 / 9), 16/5 (16 ovejas, 16 /
  12) y 16/2 + 16/1 + 16/0 (prepara1 / prepara2 ahora con otros()): todas "bien" con PRUEBA_MJ, las mismas notas
  (cap16b/t3b, t4b, t5b, t12b, repaso_merge.png).
- Scripts: pgrep -c -f /opt/flycast/build/flycast (el proceso se llama Flycast-rend).
- Datos nuevos: CAMP.DAT + 72 bytes ('M162'), MJHUD.DAT 149 -> 278 KB (+129 KB: 22 cifras de 64x64, 5 regalos de
  128x128, la oveja 128x512 con 78 cuadros y zzz).
- DESVIACION / SUPUESTO: las cifras se separan por el lateral de la carretera, no por el eje x de la camara; el panel de
  la peticion en (176, 78) (donde main.c pinta el rotulo; zoom_t5zzz: zzz cae sobre el coche del panel, PROBABLE bien);
  el regalo del HUD esta 2 s en el panel y va arriba en 0,35 s (el original espera a que acabe el panel, 0x2320d8) y se
  va al acabar la peticion (SUPUESTO: no he visto quien llama al estado 4); +0x160 del gestor (el 5) SIN averiguar; las
  ovejas avanzan con el paso de la logica.

### Lo que queda
- 6 (los coches): necesita el generador del trafico de la peticion 0x1f1a88 / 0x1f20e8 (otro agente).
- Un carril elegido conduciendo (el piloto no elige; en el 4 acerto por azar), la voz y los efectos solo por el
  registro (Flycast sin audio).
- El cartel 3D de la puerta (0x1ee000b) y el 2B003D del aviso, como en c2c16.

## COAST 2 COAST: campeonatos, octava tanda (agente c2c8, 2026-11-27)

Sobre integra (a0b2d19, con el merge de 5cb94a3). Todo en Flycast (nivel E), no en la consola. Datos de prueba:
$S/modos/c2c/dat8 (CAMP.DAT: los 35 750 bytes de dat_int iguales byte a byte y detras 'C131'; MJHUD.DAT: el de integra con
un sprite mas detras, 460006) montados con h8/datos2.sh ($S/dmip + dat_int + dat8 en modos/c2c/datos8). Scripts en
$S/modos/c2c/h8 (los de h7 para este worktree; ab.sh: A/B de fps con datos8; mide8.py: fps y triangulos entre dos lineas;
p13.py: los parametros del 13 en los guiones). Capturas y registros en $S/modos/c2c/cap8.

### HECHO: RAM del arranque (+95 KB)
| que | KB | coste en fps |
|---|---|---|
| `buf_arr` (demo/main.c): el bufer de paso de los textos de 512 x 128 texeles (128 KB de BSS) a 512 x 32; texto_arriba sube la textura de 128 filas en 5 franjas con cada linea entera en la suya (texto_centro ya usaba 16 KB) | 96 | ninguno: solo corre con el marcador de depuracion (`ver`) o sin HUD; con `-DVER=1` las tres lineas salen como antes (cap8/ver_texto/a_003.png) |
| el resto de esta tanda (el 13, el generador, hudd_sube por trozos): +7 KB de codigo, sin escalon | -1 | |
- nav3.sh (h8/nav3.sh, MODO_JUEGO=1, sin defines, compilado limpio, datos8; cap8/nav3): "VOZ: VOCES.DAT, 35 muestras",
  "RAMMAPA arranque: libre 495K" (integra 400K; solo buf_arr: 496K, cap8/nav3_arr), menu 496K, carrera OutRun 786K, sin
  "Out of memory". Sin -Os ni FRIO nuevos: no hizo falta.
- fps (h8/ab.sh, PRUEBA_AUTO, 140 s por etapa, nodos 0 y 1, recortado al mismo s con tabla2.py; cap8/res A0 A1 = integra
  5cb94a3, B0 B1 = c2c8): PALM 38,55 / 38,54, LAKE 39,42 / 39,32, media 38,99 / 38,93 (ruido); 0 "Out of memory", la voz en
  las 4.

### HECHO: "Out of memory ... diff 40960" de hudd_sube en la META final del campeonato
- Causa (c2c7, cap7/fin_oom): hudd_sube (comun/hud.c) lee del GD de una vez todos los sectores de las texturas que suben (el
  grupo del rotulo META, texturas 0..2 de su fichero parcial: 40 KB) en un memalign; en la carrera no hay bufer prestado y la
  cima del monton tenia 28 KB: KallistiOS no podia crecer, el aviso y el grupo sin subir en ese cuadro (se reintentaba).
- Arreglo (54a250a): si no hay bufer prestado que quepa, la lectura pasa de 10 KB y la cima del monton no tiene la lectura +
  32 KB (cabe_seguido, mallinfo), cada textura se lee y se sube por trozos de 8 KB (sube_trozos, con su paleta de 4 / 8 bits
  como sube_tex) con un bufer de 10 KB. Sin RAM nueva al arrancar; la lectura grande de antes sigue igual cuando cabe.
- E: con la RAM de ahora la lectura cabe (Holly nodo 15 lista 2, cap8/fin_h: "MALLOC memalign 40960 -> ..." sin aviso); con
  `-DDIAG_HUD_TROZOS` (siempre por trozos; cap8/fin_tz) "HUD: texturas 0..2 del GD por trozos (40K seguidos sin bufer)" tras el
  GOAL y el rotulo META entero (cap8/fin_tz/mos_meta.png). Metas finales de Clarissa (NEWY, cap8/fin_cla) y Jennifer (CAPE,
  cap8/fin_jen): GOAL sin "Out of memory" (DIAG_MALLOC). NO reproducido el caso de c2c7 tal cual (con la RAM nueva no falta;
  DIAG_RESERVA=98304 tampoco lo reprodujo: la cima de la META no depende de lo que se toma al arrancar).

### HECHO: el generador del trafico de la peticion (0x1f20e8 / 0x1f1a88)
- Lo averiguado: 0x1f20e8(r) solo guarda r en [0x5d5fd8]; 0x1f1a88 (el registro de 0x46 bytes del sitio, PC 4F0030) con
  [0x5d5fd8] != 0 devuelve ese registro en vez de copiar el de la etapa (tabla 0x5d5e78, 5 x 0x46, por la posicion +8) a
  0x5d5fe0 y pisar +0xe..+0x18 (250, 250, 250, 290, 290, 310), +0x1c..+0x26 (270,9..473 x 0,8 en los modos 3 / 4), +0x2a..+0x42
  y +4 (7 / 10). La peticion copia el de la etapa, pone sus 7 modelos en +0xe..+0x1a (+0x64 de sus parametros o un valor fijo:
  0x6e en el 13, 0x96 / 0xa5 en el 11) y lo instala; el aparecedor del trafico (0x1dbba0 / 0x1db6d0) lo lee al abrir cada
  coche. Llamadas: 11 (0x294b6c), 13 (0x2992e0), 14 (0x29cd60), 20 (0x2961b4), 21 (0x2928e0), 0x29ecb8 / 0x29f0b8 y el
  subtipo 6 de los minijuegos (0x2a3398). El 22 NO lo llama (nada en 0x29b3d0..0x29c3ff): sus "coches" no existen (la clase
  +0x14 de c2c7 es la bocina, 0x29b798 lee el boton 0x10 / 8 con 0x1b4518).
- Portado (main.c trafpet_pon; interfaz en comun/campeonato.h `camp_trafpet_t` / `camp_trafpet`): modelo[7] (ids del PS2,
  repartidos en orden a los coches que nacen y a los de la lista de la peticion; los que la DC no tiene se quedan con el del
  trafico), tope de vivos marcados, el LOD mas fino de los coches marcados y una marca para los que nacen; NULL lo quita. Lo
  usan el 13 y el 14 (tope 4, LOD 1, sin marca: sus coches son los de la lista). Para el subtipo 6 de minijuego.c (sin tocar):
  en su fase 1 `camp_trafpet(&r)` con sus modelos y HA_CAMP en marca, en la 5 `camp_trafpet(NULL)`; sus coches son los de
  camp_autos con HA_CAMP.
- DESVIACION / PROBABLE: el 0x6e del 13 no es un modelo que tenga la DC (se quedan los del trafico); el LOD ligero y el tope
  son del port (por el polycount). El 11, 20 y 21 siguen con su camino SIMPLIFICADO (los coches que aparecen son los suyos).

### HECHO (SIMPLIFICADO): tipo 13, la comida (0x2991b8)
- Lo averiguado (0x2991b8 / 0x299dd8 / 0x299e80 / 0x29a0c8 / 0x299b58): el rotulo es "¡Alimenta a la chica!"; los coches son
  la lista +0x5c de los parametros (como el 14: +8 seccion desde el inicio, +0xa carril; 0x22d4c0 pone +0xc en +0xd14 = 2 y
  +0x10 en +0xd18 = la clase). +0x78 clases (3 como mucho) distintas al azar entre 3 (+0x8c), +0x9a fases (3 como mucho) que
  parten [inicio, fin] a partes iguales (+0x7a); el objetivo de la fase k es la clase k. 0x29a0c8 da la clase a los que faltan
  por poner: tras uno del objetivo con rand < 0,15 otro, tras otro con rand < 0,85 el objetivo, si no una al azar; se repite
  0x87 secciones antes de cada cambio. Fase 4: cada coche de la peticion a menos de 4 posiciones por detras y 3 m (0x164f6c), o
  golpeado: el bueno + 80 / p98 al medidor y +0x79 + 1, el otro la penalizacion + 40 / p98; se va (0x19c990); los que se quedan
  atras se van (+0x38). Al pasar el cambio de fase, la voz 0x423220[fila][clase] (0x11c090). Dibujo: sobre cada coche el
  sprite 0x4231d8[fila][clase] del banco 0x46 (comida), x 1,5 el del objetivo y x 0,75 los demas; fila = +8 de los parametros
  (el azar del banco FOOD, 0x22ac00). Nota 6: n = +0x79 > p98 ? (n < p99 ? (n - p98) / (p99 - p98) x 30 : (n - p99) x 5 + 30)
  : 0, + 40 x 0x22f090. p98 / p99 / +0x9a son bytes de +0x98..+0x9a de los parametros.
- tools/conv_camp.py: 'C131' detras de 'M161' (u8 sprite[6][3] = el grupo MJ_kk de MJHUD.DAT, u16 voz[6][3]; por peticion
  coches, clases, p98, p99, fases, +0x64, +0x66 y la lista): 25 peticiones, 14..76 coches, 2 936 bytes; los sprites que el
  16 no tenia van detras en MJHUD.DAT (solo 460006).
- Portado: comun/campeonato.c (case 13, reparte13, camp_coche13, camp_autos, camp_spr3d) y main.c (los coches de la lista
  con los del 14). SIMPLIFICADO: sin el panel del objetivo (0x231fe0) ni el vaiven del sprite (0x136d60); efectos del toque
  0xd6 / 0xd4 (SUPUESTO, los del 14); la voz de la fase 1 por la cola de voces (0x1ac3d8 con 0x3c, 0xb).
- E (Holly nodo 15 lista 0, Palm Beach 13/0 140..565, `-DPRUEBA_C13`: los del objetivo a 1 seccion cuentan como tocados;
  cap8/t13b): "comida: 68 coches, 3 clases (0 2 1), 3 fases, umbrales 20 / 26, fila 5", los dos cambios de objetivo (secciones
  282 y 424), 22 buenas y 2 malas (de verdad: los otros a menos de 3 m), "peticion 13 hecha: 88 - 4, nota 6, 80 + 50
  corazones"; el rotulo "¡Alimenta a la chica!" y los sprites de comida sobre los coches (cap8/t13b/q_056.png,
  z_q_056.png). NO verificado: toques sin PRUEBA_C13 (el piloto no apunta a los coches; sin ella 8 buenas y 7 malas, nota 0).

### Medidas (Flycast, DIAG_MS, la misma pasada; medidas entre "TRAFPET: puesto" y "peticion 13 hecha")
| elemento | tramo | fps con / sin | triangulos de la escena con / sin | suyos |
|---|---|---|---|---|
| 13, coches y sprites (sin `-DDIAG_SIN_C14`) | 13/0 140..565, 49 / 45 muestras (cap8/m13_con, m13_sin) | 37,78 / 38,56 (mediana 38,70 / 39,30) | 18117 / 18005 | 4 coches como mucho (LOD 1) y sus sprites |
| 13 con el LOD 0 (`-DDIAG_TP_LOD0`) | el mismo (m13_l0) | 37,73 | 17960 | |
- Por encima del ~0,3 que se pedia: -0,6 fps de mediana. Un cuadro de 22 fps (la primera subida de los sprites de MJHUD.DAT
  al aparecer, que lee del GD en el cuadro) y +0,35 ms de logica / +0,44 ms de dibujo. El LOD ligero no se nota en los
  triangulos (el LOD 0 solo va a menos de ~70 m y con el tope de 4 apenas hay coches tan cerca): PENDIENTE subir los 3
  sprites de la peticion en su fase 1 y medir con mas pasadas.

### Lo que queda
- 13: el panel del objetivo, el vaiven del sprite, la subida de sus sprites al anunciar la peticion (el tiron de un cuadro) y
  bajar su coste (arriba).
- El generador en el 11, 20 y 21 (siguen SIMPLIFICADOS) y en el subtipo 6 de los minijuegos (la interfaz esta; lo llama quien
  porte el 6). El 22 no tiene coches en el original.
- Pruebas nuevas (solo con defines): PRUEBA_C13, DIAG_TP_LOD0, DIAG_HUD_TROZOS, DIAG_RESERVA. Datos nuevos: CAMP.DAT +2 936 bytes
  ('C131'), MJHUD.DAT +6,3 KB (285 056 -> 291 360).

## COAST 2 COAST: campeonatos, novena tanda (agente c2c9, 2026-11-28)

Sobre integra b41fb8c (con c2c8). Rama local c2c9, sin push. Todo en Flycast (nivel E), no en la consola. Datos de prueba:
$S/modos/c2c/dat9 (CAMP.DAT de `python3 -I tools/conv_camp.py $S/ps2/or2.iso $S/modos/c2c/dat9`: sus 38 686 primeros bytes
iguales byte a byte a los de dat8, C131 incluido, y detras 'M163', 600 bytes; MJHUD.DAT, CAMPHUD.DAT, CAMPOBJ.DAT y VOCESM.DAT
iguales a los de datos8) montados con h9/datos2.sh ($S/dmip + dat_int + dat9 en modos/c2c/datos9). Scripts en $S/modos/c2c/h9
(los de h8 para este worktree; corte.py: fps, cpu, triangulos y partes de PRF entre dos lineas del registro, todos los
registros cortados al mismo s; corrA.sh / mkA9.sh: lo mismo con la base b41fb8c en modos/c2c/A9; p166.py: las peticiones de
un tipo / subtipo en los guiones con su lista +0x5c). Capturas y registros en $S/modos/c2c/cap9.

### HECHO: tipo 13, el tiron y el coste
- De donde salia el coste (Palm Beach 13/0, -DDIAG_MS, PRF y -DDIAG_C13T): ~0,7 ms por cuadro en la parte 2D (l2 1167 us
  frente a 436): ha_spr3d bajaba de la VRAM los grupos que no se veian en el cuadro y los volvia a leer del GD al verse
  otra vez (cada lectura 12..17 ms, -DDIAG_SPR13), y el primero el cuadro de 22 fps; +0,35 ms de logica: los coches de la
  lista (4,8 coches de media, 120 us por coche y paso; coche_coloca 67 us, de ellos 4 coli_altura con 3 en modo 0x400 de
  cerca); los triangulos (+112 de media) apenas.
- Arreglos: (1) camp_spr3d da los 3 sprites de la peticion con cuadro -1 desde el anuncio (fase 1) y ha_spr3d los sube (uno
  cada 16 cuadros) y no los baja hasta que acaba: sin el tiron ni las relecturas; (2) coli_altura guarda cosf / sinf del giro
  del marco (los mismos valores; se calculaban 2 por consulta con la libreria, lenta) y coche_coloca no calcula ca / sa
  cuando el coche se asienta con traf_asienta (afecta a todo el trafico, la misma imagen); (3) los coches de la peticion con
  el LOD ligero (tp.lod = 1, c2c8) se asientan con 0x100 en vez de 0x400 (el original usa 0x400 solo en el LOD 0, que estos
  no dibujan: PROBABLE igual al original para el LOD que se ve).
- HECHO (SIMPLIFICADO): el icono del objetivo (0x2994ac / 0x2998b4 -> 0x231fe0 con 0,5 y 0x2320d8): el sprite de la clase
  pedida crece en el panel (176, 78) y a los 2 s sube a (180, -110) a 0,75, como el regalo del 16/4; otra vez al cambiar
  +0x1d (15 secciones antes del cambio de objetivo). SUPUESTO el sitio y los tiempos (los de c2c16b). Sin el vaiven
  (0x136d60). cap9/t13p/mos_t13p.png: la sandia arriba a la derecha y sobre los coches.

### HECHO: subtipo 6 de los minijuegos, los coches (0x2a3300 / 0x2a3510; comun/minijuego.c, -Os)
- Lo averiguado: 0x2a3300 (fase 1): +0x78 = +8 de los parametros (el azar del banco CARS, 0..2), +0x7a = 0, la puerta 0 con
  el carril bueno rand % n y sus simbolos a -1; 0x2a3bd0(lista +0x5c, +0x78 + 3) pone en +0xb de cada coche de la lista
  (filas de 0x14 hasta +8 < 0 o +0xa < 0) la clase pedida con rand < 0,5 o una de las otras dos (3..5); 0x1f1a88 / 0x1f20e8
  con los modelos +0x64 o 0x104 y 0x22dac0(+0x66 o 0x14). 0x22d0a0 (abre un coche de la lista; el mismo del 13 y el 14):
  con el 16/6 en curso, si su +0xb es +0x78 + 3, +0x7a + 1 (0x22d278) y el modelo 0x415500[plaza % 3] (33, 34, 38); si no
  0x415510 / 0x415530 [plaza % 7] (por la clave de la etapa); +0xd10 = la clase. 0x19c6f0 tine el coche por +0xd10 (3
  rojo, 4 verde, 5 azul: el vector 0x577a48 que lee 0x19c488 para 0x111bb0). 0x2a3510 (fase 4): si el carril bueno de la
  puerta 0 no tiene la cuenta, se la pone y los demas cuenta - 1 + rand % 3 (> 0 y distintos); la puerta como los demas
  (0x29fb28, + 80 x 1,1 / puertas, el destello). Dibujo 0x2a36f8 -> 0x2a0138 / 0x2a0788: solo las puertas, numeros a 3.
- tools/conv_camp.py: 'M163' detras de 'C131' (u8 contados[3] 0x415500, otros[7] 0x415510, otros_1a[7] 0x415530, u32 n y por
  peticion {u16 peticion, u8 coches, 0, s16 +0x64, +0x66, coches x {s16 seccion desde el inicio, s8 carril, 0}}): 15
  peticiones, 5..12 coches. Cada tabla comprobada por el codigo que la lee.
- Portado: prepara6 (fase 1; camp_trafpet(&r) con +0x64 o 0x104, tope 4, LOD 1 y marca 0: los coches de la peticion son los
  de la lista), mj_coche (main.c traf_aparece los abre como los del 13 / 14: 40 secciones por delante, la cuenta al abrirlos,
  el modelo del juego si la DC lo tiene en la marca), la cuenta en la puerta 0 en la fase 4 y camp_trafpet(NULL) en la 5
  (y en mj_libera). campeonato.c: pet11 / camp_autos con el 16/6 (los de atras salen del tope, HA_SUELTO), sin sprites.
- DESVIACION: (1) el tinte: el color del coche x 0,5 + rojo / verde / azul por el color especular (sq_vtx_fe del fundido,
  en el camino rapido de dibuja_indices; con color_mul por dibuja_malla costaba +1,5 ms de dibujo en la tercera peticion:
  -0,6 fps); 0x111bb0 SIN averiguar (PROBABLE que multiplique). (2) Tope de 4 coches de la lista vivos (los del 13 / 14): de 7
  coches salen 4 y la cuenta solo cuenta los que salen (el original los abre todos). (3) El modelo de los demas al azar de
  0x415510 (sin 0x415530 ni plaza % 7); 0x104 no es un modelo de la DC (el trafico de siempre). (4) Sin 0x22dac0.
- E (Industrial Complex al reves, PRUEBA_CAMP=2 PRUEBA_CAMP_NODO=10 PRUEBA_CAMP_LISTA=0, 16/6 135..255, 325..460, 530..610;
  cap9/t6a, t6b): "MJ: coches: 7 en la lista, la clase 5 (azar 2)", "MJ: coche 3 ...: clase 5, modelo 38, cuenta 1", la
  puerta "carril bueno 0 de 2 (1 2)", con PRUEBA_MJ "bien" y "peticion 16 hecha: 88 - 0, nota 6" en las tres; sin
  PRUEBA_MJ (m6_con2) las mismas notas (el piloto acerto por azar). Capturas: t6a/r_080.png (el "1" y el "2" verdes de la
  puerta entre los conos), t6a/r_050.png y t6b/zoom_t6b.png (coches rojo, verde y azul). NO verificado: el carril elegido
  conduciendo; la voz del banco CARS (Flycast sin audio, por el registro).

### HECHO: el generador en el 11, 20 y 21 (campeonato.c trafpet_fijo)
- Lo averiguado: 11 (0x294c50): +0x64 o, sin el, 0xa5 con el subtipo 1 y 0x96 con el 0; 20 (0x296290): +0x64 o 0x104; 21
  (0x2929bc): +0x64 o 0xb4; los tres con 0x22dac0 y 0x1f20e8(0) en la fase 5.
- Portado: camp_trafpet(&r) en la fase 1 con esos modelos (tope 0, LOD 0, marca 0: camp_autos sigue marcando los que nacen
  por delante, como hasta ahora) y NULL en la 5. Ninguno de esos modelos esta en la DC: el trafico sale como antes
  (SIMPLIFICADO: lo que cambia es que el registro esta puesto mientras dura la peticion).
- E: Clarissa nodo 4 lista 1 (cap9/t2021w): "TRAFPET: puesto (modelo 260 ...)" en los dos 20 y "(modelo 180 ...)" en el 21,
  "quitado" en cada fase 5, "peticion 21 hecha: 94 - 0, nota 6, 80 + 90 corazones" (lo mismo que c2c6); BEAC 11/0 con
  PRUEBA_C11 (cap9/t11w): "(modelo 150 ...)", 4 de 10 golpes y "peticion 11 hecha: 32 - 0, nota 3", igual que la base
  (cap9/t11a).

### Medidas (Flycast, DIAG_MS, la misma prueba con y sin; corte.py, cortadas al mismo s)
| elemento | tramo | fps con / sin | cpu (dib + log) con / sin |
|---|---|---|---|
| 13 de c2c8 (base) | PALM 13/0, s 885..3988 | 37,83 / 38,81 (-0,98; min 21,5) | 23,89 / 22,92 |
| 13 ahora (sprites fijos, coli) | el mismo | 38,27 / 38,56 (-0,29; min 33,9) | 23,34 / 22,97 |
| 13 ahora, antes del asiento 0x100 | CAST 13/0, s 2209..3534 | 39,36 / 39,74 (-0,38) | 22,52 / 20,35 |
| 13 final (asiento 0x100) | el mismo | 39,59 / 39,74 (-0,15) | 22,63 / 20,35 |
| 16/6 con color_mul | IND_r 16/6 x 3, s 882..4863 | 38,97 / 39,26 (-0,29; la tercera -0,61) | |
| 16/6 final (tinte especular) | el mismo, s 882..4841 | 39,16 / 39,27 (-0,11; -0,10 / +0,93 / -0,16) | 23,41 / 22,27 |
| 11 / 20 / 21 (generador) | BEAC 11/0, s 506..2297 (base / c2c9) | 38,91 / 39,30 | |
| 20 / 21 (generador) | Clarissa nodo 4 lista 1, s 536..7185 (base / c2c9) | 38,90 / 38,88 | 20,78 / 20,80 |
("sin" del 13: -DDIAG_SIN_C14, sin los coches ni sus sprites; del 16/6: -DDIAG_SIN_MJ6, sin los coches de la lista.) El cuadro
de 22 fps ya no sale (el minimo de la peticion 33,9 en PALM, 37,8 en CAST); las tres subidas fijas al anunciar cuestan un
cuadro cada una (12..17 ms, separadas 16 cuadros). En el 13 queda ~1 ms de logica y ~0,5 ms de dibujo por los 4 coches de
la lista (el original los tiene; tope y LOD ligero de c2c8).

### A/B general y RAM
- h9/ab.sh (PRUEBA_AUTO, 140 s, nodos 0 y 1; A = la base b41fb8c en modos/c2c/A9, B = c2c9; RES=cap9/res tabla2.py A B):
  PALM 38,52 / 38,51 (s comun 2722), LAKE 39,45 / 39,36, media 38,99 / 38,94 (ruido); 4 de 4 con la voz, 0 "Out of memory".
- RAM al arrancar (h9/nav3.sh, MODO_JUEGO=1, sin defines, compilado limpio, datos9; cap9/nav3f): "VOZ: VOCES.DAT, 35
  muestras", "RAMMAPA arranque: libre 495K" (igual que c2c8), menu 496K, carrera OutRun 786K, sin "Out of memory".
- El codigo nuevo (+2,9 KB de .text) pasaba .init de 0x8c0cc000 a 0x8c0ce000 (-8K). Recuperado con -Os (FRIO) en funciones que
  solo corren al cargar o en un suceso: main.c gh_empieza, meta_entra, fin_empieza, ha_meta_carga, sol_carga, traf_ani_lee,
  curso_lee_etapa, meta_lee, repsec_lee, hueco_pcarga, curso_busca, coche_color, sube_inicia, libera_modelo, trafpet_pon,
  ta_meta_final; hud.c hudd_abre, pal_reparte, hudd_grupos_carga; sfx.c sfx_inicia, voces_lee; robot.c robot_inicia,
  robot_band_inicia; or2_fisica.c or2_carga_param; gho.c gho_lee (ninguna del dibujo ni de la fisica por cuadro). .text
  0xbbff0: OJO, quedan 16 bytes hasta el siguiente paso de 8 KB.

### Lo que queda
- 13: el vaiven del sprite (0x136d60); ~0,15..0,3 fps por los 4 coches de la lista (logica y dibujo).
- 16/6: los 7..12 coches de la lista enteros (tope 4), 0x415530 / plaza % 7, 0x22dac0, el tinte exacto de 0x111bb0.
- 11 / 20 / 21: sus modelos (0x96 / 0xa5 / 0x104 / 0xb4) no estan en la DC.
- Pruebas nuevas (solo con defines): DIAG_SPR13 (cada subida de los sprites del 13 con su tiempo), DIAG_SIN_SPR13, DIAG_C13T
  (us por paso de aparecer / mover / choques / gestor del trafico), DIAG_SIN_MJ6. Datos nuevos: CAMP.DAT +600 bytes ('M163').
## Ferrari con menos vertices (agente ferrari, 2026-11-28; DESVIACION del port)
Menos vertices en los coches del jugador, en el menu (COCHEnn.DAT) y en la carrera (COCHEnnL.DAT), con el colapso de
aristas de `tools/conv_coche.py` (simplifica) y los parametros nuevos de la linea de ordenes (--razon, --menu-razon,
--error, --giro, --borde, --cerca; el parche del agente fps2, terminado). Silueta, ruedas, luces, habitaculo de la vista
interior y el bloque CEX2/ANI1 salen como antes: lo que no es simplificable no se toca. La ficha 7 del Mod (taxi) no se
genera nunca.
- Llamada (por ficha k, la del paso `coche<k>` de tools/regenera.py, anadiendo a `--visible`):
  `conv_coche.py <iso> --todos <dir> --fichas k --visible --razon 0.3 --error 0.05 --giro 0.3 --borde 3 --cerca C --menu-razon 0.6`
  con C = 0.02 en las fichas 1, 3, 8, 12 y 13 y C = 0.005 en las 0, 2, 4, 5, 6, 9, 10, 11 y 14.
- Vertices en el fichero (antes = lgdat2): menu -23 % a -30 % en las 14; carrera -16 % a -28 % (0, 1, 3, 5, 8, 11, 12, 13,
  14 por encima del -20 %; 2, 4, 6, 9 y 10 entre -16 % y -20 %: con --cerca 0.005 no dan mas sin quitar el limite de giro).
- A/B en Flycast (PRUEBA_AUTO, coche ficha 0, mismo s): PALM 38,80 -> 38,96 fps, CLOU 36,37 -> 36,80, METR 37,03 -> 36,98;
  triangulos por cuadro 18622 -> 17076, 17598 -> 16039, 15448 -> 13820 (-1550 de media). RAM al arrancar: libre 495K antes, 500K despues;
  VOZ 35 muestras, sin Out of memory. Mejora pequena en fps (+0,2 de media): el coche no es lo que limita.
- PENDIENTE: integrar la llamada en tools/regenera.py y en el programa de Windows (FICHAS_ISO sin la 7).
## fps3: decoracion -15 % y edificios de PALM 32x32 (agente fps3, 2026-11-28; DESVIACION pedida)

Pedido del usuario, para ganar fps en todas las etapas: (1) quitar un 15 % de los arboles, arbustos y vallas
publicitarias de cada etapa, repartido de forma uniforme y determinista, respetando lo que tiene funcion (lo de las
peticiones y campeonatos, la meta, los checkpoints y las bifurcaciones); (2) en PALM, las texturas de los edificios a
32x32, cuadradas y compatibles con los mipmaps y los libros VQ. Sobre la rama integra (b41fb8c). Sin cambios en el
codigo de la demo: todo en los datos, con una herramienta que el programa de Windows rehace desde los discos.

**Herramienta**: `tools/aligera.py --datos entrada/ salida/ trabajo/` (y `--info entrada/ [SEGnn...]`, `--solo`,
`--sin-decor`, `--sin-palm`), sobre una carpeta de datos ya hecha, como `tools/remip.py` (recompone las SEG de SEGS.PAK
y reempaqueta con pak_union.py). `tools/regenera.py` la pasa al final, una vez, detras de remip (marca
`trabajo/aligera.hecho`); `windows/construye.sh` copia `demo/decor_tabla.h` junto a las tools del programa (la tabla de
vegetacion de la demo). Determinista: los mismos nodos y los mismos bytes en dmip (con mipmaps) y en lgmod2 (sin ellos;
mismas cuentas en las 62 etapas).

### 1. Decoracion -15 % (las 62 SEGnn.DAT)
- Que se quita: las mallas del nodo con 0 vertices (la demo no dibuja las de menos de 3), con su version lejana de
  lod_veg.py y, en los arboles, su tronco si va en otro nodo (la misma regla que decor_reduce).
- Vegetacion: los nodos que `decor_reduce` (demo/main.c) toma por vegetacion (decor_tabla.h y sus medidas) y que el
  no quita ya (su hash: 6 de cada 20). El 15 % es de lo que se veia: PALM 399 a la vista, 60 fuera; en el registro
  de la demo "DECOR: 575 de vegetacion" pasa a 515 y lo que quita ella sigue igual (176).
- Vallas publicitarias: nodos con una malla de un anuncio. Los anuncios se reconocen por la firma de su textura
  (16x8 colores, RMS < 15; con o sin mipmaps, tambien en espejo) frente a los patrocinadores de PALM y TULI
  (REF_ANUNCIO, numeros de textura: se rehacen desde el disco). En las 62 etapas salen 285 texturas parecidas, todas
  anuncios mirados a mano (cap/anuncios.png: <= 8,3 de distancia; lo siguiente, 23,8). Solo la valla entera (de 8
  a 64 vertices, <= 16 m de planta, >= 2,5 m de alto: el cartel con su marco), fuera del asfalto (mas de 1 m) y a
  mas de 100 m del principio y del final de la ruta. No: los paneles sueltos de las gradas y del muro de la salida
  de PALM (un cuadrilatero pegado a la estructura: quedaria el hueco).
- Reparto: los candidatos de cada clase ordenados por su punto de la ruta (en el mismo sitio, por un hash de la
  posicion) y fuera uno de cada 1 / 0,15 seguidos: el 15 % exacto y a lo largo de toda la etapa, nunca un tramo.
- Lo que no se toca: carteles de la carretera (flechas, SLOW DOWN, la pancarta CHECK POINT), farolas, publico,
  edificios. Los objetos de las peticiones y de los campeonatos (HAOBJ, CAMPOBJ, INFARTOP...) y la meta (META.DAT) son
  otros ficheros: ninguna valla de las etapas forma parte de una peticion.
- Total (62 etapas): vegetacion 12 306 a la vista, 1 845 fuera (15,0 %); vallas 329, 50 fuera (15,2 %); 56 etapas
  cambian (en SEG04 INDU, SEG28, SEG37 y SEG64 no hay vegetacion en la tabla ni vallas). Por etapa del modo OutRun
  (vegetacion vista / fuera, vallas / fuera): PALM 399/60, 33/5; LAKE 239/36, 6/1; ALPI 547/82, 12/2; CAST 86/13;
  FORE 555/83; DESE 203/30; CLOU 960/144, 18/3; INDU 0; SNOW 284/43; GHOS 146/22; TULI 119/18, 28/4; METR 124/19;
  RUIN 52/8; IMPE 282/42; CAPE 322/48.

### 2. PALM: fachadas a 32x32
- Elegidas a mano en la hoja de las 170 texturas de SEG00 y su uso (cara vertical, alto de los nodos): 30 de ventanas,
  paredes de edificios, rascacielos del fondo y almacenes del puerto (EDIF_PALM: 14, 40, 41, 44, 45, 48, 50, 53, 54,
  62-64, 66, 67, 69, 72, 73, 77, 80-83, 85-91, 124). No: carretera, aceras, empedrado, muros de piedra del paseo,
  rocas, cielo, palmeras (troncos y hojas), plantas colgantes, publico, coches aparcados, letreros de los hoteles
  (43, 47, 52, 56, 75, 76), anuncios, farolas, la noria (95).
- Cada una: el nivel mayor de ahora reducido por promedio (128 o 64 -> 32; 82 era de 128x64) a 32x32 cuadrada con
  mipmaps; VQ con niveles y libro reducido (remip.codifica: 88 entradas, error RMS 1,5-8,2, todas en VQ) y el D adjust
  de remip se queda. En recorte (ARGB1555), el alfa de cada nivel guarda la cobertura. En las uniones del curso de PALM
  no hay ninguna con los mismos bytes (no cambia ninguna BK).
- VRAM de SEG00: 569 -> 482 KB con mipmaps (dmip; -87 KB); en la Mod (lgmod2, sin mipmaps) 451 -> 412 KB.
- Capturas (cuadro congelado `-DDIAG_CONGELA=1200`, PRUEBA_NODO=0, mismo binario, dmip / fps3):
  scratchpad modos/fps3/cap/palm_1200_antes_despues.png (falta la valla de AMD de la fila de la izquierda y alguna
  planta; los edificios mas lisos), palm_1200_edificios_zoom.png (la fachada grande de la derecha pierde casi todo el
  detalle de las ventanas), palm_2400_antes_despues.png (los rascacielos del fondo: casi igual, ya usaban niveles
  pequenos de lejos) y edif_texturas_antes_despues.png (las 30 texturas antes y despues).

### Medida (Flycast; NO emula el coste de relleno de la PowerVR)
`-DPRUEBA_AUTO -DPRUEBA_SIN_TIEMPO -DDIAG_MS -DDIAG_POPPING -DPRUEBA_NODO=n`, 140 s, el mismo binario con dmip (A) y con
dmip + fps3 (B), cada etapa cortada al mismo s (tabla2.py de fps2 y tabla3.py: triangulos y vertices por cuadro, VRAM
libre de la zona al subir la etapa, nodos de la precarga):

| Etapa | s comun | fps A / B | tri/cuadro A / B | vtx/cuadro A / B | cpu ms A / B | VRAM libre zona tras la etapa (K) A / B | nodos de la precarga A / B |
|---|---|---|---|---|---|---|---|
| PALM | 3508 | 38,2 / 38,3 (0,03) | 18595 / 18616 (+0,1 %) | 29027 / 29049 | 23,55 / 23,52 | 742 / 829 | 402 / 426 |
| LAKE | 2116 | 39,4 / 39,4 (0,07) | 19718 / 19939 (+1,1 %) | 31195 / 30638 | 21,51 / 21,42 | 742 / 742 | 158 / 149 |
| ALPI | 2272 | 39,4 / 39,4 (-0,06) | 17104 / 16950 (-0,9 %) | 27074 / 26901 | 20,51 / 20,41 | 742 / 742 | 361 / 347 |
| CAST | 1883 | 39,1 / 39,2 (0,05) | 20498 / 20443 (-0,3 %) | 30939 / 30944 | 23,18 / 23,21 | 769 / 769 | 340 / 340 |
| FORE | 3283 | 39,3 / 39,4 (0,16) | 13831 / 13811 (-0,1 %) | 21895 / 21831 | 19,62 / 19,50 | 720 / 720 | 686 / 649 |
| DESE | 4290 | 39,0 / 39,0 (-0,06) | 16227 / 16302 (+0,5 %) | 24215 / 24370 | 19,39 / 19,41 | 784 / 784 | 253 / 243 |
| CLOU | 7280 | 36,9 / 36,9 (0,00) | 16906 / 17009 (+0,6 %) | 27371 / 27431 | 24,14 / 24,15 | 742 / 742 | 303 / 301 |
| INDU | 1282 | 39,3 / 39,2 (-0,08) | 15112 / 15051 (-0,4 %) | 23975 / 23828 | 20,90 / 20,85 | 742 / 742 | 233 / 233 |
| SNOW | 5878 | 39,5 / 39,5 (-0,08) | 14860 / 14847 (-0,1 %) | 23263 / 23248 | 19,04 / 18,97 | 742 / 742 | 183 / 181 |
| GHOS | 4627 | 37,9 / 37,8 (-0,06) | 18013 / 18055 (+0,2 %) | 26281 / 26332 | 23,55 / 23,52 | 742 / 742 | 496 / 492 |
| TULI | 5174 | 38,8 / 38,8 (-0,01) | 17003 / 16841 (-1,0 %) | 26759 / 26597 | 22,05 / 22,01 | 742 / 742 | 468 / 455 |
| METR | 2555 | 36,8 / 37,0 (0,21) | 16033 / 15974 (-0,4 %) | 26464 / 26199 | 25,11 / 24,81 | 742 / 742 | 255 / 254 |
| RUIN | 5172 | 39,1 / 39,3 (0,20) | 15674 / 16124 (+2,9 %) | 23701 / 24449 | 19,07 / 19,31 | 748 / 748 | 442 / 439 |
| IMPE | 5348 | 39,1 / 39,1 (-0,01) | 13853 / 15053 (+8,7 %) | 21950 / 23678 | 19,19 / 19,74 | 592 / 592 | 237 / 233 |
| CAPE | 4764 | 39,2 / 39,1 (-0,08) | 11386 / 12718 (+11.7 %) | 18413 / 20364 | 17,68 / 18,33 | 742 / 742 | 350 / 341 |
| **Media (15)** | | 38,7 / 38,8 | 16321 / 16515 (+1,2 %) | 25502 / 25724 | | | |

- Lo que se ve: en Flycast **los fps no cambian** (media 38,74 / 38,76; las diferencias por etapa, +-0,2, son ruido de
  la carga de la maquina) y los triangulos por cuadro tampoco bajan (+1,2 % de media). La escena va por el tope del TA
  (reparto de nodos_visibles): lo que se quita lo gasta el reparto en dibujar mas de lo que antes no cabia (PALM: 402
  -> 426 nodos en la precarga, estimado 1198 -> 1286 K; IMPE +8,7 % y CAPE +11,7 % de triangulos: mas distancia de
  dibujo). El popping por el tope, igual (8 px+ por s, sumado en las 15: 205,7 / 207,8; 32 px+ 43,8 / 45,2). En la consola, menos relleno de recorte (hojas y vallas) y texturas de PALM 4-16 veces menores en la
  cache de texturas de la PowerVR: PROBABLE que gane algo en las zonas que van por el relleno; NO medido.
- VRAM: solo PALM, +87 KB libres en la zona tras subir la etapa (742 -> 829 K).
- RAM: el codigo no cambia. nav3.sh (compilacion normal, datos fps3): "RAM: al arrancar ... libre 495K", "VOZ:
  VOCES.DAT, 35 muestras", sin "Out of memory"; en las 30 tiradas de prueba (15 etapas x 2), 35 muestras y ningun "Out of memory"
  (las de PRUEBA_AUTO arrancan con 449K: lo mismo en A y en B, son los defines de la prueba).
- GD: SEGS.PAK 239 342 308 -> 239 397 190 bytes (+54 KB: los trozos de pak_union cambian donde hay mallas a 0);
  ningun SEG crece (el hueco de la etapa no cambia).

### DESVIACIONES respecto al original
- Faltan el 15 % de los arboles, arbustos y palmeras que se veian (ademas del 30 % de la Rev 2.9.2.a) y el 15 % de
  las vallas de patrocinadores (ademas de 1 de cada 3 repetidas iguales de decor_reduce: si al quitar una quedan
  menos de 3 iguales, la demo ya no quita otra de esas).
- PALM: 30 texturas de fachadas a 32x32: de cerca las ventanas y los rascacielos se ven borrosos o lisos.
- No se ha medido en la consola; Flycast no puede mostrar la ganancia (no emula el coste de relleno).

### Datos (solo en scratchpad)
- modos/fps3/dat: SEGS.PAK y 61 descriptores / sueltas SEGnn.DAT (la clasica, desde dmip); modos/fps3/datos: carpeta
  de prueba (enlaces a dmip con lo de dat encima). modos/fps3/mod: lo mismo desde lgmod2 (la Mod; la ficha 7, el taxi,
  no se toca: solo etapas). Registros: modos/fps3/res (aligera_dmip.txt, aligera_mod.txt, A*/B*, nav).

### Queda
- Medir en la consola real (Y en la pausa: PowerVR y TA) PALM y una etapa de bosque (CLOU, FORE) con y sin fps3.
- Si se quiere que Flycast muestre algo: el 15 % solo baja los triangulos con un tope del TA mas alto o fijo.

### Auditoria de las fachadas de PALM ("no tienen detalle", 2026-11-28)
- De las 30 texturas de EDIF_PALM, 17 eran de 64x64 y 13 de 128x128 (o 128x64): 63, 66, 67, 69, 72, 73, 82, 83, 85, 86,
  87, 89 y 90, casi todas rejillas de ventanas de rascacielos con un periodo de 3-4 texeles. Reducidas a 32x32 por
  promedio, cada ventana quedaba por debajo de un texel y la fachada se fundia en una mancha gris (comparativa con
  filtro Lanczos y enfoque: no lo arregla y rompe los recortes con alfa).
- Ahora cada fachada pasa a la mitad de lado (`aligera.lado_de`): las de 64 a 32x32 y las de 128 a 64x64. En Flycast,
  PALM a 1200 m se ve como el original. Las 54 etapas restantes y la decoracion -15 % no cambian (mismos bytes
  recompuestos). La VRAM de PALM baja de 569 a 499 KB.
- Probado y descartado: el D adjust 0,5 en las fachadas (un nivel de mip mas nitido para las paredes vistas de canto).
  En Flycast no cambia nada visible y en la consola real puede centellear. La pared grande de la derecha ya se veia
  algo borrosa en el original: la PowerVR no tiene filtro anisotropico.

## COAST 2 COAST: campeonatos, decima tanda (agente c2c10, 2026-11-28): voces del minijuego de los coches y nivel de SPLASH WAVE REMASTER

Sobre integra 14bf114 (rama c2c10, sin push). Pruebas solo en Flycast (`-DPRUEBA_AUTO -DPRUEBA_CAMP=0/1/2 -DPRUEBA_SIN_TIEMPO`,
datos $S/dmip). Afecta igual a la clasica y al Mod (el codigo de vozmis.c y menu_musica.py es comun).
- FALLO (c2c9): vm_etapa sumaba el azar del 16/6 (rand % 3, 0x22afec) al banco: CARS + 0..2 = CARS, UFO.pak o HOLDLINE.pak
  (vacio). El azar elige la voz y los coches, no el banco (solo FOOD1..6 del 13 son bancos seguidos). Antes: "VOZM: etapa con
  4 bancos (COMMON.pak RANK3.pak HOLDLINE.pak FOOD1.PAK)" y el 16/6 sin las voces 0x18a / 0x15e; ahora CARS.PAK y suenan.
  vm_etapa reescrita sin crecer: .text 0xbbff0 -> 0xbbfd0 (integra iba a 16 bytes del paso de 8 KB de .init; la primera
  version del arreglo lo pasaba: 491K libres al arrancar en vez de 499K). _end igual que integra.
- FALLO (mezcla): MUS70.ADP / MUS71.ADP (SPLASH WAVE REMASTER) a -18,5 / -18,9 dBFS RMS y los temas de carrera del original a
  -11..-16 (mediana -13,6; pista entera decodificada del ADPCM de Yamaha): la cancion sonaba 5 dB por debajo y las voces
  (-14,9 dBFS RMS mientras hablan, medido en Flycast con SDL disk y PRUEBA_MEZCLA=3) la tapaban. menu_musica.py la lleva a
  -13,6 dBFS RMS con pico <= -0,5 dBFS (+5,0 dB). ALL I WANT del Mod (MUS67) ya esta en -11,8: sin cambio.
- Sin cambio en el codigo de volumen (vol_se, musica_nivel, sfx_master) respecto de la Rev 2.9.9: no es una regresion del motor.
