#!/usr/bin/env bash
# vmu_diag.sh <disc.gdi> [pantalla] [salida/]: VMU casi llena y pantalla de diagnostico (revision 44). Antes de arrancar
# se pone en la VMU A1 de Flycast una imagen formateada con un fichero RELLENO de 190 bloques (quedan 10 libres; el
# fichero OR2C2C ocupa 20): la licencia nueva no se puede guardar ("menu: guardado ... no hay sitio") y Y en el menu
# principal ensena "No hay espacio suficiente para guardar." con el codigo (E-7 FS ... L10 N20); captura vm_sitio.
# Despues L + R + Y (F y V pisados): la pantalla de diagnostico (raiz, FAT, directorio, "NO HAY SITIO", escritura y
# relectura de un bloque libre y su restauracion con KallistiOS); captura vm_diag; A para salir (vm_fin). Error si el
# registro no dice "vmuesc: ... -7", "vmudiag: 5 KOS vmu_block_write (restaura): 0" o "releer: BIEN".
D=$(cd "$(dirname "$0")" && pwd); . "$D/comun.sh"
GDI=${1:-}; PANTALLA=${2:-91}; SAL=${3:-/dev/shm/prueba_vmu_diag}
rm -rf "$SAL/cfg" "$SAL/dat"; mkdir -p "$SAL/dat/flycast"
python3 - "$SAL/dat/flycast/ORC2CDC_vmu_save_A1.bin" <<'EOF'
# VMU formateada como la BIOS (raiz 255, FAT 254, directorio 253..241, 200 bloques de usuario) con RELLENO en 199..10
import struct, sys
B = 512; img = bytearray(256 * B)
r = bytearray(B); r[0:16] = b'\x55' * 16
struct.pack_into('<6H', r, 0x46, 254, 1, 253, 13, 0, 200)
img[255 * B:256 * B] = r
fat = [0xFFFC] * 256
fat[255] = fat[254] = 0xFFFA
for b in range(253, 241, -1): fat[b] = b - 1
fat[241] = 0xFFFA
for b in range(199, 10, -1): fat[b] = b - 1
fat[10] = 0xFFFA
img[254 * B:255 * B] = struct.pack('<256H', *fat)
e = bytearray(32); e[0] = 0x33; struct.pack_into('<H', e, 2, 199); e[4:16] = b'RELLENO\0\0\0\0\0'
e[16:24] = bytes([0x20, 0x26, 0x11, 0x13, 0x12, 0x00, 0x00, 0x04]); struct.pack_into('<2H', e, 24, 190, 0)
img[253 * B:253 * B + 32] = e
open(sys.argv[1], 'wb').write(img)
EOF
cp "$SAL/dat/flycast/ORC2CDC_vmu_save_A1.bin" "$SAL/dat/flycast/T-00000_vmu_save_A1.bin"   # (disco sin id de producto)
CONSERVA_VMU=1 arranca; marca
espera_log 'VIDEO MVTS.VQV: precarga' 900; espera 3
tecla Return; espera_log 'MVTS.VQV: (saltado|fin)' 60; espera 4
tecla Return; espera_log 'musica: MUS30' 30; espera 5
tecla x; espera 5
tecla Down; espera 1; tecla Down; espera 1; tecla Down; espera 1
tecla x; espera_log 'menu: guardado OR2C2C' 60; espera 5
tecla d; espera_log 'menu: guardado OR2C2C' 60; espera 0.5; foto vm_sitio
espera 10
xdotool keydown f; xdotool keydown v; espera 0.5; tecla d; espera 0.5; xdotool keyup f; xdotool keyup v
espera_log 'vmudiag: (TODO BIEN|HAY FALLOS)' 60; espera 1; foto vm_diag
tecla x; espera 3; foto vm_fin
log 'vmuesc|menu: guardado|vmudiag' 40
grep -a -q -E 'vmuesc: OR2C2C en a1: -7 ' "$SAL/flycast.txt" || { echo "no sale el -7 (sin sitio)" >&2; ERROR=1; }
grep -a -q -E 'vmudiag: 5 KOS vmu_block_write \(restaura\): 0,' "$SAL/flycast.txt" || { echo "la prueba de KOS fallo" >&2; ERROR=1; }
grep -a -q -E 'vmudiag:   releer: BIEN' "$SAL/flycast.txt" || { echo "la relectura de la prueba fallo" >&2; ERROR=1; }
