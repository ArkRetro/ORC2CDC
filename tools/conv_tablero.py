#!/usr/bin/env python3
"""conv_tablero.py <iso> <dir_salida>: los tableros de eleccion de curso de UN JUGADOR del original (INFARTO y
CONTRARRELOJ) -> TABLERO.DAT para comun/menu.c (pantalla P_TABLERO). Sale de tu disco: no se distribuye.

Original (SLES-53998; fabricas de la raiz 0x239eb8 por clave, como 4D05E0 y familia del PC, platform/frontend_rankings):
 - clave 0x24 (fabrica 0x23e4e8, objeto 0x2528e8, vtable 0x419f3c: init 0x252b50, control 0x2530a0): el juego de
   cursos. La seleccion de un jugador (0x25dd58, tabla 0x375f58) lleva aqui OUTRUN / INFARTO / CONTRARRELOJ con los
   bits 5..7 de la palabra del frontal (+0x208 de la raiz: 0x20 / 0x40 / 0x80). Carrusel 0x419fb0 (0x4400a5, 4
   posiciones: OUTRUN) o 0x419f70 (0x44003b, 2), indice = bits 0..1. A: modos 1..3 -> clave 0xa (coche) con los bits
   0..1 = indice (OUTRUN 2 / 3: bits 0xcc / 0xcb, 0x253434); 4..5 -> clave 0x25 con +0x208 |= 0x800 (0x2539c4).
 - clave 0x25 (0x2558e8, vtable 0x41a24c, control 0x255e58): CURSO / ETAPA. Carrusel 0x41a280 (0x44003c) o 0x41a2c0
   (0x4400a2) por los bits 0..1 (0x257600); A: 0x375e48[i] (0x26 / 0x27), i = 0 pone 0x800, 1 lo quita (0x256520).
 - clave 0x26 (0x24f340, vtable 0x419a3c, control 0x24f818): las metas. Carrusel 0x251da0: 0x419a70 / 0x419b10
   (0x44003d / 0x4400bf, juego 0) o 0x419bb0 / 0x419c50 (0x440043 / 0x4400be, juego 1) segun +0x3f4 (R, global
   0x375e30); 6 posiciones (A..E y el curso seguido). A (0x2502cc): bits 12..17 = indice (+6 con R), 0x800; candado
   (0x250320): indice 5: 0xcc / 0xce (juego 0, R) o 0xcb / 0xcd; < 5 con R: 0xc2 + 2i (juego 0) o 0xc1 + 2i; sonido 3.
 - clave 0x27 (0x257e58, vtable 0x41a51c, control 0x258610): las etapas. Carrusel 0x2581a0: 0x41a550 / 0x41a718
   (0x440036 / 0x4400bd, juego 0) o 0x41a8e0 / 0x41aa58 (0x440042 / 0x4400bc, juego 1) segun +0x538 (R, global
   0x375f50); 15 posiciones. A (0x259198): bits 12..17 = indice (+15 con R), 0x20c bit 0 = opcion B (+0x27e68); con R
   candado 0x8b + 2i (juego 0) o 0x8a + 2i (0x2593c4), sonido 3. Abajo en la etapa 0 (0x259ea0): la opcion B con el
   bit 0xa9 / 0xab (juego 0) o 0xa8 / 0xaa, animacion +0x348 (0x41a6c8 0x4400e2, 0x41a6ec 0x4400f5, 0x41a890
   0x4400e3, 0x41a8b4 0x4400f6) y aviso +0x3e8 (0x41abd0: 0x4400e4) si el bit esta (0x258d20).
 - comunes: cursor 0x4400f2 cuadro 1 en la capa 4 (0x41a078 / 0x41a358 / 0x419d48 / 0x41ac38), candado 0x440066
   cuadro 5 en (-363, -206) capa 4 (0x41a088 / 0x419d70 / 0x41ac48), indicador de R 0x4400f3 cuadro 1 / 0 (0x419d58 /
   0x419d64, 0x41ac58 / 0x41ac64); barra de ayuda 0x23bd70 {4, 0x294}, {8, 0x295} (sin tarjeta de memoria / lista).

TABLERO.DAT: el banco SPRANI 0x44 recortado a esas escenas (conv_sprani.escribe con solo=True), un grupo de atlas por
tablero, y el bloque 'TAB1' (little endian; la barra de ayuda {4, 0x294}, {8, 0x295} es la de MENU.DAT 'CAMBIO'):
  'TAB1', u32 0
  desc car_b4[8], car_b2[4], car_c0[4], car_c1[4], car_a[4][13], car_d[4][30]   {token, primero, ultimo}
  desc cursor, candado, r_si, r_no, aviso_b, b_ani[4][3]                          (b_ani: 0x41a6c8, 0x41a6f0, 0x41a890,
                                                                                   0x41a8b8: encender 0..0xf modo 5,
                                                                                   apagar 0xf..0x1e modo 3, capa 1)
  f32 candado_x, candado_y"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conv_hud as ch
import conv_menu as cm
import conv_sprani as cs

BANCO = 0x44
TABLAS = [(0x419fb0, 8), (0x419f70, 4), (0x41a280, 4), (0x41a2c0, 4),
          (0x419a70, 13), (0x419b10, 13), (0x419bb0, 13), (0x419c50, 13),
          (0x41a550, 30), (0x41a718, 30), (0x41a8e0, 30), (0x41aa58, 30),
          (0x41a078, 1), (0x41a088, 1), (0x419d58, 1), (0x419d64, 1), (0x41abd0, 1),
          (0x41a6c8, 3), (0x41a6f0, 3), (0x41a890, 3), (0x41a8b8, 3)]


def main():
    if len(sys.argv) < 3: raise SystemExit(__doc__.split('\n')[0])
    D = ch.Disco(sys.argv[1]); sal = sys.argv[2]; os.makedirs(sal, exist_ok=True)
    E = D.elf
    u = lambda a: struct.unpack_from('<I', E, a - 0xfff80)[0]
    def par(a, b):                                           # lui / addiu
        x, y = u(a), u(b); assert x >> 26 == 15 and y >> 26 == 9, 'codigo inesperado en 0x%x / 0x%x' % (a, b)
        return ((x & 0xffff) << 16) + (y & 0xffff) - (0x10000 if y & 0x8000 else 0)
    def inm(a, v):                                           # addiu rt, zero, v
        w = u(a); assert w >> 21 == 0x120 and (w & 0xffff) == (v & 0xffff), 'codigo inesperado en 0x%x: %08x' % (a, w)
    # ---- las tablas por su codigo
    assert par(0x252d00, 0x252d04) == 0x419fb0 and par(0x252e38, 0x252e3c) == 0x419f70
    inm(0x252d14, 8); inm(0x252e4c, 4)
    assert par(0x257614, 0x25761c) == 0x41a2c0 and par(0x257620, 0x257624) == 0x41a280
    assert par(0x251e90, 0x251e9c) == 0x419b10 and par(0x251ea0, 0x251eac) == 0x419a70
    assert par(0x251eb4, 0x251ec0) == 0x419c50 and par(0x251ec4, 0x251ecc) == 0x419bb0
    inm(0x251ed0, 0x18)
    assert par(0x2581a8, 0x2581b0) == 0x41aa58 and par(0x2581b4, 0x2581bc) == 0x41a8e0
    assert par(0x2581c4, 0x2581cc) == 0x41a718 and par(0x2581d0, 0x2581d4) == 0x41a550
    inm(0x2581e8, 0x1e)
    assert par(0x2533ac, 0x2533b8) == 0x41a078 and par(0x25345c, 0x253468) == 0x41a088
    assert par(0x258c68, 0x258c74) == 0x41ac48 and par(0x258dbc, 0x258dc8) == 0x41abd0
    assert par(0x259f20, 0x259f2c) == 0x41a8b8 and par(0x259fdc, 0x259fe8) == 0x41a6c8
    assert par(0x259f98, 0x259fa4) == 0x41a6f0 and par(0x259f60, 0x259f70) == 0x41a890
    inm(0x259f44, 1); inm(0x259f48, 5); inm(0x25a0a8, 1); inm(0x25a0ac, 3)          # capa 1; modo 5 / 3
    # candado en (-363, -206) (0x253480: lui 0xc3b5 / ori 0x8000; 0x253490: lui 0xc34e)
    assert u(0x253480) >> 16 == 0x3c01 and u(0x253480) & 0xffff == 0xc3b5 and u(0x253484) & 0xffff == 0x8000
    assert u(0x253490) & 0xffff == 0xc34e
    # bits de los candados (0x253434 / 0x250364.. / 0x258bdc.. / 0x259ef8..)
    inm(0x253434, 0xcb); inm(0x253438, 0xcc); inm(0x250364, 0xcb); inm(0x250370, 0xcd); inm(0x250378, 0xcc)
    inm(0x25037c, 0xce); assert u(0x250490) & 0xffff == 0xc1 and u(0x250498) & 0xffff == 0xc2
    assert u(0x258be0) & 0xffff == 0x8b and u(0x258c30) & 0xffff == 0x8a and u(0x2593e0) & 0xffff == 0x8b
    inm(0x259ef8, 0xa8); inm(0x259efc, 0xaa); inm(0x259f74, 0xa9); inm(0x259f78, 0xab)
    # destinos: 0x375f58 (un jugador), 0x375e48 (curso / etapa), 0x375e60 (etapas)
    assert [u(0x375f58 + 8 * k) for k in range(5)] == [0x3a, 0x24, 0x24, 0x24, 0x2b]
    assert [u(0x375e48 + 8 * k) for k in range(2)] == [0x26, 0x27]
    assert all(u(0x375e60 + 8 * k) == 0xa for k in range(15))
    # barra de ayuda sin la lista (0x25303c..0x253068: {4, 0x294}, -1, -1, {8, 0x295})
    inm(0x25304c, 4); inm(0x253050, 0x294); inm(0x253068, 8); inm(0x253040, 0x295)

    out = bytearray(b'TAB1') + bytes(4)
    toks = set()
    for t, n in TABLAS:
        for k in range(n):
            tok, f0, f1 = u(t + 12 * k), u(t + 12 * k + 4), u(t + 12 * k + 8)
            assert tok >> 16 == 0x44, 'tabla 0x%x: %x' % (t, tok)
            out += struct.pack('<3I', tok, f0, f1); toks.add(tok & 0xffff)
    out += struct.pack('<2f', -363.0, -206.0)
    grupos = [('B', [0x3b, 0xa5, 0xf2, 0x66]), ('C', [0x3c, 0xa2]), ('A', [0x3d, 0xbf, 0x43, 0xbe, 0xf3]),
              ('D', [0x36, 0xbd, 0x42, 0xbc, 0xe2, 0xe3, 0xf5, 0xf6, 0xe4])]
    assert set(k for _, ks in grupos for k in ks) == toks, sorted(toks)
    ruta = os.path.join(sal, 'TABLERO.DAT')
    cs.escribe(D, BANCO, ruta, grupos, bytes(out), vq=ch.vq_pequeno, solo=True)
    print('TABLERO.DAT: %d bytes (tablas TAB1 %d bytes)' % (os.path.getsize(ruta), len(out)))


if __name__ == '__main__':
    main()
