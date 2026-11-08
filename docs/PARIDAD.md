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
| Aceleracion, motor, cambio auto/manual | tabla de parametros 0x2dbc30 (19 col.), par 162 f, freno 256 f; cambio PS2 0x18d530 | PARCIAL | or2_fisica.c clonado; columna por modelo MAP1 | `ACEL 1.35` multiplica el par (peticion del probador, no original) | decidir con el probador si se quita; si no, documentar como opcion | H, E |
| Direccion, derrape (inicio/mantenimiento/salida) | SlipAngleCtrl / RearGripCtrl / CorneringCtrl; temporizador 40 cuadros | OK | decompilacion + banco | ninguna conocida | trazas de entrada comparables con PS2 | H, E |
| Adherencia por superficie | get_road_mu 500B30, running_resistance 502120, COLI0200 | OK | coli por rueda | ninguna conocida | — | H, E |
| Choques coche-coche y muro, choque fuerte, grua | 4F9450/4F8490/4F6FE0/4F7BF0, cbw_coli_wall, 5038D0, PlWrecker | PARCIAL | CHQ1/POS1 de PARAM.DAT | grua recoloca en el carril mas cercano (no en la fila de poligonos); avance de 25 m supuesto | medir la escala de "posicion de carretera" | H, E |
| Camara de carrera | evento CAMERA 385: 4858D0, 484810, 483110, 4A1680 SetCarCamera, 485590/483CE0/4834A0/483600, 4832F0/484A40, 483C10/484BD0 (PS2 0x178500..0x17b540, 0x1817c0) | PARCIAL | demo/camara.c con las tablas CAM1 de PARAM.DAT (vistas 0x2ccd88, por vista y modelo 0x2cd3d8, por modelo 0x2d2838 del fichero SLES) | vista 1 sin interior (no se dibuja el coche); camara de salida en los 60 primeros cuadros de la cuenta (SUPUESTO); sin vista 0x1D (780258 = 0) ni camaras de meta/fin; plano cercano 0,5 (original 0,1) | ver seccion Camara de carrera | E |
| Trafico (clases, IA, reaccion) | 472090, 472450, 472A50, 47DC10, 4736D0, hit_476040 | PARCIAL | TRAFC.DAT; colores (tabla 0x36bc18, modelos 30..43) y materiales en TRAFn.DAT (TCX1) | velocidades por tramo (registros 0x46 de 4F0030) sin clonar | clonar tabla de velocidades por tramo | E |
| Rivales | — | FALTA | — | no hay rivales | — | — |
| Carretera, pendientes, bifurcacion | STAGE.PS2, BK.PS2 cs_BK_PALM/cs_ENVBK_PALM, COMMON obj_course_obj_bk_palm + obj_OBJ_<ZONA>_BUNKI_TX, descriptores 0xb0, csc_data_2 | OK | conv_tramo/conv_union, CURSO.DAT | arbol entero (15 etapas, 10 bifurcaciones con sus carteles, 5 metas); cielo de la union = el de la etapa que se deja, cambia al llegar a la siguiente (sin mezcla); bifurcaciones y su entorno = las de PALM (las demas no estan en el disco) | objetos animados de BK (ANIMS), sol | E |
| Temporizador (inicial y ampliacion) | 44DC70, 450CC0, 450AC0; tabla 7D2E98 = PS2 0x3cb708 copiada por 0x176040 | PARCIAL | TIM1 de PARAM.DAT | inicial 5999 cuadros CONFIRMADO; ampliacion al entrar en la union con el nivel (0..4, profundidad en el arbol: el +0x08 de csc_data_2 vale 0) de la etapa siguiente; en la meta no amplia; fila/preset/dificultad, +60 y k636904 PENDIENTES | localizar en PS2 la variante del modo y la dificultad por defecto | E |
| Checkpoints, meta, resultados | race_manager, BK.PS2 cs_BK_<zona> | PARCIAL | meta del original en las 5 etapas finales (pancarta GOAL de cs_BK_<zona>, z = 0 de la union) | sin pantalla de resultados ni final del original | — | E |
| Puntuacion modo OutRun | 4F2DF0, 4F2660, 4F2B20, 4B9770 | PARCIAL | main.c pt_* | escala de "posicion de carretera" supuesta | medir la escala | E |
| Eventos, scripts, progresion, misiones | SCRIPTS.PS2 (Races, RaceAttack, OutrunMiles, Req_*) | FALTA | — | — | — | — |
| Coches del jugador | CARS.PS2 1-32 / 33-63, disposicion 0x3d3ff0; materiales tabla 4/5 del bloque; colores 0x35ad48 (PC 64D120/408C80); mapa de entorno del bloque | PARCIAL | 15 coches enteros con interior, 9540-10915 vertices; pintura, 8 colores (Y en la seleccion) y reflejo del original (CEX1) | UV de entorno con la formula esferica clasica (la del microcodigo VU1 sin leer); capa 2 de mallas con textura propia y luces de freno sin dibujar | ver seccion Coches | E |
| Escenarios, texturas, materiales | STAGE.PS2, VQ, scn_env_fog_<zona> | PARCIAL | 15 tramos + 15 uniones; mallas sin textura con su material (tabla 5); niebla del original | sin entorno lejano (cs_ENV_*), objetos de tramo (obj_course_obj_cs_*), sol; niebla mezclada por distancia en la union (el original por tiempo) | — | E |
| HUD, menus, transiciones | SPRANI/XST, TEXT.PS2 | PARCIAL | HUD.DAT, MENU.DAT | titulo es un cuadro del video; textos bfont antiguos en la demo automatica y el aviso de rama | quitar textos bfont | E |
| Musica y efectos | MUSIC.PS2, SOUND.PS2 ICS/MENU/FE | PARCIAL | musica.c, sfx.c | 48 kHz supuesto; sin voces, sin sonido por superficie ni del choque fuerte | — | E |
| Cargas y estabilidad | — | PARCIAL | carga en segundo plano, mutex del GD | — | medir en GDEMU | E |
| Rendimiento / TA en consola real | — | PARCIAL | tope global del TA (DREAMCAST.md) | primer cuelgue medido en la consola (VB lleno a 24656 vtx) | esperar prueba R de la demo 14+ | E (R pendiente) |

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
  meta/fin (4853D0, 485470, 485830); desplazamiento del centro de proyeccion +C4/+C8 (cero en las vistas usadas);
  plano cercano 0,1.

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
  piezas anadidas. Solo se quita lo que no se ve desde las camaras del juego y las uniones entre tiras (el original
  empaqueta varias tiras por lote de 59 vertices y no dibuja los triangulos que las unen; ya no salen astillas).
- Ruedas: el original dibuja en el coche del jugador siempre las del buje de la disposicion (46AE70: +88 y +8C/+90,
  sin cambio por distancia en esa funcion); las "ligeras" son del modelo del rival (obj_rc_*). Ya no se usan.
- Presupuesto: TOPE 11000 vertices por coche (antes 9200); ver DREAMCAST.md.
- Trafico: materiales igual (los cristales con UV de entorno fijas: vista desde detras, sin calculo por cuadro); la
  capa 2 de la carroceria no se dibuja. Colores: tabla de 44 listas en 0x36bc18 (modelos 30..43: {clave, color 1,
  color 2}, otras texturas del mismo bloque); el original da a cada coche nuevo de un modelo el color siguiente
  (0, 1, 2; 2 en el modelo 38 y 1 en el 39 y el 41: PC 478B90). Las listas confirman que los registros 69 y 136 son
  los modelos 33 y 37 (antes al reves). VRAM del trafico: 140 -> 261 KB (las texturas de los otros colores).
- Comprobado: render de cada COCHEnn.DAT leyendo el fichero como la demo (hoja de comprobacion fuera del repositorio: colores 1, 3 y
  6 y las texturas del disco al lado); compila sin avisos; Flycast (seleccion y carrera).
- PENDIENTE: luces de freno (+80 de la disposicion, con car+4 & 0x300) y tras el choque (+84); capa 2 de las mallas
  con textura propia (mismo mapa: haria falta una segunda pasada); el estado de la tabla 7 (mezclas del GS); leer el
  microcodigo VU1 para la formula exacta del reflejo; reflejo del trafico por cuadro.

## Curso entero (arbol del modo)

- Arbol: SCRIPTS `\bin\csc_data_2.bin` (15 registros de 0x78 tras la cabecera: +0x04 clave, +0x1c descriptor de tramo,
  +0x20 de union, +0x2c/+0x30 hijos). Niveles: PALM; LAKE, ALPI; CAST, FORE, DESE; CLOU, INDU, SNOW, GHOS; metas TULI,
  METR, RUIN, IMPE, CAPE (STAGE segmentos 0, 3, 5, 8, 10, 11, 7, 4, 6, 9, 12, 13, 14, 16, 15).
- Encadenado: la union se coloca en su etapa con el +0x98 del descriptor de tramo (x, y, z, giro y; giros x/z 0 en
  todos); la etapa siguiente en el final de la rama (colision LBK/RBK_PALM). Cada fichero en su marco; la demo los
  coloca al leerlos y recentra el marco en el principio de cada union (traslacion; el giro se acumula).
- Uniones: bifurcacion = cs_BK_PALM + cs_ENVBK_PALM + porticos obj_course_obj_bk_palm con los carteles BUNKI_TX de
  los dos destinos en sus 4 texturas hueco de 8x8 (asignacion por la posicion de cada malla: PROBABLE); meta =
  cs_BK_<zona>. Los cs_BK_/cs_ENVBK_/obj_course_obj_bk_<zona> de las demas zonas que nombra el ejecutable no estan en
  el disco: se usan los de PALM (SUPUESTO: que el original haga lo mismo).
- Niebla: lista 1 de scn_env_fog_<zona> (exp/exp2, densidad y color por posicion de carretera, ~7,9 m cada una), en
  la tabla de niebla de la PowerVR junto con el fundido de la distancia de dibujo.
- Fallo de la union (coche hundido y planos grises inclinados al entrar en ella), causa y arreglo: (1) la ruta de cada
  etapa acababa hasta 20 m antes del principio de la union (la de LAKE, 19,7 m) y la de la carrera daba ese salto en
  un paso de 4 m; ahora conv_tramo alarga la ruta en recto hasta el extremo de la calzada de su colision (quedan
  1-9 m) y ruta_anade rellena lo que falte con puntos interpolados. (2) cs_ENVBK_PALM (malla 8, muros de 5 m) une en
  una misma tira los muros de los dos lados de la calzada y el triangulo puente cruzaba la carretera a 0,4-2,9 m de
  altura entre 15 y 35 m dentro de la union (medido en el PC con la malla colocada: tools en el scratchpad); la PS2 no
  lo dibuja: modelo.py quita en las uniones los triangulos cuyos vertices tienen normales opuestas (mod.puentes). Las
  alturas de la malla, la colision y la ruta coinciden (0 m de diferencia en la calzada). Verificado en E (nodo 1,
  PRUEBA_FIN=900). Queda: la calzada de cs_ENVBK_PALM repite la de cs_BK_PALM a la misma altura (parpadeo de las
  rayas a lo lejos).
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

Temporizador con la tabla del original (TIM1): inicial 5999 cuadros (se ve 99 y baja a 1 cuadro de logica), muestra
cuadros/60,2 + 0,34, crono en cuadros/60,2. Verificado en E (Flycast): 99 al salir, 95 a 4,8 s, 90 a 9,9 s.

## Proxima tarea

Camara de carrera del original (sistema visible en todo momento y hoy escrito a mano).
