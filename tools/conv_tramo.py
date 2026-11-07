#!/usr/bin/env python3
"""conv_tramo.py tu.iso n salida.dat: segmento n de STAGE.PS2 al formato de la prueba 2 de Dreamcast, con texturas
(todos los bloques del segmento; el cielo aparte),
y el recorrido de la camara por la calzada. Formato (little endian):
 'ORD3', nmallas, nvert, ncam, ntex, inicio de cada lista (opaca, recorte, transparente, cielo, fin);
 texturas (ancho, alto, formato 0 ARGB1555 / 1 RGB565 / 2 ARGB4444, lista, desplazamiento, tamano: 24 bytes);
 mallas ordenadas por lista y textura (cx, cy, cz, radio float, primer vertice, cuenta, textura o -1: 28 bytes);
 vertices (x, y, z, u, v float, color ARGB: 24 bytes); camara (x, y, z float); relleno a 32; texeles twiddled.
Cada malla es una tira de triangulos (un UNPACK de posiciones del paquete VIF); su textura sale de la tabla 2 del
registro. Sale de tu disco: no se distribuye."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2 import iso as isomod, stage, tex, vif

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

def twiddle(w, h):
    """orden de los texeles en una textura 'twiddled' de la PowerVR (y en los bits pares, x en los impares;
    las no cuadradas son bloques cuadrados de lado min(w, h) seguidos)"""
    s = min(w, h); y, x = np.mgrid[0:h, 0:w]
    def esp(v):
        r = np.zeros_like(v)
        for b in range(10): r |= ((v >> b) & 1) << (2 * b)
        return r
    blo = (x // s) if w > h else (y // s)
    return blo * s * s + esp(y % s) | (esp(x % s) << 1)

def textura_dc(a):
    """RGBA -> (formato, lista, datos twiddled de 16 bits). Lista 0 opaca, 1 recorte (alfa de 1 bit), 2 transparente"""
    al = a[..., 3].astype(int); h, w = al.shape
    if (al >= 240).all() or al.max() - al.min() < 64: lista, fmt = 0, 1   # RGB565 (alfa casi constante: no es
                                                                 # transparencia, lo usa el juego para otra cosa)
    elif ((al < 16) | (al >= 240)).mean() > 0.97: lista, fmt = 1, 0   # ARGB1555
    else: lista, fmt = 2, 2                                      # ARGB4444
    r, g, b = (a[..., k].astype(np.uint32) for k in range(3))
    if fmt == 1: v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    elif fmt == 0: v = ((al >= 128).astype(np.uint32) << 15) | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)
    else: v = ((al.astype(np.uint32) >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)
    out = np.zeros(w * h, np.uint16); out[twiddle(w, h).ravel()] = v.ravel()
    return fmt, lista, out.tobytes()

def main():
    ruta, n, sal = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    seg = stage.segmentos(isomod.Iso(ruta))[n]; m, A = seg.m, seg.A
    L = np.array([0.3, 0.8, 0.5]); L /= np.linalg.norm(L)
    texs = []; blob = b''; mallas = []; verts = []; nv = 0
    for blq in seg.bloques:
        if blq.o != seg.o and not es_cielo(blq): continue     # los otros bloques: versiones de lejos (LOD), aparte
        m, A = blq.m, blq.A; base = len(texs)
        for h in tex.cabeceras(blq):
            a = tex.imagen(blq, h); fmt, lista, d = textura_dc(a)
            texs.append((a.shape[1], a.shape[0], fmt, lista, len(blob), len(d))); blob += d
        ntb = len(texs) - base; cielo = None
        for r, reg in enumerate(blq.registros()):
            c, o = reg[3]; t2 = reg[2][1]
            if blq.o == seg.o and local(blq, r): continue           # prototipos en su origen (arboles...): se colocan aparte
            for i in range(c):
                e = struct.unpack('<6I', m[A + o + 24 * i:A + o + 24 * i + 24])
                ti = struct.unpack('<i', m[A + t2 + 16 * i:A + t2 + 16 * i + 4])[0]
                ti -= 1                                       # la tabla 2 cuenta las texturas desde 1 (0: sin textura)
                ti = base + ti if 0 <= ti < ntb else -1
                for pk in vif.datos(m, A + e[2], A + blq.w[2]):
                    for tr in vif.tiras(pk):                  # cada tira: GIFtag, posiciones, normales, UV, color
                        P = tr[0x68].astype(np.float32); nn = len(P)
                        if nn < 3: continue
                        uv = np.zeros((nn, 2))
                        for cl in (0x6d, 0x65):
                            if cl in tr and len(tr[cl]) == nn: uv = tr[cl][:, :2].astype(float) / 4096; break
                        if 0x6e in tr and len(tr[0x6e]) == nn:
                            col = np.clip(tr[0x6e][:, :3].astype(float) * 2, 0, 255)
                        else:
                            N = tr[0x69].astype(float) / 32768 if 0x69 in tr and len(tr[0x69]) == nn else np.zeros_like(P)
                            luz = 0.7 + 0.3 * np.clip(np.abs(N @ L), 0, 1)
                            col = np.repeat(255 * luz[:, None], 3, 1)
                        col = col.astype(np.uint32)
                        argb = (0xFF << 24) | (col[:, 0] << 16) | (col[:, 1] << 8) | col[:, 2]
                        if ti < 0: argb[:] = 0x60202020            # textura 0: aun sin identificar (gris oscuro translucido)
                        cc = P.mean(0); rad = float(np.sqrt(((P - cc) ** 2).sum(1).max()))
                        lista = texs[ti][3] if ti >= 0 else 2
                        mallas.append([lista, ti, cc, rad, nv, nn, blq])
                        v = np.zeros(nn, dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('u', '<f4'), ('v', '<f4'), ('c', '<u4')])
                        v['x'], v['y'], v['z'], v['u'], v['v'], v['c'] = P[:, 0], P[:, 1], P[:, 2], uv[:, 0], uv[:, 1], argb
                        verts.append(v); nv += nn
    # el cielo: el bloque centrado en el origen que sube muy alto; va en su propia lista (3), alrededor de la camara
    for blq in seg.bloques:
        if es_cielo(blq):
            for x in mallas:
                if x[6] is blq: x[0] = 3
    mallas.sort(key=lambda x: (x[0], x[1]))
    cam = recorrido(seg)
    inicio = [sum(1 for x in mallas if x[0] < l) for l in range(5)]
    with open(sal, 'wb') as f:
        f.write(b'ORD3' + struct.pack('<9I', len(mallas), nv, len(cam), len(texs), *inicio))
        for t in texs: f.write(struct.pack('<6I', *t))
        for lista, ti, cc, rad, i0, cnt, _ in mallas: f.write(struct.pack('<4f2Ii', *cc, rad, i0, cnt, ti))
        for v in verts: f.write(v.tobytes())
        f.write(cam.astype('<f4').tobytes())
        f.write(b'\0' * (-f.tell() % 32)); f.write(blob)
    print('segmento %d: %d tiras (%d opacas, %d recorte, %d transparentes, %d cielo), %d vertices, %d triangulos, %d texturas '
          '(%d KB), camara %d puntos, %d KB'
          % (n, len(mallas), inicio[1], inicio[2] - inicio[1], inicio[3] - inicio[2], inicio[4] - inicio[3], nv,
             sum(x[5] - 2 for x in mallas), len(texs), len(blob) >> 10, len(cam), os.path.getsize(sal) >> 10))

if __name__ == '__main__':
    main()
