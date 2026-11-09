#!/usr/bin/env python3
"""pvs.py fichero.DAT [ruta_extra.DAT ...]: visibilidad precalculada (PVS) de los nodos de una etapa o union por
secciones de la ruta, anadida al fichero ORD7 (lo reescribe). La llaman conv_tramo.py y conv_union.py tras escribir
cada SEGnn.DAT / BKkk.DAT; tambien se puede pasar a ficheros ya convertidos (no hace falta el disco).

Metodo (tools/c/pvs.c): desde muestras del ojo en toda la zona por la que puede ir la camara (cada punto de la ruta,
cada 4 m; a lo ancho del limite transitable mas 3 m a cada lado, cada 4 m como mucho; a 0,6, 3,3 y 6 m sobre el suelo
del perfil) se dibujan por software, con z-buffer, cuatro caras alrededor (todos los rumbos; cabeceo de -45 a +60
grados). Tapan solo los triangulos opacos (lista 0) de los nodos que el LOD del original dibuja seguro desde ahi
(distancia al centro <= r 2^(umbral - 1) fx / 240 con la fov mas abierta, FOV_MAX) y que no son de una rama de la union
ni carteles; el follaje (recorte, lista 1) y lo transparente no tapan. El z-buffer se erosiona lo que puede moverse un
borde visto desde cualquier punto a EPS metros de la muestra (eps / z radianes, mas un pixel), y cada nodo se ve si
algun pixel que toca (rasterizado conservador) no queda detras. Es conservador: puede quedar algo que no se ve, pero
no se quita nada que se pueda ver desde la zona muestreada.

Secciones: SEC_PUNTOS puntos de la ruta (8 m). La muestra va a la seccion del punto de la ruta del fichero mas
cercano a ella y a las dos vecinas (la demo elige la seccion por el punto mas cercano al ojo). Las SEC_LLENAS primeras y
ultimas secciones llevan todos los nodos (el ojo puede estar fuera de la ruta, en la etapa de antes o la de despues).

Formato (ORD7 ampliado): nsecciones con el bit 31 = listas XOR: la lista de cada seccion son los nodos que cambian
respecto a la anterior (la primera: los visibles de la seccion 0); la demo lleva la mascara de la seccion en la que esta
y la mueve aplicando las listas (adelante o atras: XOR). Mismo sitio y forma que la tabla de nodos por secciones de
antes (inicio de cada lista, listas u16, seccion de cada punto de la ruta u16)."""
import ctypes, os, struct, subprocess, sys, time
import numpy as np

SEC_PUNTOS = 2                                                  # puntos de la ruta por seccion (4 m cada uno)
SEC_LLENAS = 2                                                  # secciones de cada punta con todos los nodos
LADO_EXTRA = 3.0                                                # metros fuera del limite transitable
PASO_LADO = 4.0
ALTURAS = (0.6, 3.3, 6.0)                                       # sobre el suelo del perfil (la demo: ALTO_PVS)
EPS = float(os.environ.get("PVS_EPS", "3.2"))                   # >= distancia de cualquier punto a su muestra (m)
K = float(os.environ.get("PVS_K", "128"))                       # pixeles por unidad de tangente
FOV_MAX = np.radians(100.0)                                     # fov horizontal mas abierta para el LOD de los oclusores
LEJOS = 4000.0
NPERF, NCARRIL, NEXT = 16, 6, 3
C_LIM = 5 + NPERF + NCARRIL; C_EXT = C_LIM + 2
XOR = 0x80000000

_lib = None
def lib():
    global _lib
    if _lib is None:
        d = os.path.dirname(os.path.abspath(__file__)); src = os.path.join(d, 'c', 'pvs.c')
        so = os.path.join(os.environ.get('TMPDIR', '/tmp'), 'or2_pvs_%d.so' % os.getuid())
        if not os.path.exists(so) or os.path.getmtime(so) < os.path.getmtime(src):
            subprocess.check_call(['cc', '-O3', '-march=native', '-shared', '-fPIC', '-o', so, src, '-lpthread', '-lm'])
        _lib = ctypes.CDLL(so)
    return _lib

MALLA = np.dtype([('c', '<f4', 3), ('r', '<f4'), ('v0', '<u4'), ('n', '<u4'), ('t', '<i4'), ('nodo', '<i4')])
VERT = np.dtype([('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('u', '<f4'), ('v', '<f4'), ('c', '<u4')])
NODO = np.dtype([('c', '<f4', 3), ('r', '<f4'), ('fl', '<u4'), ('cartel', '<i4'), ('rel', '<u4', 2)])

def lee(fn):
    """las partes de un ORD7: dict con cabecera, bytes de cada parte y arrays"""
    b = open(fn, 'rb').read()
    assert b[:4] == b'ORD7', fn
    h = list(struct.unpack_from('<13I', b, 4))
    nm, nv, nr, pr, nt = h[:5]; nn, nc, ns = h[10:13]; ns &= ~XOR
    p = 56; d = dict(h=h, fn=fn)
    def toma(n):
        nonlocal p; x = b[p:p + n]; p += n; return x
    d['texs'] = toma(24 * nt); d['mallas'] = toma(32 * nm); d['verts'] = toma(24 * nv); d['ruta'] = toma(4 * nr * pr)
    d['nodos'] = toma(32 * nn); d['carteles'] = toma(64 * nc)
    if ns:
        ini = np.frombuffer(b, '<u4', ns + 1, p); p += 4 * (ns + 1)
        p += 2 * int(ini[-1]) + 2 * nr
    p += -p % 4; p += -p % 32
    d['blob'] = b[p:]
    d['M'] = np.frombuffer(d['mallas'], MALLA); d['V'] = np.frombuffer(d['verts'], VERT)
    d['R'] = np.frombuffer(d['ruta'], '<f4').reshape(nr, pr) if nr else np.zeros((0, pr or 55), np.float32)
    d['N'] = np.frombuffer(d['nodos'], NODO)
    return d

def escribe(d, fn, ini, listas, mapa):
    """reescribe el ORD7 con la tabla de secciones XOR"""
    h = list(d['h']); h[12] = (len(ini) - 1) | XOR if len(ini) > 1 else 0
    with open(fn, 'wb') as f:
        f.write(b'ORD7' + struct.pack('<13I', *h))
        for k in ('texs', 'mallas', 'verts', 'ruta', 'nodos', 'carteles'): f.write(d[k])
        if len(ini) > 1:
            f.write(np.asarray(ini, '<u4').tobytes()); f.write(np.asarray(listas, '<u2').tobytes())
            f.write(np.asarray(mapa, '<u2').tobytes())
        f.write(b'\0' * (-f.tell() % 4)); f.write(b'\0' * (-f.tell() % 32)); f.write(d['blob'])

def altura_punto(a, x):
    """como altura_punto de demo/main.c: el perfil de -izq-2 a der+2 y fuera las alturas exteriores hasta el limite"""
    g = (x + a[3] + 2) / (a[3] + a[4] + 4) * (NPERF - 1)
    if len(a) >= C_EXT + 2 * NEXT and (g < 0 or g > NPERF - 1):
        lado = g < 0; x0 = -a[3] - 2 if lado else a[4] + 2; x1 = -a[C_LIM] if lado else a[C_LIM + 1]
        hh = a[C_EXT + (0 if lado else NEXT):][:NEXT]; h0 = a[5] if lado else a[5 + NPERF - 1]
        if abs(x1) <= abs(x0) + 0.01: return h0
        t = (x - x0) / (x1 - x0) * NEXT
        if t <= 0: return h0
        if t >= NEXT: return hh[NEXT - 1]
        j = int(t); t -= j; a0 = hh[j - 1] if j else h0
        return a0 + t * (hh[j] - a0)
    g = max(0.0, min(NPERF - 1.001, g)); j = int(g); g -= j
    return a[5 + j] + g * (a[6 + j] - a[5 + j])

def muestras(R):
    """posiciones del ojo a lo largo de una ruta (ver arriba)"""
    out = []; n = len(R)
    for k in range(n):
        a = R[k].astype(float); p = R[max(k - 1, 0)]; q = R[min(k + 1, n - 1)]
        t = np.array([q[0] - p[0], 0.0, q[2] - p[2]]); l = np.linalg.norm(t)
        if l < 1e-6: continue
        t /= l; der = np.array([-t[2], 0.0, t[0]])
        li, ld = (a[C_LIM], a[C_LIM + 1]) if len(a) > C_LIM + 1 else (a[3], a[4])
        x0, x1 = -li - LADO_EXTRA, ld + LADO_EXTRA; m = int(np.ceil((x1 - x0) / PASO_LADO)) + 1
        for x in np.linspace(x0, x1, m):
            g = altura_punto(a, x)
            for hh in ALTURAS:
                out.append((a[0] + x * der[0], g + hh, a[2] + x * der[2]))
    return np.array(out, np.float32)

def calcula(d, extras=(), hilos=os.cpu_count() or 4, informe=True):
    """las listas XOR por seccion del modelo d (lee), por malla (cada tira del fichero; las de los nodos del original).
    extras: rutas (arrays) de mas por las que puede ir el ojo (la rama derecha de la bifurcacion). Devuelve (ini,
    listas, mapa, estadisticas)"""
    t0 = time.time()
    M, V, N, R = d['M'], d['V'], d['N'], d['R']; h = d['h']; nn = len(N); nm = len(M); ini_l = h[5:10]
    if nn == 0 or nm == 0 or len(R) < 2 * SEC_PUNTOS: return [0], [], [], None
    assert nm < 65536
    P = np.stack([V['x'], V['y'], V['z']], 1).astype(np.float32)
    g = M['nodo'].astype(int); n = M['n'].astype(int)
    util = (g >= 0) & (g < nn) & (n >= 3)                         # mallas de nodos (las demas: cielo, se dibujan)
    gg = np.where(util, g, 0)
    cartel = util & (N['cartel'][gg] >= 0); rama = np.where(util, (N['fl'][gg] >> 8) & 3, 0)
    opaco = (np.arange(nm) >= ini_l[0]) & (np.arange(nm) < ini_l[1])
    # triangulos de cada malla (tira: v, v + 1, v + 2), las de los carteles sin triangulos (se prueban por la esfera)
    nt = np.where(util & ~cartel, n - 2, 0); t0n = np.concatenate([[0], np.cumsum(nt)]).astype(np.int32)
    mi = np.repeat(np.arange(nm), nt); i = M['v0'].astype(np.int64)[mi] + (np.arange(t0n[-1]) - t0n[:-1][mi])
    T = np.ascontiguousarray(np.stack([P[i], P[i + 1], P[i + 2]], 1).reshape(-1, 9) if len(i) else np.zeros((1, 9)), np.float32)
    O = np.ascontiguousarray(opaco[mi].astype(np.uint8) if len(i) else np.zeros(1, np.uint8))
    esf = np.concatenate([M['c'], M['r'][:, None]], 1).astype(np.float32)
    esf[cartel] = np.concatenate([N['c'], N['r'][:, None]], 1)[gg[cartel]]   # sus vertices van en el marco del nodo
    esf = np.ascontiguousarray(esf)
    fl = np.ascontiguousarray((cartel.astype(np.uint8) | ((opaco & util & ~cartel & (rama == 0)).astype(np.uint8) << 1)
                               | ((~util).astype(np.uint8) << 2)))
    u = (N['fl'] & 15).astype(int); lod = (320.0 / np.tan(FOV_MAX / 2)) / 240.0
    occ_n = np.where(u > 0, N['r'] * lod * np.exp2(np.maximum(u, 1) - 1), LEJOS)
    occ_n = np.minimum(occ_n, LEJOS - N['r']).clip(0)              # distancia al centro del nodo
    # la malla tapa si su nodo se dibuja seguro: distancia al centro de la malla + la de los centros
    dcn = np.linalg.norm(esf[:, :3] - N['c'][gg], axis=1)
    occ = np.ascontiguousarray(np.where(util, (occ_n[gg] - dcn).clip(0), 0), np.float32)
    pos = [muestras(R)] + [muestras(x) for x in extras]; pos = np.ascontiguousarray(np.concatenate(pos), np.float32)
    nw = (nm + 31) // 32; sal = np.zeros((len(pos), nw), np.uint32)
    L = lib(); fp = ctypes.POINTER(ctypes.c_float)
    L.pvs.argtypes = [fp, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, fp, ctypes.c_void_p, fp, fp, ctypes.c_int,
                      ctypes.c_float, ctypes.c_float, ctypes.c_int, ctypes.c_void_p]
    c = lambda a: a.ctypes.data_as(fp)
    L.pvs(c(T), O.ctypes.data, t0n.ctypes.data, nm, c(esf), fl.ctypes.data, c(occ), c(pos), len(pos), K, EPS, hilos,
          sal.ctypes.data)
    vis = np.unpackbits(sal.view(np.uint8), axis=1, bitorder='little')[:, :nm].astype(bool)
    # la seccion de cada muestra: la del punto de la ruta mas cercano (como la demo con el ojo) y sus vecinas
    from scipy.spatial import cKDTree
    nsec = (len(R) + SEC_PUNTOS - 1) // SEC_PUNTOS
    _, kp = cKDTree(R[:, :3].astype(float)).query(pos.astype(float))
    sec = np.zeros((nsec, nm), bool)
    for s, v in zip(kp // SEC_PUNTOS, vis):
        sec[max(s - 1, 0):s + 2] |= v
    sec[:SEC_LLENAS] = util; sec[-SEC_LLENAS:] = util
    sec &= util
    prev = np.zeros(nm, bool); ini = [0]; listas = []
    for s in range(nsec):
        x = np.nonzero(sec[s] ^ prev)[0]; listas += x.tolist(); ini.append(len(listas)); prev = sec[s]
    mapa = np.arange(len(R)) // SEC_PUNTOS
    vn = np.where(util, n, 0).astype(float)
    mid = sec[SEC_LLENAS:-SEC_LLENAS] if nsec > 2 * SEC_LLENAS else sec
    est = dict(sec=sec, vn=vn, util=util, muestras=len(pos), mallas=int(util.sum()), secciones=nsec, xor=len(listas),
               seg=time.time() - t0, frac_mallas=float(mid.sum(1).mean() / max(1, util.sum())),
               frac_vtx=float((mid @ vn).mean() / max(1, vn.sum())))
    if informe:
        print('PVS %s: %d muestras, %d mallas de nodos, %d secciones, %d entradas XOR (%d KB); quedan de media %.0f %% '
              'de las mallas y %.0f %% de los vertices; %.0f s' % (os.path.basename(d['fn']), est['muestras'],
              est['mallas'], nsec, len(listas), (4 * (nsec + 1) + 2 * len(listas) + 2 * len(R)) >> 10,
              100 * est['frac_mallas'], 100 * est['frac_vtx'], est['seg']))
    return ini, listas, mapa, est

def anade(fn, extras=()):
    """calcula el PVS del fichero fn y lo reescribe con el. extras: ficheros ORD7 con rutas de mas (BKD.DAT)"""
    d = lee(fn)
    ex = [lee(x)['R'] for x in extras]
    ini, listas, mapa, est = calcula(d, ex)
    escribe(d, fn, ini, listas, mapa)
    return est

def main():
    """pvs.py fichero.DAT [--ruta otra.DAT]...: con --ruta, rutas de mas por las que puede ir el ojo"""
    args = sys.argv[1:]; ex = []; fs = []
    while args:
        a = args.pop(0)
        if a == '--ruta': ex.append(args.pop(0))
        else: fs.append(a)
    for f in fs: anade(f, ex)

if __name__ == '__main__':
    main()
