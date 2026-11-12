#!/usr/bin/env bash
# seguidas.sh <disc.gdi> [pantalla] [salida/]: carreras seguidas por el menu real, para ver que la RAM y la VRAM vuelven
# a lo mismo antes de cada carrera (docs/DREAMCAST.md, Carreras seguidas). El GDI va compilado con -DPRUEBA_PILOTO_MENU
# -DPRUEBA_NODO=13 -DPRUEBA_FIN=300 -DPRUEBA_SIN_TIEMPO (cada carrera desde IMPE a 300 m de la meta, con el piloto) y
# mejor -DDIAG_PVRMEM (el mapa de la VRAM con cada RAMMAPA, lineas PVRMAPA). Recorrido (como menu.sh hasta la primera):
# 1 meta -> millas -> continuar A (REINTENTAR) -> 2 meta -> B (al titulo; tras 8 s la pelicula de la demo de atraccion,
# START) -> menu -> 3 pausa ABANDONAR -> millas -> GAME OVER -> menu -> 4 pausa REINTENTAR -> millas -> 5 meta -> B ->
# menu con otro coche -> 6 meta -> B -> menu -> 7 hasta la meta. Capturas sN_35 (continuar), sN_tit, sN_carrera.
# MODO_JUEGO=n como menu.sh (3 CONTRARRELOJ). Al final las lineas RAMMAPA, VRAM: zona y FIN: de cada carrera.
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"; set +e
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_seguidas}
arranca ${FLYCAST_OPC:-}; marca
. "$D/menu.sh"
# MARCA en la linea que casa (no al final: lo que viene detras puede haber salido ya)
casa() { local n; n=$(tail -n +$((MARCA + 1)) "$SAL/flycast.txt" | grep -a -n -m1 -E "$1" | cut -d: -f1); [ -n "$n" ] && { MARCA=$((MARCA + n)); return 0; }; return 1; }
espera_log() {    # patron [max s] (la de comun.sh deja MARCA al final)
    local t=0
    until casa "$1"; do
        sleep 1; t=$((t + 1)); kill -0 $FLY 2> /dev/null || { echo "Flycast se ha parado" >&2; ERROR=1; exit 1; }
        [ $t -lt ${2:-60} ] || { echo "no sale '$1' en ${2:-60} s" >&2; ERROR=1; return 1; }
    done
}
pulsa_hasta() {   # tecla patron [max s]: la tecla cada 2 s hasta la linea
    local t=0
    until casa "$2"; do
        tecla $1; espera 2; t=$((t + 2)); [ $t -lt ${3:-60} ] || { echo "no sale '$2'" >&2; ERROR=1; return 1; }
    done
}
meta_y() {        # tecla tras la meta y el modo 35 (x REINTENTAR, c al menu)
    espera_log 'CURSO: GOAL' 400 || return 1
    espera_log 'FIN: modo 35' 120 || return 1
    espera 5; foto "s${1}_35"
    pulsa_hasta $2 'FIN: modo (30|28)' 40
}
al_menu() {       # del titulo (o de la pelicula de la demo de atraccion, que sale tras 8 s quieto en el titulo) a la
                  # carrera: START hasta la musica del menu; $2 = otro: una a la derecha en la pantalla del coche
    espera 4; foto "s${1}_tit"
    local t=0
    until casa 'musica: MUS30'; do tecla Return; espera 3; t=$((t + 3)); [ $t -lt 60 ] || { echo "no sale el menu" >&2; ERROR=1; return 1; }; done
    espera 4
    local i=0
    until casa 'VRAM: zona del curso'; do
        [ "$i" = 2 ] && [ "${2:-}" = otro ] && { tecla Right; espera 5; foto "s${1}_coche"; }
        tecla x; espera 6; i=$((i + 1))
        [ $i -lt 12 ] || { echo "no llega a la carrera" >&2; ERROR=1; return 1; }
    done
}
pausa() {         # pausa y n abajo (2 ABANDONAR, 1 REINTENTAR), A y A
    espera 7; tecla Return; espera 2
    local k; for k in $(seq 1 $1); do tecla Down; espera 0.5; done
    tecla x; espera 2; tecla x; espera 2
}
meta_y 1 x; espera_log 'VRAM: zona del curso' 60; espera 6; foto s2_carrera       # 2 (REINTENTAR)
meta_y 2 c; al_menu 2; espera 3; foto s3_carrera                                 # 3 (menu)
pausa 2; espera_log 'FIN: modo 28' 60; al_menu 3; foto s4_carrera                # 4 (ABANDONAR)
pausa 1; espera_log 'FIN: modo 30' 60; espera_log 'VRAM: zona del curso' 60; espera 6; foto s5_carrera   # 5 (REINTENTAR)
meta_y 5 c; al_menu 5 otro; espera 6; foto s6_carrera                             # 6 (menu, otro coche)
meta_y 6 c; al_menu 6; espera 6; foto s7_carrera                                 # 7
espera_log 'CURSO: GOAL' 400; espera 5
log '^RAMMAPA (menu|carrera|vuelta)|Out of|pvr_mem|VRAM: zona|PVRMEM: (sin|libera)|FIN: modo' 80
