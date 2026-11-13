#!/usr/bin/env python3
"""conv_efectos.py tu.iso salida.dat: texturas de los efectos de carrera (EFECTOS.DAT).

Marcas de las ruedas: SLES-53998 0x1196f0 (el 486220 del PC) guarda en la tabla de 0x344d90 (3 registros de 0x14
bytes: +0 siguiente, +8 tamano del anillo, +0xC bufer, +0x10 textura) las texturas 0x570000, 0x570001 y 0x570002: el
recurso 0x57 (\\Common\\obj_course_obj_common_pmt.sz) y sus texturas 0, 1 y 2 (64x32: asfalto, tierra/arena,
hierba). Detras, las dos de las chispas (fuente 3 de las particulas, SLES-53998 0x10bca8 / 0x10bcd4): 0x570029 y
0x570028 (16x16; la demo las lee en ese orden: vida par, vida impar), y el bloque 'ATR1' (tramos del choque contra
el muro por etapa, ver abajo). Formato: 'EFE1', u32 n y por textura u32 ancho, alto, formato (0 ARGB1555, 1 RGB565, 2 ARGB4444), bytes y
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
    # tramos del choque contra el muro por etapa (bloque 'ATR1'): filas de 64 bytes (16 parejas de posiciones de
    # carretera, -1 cierra) de las tablas del ejecutable indexadas por el descriptor de tramo (+0x00 = su indice; 0x176700):
    # atrapamiento del choque 0x3dee58 (0x19a6c0), del roce 0x3dfed8 (0x19a768) y desvio del estado 1 al 5 0x3d2f38
    # (0x182d84). Aqui van por SEGnn (el segmento de STAGE.PS2 de cada descriptor), las tres filas seguidas.
    import conv_union as cu
    E = A.elf; base, n, _ = cu.tablas(A)
    assert n == 66 and base == 0x3692a8, 'tabla de descriptores inesperada (direcciones de SLES-53998)'
    filas = [b'\xff' * 192] * n
    for ti in range(n):
        seg = cu.segmento_de(A, cu.cadena(E, cu.u32(E, base + 4 * ti) + 0x0c))
        filas[seg] = b''.join(E.lee(t + 64 * ti, 64) for t in (0x3dee58, 0x3dfed8, 0x3d2f38))
    sal.append(b'ATR1' + struct.pack('<I', n) + b''.join(filas))
    print('tramos de choque: %d etapas, %d con atrapamiento' % (n, sum(f[:2] != b'\xff\xff' for f in filas)))
    sal.append(b'PAR1' + struct.pack('<I', 3))
    for k in (0x2a, 0x2b, 0x2d):                          # grava, hierba, humo (race_particles: fuentes 1, 2 y 4)
        a = tex.imagen(b, hs[k]); fmt, lista, d = modelo.textura_dc(a)
        sal.append(struct.pack('<4I', a.shape[1], a.shape[0], fmt, len(d)) + d)
        print('particula %#x: %dx%d formato %d lista %d, %d bytes' % (k, a.shape[1], a.shape[0], fmt, lista, len(d)))
    dsc = bytearray(b'\xff' * n)                          # descriptor de tramo de cada SEGnn (su +0x00)
    for ti in range(n):
        d0 = cu.u32(E, base + 4 * ti)
        dsc[cu.segmento_de(A, cu.cadena(E, d0 + 0x0c))] = cu.u32(E, d0) & 0xff
    sal.append(b'DSC1' + struct.pack('<I', n) + bytes(dsc) + b'\0' * (-n & 3))
    print('descriptores por segmento: %s' % ' '.join('%d' % x for x in dsc))
    open(sys.argv[2], 'wb').write(b''.join(sal))

if __name__ == '__main__':
    main()
