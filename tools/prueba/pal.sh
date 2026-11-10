#!/usr/bin/env bash
# pal.sh <disc.gdi> [pantalla] [salida/]: consola PAL (region Europa, emision PAL, cable de TV: sin VGA la demo pregunta
# el modo de pantalla con MODOV.DAT, como el original: 50Hz / 60Hz - Prueba), elige 50 Hz (A en la primera) y hace el recorrido de nav.sh hasta la carrera. Capturas
# pal_0 (la eleccion), pal_1..pal_8 y pal_fin; error si no arranca a 50 Hz o si sale "Out of memory".
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_pal}
arranca -config config:Dreamcast.Region=2 -config config:Dreamcast.Broadcast=1 -config config:Dreamcast.Cable=2
marca; espera 6; foto pal_0                            # eleccion del modo de pantalla (50Hz resaltado)
tecla x                                                # A = 50Hz
P=pal; . "$D/menu.sh"
espera 10; foto pal_fin
grep -a -q 'PAL' "$SAL/flycast.txt" || { echo "no arranco en PAL" >&2; ERROR=1; }
log 'vid_set_mode' 3; log 'RAM|VRAM|No se pud|sin ' 10
