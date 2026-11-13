#!/usr/bin/env bash
# salidas.sh <disc.gdi> [pantalla] [salida/]: la salida del jugador tres veces en el mismo modo (revision 44, "a veces el
# vehiculo comienza descentrado"): menu.sh hasta la carrera (MODO_JUEGO=n / TB_JUEGO=1 / MENU_SP=1 como alli;
# MODO_JUEGO=3, la CONTRARRELOJ: A en los tableros 0x24 / 0x25 / 0x26, el primer curso), captura de la salida (sa_1),
# GAS segundos de gas (8), pausa y REINTENTAR (sa_2), otra vez gas, pausa y ABANDONAR, de vuelta al menu y A en cada
# pantalla hasta la carrera siguiente (sa_3). Las tres lineas "SALIDA:" de la consola serie tienen que ser iguales (salvo
# "(REINTENTAR)"); error si no.
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_salidas}
arranca ${FLYCAST_OPC:-}; marca
ESPERA_CARRERA=${ESPERA_SALIDA:-5}
if [ "${MODO_JUEGO:-0}" = 3 ]; then                                    # CONTRARRELOJ: los tableros (tools/prueba/tablero.sh)
    espera_log 'VIDEO MVTS.VQV: precarga' 900; espera 3
    for _i in $(seq 1 40); do tecla Return; espera_log 'musica: MUS30' 3 2> /dev/null && break; done
    espera 5; tecla x; espera 5
    tecla Down; espera 1; tecla Down; espera 1; tecla Down; espera 1
    tecla x; espera 6                                                  # menu principal
    tecla x; espera 5                                                  # un jugador
    for _m in 1 2 3; do tecla Right; espera 1; done                    # CONTRARRELOJ
    tecla x; espera_log 'TABLERO: clave 0x24' 30; espera 4             # el juego de cursos
    tecla x; espera_log 'TABLERO: clave 0x25' 30; espera 4             # CURSO / ETAPA
    tecla x; espera_log 'TABLERO: clave 0x26' 30; espera 4             # las metas: la primera
    tecla x; espera 6; tecla x; espera 5; tecla x; espera 6            # coche, cambio, musica
    tecla x; espera 1
    espera_log 'VRAM: zona del curso' 120; espera ${ESPERA_CARRERA}
else
    . "$D/menu.sh"
fi
foto sa_1
xdotool keydown x; espera ${GAS:-8}; xdotool keyup x
tecla Return; espera 2; tecla Down; espera 0.5; tecla x; espera 2      # pausa, REINTENTAR, "Estas seguro?"
marca; tecla x
espera_log 'VRAM: zona del curso' 120; espera ${ESPERA_SALIDA:-5}; foto sa_2
xdotool keydown x; espera ${GAS:-8}; xdotool keyup x
tecla Return; espera 2; tecla Down; espera 0.5; tecla Down; espera 0.5; tecla x; espera 2   # ABANDONAR
tecla x; espera 8; marca
tecla Return; espera 6                                                 # START: al menu principal (o al frontal)
i=0
until tail -n +$((MARCA + 1)) "$SAL/flycast.txt" | grep -a -q 'VRAM: zona del curso'; do
    tecla x; espera 6; i=$((i + 1))
    [ $i -lt 30 ] || { echo "no llega a la carrera siguiente" >&2; ERROR=1; break; }
done
espera ${ESPERA_SALIDA:-5}; foto sa_3
log 'SALIDA: arbol|CURSO: arbol|CURSO: tablero|MODO:' 20
n=$(grep -a 'SALIDA: arbol' "$SAL/flycast.txt" | sed 's/ (REINTENTAR)//' | sort -u | wc -l)
t=$(grep -a -c 'SALIDA: arbol' "$SAL/flycast.txt")
echo "salidas: $t, distintas: $n"
[ "$t" -ge 3 ] && [ "$n" = 1 ] || { echo "las salidas no son iguales (o faltan)" >&2; ERROR=1; }
