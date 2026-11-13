#!/usr/bin/env bash
# salon.sh <disc.gdi> [pantalla] [salida/]: el SALON (showroom, SALON.DAT). GDI con -DPRUEBA_MILLAS=20000 (la licencia
# nueva con 20000 millas al entrar en el salon). Primer arranque con la VMU vacia: licencia nueva, UN JUGADOR, SALON
# (cuatro a la derecha): pagina principal (sa_1), COCHES (sa_2: el F50, 7500 M), ventana de compra (sa_3), A: comprado
# y guardado (sa_4); MUSICA (tres abajo: sa_5), compra del primer tema (sa_6); fuera del salon (sa_7); OUTRUN, la
# seleccion de coche en el F50 (sa_8: sin candado) y la musica en el tema 8 de la lista 0 (sa_9: sin candado).
# Segundo arranque con la misma VMU (CONSERVA_VMU=1): la seleccion de coche en el F50 (sa_10) y el salon con las
# millas que quedaron (sa_11: 10000 M). Error sin "SALON: comprado" o sin los guardados.
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_salon}
arranca; marca
espera_log 'VIDEO MVTS.VQV: precarga' 900; espera 3
tecla Return; espera_log 'MVTS.VQV: (saltado|fin)' 60; espera 4
tecla Return; espera_log 'musica: MUS30' 30; espera 5
tecla x; espera 5
tecla Down; espera 1; tecla Down; espera 1; tecla Down; espera 1
tecla x; espera_log 'menu: guardado OR2C2C' 60; espera 5                 # menu principal
tecla x; espera 5                                                        # un jugador
for _m in 1 2 3 4; do tecla Right; espera 1; done                        # SALON
tecla x; espera_log 'SALON: SALON.DAT' 30; espera 5; foto sa_1           # pagina principal
tecla x; espera 5; foto sa_2                                             # COCHES: el primero (F50)
tecla x; espera 2; foto sa_3                                             # ventana de compra
tecla x; espera_log 'SALON: comprado' 10 || true; espera_log 'SALON: guardado' 60 || true; espera 3; foto sa_4
tecla c; espera 4                                                        # a la pagina principal
tecla Down; espera 1; tecla Down; espera 1; tecla Down; espera 2         # MUSICA
tecla x; espera 5; foto sa_5
tecla x; espera 2; tecla x; espera_log 'SALON: comprado' 10 || true; espera_log 'SALON: guardado' 60 || true
espera 3; foto sa_6
tecla c; espera 4; tecla c; espera 5; foto sa_7                          # fuera del salon: UN JUGADOR
for _m in 1 2 3; do tecla Left; espera 1; done                           # OUTRUN
tecla x; espera 6; tecla Right; espera 2; tecla Right; espera 6; foto sa_8   # seleccion de coche: el F50
tecla x; espera 5; tecla x; espera 6                                     # cambio, musica
for _m in 1 2 3 4 5 6 7; do tecla Right; espera 1; done; espera 3; foto sa_9   # el tema 8 de la lista 0
log 'SALON|LICENCIA|guardado|pvr_mem|Out of' 20
grep -a -c 'SALON: comprado' "$SAL/flycast.txt" | grep -q 2 || { echo "no se han comprado los dos objetos" >&2; ERROR=1; }
kill $FLY; wait $FLY 2> /dev/null || true
cp "$SAL/flycast.txt" "$SAL/flycast_1.txt"; mkdir -p "$SAL/arranque1"; mv "$SAL"/sa_*.png "$SAL/arranque1/"
CONSERVA_VMU=1 arranca; marca
espera_log 'menu: OR2C2C en' 900
espera_log 'VIDEO MVTS.VQV: precarga' 900; espera 3
tecla Return; espera_log 'MVTS.VQV: (saltado|fin)' 60; espera 4
tecla Return; espera 8                                                   # menu principal (licencia leida)
tecla x; espera 5; tecla Right; espera 1                                 # un jugador, OUTRUN
tecla x; espera 6; tecla Right; espera 2; tecla Right; espera 6; foto sa_10
tecla c; espera 5; for _m in 1 2 3; do tecla Right; espera 1; done       # SALON
tecla x; espera_log 'SALON: SALON.DAT' 30; espera 5; foto sa_11
log 'SALON|pvr_mem|Out of' 10
if grep -a -q 'pvr_mem: ERROR' "$SAL/flycast.txt" "$SAL/flycast_1.txt"; then echo "AVISO: pvr_mem: ERROR" >&2; ERROR=1; fi
