#!/usr/bin/env python3
"""conv_mision.py tu.iso salida/: las misiones de COAST 2 COAST y sus coches de carreras (el rival) de OutRun 2006
Coast 2 Coast (PS2, SLES-53998) -> MISION.DAT. Sale de tu disco: no se distribuye.

De SCRIPTS \\Races.bin (94 registros de 0x44, categoria 'Races' del indice de 4F1260 / 0x2340f8; punteros con la
tabla de reubicacion del fichero) y del ELF de PS2 (cada tabla se comprueba por el par lui / addiu del codigo que la
usa). Logica y nombres del port de PC (outrun2-decomp: platform/mission_manager.cpp 4965A0 / 496A30 / 495C20,
platform/racer_setup.cpp 47CF40, platform/race_traffic.cpp 47C8A0..47E780); direcciones de PS2:
  montaje 47CF40 = 0x1ef900 (modelo de la lista 0x1d99f0, familia 0x1d9a58 / 0x1d9aa0, clase de los especiales
  0x1d9ae8), largo del curso 44B820 = 0x1724e0 (descriptor +0x96), avance sin coche 47C8A0 = 0x1eefe0, 47CA40 =
  0x1ef230, 47BC90 = 0x1ecef8, objetivo de velocidad del coche de carreras 47A7C0 = 0x1e5dc0.

MISION.DAT (little endian):
  'MIS1', u32 n (94), u32 bytes por registro, u32 desplazamiento de las tablas
  n registros (el i: el curso es el arbol 100 + i de CURSO.DAT, tools/conv_union.py arbol_mision):
    u32 clave, subclave, +0x14 (3; 4 sin trafico ni coches de carreras), tipo (+0x20: 0 carrera, 1 eliminacion por
      tiempo, 2 derrape contra el rival, 3 conduccion limpia y rapida, 6 pillar a los rivales); f32 segundos (+0x28);
      i32 umbrales de la nota (+0x34, +0x38, +0x3C)
    i32 cfg[13]: el registro de 47CF40 (+0x1C de Races.bin) +0x00..+0x30: n coches, c4, c8 (segundos del curso del
      primero y del ultimo), cc, c10 (separacion), c14 (salida), c18 (vueltas, -1), c1c, f32 c20, c24 (lista de
      modelos), c28 (porcentaje del color 1), c2c (semilla, 0: 0xA5DE), c30 (banderas: 8 476CA0, 0x10 476DF0)
    f32 perfil[16] (+0x38, o el de defecto PS2 0x36bd00): +8/+0xC reparto, +0x10 / +0x14 escalas de 47C8A0 / 47A7C0,
      +0x1C (entero) +0x20..+0x30 los vaivenes
    f32 c3c[2] (+0x3C: base y amplitud de +0x20, o PS2 0x36bcf8)
    f32 luces[12] (+0x40, o PS2 0x40b5d8)
    u32 hay44, f32 t44[12]; u32 hay48, f32 t48[12] (tablas de los vaivenes 476CA0 / 476DF0 / 476F40)
    u32 nesp; 8 x {i32 e0 clase, e4 color, e8 rival (0..5, -1), ec plaza desde el final, e10, e14, e18; f32 e1c,
      e20; u32 hay24; f32 e24[2]} (los especiales, +0x34; registros de 0x28 acabados en un primero negativo)
    u32 nnodos; u16 largo de la etapa[16] (descriptor de tramo +0x96 de cada registro del curso, en el orden del
      arbol); u16 largo de la union[16] (descriptor de union +0x96); u16 largo del curso (0x1724e0 con vueltas -1),
      u16 0
  tablas:
    8 x {u32 n, u8 modelos[12]} (listas de modelos por c24, PS2 0x36be70)
    u8 familia[64] (pares {familia, modelo} PS2 0x40b818), u8 clase[64] (pares {clase, modelo} PS2 0x40b858)
    u8 colores[30][8] (por familia = modelo 0..29, PS2 0x40b618)
    f32 acel[5] (tabla de 472090 del coche de carreras, PS2 0x40bc98: referencia, aceleracion, frenada, fuerte, fuerte)
    f32 curva[12] (pares {radio, km/h del modelo} de 47A7C0, PS2 0x40bb68)
    f32 marcha[6] (aceleracion por marcha +208 de 472090 con los coches de carreras, x 0,002; PS2 0x40bcb0)
    i32 millas[8] por nota 0..7 (OutrunMiles.bin SETUP como lo pone 4EF280: 84B900 + 4 nota; 4EF550 con la variante 6)"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.assets import Assets
import conv_menu, conv_union

NESP = 8
NNOD = 16

def main():
    if len(sys.argv) < 3: raise SystemExit(__doc__.split('\n')[0])
    A = Assets(sys.argv[1]); sal = sys.argv[2]; os.makedirs(sal, exist_ok=True)
    E = A.elf
    u32 = lambda a: E.u32(a)
    def par(lui, add):
        """direccion que forman un lui y un addiu del codigo"""
        a, b = u32(lui), u32(add)
        assert a >> 26 == 15 and b >> 26 == 9, 'codigo inesperado en 0x%x / 0x%x' % (lui, add)
        return ((a & 0xffff) << 16) + (b & 0xffff) - (0x10000 if b & 0x8000 else 0)
    listas = par(0x1d99fc, 0x1d9a00); fam = par(0x1d9a64, 0x1d9a68); cla = par(0x1d9b04, 0x1d9b08)
    col = par(0x1efd60, 0x1efd70); perf0 = par(0x1efbb8, 0x1efbc0); c3c0 = par(0x1f01f4, 0x1f01f8)
    luz0 = par(0x1ef9fc, 0x1efa04); acel = par(0x1e5dcc, 0x1e5ddc); curva = par(0x1e60b0, 0x1e60b8)
    marcha = par(0x1e4460, 0x1e4464); assert marcha == 0x40bcb0, 'ELF inesperado'
    assert (listas, fam, cla, col, perf0, c3c0, luz0, acel, curva) == (0x36be70, 0x40b818, 0x40b858, 0x40b618, 0x36bd00,
                                                                      0x36bcf8, 0x40b5d8, 0x40bc98, 0x40bb68), 'ELF inesperado'
    lee = lambda a, n: bytes(E.lee(a, n))
    fl = lambda a, n: list(struct.unpack('<%df' % n, lee(a, 4 * n)))
    base, nprim, base2 = conv_union.tablas(A)
    largo_p = lambda i: struct.unpack('<H', lee(u32(base + 4 * i) + 0x96, 2))[0]
    largo_s = lambda i: struct.unpack('<H', lee(u32(base2 + 4 * i) + 0x96, 2))[0]

    e = [x for x in A.busca('*Races.bin') if x.paquete.startswith('SCRIPTS')][0]; d = bytes(A.datos(e))
    n, o = conv_menu._r077(d, 'Races'); assert n == 94, 'Races.bin'
    nc, nrel = struct.unpack_from('<2I', d, 0)
    rel = {struct.unpack_from('<I', d, 8 + 8 * k)[0]: struct.unpack_from('<I', d, 12 + 8 * k)[0] for k in range(nrel)}
    cats = {struct.unpack_from('<I', d, 8 + 8 * nrel + 12 * k + 8)[0]: struct.unpack_from('<I', d, 8 + 8 * nrel + 12 * k + 4)[0]
            for k in range(nc)}
    W = lambda a: struct.unpack_from('<I', d, a)[0]
    Wi = lambda a: struct.unpack_from('<i', d, a)[0]
    F = lambda a, k: list(struct.unpack_from('<%df' % k, d, a))
    regs = []
    for i in range(n):
        r = o + 0x44 * i
        out = struct.pack('<4I', W(r), W(r + 4), W(r + 0x14), W(r + 0x20)) + struct.pack('<f3i', *F(r + 0x28, 1),
                                                                                         Wi(r + 0x34), Wi(r + 0x38), Wi(r + 0x3c))
        c = rel[r + 0x1c]
        cfg = [Wi(c + 4 * k) for k in range(13)]
        out += struct.pack('<8i', *cfg[:8]) + struct.pack('<f', F(c + 0x20, 1)[0]) + struct.pack('<4i', *cfg[9:13])
        out += struct.pack('<16f', *(F(rel[c + 0x38], 16) if c + 0x38 in rel else fl(perf0, 16)))
        out += struct.pack('<2f', *(F(rel[c + 0x3c], 2) if c + 0x3c in rel else fl(c3c0, 2)))
        out += struct.pack('<12f', *(F(rel[c + 0x40], 12) if c + 0x40 in rel else fl(luz0, 12)))
        for k in (0x44, 0x48):
            out += struct.pack('<I12f', 1, *F(rel[c + k], 12)) if c + k in rel else struct.pack('<I12f', 0, *[0.0] * 12)
        esp = []
        if c + 0x34 in rel:
            a = rel[c + 0x34]
            if Wi(a) >= 0:
                while True:
                    w = [Wi(a + 4 * k) for k in range(7)]; f2 = F(a + 0x1c, 2)
                    e24 = F(rel[a + 0x24], 2) if a + 0x24 in rel else [0.0, 0.0]
                    esp.append(struct.pack('<7i2fI2f', *w, *f2, 1 if a + 0x24 in rel else 0, *e24))
                    if Wi(a + 0x28) < 0: break
                    a += 0x28
        assert len(esp) <= NESP, 'registro %d: %d especiales' % (i, len(esp))
        out += struct.pack('<I', len(esp)) + b''.join(esp) + b'\0' * (48 * (NESP - len(esp)))
        # el curso (registros de 0x78 como csc_data; +0x1C tramo, +0x20 union, +0x2C / +0x30 claves de los hijos)
        t = rel[r + 0x18]; m = cats[t]
        q = [struct.unpack_from('<30i', d, t + 0x78 * k) for k in range(m)]
        assert m <= NNOD
        lp = [largo_p(x[7]) for x in q]; ls = [largo_s(x[8]) for x in q]
        idx = {x[1]: k for k, x in enumerate(q)}
        tot, k, guarda = 0, 0, 0                             # 0x1724e0 con vueltas <= 0
        while guarda < 64:
            tot += lp[k] + 1
            if q[k][11] == -1 and q[k][12] == -1: break
            tot += ls[k] + 1
            k = idx[q[k][11] if q[k][11] != -1 else q[k][12]]; guarda += 1
        out += struct.pack('<I', m) + struct.pack('<16H', *(lp + [0] * (NNOD - m))) + struct.pack('<16H', *(ls + [0] * (NNOD - m)))
        out += struct.pack('<2H', tot & 0xffff, 0)
        regs.append(out)
    tam = len(regs[0]); assert all(len(x) == tam for x in regs)
    tab = b''
    for k in range(8):
        p, cnt = struct.unpack('<2I', lee(listas + 8 * k, 8)); assert cnt <= 12
        tab += struct.pack('<I', cnt) + lee(p, cnt) + b'\0' * (12 - cnt)
    tab += lee(fam, 64) + lee(cla, 64) + lee(col, 240)
    tab += struct.pack('<5f', *fl(acel, 5)) + struct.pack('<12f', *fl(curva, 12)) + struct.pack('<6f', *fl(marcha, 6))
    # millas por nota (4EF280: 84B918..84B900 = SETUP[0..6] de \Scripts\bin\OutrunMiles.bin, nota 6..0; 84B91C = 0 la 7)
    em = [x for x in A.busca('*OutrunMiles.bin') if x.paquete.startswith('SCRIPTS')][0]; dm = bytes(A.datos(em))
    ns, os_ = conv_menu._r077(dm, 'SETUP'); setup = struct.unpack_from('<7i', dm, os_)
    tab += struct.pack('<8i', *[setup[6 - g] for g in range(7)], 0)
    cab = 16; sal_b = b'MIS1' + struct.pack('<3I', n, tam, cab + n * tam) + b''.join(regs) + tab
    f = os.path.join(sal, 'MISION.DAT'); open(f, 'wb').write(sal_b)
    print('%s: %d misiones de %d bytes, %d bytes' % (f, n, tam, len(sal_b)))

if __name__ == '__main__':
    main()
