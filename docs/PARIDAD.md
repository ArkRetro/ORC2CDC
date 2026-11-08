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
  `PRUEBA_TRAFICO`, `PRUEBA_CHOQUE`, `PRUEBA_RAMA=0/1`, `PRUEBA_V`, `SALIDA`, `VER=1`, `DIAG_VTX`.
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
| Camara de carrera | race_camera (PC 4832F0 y relacionados) | PROVISIONAL | camara escrita a mano en main.c (6,5 m detras, 2,4 m alto, mezcla avance/carroceria) | no es la del original; falta la camara de choque | clonar la camara de la decompilacion | E |
| Trafico (clases, IA, reaccion) | 472090, 472450, 472A50, 47DC10, 4736D0, hit_476040 | PARCIAL | TRAFC.DAT | velocidades por tramo (registros 0x46 de 4F0030) sin clonar | clonar tabla de velocidades por tramo | E |
| Rivales | — | FALTA | — | no hay rivales | — | — |
| Carretera, pendientes, bifurcacion | STAGE.PS2, BK.PS2 cs_BK_PALM, descriptores 0xb0, csc_data_2 | PARCIAL | conv_tramo/conv_union | solo la primera bifurcacion (PALM -> LAKE/ALPINE); sin carteles ni objetos de la union; cielo cambia de golpe | encadenar el resto del arbol | E |
| Temporizador (inicial y ampliacion) | 44DC70, 450CC0, 450AC0; tabla 7D2E98 = PS2 0x3cb708 copiada por 0x176040 | PARCIAL | TIM1 de PARAM.DAT | inicial 5999 cuadros CONFIRMADO; fila/preset/dificultad, +60 y k636904 PENDIENTES | localizar en PS2 la variante del modo y la dificultad por defecto | E |
| Checkpoints, meta, resultados | race_manager | PARCIAL | meta al final de la segunda etapa | sin pantalla de resultados del original | — | E |
| Puntuacion modo OutRun | 4F2DF0, 4F2660, 4F2B20, 4B9770 | PARCIAL | main.c pt_* | escala de "posicion de carretera" supuesta | medir la escala | E |
| Eventos, scripts, progresion, misiones | SCRIPTS.PS2 (Races, RaceAttack, OutrunMiles, Req_*) | FALTA | — | — | — | — |
| Coches del jugador | CARS.PS2 1-32 / 33-63, disposicion 0x3d3ff0 | PARCIAL | 15 coches, <= 9200 vertices | interior quitado con tapa; ruedas ligeras; sin colores, freno, faros | — | E |
| Escenarios, texturas, materiales | STAGE.PS2, VQ | PARCIAL | 3 tramos + union | sin sombras ni reflejos del coche; carteles de la union | — | E |
| HUD, menus, transiciones | SPRANI/XST, TEXT.PS2 | PARCIAL | HUD.DAT, MENU.DAT | titulo es un cuadro del video; textos bfont antiguos en la demo automatica y el aviso de rama | quitar textos bfont | E |
| Musica y efectos | MUSIC.PS2, SOUND.PS2 ICS/MENU/FE | PARCIAL | musica.c, sfx.c | 48 kHz supuesto; sin voces, sin sonido por superficie ni del choque fuerte | — | E |
| Cargas y estabilidad | — | PARCIAL | carga en segundo plano, mutex del GD | — | medir en GDEMU | E |
| Rendimiento / TA en consola real | — | PARCIAL | tope global del TA (DREAMCAST.md) | primer cuelgue medido en la consola (VB lleno a 24656 vtx) | esperar prueba R de la demo 14+ | E (R pendiente) |

## Ultimo cambio verificado

Temporizador con la tabla del original (TIM1): inicial 5999 cuadros (se ve 99 y baja a 1 cuadro de logica), muestra
cuadros/60,2 + 0,34, crono en cuadros/60,2. Verificado en E (Flycast): 99 al salir, 95 a 4,8 s, 90 a 9,9 s.

## Proxima tarea

Camara de carrera del original (sistema visible en todo momento y hoy escrito a mano).
