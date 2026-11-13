#!/usr/bin/env bash
# vmu.sh <disc.gdi> [pantalla] [salida/]: guardado en la VMU y vuelta a leer. Primer arranque con la VMU vacia: titulo,
# licencia nueva (LISTO) y menu principal (se guarda la licencia: "menu: guardado OR2C2C ... bien, releido igual"),
# Y en el menu principal (GUARDAR: otro guardado) y fuera. La imagen de la VMU A1 de Flycast pasa por
# tools/comprueba_vmu.py (lo que miraria la BIOS). Segundo arranque con la misma VMU: tiene que leer la licencia
# ("menu: OR2C2C en a1 ... licencia 0") y el titulo lleva al menu principal sin crear otra. Capturas del primer
# arranque en arranque1/: vm_1 (licencia nueva), vm_2 (menu principal tras guardar), vm_3 (tras Y: el aviso "Exito al
# guardar."); del segundo: vm_4 (titulo) y vm_5 (el menu principal, sin pasar por la licencia).
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_vmu}
arranca; marca
espera_log 'VIDEO MVTS.VQV: precarga' 900; espera 3
tecla Return; espera_log 'MVTS.VQV: (saltado|fin)' 60; espera 4
tecla Return; espera_log 'musica: MUS30' 30; espera 5; foto vm_1
tecla x; espera 5
tecla Down; espera 1; tecla Down; espera 1; tecla Down; espera 1
tecla x; espera_log 'menu: guardado OR2C2C' 60; espera 5; foto vm_2
tecla d; espera 0.5; foto vm_3a; espera_log 'menu: guardado OR2C2C' 60; espera 0.3; foto vm_3
log 'menu: (guardado|sin|OR2C2C)' 6
kill $FLY; wait $FLY 2> /dev/null || true
IMG=$(ls -t "$SAL"/dat/flycast/*vmu_save_A1.bin | head -1)
python3 "$D/../comprueba_vmu.py" "$IMG" OR2C2C || ERROR=1
cp "$SAL/flycast.txt" "$SAL/flycast_1.txt"; mkdir -p "$SAL/arranque1"; mv "$SAL"/vm_*.png "$SAL/arranque1/"   # (arranca borra las capturas)
CONSERVA_VMU=1 arranca; marca
espera_log 'menu: OR2C2C en' 900
log 'menu: (OR2C2C|cabecera)' 3
grep -a -q -E 'menu: OR2C2C en a1, [0-9]+ bytes, licencia 0' "$SAL/flycast.txt" || { echo "no se ha leido la licencia" >&2; ERROR=1; }
espera_log 'VIDEO MVTS.VQV: precarga' 900; espera 3
tecla Return; espera_log 'MVTS.VQV: (saltado|fin)' 60; espera 4; foto vm_4
tecla Return; espera 8; foto vm_5                                     # con la licencia leida: el menu principal sin crear otra
