#!/usr/bin/env python3
"""conv_tramo.py tu.iso n salida.dat: segmento n de STAGE.PS2 al formato de la prueba 1 de Dreamcast (sin texturas:
color por parte del segmento con la luz de la normal ya aplicada) y el recorrido de la camara por la calzada.
Formato (little endian): 'ORD1', nmallas, nvert, ncam; mallas (cx, cy, cz, radio float, primer vertice, cuenta,
registro: 28 bytes); vertices (x, y, z float, color ARGB: 16 bytes); camara (x, y, z float).
Cada malla es una tira de triangulos (un UNPACK de posiciones del paquete VIF). Sale de tu disco: no se distribuye."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2 import iso as isomod, stage

COLOR = {0: (120, 120, 125), 7: (200, 200, 190), 1: (90, 140, 70), 5: (110, 150, 80)}   # calzada, bordes, terreno

def recorrido(seg):
    pts = []
    for pk in seg.mallas(0):                            # solo la superficie: tiras casi horizontales (normal casi vertical)
        if 0x68 not in pk or 0x69 not in pk: continue
        P = np.vstack([a for _, a in pk[0x68]]).astype(float)
        N = np.vstack([a for _, a in pk[0x69]]).astype(float) / 32768
        if np.abs(N[:, 1]).mean() < 0.9: continue
        for i in range(0, len(P) - 1, 2): pts.append((P[i] + P[i + 1]) / 2)
    pts = np.array(pts)
    # vecino mas cercano desde el extremo (el punto mas alejado del centro)
    c = pts.mean(0); i = int(np.argmax(((pts - c) ** 2).sum(1)))
    usado = np.zeros(len(pts), bool); orden = [i]; usado[i] = True
    while True:
        d = ((pts - pts[orden[-1]]) ** 2).sum(1); d[usado] = np.inf
        j = int(np.argmin(d))
        if not np.isfinite(d[j]) or d[j] > 60.0 ** 2: break
        orden.append(j); usado[j] = True
    p = pts[orden]
    # cada 4 unidades, suavizado
    L = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(p, axis=0), axis=1))])
    s = np.arange(0, L[-1], 4.0)
    q = np.stack([np.interp(s, L, p[:, k]) for k in range(3)], 1)
    k = np.ones(9) / 9
    q = np.stack([np.convolve(np.pad(q[:, j], 4, mode='edge'), k, 'valid') for j in range(3)], 1)
    return q

def main():
    ruta, n, sal = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    seg = stage.segmentos(isomod.Iso(ruta))[n]
    L = np.array([0.3, 0.8, 0.5]); L /= np.linalg.norm(L)
    mallas = []; verts = []
    for r in range(seg.w[0]):
        base = np.array(COLOR.get(r, (170, 150, 120)), float)
        for pk in seg.mallas(r):
          if 0x68 not in pk: continue
          for k, (_, P) in enumerate(pk[0x68]):         # cada UNPACK de posiciones es una tira (su GIFtag: NLOOP)
            P = P.astype(np.float32)
            N = pk[0x69][k][1].astype(float) / 32768 if 0x69 in pk and k < len(pk[0x69]) else np.zeros_like(P)
            if len(P) < 3: continue
            luz = 0.45 + 0.55 * np.clip(np.abs(N @ L), 0, 1)
            col = np.clip(base[None, :] * luz[:, None], 0, 255).astype(np.uint32)
            argb = (0xFF << 24) | (col[:, 0] << 16) | (col[:, 1] << 8) | col[:, 2]
            c = P.mean(0); rad = float(np.sqrt(((P - c) ** 2).sum(1).max()))
            mallas.append((c, rad, sum(len(v) for v, _ in verts), len(P), r))
            verts.append((P, argb))
    cam = recorrido(seg)
    with open(sal, 'wb') as f:
        nv = sum(len(v) for v, _ in verts)
        f.write(b'ORD1' + struct.pack('<3I', len(mallas), nv, len(cam)))
        for c, rad, i0, cnt, r in mallas: f.write(struct.pack('<4f3I', *c, rad, i0, cnt, r))
        for P, argb in verts:
            a = np.zeros(len(P), dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('c', '<u4')])
            a['x'], a['y'], a['z'], a['c'] = P[:, 0], P[:, 1], P[:, 2], argb
            f.write(a.tobytes())
        f.write(cam.astype('<f4').tobytes())
    print('segmento %d: %d mallas, %d vertices, %d triangulos, camara %d puntos (%.0f unidades), %d KB'
          % (n, len(mallas), nv, sum(c - 2 for _, _, _, c, _ in mallas), len(cam), 4 * len(cam), os.path.getsize(sal) >> 10))

if __name__ == '__main__':
    main()
