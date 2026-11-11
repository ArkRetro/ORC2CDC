#!/usr/bin/env python3
"""conv_efectos.py tu.iso salida.dat: texturas de los efectos de carrera (EFECTOS.DAT).

Marcas de las ruedas: SLES-53998 0x1196f0 (el 486220 del PC) guarda en la tabla de 0x344d90 (3 registros de 0x14
bytes: +0 siguiente, +8 tamano del anillo, +0xC bufer, +0x10 textura) las texturas 0x570000, 0x570001 y 0x570002: el
recurso 0x57 (\\Common\\obj_course_obj_common_pmt.sz) y sus texturas 0, 1 y 2 (64x32: asfalto, tierra/arena,
hierba). Detras, las dos de las chispas (fuente 3 de las particulas, SLES-53998 0x10bca8 / 0x10bcd4): 0x570029 y
0x570028 (16x16; la demo las lee en ese orden: vida par, vida impar). Formato: 'EFE1', u32 n y por textura u32 ancho, alto, formato (0 ARGB1555, 1 RGB565, 2 ARGB4444), bytes y
los texeles de 16 bits twiddled (modelo.textura_dc). Sale de tu disco: no se distribuye."""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.assets import Assets
from ps2 import tex
import modelo

def main():
    if len(sys.argv) < 3: raise SystemExit(__doc__)
    A = Assets(sys.argv[1])
    b = A.bloque(A.entrada('COMMON.PS2', 'obj_course_obj_common_pmt.sz'))
    hs = tex.cabeceras(b); sal = [b'EFE1', struct.pack('<I', 5)]
    for k in range(3):                                    # 0x570000..0x570002 (SLES-53998 0x119888)
        a = tex.imagen(b, hs[k]); fmt, lista, d = modelo.textura_dc(a)
        sal.append(struct.pack('<4I', a.shape[1], a.shape[0], fmt, len(d)) + d)
        print('marca %d: %dx%d formato %d lista %d, %d bytes' % (k, a.shape[1], a.shape[0], fmt, lista, len(d)))
    for k in (0x29, 0x28):                                # chispas
        a = tex.imagen(b, hs[k]); fmt, lista, d = modelo.textura_dc(a)
        sal.append(struct.pack('<4I', a.shape[1], a.shape[0], fmt, len(d)) + d)
        print('chispa %#x: %dx%d formato %d lista %d, %d bytes' % (k, a.shape[1], a.shape[0], fmt, lista, len(d)))
    open(sys.argv[2], 'wb').write(b''.join(sal))

if __name__ == '__main__':
    main()
