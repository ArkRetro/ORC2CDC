#!/usr/bin/env python3
"""conv_sprani.py: un banco SPRANI entero (animaciones 2D del original) -> fichero con el formato HUD1 de conv_hud.py
(texturas y sprites, sin grupos) mas un bloque 'SPR1' con la animacion para el reproductor de escenas de la Dreamcast
(comun/sprani.c), que la evalua en cada cuadro como el original (429460 / 429350 / 48B970 del PC) en vez de hornear
cuadro a cuadro (las escenas del frontal de recreativa, SELECTOR_CVT, duran hasta 2400 cuadros: horneadas serian
~290000 piezas, 9 MB).

Se usa desde otros conversores (conv_sp.py): escribe(D, banco, ruta, grupos, extra).

Que se guarda:
 - La animacion del banco (ani_SPRANI_*.sz, sin la palabra de tamano: los punteros son relativos a ese punto) con
   cada cuadro de footage (+0 de los registros de 20 bytes {textura, izq, arr, der, abj}) cambiado por el indice de su
   sprite en el fichero (0xFFFF.... se queda: cuadro vacio). Las capas de mascara (+0x48 & 8) no se dibujan nunca: sus
   cuadros no tienen sprite.
 - Sprites: el rectangulo de la textura de cada cuadro recortado a su alfa (con 1 pixel de margen, conv_hud.Atlas) y
   empaquetado en atlas por grupos de escenas (`grupos`: lo que se ve junto; un rectangulo va en el atlas del primer
   grupo que lo usa). Los rectangulos que no caben en 512x256 / 256x512 van con su textura de origen entera. Por
   sprite, la parte del rectangulo original que cubre (4 floats 0..1: x0, y0, x1, y1), para dibujar el trozo
   recortado en su sitio.
 - Mascaras (+0x48 & 8: la capa siguiente se dibuja con el alfa de la mascara): cada combinacion se hornea como en
   conv_hud.hornea y se busca por (escena, cuadro entero, capa): tabla ordenada. El reproductor del original avanza
   rate * velocidad / 60 cuadros por paso de 60 Hz y todas las escenas del banco van a 60 por segundo: los cuadros son
   enteros.
 - Por escena, las texturas que usa (para subirlas a la VRAM al crearla).
Bloque SPR1 (detras de los tacometros, ver hudd_extra; desplazamientos desde el principio del bloque):
  'SPR1', banco, n escenas, n sprites, off ani, tam ani, off fracciones (n sprites x 4 f32), off texturas por escena
  (n escenas x {u32 off, u32 n} y listas de u16), off mascaras, n mascaras ({u16 escena, u16 cuadro, u32 capa, u16
  sprite, u16 0}), off extra, tam extra (lo que pase el conversor que lo usa).
"""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conv_hud as ch
import conv_fin as cf


def u32(a, o): return struct.unpack_from('<I', a, o)[0]
def i32(a, o): return struct.unpack_from('<i', a, o)[0]


def raiz(A, k):
    sc = A.escenas[k]; nc, pc = struct.unpack_from('<2I', A.a, sc); return pc + (nc - 1) * 0x24


def capas(A, k):
    """capas (desplazamiento de su registro de 0x4C) alcanzables desde la escena k"""
    vis = []; vistos = set()
    def rec(pl, n):
        for q in range(n):
            L = pl + q * 0x4c
            if L in vistos: continue
            vistos.add(L); vis.append(L)
            p = u32(A.a, L + 8)
            if p and not A.a[L + 0x48] & 1: rec(u32(A.a, p + 0x20), i32(A.a, p + 0x1c))
    r = raiz(A, k); rec(u32(A.a, r + 0x20), i32(A.a, r + 0x1c))
    return vis


def footage(A, L):
    """cuadros {(indice, textura, rect)} del footage de la capa L"""
    p = u32(A.a, L + 8); w, h, ritmo, n, fr = struct.unpack_from('<iiiiI', A.a, p)
    res = []
    for i in range(max(0, n)):
        t, l, tp, r, b = struct.unpack_from('<I4f', A.a, fr + 20 * i)
        if t != 0xffffffff: res.append((fr + 20 * i, t & 0xffff, (l, tp, r, b), w, h))
    return res


def escribe(D, bk, ruta, grupos, extra=b'', vq=None, log=print):
    """grupos: [(nombre, [escenas])] en el orden de carga; las escenas que no esten en ninguno van en un grupo 'RESTO'"""
    A, T, _ = D.banco(bk)
    todas = set(k for _, ks in grupos for k in ks)
    resto = [k for k in range(len(A.escenas)) if k not in todas]
    if resto: grupos = list(grupos) + [('RESTO', resto)]
    hud = ch.Hud(); hud.taco = [0xff] * 16
    if vq: hud.vq = vq
    ani = bytearray(A.a)
    bkl = 'S%X' % bk                                 # banco en las claves del atlas (se ordenan con 'HORNO')
    sprite_de = {}                                   # clave del atlas -> indice de sprite
    frac = []                                        # por sprite: x0, y0, x1, y1 del rectangulo original
    tex_esc = {k: set() for k in range(len(A.escenas))}
    parche = {}                                      # cuadro de footage (desplazamiento) -> sprite
    masc = []
    ch.HORNO.clear()
    for nom, ks in grupos:
        at = ch.Atlas(); at.prefijo = 'spr_%x_%s' % (bk, nom)
        nuevas = []; grandes = []; usos = []          # usos: (escena, desplazamiento del cuadro o None, clave, p)
        for k in ks:
            for L in capas(A, k):
                if not A.a[L + 0x48] & 1 or A.a[L + 0x48] & 8: continue
                for o, t, src, w, h in footage(A, L):
                    p = dict(tex=t, src=src, w=w, h=h)
                    clave = at._clave(bkl, p)
                    usos.append((k, o, clave, p, bkl, T))
            # mascaras: cada cuadro entero de la escena
            if any(A.a[L + 0x48] & 8 for L in capas(A, k)):
                for f in range(cf.duracion(A, k)):
                    for p in ch.escena(A, k, float(f)):
                        if 'mascara' not in p: continue
                        q = ch.hornea(p, T)
                        clave = at._clave('HORNO', q)
                        usos.append((k, (f, p['L']), clave, q, 'HORNO', ch.HORNO))
        for k, o, clave, p, b2, T2 in usos:
            if clave in sprite_de: continue
            if ch.grande(p['src']): grandes.append((clave, p, b2, T2))
            else:
                at.registra(b2, T2, p); nuevas.append(clave)
            sprite_de[clave] = None
        if nuevas:
            pl = at.plan(nuevas); t0 = len(hud.tex); at.empaqueta(hud, pl)
            log('  %X %s: %d rectangulos en %d texturas (%d..%d), %d KB a 16 bits' % (
                bk, nom, len(set(nuevas)), len(pl), t0, len(hud.tex) - 1, at.bytes_plan(pl) >> 10))
        for clave, p, b2, T2 in grandes:          # con su textura de origen entera
            ti = hud.textura((b2, p['tex']), T2[p['tex']])
            sprite_de[clave] = len(hud.spr)
            hud.spr.append((ti, tuple(p['src']), 0, 0)); frac.append((0.0, 0.0, 1.0, 1.0))
        for clave in set(nuevas):
            if sprite_de[clave] is not None: continue
            ti, ox, oy = at.donde[clave]; L, Tp, R, B = at.reg[clave][1]
            l, t, r, b = clave[2]
            sl, st, sr, sb = max(l, L), max(t, Tp), min(r, R), min(b, B)
            sprite_de[clave] = len(hud.spr)
            hud.spr.append((ti, (sl + ox, st + oy, sr + ox, sb + oy), 0, 0))
            frac.append(((sl - l) / (r - l), (st - t) / (b - t), (sr - l) / (r - l), (sb - t) / (b - t)))
        for k, o, clave, p, b2, T2 in usos:
            s = sprite_de[clave]; tex_esc[k].add(hud.spr[s][0])
            if isinstance(o, tuple): masc.append((k, o[0], o[1], s))
            else:
                if o in parche: assert parche[o] == s
                parche[o] = s
    for o, s in parche.items(): struct.pack_into('<I', ani, o, s)
    masc = sorted(set(masc))
    log('  %X: %d escenas, %d sprites, %d texturas, %d mascaras horneadas (%d usos), animacion %d KB' % (
        bk, len(A.escenas), len(hud.spr), len(hud.tex), len(ch.HORNO), len(masc), len(ani) >> 10))
    # bloque SPR1
    ne = len(A.escenas); cab = 12 * 4
    o_ani = cab; o_frac = (o_ani + len(ani) + 3) & ~3
    o_tex = o_frac + 16 * len(frac); listas = bytearray(); tab = bytearray()
    for k in range(ne):
        l = sorted(tex_esc[k]); tab += struct.pack('<2I', o_tex + 8 * ne + len(listas), len(l))
        listas += struct.pack('<%dH' % len(l), *l)
    listas += bytes((-len(listas)) % 4)
    o_masc = o_tex + 8 * ne + len(listas)
    mtab = b''.join(struct.pack('<HHIHH', k, f, L, s, 0) for k, f, L, s in masc)
    o_extra = o_masc + len(mtab)
    blq = bytearray(b'SPR1' + struct.pack('<11I', bk, ne, len(hud.spr), o_ani, len(ani), o_frac, o_tex, o_masc,
                                          len(masc), o_extra, len(extra)))
    blq += ani; blq += bytes(o_frac - len(blq))
    for f4 in frac: blq += struct.pack('<4f', *f4)
    blq += tab + listas + mtab + extra
    assert len(blq) == o_extra + len(extra)
    texs = hud.escribe(ruta)
    # el bloque va entre los tacometros y los datos de textura (como MNU1 de conv_menu.escribe): se desplazan los datos
    d = bytearray(open(ruta, 'rb').read())
    c = struct.unpack_from('<12I', d, 0); o_dat = c[11] + 16
    ini = (o_dat + 31) & ~31; nuevo = (o_dat + len(blq) + 31) & ~31; dx = nuevo - ini
    out = d[:o_dat] + blq; out += bytes(nuevo - len(out)); out += d[ini:]
    for i in range(c[1]):
        o = c[6] + 16 * i + 8; struct.pack_into('<I', out, o, struct.unpack_from('<I', out, o)[0] + dx)
    open(ruta, 'wb').write(out)
    hud.vram = sum(len(x[4]) for x in texs)
    log('%s: %d bytes, %d KB de VRAM' % (os.path.basename(ruta), os.path.getsize(ruta), hud.vram >> 10))
    return hud
