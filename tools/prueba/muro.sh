#!/usr/bin/env bash
# muro.sh <disc.gdi> [pantalla] [salida/] [segundos (240)]: los muros y el suelo de la carroceria. El GDI va compilado
# con -DPRUEBA_AUTO -DPRUEBA_MURO -DPRUEBA_SIN_TIEMPO (y PRUEBA_CURSO / PRUEBA_NODO / PRUEBA_RAMA / PRUEBA_FIN para
# elegir arbol, etapa, rama y sitio; -DMURO_ANTES para comparar con los muros y el suelo de antes): el piloto conduce y
# cada 8 s gira a fondo contra el muro 1 s o roza 3,3 s. Espera a la carrera, la deja correr y cuenta lo que mide la
# demo cada cuadro (demo/main.c prueba_mide): MURO_DENTRO (la carroceria mas de 0,25 m dentro de un muro de la
# colision), RUEDA_HUNDIDA (una rueda mas de 5 cm bajo el suelo), TRAF_HUNDIDO (lo mismo en el trafico), CAMARA_SALTO
# (el ojo se mueve mas de 0,6 m respecto al coche en un cuadro o queda a mas de 4 m sobre el), PlWrecker y
# MURO_FUERA (ninguna esquina sobre la colision). Capturas cada 15 s (m_000..). Error si hay MURO_DENTRO o
# MURO_FUERA; las ruedas se cuentan (quedan unos cuadros de 5-10 cm derrapando o en el aire: docs/PARIDAD.md, Muros y
# suelo de la carroceria del original).
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_muro}; SEG=${4:-240}
arranca; marca
espera_log 'RAMMAPA carrera' 900
t0=$(date +%s); i=0
while [ $(( $(date +%s) - t0 )) -lt $SEG ]; do espera 15; foto $(printf 'm_%03d' $i); i=$((i + 1)); done
for k in MURO_DENTRO RUEDA_HUNDIDA TRAF_HUNDIDO CAMARA_SALTO 'PlWrecker' MURO_FUERA; do
    echo "$k: $(grep -a -c "$k" "$SAL/flycast.txt")"
done
log '^MIDE' 3
grep -a -q 'MURO_DENTRO\|MURO_FUERA' "$SAL/flycast.txt" && ERROR=1
exit ${ERROR:-0}
