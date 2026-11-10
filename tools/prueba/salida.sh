#!/usr/bin/env bash
# salida.sh <disc.gdi> [pantalla] [salida/] [pal]: el recorrido de nav.sh (o de pal.sh con el cuarto argumento "pal")
# hasta la carrera y, en cuanto empieza la escena de la salida (consola serie "SALIDA: escena"), una captura cada
# 0,25 s durante 7 s (s_00..s_27) y la hoja de contactos hoja.png (montage de ImageMagick): el coche derrapando, el
# banderero, la cuenta atras y el GO.
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_salida}; MODO=${4:-ntsc}
if [ "$MODO" = pal ]; then
    arranca -config config:Dreamcast.Region=2 -config config:Dreamcast.Broadcast=1 -config config:Dreamcast.Cable=2
    marca; espera 6; tecla x; P=pal
else arranca; marca; P=nv; fi
ESPERA_CARRERA=0; . "$D/menu.sh"
t=0; until grep -a -q 'SALIDA: escena' "$SAL/flycast.txt"; do espera 0.2; t=$((t + 1)); [ $t -lt 300 ] || { echo "no sale la escena" >&2; ERROR=1; exit 1; }; done
for i in $(seq -w 0 27); do foto s_$i; sleep 0.25; done
montage "$SAL"/s_*.png -tile 7x4 -geometry 256x192+2+2 "$SAL/hoja.png" 2> /dev/null || true
log 'SALIDA|VOZ|RAM|No se pud|sin ' 20
