#!/usr/bin/env python3
"""conv_salon.py <iso> <dir_salida>: el SALON del original (showroom: la tienda de las millas OutRun del menu UN
JUGADOR) -> SALON.DAT para comun/menu.c. Sale de tu disco: no se distribuye.

Original: SLES-53998 0x25b338..0x25db20 (vtable 0x41adec: 0x25b338 destructor, 0x25b418 init, 0x25b5d0 control, 0x25b8c0
dibujo, 0x25b998 suspender) = 4D4230.. del PC (outrun2-decomp platform/frontend_showroom.cpp, tablas 5CAE00..). La
estructura es la del PC con otros tokens del banco 0x44 y otros desplazamientos en el objeto (+0x40c estado, +0xc84
base de la pagina, +0xbdc estado del icono de comprado, +0x2124 / +0x25cc los textos del precio y de las millas).

SALON.DAT: el banco SPRANI 0x44 (ani_SPRANI_MENU) recortado a las escenas del salon (conv_sprani.escribe con
solo=True), un grupo de atlas por pagina (las texturas de cada escena se suben al crearla), mas:
 - el uso 'F14': los glifos de la fuente 14 (los textos del salon: 0x26cd40(0xe) en 0x25b4fc / 0x25d2a4) que usan sus
   textos, como en C2C.DAT (conv_c2c.py);
 - el bloque 'SAL1' (extra de SPR1) con las tablas del ELF (little endian):
     'SAL1', u32 desplazamiento de los textos (desde 'SAL1')
     desc filas[257]  (0x41ae20: {token, primero, ultimo}; 0x25b468 / 0x25b470; las 252.. son los iconos de la
                       ventana de compra: 0x25d65c..0x25d9e0)
     u32 precio[0xcf] (0x629ee0 que rellena 0x1f3c90: 10 x i y los pares {objeto, precio} de PRICES de
                       \\Scripts\\bin\\OutrunMiles.bin; 0x1f4198)
     i32 fuera[26]    (0x41ad38: los objetos que no se venden en esta version; 0x25d790)
     coches: i32 modelo[30], bit[30], ficha[30] (0x41bda0: {modelo, bit}; ficha = indice del modelo en 0x375968, como
                       0x25bf68), i8 color[30] (0x3d4b80 por modelo: 0x18a110), 2 de relleno
     colores: i32 modelo[30] (0x41be90), ficha[30]; filas[110] {u8 registro, i8 color} (0x41ba30: 0x25cc14 / 0x25ce54)
     u32 pista[21] (0x375dcc = 0x375db0 + 4 x 7: 0x25ca10 / 0x25ca4c; suena la pista + 0x21)
     u32 texto de la ventana (0x3a4 "COMPRAR OBJETO": 0x25d5ac), u32 formato de las cifras ("%dM", 0x41ad00: 0x25b8dc)
     u16 ancho, alto[16] de las imagenes 0x30040..0x3004f de la ventana (spr_etc_xst; uso 'VENT', un cuadro cada una)
     textos: cadenas de bytes (codigos de la fuente 14) acabadas en 0
Valores que van en el codigo de menu.c (las instrucciones se comprueban aqui): paginas (0x25bce8, 0x25c038, 0x25c4e0,
0x25c750, 0x25ca80, 0x25cf00: base de los objetos, filas de la lista, escena de la tira, ultimo indice, paso, base,
titulo), lista en (-180, -50) con 19 de paso y capa 1 (0x24e038 / 0x24e040 / 0x24e060), millas en (480, 87) (0x25b534),
precio en (480, 368) (0x25d2c8), ventana (330, 180, 239, 130, capa 0xc) (0x25d5c4..0x25d5f8), icono de la ventana en la
capa 0xd."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conv_hud as ch
import conv_menu as cm
import conv_sprani as cs

BANCO = 0x44
FUENTE = 14
FILAS = 0x41ae20
N_FILAS = 257
# paginas (control 0x25b5d0, tabla de saltos 0x41aca0; la principal 0x25bce8): (codigo, base, filas, tira, ultimo,
# paso, base de la tira, titulo)
PAGINAS = [(0x25bce8, 0xcf, (0x28, 0x2d), 0x44007d, 4, 0x14, 0xa, None),        # principal (0xcf: ninguno)
           (0x25c038, -1, (0x2d, 0x49), 0x44007e, 0x1b, 0xa, 9, 0x13),           # coches (registros: bit propio)
           (0x25c4e0, 0x8a, (0xb7, 0xd9), 0x44007f, 0x21, 0xa, 9, 0x16),         # escenarios
           (0x25c750, 0xac, (0xd9, 0xee), 0x4400a9, 0x14, 0xa, 9, 0x1c),         # musica
           (0x25ca80, 0x1c, (0x49, 0xb7), 0x440080, 0x6d, 0xa, 9, 0x1f),         # colores
           (0x25cf00, 0xc1, (0xee, 0xfc), 0x4400c5, 0xd, 0xa, 9, 0x19)]          # extras


BARRA = [(4, 0x294), None, (0x4000, 0x299), (8, 0x295)]


def compone(ps):
    """las piezas (textura de 'tx', rectangulo 'src', matriz 'M' de solo traslacion) en una imagen RGBA recortada:
    (imagen, x, y) de su esquina en la pantalla de 640x480"""
    lienzo = np.zeros((480, 640, 4), float)
    for p in ps:
        M = p['M']; assert np.allclose(M[:2, :2], np.eye(2)), 'pieza con escala o giro'
        l, t, r, b = (int(round(v)) for v in p['src'])
        T = p['tx'][p['tex']]; a = T[t:b, l:r].astype(float)
        assert abs((r - l) - p['w']) < 0.5 and abs((b - t) - p['h']) < 0.5, 'pieza escalada'
        x, y = int(round(M[2, 0])), int(round(M[2, 1])); al = a[..., 3:4] / 255 * p.get('alfa', 1.0)
        d = lienzo[y:y + a.shape[0], x:x + a.shape[1]]
        d[..., :3] = a[..., :3] * al + d[..., :3] * (1 - al); d[..., 3:4] = al * 255 + d[..., 3:4] * (1 - al)
    ys, xs = np.nonzero(lienzo[..., 3] > 0)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    return np.clip(np.round(lienzo[y0:y1, x0:x1]), 0, 255).astype(np.uint8), int(x0), int(y0)


def main():
    if len(sys.argv) < 3: raise SystemExit(__doc__.split('\n')[0])
    D = ch.Disco(sys.argv[1]); sal = sys.argv[2]; os.makedirs(sal, exist_ok=True)
    E = D.elf
    u = lambda a: struct.unpack_from('<I', E, a - 0xfff80)[0]
    i = lambda a: struct.unpack_from('<i', E, a - 0xfff80)[0]
    def par(a, b):                                           # lui / addiu
        x, y = u(a), u(b); assert x >> 26 == 15 and y >> 26 == 9, 'codigo inesperado en 0x%x / 0x%x' % (a, b)
        return ((x & 0xffff) << 16) + (y & 0xffff) - (0x10000 if y & 0x8000 else 0)
    def inm(a, v):                                           # addiu rt, zero, v
        w = u(a); assert w >> 21 == 0x120 and (w & 0xffff) == (v & 0xffff), 'codigo inesperado en 0x%x: %08x' % (a, w)
    def lui(a, v):
        w = u(a); assert w >> 26 == 15 and (w & 0xffff) == v, 'codigo inesperado en 0x%x: %08x' % (a, w)

    # ---- tablas por su codigo
    assert par(0x25b468, 0x25b470) == FILAS and par(0x25d1f0, 0x25d1fc) == FILAS
    assert par(0x25c0bc, 0x25c0c0) == 0x41bda0 and par(0x25caf4, 0x25caf8) == 0x41be90
    assert par(0x25cc0c, 0x25cc14) == 0x41ba30 and par(0x25ce50, 0x25ce54) == 0x41ba30
    assert par(0x25ca44, 0x25ca4c) == 0x375db0 and u(0x25ca5c) == 0x24420007, 'pista: tabla + 4 x (i + 7)'
    assert par(0x18a114, 0x18a118) == 0x3d4b80 and par(0x1f4198, 0x1f41a0) == 0x629ee0
    assert par(0x25d790, 0x25d798) == 0x41ad38 and u(0x25d85c) == 0x2882001a, 'fuera: 26'
    assert par(0x25b8cc, 0x25b8dc) == 0x41ad00 and par(0x25b504, 0x25b50c) == 0x41ac98
    inm(0x25d5ac, 0x3a4)
    # paginas
    def hay(a0, n, w, m=0xffffffff): return any(u(a) & m == w for a in range(a0, a0 + n, 4))
    ADDIU = 0xffe0ffff                                       # addiu rt, zero, v (cualquier rt)
    for cod, base, (f0, f1), tira, ult, paso, b0, tit in PAGINAS:
        assert hay(cod, 0x80, 0x24000000 | (base & 0xffff), ADDIU), 'base %x' % cod
        assert hay(cod, 0x300, 0x24000000 | f0, ADDIU), 'primera fila %x' % cod
        assert hay(cod, 0x300, 0x28020000 | f1, 0xfc1fffff), 'ultima fila %x' % cod     # slti v0, rs, f1
        assert hay(cod, 0x300, 0x34a50000 | (tira & 0xffff)), 'tira %x' % cod
        assert hay(cod, 0x300, 0x24060000 | ult) and hay(cod, 0x300, 0x24070000 | paso) and \
            hay(cod, 0x300, 0x24080000 | b0), 'tira %x' % cod
        if tit is not None: assert hay(cod, 0x300, 0x24050000 | tit), 'titulo %x' % cod
    # lista: 19 de paso, (-180, -50), capa 1, limite 7 (0x60 la principal)
    lui(0x25bd4c, 0x4198); lui(0x25bd5c, 0xc248); lui(0x25bd64, 0xc334); inm(0x25bd7c, 1); inm(0x25bd24, 0x60)
    inm(0x25c1dc, 7)
    # textos: millas (480, 87) color 0xFF3F474A, precio (480, 368) blanco, fuente 14
    lui(0x25b534, 0x43f0); lui(0x25b53c, 0x42ae); lui(0x25b518, 0xff3f); inm(0x25b500, 0xe)
    lui(0x25d2c8, 0x43f0); lui(0x25d2d0, 0x43b8); inm(0x25d2a8, 0xe)
    # ventana (330, 180, 239, 130), capa 0xc; icono capa 0xd
    lui(0x25d5c8, 0x43a5); lui(0x25d5d0, 0x4334); lui(0x25d5dc, 0x436f); lui(0x25d5e8, 0x4302); inm(0x25d5f8, 0xc)
    inm(0x25d668, 0xd); inm(0x25d6cc, 0xd); inm(0x25d734, 0xd)

    filas = [(u(FILAS + 12 * k), u(FILAS + 12 * k + 4), u(FILAS + 12 * k + 8)) for k in range(N_FILAS)]
    assert filas[4] == (0x440053, 0, 20) and filas[16] == (0x4400bb, 0, 30) and filas[252] == (0x44005c, 0, 0)
    assert u(FILAS + 12 * N_FILAS) >> 16 != 0x44, 'filas: 257'
    # precios (0x1f3c90: 10 x i; luego PRICES)
    from ps2.assets import Assets
    As = Assets(sys.argv[1])
    em = [x for x in As.busca('*OutrunMiles.bin') if x.paquete.startswith('SCRIPTS')][0]; dm = bytes(As.datos(em))
    precio = [10 * k for k in range(0xcf)]
    n, o = cm._r077(dm, 'PRICES')
    for k in range(n):
        it, pr = struct.unpack_from('<2I', dm, o + 8 * k); assert it < 0xcf; precio[it] = pr
    print('  PRICES: %d precios (%s)' % (n, ', '.join('%d:%d' % (k, precio[k]) for k in (0, 1, 2, 28, 138, 172, 193))))
    fuera = [i(0x41ad38 + 4 * k) for k in range(26)]
    orden = [u(0x375968 + 4 * k) for k in range(30)]
    col_mod = [struct.unpack_from('<b', E, 0x3d4b80 - 0xfff80 + m)[0] for m in range(30)]
    # el bit de cada coche del salon es el de la seleccion de coche (0x3759E0 por ficha) y el modelo el de la ficha
    desb = [struct.unpack_from('<h', E, 0x3759e0 - 0xfff80 + 2 * k)[0] for k in range(30)]

    out = bytearray(b'SAL1') + bytes(4)
    W = lambda *v: out.extend(struct.pack('<%dI' % len(v), *[x & 0xffffffff for x in v]))
    cad = cm.textos(D, os.environ.get('OR2_IDIOMA', 'es'))
    usados = set(); ref = []
    def texto(s):
        o_ = len(out); W(0); ref.append((o_, s)); usados.update(s)
    for t in filas: W(*t)
    W(*precio)
    W(*fuera)
    cm_, cb, cf_ = [], [], []
    for k in range(30):
        m, b = u(0x41bda0 + 8 * k), u(0x41bda4 + 8 * k)
        fi = orden.index(m) if m in orden else 0
        if k < 28: assert desb[fi] == b or (fi < 2 and b in (0, 1)), 'bit del coche %d: %d / %d' % (k, b, desb[fi])
        cm_.append(m); cb.append(b); cf_.append(fi)
    W(*cm_); W(*cb); W(*cf_)
    out.extend(struct.pack('<30b', *[col_mod[m] for m in cm_])); out.extend(bytes(2))
    mods = [u(0x41be90 + 4 * k) for k in range(30)]
    W(*mods); W(*[orden.index(m) for m in mods])
    for k in range(110):
        r, c = u(0x41ba30 + 8 * k), struct.unpack_from('<b', E, 0x41ba34 - 0xfff80 + 8 * k)[0]
        assert r < 30; out.extend(struct.pack('<Bb', r, c))
    W(*[u(0x375dcc + 4 * k) for k in range(21)])
    texto(cad[0x3a4])
    fm = bytes(E[0x41ad00 - 0xfff80:0x41ad00 - 0xfff80 + 8]).split(b'\0')[0]; assert fm == b'%dM'; texto(fm)
    # la ventana de la compra (0x271ca8, el dibujo de la ventana = 48C5F0 del PC): imagenes 0x30040..0x3004f (42D300 =
    # 0x117b88: banco 3 = spr_etc_xst, ver conv_hud imagenes), su ancho y alto
    xs = D._ent(D.p_spr, D.n_spr.index('spr_etc_xst.sz')); Te = ch.texturas(xs); vent = []
    for k in range(0x40, 0x50):
        ti, l, tp, r, b = ch.imagen_xst(xs, k); vent.append((ti, l, tp, r, b))
        out.extend(struct.pack('<2H', int(round(r - l)), int(round(b - tp))))
    assert len(set(v[0] for v in vent)) == 1, 'ventana: una textura'
    for a_, k in ((0x271d38, 0x4a), (0x271d7c, 0x49), (0x271e9c, 0x40), (0x271ee4, 0x4f), (0x271f44, 0x43), (0x271f70, 0x4e),
                  (0x271fbc, 0x4d), (0x271ff4, 0x42), (0x272040, 0x41), (0x272084, 0x4c), (0x2720d4, 0x4b)):
        assert u(a_) == 0x34a50000 | k, 'ventana: imagen en 0x%x' % a_
    usados |= set(b'0123456789-')
    t0 = len(out); struct.pack_into('<I', out, 4, t0)
    pool = bytearray(b'\0')
    for o_, s_ in ref: struct.pack_into('<I', out, o_, t0 + len(pool)); pool += s_ + b'\0'
    out += pool; out += bytes((-len(out)) % 4)

    # ---- escenas por pagina (un grupo de atlas cada una)
    toks = lambda f0, f1: sorted(set(filas[k][0] & 0xffff for k in range(f0, f1)))
    comun = sorted(set(filas[k][0] & 0xffff for k in list(range(0, 34)) + list(range(252, 257))))
    grupos = [('COMUN', comun + [0x7d] + toks(0x28, 0x2d))]
    for nom, (cod, base, (f0, f1), tira, *_r) in zip(['COCHES', 'ESCENARIOS', 'MUSICA', 'COLORES', 'EXTRAS'], PAGINAS[1:]):
        grupos.append((nom, sorted(set(toks(f0, f1) + [tira & 0xffff]) - set(k for _, ks in grupos for k in ks))))
    for nom, ks in grupos: print('  grupo %s: %s' % (nom, ' '.join('%x' % k for k in ks)))

    Ft = cm.fuentes_xst(D); Fu = cm.Fuente(D, FUENTE, Ft); Fa = cm.Fuente(D, cm.FUENTE_AYUDA, Ft)
    assert u(0x25b580) == 0x24050004 and u(0x25b584) == 0x24060294 and u(0x25b590) == 0x24094000 and \
        u(0x25b594) == 0x240a0299 and u(0x25b59c) == 0x240b0008 and u(0x25b578) == 0x24020295, 'barra de ayuda'
    usados.discard(0x20)
    chars = sorted(c for c in usados if 0 <= c - Fu.pri < Fu.n)
    falta = sorted(c for c in usados if not 0 <= c - Fu.pri < Fu.n); assert not falta, falta
    sin_vq = set()
    def antes(hud):
        por = 256 // Fu.w; nfil = (len(chars) + por - 1) // por
        img = np.zeros((nfil * Fu.h, por * Fu.w, 4), np.uint8)
        for j, c in enumerate(chars):
            k = c - Fu.pri; l, tp = (k & 15) * Fu.w, (k >> 4) * Fu.h
            x, y = (j % por) * Fu.w, (j // por) * Fu.h
            img[y:y + Fu.h, x:x + Fu.w] = Ft[FUENTE][tp:tp + Fu.h, l:l + Fu.w]
        ti = hud.textura(('SALON', 'F14'), img); sin_vq.add(ti)
        g = hud.grupo('F14', Fu.n)
        for j, c in enumerate(chars):
            x, y = (j % por) * Fu.w, (j // por) * Fu.h
            si = hud.sprite(ti, (x, y, x + Fu.w, y + Fu.h), Fu.w, Fu.h)
            hud.pie.append((g, c - Fu.pri, si, 0, float(-Fu.met[c - Fu.pri][0]), 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0))
        hud.uso('F14', g, 0, 0)
        print('  F14: %d glifos en una textura de %dx%d' % (len(chars), img.shape[1], img.shape[0]))
        # barra de ayuda del salon (0x25b578..0x25b59c: 0x23bd70 con {4, 0x294}, {-1, -1}, {0x4000, 0x299}, {8, 0x295}:
        # ELEGIR, LICENCIA y ATRAS) con los botones de la Dreamcast de MENU.DAT (conv_menu.barra_ayuda, DESVIACION de la
        # revision 42), compuesta en una imagen (las piezas solo se trasladan)
        img, x0, y0 = compone(cm.barra_ayuda(D, Fa, cad, cm.iconos_dc(D, Fa), BARRA))
        ti = hud.textura(('SALON', 'BARRA'), img); sin_vq.add(ti)
        g = hud.grupo('BARRA', 1)
        si = hud.sprite(ti, (0, 0, img.shape[1], img.shape[0]), img.shape[1], img.shape[0])
        hud.pie.append((g, 0, si, 0, float(x0), float(y0), 0.0, 0.0, 0.0, 1.0, 1.0, 1.0))
        hud.uso('BARRA', g, 0, 0)
        # las 16 imagenes de la ventana, un cuadro cada una (sin VQ: bordes de 1 a 3 pixeles)
        ti = hud.textura(('SALON', 'VENT'), Te[vent[0][0]]); sin_vq.add(ti)
        g = hud.grupo('VENT', 16)
        for k, (_, l, tp, r, b) in enumerate(vent):
            si = hud.sprite(ti, (l, tp, r, b), r - l, b - tp)
            hud.pie.append((g, k, si, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0))
        hud.uso('VENT', g, 0, 0)
        print('  BARRA: %dx%d en (%d, %d)' % (img.shape[1], img.shape[0], x0, y0))
    vq = lambda k, a, fmt, lista: None if k in sin_vq else ch.vq_pequeno(k, a, fmt, lista)
    ruta = os.path.join(sal, 'SALON.DAT')
    cs.escribe(D, BANCO, ruta, grupos, bytes(out), vq=vq, solo=True, antes=antes)
    print('SALON.DAT: %d bytes (tablas SAL1 %d bytes, %d textos)' % (os.path.getsize(ruta), len(out), len(ref)))


if __name__ == '__main__':
    main()
