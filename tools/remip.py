"""remip.py: mipmaps de la PowerVR en lo lejano de las etapas y uniones (ORD7). Ver docs/PARIDAD.md, "Mipmaps en lo
lejano".

 remip.py --info [-v] fichero.DAT... | datos/ SEGnn BKnn...: texturas con y sin mipmaps (mip, cuadradas VQ o de 16 bits
   sin ellos, rectangulares, cielo; numero y KB) y, con -v, cada una con su texel en metros y desde donde se reduce
   (por debajo de 1 texel por pixel, con la focal de la carrera fx = 426,7).
 remip.py --datos entrada/ salida/ trabajo/ [--solo SEG00,BK06,...]: anade los mipmaps a las etapas y uniones de una
   carpeta de datos ya hecha (las de los paquetes se recomponen y se reempaquetan con pak_union.py, como
   grupos_nodos.datos); los ficheros que cambian van a salida/ (puede ser la misma). tools/regenera.py lo pasa al final.

Hasta ahora solo llevaban mipmaps las texturas cuadradas del suelo con MXL > 0 en el original (modelo.ajusta_texturas).
Aqui, todas las de la escena que se ven reducidas (salvo el cielo y las que no se reducen antes de D_MAX m), por orden
de detalle fino x area / coste y hasta el tope de VRAM de cada fichero (TOPE_ETAPA, TOPE_UNION y PAREJA: ninguna
combinacion etapa + union del curso pasa del peor caso de antes, 944 KB):
 - cuadradas: sus niveles (promedio de 2x2; en las de recorte ARGB1555 el alfa de cada nivel se corta para guardar la
   cobertura de la base) desde la imagen de ahora (VQ decodificada o 16 bits) y VQ con un libro para todos los niveles
   (vq.codifica_mip) si el error RMS frente a esa imagen y sus niveles es <= UMBRAL; si no, sin mipmaps (en 16 bits
   costarian 2,7 veces mas). Las de menos de 32x32, en 16 bits con niveles.
 - rectangulares (la PowerVR solo los admite cuadrados): estiradas al lado mayor (interpolacion lineal entre centros de
   texel: las UV normalizadas no cambian y con el filtro bilineal se ve igual de cerca) y con el D adjust que deja el
   nivel de mip donde lo pondria la de antes (sesgo; bits 24-27 del formato, demo/main.c compila_cab). Al lado menor
   solo si pierde <= PERDIDA de RMS (en las de las etapas, nunca: tienen detalle en los dos ejes).
Determinista por contenido (misma textura y lado -> mismos bytes): la meta y su etapa la siguen compartiendo
(DECIDIDO: lo que hizo la etapa con cada textura lo repite la union) y los paquetes deduplican igual."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import revq, vq

MALLA = np.dtype([('c', '<f4', 3), ('r', '<f4'), ('i0', '<u4'), ('n', '<u4'), ('ti', '<i4'), ('nodo', '<i4')])
VERT = np.dtype([('p', '<f4', 3), ('uv', '<f4', 2), ('c', '<u4')])
FX = 426.7                                                       # focal de la carrera (px; demo/main.c, fx)


def parte(b):
    """las partes de un ORD7 (bytes): cabecera, texturas, mallas, vertices, inicio de las listas y del blob"""
    if b[:4] != b'ORD7': raise ValueError('no es un ORD7')
    h = struct.unpack_from('<13I', b, 4); nm, nv, ntex = h[0], h[1], h[4]
    texs = [list(struct.unpack_from('<6I', b, 56 + 24 * i)) for i in range(ntex)]
    pm = 56 + 24 * ntex
    M = np.frombuffer(b, MALLA, nm, pm); V = np.frombuffer(b, VERT, nv, pm + 32 * nm)
    return dict(h=h, texs=texs, M=M, V=V, inicio=h[5:10], blob0=revq.bloque_texeles(b))


def lista_de(d):
    """lista de cada malla (0 opaca, 1 recorte, 2 transparente, 3 cielo)"""
    ini = d['inicio']; li = np.zeros(len(d['M']), int)
    for l in range(4): li[ini[l]:ini[l + 1]] = l
    return li


def uso(d, aspecto=False):
    """por textura: vertices, mallas del cielo, vertices de suelo, lado del texel en metros (mediana por area) y su
    area; con aspecto, ademas log2(lado del texel en v / en u) en el mundo (mediana por area)"""
    M, V, texs = d['M'], d['V'], d['texs']; li = lista_de(d); n = len(texs)
    nv = np.zeros(n, int); cielo = np.zeros(n, bool); suelo = np.zeros(n, int); areas = [[] for _ in range(n)]
    area = np.zeros(n); jac = [[] for _ in range(n)]
    P = V['p'].astype(np.float64); UV = V['uv'].astype(np.float64)
    for k, m in enumerate(M):
        t = int(m['ti'])
        if t < 0 or t >= n: continue
        i0, c = int(m['i0']), int(m['n']); nv[t] += c
        if li[k] == 3: cielo[t] = True; continue
        if c < 3: continue
        p = P[i0:i0 + c]; uv = UV[i0:i0 + c]
        a, b_, cc = p[:-2], p[1:-1], p[2:]
        cr = np.cross(b_ - a, cc - a); A = 0.5 * np.linalg.norm(cr, axis=1)
        nrm = np.abs(cr[:, 1]) / np.maximum(2 * A, 1e-12); suelo[t] += int((nrm > 0.7).sum())
        ua, ub, uc = uv[:-2], uv[1:-1], uv[2:]
        au = 0.5 * np.abs((ub[:, 0] - ua[:, 0]) * (uc[:, 1] - ua[:, 1]) - (uc[:, 0] - ua[:, 0]) * (ub[:, 1] - ua[:, 1]))
        ok = (A > 1e-6) & (au > 1e-9)
        if ok.any(): areas[t].append(np.stack([A[ok], au[ok]], 1)); area[t] += float(A[ok].sum())
        if aspecto and ok.any():
            e1, e2 = (b_ - a)[ok], (cc - a)[ok]; d1, d2 = (ub - ua)[ok], (uc - ua)[ok]
            det = d1[:, 0] * d2[:, 1] - d2[:, 0] * d1[:, 1]
            Pu = (e1 * d2[:, 1:2] - e2 * d1[:, 1:2]) / det[:, None]; Pv = (e2 * d1[:, 0:1] - e1 * d2[:, 0:1]) / det[:, None]
            lu, lv = np.linalg.norm(Pu, axis=1), np.linalg.norm(Pv, axis=1); bien = (lu > 1e-9) & (lv > 1e-9)
            if bien.any(): jac[t].append(np.stack([A[ok][bien], np.log2(lv[bien] / lu[bien])], 1))
    lado = np.full(n, np.nan)
    if aspecto:                                                  # por textura: log2(texel en v / texel en u) en metros
        asp = np.full(n, np.nan)
        for t in range(n):
            if not jac[t]: continue
            x = np.concatenate(jac[t]); o = np.argsort(x[:, 1]); cw = np.cumsum(x[o, 0])
            asp[t] = x[o, 1][np.searchsorted(cw, cw[-1] / 2)] + np.log2(texs[t][0] / texs[t][1])
    for t in range(n):
        if not areas[t]: continue
        x = np.concatenate(areas[t]); w, hh = texs[t][0], texs[t][1]
        s = np.sqrt(x[:, 0] / (x[:, 1] * w * hh))                    # metros por texel
        o = np.argsort(s); cw = np.cumsum(x[o, 0]); lado[t] = s[o][np.searchsorted(cw, cw[-1] / 2)]
    if aspecto: return nv, cielo, suelo, lado, area, asp
    return nv, cielo, suelo, lado, area


def clase(t, cielo):
    w, h, fmt = t[0], t[1], t[2]
    if cielo: return 'cielo'
    if fmt & 0x200: return 'mip'
    if w != h: return 'rect'
    return 'cuad_vq' if fmt & 0x100 else 'cuad_16'


def info(nombre, b, detalle=False):
    d = parte(b); nv, cielo, suelo, lado, area = uso(d)
    res = {}
    for t, x in enumerate(d['texs']):
        if nv[t] == 0: continue
        c = clase(x, cielo[t]); r = res.setdefault(c, [0, 0.0]); r[0] += 1; r[1] += ((x[5] + 31) & ~31) / 1024
    if detalle:
        for t, x in enumerate(d['texs']):
            if nv[t] == 0: continue
            print('  t%-3d %4dx%-4d fmt %06x lista %d %6.1f KB %-7s vert %6d suelo %3.0f%% texel %5.3f m -> reduce desde %5.1f m'
                  % (t, x[0], x[1], x[2], x[3], x[5] / 1024, clase(x, cielo[t]), nv[t], 100 * suelo[t] / max(1, nv[t]),
                     lado[t], lado[t] * FX))
    return res


# ------------------------------------------------------------------ niveles y codificacion
def decodifica(d, w, h, fmt):
    """texeles RGBA (h, w, 4) de una textura del fichero sin mipmaps (VQ con su libro o 16 bits)"""
    if fmt & 0x100: return np.asarray(vq.decodifica(d, w, h, fmt & 0xff, ((fmt >> 16) & 0xff) + 1))
    return np.asarray(vq.decodifica16(d, w, h, fmt & 0xff))


def niveles(a, fmt, umbral=128):
    """niveles de mip de 1x1 a la base (promedio de 2x2, como modelo.niveles_mip). En ARGB1555 (recorte) el alfa de
    cada nivel se corta por el umbral que deja la misma fraccion de texeles opacos que la base (si no, las vallas y
    las hojas se vacian o se llenan de lejos)"""
    L = [a.astype(float)]
    while L[-1].shape[0] > 1:
        x = L[-1]; L.append((x[0::2, 0::2] + x[1::2, 0::2] + x[0::2, 1::2] + x[1::2, 1::2]) / 4)
    if fmt == 0:
        cob = float((a[..., 3] >= umbral).mean())
        for x in L[1:]:
            al = x[..., 3].ravel(); k = int(round(cob * al.size))
            if k <= 0: t = 256.0
            elif k >= al.size: t = -1.0
            else: t = 0.5 * (np.sort(al)[::-1][k - 1] + np.sort(al)[::-1][k])
            x[..., 3] = np.where(x[..., 3] > t, 255.0, 0.0)
    return [np.clip(np.rint(x), 0, 255).astype(np.uint8) for x in L[::-1]]


def _libro_rgba(d, n, fmt):
    """las n entradas del libro decodificadas: (n, 4 texeles, RGBA) en 0-255"""
    v = np.frombuffer(d[:8 * n], np.uint16).astype(np.uint32)
    if fmt == 1: c = [(v >> 11) * 255 / 31, ((v >> 5) & 63) * 255 / 63, (v & 31) * 255 / 31, v * 0 + 255]
    elif fmt == 0: c = [((v >> 10) & 31) * 255 / 31, ((v >> 5) & 31) * 255 / 31, (v & 31) * 255 / 31, (v >> 15) * 255]
    else: c = [((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17, (v >> 12) * 17]
    return np.stack(c, -1).astype(float).reshape(n, 4, 4)


def bloques(niv):
    """los bloques de 2x2 (k, 4 texeles twiddled, RGBA) de los niveles dados, seguidos (el de 1x1: su color x 4)"""
    P = [vq._twiddled(np.repeat(np.repeat(x, 2, 0), 2, 1) if x.shape[0] == 1 else x) for x in niv]
    return np.concatenate(P, 0).reshape(-1, 4, 4)


def _vec(B, fmt):
    can = 3 if fmt == 1 else 4; pes = np.tile(np.sqrt(vq.PESO[:can]), 4)
    V = B[:, :, :can].reshape(len(B), -1).copy()
    if fmt == 0: V[:, 3::4] *= 2
    return V * pes


def mip_mismo_libro(d, w, fmt):
    """VQ cuadrada sin mipmaps -> con mipmaps sin tocar la base: el mismo libro y los mismos indices de la base; los
    niveles de abajo (de la base decodificada) con la entrada mas cercana del libro. (datos, error RMS de los niveles
    nuevos frente a su promedio en 16 bits)"""
    n = ((fmt >> 16) & 0xff) + 1; fb = fmt & 0xff
    base = decodifica(d, w, w, fmt); niv = niveles(base, fb)[:-1]
    C = _libro_rgba(d, n, fb); B = bloques(niv)
    idx, _ = vq._asigna(_vec(B, fb), _vec(C, fb))
    _, ref = vq._cuantifica(B, fb); can = 3 if fb == 1 else 4
    err = float(np.sqrt(((C[idx] - ref)[:, :, :can] ** 2).mean()))
    datos = d[:8 * n] + (idx + 256 - n).astype(np.uint8).tobytes() + d[8 * n:8 * n + w * w // 4]
    return datos, err


def remuestrea(a, W, H, clamp=0):
    """la textura (h, w, 4) a W x H: el lado que baja, por promedio de 2^k; el que sube, por interpolacion lineal entre
    centros de texel (repitiendo la textura, o con su CLAMP: bits 0x1000 en u, 0x2000 en v): con el filtro bilineal se
    ve como la de antes"""
    x = a.astype(float)
    def eje(x, n, ax, cl):
        m = x.shape[ax]
        if n == m: return x
        if n < m:
            f = m // n; s = x.reshape(x.shape[:ax] + (n, f) + x.shape[ax + 1:]); return s.mean(ax + 1)
        p = (np.arange(n) + 0.5) * m / n - 0.5; i0 = np.floor(p).astype(int); t = p - i0
        i1 = i0 + 1
        if cl: i0, i1 = np.clip(i0, 0, m - 1), np.clip(i1, 0, m - 1)
        else: i0, i1 = i0 % m, i1 % m
        sh = [1] * x.ndim; sh[ax] = n; t = t.reshape(sh)
        return np.take(x, i0, ax) * (1 - t) + np.take(x, i1, ax) * t
    x = eje(x, W, 1, clamp & 0x1000); x = eje(x, H, 0, clamp & 0x2000)
    return np.clip(np.rint(x), 0, 255).astype(np.uint8)


def rms(a, b, fmt):
    can = 3 if fmt == 1 else 4
    return float(np.sqrt(((a[..., :can].astype(float) - b[..., :can]) ** 2).mean()))


def codifica(base, fmt, n0=256):
    """textura cuadrada (RGBA) con mipmaps de la PowerVR: (datos, formato nuevo sin los bits de arriba, error) en VQ
    si se puede (el libro de antes, n0 entradas, o el de vq.elige, o 256; el primero con error <= UMBRAL frente a la
    base y sus niveles en 16 bits) y si no en 16 bits (6 bytes de relleno y los niveles de 1x1 a la base, como
    modelo.datos_mip) solo las de menos de 32x32; None si la VQ no pasa"""
    import modelo
    S = base.shape[0]; fb = fmt & 0xff; niv = niveles(base, fb)
    d16 = b'\0' * 6 + b''.join(modelo.codifica16(x, fb) for x in niv)
    if S >= 32:
        nb = sum(max(1, x.shape[0] * x.shape[0] // 4) for x in niv)
        emin = 1e9
        for n in sorted({n0, 256 if nb >= 1024 else max(16, (nb // 4 + 3) & ~3), 256}):
            d, err = vq.codifica_mip(niv, fb, n, tiempo=1e9)
            if err <= UMBRAL and len(d) < len(d16): return d, fb | 0x300 | (n - 1) << 16, err
            emin = min(emin, err)
        return None, 0, emin                                        # (en 16 bits costaria 2,7 veces mas: sin mipmaps)
    return d16, fb | 0x200, 0.0


UMBRAL = float(os.environ.get('REMIP_UMBRAL', '9'))      # error RMS de la VQ con niveles (como vq.UMBRAL)
PERDIDA = float(os.environ.get('REMIP_PERDIDA', '3'))    # rectangular a un cuadrado menor: lo que puede perder (RMS)
D_MAX = 600.0                                             # m: la que no se reduce antes no gana nada


def candidatos(t, x, a, area, lado):
    """opciones de una textura sin mipmaps: [(lado del cuadrado, perdida)] de la de mas detalle a la de menos, y su
    detalle fino (lo que quita el primer nivel de mip: RMS)"""
    w, h, fmt = x[0], x[1], x[2]
    fb = fmt & 0xff
    m1 = remuestrea(remuestrea(a, max(1, w // 2), max(1, h // 2)), w, h, fmt)
    fino = rms(a, m1, fb)
    op = []
    S = max(w, h)
    while S >= min(w, h):
        if S >= 8 and not (S <= 32 < max(w, h)):            # (no a 32x32 o menos: demo/main.c apaga esas en el cielo)
            per = 0.0 if S >= max(w, h) else rms(a, remuestrea(remuestrea(a, min(w, S), min(h, S)), w, h, fmt), fb)
            op.append((S, per))
        S //= 2
    return op, fino


def sesgo(w, h, S, asp):
    """D adjust de la PowerVR (en cuartos: 4 = 1,0) para una textura de w x h que pasa a S x S. El nivel de mip lo
    elige el eje de texel mas fino en la pantalla; al estirar el lado corto por f = S / lado, ese eje tiene f veces mas
    texeles que antes y, si pasa a ser el mas fino, el otro se veria hasta log2 f niveles mas borroso que sin estirar.
    D = 2^-(lo que se adelanta), con asp = log2(texel en v / texel en u) en el mundo (uso): el eje estirado se adelanta
    log2 f - max(0, lo que ya era mas grueso que el otro). Redondeado a cuartos: 0,25 (2 niveles), 0,5 (1), 0,75 (0,4)"""
    if S == w == h or not np.isfinite(asp): return 4
    fu, fv = np.log2(S / w), np.log2(S / h)                    # lo que se estira cada eje (niveles)
    a = asp if fv > 0 else -asp                                 # (log2 del texel del eje estirado / el del otro)
    adel = float(np.clip(max(fu, fv) - max(a, 0.0), 0.0, max(fu, fv)))
    return int(np.clip(round(4 * 2 ** -adel), 1, 4))


FINO_MIN = 1.0                                            # sin detalle fino no hay centelleo que quitar
_cache = {}                                               # (huella, lado) -> (datos, formato, error)
DECIDIDO = {}                                             # huella -> lado elegido (0 sin mipmaps): la meta y su etapa
A32 = lambda n: (n + 31) & ~31


def huella(x, d):
    import hashlib
    return hashlib.sha1(struct.pack('<3I', x[0], x[1], x[2]) + d).digest()


def transforma(b, presupuesto, nombre='', informe=None):
    """bytes de un ORD7 -> (bytes nuevos o None, lineas del informe). presupuesto: bytes de VRAM (y del fichero) que
    puede crecer. Orden: lo que ahorra (rectangulares que pasan a un cuadrado menor sin perder mas de PERDIDA) y luego
    por detalle fino x area / coste; cada textura con la primera opcion que cabe (la de mas detalle)"""
    d = parte(b); nv, cielo, suelo, lado, area, asp = uso(d, True); texs = d['texs']; b0 = d['blob0']
    cand = []
    for t, x in enumerate(texs):
        w, h, fmt, li, off, tam = x
        if nv[t] == 0 or cielo[t] or fmt & 0x200 or (fmt & 0xff) > 2 or min(w, h) < 8: continue
        if not np.isfinite(lado[t]) or lado[t] * FX > D_MAX: continue
        dat = b[b0 + off:b0 + off + tam]; hu = huella(x, dat)
        a = decodifica(dat, w, h, fmt)
        op, fino = candidatos(t, x, a, area[t], lado[t])
        if fino < FINO_MIN: continue
        op = [o for o in op if o[1] <= PERDIDA]
        if hu in DECIDIDO:
            op = [o for o in op if o[0] == DECIDIDO[hu]]
            if not op: continue
        n0 = ((fmt >> 16) & 0xff) + 1 if fmt & 0x100 else 256
        def coste(S): return A32(8 * n0 + 1 + (S * S - 1) // 3 if S >= 32 else 6 + 2 * (S * S * 4 - 1) // 3) - A32(tam)
        if not op: continue
        S, per = min(op, key=lambda o: (coste(o[0]), -o[0]))     # la mas barata de las que se ven igual
        c0 = coste(S)
        pri = (0, c0) if c0 < 0 else (1, -(area[t] * fino) / max(c0, 64))
        cand.append((pri, t, hu, a, S, per, n0, coste))
    cand.sort(key=lambda c: (c[0], c[1]))
    nuevo = {}; queda = presupuesto; lin = []
    for pri, t, hu, a, S, per, n0, coste in cand:
        x = texs[t]; w, h, fmt = x[0], x[1], x[2]
        hecho = False
        if coste(S) <= max(queda, 0):
            k = (hu, S)
            if k not in _cache:
                base = a if S == w == h else remuestrea(a, S, S, fmt)
                _cache[k] = codifica(base, fmt, n0)
            dd, nf, err = _cache[k]
            if dd is None:
                lin.append('  t%-3d %3dx%-3d -> %3dx%-3d la VQ no pasa (error %.1f)' % (t, w, h, S, S, err))
                DECIDIDO.setdefault(hu, 0); continue
            c = A32(len(dd)) - A32(x[5])
            if c <= max(queda, 0):
                dj = sesgo(w, h, S, asp[t])
                nuevo[t] = (S, dd, nf | (fmt & 0xFC00) | (dj if dj != 4 else 0) << 24); queda -= c; hecho = True
                DECIDIDO.setdefault(hu, S)
                lin.append('  t%-3d %3dx%-3d -> %3dx%-3d %s err %4.1f perdida %3.1f D %.2f %+6.0f B' % (t, w, h, S, S, 'VQ ' if nf & 0x100 else '16b', err, per, dj / 4, c))
        if not hecho:
            DECIDIDO.setdefault(hu, 0)
            lin.append('  t%-3d %3dx%-3d sin sitio (%d B)' % (t, w, h, coste(S)))
    if not nuevo: return None, lin, 0
    blob = b''; tab = []
    for t, (w, h, fmt, li, off, tam) in enumerate(texs):
        if t in nuevo: S, dd, fmt = nuevo[t]; w = h = S
        else: dd = b[b0 + off:b0 + off + tam]
        dd += b'\0' * (-len(dd) % 32)
        tab.append(struct.pack('<6I', w, h, fmt, li, len(blob), len(dd))); blob += dd
    ntex = len(texs)
    nb = b[:56] + b''.join(tab) + b[56 + 24 * ntex:b0] + blob
    return nb, lin, presupuesto - queda


def lee(ruta):
    b = open(ruta, 'rb').read()
    if b[:4] == b'PAKD':
        import pak_union, mmap
        pak = os.path.join(os.path.dirname(ruta), b[20:36].rstrip(b'\0').decode())
        with open(pak, 'rb') as f:
            P = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ); b = pak_union.recompone(b, P); P.close()
    return b


# VRAM de cada fichero con lo nuevo (KB): hasta el tope de su clase y sin que ninguna pareja del curso (etapa + su
# union, union + la etapa de cada rama; CURSO.DAT y CURSOTA.DAT) pase de PAREJA, salvo las que ya pasaban (no crecen):
# el peor caso de la zona del curso no empeora (944 KB antes: IMPE/49 + BK198; la zona es de 1056 KB en las misiones de
# COAST 2 COAST con dos rivales). Las etapas primero (con las uniones de antes) y las uniones despues (con las etapas
# nuevas). tools/vram.py; docs/PARIDAD.md
TOPE_ETAPA = int(os.environ.get('REMIP_TOPE_ETAPA', '720')); TOPE_UNION = int(os.environ.get('REMIP_TOPE_UNION', '240'))
PAREJA = int(os.environ.get('REMIP_PAREJA', '944'))
PAQUETES = (('SEGS.PAK', 'SEG', ['--pak=SEGS.PAK', '--prefijo=SEG', '--tope=7842']), ('UNIONES.PAK', 'BK', []),
            ('UNIONESTA.PAK', 'BK', ['--pak=UNIONESTA.PAK']), ('UNIONESR.PAK', 'BK', ['--pak=UNIONESR.PAK']))
SECT = lambda n: -(-n // 2048) * 2048


def es_modelo(f, pre):
    return f.startswith(pre) and f[len(pre):-4].isdigit() and f.endswith('.DAT')


def tam_recompuesto(ruta):
    with open(ruta, 'rb') as f: b = f.read(8)
    return struct.unpack_from('<I', b, 4)[0] if b[:4] == b'PAKD' else os.path.getsize(ruta)


def datos(E, D, W, solo=None):
    """las etapas y uniones de la carpeta de datos E con mipmaps nuevos; los ficheros que cambian (los paquetes y
    descriptores nuevos, los sueltos) van a D (E no se toca). Primero las etapas (en orden: las del modo OutRun
    primero) y despues las uniones, para que la meta haga con cada textura lo mismo que su etapa (DECIDIDO). Tope de
    cada fichero: el de la VRAM (TOPE_ETAPA / TOPE_UNION) y el del hueco de la RAM (el mayor fichero de su clase no
    crece: demo/main.c curso_huecos reserva lo que ocupa)"""
    import shutil, subprocess, pak_union, mmap, time
    os.makedirs(D, exist_ok=True); os.makedirs(W, exist_ok=True)
    num = lambda f: int(''.join(c for c in f if c.isdigit()))
    H = {pre: max(SECT(tam_recompuesto(os.path.join(E, f))) for f in os.listdir(E) if es_modelo(f, pre)) for pre in ('SEG', 'BK')}
    print('huecos: etapa %d KB, union %d KB' % (H['SEG'] >> 10, H['BK'] >> 10))
    import vram as vr
    pareja = {}                                             # fichero -> los que conviven con el en la zona
    for cf in ('CURSO.DAT', 'CURSOTA.DAT'):
        if not os.path.isfile(os.path.join(E, cf)): continue
        for curs in vr.curso(E, cf).values():
            for c in curs:
                s_, u_ = 'SEG%02d.DAT' % c[1], 'BK%02d.DAT' % c[5]
                for h in [c] + [curs[k] for k in (c[3], c[4]) if 0 <= k < len(curs)]:
                    pareja.setdefault(u_, set()).add('SEG%02d.DAT' % h[1]); pareja.setdefault('SEG%02d.DAT' % h[1], set()).add(u_)
    zona = {}                                               # VRAM de cada fichero (la de antes; la nueva al cambiarlo)
    for f in os.listdir(E):
        if es_modelo(f, 'SEG') or es_modelo(f, 'BK'):
            zona[f] = sum(A32(x[5]) for x in parte(lee(os.path.join(E, f)))['texs'])
    tot = {'SEG': [0, 0, 0], 'BK': [0, 0, 0]}
    def tr(f, b):
        pre = 'SEG' if f.startswith('SEG') else 'BK'
        if solo is not None and f[:-4] not in solo: return b
        t0 = time.time(); d = parte(b)
        vram = sum(A32(x[5]) for x in d['texs'])
        tope = (TOPE_ETAPA if pre == 'SEG' else TOPE_UNION) << 10
        otro = max([zona.get(x, 0) for x in pareja.get(f, ())] or [0])     # la mayor con la que convive
        pres = max(0, min(tope - vram, (PAREJA << 10) - otro - vram, H[pre] - SECT(len(b))))
        nb, lin, usado = transforma(b, pres, f)
        n = sum('->' in l and 'no pasa' not in l for l in lin)
        print('%s: VRAM %d KB, presupuesto %d KB, %d con mipmaps nuevos, %+.1f KB, %d sin sitio, %d sin VQ, %.0f s'
              % (f, vram >> 10, pres >> 10, n, usado / 1024, sum('sin sitio' in l for l in lin),
                 sum('no pasa' in l for l in lin), time.time() - t0))
        if os.environ.get('REMIP_DETALLE'): print('\n'.join(lin))
        zona[f] = vram + usado
        tot[pre][0] += 1; tot[pre][1] += n; tot[pre][2] += usado
        return nb if nb is not None else b
    sueltos = sorted((f for f in os.listdir(E) if (es_modelo(f, 'SEG') or es_modelo(f, 'BK'))), key=lambda f: (f[0] != 'S', num(f)))
    for pak, pre, op in PAQUETES:
        P = None
        if os.path.isfile(os.path.join(E, pak)):
            fp = open(os.path.join(E, pak), 'rb'); P = mmap.mmap(fp.fileno(), 0, access=mmap.ACCESS_READ)
        e, s = os.path.join(W, 'ent_' + pak), os.path.join(W, 'sal_' + pak)
        for x in (e, s):
            if os.path.isdir(x): shutil.rmtree(x)
            os.makedirs(x)
        cambia = 0
        for f in sueltos:
            if not es_modelo(f, pre): continue
            b = open(os.path.join(E, f), 'rb').read()
            if b[:4] == b'ORD7':
                if pak != 'SEGS.PAK' and pak != 'UNIONES.PAK': continue      # (los sueltos, una vez por clase)
                nb = tr(f, b)
                if nb != b: open(os.path.join(D, f), 'wb').write(nb)
                continue
            if b[:4] != b'PAKD' or b[20:36].rstrip(b'\0').decode() != pak or P is None: continue
            b = pak_union.recompone(b, P); nb = tr(f, b); cambia += nb != b
            open(os.path.join(e, f), 'wb').write(nb)
        if P is not None: P.close(); fp.close()
        print('%s: %d cambian' % (pak, cambia))
        if cambia:
            subprocess.run([sys.executable, '-u', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pak_union.py'), e, s] + op,
                           check=True, stdout=subprocess.DEVNULL)
            for f in os.listdir(s): shutil.copyfile(os.path.join(s, f), os.path.join(D, f))
        shutil.rmtree(e); shutil.rmtree(s)
    for pre, (n, k, u) in tot.items():
        print('%s: %d ficheros, %d texturas con mipmaps nuevos, %+.0f KB de VRAM en total' % (pre, n, k, u / 1024))


def main():
    a = sys.argv[1:]
    if a and a[0] == '--datos':
        solo = None
        if '--solo' in a: i = a.index('--solo'); solo = set(a[i + 1].split(',')); a = a[:i] + a[i + 2:]
        return datos(a[1], a[2], a[3], solo)
    if not a or a[0] != '--info': raise SystemExit(__doc__)
    det = '-v' in a; a = [x for x in a[1:] if x != '-v']
    if os.path.isdir(a[0]):
        rutas = [os.path.join(a[0], x if x.endswith('.DAT') else x + '.DAT') for x in a[1:]]
    else: rutas = a
    C = ('mip', 'cuad_vq', 'cuad_16', 'rect', 'cielo')
    print('%-10s' % 'fichero' + ''.join('%14s' % c for c in C))
    for r in rutas:
        nm = os.path.basename(r)
        if det: print(nm)
        res = info(nm, lee(r), det)
        print('%-10s' % nm[:-4] + ''.join('%5d %6.0f KB' % tuple(res.get(c, (0, 0))) for c in C))


if __name__ == '__main__':
    main()
