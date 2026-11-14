#!/usr/bin/env bash
# sp.sh <disc.gdi> [pantalla] [salida/] [segundos (120)]: OUTRUN2SP desde el menu principal (menu.sh con MENU_SP=1:
# licencia nueva, uno a la derecha, el frontal de recreativa: coche, transmision, modo con el gatillo R, musica) hasta la carrera del juego de OUTRUN2SP
# (SP_LADO=1, por defecto: el gatillo R, "OutRun2": preset 1, arbol 0 = csc_data_2; SP_LADO=0: "Recorrido OutRun2SP",
# preset 0, arbol 1 = csc_data_cvt; 0x1760c0)
# y despues conduce como conduce.sh: una captura cada 5 s (sp_c000..) y "CONGELADO" con error si dos son iguales.
# Comprueba en la consola serie "juego OUTRUN2SP, preset <p>" y "CURSO: arbol <a>" del lado.
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
    # con el lado 1 (PALM, arbol 0) el tiempo se acaba antes de los 120 s sin llegar al punto de control: la pantalla
    # de rankings (modo 25) queda quieta y no es un cuadro congelado
    if grep -a -q "CURSO: tiempo agotado" "$SAL/flycast.txt"; then echo "tiempo agotado a los $((i * 5)) s de carrera ($f)"; break; fi
    if [ "$h" = "$ant" ]; then echo "CONGELADO a los $((i * 5)) s de carrera ($f)" >&2; ERROR=1; break; fi
    ant=$h; i=$((i + 1))
done
xdotool keyup x
if [ "${SP_LADO:-1}" = 1 ]; then PR=1; AR=0; else PR=0; AR=1; fi
grep -q "juego OUTRUN2SP, preset $PR" "$SAL/flycast.txt" || { echo "no se ha corrido el juego de OUTRUN2SP con el preset $PR" >&2; ERROR=1; }
# (el arbol 0 es el del arranque: sin "CURSO: arbol" si no se lee otro)
if [ $AR = 0 ]; then ! grep -q "CURSO: arbol [1-9]" "$SAL/flycast.txt" || { echo "se ha leido otro arbol que el 0" >&2; ERROR=1; }
else grep -q "CURSO: arbol $AR," "$SAL/flycast.txt" || { echo "no se ha leido el arbol $AR" >&2; ERROR=1; }; fi
log 'MODO|RAM|VRAM|No se pud|sin |PVR|CARGA|curso|BAND|TAPD' 40
