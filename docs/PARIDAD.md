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
  `PRUEBA_TRAFICO`, `PRUEBA_CHOQUE`, `PRUEBA_RAMA=0/1` (rama que toma el piloto), `PRUEBA_NODO=k` (etapa de salida, clave del arbol), `PRUEBA_V`, `PRUEBA_FISICA` (resumen de la fisica cada segundo), `PRUEBA_VMU` (guarda la licencia al arrancar y la vuelve a leer), `SALIDA`, `VER=1`, `DIAG_VTX`.
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
| Choques coche-coche y muro, choque fuerte, grua | 4F9450/4F9000/4F8490/4F6FE0/4F76E0/4F7BF0/476040/473F30 (ver Choques entre coches), cbw_coli_wall, 5038D0, PlWrecker | PARCIAL | CHQ1/POS1 de PARAM.DAT, CJM1/SEC1 de TRAFC.DAT, efectos de IGC.PAK en SFX.DAT | coche-coche: ver Choques entre coches (PROVISIONALES alli; golpeados: parpadeo y cierre del original); vibracion del mando de PS2 (0x1d0420) salvo la de la superficie; grua recoloca en el carril mas cercano (no en la fila de poligonos); avance 10 posiciones x m_pos (CONFIRMADO, antes 25 m supuestos) | — | H, E |
| Camara de carrera | evento CAMERA 385: 4858D0, 484810, 483110, 4A1680 SetCarCamera, 485590/483CE0/4834A0/483600, 4832F0/484A40, 483C10/484BD0 (PS2 0x178500..0x17b540, 0x1817c0) | PARCIAL | demo/camara.c con las tablas CAM1 de PARAM.DAT (vistas 0x2ccd88, por vista y modelo 0x2cd3d8, por modelo 0x2d2838 del fichero SLES) | vista 1 sin interior (no se dibuja el coche); camara de salida en los 60 primeros cuadros de la cuenta (SUPUESTO); sin vista 0x1D (780258 = 0) ni camaras de meta/fin | ver seccion Camara de carrera | E |
| Trafico (clases, IA, reaccion) | 472090, 472450, 472A50, 47DC10, 4736D0, hit_476040; aparicion 479080 (PS2 0x1dcdd0): 478B90 (0x1dbba0), 4787E0 (0x1db6d0), 4EFB90 (0x1f3818), 4F0030 (0x1f1a88), 46CE30, 478430, 46CC10; cierre 477F60 | PARCIAL | TRAFC.DAT (TRC1, ACL1 y TAP1: tablas de aparicion, velocidad y puntos fijos por etapa); colores (tabla 0x36bc18, modelos 30..43) y materiales en TRAFn.DAT (TCX1) | camino "Lead" de 472450 (velocidad segun el jugador) sin portar: todos a la velocidad del registro; 478640/46CDA0 en la union simplificados; puntos fijos de coches de carreras y de los modelos 39..43 sin coche | ver Trafico: densidad del original | E |
| Rivales | — | FALTA | — | no hay rivales | — | — |
| Carretera, pendientes, bifurcacion | STAGE.PS2, BK.PS2 cs_BK_PALM, COMMON obj_course_obj_bk_palm + obj_OBJ_<ZONA>_BUNKI_TX, descriptores 0xb0, csc_data_2 | OK | conv_tramo/conv_union, CURSO.DAT | arbol entero (15 etapas, 10 bifurcaciones con sus carteles, 5 metas); cielo, niebla, luz y sol de la union como el original (ver Entorno); bifurcaciones = las de PALM (las demas no estan en el disco) | objetos animados de BK (ANIMS) | E |
| Temporizador (inicial y ampliacion) | 44DC70 = PS2 0x176730, 450CC0 = PS2 0x1cee40, 450AC0 = PS2 0x1ce1f8 (-1 por paso en 0x5c5f44, crono +1 en 0x5c5f48); cuenta atras t / 60,2 + 0,34 (4BDA30 = PS2 0x1bfff8); crono con CalcFrame2Time (449B30 = PS2 0x1f6d70: t / 60,2, milesimas truncadas); paso real 59,94 Hz: HUD 0,43 % mas lento que el reloj, como el original; tabla 7D2E98 = PS2 0x3cb708 copiada por 0x176040; k636904 = PS2 0x35a938 | OK | TIM1 y KTE1 de PARAM.DAT | formula y tablas CONFIRMADAS en el ELF; variante 1 (fila 0), preset 0 y dificultad 0 PROBABLES (frontal del PC) | trazar el frontal de PS2 | E |
| Checkpoints, meta, resultados | 44FF20 / 4515B0 (branch_state 7D38E8) con las banderas +0x3C de COLI0200; 4513C0 meta | OK | ampliacion al pasar la pancarta CHECK POINT del principio de la etapa siguiente (ver Checkpoint); meta del original en las 5 etapas finales (z = 0 de la union); voz y efecto de EXTEND TIME, GOAL y TIME OVER (voces.c) | sin pantalla de resultados ni final del original; sin la voz de GAME OVER (0x154: no hay pantalla de GAME OVER) | — | E |
| Puntuacion modo OutRun | 4F2DF0, 4F2660, 4F2B20, 4B9770 | PARCIAL | main.c pt_* | posicion de carretera = m_pos de CURSO.DAT (CONFIRMADO; antes 2,5 m supuestos, 3,2 veces los puntos por avance); bonus de etapa en el checkpoint | el salto de +64 al cambiar de curso (PENDIENTE) | E |
| Eventos, scripts, progresion, misiones | SCRIPTS.PS2 (Races, RaceAttack, OutrunMiles, Req_*) | FALTA | — | — | — | — |
| Coches del jugador | CARS.PS2 1-32 / 33-63, disposicion 0x3d3ff0; materiales tabla 4/5 del bloque; colores 0x35ad48 (PC 64D120/408C80); mapa de entorno del bloque | PARCIAL | 15 coches montados como 46A560 (port comprobado), con interior, 9320-11195 vertices dibujados; pintura, 8 colores (Y en la seleccion), reflejo y luces (+7C/+80/+84/+60, CEX2) del original; piezas animadas (ruedas, volante, aleron, faros, disco caliente, car+F0 y el cuerpo de la fisica; ANI1) | UV de entorno con la formula esferica clasica (la del microcodigo VU1 sin leer); sin bits de noche/tunel | ver secciones Coches | E |
| Escenarios, texturas, materiales | STAGE.PS2, VQ, scn_env_fog_<zona> | PARCIAL | 15 tramos + 15 uniones; mallas con su material (tabla 5) y la luz del entorno 1 de la zona; niebla, cielo y sol del original (ver Entorno); tiras (ADC), nodos, LOD y planos del original | sin objetos de tramo (obj_course_obj_cs_*); cs_ENV_* no es de la escena (es del reflejo del coche); destello sin consulta de oclusion; luces locales (maya_spl) sin portar | — | E |
| HUD, menus, transiciones | SPRANI/XST, TEXT.PS2 (espanol: SPANISH.bin y XST _sxst) | PARCIAL | HUD.DAT, MENU.DAT; todas las pantallas del menu principal y de un jugador, licencia con VMU (icono, eyecatch y LCD: VMU.DAT, ver VMU), seleccion de coche del original, menu de pausa con AUDIO (ver Menus y Pausa y mezcla) | titulo es un cuadro del video (la pelicula de titulo en bucle detras del rotulo, sin hacer); textos bfont antiguos en la demo automatica y el aviso de rama; rotulos de las filas de opciones (sprites sin localizar) | quitar textos bfont | E |
| Musica y efectos | MUSIC.PS2, SOUND.PS2 ICS/MENU/FE; volumenes 0x11ae38 / 0x100210 / 0x11b600, SOUNDCD.IRX 0x2118 | PARCIAL | musica.c, sfx.c; mezcla del original (ver Pausa y mezcla) | 48 kHz supuesto; sin sonido por superficie ni del choque fuerte | — | E |
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
  SHOWROOM existen con la accion bloqueada (sin red, ranking ni esos modos). OPCIONES (cabecera 0x44006D, barra
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
  licencia +0x28: con una licencia nueva solo Dino y 365, como el original), colores 0x375A1C (-1 siempre, -2 nunca) y
  color inicial 0x375C00; Y = siguiente color desbloqueado (sin sonido, como el PC), A en una bloqueada = sonido 3,
  candado 0x440066 cuadro 5 en (-275, -24). Variante "_t" con X (en el original el bit 0x2000 del mando: boton de la DC
  SUPUESTO); los modelos "_t" no estan convertidos y se ven y corren con el normal (PROVISIONAL). La ficha del coche lleva
  barras de manejo / velocidad / aceleracion, clase y cambios de la propia escena. El coche gira +100/65536 de vuelta por
  paso (4A5B20, CONFIRMADO en el PC); la camara de la seleccion sigue siendo la de antes (la del evento 0x181 sin leer).
  Los movimientos del carrusel guardan un cuadro de cada dos (RAM).
- Ganchos: `menu_gancho(pantalla, evento, valor)` (comun/menu.h) al entrar en cada pantalla y al elegir; `video_titulo()`
  (simbolo debil en demo/main.c) antes del primer titulo.
- VRAM por pantalla (MENU.DAT): COMUN 170 KB, MENU 290, MODO 68, COCHE 512, CAMBIO 256, MUSICA 802, LICENCIA 546,
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
| `TIM_PRESET 0` | 44C020 por [78024C]; 4C3DC0: 0/1 cursos de 5 niveles | 0 (curso OutRun 2, csc_data_2) | PROBABLE | SE QUEDA |
| `TIM_DIF 0` | [7C24C0] = licencia +E0; 4471A0 la pone a 0 | 0 | PROBABLE | SE QUEDA |
| Sin el +60 ni k636904 | 44DC70 = PS2 0x176730: + k636904[clave] si 43F960 == 0 y 456D60 <= 1; + 60 si 48B1A0 != 1 y nivel > 0 | k636904 = {0,0,-60,0,-60,-90,0,-60,-90,-120,0,-60,-90,-120,-150} (PS2 0x35a938, KTE1); +60 con coche de serie | CONFIRMADO (formula y tabla); PROBABLE (coche de serie = 48B1A0 != 1) | CORREGIDO (Flycast: LAKE +3480 = 3420 + 0 + 60; DESE +3450 = 3480 - 90 + 60) |
| Bonus de etapa (4F2B20) al entrar en la union | 450CC0 lo da tras la ampliacion con 7D3948 (cuadros de la etapa, union incluida); 4F2DF0 en la meta (modo 0x13) | en el checkpoint y en la meta | CONFIRMADO | CORREGIDO |
| Cuenta atras: GO a los 240 cuadros de la escena | 8367BC = 180 al entrar en GAME; coche, trafico y tiempo quietos mientras > 60 (4A8100, 450AC0) | 120 cuadros de GAME; GO de la escena 2C000A en el 240 | PROBABLE (no se ha trazado cuando empieza la escena respecto a GAME) | SE QUEDA |
| Camara de salida los 60 primeros cuadros | 485250 mientras 8367BC > 180 (durante START) | duracion de START | PROVISIONAL | SE QUEDA (falta medir START) |
| Trafico: 11 coches al salir en s0 + 130 + i 121 + azar 66 m | 4787E0: sin coches al salir (salvo puntos fijos 478B90); aparecen de uno en uno | ninguno al salir | CONFIRMADO (logica) | CORREGIDO |
| Trafico: reaparece a 550..1100 m si queda 150 m detras | 477F60 cierra con distancia < -2 o > 60 posiciones; 4787E0 abre a 50 posiciones (0x32; 35 en la union pasada la 120) cada max(3, registro +5) posiciones | -2 / +60 / +50 (35) posiciones, separacion del registro (12..20 en el curso) | CONFIRMADO | CORREGIDO (2026-11-08: separacion del registro de aparicion y caso de la union) |
| `NTRAF 11` (todas las plazas llenas) | tope 20 (0x14) x densidad 46CD60 (1 fuera de la red) x registro +4 de 4EFB90 (por tramo), mas los puntos fijos de 478B90 | registro +4: 0..12 en el curso (PALM 5/5/3/0/4, LAKE 12/7/5/2/3) | CONFIRMADO (tablas TAP1) | CORREGIDO (NTRAF 16 = plazas; cuantos abre lo dice el registro) |
| Velocidad del trafico 36-50 m/s al azar | 4F0030 (PS2 0x1f1a88): registro de 0x46 bytes del sitio del coche (+0xC i16 x K5B43A4 = 0,0046142, PS2 0x1dba94), tabla 84BF60 copiada por 4EFAF0 del descriptor +0x84 (PS2) | 100 km/h del modelo (27,7 m/s) en todo el curso | CONFIRMADO | CORREGIDO (la velocidad por clase y el camino "Lead" de 472450 siguen sin portar) |
| Trafico: frenar a 0,9 del de delante a < 10 m y carril libre 15/30 m al cambiar | el original tiene choques entre coches de trafico (4F9450) | ahora portados | CONFIRMADO | CORREGIDO (2026-11-08: quitados; 25 m al nacer = 43EB60, se queda) |
| Trafico: vuelta al centro del carril 0,02 m/cuadro | 6280EC = 0,02 es un tope (B08 no se acerca a 0 mas de eso); lo que lo devuelve es 473D10 (x 0,995 o 0,05 apartandose del jugador) y los cambios de carril | 473D10 portado (PS2 0x1e77e0) | CONFIRMADO | CORREGIDO (2026-11-08) |
| Caja de choque con el jugador: medio largo + 2,3 m, ancho + 1 m | cajas del descriptor 520950 de los dos modelos, esquinas contra bordes (4F9000) | CJM1/TRC1 | CONFIRMADO | CORREGIDO (2026-11-08) |
| Golpe al trafico: deriva segun la posicion lateral relativa, animacion reflejada segun el lado, fuerza del sonido por diferencia de velocidad | 476040: +B58 por +26C del jugador, 4A6EA0 con lado 0, efectos por tabla | — | CONFIRMADO | CORREGIDO (2026-11-08) |
| Muro a 1 m del limite | CbwColiWall con los puntos de la carroceria | — | PROVISIONAL | SE QUEDA |
| Altura y orientacion del coche: 4 muestras a 1,6/0,8 m | suspension y contactos de las ruedas | — | PROVISIONAL (solo dibujo; encima van la altura, el cabeceo y el balanceo de la fisica y car+F0: Coches: piezas animadas) | SE QUEDA |
| Escala de puntos por avance (x10, x110) | PS2 0x28F984 / 4F2DF0 (K628244, K5C68E0) | igual | CONFIRMADO | SE QUEDA (ahora con m_pos) |
| Piloto automatico de la demo (DEMO del titulo) | el original pasa la pelicula de MV.PS2 (ahora tambien la demo: video.c) | — | invento propio (solo si faltan las peliculas, o pruebas) | SE QUEDA |
| META/AGOTADO: freno 0,6 y vuelta al titulo a los 600 cuadros | el original conduce solo hasta el final (camaras 4853D0..) | — | PROVISIONAL | SE QUEDA |
| Aviso de bifurcacion a 450 m de la separacion | 4BB890: BIFURCA con el avance car+0x102C entre 0,65 y 0,72 / 0,70 y 0,75 | — | PROVISIONAL | SE QUEDA |
| Marcas de las ruedas: 4 ruedas, media anchura del neumatico (filas 40/41 de la tabla de parametros), 0,2 m entre tramos, alfa 0,5, +0,05 m, v = longitud x 0,5, anillos 256/64/64, texturas 0x570000..2 (EFECTOS.DAT), clase por la superficie (0xC02 asfalto, 0x8004 tierra/arena, 0x18 hierba) | tire_mark_req 41FB50 / 41FDD0 / 4862C0 del PC; SLES-53998 0x10dcd0 / 0x10df38 / 0x1198f0, tabla 0x344d90, constantes 0x32b390..98 | igual | CONFIRMADO | CORREGIDO (2026-11-08) |
| Marcas en asfalto: las traseras mientras dura el derrape | el original usa la mascara 95AFA8 de PcTireSmokeReq 41C570 (deslizamiento +E0 y agarre +E4 de cada rueda contra 74FF5C) | — | PROVISIONAL (la fisica de la demo no da +E0) | SE QUEDA |
| Humo del derrape (32, vida 1 s, tamano 0,6 + 2,6 t), ahora desde las ruedas traseras de la fisica | PcTireSmoke 41C420 / 41C570 / 41CB30 / 41D1E0 / 41D330 (fuente 4: 512 particulas, textura 0x19 del PC = 0x2e de obj_course_obj_common en PS2) | — | PROVISIONAL | SE QUEDA (pendiente: portar 41C570..41D5C0 y polvo / chispas 41D6B0..41F4B0) |
| Sonido: `ESCALA 3` de los efectos frente a la musica | balance 4279C0 (624238 / 624258) | — | PROVISIONAL (mezcla del AICA) | SE QUEDA |
| Sonido del derrape: deslizamiento / 30 grados | 4260E0 PlaySkid: maximo del deslizamiento de las 4 ruedas (+60/+64.. del cuerpo) | — | PROVISIONAL | SE QUEDA |
| Golpe: fuerza 0,4 + dv/25 y v/40, 20 cuadros entre golpes | 4264C0 PlayWall, 4F65D0 (una vez cada 60 cuadros) | — | PROVISIONAL | SE QUEDA |
| Salto de cuadros (paso de 16683,33 us, hasta 600 por cuadro) | 0x1078c8: pasos = campos de video, 59,94 por segundo a 50 y 60 Hz, sin tope hasta 600 (CONFIRMADO) | antes hasta 3 pasos (por debajo de 20 fps iba lento); TIEMPO TOTAL de resultados dividia por 60 | CONFIRMADO | CORREGIDO (2026-11-09) |
| camara.c: la carrera siguiente empieza con la vista elegida | — | — | PROVISIONAL | SE QUEDA |
| Voz y sonido de EXTEND TIME | 4B93E0 del PC (race_hud_navi.cpp timer_4b93e0) = PS2 0x1be78c: efecto 0x4d (0x84d) y voz 0x16c (0x96c) | — | CONFIRMADO | SE QUEDA (voces.c) |

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
- PENDIENTE: humo, polvo, grava, hierba y chispas de race_particles (41C570.., 41D6B0 chispas al rozar, 41DE20 grava,
  41E360 hierba; texturas 0x29 chispa, 0x2a grava, 0x2b hierba, 0x2e humo, 0x2f polvo de obj_course_obj_common en
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
- Agua: el mar de PALM es opaco con su brillo aditivo (arriba); el lago de LAKE es una malla opaca con su textura
  (registro 8, textura 12, 370 m a 95 m bajo la salida), sin capa 2 ni mezcla. Sin animacion de textura en sus nodos
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
  meta/fin (4853D0, 485470, 485830); desplazamiento del centro de proyeccion +C4/+C8 (cero en las vistas usadas).
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
- PENDIENTE: luces del trafico (+10/+14: noche y freno); los bits de noche y tunel (SCN_EFC) para +80 y +60 (y
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
- PENDIENTE: descifrar las listas por seccion de cs_CS_<zona>_bin (en la PS2; el PC usa otro formato) para no tener
  que recortar; medir en la consola real lo que gasta el TA por tira (`ta_f`) y el tiempo de CPU del cuadro; banderas
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
  RUIN (convertidos y con la misma mecanica). Cifras de RAM, VRAM y cargas en DREAMCAST.md. Texturas de los 14
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
- Sol y destello (CONFIRMADO salvo lo dicho): bandera 2 de +0x3C (todas menos CLOU, GHOS y METR); intensidad que sube o
  baja en 120 cuadros en la union (4AFCC0); direccion -luz 1 con la inclinacion x cam+358; objetos 2, 3..0x10 del banco
  0x57 (SOL.DAT) en el espacio de la camara como 40CBC0 (disco, lejanos, brillo, 11 destellos y brillo de cara al
  sol), tenidos con el ambiente del entorno 0. DIFERENCIAS: sin consulta de oclusion (la PowerVR no la tiene): la parte
  visible va fija en 0,22 (el tope del original), asi que el destello no se apaga detras de lo que tapa el sol; los
  lejanos a 1000 m en vez de 2500 (el cielo esta en la escena); mezcla aditiva (PROBABLE: texturas de fondo negro).
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

## Ultimo cambio verificado

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
