#!/usr/bin/env bash
# arena.sh <disc.gdi> [pantalla] [salida/] [segundos (60)] [cada (2)]: las particulas de las ruedas (grava, hierba y
# polvo de race_particles) y el frenado fuera del asfalto. El GDI va compilado con -DPRUEBA_AUTO -DPRUEBA_SIN_TIEMPO
# -DPRUEBA_X=<desvio> -DPRUEBA_ARENA=<desvio> -DDIAG_PARTICULAS=<pasos> (y -DPRUEBA_ARENA_GAS=<pasos> para soltar el gas:
# la deceleracion sin gas): el piloto lleva el coche por ese desvio de la ruta (p. ej. la arena de PALM) y la consola
# serie escribe cada DIAG_PARTICULAS pasos "PART": s, x, velocidad (m/s), km/h, superficie de cada rueda (orden del
# original) y del centro, mu y resistencia de tierra (kgf) de cada rueda, lo movido por las traseras, clase del humo,
# mascara y particulas vivas. Capturas cada <cada> s (ar_000..) desde la carrera.
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_arena}; SEG=${4:-60}; CADA=${5:-2}
arranca; marca
espera_log 'RAMMAPA carrera' 900
t0=$(date +%s); i=0
while [ $(( $(date +%s) - t0 )) -lt $SEG ]; do espera $CADA; foto $(printf 'ar_%03d' $i); i=$((i + 1)); done
log '^PART' 5
exit ${ERROR:-0}
