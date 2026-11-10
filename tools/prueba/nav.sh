#!/usr/bin/env bash
# nav.sh <disc.gdi> [pantalla] [salida/]: consola NTSC (la de Flycast por defecto), VMU vacia, recorrido por el menu real
# hasta una carrera: pelicula (START la salta), titulo (START), licencia nueva (A, y LISTO en el editor con el nombre de
# fabrica), menu principal, un jugador, coche, cambio y musica (A en cada pantalla: la primera opcion) y la carrera.
# Capturas nv_1 (pelicula), nv_2 (titulo), nv_3 (licencia), nv_4 (menu principal), nv_5 (un jugador), nv_6 (coche),
# nv_7 (cambio), nv_8 (musica) y nv_fin (la carrera); error si la consola serie dice "Out of memory".
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_nav}
arranca; marca
. "$D/menu.sh"
espera 25; foto nv_fin
log 'RAM|VRAM|CURSO|No se pud|sin ' 10
