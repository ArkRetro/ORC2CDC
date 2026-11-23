#!/usr/bin/env python3
"""lod_grande.py fichero.DAT... | --datos datos/ trabajo/ [ficheros]: version lejana lowpoly de los elementos grandes de las
etapas y uniones (ORD7). DESVIACION pedida por el usuario (Rev 2.9.2.d, docs/PARIDAD.md): "generar modelos lowpoly y
texturas de baja resolucion para los elementos grandes como puentes, montanas, bloques de edificios, ruinas,
piramides; ... cuando el coche este a 900 m cargara completo".

Candidatos: nodos del original (no carteles, ni con version lejana de lod_veg.py, ni lejanos) de radio >= R_MIN m con la
mayor parte de sus vertices en la lista opaca y un coste teorico del TA de al menos MIN_TEO bytes. Su version lejana:
 - solo los triangulos opacos (las tiras de la lista 0: lo de recorte y transparente, barandillas, cables, hojas,
   cristales, de lejos no se ve);
 - simplificada por agrupacion de vertices en una rejilla (cada celda un vertice: la media de los suyos), con la celda
   creciendo hasta quedar en OBJ de los triangulos (como mucho MAX_TRI, como poco MIN_TRI); fuera los triangulos que
   se aplastan y los repetidos;
 - "textura de baja resolucion": cada textura reducida a BAJA x BAJA texeles (media de cada bloque) y cocida en el color
   de los vertices (textura x color del vertice / 255, como el dibujo de la PowerVR con MODULATE): sin texturas nuevas
   (0 KB de VRAM), una cabecera de solo color;
 - en tiras (voraz por aristas compartidas; hasta 256 vertices por tira) en la lista opaca, sin textura (-1).
En el fichero: las mallas lejanas al final de la lista opaca, sus vertices al final, un nodo por version lejana al
final de la tabla (esfera del completo, banderas del completo con NODO_LEJ y NODO_LEJ3D, cartel -1, relleno[1] = el
completo) y en el completo cartel = -2 - (su lejano). La demo (demo/main.c nodos_visibles) dibuja el lejano con el
centro del nodo a mas de LOD_GRANDE_D (900 m) por delante y el completo al acercarse (con histeresis). La PVS se rehace
con los indices nuevos (las mallas lejanas se ven en una seccion si alguna del completo se ve). Va despues de
lod_veg.py (que no toca ficheros que ya tienen nodos lejanos). Idempotente: un fichero con NODO_LEJ3D no se toca."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pvs, vq
from lod_veg import teo, _pvs_raw, _decodifica_lista, _textura, NODO_LEJ

NODO_LEJ3D = 0x40                                               # version lejana en 3D (no impostor)
R_MIN = 40.0                                                     # m: radio minimo del nodo
MIN_TEO = 6000                                                   # bytes teoricos del TA (~250 triangulos)
OBJ, MIN_TRI, MAX_TRI = 0.12, 24, 400
BAJA = 8                                                         # texeles de lado de la textura reducida

def _baja(d, t, cache, cb):
    if t in cb: return cb[t]
    tx, li, fmt = _textura(d, t, cache)
    h, w = tx.shape[:2]; bh, bw = max(1, h // BAJA), max(1, w // BAJA)
    a = tx[:bh * BAJA, :bw * BAJA, :3].reshape(BAJA, bh, BAJA, bw, 3).mean((1, 3)) if h >= BAJA and w >= BAJA else \
        np.broadcast_to(tx[..., :3].reshape(-1, 3).mean(0), (BAJA, BAJA, 3)).copy()
    cb[t] = a.astype(np.float32); return cb[t]

def candidatos(d):
    M, N = d['M'], d['N']; ini = d['h'][5:10]; nn = len(N)
    vt = np.zeros(nn); vo = np.zeros(nn); tb = np.zeros(nn); ms = {}
    for k in range(ini[3]):
        x = M[k]; nd = int(x['nodo'])
        if not 0 <= nd < nn or x['n'] < 3: continue
        vt[nd] += x['n']; tb[nd] += teo(min(int(x['n']), 256))
        if k < ini[1]: vo[nd] += x['n']; ms.setdefault(nd, []).append(k)
    ok = (vt > 0) & (N['r'] >= R_MIN) & (vo >= 0.6 * vt) & (tb >= MIN_TEO) & (N['cartel'] == -1) & ((N['fl'] & NODO_LEJ) == 0)
    return {int(i): ms[int(i)] for i in np.nonzero(ok)[0] if int(i) in ms}

def simplifica(P, C, T, r):
    """(vertices, colores, triangulos) agrupando en una rejilla cada vez mayor hasta OBJ de los triangulos"""
    obj = int(np.clip(OBJ * len(T), MIN_TRI, MAX_TRI))
    lo = P.min(0); celda = max(r / 40.0, 0.5)
    for _ in range(40):
        q = np.floor((P - lo) / celda).astype(np.int64)
        clave = (q[:, 0] * 73856093) ^ (q[:, 1] * 19349663) ^ (q[:, 2] * 83492791)
        _, grupo = np.unique(clave, return_inverse=True)
        G = grupo[T]
        bueno = (G[:, 0] != G[:, 1]) & (G[:, 1] != G[:, 2]) & (G[:, 0] != G[:, 2])
        G = G[bueno]
        if len(G):
            G = np.unique(np.sort(G, 1), axis=0) if True else G
        if len(G) <= obj: break
        celda *= 1.25
    ng = grupo.max() + 1
    cnt = np.bincount(grupo, minlength=ng).astype(np.float64)[:, None]
    Pc = np.zeros((ng, 3)); np.add.at(Pc, grupo, P); Pc /= np.maximum(cnt, 1)
    Cc = np.zeros((ng, 3)); np.add.at(Cc, grupo, C); Cc /= np.maximum(cnt, 1)
    usados = np.unique(G); nuevo = -np.ones(ng, np.int64); nuevo[usados] = np.arange(len(usados))
    return Pc[usados], Cc[usados], nuevo[G], celda

def tiras(T, maximo=256):
    """tiras voraces por aristas compartidas: [[indices]]"""
    aristas = {}
    for i, (a, b, c) in enumerate(T):
        for e in ((a, b), (b, c), (c, a)): aristas.setdefault(frozenset(e), []).append(i)
    libre = np.ones(len(T), bool); res = []
    for i in range(len(T)):
        if not libre[i]: continue
        libre[i] = False; s = list(T[i])
        while len(s) < maximo:
            x, y = s[-2], s[-1]; sig = None
            for j in aristas.get(frozenset((x, y)), ()):
                if libre[j]: sig = j; break
            if sig is None: break
            libre[sig] = False; z = [v for v in T[sig] if v != x and v != y]
            if not z: break
            s.append(int(z[0]))
        res.append(s)
    return res

def aplica(fn, informe=True):
    d = pvs.lee(fn)
    N, M, V = d['N'], d['M'], d['V']; h = list(d['h']); nn = len(N)
    if nn and (N['fl'] & NODO_LEJ3D).any(): return None        # ya hecho
    cand = candidatos(d)
    if not cand: return None
    raw = _pvs_raw(fn); cache = {}; cb = {}
    ini = list(h[5:10]); ins = ini[1]; nv0 = len(V)
    vn = []; mn = []; nodos_n = []; orden = sorted(cand); n_in = n_out = 0; meshes_de = {}
    Nn = N.copy()
    for j, nd in enumerate(orden):
        Ps, Cs, Ts = [], [], []; base = 0
        for k in cand[nd]:
            x = M[k]; vv = V[int(x['v0']):int(x['v0']) + int(x['n'])]
            P = np.stack([vv['x'], vv['y'], vv['z']], 1).astype(np.float64)
            c = vv['c'].astype(np.uint32)
            C = np.stack([(c >> 16) & 255, (c >> 8) & 255, c & 255], 1).astype(np.float64)
            t = int(x['t'])
            if t >= 0:
                tb = _baja(d, t, cache, cb)
                iu = (np.floor(vv['u'] * BAJA).astype(int) % BAJA); iv = (np.floor(vv['v'] * BAJA).astype(int) % BAJA)
                C = tb[iv, iu] * C / 255.0
            n = len(P); idx = np.arange(n) + base
            Ts.append(np.stack([idx[:-2], idx[1:-1], idx[2:]], 1)); Ps.append(P); Cs.append(C); base += n
        P = np.concatenate(Ps); C = np.concatenate(Cs); T = np.concatenate(Ts)
        a = np.linalg.norm(np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]]), axis=1)
        T = T[a > 1e-6]; n_in += len(T)
        Pc, Cc, Tc, celda = simplifica(P, C, T, float(N[nd]['r']))
        n_out += len(Tc)
        jl = nn + j; meshes_de[nd] = []
        for s in tiras([tuple(t) for t in Tc]):
            q = np.zeros(len(s), pvs.VERT); Q = Pc[s]
            q['x'], q['y'], q['z'] = Q[:, 0], Q[:, 1], Q[:, 2]
            col = np.clip(np.rint(Cc[s]), 0, 255).astype(np.uint32)
            q['c'] = 0xFF000000 | (col[:, 0] << 16) | (col[:, 1] << 8) | col[:, 2]
            m = np.zeros(1, pvs.MALLA); cc = Q.mean(0)
            m['c'] = cc; m['r'] = float(np.sqrt(((Q - cc) ** 2).sum(1).max())); m['v0'] = nv0 + sum(len(x) for x in vn)
            m['n'] = len(s); m['t'] = -1; m['nodo'] = jl
            vn.append(q); meshes_de[nd].append(len(mn)); mn.append(m)
        nd_n = np.zeros(1, pvs.NODO); nd_n['c'] = N[nd]['c']; nd_n['r'] = N[nd]['r']
        nd_n['fl'] = (int(N[nd]['fl']) & 0xFFFF030F) | NODO_LEJ | NODO_LEJ3D; nd_n['cartel'] = -1; nd_n['rel'] = (0, nd)
        nodos_n.append(nd_n); Nn[nd]['cartel'] = -2 - jl
    nadd = len(mn)
    if not nadd: return None
    mn = np.concatenate(mn); vn = np.concatenate(vn); nodos_n = np.concatenate(nodos_n)
    Mn = np.concatenate([M[:ins], mn, M[ins:]])
    ini2 = ini[:1] + [x + nadd for x in ini[1:]]
    h[0] = len(Mn); h[1] = nv0 + len(vn); h[5:10] = ini2; h[10] = nn + len(orden)
    d2 = dict(d); d2['h'] = h; d2['mallas'] = Mn.tobytes(); d2['verts'] = d['verts'] + vn.tobytes()
    d2['nodos'] = Nn.tobytes() + nodos_n.tobytes()
    if raw is not None:
        ini_p, li, mapa = raw; nsec = len(ini_p) - 1; nm0 = len(M)
        nodo_de = M['nodo']; jf = {nd: j for j, nd in enumerate(orden)}
        cnt = np.zeros(len(orden), np.int64); vis = np.zeros(nm0, bool); nuevo = bytearray(); ini_n = [0]
        for s in range(nsec):
            x = _decodifica_lista(li[ini_p[s]:ini_p[s + 1]]); antes = cnt > 0
            for k in x:
                vis[k] = not vis[k]; nd = int(nodo_de[k])
                if nd in jf: cnt[jf[nd]] += 1 if vis[k] else -1
            cambia = np.nonzero((cnt > 0) != antes)[0]
            y = [k if k < ins else k + nadd for k in x]
            for jj in cambia: y += [ins + mm for mm in meshes_de[orden[int(jj)]]]
            nuevo += pvs.codifica(sorted(y)); ini_n.append(len(nuevo))
        pvs.escribe(d2, fn, ini_n, nuevo, mapa)
    else: pvs.escribe(d2, fn, [0], [], [])
    if informe:
        print('LODGRANDE %s: %d nodos grandes con version lejana lowpoly, triangulos %d -> %d (%.1f %%), %d tiras, %d vertices'
              % (os.path.basename(fn), len(orden), n_in, n_out, 100.0 * n_out / max(1, n_in), nadd, len(vn)))
    return dict(nodos=len(orden), tri0=n_in, tri=n_out)

def aplica_bytes(b, informe=True):
    """sobre los bytes de un ORD7 (grupos_nodos.datos); los mismos si no cambia"""
    import tempfile
    if b[:4] != b'ORD7': return b
    fd, fn = tempfile.mkstemp(suffix='.DAT', dir='/dev/shm' if os.path.isdir('/dev/shm') else None); os.close(fd)
    try:
        open(fn, 'wb').write(b); aplica(fn, informe); return open(fn, 'rb').read()
    finally: os.remove(fn)

def main():
    a = sys.argv[1:]
    if a and a[0] == '--datos':
        import grupos_nodos
        os.makedirs(a[2], exist_ok=True); grupos_nodos.datos(a[1], a[2], aplica_bytes)
    elif a:
        for f in a: aplica(f)
    else: raise SystemExit(__doc__)

if __name__ == '__main__':
    main()
