#!/usr/bin/env python3
"""invasion.py datos/ [salida.csv] [SEGnn.DAT|BKnn.DAT ...]: geometria que invade el espacio de la calzada.

Para cada etapa (y union, si se piden), los triangulos de la escena (listas 0, 1 y 2, sin el cielo ni el agua, ni los carteles: sus vertices van en el marco de su
matriz, ni las versiones lejanas) con
algun punto dentro del volumen por donde circula el coche: sobre el asfalto (del ancho izquierdo al derecho de la
ruta, menos MARGEN) y de ALTO0 a ALTO1 m por encima de la calzada (las NPERF alturas del perfil de la ruta a lo ancho,
tools/conv_tramo.py). Se miran los vertices, el centro y los puntos medios de los lados de cada triangulo contra el
punto de la ruta mas cercano (cada 4 m), en su marco (tangente, derecha). Tambien los triangulos enormes (area >
GIGANTE m2) que cortan ese volumen.

No decide nada: es una lista para revisar (lo que esta legitimamente sobre la calzada: porticos de carteles bajos,
pancartas, peajes, barreras de una bifurcacion, y lo que la ruta no describe bien en los cruces). Sale una fila por
(fichero, nodo, textura) con los triangulos, el tramo de la ruta (s en m), la altura y la distancia lateral, y un
resumen por fichero. Solo numeros y nombres de fichero: nada del disco."""
import csv, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conv_reflejo as cr

NPERF = 16
NCARRIL = 6
MARGEN = float(os.environ.get("INV_MARGEN", "2.0"))           # m dentro del borde del asfalto (guardarrailes y bordillos fuera)
ALTO0, ALTO1 = 0.6, 3.5                                        # m sobre la calzada: el coche y su camara
GIGANTE = 400.0                                                # m2
NODO_CIELO, NODO_AGUA = -256, -258

def triangulos(m):
    """(T [n,3,3], nodo [n], tex [n], lista [n]) de las listas 0..2 sin cielo ni agua"""
    T, nd, tx, ls = [], [], [], []
    for l in range(3):
        for k in range(m.listas[l], m.listas[l + 1]):
            ma = m.mallas[k]; n = int(ma['n'])
            if n < 3 or int(ma['nodo']) in (NODO_CIELO, NODO_AGUA): continue
            nn = int(ma['nodo'])                               # carteles (vertices en su marco: los coloca su matriz)
            if 0 <= nn < m.nn and (m.nodos[nn]['cartel'] != -1 or m.nodos[nn]['ban'] & 0x20): continue   # y lejanos
            P = m.verts['p'][int(ma['v0']):int(ma['v0']) + n].astype(np.float64)
            t = np.stack([P[:-2], P[1:-1], P[2:]], 1)
            area = np.linalg.norm(np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0]), axis=1) * 0.5
            t = t[area > 1e-4]
            T.append(t); nd += [int(ma['nodo'])] * len(t); tx += [int(ma['tex'])] * len(t); ls += [l] * len(t)
    return (np.concatenate(T) if T else np.zeros((0, 3, 3))), np.array(nd), np.array(tx), np.array(ls)

def analiza(m, nombre):
    from scipy.spatial import cKDTree
    R = m.ruta.astype(np.float64)
    if len(R) < 3 or R.shape[1] < 5 + NPERF: return []
    C = R[:, :3]; izq, der = R[:, 3], R[:, 4]; perf = R[:, 5:5 + NPERF]
    if R.shape[1] > 5 + NPERF + NCARRIL + 1:                   # el limite transitable de la colision (hasta el muro)
        li, ld = R[:, 5 + NPERF + NCARRIL], R[:, 5 + NPERF + NCARRIL + 1]
        izq = np.where(li > 0.5, np.minimum(izq, li), izq); der = np.where(ld > 0.5, np.minimum(der, ld), der)
    t = np.gradient(C, axis=0); t[:, 1] = 0; t /= np.linalg.norm(t, axis=1, keepdims=True) + 1e-9
    dr = np.stack([-t[:, 2], np.zeros(len(t)), t[:, 0]], 1)
    T, nd, tx, ls = triangulos(m)
    if not len(T): return []
    mid = np.stack([(T[:, 0] + T[:, 1]) / 2, (T[:, 1] + T[:, 2]) / 2, (T[:, 2] + T[:, 0]) / 2, T.mean(1)], 1)
    S = np.concatenate([T, mid], 1)                            # 7 puntos por triangulo
    area = np.linalg.norm(np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]), axis=1) * 0.5
    arbol = cKDTree(C[:, [0, 2]])
    Sp = S.reshape(-1, 3); _, j = arbol.query(Sp[:, [0, 2]])
    d = Sp - C[j]
    a = (d * t[j]).sum(1); lat = (d * dr[j]).sum(1)
    xs0 = -R[j, 3] - 2; paso = (R[j, 3] + R[j, 4] + 4) / (NPERF - 1)   # (el perfil va con el ancho del asfalto)
    f = np.clip((lat - xs0) / np.maximum(paso, 1e-6), 0, NPERF - 1.001); i0 = f.astype(int); w = f - i0
    suelo = perf[j, i0] * (1 - w) + perf[j, i0 + 1] * w
    h = Sp[:, 1] - suelo
    dentro = (np.abs(a) <= 2.5) & (lat > -izq[j] + MARGEN) & (lat < der[j] - MARGEN) & (h > ALTO0) & (h < ALTO1)
    dentro = dentro.reshape(len(T), -1)
    malo = dentro.any(1); gig = malo & (area > GIGANTE)
    filas = {}
    J = j.reshape(len(T), -1); H = h.reshape(len(T), -1); L = lat.reshape(len(T), -1)
    for k in np.nonzero(malo)[0]:
        q = dentro[k]; key = (int(nd[k]), int(tx[k]), int(ls[k]))
        r = filas.setdefault(key, dict(tri=0, gig=0, s0=1e9, s1=-1e9, h0=1e9, h1=-1e9, l0=1e9, l1=-1e9, area=0.0))
        r['tri'] += 1; r['gig'] += bool(gig[k]); r['area'] = max(r['area'], float(area[k]))
        ss = J[k][q] * 4.0; r['s0'] = min(r['s0'], ss.min()); r['s1'] = max(r['s1'], ss.max())
        r['h0'] = min(r['h0'], H[k][q].min()); r['h1'] = max(r['h1'], H[k][q].max())
        r['l0'] = min(r['l0'], L[k][q].min()); r['l1'] = max(r['l1'], L[k][q].max())
    return [dict(fichero=nombre, nodo=k[0], tex=k[1], lista=k[2], **{x: (round(v, 1) if isinstance(v, float) else v) for x, v in r.items()})
            for k, r in sorted(filas.items(), key=lambda kv: -kv[1]['tri'])]

def main():
    a = sys.argv[1:]
    if not a: raise SystemExit(__doc__)
    datos = a[0]; sal = a[1] if len(a) > 1 and a[1].endswith('.csv') else None
    nombres = [x for x in a[1:] if x.endswith('.DAT')] or sorted(f for f in os.listdir(datos) if f.startswith('SEG') and f[3:5].isdigit() and len(f) == 9)
    todo = []; paks = {}
    for f in nombres:
        try: m = cr.carga(datos, f, paks)
        except Exception as e: print('%s: %s' % (f, e)); continue
        r = analiza(m, f); todo += r
        print('%s: %d grupos (nodo, textura), %d triangulos dentro, %d gigantes' % (f, len(r), sum(x['tri'] for x in r), sum(x['gig'] for x in r)), flush=True)
    if sal and todo:
        with open(sal, 'w', newline='') as o:
            w = csv.DictWriter(o, list(todo[0].keys())); w.writeheader(); w.writerows(todo)
        print('%d filas en %s' % (len(todo), sal))

if __name__ == '__main__':
    main()
