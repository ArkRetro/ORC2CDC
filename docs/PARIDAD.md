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
fisica usa ese paso. PENDIENTE: a cuantos pasos por segundo real corre la version PAL (50 Hz o PAL60); la demo da un
paso por 16683 us (59,94 Hz).

## Como compilar y probar

- `cd demo && make` (KallistiOS en /opt/toolchains/dc/kos). Defines de prueba: `PRUEBA_JUGADOR`, `PRUEBA_AUTO`,
  `PRUEBA_TRAFICO`, `PRUEBA_CHOQUE`, `PRUEBA_RAMA=0/1` (rama que toma el piloto), `PRUEBA_NODO=k` (etapa de salida, clave del arbol), `PRUEBA_V`, `SALIDA`, `VER=1`, `DIAG_VTX`.
- Datos: conversores de tools/ (ver ASSETS.md) a un dir de datos; `tools/mkgdi.py demo/demo.elf dir --datos datos`.
- Niveles de verificacion usados abajo: **C** compila, **H** verificado en host (tools/c/prueba_fisica.c),
  **E** verificado en emulador (Flycast), **R** verificado en Dreamcast real (GDEMU del probador).

## Inventario

Estado: OK (conectado y verificado al nivel indicado), PARCIAL, PROVISIONAL (aproximacion delimitada), FALTA.

| Sistema | Referencia PS2 / PC | Estado DC | Evidencia | Diferencia | Proxima accion | Verificacion |
|---|---|---|---|---|---|---|
| Aceleracion, motor, cambio auto/manual | tabla de parametros 0x2dbc30 (19 col.), par 162 f, freno 256 f; cambio PS2 0x18d530 | OK | or2_fisica.c clonado; columna por modelo MAP1 | ninguna conocida (`ACEL` = 1 por defecto; `-DACEL=1.35f` da la aceleracion de mas que se pidio antes, no es del juego) | — | H, E |
| Direccion, derrape (inicio/mantenimiento/salida) | SlipAngleCtrl / RearGripCtrl / CorneringCtrl; temporizador 40 cuadros | OK | decompilacion + banco | ninguna conocida | trazas de entrada comparables con PS2 | H, E |
| Adherencia por superficie | get_road_mu 500B30, running_resistance 502120, COLI0200 | OK | coli por rueda | ninguna conocida | — | H, E |
| Choques coche-coche y muro, choque fuerte, grua | 4F9450/4F8490/4F6FE0/4F7BF0, cbw_coli_wall, 5038D0, PlWrecker | PARCIAL | CHQ1/POS1 de PARAM.DAT | grua recoloca en el carril mas cercano (no en la fila de poligonos); avance 10 posiciones x m_pos (CONFIRMADO, antes 25 m supuestos) | — | H, E |
| Camara de carrera | evento CAMERA 385: 4858D0, 484810, 483110, 4A1680 SetCarCamera, 485590/483CE0/4834A0/483600, 4832F0/484A40, 483C10/484BD0 (PS2 0x178500..0x17b540, 0x1817c0) | PARCIAL | demo/camara.c con las tablas CAM1 de PARAM.DAT (vistas 0x2ccd88, por vista y modelo 0x2cd3d8, por modelo 0x2d2838 del fichero SLES) | vista 1 sin interior (no se dibuja el coche); camara de salida en los 60 primeros cuadros de la cuenta (SUPUESTO); sin vista 0x1D (780258 = 0) ni camaras de meta/fin | ver seccion Camara de carrera | E |
| Trafico (clases, IA, reaccion) | 472090, 472450, 472A50, 47DC10, 4736D0, hit_476040; aparicion 4787E0, cierre 477F60 | PARCIAL | TRAFC.DAT; colores (tabla 0x36bc18, modelos 30..43) y materiales en TRAFn.DAT (TCX1); aparicion a 50 posiciones y cierre a -2/+60 del original | velocidades y densidad por tramo (registros 0x46 de 4F0030 y 12 bytes de 4EFB90, por etapa) sin clonar; puntos fijos 478B90 | extraer las tablas de aparicion/velocidad por etapa (4EFD20) | E |
| Rivales | — | FALTA | — | no hay rivales | — | — |
| Carretera, pendientes, bifurcacion | STAGE.PS2, BK.PS2 cs_BK_PALM, COMMON obj_course_obj_bk_palm + obj_OBJ_<ZONA>_BUNKI_TX, descriptores 0xb0, csc_data_2 | OK | conv_tramo/conv_union, CURSO.DAT | arbol entero (15 etapas, 10 bifurcaciones con sus carteles, 5 metas); cielo de la union = el de la etapa que se deja, cambia al llegar a la siguiente (sin mezcla); bifurcaciones = las de PALM (las demas no estan en el disco) | objetos animados de BK (ANIMS), sol | E |
| Temporizador (inicial y ampliacion) | 44DC70 = PS2 0x176730, 450CC0 = PS2 0x1cee40, 450AC0; tabla 7D2E98 = PS2 0x3cb708 copiada por 0x176040; k636904 = PS2 0x35a938 | OK | TIM1 y KTE1 de PARAM.DAT | formula y tablas CONFIRMADAS en el ELF; variante 1 (fila 0), preset 0 y dificultad 0 PROBABLES (frontal del PC) | trazar el frontal de PS2 | E |
| Checkpoints, meta, resultados | 44FF20 / 4515B0 (branch_state 7D38E8) con las banderas +0x3C de COLI0200; 4513C0 meta | OK | ampliacion al pasar la pancarta CHECK POINT del principio de la etapa siguiente (ver Checkpoint); meta del original en las 5 etapas finales (z = 0 de la union) | sin pantalla de resultados ni final del original; sin voz | — | E |
| Puntuacion modo OutRun | 4F2DF0, 4F2660, 4F2B20, 4B9770 | PARCIAL | main.c pt_* | posicion de carretera = m_pos de CURSO.DAT (CONFIRMADO; antes 2,5 m supuestos, 3,2 veces los puntos por avance); bonus de etapa en el checkpoint | el salto de +64 al cambiar de curso (PENDIENTE) | E |
| Eventos, scripts, progresion, misiones | SCRIPTS.PS2 (Races, RaceAttack, OutrunMiles, Req_*) | FALTA | — | — | — | — |
| Coches del jugador | CARS.PS2 1-32 / 33-63, disposicion 0x3d3ff0; materiales tabla 4/5 del bloque; colores 0x35ad48 (PC 64D120/408C80); mapa de entorno del bloque | PARCIAL | 15 coches montados como 46A560 (port comprobado), con interior, 9320-11195 vertices dibujados; pintura, 8 colores (Y en la seleccion), reflejo y luces (+7C/+80/+84/+60, CEX2) del original | UV de entorno con la formula esferica clasica (la del microcodigo VU1 sin leer); piezas sin animar; sin bits de noche/tunel | ver secciones Coches | E |
| Escenarios, texturas, materiales | STAGE.PS2, VQ, scn_env_fog_<zona> | PARCIAL | 15 tramos + 15 uniones; mallas sin textura con su material (tabla 5); niebla del original | sin objetos de tramo (obj_course_obj_cs_*), sol (cs_ENV_* no es de la escena: es del reflejo del coche); niebla mezclada por distancia en la union (el original por tiempo) | — | E |
| HUD, menus, transiciones | SPRANI/XST, TEXT.PS2 | PARCIAL | HUD.DAT, MENU.DAT | titulo es un cuadro del video; textos bfont antiguos en la demo automatica y el aviso de rama | quitar textos bfont | E |
| Musica y efectos | MUSIC.PS2, SOUND.PS2 ICS/MENU/FE | PARCIAL | musica.c, sfx.c | 48 kHz supuesto; sin voces, sin sonido por superficie ni del choque fuerte | — | E |
| Cargas y estabilidad | — | PARCIAL | carga en segundo plano, mutex del GD | — | medir en GDEMU | E |
| Rendimiento / TA en consola real | — | PARCIAL | tope global del TA (DREAMCAST.md) | primer cuelgue medido en la consola (VB lleno a 24656 vtx) | esperar prueba R de la demo 14+ | E (R pendiente) |

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
| Trafico: reaparece a 550..1100 m si queda 150 m detras | 477F60 cierra con distancia < -2 o > 60 posiciones; 4787E0 abre a 50 posiciones (0x32; 35 en la union pasada la 120) cada max(3, registro +5) posiciones | -2 / +60 / +50 posiciones | CONFIRMADO; la separacion del registro (+5) y el 35 de la union PROVISIONALES | CORREGIDO (separacion 3, sin el caso de la union) |
| `NTRAF 11` | tope 20 (0x14) x densidad 46CD60 x registro +4 de 4EFB90 (por tramo) | depende del tramo | PROVISIONAL (tablas de aparicion por etapa no extraidas; 11 es el tope de dibujo medido en la Dreamcast) | SE QUEDA |
| Velocidad del trafico 36-50 m/s al azar | 4F0030: registro de 0x46 bytes del tramo (+0xC i16 x K5B43A4), copiado por 4EFD20 de tablas del ejecutable por etapa | por tramo y clase | PROVISIONAL | SE QUEDA (falta localizar las tablas 84BF60 en PS2) |
| Trafico: frenar a 0,9 del de delante a < 10 m, carril libre 15/30 m y 25 m al nacer | el original tiene choques entre coches de trafico (4F9450) | — | PROVISIONAL | SE QUEDA |
| Trafico: vuelta al centro del carril 0,02 m/cuadro | 6280EC = 0,02 (ese tope si); la vuelta (473620) sin portar | — | PROVISIONAL | SE QUEDA |
| Caja de choque con el jugador: medio largo + 2,3 m, ancho + 1 m | 4F9450: cajas de los dos coches (520950 / disposicion) | — | PROVISIONAL | SE QUEDA |
| Muro a 1 m del limite | CbwColiWall con los puntos de la carroceria | — | PROVISIONAL | SE QUEDA |
| Altura y orientacion del coche: 4 muestras a 1,6/0,8 m | suspension y contactos de las ruedas | — | PROVISIONAL (solo dibujo) | SE QUEDA |
| Escala de puntos por avance (x10, x110) | PS2 0x28F984 / 4F2DF0 (K628244, K5C68E0) | igual | CONFIRMADO | SE QUEDA (ahora con m_pos) |
| Piloto automatico de la demo (DEMO del titulo) | el original pasa la pelicula de MV.PS2 | — | invento propio (solo prueba/atraccion) | SE QUEDA |
| META/AGOTADO: freno 0,6 y vuelta al titulo a los 600 cuadros | el original conduce solo hasta el final (camaras 4853D0..) | — | PROVISIONAL | SE QUEDA |
| Aviso de bifurcacion a 450 m de la separacion | 4BB890: BIFURCA con el avance car+0x102C entre 0,65 y 0,72 / 0,70 y 0,75 | — | PROVISIONAL | SE QUEDA |
| Marcas de derrape (256, ancho 0,26 m) y humo (32, vida 1 s, tamano 0,6 + 2,6 t) | tire_mark_init 41FB10 / tire_mark_req 41FB50 / 486450 y PcTireSmoke 41C420/41C570 (race_particles*) | — | PROVISIONAL | SE QUEDA (proximo paso: portar race_particles) |
| Sonido: `ESCALA 3` de los efectos frente a la musica | balance 4279C0 (624238 / 624258) | — | PROVISIONAL (mezcla del AICA) | SE QUEDA |
| Sonido del derrape: deslizamiento / 30 grados | 4260E0 PlaySkid: maximo del deslizamiento de las 4 ruedas (+60/+64.. del cuerpo) | — | PROVISIONAL | SE QUEDA |
| Golpe: fuerza 0,4 + dv/25 y v/40, 20 cuadros entre golpes | 4264C0 PlayWall, 4F65D0 (una vez cada 60 cuadros) | — | PROVISIONAL | SE QUEDA |
| Salto de cuadros (paso de 16683 us, hasta 3) | tick 1/60,2 s (CONFIRMADO); pasos por segundo de la version PAL | — | PROVISIONAL (tecnico) | SE QUEDA |
| camara.c: la carrera siguiente empieza con la vista elegida | — | — | PROVISIONAL | SE QUEDA |
| Voz y sonido de EXTEND TIME | no aparecen en 450CC0 ni en 4B9630 | — | PENDIENTE (buscar en la escena 2C00AC o en race_robots) | — |

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
- PENDIENTE: LOD del trafico por distancia (2-4; umbrales 5B4354..5B4364 del PC, sin localizar en el ELF de PS2);
  luces del trafico (+10/+14: noche y freno); animaciones (volante con car+204, giro y direccion de las ruedas, aleron
  del Enzo con la velocidad, faros escamoteables al abrirse, disco +94 con car+1F4); los bits de noche y tunel
  (SCN_EFC) para +80 y +60; suspension (car+130 y car+F0 de la fisica).

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
  (`carga_cartel` en main.c, vertices en el marco del nodo). 571 en GHOS-? (seg 10), 10 en la bifurcacion.
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
- Niebla: lista 1 de scn_env_fog_<zona> (exp/exp2, densidad y color por posicion de carretera, ~7,9 m cada una), en
  la tabla de niebla de la PowerVR (el fundido de la distancia de dibujo solo si esta se acorta por seguridad).
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
- PENDIENTE: mezcla de cielo en la union, sol, entorno lejano, objetos animados de BK; probar en R.

## Ultimo cambio verificado

Auditoria de inventos y checkpoint del original (ver arriba): ACEL 1, posicion de carretera m_pos (puntuacion, IA y
grua), rumbo adelante a 16 posiciones, ampliacion en el principio de la etapa con 44DC70 completo (KTE1), aparicion y
cierre del trafico del original. Verificado: compila sin avisos, banco de fisica, Flycast (checkpoint LAKE y DESE).

## Proxima tarea

Camara de carrera del original (sistema visible en todo momento y hoy escrito a mano).
