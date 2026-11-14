#!/usr/bin/env bash
# orden.sh <disc.gdi> [pantalla] [salida/] [iso] [carreras (17)]: el orden de las etapas en todos los modos (revision
# 44). El GDI va compilado con -DPRUEBA_AUTO -DPRUEBA_ORDEN -DPRUEBA_SIN_TIEMPO -DPRUEBA_RAMA=r -DPRUEBA_RAMA_FIJA=r (0 siempre a la
# izquierda, 1 siempre a la derecha) -DPRUEBA_FIN=600 -DPRUEBA_SALTO=600 (cada etapa empieza a 600 m de su final): el
# piloto corre seguidas las carreras (desde la -DPRUEBA_ORDEN_DESDE=k, si va; en cada bifurcacion el coche pasa a un
# carril de la rama fijada: el piloto sigue los carriles del asfalto y se quedaba en la otra calzada) de la lista PO de demo/main.c (OUTRUN con los juegos 0..3 del tablero 0x24 -
# arboles 0, 1, 290, 291 -, CONTRARRELOJ de una etapa, de metas A..E del derecho y del reves y de curso seguido, INFARTO
# de OutRun 2 SP, misiones de COAST 2 COAST y OUTRUN2SP por los dos lados del gatillo R) y tools/prueba/orden.py compara lo que escribe la consola serie
# (SALIDA, RECORRIDO, CHECKPOINT, MAPA, GOAL) con la tabla del original de tu ISO: el arbol, la etapa a la que lleva
# cada rama (con -1 la misma), la meta, el nivel y la casilla del mapa de ruta del HUD. Una captura cada 20 s
# (o_000..). Error si una carrera no coincide.
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_orden}; ISO=${4:-}; N=${5:-17}
[ -n "$ISO" ] || { echo "falta la ISO (cuarto argumento)" >&2; exit 2; }
arranca; marca
espera_log 'ORDEN: carrera 0 ' 1200
i=0
until [ "$(grep -a -c 'ORDEN: carrera' "$SAL/flycast.txt")" -gt "$N" ]; do
    espera 20; foto $(printf 'o_%03d' $i); i=$((i + 1))
    [ $i -lt 400 ] || { echo "demasiado tiempo" >&2; ERROR=1; break; }
done
python3 "$D/orden.py" "$SAL/flycast.txt" "$ISO" || ERROR=1
exit ${ERROR:-0}
