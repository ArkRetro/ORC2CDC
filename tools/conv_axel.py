#!/usr/bin/env python3
"""conv_axel.py: Axel (personaje 0x3B de Crazy Taxi, Dreamcast) sentado en el taxi, para COCHE15.DAT.

Lee las 15 piezas del piloto de POLDC0.BIN (offsets y matrices del mapa de modelos de la decompilacion;
consolesplayingconsoles/crazy-taxi-decomp, model-map/game.dcgs), las coloca con una postura sentada
resuelta con IK a los mandos del taxi (volante y pedales), las escala y las convierte a mallas ORD7 en
el marco del port (x derecha, y arriba, z atras), con las texturas que usan de TEXDC0.BIN.

La postura no es a ojo: el juego original anima al piloto con una curva Fcv (MOTDC.BIN 0x64D64, la que
carga Drv_StartDrive) y con drvJolt (aceleracion -> inclinacion del cuerpo y giro de cabeza).  Aqui se
parte de esa misma idea: cada mano va al aro del volante y cada pie a los pedales, con el cuerpo sentado
a la altura de la pelvis del original (5,608 u sobre el origen del coche).  El piloto va a la escala del
juego (1 u = 0,10625 m), la misma del taxi: la postura nueva deja los pies dentro del habitaculo (0,21 m,
por encima de los bajos de 0,144 m) y la cabeza bajo el marco del parabrisas (1,28 m).  Las manos y los
pies siguen en los mandos porque el IK se resuelve con los huesos ya escalados.

Uso (lo llama tools/conv_crazytaxi.py):
  conv_axel.anade(mod, dirpiezas, escala=0.10625)
  conv_axel.pivotes()          -> (cadera, cuello) en metros del port
  conv_axel.pivote_volante()   -> (centro, eje) del volante en metros del port
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
# geometria del taxi y del piloto, en unidades del modelo (1 u = 0,10625 m)
ESCALA = 0.10625
ESCALA_PILOTO = 1.00
ASIENTO = np.array([-4.36, 0.0, 1.01])          # port: x derecha, z atras (morro -z)
PELVIS_Y = 5.608                                 # altura de la pelvis sentada (curva Fcv de Drv_StartDrive)
TORSO_PITCH = -8.0                               # grados, hacia delante
# volante del taxi: aro de strips 252/253/254 (sintex) + centro 173..176; centro y normal del modelo
VOLANTE_C = np.array([-4.3504, 7.7064, -3.0173])
VOLANTE_EJE = np.array([0.0, -0.20903, -0.97791])
VOLANTE_EJE /= np.linalg.norm(VOLANTE_EJE)
VOLANTE_R = 1.815
# manos en el aro (9 y 3 en punto, algo por debajo del centro) y pies sobre el suelo del habitaculo
MANO_L = VOLANTE_C + np.array([-VOLANTE_R * 0.985, -0.30, 0.10])
MANO_R = VOLANTE_C + np.array([ VOLANTE_R * 0.985, -0.30, 0.10])
PIE_L = np.array([-5.40, 3.25, -4.90])
PIE_R = np.array([-3.40, 3.25, -5.60])
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


def _rot4(R):
    M = np.eye(4); M[:3, :3] = R; return M


def _rota(axis, grados):
    a = np.radians(grados); c, s = np.cos(a), np.sin(a)
    if axis == 'x': return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])
    if axis == 'y': return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])


def _aim(base, target):
    """rota la base (hueso +x local) para apuntar a target; giro minimo (conserva el twist)"""
    p = (np.linalg.inv(base) @ np.append(target, 1.0))[:3]
    d = p / max(np.linalg.norm(p), 1e-9)
    x = np.array([1.0, 0.0, 0.0])
    c = float(np.dot(x, d)); ax = np.cross(x, d); s = np.linalg.norm(ax)
    if s < 1e-8:
        if c > 0: return base
        ax = np.array([0.0, 0.0, 1.0]); a = np.pi
    else:
        ax /= s; a = np.arctan2(s, c)
    K = np.array([[0, -ax[2], ax[1]], [ax[2], 0, -ax[0]], [-ax[1], ax[0], 0]], float)
    R = np.eye(3) + np.sin(a) * K + (1 - np.cos(a)) * (K @ K)
    return base @ _rot4(R)


def _ik(S, T, l1, l2, pole):
    d = T - S; dist = np.linalg.norm(d)
    dist = max(1e-6, min(dist, l1 + l2 - 1e-5)); dn = d / dist
    a = (l1 * l1 - l2 * l2 + dist * dist) / (2 * dist)
    h = np.sqrt(max(0.0, l1 * l1 - a * a))
    p = pole - dn * np.dot(pole, dn)
    if np.linalg.norm(p) < 1e-6:
        p = np.cross(dn, [0, 1, 0])
        if np.linalg.norm(p) < 1e-6: p = np.cross(dn, [0, 0, 1])
    p /= np.linalg.norm(p)
    return S + dn * a + p * h


def _rest():
    Mb = {}
    for nombre, off, b, m in PIEZAS:
        Mb.setdefault(b, np.array(m, float).reshape(3, 4))
    return Mb


def matrices(escala=ESCALA_PILOTO, torso_pitch=TORSO_PITCH, head_yaw=0.0, head_nod=0.0):
    """matriz 4x4 de cada hueso en el marco del port (x derecha, y arriba, z atras), en unidades"""
    Mb = _rest()
    L = {0: np.eye(4)}
    for b, pad in HUESOS.items():
        if b == 0: continue
        L[b] = np.linalg.inv(_a4(Mb[pad])) @ _a4(Mb[b]) if pad != 0 else _a4(Mb[b])
        L[b][:3, 3] *= escala
    root = np.eye(4); root[:3, 3] = ASIENTO + np.array([0, PELVIS_Y, 0])
    R15 = _a4(Mb[15]).copy(); R15[:3, 3] = 0
    R8 = _a4(Mb[8]).copy(); R8[:3, 3] = 0
    M = {}
    M[15] = root @ R15
    M[8] = root @ R8 @ _rot4(_rota('z', torso_pitch))
    M[1] = M[8] @ L[1] @ _rot4(_rota('y', head_yaw) @ _rota('z', head_nod))
    for b4, b3, b2, target, pole in (
            (4, 3, 2, MANO_L, np.array([0, -0.5, 0.8])),
            (7, 6, 5, MANO_R, np.array([0, -0.5, 0.8]))):
        base = M[8] @ L[b4]
        codo = _ik(base[:3, 3], target, np.linalg.norm(L[b3][:3, 3]), np.linalg.norm(L[b2][:3, 3]), pole)
        M[b4] = _aim(base, codo)
        M[b3] = _aim(M[b4] @ L[b3], target)
        M[b2] = M[b3] @ L[b2]
    Rpie = _a4(Mb[9]).copy(); Rpie[:3, 3] = 0
    for b11, b10, b9, target, pole in (
            (11, 10, 9, PIE_L, np.array([0, 0.75, 0.25])),
            (14, 13, 12, PIE_R, np.array([0, 0.75, 0.25]))):
        base = M[15] @ L[b11]
        rodilla = _ik(base[:3, 3], target, np.linalg.norm(L[b10][:3, 3]), np.linalg.norm(L[b9][:3, 3]), pole)
        M[b11] = _aim(base, rodilla)
        M[b10] = _aim(M[b11] @ L[b10], target)
        M[b9] = np.eye(4); M[b9][:3, 3] = target; M[b9][:3, :3] = Rpie[:3, :3]
    return M


def pivotes():
    """(cadera, cuello) del piloto en el marco del port (metros)"""
    M = matrices()
    return M[15][:3, 3] * ESCALA, M[1][:3, 3] * ESCALA


def pivote_volante():
    """(centro, eje) del volante en metros del port. El eje va en pz[3][3..4] (x = 0)"""
    return VOLANTE_C * ESCALA, VOLANTE_EJE


# ------------------------------------------------------------------ conversion a ORD7
def anade(mod, dirpiezas, escala=ESCALA, escala_piloto=ESCALA_PILOTO):
    """anade el piloto al modelo ORD7 mod (modelo.Modelo) en las piezas 1 (cuerpo) y 2 (cabeza)."""
    D = open(os.path.join(dirpiezas, 'POLDC0.BIN'), 'rb').read()
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
    M = matrices()
    nm = nv = 0
    for nombre, off, b, _m in PIEZAS:
        recs = parse_modelo(D, off)
        Mw = M[b]
        for rec in recs:
            t = int(rec['tex']); tex_ti, tex_li = anade_textura(t) if t >= 0 else (None, 0)
            for s in rec['strips']:
                if len(s['verts']) < 3: continue
                P = np.array([v[:3] for v in s['verts']], float) * escala_piloto
                N = np.array([v[3:6] for v in s['verts']], float)
                U = np.array([v[6] for v in s['verts']], float)
                V = np.array([v[7] for v in s['verts']], float)
                P = P @ Mw[:3, :3].T + Mw[:3, 3]
                N = N @ Mw[:3, :3].T
                P *= escala
                N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-6)
                luz = 0.7 + 0.3 * np.clip(np.abs(N @ LUZ), 0, 1)
                if t >= 0:
                    alfa = np.full(len(P), 255.0); rgb = np.ones((len(P), 3)) * luz[:, None]
                else:
                    f = rec['face']; alfa = np.full(len(P), max(0.0, min(1.0, f[0])) * 255.0)
                    rgb = np.array(f[1:4])[None, :] * luz[:, None]
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
                # pieza 1: cuerpo (con las manos al volante); pieza 2: cabeza (giro/nodo)
                nodo = (2 if nombre == 'head' else 1) << 8
                mod.mallas.append([tex_li, -1 if tex_ti is None else tex_ti, cc, rad, mod.nv, n, None, nodo])
                mod.nv += n; nm += 1; nv += n
    return nm, nv, len(tex_local)
