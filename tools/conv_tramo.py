#!/usr/bin/env python3
"""conv_tramo.py tu.iso n salida.dat: segmento (etapa) n de STAGE.PS2 al formato ORD4 de la Dreamcast (modelo.py):
el bloque principal con sus texturas, el cielo aparte y la ruta (centro de la calzada cada 4 m con el ancho a cada
lado). Sale de tu disco: no se distribuye."""
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

def main():
    ruta, n, sal = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    seg = stage.segmentos(isomod.Iso(ruta))[n]
    mod = modelo.Modelo(); mod.limita = True
    for blq in seg.bloques:
        if blq.o == seg.o:                                     # el bloque principal, con los objetos en su sitio
            mod.bloque(blq, objetos=True)
        elif es_cielo(blq):
            mod.bloque(blq, lista=3)                           # el cielo: alrededor de la camara, sin niebla
        # los otros bloques: versiones de lejos (LOD), aparte
    cam = recorrido_soo(seg)
    if cam is None: cam = recorrido(seg)
    cam, an, car = ajusta_al_asfalto(seg, cam)                 # centro, bordes y carriles: los del asfalto
    if an is None: an = anchos(seg, cam); car = np.zeros((len(cam), NCARRIL))
    mod.ruta = np.concatenate([cam, an, perfil(seg, cam, an), car], 1)
    ini = mod.escribe(sal)
    print('segmento %d: %d tiras (%d opacas, %d recorte, %d transparentes, %d cielo), %d vertices, %d triangulos, '
          '%d texturas (%d KB), ruta %d puntos (%.0f m), %d KB'
          % (n, len(mod.mallas), ini[1], ini[2] - ini[1], ini[3] - ini[2], ini[4] - ini[3], mod.nv,
             sum(x[5] - 2 for x in mod.mallas), len(mod.texs), len(mod.blob) >> 10, len(cam), 4 * len(cam),
             os.path.getsize(sal) >> 10))

if __name__ == '__main__':
    main()
