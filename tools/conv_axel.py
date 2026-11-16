#!/usr/bin/env python3
"""conv_axel.py: Axel (personaje 0x3B de Crazy Taxi, Dreamcast) sentado en el taxi, para meterlo en COCHE15.DAT.

Lee las 15 piezas del piloto de POLDC0.BIN (offsets y matrices del mapa de modelos de la decompilacion;
consolesplayingconsoles/crazy-taxi-decomp, model-map/game.dcgs), les aplica una postura sentada estatica y las
convierte a mallas ORD7 en el marco del coche del port (x derecha, y arriba, z atras), con las texturas 243..256 de
TEXDC0.BIN. Las mallas van en la pieza 0 (carroceria) para que se dibujen con la matriz del coche, como el resto del
cuerpo; no se toca el codigo de dibujo.

Uso (lo llama tools/conv_crazytaxi.py):
  conv_axel.anade(mod, dirpiezas, escala=0.10625, asiento=(-4.36, 0, 1.01))

dirpiezas es la carpeta de _trabajo/crazytaxi con POLDC0.BIN, TEXDC0.BIN y 1ST_READ.BIN (los saca
_trabajo/crazytaxi_lee.py). Devuelve (mallas, vertices, texturas) anadidos.
"""
import os, struct
import numpy as np
import modelo

# piezas (nombre, offset en POLDC0, hueso) y matriz 3x4 de la pose de pie (12 floats, filas)
PIEZAS = [
    ('torso',       0x1DDEB8, 8,  (-0.00052, 0.00000, 1.00000, 0.00000, 0.99902, -0.04418, 0.00052, 10.58410, 0.04418, 0.99902, 0.00002, 0.00000)),
    ('torso_extra', 0x1DE778, 8,  (-0.00052, 0.00000, 1.00000, 0.00000, 0.99902, -0.04418, 0.00052, 10.58410, 0.04418, 0.99902, 0.00002, 0.00000)),
    ('head',        0x1D9A18, 1,  (0.00006, -0.00019, 1.00000, -0.00234, -0.06390, 0.99796, 0.00019, 15.06462, -0.99796, -0.06390, 0.00004, 0.19816)),
    ('l_uparm',     0x1DB990, 4,  (-0.10786, -0.02790, -0.99377, -1.78951, -0.97298, 0.20825, 0.09975, 14.23822, 0.20417, 0.97768, -0.04961, 0.21767)),
    ('l_uparm_x',   0x1DC0E8, 4,  (-0.10786, -0.02790, -0.99377, -1.78951, -0.97298, 0.20825, 0.09975, 14.23822, 0.20417, 0.97768, -0.04961, 0.21767)),
    ('l_forearm',   0x1DB6B8, 3,  (-0.08689, -0.06973, -0.99377, -2.06001, -0.97267, -0.20967, 0.09976, 11.79799, -0.21532, 0.97528, -0.04960, 0.72972)),
    ('l_hand',      0x1DB320, 2,  (-0.00672, -0.06929, -0.99757, -2.27850, -0.99326, -0.11500, 0.01467, 9.35222, -0.11573, 0.99095, -0.06805, 0.18829)),
    ('r_uparm',     0x1DD568, 7,  (0.10751, 0.07714, -0.99121, 1.78569, -0.96038, 0.26590, -0.08347, 14.24008, 0.25712, 0.96091, 0.10267, 0.21775)),
    ('r_uparm_x',   0x1DDCC0, 7,  (0.10751, 0.07714, -0.99121, 1.78569, -0.96038, 0.26590, -0.08347, 14.24008, 0.25712, 0.96091, 0.10267, 0.21775)),
    ('r_forearm',   0x1DD290, 6,  (0.05444, 0.11983, -0.99130, 2.05533, -0.96622, -0.24413, -0.08257, 11.83145, -0.25190, 0.96231, 0.10249, 0.86260)),
    ('r_hand',      0x1DCEF8, 5,  (-0.00500, 0.12266, -0.99244, 2.19221, -0.98314, -0.18203, -0.01754, 9.40188, -0.18280, 0.97561, 0.12150, 0.22920)),
    ('hips',        0x1E1BD0, 15, (0.00000, 0.00000, 1.00000, 0.00000, -0.97889, 0.20441, 0.00000, 10.58410, -0.20441, -0.97889, 0.00000, 0.00000)),
    ('l_thigh',     0x1DFFB0, 11, (-0.00150, -0.25167, 0.96781, -1.17640, -0.99823, -0.05717, -0.01641, 9.47867, 0.05946, -0.96612, -0.25114, -0.38264)),
    ('l_shin',      0x1DF6B8, 10, (0.02780, -0.25014, 0.96781, -1.18245, -0.98479, -0.17295, -0.01641, 5.43484, 0.17149, -0.95264, -0.25114, -0.14177)),
    ('l_foot',      0x1DEFA8, 9,  (0.00168, -0.25167, 0.96781, -1.06439, -0.99743, -0.06978, -0.01641, 1.25282, 0.07166, -0.96529, -0.25114, 0.58646)),
    ('r_thigh',     0x1E1558, 14, (0.00271, 0.22961, 0.97328, 1.17640, -0.99828, -0.05644, 0.01610, 9.47868, 0.05863, -0.97164, 0.22906, -0.38264)),
    ('r_shin',      0x1E0C60, 13, (-0.02426, 0.22834, 0.97328, 1.18739, -0.98475, -0.17323, 0.01610, 5.43466, 0.17227, -0.95805, 0.22906, -0.14512)),
    ('r_foot',      0x1E05B0, 12, (-0.00042, 0.22962, 0.97328, 1.08439, -0.99741, -0.07006, 0.01610, 1.25282, 0.07189, -0.97075, 0.22906, 0.58646)),
]
HUESOS = {0: None, 8: 0, 1: 8, 4: 8, 3: 4, 2: 3, 7: 8, 6: 7, 5: 6, 15: 0, 11: 15, 10: 11, 9: 10,
          14: 15, 13: 14, 12: 13}
# postura sentada: giros extra en ejes locales del hueso (grados)
GIROS = {8: ('z', 8.0), 1: ('z', -4.0), 4: ('z', -38.0), 7: ('z', -38.0), 3: ('z', -38.0), 6: ('z', -38.0),
         11: ('z', 82.0), 14: ('z', 82.0), 10: ('z', -82.0), 13: ('z', -82.0)}
PELVIS = 10.58410       # altura de la pelvis en la pose de pie (unidades)
PELVIS_SENTADO = 6.60   # altura de la pelvis sentada (unidades del coche original)
ASIENTO = (4.36, 0.0, -1.01)   # asiento del piloto en ejes del coche original (+x izquierda, +z delante)
ESCALA = 0.10625
# El modelo de Axel es mas grande que el habitaculo del taxi: a escala 1 los pies quedan a -0,08 m (debajo de la
# carroceria) y la cabeza casi en el techo.  Se reduce al 80 % y se sube 24,5 cm: pies a +0,18 m (dentro, por encima
# de los bajos de 0,144 m) y cabeza a 1,40 m (bajo el techo de 1,54 m).
PILOTO_ESCALA = 0.80
PILOTO_SUBIDA = 0.245
LUZ = np.array([0.3, 0.8, 0.5], float); LUZ /= np.linalg.norm(LUZ)


# ------------------------------------------------------------------ Naomi
def _es_ref(w):
    f = struct.unpack('<f', struct.pack('<I', w))[0]
    return (not np.isfinite(f)) or abs(f) > 1e9


def parse_modelo(D, off):
    obj, flags = struct.unpack('<2I', D[off:off + 8])
    if obj not in (0, 1) or flags > 0xFFFF:
        raise ValueError('cabecera Naomi dudosa en 0x%X' % off)
    recs = []; pos = off + 24
    for _ in range(500):
        if pos + 0x50 > len(D): break
        pcw = struct.unpack('<I', D[pos:pos + 4])[0]
        if pcw == 0 or not (pcw & 0x80000000): break
        tex, spec = struct.unpack('<2i', D[pos + 32:pos + 40])
        amb = struct.unpack('<f', D[pos + 40:pos + 44])[0]
        face = struct.unpack('<4f', D[pos + 44:pos + 60]); offc = struct.unpack('<4f', D[pos + 60:pos + 76])
        ln = struct.unpack('<I', D[pos + 76:pos + 80])[0]
        lista = (pcw >> 24) & 7
        strips = []; p, pe = pos + 0x50, pos + 0x50 + ln; vert_at = {}
        while p + 8 <= pe:
            g, n = struct.unpack('<2I', D[p:p + 8])
            if g == 0: break
            if g & 0x80000000: break
            p += 8
            nv = 3 * n if (g & 0x8) else n
            vs = []
            for _ in range(nv):
                if p + 8 > pe: break
                w0, w1 = struct.unpack('<2I', D[p:p + 8])
                if _es_ref(w0):
                    ofs = struct.unpack('<i', struct.pack('<I', w1))[0]
                    tt = p + 8 + ofs
                    if tt in vert_at: vs.append(vert_at[tt])
                    elif 0 <= tt <= len(D) - 32:
                        v = struct.unpack('<8f', D[tt:tt + 32]); vert_at[tt] = v; vs.append(v)
                    else: vs.append((0.0,) * 8)
                    p += 8
                elif p + 32 <= pe:
                    v = struct.unpack('<8f', D[p:p + 32]); vert_at[p] = v; vs.append(v); p += 32
                else: break
            strips.append(dict(g=g, n=n, verts=vs))
        recs.append(dict(pcw=pcw, tex=tex, spec=spec, amb=amb, face=face, offc=offc, lista=lista, strips=strips))
        pos = pe
        if pos + 4 > len(D) or struct.unpack('<I', D[pos:pos + 4])[0] == 0: break
    return recs


TAMANOS_PVR = (8, 16, 32, 64, 128, 256, 512, 1024)


def _layout(w, h, typ):
    if typ in (1, 2, 3, 4) and w != h:
        raise ValueError('textura %dx%d tipo %d no cuadrada' % (w, h, typ))
    prefix = 0
    if typ == 2: prefix = 6 + sum(2 * s * s for s in TAMANOS_PVR if 2 <= s < w) + 2 * (4 + 16)
    elif typ == 4: prefix = 2048 + 1 + sum((s // 2) ** 2 for s in (2, 4, *TAMANOS_PVR) if s < w)
    elif typ == 3: prefix = 2048
    base = w * h // 4 if typ in (3, 4) else w * h * 2
    return prefix, prefix + base


def _esp(v):
    r = np.zeros_like(v)
    for b in range(10): r |= ((v >> b) & 1) << (2 * b)
    return r


def _twiddle(w, h):
    s = min(w, h); y, x = np.mgrid[0:h, 0:w]
    blo = (x // s) if w > h else (y // s)
    return blo * (s * s) + (_esp(x % s) << 1 | _esp(y % s))


def _dec16(v, fmt):
    v = v.astype(np.uint32)
    if fmt == 1:
        r = ((v >> 11) & 31) * 255 // 31; g = ((v >> 5) & 63) * 255 // 63; b = (v & 31) * 255 // 31
        return np.stack([r, g, b, np.full_like(r, 255)], -1).astype(np.uint8)
    if fmt == 0:
        a = ((v >> 15) & 1) * 255; r = ((v >> 10) & 31) * 255 // 31; g = ((v >> 5) & 31) * 255 // 31; b = (v & 31) * 255 // 31
        return np.stack([r, g, b, a], -1).astype(np.uint8)
    a = ((v >> 12) & 15) * 17; r = ((v >> 8) & 15) * 17; g = ((v >> 4) & 15) * 17; b = (v & 15) * 17
    return np.stack([r, g, b, a], -1).astype(np.uint8)


def _tabla_texturas(S):
    off = 0x140570 - 0x10000  # 0x0C140570 con la base 0x0C010000
    ents = []
    for k in range(438):
        wh, tf, ad, z = struct.unpack('<4I', S[off + 16 * k:off + 16 * k + 16])
        w, h = wh & 0xFFFF, wh >> 16; typ, fmt = tf >> 8, tf & 0xFF
        ents.append((w, h, typ, fmt, ad - 0x0CB40000))
    return ents


def _imagen_pvr(T, ent):
    w, h, typ, fmt, off = ent
    prefix, _ = _layout(w, h, typ)
    if typ in (3, 4):
        cb = np.frombuffer(T[off:off + 2048], '<u2').reshape(256, 4)
        mem = np.frombuffer(T[off + prefix:off + prefix + (w // 2) * (h // 2)], np.uint8)
        by, bx = np.mgrid[0:h // 2, 0:w // 2]
        pos = (_esp(bx) << 1 | _esp(by)); blk = cb[mem[pos]]
        out = np.zeros((h, w, 4), np.uint8)
        for t, (dy, dx) in enumerate(((0, 0), (0, 1), (1, 0), (1, 1))):
            out[dy::2, dx::2] = _dec16(blk[:, :, t], fmt)
        return out
    v = np.frombuffer(T[off + prefix:off + prefix + w * h * 2], '<u2')
    if typ in (1, 2, 13): v = v[_twiddle(w, h)]
    return _dec16(v, fmt).reshape(h, w, 4)


# ------------------------------------------------------------------ postura
def _a4(M):
    R = np.eye(4); R[:3, :4] = M; return R


def _rota(axis, grados):
    a = np.radians(grados); c, s = np.cos(a), np.sin(a)
    if axis == 'x': return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])
    if axis == 'y': return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])


def matrices():
    """matriz 4x4 de cada pieza en la pose sentada, en ejes del coche original (+x izquierda, +y arriba, +z delante)"""
    Mb = {}; off_de = {}
    for nombre, off, b, m in PIEZAS:
        Mb.setdefault(b, np.array(m, float).reshape(3, 4)); off_de.setdefault(b, []).append(off)
    # locales: L_b = inv(M_pad) * M_b
    L = {}
    for b in Mb:
        pad = HUESOS[b]
        L[b] = _a4(Mb[b]) if pad is None or pad == 0 else np.linalg.inv(_a4(Mb[pad])) @ _a4(Mb[b])
    E = {b: np.eye(4) for b in Mb}
    for b, (eje, g) in GIROS.items():
        if b in E: E[b][:3, :3] = _rota(eje, g)
    G = np.eye(4); G[1, 3] = PELVIS_SENTADO - PELVIS
    Mw = {}
    orden = [0, 8, 15, 1, 4, 7, 3, 6, 2, 5, 11, 14, 10, 13, 9, 12]
    for b in orden:
        if b not in E: continue
        pad = HUESOS.get(b)
        Mp = np.eye(4) if pad is None or pad == 0 else Mw[pad]
        Mw[b] = Mp @ L[b] @ E[b]
    for b in list(Mw): Mw[b] = G @ Mw[b]
    return Mw


def monta_sentado(dirpiezas):
    """[(nombre, offset, recs, M 3x4)] de las piezas de Axel sentado"""
    D = open(os.path.join(dirpiezas, 'POLDC0.BIN'), 'rb').read()
    Mw = matrices()
    out = []
    for nombre, off, b, _m in PIEZAS:
        recs = parse_modelo(D, off)
        out.append((nombre, off, recs, Mw[b][:3, :4]))
    return out


def pivotes():
    """(cadera, cuello) del piloto en el marco del port (metros): pivotes de la pieza 1 (cuerpo) y la 2 (cabeza)"""
    M = matrices()
    esc = ESCALA * PILOTO_ESCALA
    tras = np.array([-ASIENTO[0], 0.0, -ASIENTO[2]], float) * ESCALA
    out = []
    for b in (15, 1):
        p = M[b][:3, 3] * esc + tras
        p[1] += PILOTO_SUBIDA
        out.append(p)
    return out


# ------------------------------------------------------------------ conversion a ORD7
def anade(mod, dirpiezas, escala=ESCALA, asiento=ASIENTO, escala_piloto=PILOTO_ESCALA,
          subida=PILOTO_SUBIDA):
    """anade el piloto al modelo ORD7 mod (modelo.Modelo) en la pieza 0. Devuelve (mallas, vertices, texturas)."""
    T = open(os.path.join(dirpiezas, 'TEXDC0.BIN'), 'rb').read()
    S = open(os.path.join(dirpiezas, '1ST_READ.BIN'), 'rb').read()
    tab = _tabla_texturas(S)
    cache = {}
    def textura(t):
        if t < 0: return None
        if t not in cache:
            cache[t] = _imagen_pvr(T, tab[t])
        return cache[t]
    # texturas: opacas (lista 0) salvo las que tienen recorte (253, 255: lista 2)
    tex_local = {}
    def anade_textura(t):
        if t not in tex_local:
            a = textura(t)
            li = 2 if (t in (253, 255)) else 0
            if li == 2:
                fmt = modelo.formato_mezcla(a)
                datos = modelo.codifica16(a, fmt)
            else:
                fmt = 1
                datos = modelo.codifica16(a, fmt)
            off = len(mod.blob)
            mod.texs.append((a.shape[1], a.shape[0], fmt, li, off, len(datos)))
            mod.blob += datos; mod.rgba.append(a)
            tex_local[t] = (len(mod.texs) - 1, li)
        return tex_local[t]
    piezas = monta_sentado(dirpiezas)
    # asiento: original -> port (x -> -x, z -> -z), escala
    tras = np.array([-asiento[0], 0.0, -asiento[2]], float) * escala
    nm = nv = 0
    for nombre, off, recs, M in piezas:
        for rec in recs:
            t = int(rec['tex']); tex_ti, tex_li = anade_textura(t) if t >= 0 else (None, 0)
            for s in rec['strips']:
                if len(s['verts']) < 3: continue
                P = np.array([v[:3] for v in s['verts']], float)
                N = np.array([v[3:6] for v in s['verts']], float)
                U = np.array([v[6] for v in s['verts']], float)
                V = np.array([v[7] for v in s['verts']], float)
                P = P @ M[:, :3].T + M[:, 3]
                N = N @ M[:, :3].T
                # el personaje ya mira a -z (como el morro del coche en el port): no se le gira 180; solo
                # se escala y se lleva al asiento (port: x -4,36, z +1,01 unidades).  El giro 180 que lleva el
                # coche de Naomi al port se compensa con el que hay que darle al piloto para mirar al morro
                # en los ejes originales, asi que aqui no queda ninguno.
                P = P * (escala * escala_piloto) + tras
                P[:, 1] += subida
                N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-6)
                luz = 0.7 + 0.3 * np.clip(np.abs(N @ LUZ), 0, 1)
                if t >= 0:
                    alfa = np.full(len(P), 255.0); rgb = np.ones((len(P), 3)) * luz[:, None]
                else:
                    f = rec['face']; alfa = np.full(len(P), max(0.0, min(1.0, f[0])) * 255.0)
                    rgb = np.array(f[1:4])[None, :] * luz[:, None]
                # lista de triangulos -> tira con degenerados (como ordena_tiras)
                if s['g'] & 8:
                    q = [0, 1, 2]
                    for j in range(3, len(P), 3): q += [q[-1], j, j, j + 1, j + 2]
                    P, U, V = P[q], U[q], V[q]
                    rgb, alfa = rgb[q], alfa[q]
                n = len(P)
                C = np.empty((n, 4), np.uint32)
                C[:, 0] = np.clip(np.rint(rgb[:, 0] * 255), 0, 255)
                C[:, 1] = np.clip(np.rint(rgb[:, 1] * 255), 0, 255)
                C[:, 2] = np.clip(np.rint(rgb[:, 2] * 255), 0, 255)
                C[:, 3] = np.clip(np.rint(alfa), 0, 255)
                cc = P[:, :3].mean(0); rad = float(np.sqrt(((P[:, :3] - cc) ** 2).sum(1).max()))
                m = np.zeros(n, dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('u', '<f4'), ('v', '<f4'), ('c', '<u4')])
                m['x'], m['y'], m['z'] = P[:, 0], P[:, 1], P[:, 2]
                m['u'], m['v'] = U, V
                m['c'] = (C[:, 3] << 24) | (C[:, 0] << 16) | (C[:, 1] << 8) | C[:, 2]
                mod.verts.append(m)
                # pieza 1: cuerpo (con las manos al volante); pieza 2: cabeza (giro/nodo). El coche las dibuja
                # con sus matrices de ANI1 (PZ_VOLANTE / PZ_ALERON) y main.c las mueve para el taxi.
                nodo = (2 if nombre == 'head' else 1) << 8
                mod.mallas.append([tex_li, -1 if tex_ti is None else tex_ti, cc, rad, mod.nv, n, None, nodo])
                mod.nv += n; nm += 1; nv += n
    return nm, nv, len(tex_local)
