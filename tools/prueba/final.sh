#!/usr/bin/env bash
# final.sh <disc.gdi> [pantalla] [salida/]: el final de recreativa de OUTRUN2SP (modo 24, demo/final.c). El GDI va
# compilado con -DPRUEBA_PILOTO_MENU -DPRUEBA_NODO=13 -DPRUEBA_FIN=300 -DPRUEBA_SIN_TIEMPO (la carrera del menu desde la
# clave 13 del arbol, a 300 m de la meta, con el piloto). OUTRUN2SP como resultados.sh (MENU_SP=1, SP_LADO=0: preset 0, arbol 1 = csc_data_cvt)
# hasta la meta (GOAL, escena de la meta), despues el modo 24: fi_00..fi_NN (una cada 2 s), los resultados (25: fi_25),
# el GAME OVER (fi_go) y la vuelta al frontal (fi_fsp). Error si no salen "FINAL: modo 24", "FIN: modo 25" y
# "FSP: frontal" otra vez.
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_final}
arranca ${FLYCAST_OPC:-}; marca
SP_LADO=${SP_LADO:-0} MENU_SP=1 P=fi . "$D/menu.sh"
espera_log 'CURSO: GOAL' 400 || true
for i in 0 1 2 3 4 5; do espera 3; foto fi_m$i; done                    # la escena de la meta (la pareja)
espera_log 'FINAL: modo 24' 120 || true
for i in $(seq -w 0 ${FOTOS:-14}); do foto fi_$i; espera 2; done
espera_log 'FIN: modo 25' 90 || true; espera 3; foto fi_25
espera_log 'FIN: modo 27' 60 || true; espera 2; foto fi_go
espera_log 'FSP: frontal' 60 || true; espera 4; foto fi_fsp
grep -a -q 'FINAL: modo 24' "$SAL/flycast.txt" || { echo "sin el modo 24" >&2; ERROR=1; }
grep -a -q 'FIN: modo 25' "$SAL/flycast.txt" || { echo "sin el modo 25" >&2; ERROR=1; }
[ "$(grep -a -c 'FSP: frontal' "$SAL/flycast.txt")" -ge 2 ] || { echo "no vuelve al frontal" >&2; ERROR=1; }
log 'FINAL|FIN:|FSP:|META:|GOAL|RAMMAPA|No se pud|sin |Out of' 40
