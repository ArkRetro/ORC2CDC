#!/usr/bin/env bash
# abandona.sh <disc.gdi> [pantalla] [salida/]: nav.sh hasta la carrera, 8 s de gas, pausa (START; ab_1), ABANDONAR (dos
# abajo y A; ab_2 la confirmacion, A) y de vuelta al menu (ab_3: las piezas de MENU.DAT se vuelven a leer del GD,
# menu_tablas), A en cada pantalla hasta la segunda carrera (ab_4..; ab_fin) y la pausa otra vez (ab_pausa). Con MENU_SP=1
# (OUTRUN2SP) despues del GAME OVER se vuelve al frontal de recreativa (modo 10).
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_abandona}
arranca ${FLYCAST_OPC:-}; marca
. "$D/menu.sh"
xdotool keydown x; espera 8; xdotool keyup x
tecla Return; espera 2; foto ab_1
tecla Down; espera 0.5; tecla Down; espera 0.5; tecla x; espera 2; foto ab_2
tecla x; espera 8; foto ab_3; marca                                 # el titulo (tras ABANDONAR)
tecla Return; espera 6                                               # START: al menu principal
i=4
until tail -n +$((MARCA + 1)) "$SAL/flycast.txt" | grep -a -q 'VRAM: zona del curso'; do
    tecla x; espera 6; foto ab_$i; i=$((i + 1))
    [ $i -lt 14 ] || { echo "no llega a la segunda carrera" >&2; ERROR=1; break; }
done
espera 15; xdotool keydown x; espera 6; xdotool keyup x; foto ab_fin
tecla Return; espera 2; foto ab_pausa
log 'RAMMAPA|MENU:|No se pud|sin ' 30
