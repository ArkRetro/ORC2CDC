#!/usr/bin/env python3
"""lod_veg.py fichero.DAT... | --datos datos/ trabajo/ [ficheros]: LOD lejano de la vegetacion de recorte de las etapas y
uniones (ORD7). DESVIACION pedida por el usuario (revision 46, docs/PARIDAD.md: Palmeras lejanas): "No quiero recortes;
en tal caso, las palmeras mas lejanas que se vean pero en version low-poly, y cuando te acerques que se vean bien". El
original no tiene LOD de la vegetacion (los nodos de PS2 solo traen la malla 0: docs/PLAN.md, Popping) y la Dreamcast
no puede dibujar todas las palmeras que dibuja el original (el reparto del TA las quitaba de lejos: popping). Aqui cada
nodo de vegetacion caro tiene ademas una version lejana que la demo dibuja cuando el nodo se ve pequeno en la pantalla
(demo/main.c nodos_visibles, LOD_LEJ_PX):
 - vegetacion: nodos con mas de la mitad de sus vertices en la lista de recorte (la medida POPVEG de DIAG_POPPING: hojas
   con el alfa de recorte de su textura), que no son carteles y cuyo coste teorico del TA es de al menos MIN_TEO bytes;
 - la version lejana es un impostor: un cuadrilatero (una tira de 4 vertices, 108 bytes teoricos del TA frente a los
   ~7300 de una palmera de PALM) orientado a la camara alrededor de la vertical (como los carteles del original,
   face_camera de 404D10: carga_cartel), con una textura que es la imagen del propio nodo dibujada aqui por software
   desde su geometria, sus texturas y el color de sus vertices (la luz del original ya cocida) tal y como estan en el
   fichero: nada de arte nuevo. Vista ortografica desde la direccion por la que llega la camara (la ruta del fichero
   hacia atras desde el punto mas cercano: la carrera va del principio al final de la ruta), con 4 x 4 muestras por
   texel, el alfa de recorte de la PowerVR (>= 0,5) y el color de los texeles transparentes rellenado con el de sus
   vecinos (el filtro bilineal no oscurece el borde);
 - los nodos que son el mismo modelo (las mismas tiras con otra matriz: misma firma de cuentas, texturas y UV) comparten
   imagen: la del primero vista desde la direccion de llegada de cada uno llevada a su marco (ajuste afin de sus
   vertices) y redondeada a BINS rumbos; el cuadrilatero de cada uno va a su propia caja vista desde su direccion;
 - las imagenes van en atlas cuadrados (hasta ATLAS texeles de lado) en VQ ARGB1555 (lista de recorte), con la z de
   las texturas de la escena (bit 0x800).
En el fichero: las texturas nuevas al final, las mallas del impostor al final de la lista de recorte (vertices al
final), un nodo nuevo por impostor al final de la tabla de nodos (esfera del nodo completo con el radio que cubre el
cuadrilatero girado, banderas del completo con 0x10 de cartel y NODO_LEJ, su cartel: la matriz con la traslacion al
centro, sin giro, y en la columna 3 la direccion de la vista con la que se hizo la imagen; relleno[1] = el nodo completo)
y en el nodo completo cartel = -2 - (su nodo lejano). La PVS (tools/pvs.py) se rehace con los indices nuevos: la malla
del impostor se ve en una seccion si alguna del nodo completo se ve. Lo aplican conv_tramo.convierte y conv_union.py
despues de la PVS; en una carpeta de datos ya hecha, --datos (tools/regenera.py --interno lodveg). Idempotente: un
fichero que ya tiene nodos lejanos no se toca."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pvs, vq

NODO_CARTEL = 0x10
NODO_LEJ = 0x20                                                 # banderas del nodo: version lejana de otro
MIN_TEO = 1500                                                  # bytes teoricos del TA del nodo (1500: ~14 impostores)
LADO = 62                                                       # texeles del lado mayor de cada imagen (64 con PAD)
SS = 4                                                          # muestras por texel en cada eje
BINS = 8                                                        # rumbos de las imagenes de un mismo modelo
ATLAS = 512                                                     # lado maximo de un atlas
PAD = 1                                                         # texeles de margen alrededor de cada imagen

def teo(n):
    t = n - 2; return 24 * t + 60 * ((t + 5) // 6)

def _pvs_raw(fn):
    """las listas de la PVS del ORD7 (ini, bytes de las listas, mapa) o None"""
    b = open(fn, 'rb').read(); h = struct.unpack_from('<13I', b, 4)
    nm, nv, nr, pr, nt = h[:5]; nn, nc, ns = h[10:13]
    if not ns & pvs.XOR: return None
    ns &= ~pvs.XOR
    p = 56 + 24 * nt + 32 * nm + 24 * nv + 4 * nr * pr + 32 * nn + 64 * nc
    ini = np.frombuffer(b, '<u4', ns + 1, p).astype(np.int64); p += 4 * (ns + 1)
    li = b[p:p + int(ini[-1])]; p += int(ini[-1]); p += p % 2
    mapa = np.frombuffer(b, '<u2', nr, p).copy()
    return ini, li, mapa

def _decodifica_lista(x):
    out = []; a = -1; i = 0
    while i < len(x):
        dd = x[i]; i += 1
        if dd == 255: dd = x[i] | x[i + 1] << 8; i += 2
        a += dd; out.append(a)
    return out

def _textura(d, t, cache):
    if t in cache: return cache[t]
    w, h, fmt, li, off, tam = struct.unpack_from('<6I', d['texs'], 24 * t); b = d['blob'][off:off + tam]
    n = ((fmt >> 16) & 255) + 1
    if fmt & 0x200:                                              # con mipmaps: la base (el ultimo nivel)
        if fmt & 0x100:                                          # VQ: el libro y los indices del nivel de lado w (tras
            o = 2 + sum(4 ** (k - 1) for k in range(2, int(np.log2(w))))   # 1x1 en 0, 2x2 en 1, 4x4 en 2, 8x8 en 6...)
            b = b[:8 * n] + b[8 * n + o:8 * n + o + w * h // 4]
        else: b = b[6 + (sum(4 ** k for k in range(int(np.log2(w)))) * 2):]
    if fmt & 0x100: a = vq.decodifica(b, w, h, fmt & 255, n)
    else: a = vq.decodifica16(b, w, h, fmt & 255)
    cache[t] = (a.astype(np.float32), li, fmt); return cache[t]

def _rumbo(R, c):
    """direccion horizontal (unitaria) desde c hacia donde llega la camara: la ruta hacia atras en su punto mas cercano"""
    if len(R) < 2: return np.array([0.0, 0.0, 1.0])
    k = int(np.argmin(((R[:, [0, 2]] - c[[0, 2]]) ** 2).sum(1)))
    a, b = R[max(k - 2, 0), :3], R[min(k + 2, len(R) - 1), :3]
    t = (b - a).astype(float); t[1] = 0; l = np.linalg.norm(t)
    return -t / l if l > 1e-6 else np.array([0.0, 0.0, 1.0])

def _ejes(D):
    """derecha (vista desde +D mirando a -D, arriba +y) y arriba"""
    return np.array([D[2], 0.0, -D[0]]), np.array([0.0, 1.0, 0.0])

def dibuja(tiras, D, ext, W, H, d, cache):
    """imagen RGBA (H x W, uint8) de las tiras [(P, UV, C, textura)] vistas desde D en la caja ext (u0, u1, v0, v1)"""
    e, up = _ejes(D); u0, u1, v0, v1 = ext
    WS, HS = W * SS, H * SS
    sx = WS / (u1 - u0); sy = HS / (v1 - v0)
    col = np.zeros((HS, WS, 3), np.float32); op = np.zeros((HS, WS), bool); zb = np.full((HS, WS), -np.inf, np.float32)
    for P, UV, C, t in tiras:
        X = ((P @ e) - u0) * sx; Y = (v1 - (P @ up)) * sy; Z = P @ D
        if t >= 0: tx, li, fmt = _textura(d, t, cache)
        else: tx, li, fmt = None, 0, 0
        for i in range(len(P) - 2):
            j = (i, i + 1, i + 2)
            x, y = X[list(j)], Y[list(j)]
            den = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])
            if abs(den) < 1e-9: continue
            xa, xb = int(max(np.floor(x.min()), 0)), int(min(np.ceil(x.max()), WS - 1))
            ya, yb = int(max(np.floor(y.min()), 0)), int(min(np.ceil(y.max()), HS - 1))
            if xa > xb or ya > yb: continue
            gy, gx = np.mgrid[ya:yb + 1, xa:xb + 1]; px = gx + 0.5; py = gy + 0.5
            l1 = ((x[2] - px) * (y[0] - py) - (x[0] - px) * (y[2] - py)) / den
            l2 = ((x[0] - px) * (y[1] - py) - (x[1] - px) * (y[0] - py)) / den
            l0 = 1 - l1 - l2
            m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
            if not m.any(): continue
            z = l0 * Z[j[0]] + l1 * Z[j[1]] + l2 * Z[j[2]]
            m &= z > zb[ya:yb + 1, xa:xb + 1]
            if not m.any(): continue
            cc = l0[..., None] * C[j[0]] + l1[..., None] * C[j[1]] + l2[..., None] * C[j[2]]
            if tx is not None:
                uv = l0[..., None] * UV[j[0]] + l1[..., None] * UV[j[1]] + l2[..., None] * UV[j[2]]
                th, tw = tx.shape[:2]
                iu = np.floor(uv[..., 0] * tw).astype(int) % tw; iv = np.floor(uv[..., 1] * th).astype(int) % th
                tc = tx[iv, iu]
                if li == 1: m &= tc[..., 3] >= 128
                elif li == 2: m &= tc[..., 3] * cc[..., 3] / 255 >= 128
                rgb = tc[..., :3] * cc[..., :3] / 255
            else: rgb = cc[..., :3]
            sub = (slice(ya, yb + 1), slice(xa, xb + 1))
            col[sub][m] = rgb[m]; op[sub][m] = True; zb[sub][m] = z[m]
    # a texeles: cobertura >= 0,5 opaca (el recorte de la PowerVR), color medio de las muestras cubiertas
    o = op.reshape(H, SS, W, SS).mean((1, 3))
    s = (col * op[..., None]).reshape(H, SS, W, SS, 3).sum((1, 3))
    n = op.reshape(H, SS, W, SS).sum((1, 3))
    rgb = s / np.maximum(n, 1)[..., None]; tiene = n > 0
    for _ in range(8):                                           # color de los vecinos en lo transparente (bilineal)
        if tiene.all(): break
        acc = np.zeros_like(rgb); cnt = np.zeros(rgb.shape[:2])
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            r = np.roll(np.roll(rgb * tiene[..., None], dy, 0), dx, 1); c = np.roll(np.roll(tiene, dy, 0), dx, 1)
            acc += r; cnt += c
        nuevo = ~tiene & (cnt > 0); rgb[nuevo] = acc[nuevo] / cnt[nuevo][:, None]; tiene = tiene | nuevo
    a = np.zeros((H, W, 4), np.uint8)
    a[..., :3] = np.clip(np.rint(rgb), 0, 255); a[..., 3] = np.where(o >= 0.5, 255, 0)
    return a

def _caja(P, D):
    e, up = _ejes(D); u = P @ e; v = P @ up; a, b, c, d = float(u.min()), float(u.max()), float(v.min()), float(v.max())
    if b - a < 0.05: a, b = (a + b) / 2 - 0.025, (a + b) / 2 + 0.025      # (de canto: un plano visto de lado)
    if d - c < 0.05: c, d = (c + d) / 2 - 0.025, (c + d) / 2 + 0.025
    return a, b, c, d

def _tam(ext):
    """texeles (ancho, alto) de una imagen: LADO el lado mayor, el otro proporcional (con PAD, multiplos de 8)"""
    du, dv = max(ext[1] - ext[0], 1e-3), max(ext[3] - ext[2], 1e-3)
    otro = lambda f: int(min(LADO, max(6, -(-(LADO * f + 2 * PAD) // 8) * 8 - 2 * PAD)))
    return (LADO, otro(dv / du)) if du >= dv else (otro(du / dv), LADO)

def _p2(x):
    L = 8
    while L < x: L *= 2
    return L

def _empaqueta(tams):
    """posicion (atlas, x, y) de cada imagen (ancho, alto, mas PAD alrededor) en estantes y (ancho, alto) de cada
    atlas (potencias de 2 hasta ATLAS; la PowerVR admite las VQ rectangulares): el ancho que deja menos texeles"""
    orden = sorted(range(len(tams)), key=lambda i: (-tams[i][1], -tams[i][0], i))
    mejor = None
    for A in (64, 128, 256, 512):
        if A > ATLAS: break
        if any(w + 2 * PAD > A for w, h in tams): continue
        pos = [None] * len(tams); atlas = []
        def nuevo(): atlas.append(dict(x=0, y=0, fila=0, ancho=0)); return len(atlas) - 1
        a = nuevo()
        for i in orden:
            w, h = tams[i][0] + 2 * PAD, tams[i][1] + 2 * PAD; s = atlas[a]
            if s['x'] + w > A: s['x'] = 0; s['y'] += s['fila']; s['fila'] = 0
            if s['y'] + h > ATLAS: a = nuevo(); s = atlas[a]
            pos[i] = (a, s['x'] + PAD, s['y'] + PAD); s['x'] += w; s['fila'] = max(s['fila'], h); s['ancho'] = max(s['ancho'], s['x'])
        dims = [(_p2(s['ancho']), _p2(s['y'] + s['fila'])) for s in atlas]
        area = sum(w * h + 8192 for w, h in dims)              # (el libro de cada atlas: 2 KB)
        if mejor is None or area < mejor[0]: mejor = (area, pos, dims)
    return mejor[1], mejor[2]

def candidatos(d):
    """nodos de vegetacion con version lejana: {nodo: [mallas]} (ver arriba)"""
    M, N = d['M'], d['N']; h = d['h']; ini = h[5:10]; nn = len(N)
    vt = np.zeros(nn); vr = np.zeros(nn); tb = np.zeros(nn); ms = {}
    for k in range(ini[3]):
        x = M[k]; nd = int(x['nodo'])
        if not 0 <= nd < nn or x['n'] < 3: continue
        vt[nd] += x['n']; tb[nd] += teo(min(int(x['n']), 256)); ms.setdefault(nd, []).append(k)
        if ini[1] <= k < ini[2]: vr[nd] += x['n']
    ok = (vt > 0) & (2 * vr > vt) & (tb >= MIN_TEO) & (N['cartel'] == -1) & ((N['fl'] & NODO_LEJ) == 0)
    return {int(i): ms[int(i)] for i in np.nonzero(ok)[0]}

def planta(T, c, d, cache):
    """si el modelo de las tiras T (centro c) va como impostor: una planta o algo de su forma, que desde cualquier
    rumbo se ve parecido (el cuadrilatero gira con la camara): en planta no alargado (desviacion menor / mayor de sus
    vertices en x z >= PLANTA_RAT: no barandillas, vallas, setos, carteles ni macizos de flores), alto (altura >=
    PLANTA_ALTO del lado mayor en planta: no lo tumbado), de silueta abierta (texeles opacos de su imagen entre
    PLANTA_LLENO_MIN y PLANTA_LLENO: no fachadas ni cajas), de muchas piezas (>= PLANTA_PIEZAS tiras: hojas o ramas) y
    no azulado (azul <= 1,05 veces el mayor de rojo y verde de media: no cristales ni fachadas). Medido en las 62 etapas
    (docs/PARIDAD.md): palmeras, arboles, arbustos y pinos; tambien unas farolas, la cabina del teleferico de las
    misiones y ruinas pequenas, que se ven igual de cualquier lado"""
    P = np.concatenate([x[0] for x in T]) - c
    q = P[:, [0, 2]] - P[:, [0, 2]].mean(0); w, v = np.linalg.eigh(q.T @ q / len(q)); ev = np.sqrt(np.maximum(w, 0))
    pr = q @ v; lado = float((pr.max(0) - pr.min(0)).max())
    if ev[0] < PLANTA_RAT * max(ev[1], 1e-6) or P[:, 1].max() - P[:, 1].min() < PLANTA_ALTO * lado or len(T) < PLANTA_PIEZAS:
        return False
    D = np.array([0.0, 0.0, 1.0]); ext = _caja(P, D); W, H = _tam(ext)
    a = dibuja([(x[0] - c,) + tuple(x[1:]) for x in T], D, ext, W, H, d, cache)
    op = a[..., 3] > 0; lleno = float(op.mean())
    if not PLANTA_LLENO_MIN <= lleno <= PLANTA_LLENO: return False
    m = a[..., :3][op].mean(0)
    return bool(m[2] <= 1.05 * max(m[0], m[1]))

PLANTA_RAT = 0.35
PLANTA_ALTO = 0.5
PLANTA_LLENO_MIN, PLANTA_LLENO = 0.02, 0.55
PLANTA_PIEZAS = 12

def aplica(fn, informe=True):
    d = pvs.lee(fn)
    N, M, V = d['N'], d['M'], d['V']; h = list(d['h']); nn = len(N)
    if nn and ((N['fl'] & NODO_LEJ).any() or (N['cartel'] <= -2).any()): return None   # ya hecho
    cand = candidatos(d)
    if not cand: return None
    raw = _pvs_raw(fn)
    R = d['R'][:, :3].astype(float) if len(d['R']) else np.zeros((0, 3))
    def tiras_de(nd):
        res = []
        for k in cand[nd]:
            x = M[k]; vv = V[int(x['v0']):int(x['v0']) + int(x['n'])]
            P = np.stack([vv['x'], vv['y'], vv['z']], 1).astype(float)
            UV = np.stack([vv['u'], vv['v']], 1).astype(float)
            c = vv['c'].astype(np.uint32)
            C = np.stack([(c >> 16) & 255, (c >> 8) & 255, c & 255, c >> 24], 1).astype(np.float32)
            res.append((P, UV, C, int(x['t'])))
        return res
    # modelos repetidos: firma (cuentas, texturas y UV de sus mallas en orden)
    firmas = {}; rep = {}; A = {}
    for nd in sorted(cand):
        T = tiras_de(nd)
        f = tuple((len(P), t, np.round(UV, 3).tobytes()) for P, UV, C, t in T)
        if f in firmas:
            r = firmas[f]; P0 = np.concatenate([x[0] for x in tiras_de(r)]); P1 = np.concatenate([x[0] for x in T])
            X = np.c_[P0 - P0.mean(0), np.zeros(len(P0))][:, :3]
            Am, *_ = np.linalg.lstsq(X, P1 - P1.mean(0), rcond=None)   # P1 - m1 = (P0 - m0) Am (vectores fila)
            err = np.abs((P0 - P0.mean(0)) @ Am - (P1 - P1.mean(0))).max()
            if err < 0.05 * max(1.0, float(np.abs(P1 - P1.mean(0)).max())) and abs(np.linalg.det(Am)) > 1e-6:
                rep[nd] = r; A[nd] = Am; continue
        firmas.setdefault(f, nd); rep[nd] = nd; A[nd] = np.eye(3)
    # solo lo que se puede ver igual desde cualquier lado (forma, ver planta)
    cache = {}; buenos = {r: planta(tiras_de(r), N[r]['c'].astype(float), d, cache) for r in set(rep.values())}
    cand = {nd: ks for nd, ks in cand.items() if buenos[rep[nd]]}
    if not cand: return None
    # vista de cada nodo: su rumbo de llegada; en el marco de su modelo, redondeado a BINS rumbos
    vista = {}; imagenes = {}                                   # (modelo, rumbo) -> ext, tam
    for nd in sorted(cand):
        c = N[nd]['c'].astype(float); D = _rumbo(R, c); r = rep[nd]
        Dm = A[nd] @ D                                          # (P1 . D = P0 . (Am D))
        Dm[1] = 0; Dm /= max(np.linalg.norm(Dm), 1e-9)
        b = int(np.round(np.arctan2(Dm[0], Dm[2]) / (2 * np.pi / BINS))) % BINS
        vista[nd] = (D, (r, b))
    for nd in sorted(cand):
        clave = vista[nd][1]
        if clave in imagenes: continue
        r, b = clave; ang = b * 2 * np.pi / BINS; Db = np.array([np.sin(ang), 0.0, np.cos(ang)])
        P = np.concatenate([x[0] for x in tiras_de(r)]); ext = _caja(P - N[r]['c'].astype(float), Db)
        imagenes[clave] = dict(D=Db, ext=ext, tam=_tam(ext))
    claves = sorted(imagenes)
    pos, dims = _empaqueta([imagenes[k]['tam'] for k in claves])
    atl = [np.zeros((h_, w_, 4), np.uint8) for w_, h_ in dims]
    for i, k in enumerate(claves):
        im = imagenes[k]; r = k[0]; c = N[r]['c'].astype(float)
        T = [(P - c, UV, C, t) for P, UV, C, t in tiras_de(r)]
        W_, H_ = im['tam']; a = dibuja(T, im['D'], im['ext'], W_, H_, d, cache)
        ai, x0, y0 = pos[i]; im['pos'] = (ai, x0, y0)
        bl = atl[ai]; bl[y0:y0 + H_, x0:x0 + W_] = a
        # margen: la fila / columna del borde repetida (con su alfa: el filtro de la PowerVR la toca)
        bl[y0 - PAD:y0, x0:x0 + W_] = a[:1]; bl[y0 + H_:y0 + H_ + PAD, x0:x0 + W_] = a[-1:]
        bl[y0 - PAD:y0 + H_ + PAD, x0 - PAD:x0] = bl[y0 - PAD:y0 + H_ + PAD, x0:x0 + 1]
        bl[y0 - PAD:y0 + H_ + PAD, x0 + W_:x0 + W_ + PAD] = bl[y0 - PAD:y0 + H_ + PAD, x0 + W_ - 1:x0 + W_]
    # texturas nuevas (VQ ARGB1555, lista de recorte, z de la escena)
    nt0 = len(d['texs']) // 24; texs = bytearray(d['texs']); blob = bytearray(d['blob']); blob += b'\0' * (-len(blob) % 32)
    errs = []
    for (Wa, Ha), a in zip(dims, atl):
        bloques = Wa * Ha // 4; n = 256 if bloques >= 1024 else max(16, (bloques // 4 + 3) & ~3)
        datos, err = vq.codifica(a, 0, n, tiempo=1e9); errs.append(err)
        fmt = 0 | 0x100 | ((n - 1) << 16) | 0x800
        texs += struct.pack('<6I', Wa, Ha, fmt, 1, len(blob), len(datos)); blob += datos + b'\0' * (-len(datos) % 32)
    # mallas, vertices, nodos y carteles nuevos
    ini = list(h[5:10]); nm0 = len(M); nf = len(cand); ins = ini[2]
    VERT = pvs.VERT; nv0 = len(V); vn = np.zeros(4 * nf, VERT); mn = np.zeros(nf, pvs.MALLA)
    Nn = N.copy(); nodos_n = np.zeros(nf, pvs.NODO); cart = []; nc0 = h[11]
    orden = sorted(cand, key=lambda nd: (imagenes[vista[nd][1]]['pos'][0], nd))   # por textura y nodo
    for j, nd in enumerate(orden):
        D, clave = vista[nd]; im = imagenes[clave]; ai, x0, y0 = im['pos']; Wa, Ha = dims[ai]; W_, H_ = im['tam']
        c = N[nd]['c'].astype(float)
        P = np.concatenate([x[0] for x in tiras_de(nd)]) - c
        u0, u1, v0, v1 = _caja(P, D)
        # la imagen guarda la proporcion del modelo: el cuadrilatero lleva la de la imagen en la caja del nodo
        e, up = _ejes(D); q = np.array([[u0, v0], [u0, v1], [u1, v0], [u1, v1]])
        uv = np.array([[x0, y0 + H_], [x0, y0], [x0 + W_, y0 + H_], [x0 + W_, y0]], float) / [Wa, Ha]
        k = 4 * j
        for i in range(4):
            p = q[i, 0] * e + q[i, 1] * up                     # en el marco del nodo (sin giro: ejes del mundo)
            vn[k + i]['x'], vn[k + i]['y'], vn[k + i]['z'] = p; vn[k + i]['u'], vn[k + i]['v'] = uv[i]; vn[k + i]['c'] = 0xFFFFFFFF
        rr = float(np.sqrt((q ** 2).sum(1).max()))
        mn[j]['c'] = (0.0, 0.0, 0.0); mn[j]['r'] = rr;           # (en el marco del nodo, como los carteles)
        mn[j]['v0'] = nv0 + k; mn[j]['n'] = 4; mn[j]['t'] = nt0 + ai; mn[j]['nodo'] = nn + j
        nodos_n[j]['c'] = c; nodos_n[j]['r'] = max(float(N[nd]['r']), rr)
        nodos_n[j]['fl'] = (int(N[nd]['fl']) & 0xFFFF030F) | NODO_CARTEL | NODO_LEJ
        nodos_n[j]['cartel'] = nc0 + j; nodos_n[j]['rel'] = (0, nd)
        Wc = np.eye(4, dtype=np.float32); Wc[3, :3] = c; Wc[0, 3], Wc[1, 3], Wc[2, 3] = D[0], 0.0, D[2]; cart.append(Wc)
        Nn[nd]['cartel'] = -2 - (nn + j)
    Mn = np.concatenate([M[:ins], mn, M[ins:]])
    ini2 = ini[:2] + [x + nf for x in ini[2:]]
    h[0] = len(Mn); h[1] = nv0 + 4 * nf; h[4] = nt0 + len(dims); h[5:10] = ini2; h[10] = nn + nf; h[11] = nc0 + nf
    d2 = dict(d); d2['h'] = h; d2['texs'] = bytes(texs); d2['blob'] = bytes(blob); d2['mallas'] = Mn.tobytes()
    d2['verts'] = d['verts'] + vn.tobytes(); d2['nodos'] = Nn.tobytes() + nodos_n.tobytes()
    d2['carteles'] = d['carteles'] + b''.join(W.astype('<f4').tobytes() for W in cart)
    # PVS: indices nuevos; la malla del impostor, visible si alguna del nodo completo lo es
    if raw is not None:
        ini_p, li, mapa = raw; nsec = len(ini_p) - 1
        nodo_de = M['nodo']; jf = {nd: j for j, nd in enumerate(orden)}
        cnt = np.zeros(nf, np.int64); vis = np.zeros(nm0, bool); nuevo = bytearray(); ini_n = [0]
        for s in range(nsec):
            x = _decodifica_lista(li[ini_p[s]:ini_p[s + 1]]); antes = cnt > 0
            for k in x:
                vis[k] = not vis[k]; nd = int(nodo_de[k])
                if nd in jf: cnt[jf[nd]] += 1 if vis[k] else -1
            cambia = np.nonzero((cnt > 0) != antes)[0]
            y = [k if k < ins else k + nf for k in x] + [ins + int(j) for j in cambia]
            nuevo += pvs.codifica(sorted(y)); ini_n.append(len(nuevo))
        pvs.escribe(d2, fn, ini_n, nuevo, mapa)
    else: pvs.escribe(d2, fn, [0], [], [])
    if informe:
        tf = sum(teo(min(int(M[k]['n']), 256)) for nd in cand for k in cand[nd])
        print('LODVEG %s: %d nodos de vegetacion con version lejana (%d modelos, %d imagenes), TA teorico %d KB -> %d KB '
              '(%.0f veces menos); %d atlas %s, %d KB de VRAM (VQ, error %s)'
              % (os.path.basename(fn), nf, len({rep[x] for x in cand}), len(claves), tf >> 10, (108 * nf) >> 10, tf / max(1, 108 * nf),
                 len(dims), dims, sum(w_ * h_ // 4 + 2048 for w_, h_ in dims) >> 10, ','.join('%.1f' % e for e in errs)))
    return dict(nodos=nf, imagenes=len(claves), atlas=dims)

def aplica_bytes(b, informe=True):
    """aplica sobre los bytes de un ORD7 (grupos_nodos.datos: una carpeta de datos ya hecha); los mismos si no cambia"""
    import tempfile
    if b[:4] != b'ORD7': return b
    fd, fn = tempfile.mkstemp(suffix='.DAT', dir='/dev/shm' if os.path.isdir('/dev/shm') else None); os.close(fd)
    try:
        open(fn, 'wb').write(b); aplica(fn, informe); return open(fn, 'rb').read()
    finally: os.remove(fn)

def main():
    a = sys.argv[1:]
    if a and a[0] == '--datos':
        import grupos_nodos; os.makedirs(a[2], exist_ok=True); return grupos_nodos.datos(a[1], a[2], aplica_bytes)
    for f in a: aplica(f)

if __name__ == '__main__':
    main()
