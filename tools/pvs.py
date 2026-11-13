#!/usr/bin/env python3
"""pvs.py fichero.DAT... [--ruta ruta_extra.DAT] [--quita]: visibilidad precalculada (PVS) de los nodos de una etapa o union por
secciones de la ruta, anadida al fichero ORD7 (lo reescribe). La llaman conv_tramo.py y conv_union.py tras escribir
cada SEGnn.DAT / BKkk.DAT; tambien se puede pasar a ficheros ya convertidos (no hace falta el disco).

Metodo (tools/c/pvs.c): desde muestras del ojo en toda la zona por la que puede ir la camara (cada punto de la ruta,
cada 4 m, y la ruta prolongada FIN_EXTRA m en linea recta por cada punta; a lo ancho del limite transitable mas 3 m a
cada lado, cada 4 m como mucho; a 0,6, 3,3 y 6 m sobre el suelo del perfil) se dibujan por software, con z-buffer,
cuatro caras alrededor (todos los rumbos; cabeceo de -45 a +60 grados). Tapan los triangulos de los nodos que el LOD
del original dibuja seguro desde ahi (distancia al centro <= r 2^(umbral - 1) fx / 240 con la fov mas abierta, FOV_MAX)
y que no son de una rama de la union ni carteles: los opacos (lista 0) enteros y los de recorte (lista 1, prueba de
alfa) solo donde el texel y sus ocho vecinos son opacos (mascaras); lo transparente no tapa. El z-buffer se erosiona
lo que puede moverse un borde visto desde cualquier punto a EPS metros de la muestra (eps / z radianes, mas un pixel),
y cada triangulo se ve si algun pixel que toca (rasterizado conservador) no queda detras; los de area nula no pintan
nada. Es conservador: puede quedar algo que no se ve, pero no se quita nada que se pueda ver desde la zona muestreada.

Secciones: SEC_PUNTOS puntos de la ruta (8 m). La muestra va a la seccion del punto de la ruta del fichero mas
cercano a ella (las de la ruta prolongada, a la de su punta) y a las dos vecinas (la demo elige la seccion por el punto
mas cercano al ojo y no usa la PVS con el ojo fuera de lo muestreado: pvs_paso, PVS_LADO, PVS_ALTO y PVS_FIN). Antes
las dos primeras y las dos ultimas secciones llevaban todos los nodos (el ojo podia estar fuera de la ruta): en la
salida de PALM, con el ojo en la seccion 1, la etapa entera en la vista (71000 vertices; 55000 ahora) y el reparto del
TA quitaba los bajos de los edificios de la calle (los porticos con ventanas de la lista de recorte se quedaban: se
veia el cielo a traves y debajo, edificios palidos).

Va por malla (cada tira del fichero de un nodo del original; el cielo y lo que no es de un nodo se dibuja siempre): una
malla se ve en la seccion si alguno de sus triangulos se ve. Los huecos de HUECO secciones o menos en los que una
malla no se ve se rellenan (se queda visible: menos listas y nunca quita nada); si aun asi las listas pasan de MAX_KB
(la RAM: la etapa mayor fija el hueco de todas), se dobla el hueco hasta que caben.

Lo que no se ve nunca (anade con quita, las etapas: conv_tramo.py): con las muestras de un volumen mas ancho (QUITA_LADO
fuera del limite, alturas QUITA_ALTURAS hasta 12 m, la ruta prolongada QUITA_FIN m; las camaras de la escena de la
salida van a menos de 3 m del coche y 4 m del suelo; las de la meta, en las uniones, que no se tocan) los triangulos
que no se ven desde ninguna salen del fichero: cada tira se parte en los trozos que se ven (un trozo sin ver de
QUITA_MIN triangulos o mas en medio, cualquiera en las puntas), y las texturas que ya no usa nadie tambien. En PALM son
122 de 109980 triangulos (0,1 %; ninguna textura): lo que no se ve desde ningun sitio casi no existe en los datos del
original y lo que se gana esta en las secciones. Como dobla el tiempo (las muestras del volumen ancho y la prueba de
cada triangulo) y quita para siempre (las escenas no tienen la guarda de pvs_paso), solo con PVS_QUITA=1; sin ella la
prueba de cada malla acaba en el primer triangulo que se ve.

Culling: los materiales de PS2 (tabla 4, +04) llevan las banderas de los del PC (408F90: bit 1 dos caras; el PC pone
CULL_CW en las demas, casi todas las de las etapas), pero el GS no recorta caras y el microcodigo del VU1 no esta visto:
la demo sigue con PVR_CULLING_NONE (el PVS ya quita las caras de atras de lo cerrado, que tapan sus caras de delante).

Formato (ORD7 ampliado, compatible: sin el bit, la tabla de antes): nsecciones con el bit 31 = PVS. Mismo sitio: inicio
de cada lista (nsecciones + 1 u32, en bytes), las listas, relleno a 2 y la seccion de cada punto de la ruta (u16). La
lista de cada seccion son las mallas que cambian respecto a la anterior (XOR; la primera: las visibles de la seccion 0),
en orden, cada una como la diferencia con la anterior en un byte (255 y 16 bits si no cabe). La demo lleva la mascara
de la seccion aplicada y la mueve aplicando las listas, hacia delante o hacia atras (pvs_paso en demo/main.c).

Las listas por seccion del original (cs_CS_<zona>_bin, conv_tramo.lee_visibilidad) no son esto: comparadas con este PVS
en PALM (nodos), el 98 % de los que nombra una seccion se ven desde ella, pero son unos 68 de 2511 por seccion, en
rachas de 1,4 secciones, y ni ellas ni su XOR acumulado (947 de media) cubren lo que se ve delante (43 %)."""
import ctypes, os, struct, subprocess, sys, time
import numpy as np

SEC_PUNTOS = 2                                                  # puntos de la ruta por seccion (4 m cada uno)
HUECO = int(os.environ.get("PVS_HUECO", "8"))                   # huecos de invisibilidad que se rellenan (secciones)
MAX_KB = int(os.environ.get("PVS_MAX_KB", "300"))               # tope de las listas (RAM: el hueco de la etapa mayor)
FIN_EXTRA = 32.0                                                # ruta prolongada por cada punta (m; demo PVS_FIN)
QUITA_LADO = 8.0                                                # volumen ancho (lo que no se ve nunca): fuera del limite
QUITA_ALTURAS = (0.6, 3.3, 6.0, 9.0, 12.0)                      # y alturas
QUITA_FIN = 64.0                                                # y prolongacion por cada punta
QUITA_MIN = 4                                                   # triangulos seguidos sin ver para partir una tira
LADO_EXTRA = 3.0                                                # metros fuera del limite transitable
PASO_LADO = 4.0
ALTURAS = (0.6, 3.3, 6.0)                                       # sobre el suelo del perfil (la demo: ALTO_PVS)
EPS = float(os.environ.get("PVS_EPS", "0"))                     # erosion por distancia al borde (m); 0: solo un pixel
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
        if h[12] & XOR: p += int(ini[-1]); p += p % 2; p += 2 * nr
        else: p += 2 * int(ini[-1]) + 2 * nr
    p += -p % 4; p += -p % 32
    d['blob'] = b[p:]
    d['M'] = np.frombuffer(d['mallas'], MALLA); d['V'] = np.frombuffer(d['verts'], VERT)
    d['R'] = np.frombuffer(d['ruta'], '<f4').reshape(nr, pr) if nr else np.zeros((0, pr or 55), np.float32)
    d['N'] = np.frombuffer(d['nodos'], NODO)
    return d

def escribe(d, fn, ini, listas, mapa):
    """reescribe el ORD7 con la tabla de secciones XOR"""
    h = list(d['h']); h[12] = (len(ini) - 1) | XOR if len(ini) > 1 else 0
    tmp = fn + '.tmp'                                           # (cortado a medias, el fichero de antes sigue entero)
    with open(tmp, 'wb') as f:
        f.write(b'ORD7' + struct.pack('<13I', *h))
        for k in ('texs', 'mallas', 'verts', 'ruta', 'nodos', 'carteles'): f.write(d[k])
        if len(ini) > 1:
            f.write(np.asarray(ini, '<u4').tobytes()); f.write(bytes(listas)); f.write(b'\0' * (-f.tell() % 2))
            f.write(np.asarray(mapa, '<u2').tobytes())
        f.write(b'\0' * (-f.tell() % 4)); f.write(b'\0' * (-f.tell() % 32)); f.write(d['blob'])
    os.replace(tmp, fn)

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

def muestras(R, lado=LADO_EXTRA, alturas=ALTURAS, cada=1, fin=None):
    """posiciones del ojo a lo largo de una ruta (ver arriba): cada punto (uno de cada `cada`), a lo ancho del limite
    transitable mas `lado` y a esas alturas sobre el perfil; y la ruta prolongada `fin` metros (FIN_EXTRA) por cada
    punta en linea recta, cada PASO_LADO, con los anchos, el perfil y la altura del punto de la punta: el ojo puede
    estar ahi con la etapa o la union dibujandose (la camara va detras del coche al salir; la etapa siguiente se dibuja
    desde el final de la anterior). La demo no usa la PVS con el ojo mas alla (pvs_paso, PVS_FIN)"""
    fin = FIN_EXTRA if fin is None else fin
    n = len(R); pts = []                                        # (punto de la ruta, tangente hacia delante)
    for k in range(0, n, cada):
        p = R[max(k - 1, 0)]; q = R[min(k + 1, n - 1)]; pts.append((R[k].astype(float), np.array([q[0] - p[0], 0.0, q[2] - p[2]])))
    if n >= 2 and fin > 0:
        for a, b, sg in ((R[0], R[1], -1.0), (R[-1], R[-2], 1.0)):
            t = np.array([b[0] - a[0], 0.0, b[2] - a[2]]) if sg < 0 else np.array([a[0] - b[0], 0.0, a[2] - b[2]])
            l = np.linalg.norm(t)
            if l < 1e-6: continue
            u = t / l
            for j in range(1, int(np.ceil(fin / PASO_LADO)) + 1):
                q = a.astype(float).copy(); q[0] += sg * u[0] * j * PASO_LADO; q[2] += sg * u[2] * j * PASO_LADO; pts.append((q, t))
    out = []
    for a, t in pts:
        l = np.linalg.norm(t)
        if l < 1e-6: continue
        t = t / l; der = np.array([-t[2], 0.0, t[0]])
        li, ld = (a[C_LIM], a[C_LIM + 1]) if len(a) > C_LIM + 1 else (a[3], a[4])
        x0, x1 = -li - lado, ld + lado; m = int(np.ceil((x1 - x0) / PASO_LADO)) + 1
        for x in np.linspace(x0, x1, m):
            g = altura_punto(a, x)
            for hh in alturas:
                out.append((a[0] + x * der[0], g + hh, a[2] + x * der[2]))
    return np.array(out, np.float32).reshape(-1, 3)

def rellena(sec, g):
    """los huecos de g secciones o menos entre dos en las que la malla se ve: visible (menos listas; nunca quita nada)"""
    if g <= 0: return sec
    n = len(sec); idx = np.arange(n)[:, None]
    ult = np.maximum.accumulate(np.where(sec, idx, -1), axis=0)                 # ultima visible <= s
    sig = np.minimum.accumulate(np.where(sec, idx, n + g + 1)[::-1], axis=0)[::-1]   # siguiente visible >= s
    return sec | ((ult >= 0) & (sig < n) & (sig - ult - 1 <= g))

def codifica(x):
    """indices crecientes -> bytes: diferencia con el anterior (el primero desde -1); 255 y 16 bits si no cabe"""
    out = bytearray(); a = -1
    for i in x:
        dd = int(i) - a; a = int(i)
        out += bytes([dd]) if dd < 255 else bytes([255, dd & 255, dd >> 8])
    return out

def mascaras(d, usadas):
    """mascara de los texeles que tapan seguro de cada textura de usadas (las de la lista de recorte): alfa >= 0,5 (la
    PowerVR con PT_ALPHA_REF 0x80, demo/main.c) en el texel y en sus ocho vecinos (con la repeticion de la textura);
    sin mipmaps (con ellos el nivel que se muestrea es otro). Devuelve (bytes, desplazamiento, ancho, alto) por textura
    (las que no tapan: 1 x 1 vacia)"""
    import vq
    nt = len(d['texs']) // 24; buf = [np.zeros(1, np.uint8)]; ofs = np.zeros(max(nt, 1), np.int32)
    ws = np.ones(max(nt, 1), np.int32); hs = np.ones(max(nt, 1), np.int32); p = 1
    for i in sorted(usadas):
        if not 0 <= i < nt: continue
        w, h, fmt, li, off, tam = struct.unpack_from('<6I', d['texs'], 24 * i)
        if fmt & 0x200 or (fmt & 0xff) not in (0, 2) or w * h == 0: continue
        b = d['blob'][off:off + tam]
        a = vq.decodifica(b, w, h, fmt & 255, ((fmt >> 16) & 255) + 1) if fmt & 0x100 else vq.decodifica16(b, w, h, fmt & 255)
        m = a[..., 3] >= 128
        m = m & np.roll(m, 1, 0) & np.roll(m, -1, 0); m = m & np.roll(m, 1, 1) & np.roll(m, -1, 1)
        ofs[i] = p; ws[i] = w; hs[i] = h; buf.append(m.astype(np.uint8).ravel()); p += w * h
    return np.ascontiguousarray(np.concatenate(buf)), ofs, ws, hs

def calcula(d, extras=(), hilos=os.cpu_count() or 4, informe=True, quita=False):
    """visibilidad del modelo d (lee) por seccion y por triangulo (los de cada malla del fichero, tira: v, v + 1,
    v + 2; las de los nodos del original). extras: rutas (arrays) de mas por las que puede ir el ojo (la rama derecha
    de la bifurcacion). quita: tambien las muestras del volumen ancho (QUITA_LADO, QUITA_ALTURAS) para saber lo que no
    se ve nunca. Devuelve dict: sec (nsecciones x nmallas, con las vecinas), tri (nsecciones x ntriangulos, idem),
    nunca (triangulos sin ver desde ninguna muestra, la del volumen ancho incluida), t0 (primer triangulo de cada
    malla), util, muestras, segundos; None si no hay ruta o nodos"""
    t0 = time.time()
    M, V, N, R = d['M'], d['V'], d['N'], d['R']; h = d['h']; nn = len(N); nm = len(M); ini_l = h[5:10]
    if nn == 0 or nm == 0 or len(R) < 2 * SEC_PUNTOS: return None
    assert nm < 65536
    P = np.stack([V['x'], V['y'], V['z']], 1).astype(np.float32); UV = np.stack([V['u'], V['v']], 1).astype(np.float32)
    g = M['nodo'].astype(int); n = M['n'].astype(int)
    util = (g >= 0) & (g < nn) & (n >= 3)                         # mallas de nodos (las demas: cielo, se dibujan)
    gg = np.where(util, g, 0)
    cartel = util & (N['cartel'][gg] >= 0); rama = np.where(util, (N['fl'][gg] >> 8) & 3, 0)
    k = np.arange(nm); opaco = (k >= ini_l[0]) & (k < ini_l[1]); recorte = (k >= ini_l[1]) & (k < ini_l[2])
    # triangulos de cada malla (tira: v, v + 1, v + 2), las de los carteles sin triangulos (se prueban por la esfera)
    nt = np.where(util & ~cartel, n - 2, 0); t0n = np.concatenate([[0], np.cumsum(nt)]).astype(np.int32)
    mi = np.repeat(np.arange(nm), nt); i = M['v0'].astype(np.int64)[mi] + (np.arange(t0n[-1]) - t0n[:-1][mi])
    ntri = int(t0n[-1])
    T = np.ascontiguousarray(np.stack([P[i], P[i + 1], P[i + 2]], 1).reshape(-1, 9) if ntri else np.zeros((1, 9)), np.float32)
    TUV = np.ascontiguousarray(np.stack([UV[i], UV[i + 1], UV[i + 2]], 1).reshape(-1, 6) if ntri else np.zeros((1, 6)), np.float32)
    tex = M['t'].astype(np.int32)
    mas = mascaras(d, set(tex[recorte & util & ~cartel].tolist()))
    tt = np.ascontiguousarray(tex[mi] if ntri else np.zeros(1, np.int32), np.int32)
    # recorte que tapa: con mascara (textura sin mipmaps de 1 bit o 4 de alfa) y el alfa de los tres vertices entero
    al = (V['c'] >> 24).astype(np.int64)
    al3 = (al[i] == 255) & (al[i + 1] == 255) & (al[i + 2] == 255) if ntri else np.zeros(1, bool)
    con_mas = np.zeros(len(mas[2]), bool); con_mas[:] = mas[2] > 1
    tapa_r = recorte[mi] & (tt >= 0) & con_mas[np.clip(tt, 0, len(con_mas) - 1)] & al3 if ntri else np.zeros(1, bool)
    area = np.linalg.norm(np.cross(T[:, 3:6] - T[:, 0:3], T[:, 6:9] - T[:, 0:3]), axis=1) if ntri else np.ones(1)
    degen = area <= 0                                           # area nula en 3D: no pinta ningun pixel
    O = np.ascontiguousarray(((opaco[mi] if ntri else np.zeros(1, bool)).astype(np.uint8)) | (degen.astype(np.uint8) << 1)
                             | (tapa_r.astype(np.uint8) << 2))
    esf = np.concatenate([M['c'], M['r'][:, None]], 1).astype(np.float32)
    esf[cartel] = np.concatenate([N['c'], N['r'][:, None]], 1)[gg[cartel]]   # sus vertices van en el marco del nodo
    esf = np.ascontiguousarray(esf)
    fl = np.ascontiguousarray((cartel.astype(np.uint8) | (((opaco | recorte) & util & ~cartel & (rama == 0)).astype(np.uint8) << 1)
                               | ((~util).astype(np.uint8) << 2)))
    u = (N['fl'] & 15).astype(int); lod = (320.0 / np.tan(FOV_MAX / 2)) / 240.0
    occ_n = np.where(u > 0, N['r'] * lod * np.exp2(np.maximum(u, 1) - 1), LEJOS)
    occ_n = np.minimum(occ_n, LEJOS - N['r']).clip(0)              # distancia al centro del nodo
    # la malla tapa si su nodo se dibuja seguro: distancia al centro de la malla + la de los centros
    dcn = np.linalg.norm(esf[:, :3] - N['c'][gg], axis=1)
    occ = np.ascontiguousarray(np.where(util, (occ_n[gg] - dcn).clip(0), 0), np.float32)
    pos = [muestras(R)] + [muestras(x) for x in extras]; pos = np.concatenate(pos)
    # la seccion de cada muestra: la del punto de la ruta mas cercano (como la demo con el ojo)
    from scipy.spatial import cKDTree
    nsec = (len(R) + SEC_PUNTOS - 1) // SEC_PUNTOS
    arbol = cKDTree(R[:, :3].astype(float))
    sp = (arbol.query(pos.astype(float))[1] // SEC_PUNTOS).astype(np.int32)
    if quita:                                                   # volumen ancho: solo para lo que no se ve nunca (fila nsec)
        pa = [muestras(R, QUITA_LADO, QUITA_ALTURAS, 2, QUITA_FIN)] + [muestras(x, QUITA_LADO, QUITA_ALTURAS, 2, QUITA_FIN) for x in extras]
        pa = np.concatenate(pa); pos = np.concatenate([pos, pa]); sp = np.concatenate([sp, np.full(len(pa), nsec, np.int32)])
    pos = np.ascontiguousarray(pos, np.float32); sp = np.ascontiguousarray(sp, np.int32)
    ntw = (ntri + 31) // 32; nmw = (nm + 31) // 32
    st = np.zeros((nsec + 1, max(ntw, 1)), np.uint32) if quita else None; sm = np.zeros((nsec + 1, nmw), np.uint32)
    L = lib(); fp = ctypes.POINTER(ctypes.c_float); vp = ctypes.c_void_p
    L.pvs.argtypes = [fp, vp, fp, vp, vp, vp, vp, vp, vp, ctypes.c_int, fp, vp, fp, fp, vp, ctypes.c_int, ctypes.c_int,
                      ctypes.c_float, ctypes.c_float, ctypes.c_int, vp, vp]
    c = lambda a: a.ctypes.data_as(fp)
    L.pvs(c(T), O.ctypes.data, c(TUV), tt.ctypes.data, mas[0].ctypes.data, mas[1].ctypes.data, mas[2].ctypes.data,
          mas[3].ctypes.data, t0n.ctypes.data, nm, c(esf), fl.ctypes.data, c(occ), c(pos), sp.ctypes.data, len(pos),
          nsec + 1, K, EPS, hilos, st.ctypes.data if quita else None, sm.ctypes.data)
    secm = np.unpackbits(sm.view(np.uint8), axis=1, bitorder='little')[:, :nm].astype(bool)
    if quita:
        sect = np.unpackbits(st.view(np.uint8), axis=1, bitorder='little')[:, :ntri].astype(bool)
        nunca = ~sect.any(0)                                    # (con la fila del volumen ancho)
        sect = sect[:nsec]
    else: sect = None; nunca = None
    secm = secm[:nsec]
    # cada seccion con sus vecinas (la demo elige la seccion por el punto mas cercano al ojo)
    def vecinas(x):
        y = x.copy(); y[1:] |= x[:-1]; y[:-1] |= x[1:]; return y
    secm = vecinas(secm)
    if sect is not None: sect = vecinas(sect)
    return dict(sec=secm, tri=sect, nunca=nunca, t0=t0n, util=util, cartel=cartel, muestras=len(pos), nsec=nsec,
                seg=time.time() - t0)

def listas(sec, util, nr):
    """las listas XOR por seccion de las mascaras sec (nsecciones x nmallas): (ini, listas, mapa, hueco)"""
    nsec, nm = sec.shape; sec0 = sec; g = HUECO
    while True:                                                 # si las listas no caben en MAX_KB, huecos mayores
        s2 = rellena(sec0, g) & util
        prev = np.zeros(nm, bool); ini = [0]; li = bytearray()
        for s in range(nsec):
            x = np.nonzero(s2[s] ^ prev)[0]; li += codifica(x); ini.append(len(li)); prev = s2[s]
        if len(li) <= MAX_KB * 1024 or g >= nsec: break
        g *= 2
    return ini, li, np.arange(nr) // SEC_PUNTOS, g, s2

def quita_nunca(d, r):
    """reescribe las mallas de d sin los triangulos que no se ven nunca (r = calcula con quita): cada tira de un nodo se
    parte en los trozos que se ven; un trozo de QUITA_MIN o mas triangulos que no se ve sale de la tira (en las puntas
    basta uno: no anade cabecera), los vertices de cada trozo se copian seguidos. Las texturas que ya no usa ninguna
    malla salen del fichero. Devuelve (d nuevo, mapa malla nueva -> malla vieja, mapa de triangulos nuevo -> viejo,
    estadisticas)"""
    M, V = d['M'], d['V']; t0 = r['t0']; nunca = r['nunca']; util = r['util'] & ~r['cartel']
    h = list(d['h']); ini_l = h[5:10]
    mallas = []; verts = []; nv = 0; viejo = []; tri_map = []; ini = [0] * 5; quitados = 0; vq0 = 0
    for k in range(len(M)):
        for l in range(4):
            if ini_l[l] == k: ini[l] = len(mallas)
        m = M[k]; v0 = int(m['v0']); n = int(m['n'])
        trozos = [(0, max(n - 2, 0))]
        if util[k] and n >= 3:
            vis = ~nunca[t0[k]:t0[k + 1]]; nt = n - 2; trozos = []; a = 0
            while a < nt:                                       # trozos visibles con los huecos cortos dentro
                while a < nt and not vis[a]: a += 1
                if a >= nt: break
                b = a
                while True:
                    while b < nt and vis[b]: b += 1
                    c = b
                    while c < nt and not vis[c]: c += 1
                    if b < nt and c < nt and c - b < QUITA_MIN: b = c; continue
                    break
                trozos.append((a, b)); a = b
            quitados += nt - sum(b - a for a, b in trozos)
        for a, b in trozos:
            if b <= a and n >= 3: continue
            nn_ = b - a + 2 if n >= 3 else n
            vv = V[v0 + a:v0 + a + nn_]
            q = np.zeros(1, MALLA)[0]
            if n >= 3 and util[k] and (a, b) != (0, n - 2):
                Pq = np.stack([vv['x'], vv['y'], vv['z']], 1).astype(np.float64); cc = Pq.mean(0)
                q['c'] = cc; q['r'] = float(np.sqrt(((Pq - cc) ** 2).sum(1).max()))
            else: q['c'] = m['c']; q['r'] = m['r']
            q['v0'] = nv; q['n'] = nn_; q['t'] = m['t']; q['nodo'] = m['nodo']
            mallas.append(q); verts.append(vv); nv += nn_; viejo.append(k)
            if util[k] and n >= 3: tri_map.append(t0[k] + np.arange(a, b))
    ini[4] = len(mallas)
    for l in range(4, -1, -1):                                  # listas sin mallas
        if ini_l[l] >= len(M): ini[l] = len(mallas)
    Mn = np.array(mallas, MALLA) if mallas else np.zeros(0, MALLA); Vn = np.concatenate(verts) if verts else V[:0]
    # texturas que siguen en uso (y su orden), los texeles de las demas fuera
    usadas = sorted({int(x) for x in Mn['t'] if x >= 0}); nt_ = len(d['texs']) // 24
    nuevo = {t: j for j, t in enumerate(usadas)}; texs = b''; blob = b''; kb_fuera = 0
    for t in range(nt_):
        w, hh, fmt, li, off, tam = struct.unpack_from('<6I', d['texs'], 24 * t)
        if t not in nuevo: kb_fuera += tam; continue
        texs += struct.pack('<6I', w, hh, fmt, li, len(blob), tam); blob += d['blob'][off:off + tam] + b'\0' * (-tam % 32)
    Mn['t'] = np.array([nuevo.get(int(x), -1) if x >= 0 else -1 for x in Mn['t']], np.int32)
    d2 = dict(d); h[0] = len(Mn); h[1] = len(Vn); h[4] = len(usadas); h[5:10] = ini
    d2['h'] = h; d2['mallas'] = Mn.tobytes(); d2['verts'] = Vn.tobytes(); d2['texs'] = texs; d2['blob'] = blob
    d2['M'] = Mn; d2['V'] = Vn
    est = dict(tri_quitados=quitados, tri=int(t0[-1]), mallas=(len(M), len(Mn)), verts=(len(V), len(Vn)),
               texs=(nt_, len(usadas)), kb_texeles=kb_fuera >> 10)
    return d2, np.array(viejo), np.concatenate(tri_map) if tri_map else np.zeros(0, int), est

def anade(fn, extras=(), quita=False):
    """calcula el PVS del fichero fn y lo reescribe con el. extras: rutas de mas (arrays como la del modelo, o ficheros
    ORD7 con ellas: BKD.DAT). quita: tambien fuera los triangulos que no se ven nunca (quita_nunca; las etapas: las
    escenas de la meta van en las uniones; solo con PVS_QUITA=1 en el entorno: ver arriba). PVS=0 en el entorno: no se
    calcula (el fichero queda sin secciones)"""
    if os.environ.get('PVS', '1') == '0': return None
    quita = quita and os.environ.get('PVS_QUITA', '0') == '1'
    d = lee(fn)
    ex = [lee(x)['R'] if isinstance(x, str) else np.asarray(x, np.float32) for x in extras]
    r = calcula(d, ex, quita=quita)
    if r is None: escribe(d, fn, [0], [], []); return None
    nm0, nv0 = len(d['M']), len(d['V']); est_q = None
    sec = r['sec']; util = r['util']
    if quita:
        d, viejo, tmap, est_q = quita_nunca(d, r)
        # la visibilidad de cada malla nueva: algun triangulo de su trozo visto desde la seccion (carteles y demas: la
        # de la malla vieja)
        util = r['util'][viejo]; sec_n = r['sec'][:, viejo].copy()
        tr = r['tri']; t0v = r['t0']; pos = 0
        for j, kv in enumerate(viejo):
            if r['util'][kv] and not r['cartel'][kv] and int(d['M'][j]['n']) >= 3:
                nt = int(d['M'][j]['n']) - 2; sec_n[:, j] = tr[:, tmap[pos:pos + nt]].any(1); pos += nt
        sec = sec_n
    ini, li, mapa, g, s2 = listas(sec, util, len(d['R']))
    escribe(d, fn, ini, li, mapa)
    vn = np.where(util, d['M']['n'], 0).astype(float)
    if informe_pvs:
        print('PVS %s: %d muestras, %d secciones, huecos de %d, %d KB de listas; quedan de media %.0f %% de las mallas y %.0f %% '
              'de los vertices; %.0f s' % (os.path.basename(fn), r['muestras'], r['nsec'], g, (4 * (r['nsec'] + 1) + len(li) + 2 * len(d['R'])) >> 10,
              100 * s2.sum(1).mean() / max(1, util.sum()), 100 * (s2 @ vn).mean() / max(1, vn.sum()), r['seg']))
        if est_q: print('PVS %s: no se ven nunca %d de %d triangulos; mallas %d -> %d, vertices %d -> %d, texturas %d -> %d (%d KB de texeles menos)'
                        % (os.path.basename(fn), est_q['tri_quitados'], est_q['tri'], nm0, est_q['mallas'][1], nv0, est_q['verts'][1],
                           est_q['texs'][0], est_q['texs'][1], est_q['kb_texeles']))
    return dict(r=r, quita=est_q)

informe_pvs = True

def main():
    """pvs.py fichero.DAT [--ruta otra.DAT]...: con --ruta, rutas de mas por las que puede ir el ojo"""
    args = sys.argv[1:]; ex = []; fs = []; q = False
    while args:
        a = args.pop(0)
        if a == '--ruta': ex.append(args.pop(0))
        elif a == '--quita': q = True
        else: fs.append(a)
    for f in fs: anade(f, ex, quita=q)

if __name__ == '__main__':
    main()
