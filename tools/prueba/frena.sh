#!/usr/bin/env bash
# frena.sh <disc.gdi> [pantalla] [salida/]: como nav.sh hasta la carrera, acelera 12 s (fr_gas), suelta y pisa el
# freno (gatillo izquierdo = F del teclado; fr_freno_0..2 cada 0,3 s) y lo suelta (fr_suelto): los pilotos del freno
# del coche (+84 en lugar de +7C con car+38 != 0, 46A560) se ven encendidos solo en fr_freno_*.
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_frena}
arranca ${FLYCAST_OPC:-}; marca
. "$D/menu.sh"
xdotool keydown x; espera 12; foto fr_gas; xdotool keyup x
xdotool keydown f
for i in 0 1 2; do sleep 0.3; foto fr_freno_$i; done
xdotool keyup f; espera 1; foto fr_suelto
log 'RAM|VRAM|No se pud|sin ' 12
