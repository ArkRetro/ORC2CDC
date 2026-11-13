#!/usr/bin/env bash
# tablero.sh <disc.gdi> [pantalla] [salida/]: los tableros de cursos de INFARTO / CONTRARRELOJ (TABLERO.DAT). GDI con
# -DPRUEBA_SALON_MILLAS=20000. Primer arranque con la VMU vacia: licencia nueva, UN JUGADOR, CONTRARRELOJ: el juego de
# cursos (tb_1), CURSO / ETAPA (tb_2), las metas (tb_3), el curso seguido con candado (tb_4, A: bloqueado), R: las
# metas del reves con candado (tb_5); ETAPA: las etapas (tb_6), R: con candado (tb_7, A: bloqueado). Al SALON: compra
# de la etapa 0x8b (PALM_R, la segunda de ETAPAS: tb_8). De vuelta a la CONTRARRELOJ: la etapa del reves ya sin
# candado (tb_9), A: coche, cambio, musica y la carrera (arbol 230, tb_10). Segundo arranque con la misma VMU
# (CONSERVA_VMU=1): la etapa sigue sin candado (tb_11); INFARTO: el juego de cursos en OutRun 2 SP (tb_12), el coche
# (tb_13) y la carrera del arbol 1 (tb_14). Error sin "SALON: comprado", sin el arbol 230 o sin el arbol 1.
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_tablero}
al_menu() {                                                              # pelicula y titulo hasta la musica del menu
    espera_log 'VIDEO MVTS.VQV: precarga' 900; espera 3
    for _i in $(seq 1 30); do tecla Return; espera_log 'musica: MUS30' 8 && return 0; done
    return 1
}
arranca; marca
al_menu; espera 5
tecla x; espera 5
tecla Down; espera 1; tecla Down; espera 1; tecla Down; espera 1
tecla x; espera_log 'menu: guardado OR2C2C' 60; espera 5                 # menu principal
tecla x; espera 5                                                        # un jugador
for _m in 1 2 3; do tecla Right; espera 1; done                          # CONTRARRELOJ
tecla x; espera_log 'TABLERO: clave 0x24' 30; espera 4; foto tb_1        # el juego de cursos
tecla x; espera_log 'TABLERO: clave 0x25' 30; espera 4; foto tb_2        # CURSO / ETAPA
tecla x; espera_log 'TABLERO: clave 0x26' 30; espera 4; foto tb_3        # las metas
for _m in 1 2 3 4 5; do tecla Right; espera 2; done; espera 2; foto tb_4 # el curso seguido (0xcc)
tecla x; espera 1; foto tb_4b                                            # bloqueado
tecla v; espera 3; tecla Left; espera 3; foto tb_5                       # R: la meta E del reves (0xca)
tecla c; espera_log 'TABLERO: clave 0x25' 30; espera 3
tecla Right; espera 3; tecla x; espera_log 'TABLERO: clave 0x27' 30; espera 4; foto tb_6   # las etapas
tecla v; espera 3; foto tb_7                                             # R: PALM_R (0x8b) con candado
tecla x; espera 1; foto tb_7b                                            # bloqueado
tecla c; espera 3; tecla c; espera 3; tecla c; espera 5                  # a UN JUGADOR
tecla Right; espera 2                                                    # SALON
tecla x; espera_log 'SALON: SALON.DAT' 30; espera 5
tecla Down; espera 2; tecla x; espera 5                                  # ETAPAS
tecla Down; espera 2; tecla x; espera 2; tecla x                         # la segunda (0x8b), SI
espera_log 'SALON: comprado' 10 || true; espera_log 'SALON: guardado' 60 || true; espera 3; foto tb_8
tecla c; espera 4; tecla c; espera 6                                     # fuera del salon
tecla Left; espera 2; tecla x; espera_log 'TABLERO: clave 0x24' 30; espera 3
tecla x; espera 4; tecla Right; espera 3; tecla x; espera_log 'TABLERO: clave 0x27' 30; espera 4
tecla v; espera 4; foto tb_9                                             # PALM_R sin candado
tecla x; espera 6; tecla x; espera 5; tecla x; espera_log 'musica: MUS33' 30 || true; espera 4
tecla x; espera_log 'CURSO: tablero' 60 || true; espera_log 'CURSO: arbol' 120 || true; espera 25; foto tb_10
log 'TABLERO|SALON: comprado|CURSO: tablero|CURSO: arbol|pvr_mem|Out of' 30
grep -a -q 'SALON: comprado' "$SAL/flycast.txt" || { echo "no se ha comprado la etapa" >&2; ERROR=1; }
grep -a -q 'arbol 230' "$SAL/flycast.txt" || { echo "la carrera no es la del arbol 230" >&2; ERROR=1; }
kill $FLY; wait $FLY 2> /dev/null || true
cp "$SAL/flycast.txt" "$SAL/flycast_1.txt"; mkdir -p "$SAL/arranque1"; mv "$SAL"/tb_*.png "$SAL/arranque1/"
CONSERVA_VMU=1 arranca; marca
espera_log 'menu: OR2C2C en' 900
al_menu; espera 6                                                        # menu principal (licencia leida)
tecla x; espera 6; for _m in 1 2 3; do tecla Right; espera 2; done       # un jugador, CONTRARRELOJ
tecla x; espera_log 'TABLERO: clave 0x24' 30; espera 3; tecla x; espera 4; tecla Right; espera 3
tecla x; espera_log 'TABLERO: clave 0x27' 30; espera 4; tecla v; espera 4; foto tb_11
tecla c; espera 3; tecla c; espera 3; tecla c; espera 5                  # a UN JUGADOR
tecla Left; espera 2; tecla x; espera_log 'TABLERO: clave 0x24' 30; espera 4   # INFARTO
tecla Right; espera 3; foto tb_12                                        # OutRun 2 SP
tecla x; espera 6; foto tb_13                                            # el coche
tecla x; espera 5; tecla x; espera_log 'musica: MUS33' 30 || true; espera 4
tecla x; espera_log 'CURSO: tablero' 60 || true; espera_log 'CURSO: arbol' 120 || true; espera 25; foto tb_14
log 'TABLERO|CURSO: tablero|CURSO: arbol|pvr_mem|Out of' 20
grep -a -q 'variante 2, codigo -1, juego 1 -> arbol 1' "$SAL/flycast.txt" || { echo "el INFARTO no corre el arbol 1" >&2; ERROR=1; }
if grep -a -q 'pvr_mem: ERROR' "$SAL/flycast.txt" "$SAL/flycast_1.txt"; then echo "AVISO: pvr_mem: ERROR" >&2; ERROR=1; fi
