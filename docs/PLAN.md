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
   SELECTOR_CVT HECHO 2026-11-11 (comun/fsp.c, PARIDAD.md); el final, la entrada de nombre y los rankings de OUTRUN2SP siguen
   SIN PORTAR: PARIDAD.md, OUTRUN2SP; la pantalla de carga, el GAME OVER (27) de la pausa, la meta y el fin del
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
