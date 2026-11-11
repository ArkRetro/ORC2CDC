#!/usr/bin/env python3
"""conv_fin.py <iso> <dir_salida> [dir_previas]: pantallas del fin de carrera (OutRun Miles, continuar y GAME OVER)
-> FIN.DAT y GOVER.DAT para demo/fin.c.

Formato: el de HUD.DAT (tools/conv_hud.py; comun/hud.c lo abre con hudd_abre, parcial: en la RAM solo las tablas). Un
atlas por grupo (sus texturas se suben solo mientras se ven) y cada pieza guarda en flags >> 4 los cuadros que abarca
(solo se escriben los cuadros donde algo cambia, como HAHUD.DAT de conv_infarto.py); bit 2 = capa aditiva (+0x48 & 4).

Origen (SLES-53998; los modos del original en la tabla 0x3f8e50, {init, control, salida, 0} por modo):
 - Modo 34 (0x22, OutRun Miles; 0x1bc270 / 0x1bc3d8 / 0x1bc618 = 49B470 / 49B570 / 49B7C0 del PC): 0x114b68 crea la
   escena 2B004C (capa 1, modo 5: se queda en su ultimo cuadro) -> grupo MILLAS (cuadros 0..80). Las cifras las dibuja
   el GAD (0x1a0c98 = 497FA0 del PC): 8 veces la escena 2B004B con 0x115768 (429530: capa 5) en el cuadro = contador de
   la cifra (0..cifra x 6) trasladada a (X - 320 - i x PASO, Y - 240) con X, Y, PASO = floats 0x35c8b8 / 0x35c8bc /
   0x35c8c0 del ELF (540, 210, 32) -> grupo MILLAS_CIFRA (cuadros 0..59) y usos MILLAS_C0..MILLAS_C7 (i = 0 las
   unidades) en esas posiciones.
 - Modo 35 (0x23, continuar; 0x1bc650 / 0x1bc700 / 0x1bc8c8 = 49B7E0 / 49D9C0 / 49DAE0): fuera de las misiones de
   Coast 2 Coast (0x1f7d80 = 4957F0) la escena 2B004D ("VOLVER A INTENTARLO?", modo 5) -> CONT_PREG (0..49) y, cuando
   acaba, 2B004A (REINTENTAR / ABANDONAR, modo 5) -> CONT_ELIGE (0..59). Los iconos de los botones del PS2 de 2B004A
   (cruz = bit 4 de 0x1b4548 = REINTENTAR, circulo = bit 8 = ABANDONAR) se cambian por los de la Dreamcast como en
   la barra de ayuda de los menus (conv_menu.iconos_dc: A y B): se reconocen comparando cada pieza con los iconos del
   PS2 de la tabla 0x374a60 (banco 0x48, cuadro 25).
 - Modo 27 (0x1b, GAME OVER; 0x1bafc8): el GAD (0x1a04f0 = 497960) crea 2C000B (capa 0, modo 1) con la consola
   (0x1b4878 = 43F860) -> GAME_OVER (cuadros de la escena entera; modo 1: se queda en el ultimo), aparte en GOVER.DAT
   (4029 piezas, ~97 KB de RAM: solo se abre en ese modo).
Las escenas y las texturas son las del banco del idioma (OR2_IDIOMA, espanol por defecto: XST _sxst).
"""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conv_hud as ch
import conv_menu as cm

def u32(elf, va): return struct.unpack_from('<I', elf, va - 0xfff80)[0]
def f32(elf, va): return struct.unpack_from('<f', elf, va - 0xfff80)[0]
def lui_ori(elf, lui, ori):
    """token de un lui (lui) y su ori (ori) del codigo; comprueba las dos instrucciones"""
    a, b = u32(elf, lui), u32(elf, ori)
    assert a >> 26 == 0xf and b >> 26 == 0xd, 'lui/ori en %x / %x' % (lui, ori)
    return (a & 0xffff) << 16 | (b & 0xffff)

def duracion(A, k):
    sc = A.escenas[k]; nc, pc = struct.unpack_from('<2I', A.a, sc); raiz = pc + (nc - 1) * 0x24
    return int(struct.unpack_from('<f', A.a, raiz + 8)[0])

def iconos_ps2(D):
    """{boton: RGBA} de los iconos del PS2 de la barra de ayuda (tabla 0x374a60, banco 0x48, cuadro 25)"""
    ab, Tb, _ = cm.banco_menu(D, 0x48)
    w = D.elf32(cm.AYUDA_ICONO, 16); mapa = {w[2 * i]: w[2 * i + 1] for i in range(8)}
    res = {}
    for bt in (4, 8):
        ps = [q for q in ch.escena(ab, mapa[bt] & 0xffff, 25.0) if q['alfa'] >= 0.004]
        assert len(ps) == 1
        l, t, r, b = (int(round(v)) for v in ps[0]['src'])
        res[bt] = Tb[ps[0]['tex']][t:b, l:r]
    return res

def boton_de(p, T, ps2):
    """boton (4 / 8) del icono del PS2 que es la pieza p, o None"""
    from PIL import Image
    l, t, r, b = (int(round(v)) for v in p['src'])
    if not (20 <= r - l <= 40 and 20 <= b - t <= 40): return None
    a = T[p['tex']][t:b, l:r].astype(float); mejor = None
    for bt, ic in ps2.items():
        q = np.asarray(Image.fromarray(ic).resize((r - l, b - t), Image.BILINEAR)).astype(float)
        e = np.sqrt(np.mean((a - q) ** 2))
        if mejor is None or e < mejor[0]: mejor = (e, bt)
    return mejor[1] if mejor[0] < 40 else None

def convierte(iso, sal, previas=None):
    D = ch.Disco(iso); elf = D.elf
    # tokens del codigo (comprobados) y posiciones de las cifras
    t_millas = lui_ori(elf, 0x1bc3a0, 0x1bc3a8)
    t_cifra = lui_ori(elf, 0x1a0d0c, 0x1a0d24)
    t_preg = lui_ori(elf, 0x1bc670, 0x1bc6d0)
    t_elige = lui_ori(elf, 0x1bc774, 0x1bc78c)
    t_over = lui_ori(elf, 0x1a0530, 0x1a0538)
    assert (t_millas, t_cifra, t_preg, t_elige, t_over) == (0x2b004c, 0x2b004b, 0x2b004d, 0x2b004a, 0x2c000b), \
        [hex(x) for x in (t_millas, t_cifra, t_preg, t_elige, t_over)]
    X, Y, PASO = f32(elf, 0x35c8b8), f32(elf, 0x35c8bc), f32(elf, 0x35c8c0)
    assert (X, Y, PASO) == (540.0, 210.0, 32.0), (X, Y, PASO)
    ag, Tg, _ = D.banco(0x2b); ac, Tc, _ = D.banco(0x2c)
    ps2 = iconos_ps2(D)
    F = cm.Fuente(D, cm.FUENTE_AYUDA, cm.fuentes_xst(D)); dc = cm.iconos_dc(D, F)
    grupos = [('MILLAS', ag, Tg, 0x2b, t_millas & 0xffff), ('MILLAS_CIFRA', ag, Tg, 0x2b, t_cifra & 0xffff),
              ('CONT_PREG', ag, Tg, 0x2b, t_preg & 0xffff), ('CONT_ELIGE', ag, Tg, 0x2b, t_elige & 0xffff),
              ('GAME_OVER', ac, Tc, 0x2c, t_over & 0xffff)]
    def vq_fin(i, a, fmt, lista):                               # VQ si el error es pequeno (como HAHUD.DAT)
        import vq
        datos, err = vq.codifica(a, fmt, 256)
        print('  textura %d %dx%d VQ error %.1f %s' % (i, a.shape[1], a.shape[0], err, 'si' if err <= 16 else 'no'))
        return datos if err <= 16 else None
    cambiados = 0; huds = {}
    for nombre, A, T, bk, k in grupos:
        fich = 'GOVER.DAT' if nombre == 'GAME_OVER' else 'FIN.DAT'
        if fich not in huds:
            huds[fich] = hud = ch.Hud(); hud.taco = [0xff] * 16; hud.vq = vq_fin
        hud = huds[fich]
        n = duracion(A, k); lst = []; ant = None
        for f in range(n):
            ps = []
            for p in ch.escena(A, k, float(f)):
                if 'mascara' in p: p = ch.hornea(p, T)
                if nombre == 'CONT_ELIGE':
                    bt = boton_de(p, T, ps2) if 'bk' not in p else None
                    if bt is not None:                          # icono del boton de la Dreamcast (A / B)
                        img, _ = dc[bt]; cambiados += 1
                        q = dict(p); l, t, r, b = p['src']
                        q.update(tex=('BOTON', bt), src=(0, 0, img.shape[1], img.shape[0]), bk='BOTON', tx={('BOTON', bt): img})
                        p = q
                ps.append(p)
            firma = [(str(p['tex']), tuple(np.round(p['M'].ravel(), 3)), round(p['alfa'], 3), tuple(p['src'])) for p in ps]
            if firma != ant: lst.append([f, ps, 1]); ant = firma
            else: lst[-1][2] += 1
        at = ch.Atlas(); at.prefijo = 'fin_' + nombre; cl = []
        for _, ps, _ in lst:
            for p in ps:
                if p['alfa'] >= 0.004 and abs(np.linalg.det(p['M'][:2, :2])) >= 1e-6:
                    b2, T2 = (p['bk'], p['tx']) if 'bk' in p else (bk, T)
                    at.registra(b2, T2, p); cl.append(at._clave(b2, p))
        t0 = len(hud.tex); pl = at.plan(cl); at.empaqueta(hud, pl)
        print('  %s: escena %X:%04X, %d cuadros (%d distintos), texturas %d..%d, %d KB' % (
            nombre, bk, k, n, len(lst), t0, len(hud.tex) - 1, at.bytes_plan(pl) >> 10))
        g = hud.grupo(nombre, n)
        for c, ps, nf in lst:
            k0 = len(hud.pie)
            for p in ps:
                b2, T2 = (p['bk'], p['tx']) if 'bk' in p else (bk, T)
                hud.piezas_escena(g, c, b2, T2, [p], atlas=at)
            vis = [p for p in ps if p['alfa'] >= 0.004 and abs(np.linalg.det(p['M'][:2, :2])) >= 1e-6]
            assert len(vis) == len(hud.pie) - k0
            for j, p in zip(range(k0, len(hud.pie)), vis):     # cuadros que abarca y capa aditiva
                q = list(hud.pie[j]); L = p.get('L')
                adi = 4 if L is not None and A.a[L + 0x48] & 4 else 0
                q[3] = (q[3] & 11) | adi | (min(nf, 4095) << 4); hud.pie[j] = tuple(q)
        if nombre == 'MILLAS_CIFRA':
            for i in range(8): hud.uso('MILLAS_C%d' % i, g, int(X - 320 - i * PASO), int(Y - 240), capa=5)
        else: hud.uso(nombre, g, 0, 0, capa=1)
    assert cambiados > 0, 'no se encontraron los iconos de los botones en 2B004A'
    for fich, hud in huds.items():
        hud.escribe(os.path.join(sal, fich))
        print('%s: %d bytes, %d texturas, %d piezas' % (fich, os.path.getsize(os.path.join(sal, fich)), len(hud.tex), len(hud.pie)))
    if previas: maquetas(huds, previas)

def maquetas(huds, d):
    """PNG de comprobacion: millas 01234567 al acabar la cuenta, la pregunta con la eleccion y GAME OVER (cuadro 100)"""
    from PIL import Image
    os.makedirs(d, exist_ok=True)
    for nombre, fich, cosas in (('fin_millas', 'FIN.DAT', [('MILLAS', 80)] + [('MILLAS_C%d' % i, 6 * d) for i, d in enumerate((7, 6, 5, 4, 3, 2, 1, 0))]),
                                ('fin_continua', 'FIN.DAT', [('CONT_PREG', 49), ('CONT_ELIGE', 59)]),
                                ('fin_game_over', 'GOVER.DAT', [('GAME_OVER', 100)])):
        hud = huds[fich]; img = Image.new('RGBA', (640, 480), (40, 60, 90, 255))
        for n, c in cosas:
            u = next(u for u in hud.lay if u[0] == n); ch.dibuja_grupo(img, hud, u[1], c, u[3], u[4])
        img.save(os.path.join(d, nombre + '.png'))

if __name__ == '__main__':
    if len(sys.argv) < 3: raise SystemExit(__doc__)
    convierte(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
