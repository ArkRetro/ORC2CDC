#!/usr/bin/env python3
"""conv_tramo.py tu.iso n salida.dat: segmento (etapa) n de STAGE.PS2 al formato ORD4 de la Dreamcast (modelo.py):
el bloque principal con sus texturas, el cielo aparte y la ruta (centro de la calzada cada 4 m con el ancho a cada
lado). Sale de tu disco: no se distribuye.
Cada punto de la ruta: x, y, z, ancho del asfalto izquierdo y derecho, NPERF alturas de -izq-2 a der+2, NCARRIL
desvios de carril, y de la colision del original (COLI0200 de la etapa): limite transitable izquierdo y derecho (hasta
el muro o el vacio, positivos), NEXT alturas a cada lado entre el perfil y el limite (de dentro afuera, izquierda y
luego derecha) y NBANDA franjas de superficie de izquierda a derecha (desvio donde empieza, indice de material del
original: bandera 1 << indice; las que sobran empiezan en el limite derecho).
Con cuatro numeros mas (tx ty tz giro_y) el tramo sale colocado en el marco del curso (transforma; conv_union.py los
saca de los descriptores del ejecutable para los tramos que siguen a una bifurcacion)."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2 import iso as isomod, stage, tex, vif
import modelo

def recorrido(seg):
    """camino de la camara: el diametro del grafo de puntos de la calzada (tiras casi horizontales del registro 0),
    con cada punto llevado al centro de la calzada; cada 4 unidades y suavizado"""
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import dijkstra
    from scipy.spatial import cKDTree
    pts = []
    for pk in seg.mallas(0):
        for tr in vif.tiras(pk):
            if 0x69 not in tr: continue
            P = tr[0x68].astype(float); N = tr[0x69].astype(float) / 32768
            if np.abs(N[:, 1]).mean() < 0.9: continue
            for i in range(0, len(P) - 1, 2): pts.append((P[i] + P[i + 1]) / 2)
    pts = np.array(pts); arbol = cKDTree(pts[:, [0, 2]])
    par = arbol.query_pairs(40.0, output_type='ndarray')
    d = np.linalg.norm(pts[par[:, 0]] - pts[par[:, 1]], axis=1) ** 1.5      # saltos largos: mas caros
    g = coo_matrix((np.r_[d, d], (np.r_[par[:, 0], par[:, 1]], np.r_[par[:, 1], par[:, 0]])), shape=(len(pts),) * 2).tocsr()
    d0 = dijkstra(g, indices=0); d0[~np.isfinite(d0)] = -1
    a = int(np.argmax(d0))
    da, pred = dijkstra(g, indices=a, return_predecessors=True); da[~np.isfinite(da)] = -1
    b = int(np.argmax(da)); cam = [b]
    while cam[-1] != a: cam.append(pred[cam[-1]])
    p = pts[cam]
    if p[0, 2] < p[-1, 2]: p = p[::-1]                       # empieza por el extremo de z mayor (la salida del tramo)
    # al centro de la calzada: media de los puntos de calzada a menos de 14 unidades
    p = np.array([pts[arbol.query_ball_point(q[[0, 2]], 14.0)].mean(0) for q in p])
    L = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(p, axis=0), axis=1))])
    s = np.arange(0, L[-1], 4.0)
    q = np.stack([np.interp(s, L, p[:, k]) for k in range(3)], 1)
    k = np.ones(15) / 15
    q = np.stack([np.convolve(np.pad(q[:, j], 7, mode='edge'), k, 'valid') for j in range(3)], 1)
    return q

def puntos(blq, r):
    return np.vstack([tr[0x68] for pk in blq.mallas(r) for tr in vif.tiras(pk)] or [np.zeros((1, 3))])

def es_cielo(blq):
    P = np.vstack([puntos(blq, r) for r in range(blq.w[0])])
    return P[:, 1].max() > 500 and np.abs(P[:, [0, 2]]).max() > 1500 and len(P) < 5000

def local(blq, r):
    """registro con la geometria alrededor del origen y pequena: modelos que el juego coloca con instancias"""
    P = puntos(blq, r)
    return np.abs(P[:, [0, 2]]).max() < 80 and np.abs(P[:, 1]).max() < 120

def anchos(seg, cam):
    """anchura de la calzada a cada lado de la ruta: puntos de la calzada (tiras casi horizontales del registro 0)
    a menos de 3 m a lo largo y 4 m en altura; sin ellos, el valor del punto anterior"""
    from scipy.spatial import cKDTree
    pts = np.vstack([tr[0x68] for pk in seg.mallas(0) for tr in vif.tiras(pk)
                     if 0x69 in tr and np.abs(tr[0x69][:, 1] / 32768).mean() >= 0.9]).astype(float)
    arbol = cKDTree(pts[:, [0, 2]])
    t = np.gradient(cam, axis=0); t[:, 1] = 0; t /= np.linalg.norm(t, axis=1, keepdims=True) + 1e-9
    der = np.stack([-t[:, 2], np.zeros(len(t)), t[:, 0]], 1)
    res = np.zeros((len(cam), 2)); ant = (8.0, 8.0)
    for i, c in enumerate(cam):
        q = pts[arbol.query_ball_point(c[[0, 2]], 45.0)] - c
        q = q[(np.abs(q @ t[i]) < 3) & (np.abs(q[:, 1]) < 4)]
        lat = q @ der[i]
        if len(lat) > 4: ant = (max(3.0, -lat.min()), max(3.0, lat.max()))
        res[i] = ant
    k = np.ones(9) / 9
    return np.stack([np.convolve(np.pad(res[:, j], 4, mode='edge'), k, 'valid') for j in range(2)], 1)

NPERF = 16
NCARRIL = 6

def carriles_soo(seg):
    ls = []
    for h, o, s in seg.ent:
        d = bytes(seg.m[o:o + s])
        if d[:4] != b'SOO\x02': continue
        v = np.frombuffer(d[4:4 + (s - 4) // 6 * 6], '<i2').reshape(-1, 3).astype(float) / 4
        if not any(len(v) == len(w) and np.abs(v - w).max() < 0.01 for w in ls): ls.append(v)
    return ls

def recorrido_soo(seg):
    """ruta = centro de los carriles de la etapa (SOO), cada 4 m y suavizada; None si no hay carriles"""
    ls = carriles_soo(seg)
    if not ls: return None
    n = min(len(v) for v in ls); c = np.mean([v[:n] for v in ls], axis=0)
    # altura: la de la calzada (tiras casi horizontales del registro 0) mas cercana (el tercer valor del SOO no es
    # siempre la altura)
    from scipy.spatial import cKDTree
    pts = np.vstack([tr[0x68] for pk in seg.mallas(0) for tr in vif.tiras(pk)
                     if 0x69 in tr and np.abs(tr[0x69][:, 1] / 32768).mean() >= 0.9]).astype(float)
    arbol = cKDTree(pts[:, [0, 2]]); _, j = arbol.query(c[:, :2], k=8)
    p = np.stack([c[:, 0], np.median(pts[j, 1], axis=1), c[:, 1]], 1)
    L = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(p, axis=0), axis=1))])
    s = np.arange(0, L[-1], 4.0)
    q = np.stack([np.interp(s, L, p[:, k]) for k in range(3)], 1)
    k = np.ones(5) / 5
    return np.stack([np.convolve(np.pad(q[:, j], 2, mode='edge'), k, 'valid') for j in range(3)], 1)

def puntos_asfalto(seg):
    """vertices del asfalto: tiras casi horizontales del registro 0 con la textura de carretera (la de mas area entre
    las grises del registro: saturacion baja y brillo medio)"""
    m, A = seg.m, seg.A; reg = seg.registros()[0]; c, o = reg[3]; t2 = reg[2][1]; n4, o4 = reg[4]
    hs = tex.cabeceras(seg); por_tex = {}
    for i in range(c):
        e = struct.unpack('<6I', m[A + o + 24 * i:A + o + 24 * i + 24]); k = struct.unpack('<i', m[A + t2 + 16 * i:A + t2 + 16 * i + 4])[0]
        ti = struct.unpack('<i', m[A + o4 + 36 * k + 12:A + o4 + 36 * k + 16])[0] if 0 <= k < n4 else -1
        for pk in vif.datos(m, A + e[2], A + seg.w[2]):
            for tr in vif.tiras(pk):
                if 0x69 not in tr or np.abs(tr[0x69][:, 1] / 32768).mean() < 0.9: continue
                por_tex.setdefault(ti, []).append(tr[0x68].astype(float))
    mejor, area = None, 0
    for ti, Ps in por_tex.items():
        if ti < 0: continue
        av = tex.imagen(seg, hs[ti])[..., :3].reshape(-1, 3).mean(0)
        if av.max() - av.min() > 30 or not 60 < av.mean() < 170: continue
        a = sum(len(P) for P in Ps)
        if a > area: mejor, area = ti, a
    return por_tex[mejor] if mejor is not None else None

def triangulos(tiras):
    T = []
    for P in tiras:
        if len(P) < 3 or not np.isfinite(P).all(): continue
        e = np.linalg.norm(np.diff(P, axis=0), axis=1); med = np.median(e)
        for j in range(len(P) - 2):
            if max(e[j], e[j + 1]) > 6 * med and max(e[j], e[j + 1]) > 15: continue
            T.append(P[j:j + 3])
    return np.array(T)

class Rejilla:
    """triangulos por celdas de 8 m en XZ; dentro(p, y) -> altura del triangulo que cubre p cerca de y, o None"""
    def __init__(self, T, C=8.0):
        self.T = T; self.C = C; self.rej = {}
        lo = np.floor(T[:, :, [0, 2]].min(1) / C).astype(int); hi = np.floor(T[:, :, [0, 2]].max(1) / C).astype(int)
        for k in range(len(T)):
            for gx in range(lo[k, 0], hi[k, 0] + 1):
                for gz in range(lo[k, 1], hi[k, 1] + 1): self.rej.setdefault((gx, gz), []).append(k)
    def dentro(self, p, y, dy=5):
        for k in self.rej.get((int(np.floor(p[0] / self.C)), int(np.floor(p[2] / self.C))), []):
            a, b, d = self.T[k]; v0 = b - a; v1 = d - a; v2 = p - a
            den = v0[0] * v1[2] - v1[0] * v0[2]
            if abs(den) < 1e-9: continue
            u = (v2[0] * v1[2] - v1[0] * v2[2]) / den; w = (v0[0] * v2[2] - v2[0] * v0[2]) / den
            if u < -1e-4 or w < -1e-4 or u + w > 1 + 1e-4: continue
            h = a[1] + u * v0[1] + w * v1[1]
            if abs(h - y) < dy: return h
        return None

def ajusta_al_asfalto(seg, cam):
    """recentra la ruta en el asfalto (triangulos con la textura de carretera) y da sus bordes y los carriles: en cada
    punto se prueba cada metro a lo ancho (-40 a 40) si cae en el asfalto"""
    tiras = puntos_asfalto(seg)
    if tiras is None: return cam, None, None
    rej = Rejilla(triangulos(tiras)); xs = np.arange(-40, 41, 1.0)
    for vuelta in range(2):
        t = np.gradient(cam, axis=0); t[:, 1] = 0; t /= np.linalg.norm(t, axis=1, keepdims=True) + 1e-9
        der = np.stack([-t[:, 2], np.zeros(len(t)), t[:, 0]], 1)
        occ = np.zeros((len(cam), len(xs)), bool)
        for i, c in enumerate(cam):
            for j, x in enumerate(xs): occ[i, j] = rej.dentro(c + x * der[i], c[1]) is not None
        lo = np.full(len(cam), np.nan); hi = np.full(len(cam), np.nan)
        for i in range(len(cam)):
            o = np.nonzero(occ[i])[0]
            if len(o): lo[i], hi[i] = xs[o[0]], xs[o[-1]]
        ok = ~np.isnan(lo); idx = np.arange(len(cam))
        lo = np.interp(idx, idx[ok], lo[ok]); hi = np.interp(idx, idx[ok], hi[ok])
        k = np.ones(9) / 9
        lo = np.convolve(np.pad(lo, 4, mode='edge'), k, 'valid'); hi = np.convolve(np.pad(hi, 4, mode='edge'), k, 'valid')
        if vuelta == 0: cam = cam + ((lo + hi) / 2)[:, None] * der
    # carriles: en los tramos ocupados (sin la mediana), unos 3,7 m cada uno; NCARRIL repartidos entre ellos
    car = np.zeros((len(cam), NCARRIL)); ant = [0.0]
    for i in range(len(cam)):
        o = occ[i]; pos = []; j = 0
        while j < len(xs):
            if o[j]:
                k2 = j
                while k2 + 1 < len(xs) and o[k2 + 1]: k2 += 1
                a2, b2 = xs[j] - 0.5, xs[k2] + 0.5; w = b2 - a2
                if w >= 3:
                    n = max(1, int(round(w / 3.7))); pos += [a2 + (m + 0.5) * w / n for m in range(n)]
                j = k2 + 1
            else: j += 1
        if not pos: pos = ant
        ant = pos
        car[i] = [pos[int(round(k * (len(pos) - 1) / (NCARRIL - 1)))] for k in range(NCARRIL)]
    k = np.ones(7) / 7
    car = np.stack([np.convolve(np.pad(car[:, j], 3, mode='edge'), k, 'valid') for j in range(NCARRIL)], 1)
    return cam, np.stack([np.maximum(3, -lo + 0.5), np.maximum(3, hi + 0.5)], 1), car

def carriles(seg, cam):
    """desvio lateral de cada carril (ficheros SOO de la etapa: puntos (x, z, y) en cuartos de metro cada ~8 m) en
    cada punto de la ruta: el punto del carril donde corta la perpendicular a la ruta"""
    ls = carriles_soo(seg)
    t = np.gradient(cam, axis=0); t[:, 1] = 0; t /= np.linalg.norm(t, axis=1, keepdims=True) + 1e-9
    der = np.stack([-t[:, 2], t[:, 0]], 1)
    res = np.zeros((len(cam), NCARRIL))
    for k in range(NCARRIL):
        if not ls: break
        v = ls[min(k, len(ls) - 1)][:, :2]                       # (x, z)
        for i, c in enumerate(cam):
            q = v - c[[0, 2]]; a = q @ t[i, [0, 2]]               # distancia a lo largo de la ruta
            j = int(np.argmin(np.abs(a) + 0.2 * np.abs(q @ der[i])))
            j2 = j + 1 if (j + 1 < len(v) and np.sign(a[j]) != np.sign(a[j + 1])) else max(j - 1, 0)
            if a[j2] != a[j]:
                f = a[j] / (a[j] - a[j2]); p = q[j] + f * (q[j2] - q[j])
            else: p = q[j]
            res[i, k] = p @ der[i]
    k = np.ones(5) / 5
    return np.stack([np.convolve(np.pad(res[:, j], 2, mode='edge'), k, 'valid') for j in range(NCARRIL)], 1)

def perfil(seg, cam, an):
    """alturas de la calzada a lo ancho en cada punto de la ruta (NPERF muestras de -izquierdo-2 a derecho+2): rayo
    vertical contra los triangulos casi horizontales del registro 0 y 1; el mas cercano a la altura de la ruta"""
    tris = []
    for r in (0, 1):
        for pk in seg.mallas(r):
            for tr in vif.tiras(pk):
                P = tr[0x68].astype(float)
                if len(P) < 3 or not np.isfinite(P).all(): continue
                e = np.linalg.norm(np.diff(P, axis=0), axis=1); med = np.median(e)
                for j in range(len(P) - 2):
                    if max(e[j], e[j + 1]) > 6 * med and max(e[j], e[j + 1]) > 15: continue
                    T = P[j:j + 3]; n = np.cross(T[1] - T[0], T[2] - T[0]); ln = np.linalg.norm(n)
                    if ln > 1e-6 and abs(n[1]) / ln > 0.6: tris.append(T)
    T = np.array(tris); C = 8.0; rej = {}
    lo = np.floor(T[:, :, [0, 2]].min(1) / C).astype(int); hi = np.floor(T[:, :, [0, 2]].max(1) / C).astype(int)
    for k in range(len(T)):
        for gx in range(lo[k, 0], hi[k, 0] + 1):
            for gz in range(lo[k, 1], hi[k, 1] + 1): rej.setdefault((gx, gz), []).append(k)
    t = np.gradient(cam, axis=0); t[:, 1] = 0; t /= np.linalg.norm(t, axis=1, keepdims=True) + 1e-9
    der = np.stack([-t[:, 2], np.zeros(len(t)), t[:, 0]], 1)
    res = np.zeros((len(cam), NPERF))
    for i, c in enumerate(cam):
        xs = np.linspace(-an[i, 0] - 2, an[i, 1] + 2, NPERF)
        for j, x in enumerate(xs):
            p = c + x * der[i]; best = None
            for k in rej.get((int(np.floor(p[0] / C)), int(np.floor(p[2] / C))), []):
                a, b, d = T[k]
                v0 = b - a; v1 = d - a; v2 = p - a
                den = v0[0] * v1[2] - v1[0] * v0[2]
                if abs(den) < 1e-9: continue
                u = (v2[0] * v1[2] - v1[0] * v2[2]) / den; w = (v0[0] * v2[2] - v2[0] * v0[2]) / den
                if u < -1e-4 or w < -1e-4 or u + w > 1 + 1e-4: continue
                y = a[1] + u * v0[1] + w * v1[1]
                if abs(y - c[1]) < 8 and (best is None or abs(y - c[1]) < abs(best - c[1])): best = y
            res[i, j] = best if best is not None else np.nan
        fila = res[i]
        if np.isnan(fila).all(): fila[:] = c[1]
        else:
            ok = ~np.isnan(fila); fila[:] = np.interp(np.arange(NPERF), np.nonzero(ok)[0], fila[ok])
    return res

NEXT = 3
NBANDA = 10
MURO = 0x010f4060                       # materiales de muro (CbwColiWall: contactos con r.u32(8) & 0x010f4060)

def es_muro(m):
    return m <= 0 or bool((1 << m) & MURO)

def superficies(seg, cam, an):
    """de la colision del original: en cada punto de la ruta se recorre la perpendicular cada 0,5 m (-60 a 60) desde el
    centro hacia fuera, tomando el poligono de colision mas cercano en altura al anterior; el limite transitable es el
    primer muro o vacio pasado el borde del asfalto. Devuelve (limites (n, 2), alturas exteriores (n, 2 * NEXT),
    franjas (n, 2 * NBANDA), alturas de la colision en las x del perfil (n, NPERF), NaN donde no hay); None si la
    etapa no tiene colision"""
    c = stage.coli(seg)
    if c is None: return None
    V, mat = c; C = 8.0; rej = {}
    lo = np.floor(V[:, :, [0, 2]].min(1) / C).astype(int); hi = np.floor(V[:, :, [0, 2]].max(1) / C).astype(int)
    for k in range(len(V)):
        for gx in range(lo[k, 0], hi[k, 0] + 1):
            for gz in range(lo[k, 1], hi[k, 1] + 1): rej.setdefault((gx, gz), []).append(k)
    # cada cuadrilatero en dos triangulos (0, 1, 2) y (0, 2, 3)
    TA = np.concatenate([V[:, 0], V[:, 0]]); TB = np.concatenate([V[:, 1], V[:, 2]]); TD = np.concatenate([V[:, 2], V[:, 3]])
    TM = np.concatenate([mat, mat]); nq = len(V)
    t = np.gradient(cam, axis=0); t[:, 1] = 0; t /= np.linalg.norm(t, axis=1, keepdims=True) + 1e-9
    der = np.stack([-t[:, 2], np.zeros(len(t)), t[:, 0]], 1)
    xs = np.arange(-60, 60.01, 0.5); j0 = len(xs) // 2
    lim = np.zeros((len(cam), 2)); ext = np.zeros((len(cam), 2 * NEXT)); ban = np.zeros((len(cam), 2 * NBANDA))
    hpf = np.full((len(cam), NPERF), np.nan)
    for i, c0 in enumerate(cam):
        P = c0[None, :] + xs[:, None] * der[i][None, :]
        cel = set(zip(np.floor(P[:, 0] / C).astype(int), np.floor(P[:, 2] / C).astype(int)))
        q = sorted({k for cc in cel for k in rej.get(cc, [])})
        alt = np.full(len(xs), np.nan); m = np.full(len(xs), -1)
        if q:
            q = np.array(q); q = np.concatenate([q, q + nq])
            a = TA[q]; v0 = TB[q] - a; v1 = TD[q] - a
            v2x = P[:, None, 0] - a[None, :, 0]; v2z = P[:, None, 2] - a[None, :, 2]
            den = v0[:, 0] * v1[:, 2] - v1[:, 0] * v0[:, 2]; ok = np.abs(den) > 1e-9; den = np.where(ok, den, 1)
            u = (v2x * v1[None, :, 2] - v1[None, :, 0] * v2z) / den; w = (v0[None, :, 0] * v2z - v2x * v0[None, :, 2]) / den
            dentro = ok[None, :] & (u >= -1e-4) & (w >= -1e-4) & (u + w <= 1 + 1e-4)
            H = np.where(dentro, a[None, :, 1] + u * v0[None, :, 1] + w * v1[None, :, 1], np.nan)
            for paso in (range(j0, -1, -1), range(j0, len(xs))):          # del centro hacia cada lado
                y = c0[1]; tol = 6.0
                for j in paso:
                    d = np.abs(H[j] - y)
                    if np.isfinite(d).any():
                        k = int(np.nanargmin(d))
                        if d[k] < tol: alt[j] = H[j, k]; m[j] = TM[q[k]]; y = alt[j]; tol = 2.5
        # huecos sueltos (juntas entre poligonos): el material del vecino
        for j in range(1, len(xs) - 1):
            if m[j] < 0 and m[j - 1] >= 0 and (m[j + 1] >= 0 or (j + 2 < len(xs) and m[j + 2] >= 0)):
                m[j] = m[j - 1]; alt[j] = alt[j - 1]
        ok = np.isfinite(alt)
        xp = np.linspace(-an[i, 0] - 2, an[i, 1] + 2, NPERF); jp = np.clip(np.round((xp + 60) / 0.5).astype(int), 0, len(xs) - 1)
        hpf[i] = np.where(ok[jp], alt[jp], np.nan)                             # la colision en las x del perfil
        if ok.any(): alt = np.interp(np.arange(len(xs)), np.nonzero(ok)[0], alt[ok])
        else: alt[:] = c0[1]
        # desde dentro de cada borde del asfalto hacia fuera, hasta el primer muro o vacio
        def borde(x0, sg):
            j = int(np.clip(round((x0 + 60) / 0.5), 0, len(xs) - 1))
            while es_muro(m[j]) and j != j0: j -= sg                          # hacia el centro hasta pisar algo
            if es_muro(m[j]): return x0
            while 0 <= j + sg < len(xs) and not es_muro(m[j + sg]): j += sg
            return xs[j] + 0.25 * sg
        # nunca por dentro del borde del asfalto (puntos sin colision bajo la ruta, como la salida de algunas etapas)
        li = max(-borde(-an[i, 0] + 1.0, -1), an[i, 0]); ld = max(borde(an[i, 1] - 1.0, 1), an[i, 1])
        lim[i] = (li, ld)
        # alturas exteriores: de -izq-2 (fin del perfil) al limite, y de der+2 al limite
        for lado, (x0, x1) in enumerate(((-an[i, 0] - 2, -li), (an[i, 1] + 2, ld))):
            for k in range(NEXT):
                x = x0 + (k + 1) / NEXT * (x1 - x0) if abs(x1) > abs(x0) else x0
                ext[i, lado * NEXT + k] = np.interp(x, xs, alt)
        # franjas entre los limites; sobre el asfalto (bordes de ajusta_al_asfalto) no se deja muro ni vacio
        sobre = (xs >= -an[i, 0]) & (xs <= an[i, 1]) & np.array([es_muro(v) for v in m])
        m[sobre] = 1
        ja = int(round((-li + 60.25) / 0.5)); jb = int(round((ld + 59.75) / 0.5)) + 1
        f = []
        for j in range(ja, max(jb, ja + 1)):
            mm = int(m[j]) if m[j] >= 0 else 0
            if not f or f[-1][1] != mm: f.append([xs[j] - 0.25, mm])
        if not f: f = [[-li, 1]]
        f[0][0] = -li
        while len(f) > NBANDA:                                               # la mas estrecha, con su vecina
            w = [(f[k + 1][0] if k + 1 < len(f) else ld) - f[k][0] for k in range(len(f))]
            k = int(np.argmin(w[1:])) + 1
            del f[k]
            if k < len(f) and f[k - 1][1] == f[k][1]: del f[k]
        f += [[ld, f[-1][1]]] * (NBANDA - len(f))
        ban[i] = np.array(f, float).reshape(-1)
    return lim, ext, ban, hpf

def carriles_uniformes(an, car=None):
    """carriles de unos 3,7 m repartidos a lo ancho del asfalto (NCARRIL desvios como en ajusta_al_asfalto) en los
    puntos sin carriles (todos en el mismo sitio: sin asfalto reconocido); car None: en todos"""
    car = np.zeros((len(an), NCARRIL)) if car is None else car.copy()
    for i in range(len(an)):
        if np.ptp(car[i]) > 1.0: continue
        a2, b2 = -an[i, 0] + 0.5, an[i, 1] - 0.5; w = b2 - a2; n = max(1, int(round(w / 3.7)))
        pos = [a2 + (m + 0.5) * w / n for m in range(n)]
        car[i] = [pos[int(round(k * (len(pos) - 1) / (NCARRIL - 1)))] for k in range(NCARRIL)]
    return car

def transforma(mod, M):
    """lleva el modelo y su ruta al marco del curso: M = (tx, ty, tz, ry), giro alrededor de y (x' = x cos + z sen,
    z' = -x sen + z cos, el de pc_matrix_rotate_y del original) y traslacion. El cielo (lista 3, centrado en la camara)
    solo gira. En la ruta se mueven la posicion y las alturas (perfil y alturas exteriores); el resto son desvios"""
    tx, ty, tz, ry = M; c, s = np.cos(ry), np.sin(ry)
    def gira(x, z): return x * c + z * s, -x * s + z * c
    cielo = {x[4] for x in mod.mallas if x[0] == 3}
    i0 = 0
    for v in mod.verts:
        x, z = gira(v['x'].astype(float), v['z'].astype(float)); en_cielo = i0 in cielo
        v['x'] = x + (0 if en_cielo else tx); v['z'] = z + (0 if en_cielo else tz)
        if not en_cielo: v['y'] = v['y'] + ty
        i0 += len(v)
    for x in mod.mallas:
        cx, cz = gira(x[2][0], x[2][2])
        x[2] = np.array([cx, x[2][1], cz]) + (0 if x[0] == 3 else np.array([tx, ty, tz]))
    r = mod.ruta.copy()
    if len(r):
        x, z = gira(r[:, 0], r[:, 2]); r[:, 0] = x + tx; r[:, 2] = z + tz; r[:, 1] += ty
        r[:, 5:5 + NPERF] += ty
        c_ext = 5 + NPERF + NCARRIL + 2
        if r.shape[1] >= c_ext + 2 * NEXT: r[:, c_ext:c_ext + 2 * NEXT] += ty
    mod.ruta = r

def convierte(seg, n, sal, mundo=None, cielo=True):
    """el segmento seg (numero n) a sal; mundo: transformacion al marco del curso (transforma)"""
    mod = modelo.Modelo(); mod.limita = True
    for blq in seg.bloques:
        if blq.o == seg.o:                                     # el bloque principal, con los objetos en su sitio
            mod.bloque(blq, objetos=True)
        elif es_cielo(blq):
            if cielo: mod.bloque(blq, lista=3)                 # el cielo: alrededor de la camara, sin niebla
        # los otros bloques: versiones de lejos (LOD), aparte
    cam = recorrido_soo(seg)
    if cam is None: cam = recorrido(seg)
    cam, an, car = ajusta_al_asfalto(seg, cam)                 # centro, bordes y carriles: los del asfalto
    if an is None: an = anchos(seg, cam); car = np.zeros((len(cam), NCARRIL))
    car = carriles_uniformes(an, car)                          # donde no se reconocieron carriles
    sup = superficies(seg, cam, an)
    if sup is None:                                            # sin colision: el limite es el borde del asfalto
        ban = np.zeros((len(cam), NBANDA, 2)); ban[:, :, 0] = an[:, 1:2]; ban[:, :, 1] = 1; ban[:, 0, 0] = -an[:, 0]
        sup = (an.copy(), np.zeros((len(cam), 2 * NEXT)), ban.reshape(len(cam), -1))
    pf = perfil(seg, cam, an)
    if len(sup) > 3:
        # fuera del asfalto el perfil (rayos contra la geometria visible) puede caer en el terreno de mas abajo: alli
        # manda la altura del suelo de la colision, el que pisa el coche en el original
        xp = np.linspace(-an[:, :1] - 2, an[:, 1:] + 2, NPERF, axis=1)[:, :, 0]
        fuera = ((xp < -an[:, :1] + 0.5) | (xp > an[:, 1:] - 0.5)) & np.isfinite(sup[3])
        dentro = ~fuera & np.isfinite(sup[3])
        if dentro.any(): print('colision - perfil en el asfalto: mediana %.2f m, p95 %.2f m'
                               % (np.median(np.abs(sup[3] - pf)[dentro]), np.percentile(np.abs(sup[3] - pf)[dentro], 95)))
        pf = np.where(fuera, sup[3], pf)
    if not sup[1].any(): sup = (sup[0], np.repeat(pf[:, [0, -1]], NEXT, 1), sup[2])
    mod.ruta = np.concatenate([cam, an, pf, car, sup[0], sup[1], sup[2]], 1)
    mod.recorta_texturas()                                     # solo las texturas que se usan (memoria de video)
    if mundo is not None: transforma(mod, mundo)
    ini = mod.escribe(sal)
    print('segmento %d: %d tiras (%d opacas, %d recorte, %d transparentes, %d cielo), %d vertices, %d triangulos, '
          '%d texturas (%d KB), ruta %d puntos (%.0f m), %d KB'
          % (n, len(mod.mallas), ini[1], ini[2] - ini[1], ini[3] - ini[2], ini[4] - ini[3], mod.nv,
             sum(x[5] - 2 for x in mod.mallas), len(mod.texs), len(mod.blob) >> 10, len(cam), 4 * len(cam),
             os.path.getsize(sal) >> 10))
    return mod

def main():
    """conv_tramo.py tu.iso n salida.dat [tx ty tz ry]: con los cuatro numeros, el tramo va colocado en el marco del
    curso (conv_union.py los saca de los descriptores del ejecutable)"""
    ruta, n, sal = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    mundo = tuple(float(x) for x in sys.argv[4:8]) if len(sys.argv) >= 8 else None
    seg = stage.segmentos(isomod.Iso(ruta))[n]
    convierte(seg, n, sal, mundo)

if __name__ == '__main__':
    main()
