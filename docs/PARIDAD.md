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
| Adherencia por superficie | get_road_mu 500B30, running_resistance 502120, COLI0200 | OK | coli por rueda | ninguna conocida | — | H, E |
| Choques coche-coche y muro, choque fuerte, grua | 4F9450/4F9000/4F8490/4F6FE0/4F76E0/4F7BF0/476040/473F30 (ver Choques entre coches), cbw_coli_wall, 5038D0, PlWrecker | PARCIAL | CHQ1/POS1 de PARAM.DAT, CJM1/SEC1 de TRAFC.DAT, efectos de IGC.PAK en SFX.DAT | coche-coche: ver Choques entre coches (PROVISIONALES alli; golpeados: parpadeo y cierre del original); vibracion del mando de PS2 (0x1d0420) salvo la de la superficie; grua recoloca en el carril mas cercano (no en la fila de poligonos); avance 10 posiciones x m_pos (CONFIRMADO, antes 25 m supuestos); angulo del choque fuerte por nivel de etapa (CORREGIDO 2026-11-11: 45 grados solo en la primera, 25 / 18 en las demas, PS2 0x1994c4: antes siempre 45 y el vuelco casi no salia); tramos de atrapamiento y de desvio 1 -> 5 por etapa HECHOS (2026-11-11, ver Choque fuerte contra el muro); sin el SE 0x43 de los estados 1 y 5 ni el SE por material (pc_collision_material_sound), sin el handicap +DB4; chispas del muro y de los coches hechas (ver Sombras y marcas) | — | H, E |
| Camara de carrera | evento CAMERA 385: 4858D0, 484810, 483110, 4A1680 SetCarCamera, 485590/483CE0/4834A0/483600, 4832F0/484A40, 483C10/484BD0 (PS2 0x178500..0x17b540, 0x1817c0) | PARCIAL | demo/camara.c con las tablas CAM1 de PARAM.DAT (vistas 0x2ccd88, por vista y modelo 0x2cd3d8, por modelo 0x2d2838 del fichero SLES) | vista 1 sin interior (no se dibuja el coche); camara de salida (485250) con el ojo y la mira de la escena de la salida los 120 cuadros de START (ver Escena de la salida); sin vista 0x1D (780258 = 0) ni camara de fin (485830); la de la meta (4853D0) en demo/meta.c (ver Escena de la meta) | ver seccion Camara de carrera | E |
| Trafico (clases, IA, reaccion) | 472090, 472450, 472A50, 47DC10, 4736D0, hit_476040; aparicion 479080 (PS2 0x1dcdd0): 478B90 (0x1dbba0), 4787E0 (0x1db6d0), 4EFB90 (0x1f3818), 4F0030 (0x1f1a88), 46CE30, 478430, 46CC10; cierre 477F60 | PARCIAL | TRAFC.DAT (TRC1, ACL1 y TAP1: tablas de aparicion, velocidad y puntos fijos por etapa); colores (tabla 0x36bc18, modelos 30..43) y materiales en TRAFn.DAT (TCX1) | camino "Lead" de 472450 portado (velocidad por clase y del jugador, ver Choques entre coches: velocidad del trafico de delante) salvo 472320 / 471F90; 478640/46CDA0 en la union simplificados; puntos fijos de coches de carreras y de los modelos 39..43 sin coche | ver Trafico: densidad del original | E |
| Piloto y acompanante | ROB01 / ROB02: PS2 0x201b10 / 0x201c40 / 0x202bf0 y 0x202c58 / 0x202de0; pareja 0x205518 / 0x2055b8; CHR.PS2 (CHR_DR_M00 / S00 / W00 / L00 / G00 / H00, G00_USA), bone.bin, ANIMS.PS2 | PARCIAL | CHRPAR.DAT y CHRppaa.DAT (conv_chr.py --pares), comun/robot.c (con el osage de G00): motor de movimientos, conducir con el volante, codo en la puerta, choque, fuera del asfalto, aburrida, META y TIME OVER (ver Coches: el piloto y la acompanante) | cambio de area, asustada, trompo ni poses pedidas (la meta del piloto, estado 9, hecha: ver Escena de la meta) | - | H, E |
| Rivales | — | FALTA | — | no hay rivales | — | — |
| Carretera, pendientes, bifurcacion | STAGE.PS2, BK.PS2 cs_BK_PALM, COMMON obj_course_obj_bk_palm + obj_OBJ_<ZONA>_BUNKI_TX, descriptores 0xb0, csc_data_2 | OK | conv_tramo/conv_union, CURSO.DAT | arbol entero (15 etapas, 10 bifurcaciones con sus carteles, 5 metas); cielo, niebla, luz y sol de la union como el original (ver Entorno); bifurcaciones = las de PALM (las demas no estan en el disco) | objetos animados de BK (ANIMS) | E |
| Temporizador (inicial y ampliacion) | 44DC70 = PS2 0x176730, 450CC0 = PS2 0x1cee40, 450AC0 = PS2 0x1ce1f8 (-1 por paso en 0x5c5f44, crono +1 en 0x5c5f48); cuenta atras t / 60,2 + 0,34 (4BDA30 = PS2 0x1bfff8); crono con CalcFrame2Time (449B30 = PS2 0x1f6d70: t / 60,2, milesimas truncadas); paso real 59,94 Hz: HUD 0,43 % mas lento que el reloj, como el original; tabla 7D2E98 = PS2 0x3cb708 copiada por 0x176040; k636904 = PS2 0x35a938 | OK | TIM1 y KTE1 de PARAM.DAT | formula y tablas CONFIRMADAS en el ELF; variante 1 (fila 0), preset 0 y dificultad 0 PROBABLES (frontal del PC) | trazar el frontal de PS2 | E |
| Checkpoints, meta, resultados | 44FF20 / 4515B0 (branch_state 7D38E8) con las banderas +0x3C de COLI0200; 4513C0 meta | OK | ampliacion al pasar la pancarta CHECK POINT del principio de la etapa siguiente (ver Checkpoint); meta del original en las 5 etapas finales (z = 0 de la union); voz y efecto de EXTEND TIME, GOAL y TIME OVER (voces.c) | escena de la meta del original (modo 19: guion, camara, coche, piloto, efecto y musica; ver Escena de la meta); millas OutRun, continuar (REINTENTAR / ABANDONAR) y GAME OVER del original (modos 34 / 35 / 27, ver Fin de la carrera); sin la tabla de tiempos de 498670 ni el ranking | — | E |
| Puntuacion modo OutRun | 4F2DF0, 4F2660, 4F2B20, 4B9770 | PARCIAL | main.c pt_* | posicion de carretera = m_pos de CURSO.DAT (CONFIRMADO; antes 2,5 m supuestos, 3,2 veces los puntos por avance); bonus de etapa en el checkpoint | el salto de +64 al cambiar de curso (PENDIENTE) | E |
| Eventos, scripts, progresion, misiones | SCRIPTS.PS2 (Races, RaceAttack, OutrunMiles, Req_*) | FALTA | — | — | — | — |
| Modos de un jugador (INFARTO, CONTRARRELOJ) | 464350 / 464D20 (PS2 0x1ae440 / 0x1a7f28), 44DC70, 479080 | PARCIAL | ver Modos de un jugador | tipos de peticion con objetos, fantasma | — | E |
| Coches del jugador | CARS.PS2 1-32 / 33-63, disposicion 0x3d3ff0; materiales tabla 4/5 del bloque; colores 0x35ad48 (PC 64D120/408C80); mapa de entorno del bloque | PARCIAL | 15 coches montados como 46A560 (port comprobado), con interior, 9320-11195 vertices dibujados; pintura, 8 colores (Y en la seleccion), reflejo y luces (+7C/+80/+84/+60, CEX2) del original; piezas animadas (ruedas, volante, aleron, faros, disco caliente, car+F0 y el cuerpo de la fisica; ANI1) | UV de entorno con la formula esferica clasica (la del microcodigo VU1 sin leer); sin bits de noche/tunel | ver secciones Coches | E |
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
  licencia -> menu principal (UN JUGADOR, MULTIJUGADOR, OUTRUN2SP, RANKINGS, OPCIONES) -> un jugador -> coche -> cambio ->
  musica -> carrera; LICENCIA (X) desde el menu principal y un jugador. MULTIJUGADOR (0x44002A, tabla 0x41C260: en
  linea, LAN), RANKINGS (0x4400A3, 0x41D908: un jugador, OUTRUN2SP, multijugador), OUTRUN2SP, HEART ATTACK, TIME ATTACK y
  SHOWROOM existen con la accion bloqueada (sin red, ranking ni esos modos); desde 2026-11-09 HEART ATTACK y TIME ATTACK
  llevan a su carrera (ver Modos de un jugador). OPCIONES (cabecera 0x44006D, barra
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
  sin pantalla de misiones (frontend_missions / categories), rankings (frontend_rankings, records), showroom,
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
| `TIM_PRESET 0` | 44C020 = PS2 0x176018 por [78024C] = PS2 [0x44350c]: preset 0 / 1 / 2 / 3 -> fila 0 / 1 / 5 / 6 de la tabla base | 0 en el juego de la consola, 1 en OUTRUN2SP (`preset_curso`) | CONFIRMADO (0x176018) | CORREGIDO (2026-11-11, ver OUTRUN2SP) |
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
| Muro a 1 m del limite | CbwColiWall con los puntos de la carroceria | — | PROVISIONAL | SE QUEDA |
| Altura del suelo: perfil a lo ancho de la ruta (mediana de las tiras de dibujo casi horizontales mas cercanas, tools/conv_tramo.py) | GetYPositionProg 43EB60 / GetRoadCond / CalcYPosSpl sobre la colision COLI0200 de la etapa (raiz 0) y de la union (raiz 1) | en TULI (SEG12, s 440-530) el perfil cogia la carretera de debajo del puente: hasta 5,7 m bajo la calzada (el coche pasaba por debajo) | CONFIRMADO | CORREGIDO (2026-11-09: Suelo del original) |
| Orientacion del coche: 4 muestras a 1,6/0,8 m | suspension y contactos de las ruedas (calc_ground_coli_face) | — | PROVISIONAL (solo dibujo; las alturas ya son las de 43EB60; encima van la altura, el cabeceo y el balanceo de la fisica y car+F0: Coches: piezas animadas) | SE QUEDA |
| Escala de puntos por avance (x10, x110) | PS2 0x28F984 / 4F2DF0 (K628244, K5C68E0) | igual | CONFIRMADO | SE QUEDA (ahora con m_pos) |
| Piloto automatico de la demo (DEMO del titulo) | el original pasa la pelicula de MV.PS2 (ahora tambien la demo: video.c) | — | invento propio (solo si faltan las peliculas, o pruebas) | SE QUEDA |
| META/AGOTADO: freno 0,6 y vuelta al titulo a los 600 cuadros | modo 19: la escena de la meta (4871A0 / 486730 / 486EF0, PasPlCar 475720, camara 4853D0) durante 0x4B0 cuadros | — | CONFIRMADO | CORREGIDO en la meta (2026-11-10: Escena de la meta); AGOTADO sigue igual (PROVISIONAL) |
| Aviso de bifurcacion a 450 m de la separacion | 4BB890: BIFURCA con el avance car+0x102C entre 0,65 y 0,72 / 0,70 y 0,75 | — | PROVISIONAL | SE QUEDA |
| Marcas de las ruedas: 4 ruedas, media anchura del neumatico (filas 40/41 de la tabla de parametros), 0,2 m entre tramos, alfa 0,5, +0,05 m, v = longitud x 0,5, anillos 256/64/64, texturas 0x570000..2 (EFECTOS.DAT), clase por la superficie (0xC02 asfalto, 0x8004 tierra/arena, 0x18 hierba) | tire_mark_req 41FB50 / 41FDD0 / 4862C0 del PC; SLES-53998 0x10dcd0 / 0x10df38 / 0x1198f0, tabla 0x344d90, constantes 0x32b390..98 | igual | CONFIRMADO | CORREGIDO (2026-11-08) |
| Marcas en asfalto: las traseras mientras dura el derrape | el original usa la mascara 95AFA8 de PcTireSmokeReq 41C570 (deslizamiento +E0 y agarre +E4 de cada rueda contra 74FF5C) | — | PROVISIONAL (la fisica de la demo no da +E0) | SE QUEDA |
| Humo del derrape (32, vida 1 s, tamano 0,6 + 2,6 t), ahora desde las ruedas traseras de la fisica; tope de 2 pantallas de area por cuadro (limite de la PowerVR, ver Escena de la salida) | PcTireSmoke 41C420 / 41C570 / 41CB30 / 41D1E0 / 41D330 (fuente 4: 512 particulas, textura 0x19 del PC = 0x2e de obj_course_obj_common en PS2) | — | PROVISIONAL | SE QUEDA (pendiente: portar 41C570..41D5C0 y polvo 41DE20..41F4B0; chispas hechas, ver Sombras y marcas) |
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
  de la misma consulta. La ruta queda para el avance, los carriles, los limites y muros (CbwColiWall sigue en la ruta).
- Sin suelo bajo una muestra (fuera de la colision) esa muestra se queda a la altura del coche (el original dejaria
  caer el coche: la demo no tiene salto); sin ficheros COL (datos antiguos), el perfil de antes. `-DSIN_COLI` fuerza
  el perfil (para comparar) y `-DDIAG_SUELO` imprime la altura de la colision frente al perfil.
- Comprobado en host (todas las etapas y uniones): en el centro de cada poligono de calzada el plano y el Hermite
  difieren de media 0,5-1,3 cm (max 17 cm, curvatura); el perfil de la ruta difiere de la colision mas de 0,5 m solo
  en TULI (22 puntos, puente) e IMPE (5 puntos, 0,72 m). Flycast: ver Ultimo cambio verificado.

## Sombras y marcas (2026-11-08)

Peticion del probador: efectos de sombras. Lo que hace el original y como va en la Dreamcast:

- Sombra del coche del jugador (SLES-53998 0x1893f8, el dibujo en carrera, vm = 1): (1) los volumenes de sombra +08 y
  +10 de la disposicion (mallas cerradas en el marco del coche: la parte baja de la carroceria y las piezas altas) con
  0x1138b8: luz en el marco del coche, silueta 0x113bf8 (aristas de 0x24 bytes con las normales de sus dos caras, como
  4211F0 / 421680 / 421740 del PC) y cuadrilateros extruidos por la luz, dos pasadas en una plantilla emulada en el GS
  (estados 0x169ce0) y 0x114150 oscurece lo marcado leyendo el bufer de imagen como textura, antes de dibujar la
  carroceria (la carroceria no recibe su sombra); el PC hace lo mismo con la plantilla de D3D (422550 / 421A90, cuadro
  0x5F000000 de 4220F0). (2) La sombra plana +0C (mancha negra con alfa, 128x32) en la matriz del coche subida 0,01 m
  (0x189904, vector 0x3d3fe0); en el PC 49A650 es un RET. Luz: la 1 del entorno (4082B0(1, 0, 0) + 44): scn_env_sun
  lista 1, (0, 0, -1) girada por la inclinacion y el rumbo (44A430) en el marco de la etapa (CURSO.DAT, `sol`; PALM
  -40 / -60). CONFIRMADO (codigo de PS2 y PC). En la demo: volumen modificador OP_MOD "cheap shadow" (`sombra_dibuja`,
  bloque SVL1 de COCHEnn.DAT) y la +0C como pieza del coche (`tools/conv_coche.py llamadas`). Diferencias: (a)
  oscurecimiento 1 - 0x5F / 255 (el del PC; el factor exacto de 0x114150 no se ha descifrado: PROBABLE); (b) extrusion
  25 m (6282D0 del PC; la PS2 la lleva hasta el borde de la vista), acortada si llegara al plano cercano; (c) envolvente
  convexa de cada parte (la PowerVR cuenta par / impar y el original suma: DREAMCAST.md); (d) recibe la sombra lo
  opaco y lo recortado (la escena y el trafico), no lo translucido. Verificado en E (hoja en el scratchpad,
  sombras/hoja.png). Prueba: `-DSOL_PRUEBA=grados` gira el sol para verla de lado.
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
  microprograma de la VU1 sin leer). En la demo (demo/main.c chispas_paso / chispas_dibuja / muro_chispas): la clase
  del muro se mira en la COLI 0,25 m pasado el limite (la demo lleva el coche como un punto a 1 m del limite) y los
  puntos de la forma son las ruedas (PROVISIONAL: la forma +690 no esta en la demo). RAM: las 500 particulas (14 KB)
  ocupan lo que dejan el bufer del texto del centro y el de la mancha del humo, que ahora pasan por buf_arr.
  PENDIENTE: el destello 0x2C0001 (SPRANI 428320(0x2C0001, 5, 1) en la rueda, solo con cam+34A == 2).
  Verificado en Flycast (scratchpad chq/roce, chq/embiste, chq/vuelco1: `-DPRUEBA_AUTO -DPRUEBA_GUION -DPRUEBA_CHOQUE
  -DCHOQUE_ANG=.. -DCHOQUE_V=.. -DCHOQUE_GIRO=.. -DCHOQUE_CONG=.. -DDIAG_CHISPA`): muro de clase 0x20 y 0x10000 con
  chispas en la rueda del lado del muro, 0x4000 sin ellas; contra el autobus, en la esquina que entra.
- PENDIENTE: humo, polvo, grava y hierba de race_particles (41C570.., 41DE20 grava,
  41E360 hierba; texturas 0x2a grava, 0x2b hierba, 0x2e humo, 0x2f polvo de obj_course_obj_common en
  SLES-53998); sombras de la escena sobre el coche y oscurecimiento en tuneles (bit 0x200 de 4A25F0); reflejos de la
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
  (textura 55, a 95-98 m bajo la salida), sin capa 2 ni mezcla, y debajo el plano del agua (abajo, 2026-11-09). Sin animacion de textura en sus nodos
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
| suspension (nueva) | ColiCar 519830 (car_sus_coli_check, car_sus_bump_push), CalcSuspensionForce 4A1B70, CalcTireLoad 4A1A90, assSpecificAmountOfTireLoad, assDiagonalTireLoad | NUEVO (antes cargas estaticas): muelle 0x688 x 6 del apriete, topes 0x4C0/0x558, estabilizadora 0x720, amortiguador (constantes 0x5c36b4..0x5c36c0); la carga es la fuerza / 6, normalizada a la total y sin diferencia entre diagonales. El cuerpo se reduce a altura, cabeceo y balanceo (angulos pequenos, inercias masa x 0x4c / 0x0e4) sobre un suelo plano; por el / 6 la transferencia es la sexta parte de la fisica real (frenando a 3,3 g: +40 kgf por rueda delantera) | pc_suspension.cpp, pc_collision.cpp |
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
| limites sin clase de muro | +2A8 & 0x10F0020 = 0 (clases 0x4000 y 0x40): solo roce, sin rebote ni choque fuerte | rebote y choque en cualquier limite | igual si la COLI da la clase 0x4000 o 0x40 (0,25 m pasado el limite); con otra clase o sin poligono (clase 1, vacio) sigue como antes (PROVISIONAL: en el original ahi actua PlWrecker fase 1) |
| frenada del roce (+282) | 0x19a200: kmh >> 7; >> 6 en tramo de atrapamiento del choque o del roce (0x3dfed8), >> 3 en los dos; un escalon menos sin clase de muro (>> 8 sin tramos) | kmh >> 7 | igual |
| sonido | rebote: SE 0x4b cada 60 cuadros (+D23); choque fuerte: tabla 0x3dedf8 por estado (0x43 los estados 1 y 5, 0x42 el 2) y despues el SE por material (0x19a4c0, tabla 0x3dee18) | golpe ICS con fuerza inventada | 0x4b y 0x42 (SFX.DAT, conv_sonido.py MURO_SE); PENDIENTE el 0x43 (pitch 0 en la tabla SE de SOUNDCD.IRX: no se sabe como lo toca) y el SE por material |

Sin hacer: handicap +DB4 y asistencia +DF8 (0 en la demo), temporizador de colision +DEC (0x1859c8), efectos
49A650 (vacio en el PC), reacciones nuevas del piloto. Pruebas en Flycast (`-DPRUEBA_AUTO -DPRUEBA_GUION
-DPRUEBA_CHOQUE -DDIAG_CHISPA` con `PRUEBA_NODO`, `CHOQUE_POS`, `CHOQUE_ANG`, `CHOQUE_V`, `CHOQUE_GIRO`, `CHOQUE_CONG`):
nivel 1 a 34 grados y 462 / 468 km/h del modelo: estado 1 (2,5 s, grua 151 cuadros, +2DC hasta 5,1 m, giros hasta
-2 pi); PALM posicion 208 (tramo de desvio) a 50 grados: estado 5 en vez de 1; etapa SEG10 a 249 km/h del modelo:
fuera de tramo (posicion 302) solo rebote, dentro (542) estado 5.

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
PROVISIONAL: solo la caja 0 (camiones con remolque 39..43 no estan en la demo); sin el filtro +300 < K(5E0AC0) ni
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
control no corre 3 cuadros (pausa, menus). SUPUESTO: escala 1,0 (la opcion de CONFIGURACION no esta en la demo; su
valor por defecto sin localizar). PENDIENTE: lo de la superficie (0x1d0138: intensidad por material de cada rueda
+24C..+258, bordes +264/+268, rebote +8 0x100, banderas +8 0x40/0x80/0x2000).

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
- Ampliacion para el modo de la demo (fila 0, preset 0, dificultad 0, un jugador, coche de serie), en cuadros de 60,2
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
  == 0x42) el byte 0x3f99b0 + (meta + preset x 5) x 4 con meta = 450380(8) - 0xA: preset 0 (el curso de OutRun 2 de
  la demo) metas A..E -> escenas 3, 33, 3, 35, 34; preset 1 -> 1..5. 4871A0(escena). Musica 401000(0, pista, 0)
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
- PENDIENTE: interior del coche para la vista 1; camara de salida con el temporizador real (8367BC) y camaras de
  fin (485830; 485470 es la del final de los recreativos, modo 24, que la consola no usa: ver Escena de la meta); desplazamiento del centro de proyeccion +C4/+C8 (cero en las vistas usadas).
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
- PENDIENTE: luces de noche del trafico (+8 / +10); los bits de noche y tunel (SCN_EFC) para +80 y +60 (y
  para abrir los faros escamoteables: coche_ban en demo/main.c se queda a 0); direccion de las ruedas del trafico
  (car+32 del trafico sin localizar) y su car+F0 (4A2910 con car+D34).

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
  enciende la luz +60). La demo no tiene los bits de noche (coche_ban = 0): se quedan cerrados como de dia. Probado
  forzando la noche (GTO: se levantan 55 grados en 1 s).
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
  esqueleto, las manos y el osage (hechos) y la lista de "aire" (hecha). El objeto de 514B60 del PC en el hueso 0x22
  de los personajes 9 / 10 no esta en el dibujo de PS2 (0x206d80 llama al cuerpo 0x2076b8, la cara 0x206e88 y las
  manos 0x206ff8): no se porta. La cara (0x206e88) no la ponen los personajes de carrera (rob_disp_init no la llena).
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
- Ramas de la union (race_area_stage_draw_44e290): registros 0 tronco, 1 rama izquierda, 2 derecha; el original dibuja
  la rama del coche (obj | 1 o | 2 segun 451350). Ahora la demo dibuja las dos hasta elegir rama y despues solo la
  elegida (PROBABLE: antes de elegir, 451350 sin estudiar).
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
  hojas como particulas (SNOW: bandera 6 como PALM). 8 / 0x10 (faros y luces del coche: noche) en GHOS y METR (no
  portado aqui: coche_luces). Tuneles: nada del entorno los oscurece (la luz es fija en la etapa; solo la niebla cambia por posicion); las luces
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
| INFARTO (variante 2) | gestor de peticiones 464350/464D20 (PS2 0x1ae440 / 0x1a7f28), misiones 5A94D8 (PS2 0x3f0b98, 41 x 0x140), fila 1 de los tiempos, voces HC/PERH + HAM\<etapa> | PARCIAL: ver abajo | E |
| CONTRARRELOJ (variante 7) | fila 2 de los tiempos (44DC70: 0 y 7 -> 2; 450CC0 pasa el 7 como 0), sin trafico (479080 y 47EC00 vuelven con la variante 0, 7 u 8), gestor 0x193 / funcion 0x22 (48B8F0..: curso por codigo [656234], fantasma 465F30..4687C0, GHOST.PS2) | PARCIAL: reglas de tiempo y sin trafico; corre el curso entero con eleccion de rama como el OutRun | E |
| COAST 2 COAST (variante 6 misiones, 5 campeonatos) | variante 6: misiones de Races.bin / RaceAssignment.bin (gestor 0x20, evento 0x191: 495B90 / 496A30); variante 5: campeonatos Request_Course_SP / OR2 / MIX con peticiones Req_*.bin (gestor 7F9460, evento 0x190); rivales (47CF40) | FALTA: corre como OutRun (como antes); estudio en COAST 2 COAST abajo | - |
| SALON (showroom) | frontal 4D4230.. | FALTA (accion bloqueada, como antes) | - |
| Dificultad (opcion de la licencia +E0) | difficulty_of de 44DC70 (PS2 0x1cdc98): 0 en el juego de la consola (43F860 = [780260] = 1, lo pone SUMO_FE 0x237e28); la licencia (+E0) solo en el de OUTRUN2SP (2 en la variante 4) | OK (2026-11-11; antes al reves: la licencia en los modos de la consola) | E (log `MODO:`) |
| Multijugador | red / LAN (variantes 3, 4) | no se hace (orden del usuario): la pantalla queda como estaba, con la accion bloqueada | - |

**Rival en las etapas.** No hay rival en OUTRUN, INFARTO ni CONTRARRELOJ: los coches de carreras (sucesos 8.. de
`racer_setup_47cf40`) solo los monta el gestor de misiones de C2C (mission_manager.cpp:136, registro de Races.bin) y
el de la variante 9 (race_variant_owners.cpp:252); 4F2DF0 solo cuenta adelantamientos de rivales si [8421C0] != 0
(4B00D0, el de esas misiones). En el INFARTO hay coches de la mision (45D470, tablas de 45D710, ver abajo) y los de la
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
  4AD8F0). Es terreno del trafico: no se hizo para no inventar un remolque rigido.
- Listas de aterrizaje / salto de la acompanante del modo (PC 5C0FB0 / 5C0FD0 = PS2 0x40ea30 / 0x40ea40): iguales que
  las del OutRun (0x40eb10 / 0x40eb20) y con las mismas cuentas: CONFIRMADO (comun/voces.c). Sus listas de movimientos
  (PS2 0x40ea48 cerrado / 0x40ea88 descapotable, PC 5C0FD8 / 5C1018) SI son otras (0x6003d, 0x60025, 0x240000/1,
  0x60022, 0x6006d, 0x230001...) y CHR.DAT no lleva 0x6003d, 0x6006d ni 0x230001: PENDIENTE (con las reacciones).
- Voces: el juego de voces del modo se cambia en el AICA (VOCESH.DAT 740 KB por VOCES.DAT 207 KB) al empezar la
  carrera, leido en el hueco de la union; banco HAM de la etapa por clave (4 distintos: grupos 0,0,0,1,1,1,2,2,2,1,0,3,
  0,0,3; PROBABLE que el juego lo elija por la clave). La acompanante es el personaje 6 (487F50), sin comentarios de
  zona ni de derrapes (solo variante 1 / 1, 3, 4).
- SIN PORTAR (PENDIENTE): tipos 7, 8, 11, 15, 16, 17, 19, 20, 21, 23, 24, 25 (no empiezan, sin anuncio ni nota), 14 /
  18 (escoltas), la hoja de resultados de la etapa 4603F0, la cara de la acompanante (45BDF0) y las reacciones del robot
  a las peticiones (487D60 / 487D90). La cuenta de
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

**CONTRARRELOJ** verificado en E: `MODO: variante 7, dificultad 0, fila de tiempos 2`, sin un solo coche de trafico,
union y ruta leidas como en el OutRun. GOAL (menu real con el piloto y `-DPRUEBA_SIN_TIEMPO`, 2026-11-09): PALM -> ALPI
(+3000 cuadros) -> FORE (+3060) -> SNOW (+3270) -> IMPE (+3240) -> GOAL, sin trafico en todo el curso. SIN PORTAR: la eleccion de etapa / curso por codigo (48B550: una etapa con su meta GOAL_*_TA o el
curso entero), el fantasma (GHOST.PS2 gc_default_*, formato QHOT/NHOT, modulo 465F30..4687C0) y su record y la
pantalla de resultado 0x50 del modo 25.

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
  los sucesos en el fin del tiempo (modo 20, 440A10: el coche de la demo sigue frenando solo, como antes); la pantalla
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
  deja hasta que START la borra: sin localizar en que estado; PROBABLE el mismo momento).
- Fin: GOAL (modo 19) -> 33 -> 24 (final de recreativa AUTOSCENE \AS\as_E_xx, 485470, fondos END_*_STAGE) -> entrada
  de nombre (modo 26, race_name_entry.cpp, bancos NAME_CMN / NAME5A..E) y rankings de OUTRUN2SP (sp_rankings.cpp);
  sin licencia ni Miles (49CFD0 solo registra con 43F860 != 0). Tiempo agotado: modo 20 -> 0x19 (no 0x22). Pausa:
  REINTENTAR -> 30 (0x249ca8), ABANDONAR -> 27 GAME OVER (0x24a150; banderin 0x34001F). Salida (modo 28, 49AF90): al
  frontal de recreativa (modo 10) salvo [65994C] (-> 32, el menu de la consola); el frontal vuelve al menu de la
  consola con su funcion 0xB cancelada (4BF510: modo 32).

Diferencias de reglas del juego de OUTRUN2SP / preset 1 (todas CONFIRMADAS en el MIPS):
| Que | Original | DC |
|---|---|---|
| Arbol del curso | 44DA00(1, 0) = csc_data_cvt (0x1b6360 en START: preset 1 -> 0x1760c0(1)) | arbol 1 de CURSO.DAT (`curso_pedido`) |
| Tabla de tiempos | 0x176018: preset 1 -> fila 1 de la tabla base (TIM1[1]); k636904 se suma (43F960 = 0) | `TIM_PRESET` = `preset_curso` |
| Dificultad | 0x1cdc98: licencia +E0 (2 en la variante 4); 0 en la consola | `tim_dif` |
| Trafico: modelos | 46CE30 = 0x1db2a0: clase 4 con preset 1 / 3 -> 0x1E, 0x20..0x24 en ciclo de 6 (0x1db49c..0x1db578); clase 0 0x27 / 0x29, 1 / 2 0x28 | `traf_modelo_46ce30` (con el dado de la clase, ver abajo) |
| Trafico: tablas de aparicion | las del descriptor de cada etapa (+0x80 / +0x84) y sus puntos fijos (4EF880) | TAPD.DAT por segmento, copiadas al leer el arbol (antes la demo usaba las del arbol 0 por indice en todos los arboles) |
| Choques entre coches | 0x191d20 / 0x191d70: con preset 1 / 3 umbrales 0x35b9d8 (80, 280: iguales) y restitucion 0x35b9f0 = 1,0 / 0,9 / 0,7 (normal 0x35b9e0: 1,0 / 0,9 / 0,8) | bloque CHA1 de PARAM.DAT, `or2_choques_preset` |
| Banderero de la salida | 0x2046f0: personaje 3 = CHR_AUT04 (recurso 0xC3, tela del recurso 0xC3: 0x100d40) con preset 1 / 3, si no 4 = CHR_AUT04_CVT; hueso de la tela 0x11; movimientos y posiciones de la fila del preset | BAND1.DAT (`conv_chr.py --banderero-sp`), el resto ya iba por preset en robot.c |
| Escena de la meta | 0x3f99b0 + (meta + preset x 5) x 4: preset 1 -> escenas 1..5 | `meta_escena(variante, preset_curso, meta)`, metas A..E = claves 10..14 del arbol 1 |
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
el tipo de infarto.c; 0x1db3c0..0x1db3ec), con la cuenta de la clase que salio (CORREGIDO 2026-11-11: antes siempre 4). Los modelos 39..43 (clases 0..2: camiones) no estan convertidos: se gastan sin coche, como los puntos
fijos de esos modelos.

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
  (26, solo con 450240 = 1: el fin del tiempo): el 25 HECHO (ver abajo), el 24 y el 26 SIN PORTAR (la meta pasa
  directa al 25 y el 25 al 27); en la pausa
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
    -> la 0x50, resultado de la Contrarreloj de 15 etapas: esos presets no salen, SIN PORTAR); A o START (210c58:
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
- SIN PORTAR: el final de recreativa (modo 24: AUTOSCENE \AS\as_E_xx, 485470, escenas 3D con los fondos END_*_STAGE:
  modelo de la etapa del final, el coche, el piloto y la acompanante con movimientos del guion, camara y luces de
  5A6AB8, escena 2D de 638DF0 en la capa 7; race_ending.cpp y race_autoscene.cpp; no cabe junto a lo demas sin portar
  antes los guiones AS.PS2), la entrada de nombre (modo 26: bancos 0x3D RANKING_CVT, 0x36 NAME_CMN, 0x37..0x3B
  NAME5A..E por la meta, los de sprites 0x12 / 0x14 / 0x15 y los textos de las fuentes 2 / 3; PS2 0x1fc560 / 0x1fcd30 /
  0x1fd1e8 / 0x1fd528 = fila 0x15: el nombre es el de la licencia, como el PC, sin teclado de letras) y los rankings de
  OUTRUN2SP (sp_rankings.cpp, frontend_rankings.cpp) con su guardado (PS2 0x106288 / 0x1063d8 en el estado 5.. de la
  entrada de nombre). La meta pasa directa al 25 y el 25 al GAME OVER (27).
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
- Camiones (modelos 39..43, clases 0..2): SIN PORTAR (2026-11-11: la matriz del remolque, car+C90, la calcula 470360
  de race_traffic.cpp con el enganche +C70 / +C7C de cada paso: hace falta portar ese seguimiento, no solo el dibujo). Su registro de 4866C0 trae un segundo descriptor (el remolque)
  en cada LOD, que 4AE0F0 / 4ADBC0 (race_ghost_car.cpp, d.model) dibuja con su propia matriz articulada: conv_trafico.py
  para en "remolque (segundo descriptor) sin portar". Hasta portarlo esos coches se gastan sin coche (como antes).
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
gatillo R cambia de lado: lado 0 rotulo "Recorrido OutRun2SP" -> preset 0, lado 1 "OutRun2" -> preset 1; variante
fila 0 -> 1, 1 -> 2, 2 -> 0) -> solo en la Contrarreloj "Elige configuracion del coche" (3) -> "Elige la musica" (9;
Y sostenido el balance musica / motor 2E0050) -> la carrera (11). B vuelve atras; B en el coche, al menu de la consola
(4C2C60 -> 4BF510: modo 32). La funcion 12 ("Single Play.", 2E004E) no sale con un jugador (el paso 0xB la salta).
La carrera usa lo elegido: coche, color, AT / MT, variante (0 = Contrarreloj), preset -> arbol de CURSO.DAT, tema.
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
  csc_data_cvt_ren / csc_data_2_ren, 0x1760c0). Esos arboles, el resultado de la Contrarreloj de 15 etapas (funcion
  0x50 del suceso 0x188, 4AED50) y sus metas no estan convertidos: DESVIACION, la Dreamcast toma 0 (3 filas) hasta
  portarlos.
- Balance musica / motor (+21, 48B1B0 = PS2 0x1bd020 -> 0x5a2079, leido por 0x1bd030): en las carreras (modos 13 / 0x10,
  0x11f148 / 0x11f570) elige el registro de mezcla 0x3c15d0 + balance x 0x1C (7 floats: 1,0 0,5 0,5 0,8 1,0 0,8 0,7 /
  0,9 0,7 0,7 0,8 1,0 0,8 0,7 / 0,8 1,5 1,0 0,8 1,0 0,8 0,7; fuera de carrera 0x3c15b0: 0,9 0,7 0,7 0,7 1,0 0,8 0,7) que
  se copia a 0x531890: +0x18 x 127 a los comandos 0xA004 / 0xA010 del IOP (0x11f3e0), +4..+0x14 escalan el volumen por
  la clase del sonido (0x11f438, tabla de saltos 0x3c1270, 10 clases), +0 en 0x11f4f4. SIN PORTAR: falta saber que
  clase de 0x3c1270 es cada sonido de la Dreamcast (motor, derrape, voces, musica) para aplicarlo en sfx.c / musica.c.
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
  La demo es la variante 1 (OutRun), preset 0: nada que cambiar con menu_dificultad().
- TRAFC.DAT no se cargaba: la demo leia registros de 52 bytes y son de 48 (sizeof trafc_t); ahora se leen bien las
  clases, cajas, aceleraciones (ACL1) y el modelo para el LOD.
Verificacion (Flycast, `-DPRUEBA_AUTO -DVER=1 -DDIAG_TRAF`, 80 s de carrera con el piloto, `DIAGTRAF` cada 1 s):
PALM antes 11 coches en la ruta todo el rato (media 10,4; dibujados 9,8), despues media 3,2 y como mucho 7 (dibujados
3,0; abre uno cada 15 posiciones, el punto fijo de la 170 con el modelo 31 en el carril 5 y el de la 390); LAKE (58 s
hasta agotar el tiempo) antes media 9,8 y siempre 11, despues media 3,9 y como mucho 10 (los 9 puntos fijos del
principio). PENDIENTE: el camino "Lead" de 472450 (470B30, 471E70, 471F90, 472320: los coches por delante del
siguiente al jugador ajustan su velocidad a la del jugador, con suelos de 250..310 km/h del modelo), 478640 y 46CDA0
en la union, camiones/autobuses 39..43 y coches de carreras de los puntos fijos.

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
  seis. PROBABLE: el codigo del PS2 que lee +0x5C no se ha localizado (en 0x172780 solo esta la tabla 0x3f9b90; la
  0x3f9c98 es el reflejo). No era la PVS, ni la tabla de secciones, ni el recorte por nodos, ni el tope del TA
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
  misma tabla de niebla de la PowerVR (`niebla_pon`): de 0 a NLIM_A = 0,3 (suave) entre 0,65 x nlim_d y nlim_d, donde
  nlim_d sigue (`nlim_paso`, ~0,3 s) a la distancia desde la que el reparto del TA quita nodos (prio_d: ahi aparece
  lo que salta al acercarse) o a lejos (4000 m) si cabe todo. Delante de 0,65 x nlim_d la tabla es la de antes: lo
  cercano y lo medio no cambian. Color: el de la niebla de la etapa; en CAST y FORE (niebla del original negra y sin
  densidad) el del horizonte de su cielo (`horizonte_mira`: media de color del vertice x texel de los vertices del
  grupo 0 entre -5 y 14 grados; tambien es el color al que va la niebla del original en el fundido de la union hacia
  esas etapas, antes hacia el negro). Fuerza x luminancia del horizonte / luminancia de la niebla (hasta 1): en las
  etapas de noche (METR, GHOS: niebla clara, cielo oscuro) casi no hay velo (aclaraba los edificios contra el cielo
  negro). `-DSIN_NIEBLA_LIMITE` lo quita (para comparar).

## Fundido de entrada de lo que aparece (2026-11-11)

DESVIACION pedida por el usuario (no esta en el original): "para evitar el popping quiero probar una cosa: aplicar un
sombreado a la textura que va a cargar para que no aparezca de golpe, como una transicion". EXPERIMENTAL; activo por
defecto, `-DSIN_FUNDIDO_ENTRADA` lo quita (demo/main.c `fe_paso`, `fe_pon`, `fe_cab`).

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
  las mallas del nodo van por `dibuja_malla` con el color del vertice x (1 - k) y el color de desplazamiento
  (especular de la PowerVR, la cabecera de su textura con el bit del especular) = color de la niebla x k:
  textura x color x (1 - k) + niebla x k; la niebla de la tabla sigue encima pixel a pixel, asi que lo lejano lleva la
  suya y con k = 0 la imagen es la de siempre (sin salto al acabar). Se descarto la niebla por vertice (PVR_FOG_VERTEX):
  sustituye a la de la tabla (habria que calcularla por vertice y en los triangulos grandes no es la misma) y al acabar
  el fundido se veria el cambio. Color: el de la niebla de la etapa, o el del horizonte del cielo si es mas oscuro
  (noche de METR y GHOS); lo aditivo de la lista transparente sale del negro.
- Por que 0,4 s: a 200 km/h son ~22 m; lo que aparece a cien metros o mas ha acabado mucho antes de llegar, y por
  debajo de ~0,25 s se ve como un parpadeo y no como una transicion. Es tambien lo que tarda la niebla del limite en
  seguir al corte del reparto.
- Coste (estimado del codigo; la medida en la consola real la hace el usuario): las mallas que se funden van por
  `dibuja_malla` (transformacion en C y a la Store Queue por `envia_tira`) en vez del bucle en ensamblador, unas 2 veces
  mas CPU por vertice, y con 28 bytes por vertice en el TA en vez de 24 (`ta_cabe` lo cuenta con `ta_ofs`: el
  presupuesto se respeta). Solo afecta a lo que aparece en los ultimos 0,4 s, una parte pequena de la etapa. Por cuadro:
  recorrer las mascaras de nodos (unos 100 palabras por modelo). RAM: 1,25 bytes por nodo (~4 KB por etapa, 3094 nodos
  en la mayor). VRAM: nada (la cabecera se copia al enviarla).
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
