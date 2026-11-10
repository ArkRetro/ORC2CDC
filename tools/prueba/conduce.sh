#!/usr/bin/env bash
# conduce.sh <disc.gdi> [pantalla] [salida/] [segundos (180)]: como nav.sh hasta la carrera y despues conduce (A = X del
# teclado pisado y la cruceta a izquierda / derecha a ratos) buscando cuelgues: una captura cada 5 s (c_000..); si dos
# seguidas son iguales (el cronometro del HUD no ha cambiado) avisa "CONGELADO" y sale con error.
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_conduce}; SEG=${4:-180}
arranca ${FLYCAST_OPC:-}; marca
. "$D/menu.sh"
xdotool keydown x; ant=; i=0
while [ $((i * 5)) -lt $SEG ]; do
    case $((i % 4)) in 1) xdotool keydown Left; sleep 0.4; xdotool keyup Left;; 3) xdotool keydown Right; sleep 0.4; xdotool keyup Right;; esac
    espera 5; f=$(printf 'c_%03d' $i); foto $f
    h=$(convert "$SAL/$f.png" rgb:- | md5sum | cut -c1-32)
    if [ "$h" = "$ant" ]; then echo "CONGELADO a los $((i * 5)) s de carrera ($f)" >&2; ERROR=1; break; fi
    ant=$h; i=$((i + 1))
done
xdotool keyup x
log 'RAM|VRAM|No se pud|sin |PVR|CARGA|curso' 30
