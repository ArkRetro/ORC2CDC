#!/usr/bin/env python3
"""conv_reflejo.py <datos/> [REFLEJO.DAT]: el reflejo del entorno en la carroceria del coche del jugador (Rev 2.4).

El original (SLES-53998 0x103dc8 / 0x1040e0 = 414340 / 414050 del PC) dibuja cada cuadro el entorno visto desde el coche
en un cubo de caras de 64x64 (con cs_ENV_<zona>, una version simplificada de la etapa, y su cielo) y lo usa de mapa de
entorno de la carroceria. DESVIACION: la Dreamcast no dibuja ese cubo; aqui se calcula una vez, de los datos ya
convertidos de cada etapa, un mapa de 16x16 cada PASO_MAPA puntos de su ruta (48 m), visto desde ALTO_OJO sobre ella:
rayos (RAYOS x RAYOS, tools/c/reflejo.c con ctypes) contra los triangulos de las listas 0 y 1 a menos de RADIO (cada uno
con el color medio de su textura por el de sus vertices) y el cielo (el grupo 0 de SEGnn.DAT, mapa_cielo) donde no
chocan; desenfocado (sin bordes duros), reducido a 16x16 y x GANANCIA2. demo/main.c mezcla cada cuadro los dos mapas
de alrededor (y en la union el ultimo de la etapa con el primero de la siguiente) y pone el Fresnel por vertice. La
Rev 1.3 (un mapa del cielo por etapa y otro del tunel que entraba de golpe; formato 'RFL1') se quito en la 1.4.

REFLEJO.DAT 'RFL2' (sectores de 2048 bytes):
  sector 0: 'RFL2', u32 registros (SEGnn: n), u32 lado (16), u32 puntos de la ruta entre mapas; desde el byte 16,
            por registro u32 sector de sus mapas y u32 cuantos (0: sin mapas)
  mapas: lado x lado texeles RGB565 'twiddled' seguidos, uno por punto 0, PASO_MAPA, 2 PASO_MAPA... y el ultimo punto.
  Proyeccion: el vector d (unitario, y arriba, en el marco del fichero) cae en u = 0,5 + 0,4 dx / (1 + dy),
  v = 0,5 + 0,4 dz / (1 + dy) (el cenit en el centro, el horizonte en el circulo de radio 0,4).
Lento (minutos; en paralelo por etapa, PYTHON_CPU_COUNT)."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pak_union, vq
from modelo import twiddle

LADO = 32; SUPER = 4; SECTOR = 2048
ESCALA = 0.4                                                  # REFL_ESCALA de demo/main.c
MIN_PUNTOS = 6; ALTO0, ALTO1 = 3.0, 14.0; LADO_M = 2.5; RAMPA = 3
SUELO = 0.45                                                  # el horizonte oscurecido bajo el cielo
GANANCIA = 0.30                                               # el cielo se suma a la pintura (textura x especular): a
                                                              # plena luz la quemaba; ~ el brillo medio del mapa fijo del coche

class Ord7:
    def __init__(s, d):
        assert d[:4] == b'ORD7', 'no es ORD7'; s.d = d
        s.nm, s.nv, s.nr, s.fpr, s.nt = struct.unpack_from('<5I', d, 4); s.listas = struct.unpack_from('<5I', d, 24)
        s.nn, s.nc, s.ns = struct.unpack_from('<3I', d, 44); o = 56
        s.tex = [struct.unpack_from('<6I', d, o + 24 * i) for i in range(s.nt)]; o += 24 * s.nt
        s.mallas = np.frombuffer(d, np.dtype([('c', '<f4', 3), ('r', '<f4'), ('v0', '<u4'), ('n', '<u4'), ('tex', '<i4'), ('nodo', '<i4')]), s.nm, o); o += 32 * s.nm
        s.verts = np.frombuffer(d, np.dtype([('p', '<f4', 3), ('uv', '<f4', 2), ('c', '<u4')]), s.nv, o); o += 24 * s.nv
        s.ruta = np.frombuffer(d, '<f4', s.nr * s.fpr, o).reshape(s.nr, s.fpr); o += 4 * s.nr * s.fpr
        s.nodos = np.frombuffer(d, np.dtype([('c', '<f4', 3), ('r', '<f4'), ('ban', '<u4'), ('cartel', '<i4'), ('_', '<u4', 2)]), s.nn, o)
        s.blob = len(d) - max((t[4] + t[5] + 31) & ~31 for t in s.tex) if s.nt else len(d)
    def imagen(s, i):
        w, h, fmt, lista, o, tam = s.tex[i]; n = ((fmt >> 16) & 255) + 1; b = s.d[s.blob + o:s.blob + o + tam]
        return vq.decodifica(b, w, h, fmt & 255, n) if fmt & 0x100 else vq.decodifica16(b, w, h, fmt & 255)

def carga(datos, nombre, paks):
    b = open(os.path.join(datos, nombre), 'rb').read()
    if b[:4] == b'PAKD':
        pk = b[20:36].split(b'\0')[0].decode()
        if pk not in paks: paks[pk] = open(os.path.join(datos, pk), 'rb').read()
        b = pak_union.recompone(b, paks[pk])
    return Ord7(b)

def tiras(m, k0, k1, filtro):
    """triangulos (indices de vertice) de las tiras de las mallas k0..k1 que pasan el filtro"""
    r = []
    for k in range(k0, k1):
        ma = m.mallas[k]
        if ma['n'] < 3 or not filtro(ma): continue
        i = np.arange(int(ma['v0']), int(ma['v0'] + ma['n'] - 2), dtype=np.int64)
        r.append(np.stack([i, i + 1, i + 2], 1))
    return np.concatenate(r) if r else np.zeros((0, 3), np.int64)

def direcciones(n):
    """la direccion de cada texel de un mapa de n x n (y si cae dentro de lo que la proyeccion cubre)"""
    v, u = (np.mgrid[0:n, 0:n] + 0.5) / n
    a, b = (u - 0.5) / ESCALA, (v - 0.5) / ESCALA; q = a * a + b * b      # a = dx / (1 + dy), b = dz / (1 + dy)
    dy = (1 - q) / (1 + q); return np.stack([a * (1 + dy), dy, b * (1 + dy)], -1)

def mapa_cielo(m):
    """lado x lado RGB (float) del grupo 0 del cielo (nodo -256) o None"""
    T = tiras(m, m.listas[3], m.listas[4], lambda ma: ma['nodo'] == -256)
    if not len(T): return None
    P = m.verts['p'].astype(np.float64); UV = m.verts['uv'].astype(np.float64); C = m.verts['c']
    tex_de = np.zeros(len(m.verts), np.int32) - 1
    for k in range(m.listas[3], m.listas[4]):
        ma = m.mallas[k]; tex_de[ma['v0']:ma['v0'] + ma['n']] = ma['tex']
    n = LADO * SUPER; D = direcciones(n).reshape(-1, 3)
    col = np.zeros((len(D), 3)); hecho = np.zeros(len(D), bool); imgs = {}
    v0, e1, e2 = P[T[:, 0]], P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]]
    for j in range(len(T)):                                   # rayo desde el centro contra cada triangulo (en orden)
        pv = np.cross(D, e2[j]); det = pv @ e1[j]
        ok = np.abs(det) > 1e-9; inv = np.where(ok, 1.0 / np.where(ok, det, 1), 0)
        tv = -v0[j]; bu = (pv @ tv) * inv; qv = np.cross(tv, e1[j]); bv = (D @ qv) * inv; t = (e2[j] @ qv) * inv
        g = ok & (bu >= -1e-6) & (bv >= -1e-6) & (bu + bv <= 1 + 1e-6) & (t > 0)
        if not g.any(): continue
        w = np.stack([1 - bu[g] - bv[g], bu[g], bv[g]], 1); i3 = T[j]
        c = sum(w[:, q:q + 1] * np.stack([(C[i3[q]] >> 16) & 255, (C[i3[q]] >> 8) & 255, C[i3[q]] & 255]).astype(float)[None] for q in range(3))
        al = np.ones(g.sum()); ti = int(tex_de[i3[0]])
        if ti >= 0:
            if ti not in imgs: imgs[ti] = m.imagen(ti).astype(float)
            im = imgs[ti]; h_, w_ = im.shape[:2]; uv = sum(w[:, q:q + 1] * UV[i3[q]][None] for q in range(3))
            x = uv[:, 0] * w_ - 0.5; y = uv[:, 1] * h_ - 0.5; x0 = np.floor(x).astype(int); y0 = np.floor(y).astype(int)
            fx, fy = (x - x0)[:, None], (y - y0)[:, None]
            s = lambda xx, yy: im[yy % h_, xx % w_]
            tx = (s(x0, y0) * (1 - fx) + s(x0 + 1, y0) * fx) * (1 - fy) + (s(x0, y0 + 1) * (1 - fx) + s(x0 + 1, y0 + 1) * fx) * fy
            c = c * tx[:, :3] / 255.0
            if (m.tex[ti][2] & 255) != 1: al = tx[:, 3] / 255.0   # con alfa (nubes sobre el degradado): mezcla normal
        col[g] = col[g] * (1 - al[:, None]) + c * al[:, None]; hecho[g] = True
    if not hecho.any(): return None
    col = col.reshape(n, n, 3); hecho = hecho.reshape(n, n); Dn = D.reshape(n, n, 3)
    hor = hecho & (np.abs(Dn[..., 1]) < 0.08)
    base = col[hor].mean(0) if hor.any() else col[hecho].mean(0)
    col[~hecho] = base * SUELO
    return col.reshape(LADO, SUPER, LADO, SUPER, 3).mean((1, 3))

def mapa_tunel():
    D = direcciones(LADO * SUPER); dx, dy, dz = D[..., 0], D[..., 1], D[..., 2]
    c = np.zeros(D.shape) + np.array([26.0, 25.0, 24.0])      # paredes y boveda en penumbra
    c[dy < -0.05] = [12, 12, 12]                              # la calzada
    az = np.abs(np.arctan2(np.abs(dx), dy))                   # angulo desde el cenit hacia los lados
    for a0 in (0.55,):                                        # una fila de lamparas a cada lado, a ~31 grados del cenit
        fila = np.exp(-((az - a0) / 0.07) ** 2) * (dy > 0.2)
        tramo = 0.5 + 0.5 * np.cos(np.arctan2(dz, np.maximum(dy, 1e-3)) * 9.0)   # lamparas sueltas a lo largo de z
        c += (fila * (0.35 + 0.65 * tramo ** 3))[..., None] * np.array([255.0, 236.0, 190.0])
    return np.clip(c, 0, 255).reshape(LADO, SUPER, LADO, SUPER, 3).mean((1, 3))

def rgb565(a):
    a = np.clip(np.round(a), 0, 255).astype(np.uint32)
    v = ((a[..., 0] >> 3) << 11) | ((a[..., 1] >> 2) << 5) | (a[..., 2] >> 3)
    out = np.zeros(LADO * LADO, np.uint16); out[twiddle(LADO, LADO).ravel()] = v.ravel(); return out.tobytes()

def cubierta(m):
    """por punto de la ruta, 0..255: bajo techo"""
    n = m.nr; R = m.ruta[:, :3].astype(np.float64)
    cartel = lambda ma: ma['nodo'] >= 0 and ma['nodo'] < m.nn and m.nodos[ma['nodo']]['cartel'] != -1
    T = tiras(m, m.listas[0], m.listas[3], lambda ma: not cartel(ma))
    P = m.verts['p'].astype(np.float64); A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
    area = np.abs((B[:, 0] - A[:, 0]) * (C[:, 2] - A[:, 2]) - (C[:, 0] - A[:, 0]) * (B[:, 2] - A[:, 2]))
    ok = area > 1e-4; A, B, C = A[ok], B[ok], C[ok]
    lo = np.minimum(np.minimum(A, B), C); hi = np.maximum(np.maximum(A, B), C)
    CEL = 16.0; rej = {}
    cx0 = np.floor(lo[:, 0] / CEL).astype(int); cx1 = np.floor(hi[:, 0] / CEL).astype(int)
    cz0 = np.floor(lo[:, 2] / CEL).astype(int); cz1 = np.floor(hi[:, 2] / CEL).astype(int)
    chico = (cx1 - cx0 < 8) & (cz1 - cz0 < 8)                 # (lo enorme, suelos y mares, no es un techo)
    for j in np.nonzero(chico)[0]:
        for x in range(cx0[j], cx1[j] + 1):
            for z in range(cz0[j], cz1[j] + 1): rej.setdefault((x, z), []).append(j)
    rej = {k: np.array(v) for k, v in rej.items()}
    def techo(p):
        l = rej.get((int(np.floor(p[0] / CEL)), int(np.floor(p[2] / CEL))))
        if l is None: return False
        a, b, c = A[l], B[l], C[l]
        d = (b[:, 2] - c[:, 2]) * (a[:, 0] - c[:, 0]) + (c[:, 0] - b[:, 0]) * (a[:, 2] - c[:, 2])
        w0 = ((b[:, 2] - c[:, 2]) * (p[0] - c[:, 0]) + (c[:, 0] - b[:, 0]) * (p[2] - c[:, 2])) / d
        w1 = ((c[:, 2] - a[:, 2]) * (p[0] - c[:, 0]) + (a[:, 0] - c[:, 0]) * (p[2] - c[:, 2])) / d
        w2 = 1 - w0 - w1; g = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        y = w0 * a[:, 1] + w1 * b[:, 1] + w2 * c[:, 1] - p[1]
        return bool((g & (y > ALTO0) & (y < ALTO1)).any())
    cub = np.zeros(n, bool)
    for i in range(n):
        t = R[min(i + 1, n - 1)] - R[max(i - 1, 0)]; l = np.hypot(t[0], t[2]) or 1.0
        lat = np.array([t[2] / l, 0, -t[0] / l])
        cub[i] = all(techo(R[i] + lat * s) for s in (0.0, -LADO_M, LADO_M))
    i = 0                                                      # tramos de menos de MIN_PUNTOS: fuera
    while i < n:
        j = i
        while j < n and cub[j] == cub[i]: j += 1
        if cub[i] and j - i < MIN_PUNTOS: cub[i:j] = False
        i = j
    v = cub.astype(float)
    for _ in range(RAMPA): v = np.maximum(v, np.convolve(v, [0.5, 0, 0.5], 'same') * 0.75)
    return np.clip(np.round(v * 255), 0, 255).astype(np.uint8), int(cub.sum())

LADO2 = 16; RAYOS = 32; PASO_MAPA = 12; RADIO = 300.0; ALTO_OJO = 1.0; GANANCIA2 = 0.85
DESENFOQUE = float(os.environ.get("REFL_DESENFOQUE", "1.2"))   # (0,5 en pruebas: mas siluetas)
NODO_LEJ = 0x20

def triangulos(m):
    """los triangulos de la etapa que se reflejan (listas 0 y 1, sin los nodos lejanos) y su color medio: la textura
    media (con su alfa) por el color de los vertices"""
    lej = lambda ma: 0 <= ma['nodo'] < m.nn and m.nodos[ma['nodo']]['ban'] & NODO_LEJ
    T = tiras(m, m.listas[0], m.listas[2], lambda ma: not lej(ma))
    P = m.verts['p'].astype(np.float32); C = m.verts['c']
    tex_de = np.full(len(m.verts), -1, np.int32)
    for k in range(m.listas[0], m.listas[2]):
        ma = m.mallas[k]; tex_de[ma['v0']:ma['v0'] + ma['n']] = ma['tex']
    med = {}
    for t in set(tex_de[T[:, 0]].tolist()):
        if t < 0: med[t] = np.array([128.0] * 3); continue
        im = m.imagen(t).astype(np.float64); a = im[..., 3:] / 255.0
        med[t] = (im[..., :3] * a).sum((0, 1)) / max(a.sum(), 1)
    vc = np.stack([(C >> 16) & 255, (C >> 8) & 255, C & 255], 1).astype(np.float64) / 255.0
    M = np.array([med.get(t, np.array([128.0] * 3)) for t in range(-1, max(med) + 1)])   # (indice t + 1)
    col = M[tex_de[T[:, 0]] + 1] * vc[T].mean(1)
    tri = P[T].reshape(-1, 9)
    ok = np.linalg.norm(np.cross(tri[:, 3:6] - tri[:, :3], tri[:, 6:9] - tri[:, :3]), axis=1) > 1e-4
    return np.ascontiguousarray(tri[ok]), np.ascontiguousarray(col[ok].astype(np.float32))

def suaviza(a, n):
    """RAYOS x RAYOS -> n x n: desenfoque (el reflejo en una carroceria curva no tiene bordes duros) y caja"""
    from PIL import Image, ImageFilter
    im = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(DESENFOQUE))
    return np.asarray(im.resize((n, n), Image.BOX)).astype(np.float64)

def rgb565n(a, n):
    a = np.clip(np.round(a), 0, 255).astype(np.uint32)
    v = ((a[..., 0] >> 3) << 11) | ((a[..., 1] >> 2) << 5) | (a[..., 2] >> 3)
    out = np.zeros(n * n, np.uint16); out[twiddle(n, n).ravel()] = v.ravel(); return out.tobytes()

def mapas_etapa(args):
    """los mapas de 16x16 de una etapa: uno cada PASO_MAPA puntos de su ruta (y el ultimo punto)"""
    datos, k = args
    import ctypes, nativo
    lib = nativo.lib('reflejo'); f = ctypes.POINTER(ctypes.c_float)
    lib.rayos.argtypes = [f, f, ctypes.c_int, f, f, ctypes.c_int, ctypes.c_float, f, f]; lib.rayos.restype = ctypes.c_int
    m = carga(datos, 'SEG%02d.DAT' % k, {})
    c = mapa_cielo(m)
    fondo = np.zeros((RAYOS, RAYOS, 3)) + 128.0 if c is None else \
        np.asarray(__import__('PIL.Image').Image.fromarray(np.clip(c, 0, 255).astype(np.uint8)).resize((RAYOS, RAYOS))).astype(np.float64)
    fondo = np.ascontiguousarray(fondo.reshape(-1, 3).astype(np.float32))
    D = direcciones(RAYOS).reshape(-1, 3); D = np.ascontiguousarray((D / np.linalg.norm(D, axis=1, keepdims=True)).astype(np.float32))
    tri, col = triangulos(m)
    lo = np.minimum(np.minimum(tri[:, 0:3], tri[:, 3:6]), tri[:, 6:9]); hi = np.maximum(np.maximum(tri[:, 0:3], tri[:, 3:6]), tri[:, 6:9])
    puntos = list(range(0, m.nr, PASO_MAPA))
    if puntos[-1] != m.nr - 1: puntos.append(m.nr - 1)
    sal = np.zeros((len(D), 3), np.float32); bloques = []
    for i in puntos:
        o = (m.ruta[i, :3].astype(np.float32) + np.array([0, ALTO_OJO, 0], np.float32))
        cerca = ((lo - RADIO <= o) & (hi + RADIO >= o)).all(1)
        t, cl = np.ascontiguousarray(tri[cerca]), np.ascontiguousarray(col[cerca])
        lib.rayos(t.ctypes.data_as(f), cl.ctypes.data_as(f), len(t), np.ascontiguousarray(o).ctypes.data_as(f),
                  D.ctypes.data_as(f), len(D), ctypes.c_float(0.3), fondo.ctypes.data_as(f), sal.ctypes.data_as(f))
        bloques.append(rgb565n(suaviza(sal.reshape(RAYOS, RAYOS, 3), LADO2) * GANANCIA2, LADO2))
    return k, b''.join(bloques), len(puntos), c is not None

def main():
    """REFLEJO.DAT 'RFL2' (Rev 2.4): por etapa, un mapa de 16x16 del entorno cada PASO_MAPA puntos de la ruta"""
    if len(sys.argv) < 2: raise SystemExit(__doc__.split('\n')[0])
    datos = sys.argv[1]; sal = sys.argv[2] if len(sys.argv) > 2 else os.path.join(datos, 'REFLEJO.DAT')
    nums = sorted(int(f[3:5]) for f in os.listdir(datos) if len(f) == 9 and f.startswith('SEG') and f.endswith('.DAT') and f[3:5].isdigit())
    if not nums: raise SystemExit('no hay SEGnn.DAT en %s' % datos)
    nreg = max(nums) + 1
    from multiprocessing import Pool
    with Pool(int(os.environ.get('PYTHON_CPU_COUNT') or os.cpu_count() or 1)) as pool:
        res = {k: (b, n, cie) for k, b, n, cie in pool.imap_unordered(mapas_etapa, [(datos, k) for k in nums])}
    cab = bytearray(SECTOR); struct.pack_into('<4s3I', cab, 0, b'RFL2', nreg, LADO2, PASO_MAPA)
    cuerpo = bytearray(); sec = 1; maxn = 0
    for k in nums:
        b, n, cie = res[k]; b += b'\0' * (-len(b) % SECTOR)
        struct.pack_into('<2I', cab, 16 + 8 * k, sec, n); cuerpo += b; sec += len(b) // SECTOR; maxn = max(maxn, n)
        print('SEG%02d: %d mapas%s' % (k, n, '' if cie else ' (sin cielo)'), flush=True)
    tmp = sal + '.tmp'; open(tmp, 'wb').write(bytes(cab) + bytes(cuerpo)); os.replace(tmp, sal)
    print('%s: %d KB, %d etapas, hasta %d mapas por etapa (%d KB)' % (os.path.basename(sal), (SECTOR + len(cuerpo)) >> 10, len(nums), maxn, maxn * LADO2 * LADO2 * 2 >> 10))

if __name__ == '__main__':
    main()
