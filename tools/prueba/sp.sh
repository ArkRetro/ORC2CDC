#!/usr/bin/env bash
# sp.sh <disc.gdi> [pantalla] [salida/] [segundos (120)]: OUTRUN2SP desde el menu principal (menu.sh con MENU_SP=1:
# licencia nueva, dos a la derecha, coche, cambio, musica) hasta la carrera del juego de OUTRUN2SP (arbol 1, preset 1)
# y despues conduce como conduce.sh: una captura cada 5 s (sp_c000..) y "CONGELADO" con error si dos son iguales.
# Comprueba en la consola serie "juego OUTRUN2SP, preset 1" y "CURSO: arbol 1".
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_sp}; SEG=${4:-120}
arranca ${FLYCAST_OPC:-}; marca
MENU_SP=1 P=sp . "$D/menu.sh"
foto sp_salida
xdotool keydown x; ant=; i=0
while [ $((i * 5)) -lt $SEG ]; do
    case $((i % 4)) in 1) xdotool keydown Left; sleep 0.4; xdotool keyup Left;; 3) xdotool keydown Right; sleep 0.4; xdotool keyup Right;; esac
    espera 5; f=$(printf 'sp_c%03d' $i); foto $f
    h=$(convert "$SAL/$f.png" rgb:- | md5sum | cut -c1-32)
    if [ "$h" = "$ant" ]; then echo "CONGELADO a los $((i * 5)) s de carrera ($f)" >&2; ERROR=1; break; fi
    ant=$h; i=$((i + 1))
done
xdotool keyup x
grep -q 'juego OUTRUN2SP, preset 1' "$SAL/flycast.txt" || { echo "no se ha corrido el juego de OUTRUN2SP" >&2; ERROR=1; }
grep -q 'CURSO: arbol 1,' "$SAL/flycast.txt" || { echo "no se ha leido el arbol 1" >&2; ERROR=1; }
log 'MODO|RAM|VRAM|No se pud|sin |PVR|CARGA|curso|BAND|TAPD' 40
