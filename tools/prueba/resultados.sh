#!/usr/bin/env bash
# resultados.sh <disc.gdi> [pantalla] [salida/]: OUTRUN2SP (MENU_SP=1, como sp.sh) con el acelerador pisado hasta el fin
# del tiempo; despues el modo 25 (el mapa de la ruta: re_0..re_7, una por segundo), el GAME OVER (re_go) y la vuelta al
# frontal de recreativa (re_fsp). Error si no salen "FIN: modo 25", "FIN: modo 27" y "FSP: frontal" otra vez.
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_resultados}
arranca ${FLYCAST_OPC:-}; marca
MENU_SP=1 P=re . "$D/menu.sh"
marca; xdotool keydown x
espera_log 'FIN: modo 25' 400 || true
xdotool keyup x
for i in 0 1 2 3 4 5 6 7; do foto re_$i; espera 1; done
espera_log 'FIN: modo 27' 60 || true; espera 2; foto re_go
espera_log 'FSP: frontal' 60 || true; espera 4; foto re_fsp
[ "$(grep -a -c 'FSP: frontal' "$SAL/flycast.txt")" -ge 2 ] || { echo "no vuelve al frontal" >&2; ERROR=1; }
grep -a -q 'FIN: modo 25' "$SAL/flycast.txt" || { echo "sin el modo 25" >&2; ERROR=1; }
log 'FIN:|FSP:|SPRANI|RAMMAPA|No se pud|sin |CARGA' 40
