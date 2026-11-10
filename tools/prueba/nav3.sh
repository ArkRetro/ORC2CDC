#!/usr/bin/env bash
# nav3.sh <disc.gdi> [pantalla] [salida/]: como nav.sh hasta la carrera y despues con el acelerador pisado (A = X del
# teclado) 40 s para que el coche ande. Capturas nv_1..nv_8, nv_9 (a los 20 s acelerando) y nv_fin.
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_nav3}
arranca; marca
. "$D/menu.sh"
xdotool keydown x; espera 20; foto nv_9; espera 20; foto nv_fin; xdotool keyup x
log 'RAM|VRAM|No se pud|sin ' 10
