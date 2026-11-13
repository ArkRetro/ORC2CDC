#!/usr/bin/env bash
# reintenta.sh <disc.gdi> [pantalla] [salida/]: hasta la carrera por el menu (menu.sh; por defecto COAST 2 COAST 1-1,
# MODO_JUEGO=n otro modo), captura de la salida (re_1), GAS segundos de gas (8), pausa (START; re_pausa), REINTENTAR
# (uno abajo y A; re_conf la confirmacion "Estas seguro?", A), la escena de MILLAS OUTRUN y la salida otra vez (re_2,
# capturada igual que re_1: tienen que coincidir el sitio, la parrilla y el coche). En la consola serie: las lineas
# "SALIDA:" de cada salida (arbol, etapa, s, x, angulo y el coche) tienen que salir iguales. PAUSA_TRAS=patron: la pausa
# cuando la consola serie lo escriba en vez del gas (con el piloto de -DPRUEBA_PILOTO_MENU).
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_reintenta}
arranca ${FLYCAST_OPC:-}; marca
ESPERA_CARRERA=${ESPERA_SALIDA:-5}
. "$D/menu.sh"
foto re_1
if [ -n "${PAUSA_TRAS:-}" ]; then                                    # PAUSA_TRAS=patron: la pausa cuando la consola serie
    marca; espera_log "$PAUSA_TRAS" ${PAUSA_MAX:-400}; espera ${PAUSA_MAS:-4}   # lo diga (con -DPRUEBA_PILOTO_MENU
    foto re_antes_pausa                                              # -DPRUEBA_RAMA=r: "CHECKPOINT" tras la rama r)
else xdotool keydown x; espera ${GAS:-8}; xdotool keyup x; fi
tecla Return; espera 2; foto re_pausa
tecla Down; espera 0.5; tecla x; espera 2; foto re_conf
marca; tecla x
espera_log 'VRAM: zona del curso' 120; espera ${ESPERA_SALIDA:-5}; foto re_2
log 'SALIDA: arbol|CURSO: arbol|CURSO: etapa|CHECKPOINT|RIVAL: mision|MODO:' 16
