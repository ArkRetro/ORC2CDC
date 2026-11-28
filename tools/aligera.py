"""aligera.py: menos decoracion en todas las etapas y los edificios de PALM a la mitad (64->32, 128->64) (fps3; DESVIACION pedida por el
usuario para ganar fps; ver docs/PARIDAD.md, "fps3: decoracion -15 % y edificios de PALM 32x32").

 aligera.py --datos entrada/ salida/ trabajo/ [--solo SEG00,...] [--sin-decor] [--sin-palm] [--sin-recorte]
   [--solo-recorte]: sobre una carpeta de
   datos ya hecha (etapas sueltas o en SEGS.PAK, como tools/remip.py); los ficheros que cambian (SEGS.PAK con todos sus
   descriptores SEGnn.DAT, las sueltas, y los paquetes de uniones si alguna union cambia) van a salida/ (puede ser la
   misma). tools/regenera.py lo pasa al final, una vez (despues de remip.py).
 aligera.py --info entrada/ [SEGnn ...]: lo que haria, sin escribir nada.

1. Decoracion (las 62 SEGnn.DAT): fuera el 15 % (DECOR_PCT) de los arboles, arbustos y palmeras que quedan a la vista
   y de las vallas publicitarias, repartido a lo largo de la ruta (ordenados por su punto de la ruta, uno de cada
   1 / 0,15 seguidos; en el mismo sitio, por un hash de la posicion): determinista, siempre los mismos. Se quitan
   poniendo a 0 los vertices de sus mallas (la demo no dibuja las mallas de menos de 3), con su version lejana
   (lod_veg.py) y su tronco si va en otro nodo (como decor_reduce).
   - vegetacion: los nodos que demo/main.c decor_reduce toma por vegetacion (tabla demo/decor_tabla.h, la misma regla
     de medidas) y que no quita ya el (su hash, DECOR_VEG_N de DECOR_VEG): el 15 % es de lo que se ve hoy.
   - vallas: nodos con una malla cuya textura se parece (firma de 16x8 colores, RMS < ANUNCIO_UMBRAL; con mipmaps o sin
     ellos, en espejo tambien) a un anuncio de REF_ANUNCIO (los patrocinadores de PALM y TULI, que se repiten en casi
     todas las etapas), de 8 a 64 vertices, 16 m de planta como mucho y 2,5 m de alto o mas (la valla entera con su marco: no los paneles sueltos de una
     estructura mayor), fuera del asfalto (mas de 1 m por fuera) y a mas de PROTEGE m del principio (checkpoint y
     salida) y del final (bifurcacion o meta) de la ruta.
   No se toca nada mas: ni carteles de la carretera (flechas, SLOW DOWN, CHECK POINT), ni farolas, ni publico, ni
   edificios. Los objetos de las peticiones y de los campeonatos (HAOBJ, CAMPOBJ, INFARTOP...) y la meta (META.DAT) son
   otros ficheros.
2. PALM (SEG00): las texturas de las fachadas de los edificios (EDIF_PALM, elegidas a mano en la hoja de sus
   texturas: ni carretera, ni suelo, ni muros, ni cielo, ni vegetacion, ni carteles ni anuncios) a la mitad de lado
   (lado_de: las 17 de 64x64 a 32x32; las 13 de 128, rejillas de ventanas que a 32 se funden, a 64x64), cuadradas
   y con mipmaps (la PowerVR solo los admite cuadrados): la imagen de ahora (el nivel mayor si ya tenia mipmaps)
   reducida por promedio (o estirada por interpolacion si un lado era menor), en VQ con niveles y libro reducido
   (remip.codifica: el primer libro con error <= remip.UMBRAL) o, si la VQ no pasa, en 16 bits con niveles. El D adjust
   (bits 24-27) se queda; si era rectangular sin el, el de remip.sesgo. Las uniones del curso de PALM (CURSO.DAT y
   CURSOTA.DAT) con la misma textura (mismos bytes) la cambian igual, para que la VRAM la siga compartiendo.
   Idempotente: si en SEG00 las de EDIF_PALM ya estan reducidas (ninguna de mas de 64 y alguna de 32), no hace nada (la decoracion se
   aplico con ella); con --sin-palm no hay esa marca y regenera.py lleva la suya (aligera.hecho).

3. Recorte (DESVIACION pedida por el usuario; docs/PARIDAD.md, "Recorte: farolas, carteles y muros"), en las 62
   etapas (las de OutRun 2 y OR2SP y sus copias de las misiones) y en todas las uniones con la bifurcacion:
   - carteles de la carretera (carteles): fuera el 50 % (RECORTE_PCT) de las flechas de curva, SLOW DOWN y paneles de
     flechas rojas (tablero: textura parecida por la firma normalizada a REF_CARTEL y no a la pancarta CHECK POINT, REF_NO_CARTEL),
     repartido a lo largo de la ruta. Un cartel es su nodo entero (OR2SP: un nodo por cartel, con su version lejana) o,
     en el nodo de los objetos de la etapa (OutRun 2: obj_course_obj_cs_<zona>, todos los carteles y la pancarta en un
     nodo), sus tableros y sus postes. Se quedan CHECK POINT, la salida y la meta y lo que este a menos de PROTEGE m del
     principio o del final de la ruta (el cartel de direccion de la bifurcacion va en la union: no se toca) y lo de mas
     de CARTEL_PLANTA m de ancho (porticos). Ojo: el campo 'cartel' de los nodos (>= 0) es la matriz de un cuadrilatero
     orientado a la camara (los arboles lejanos de lod_veg.py), no un cartel de la carretera.
   - farolas de las uniones (farolas): nodos con las dos texturas de FAROLA_BK de BK00 (por contenido: la bifurcacion es
     la misma en todas las uniones) de mas de 8 m de alto; fuera una de cada dos por ruta (la de la union o la de
     BKD.DAT, la rama derecha) y lado.
   - muros de las uniones: las texturas MUROS_BK de BK00 (muro de contencion del borde y muro bajo de hormigon) a
     LADO_MURO x LADO_MURO con mipmaps (como las de PALM) en todas las uniones y etapas con los mismos bytes.
   Idempotente: si los muros de BK00 ya son de LADO_MURO (ya_recortado), el recorte no se repite.

Los numeros de etapa y textura son los de la conversion de SLES-53998 de este repositorio (como decor_tabla.h)."""
import os, re, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import remip, vq

DECOR_PCT = float(os.environ.get('ALIGERA_PCT', '15')) / 100
DECOR_VEG, DECOR_VEG_N = 20, 6                                   # (demo/main.c decor_reduce)
PROTEGE = 100.0                                                  # m del principio y del final de la ruta
ANUNCIO_UMBRAL = 15.0                                            # (los anuncios dan <= 8,3; lo siguiente 23,8)
REF_ANUNCIO = [(0, t) for t in (96, 97, 99, 100, 101, 102, 122, 93, 127, 129, 130, 149, 150, 151)] + \
              [(12, t) for t in (119, 120, 121, 122, 124, 129, 130)]
# fachadas de PALM (SEG00): ventanas, paredes de los edificios, rascacielos del fondo, almacenes del puerto
EDIF_PALM = [14, 40, 41, 44, 45, 48, 50, 53, 54, 62, 63, 64, 66, 67, 69, 72, 73, 77, 80, 81, 82, 83, 85, 86, 87, 88,
             89, 90, 91, 124]
LADO = 32                                                        # las de 64 de lado
LADO_MAX = 64                                                    # las de 128 (rejillas de ventanas): a 32 no caben
# recorte (DESVIACION pedida por el usuario: farolas de las bifurcaciones y carteles de la carretera -50 %, muros de
# las bifurcaciones a 32x32; docs/PARIDAD.md, "Recorte: farolas, carteles y muros")
RECORTE_PCT = 0.5
REF_CARTEL = [(0, 165), (0, 166), (0, 121), (3, 142), (5, 160), (17, 111), (17, 112), (17, 131)]   # curva, SLOW DOWN, flechas
REF_NO_CARTEL = [(3, 144), (1, 162)]                             # CHECK POINT (se queda)
CARTEL_UMBRAL = 20.0                                             # (firma normalizada: carteles <= 12,7; lo demas >= 27)
CARTEL_PLANTA, CARTEL_ALTO = 30.0, 16.0                         # un cartel (nodo propio): como mucho
FAROLA_BK = (7, 8)                                               # BK00: cabeza y poste de la farola (verde, 15 m)
MUROS_BK = [1, 12]                                                 # BK00: texturas de los muros (a LADO_MURO)
LADO_MURO = 32
MURO_UMBRAL = 3.0                                                # (la misma imagen: <= 0,8; lo siguiente, 19)


def lado_de(w, h):
    """la mitad del lado mayor, entre LADO y LADO_MAX: las de 64 a 32 y las de 128 a 64 (a 32 las ventanas de los
    rascacielos, de 3-4 texeles de periodo, se funden en una mancha; auditoria del usuario: "no tienen detalle")"""
    return min(LADO_MAX, max(LADO, max(w, h) // 2))
NODO = np.dtype([('c', '<f4', 3), ('r', '<f4'), ('fl', '<u4'), ('cartel', '<i4'), ('rel', '<u4', 2)])
NODO_LEJ = 0x20


def tabla_decor():
    """{etapa: {textura: tipo}} de demo/decor_tabla.h (o de una copia junto a este fichero: el programa de Windows)"""
    aqui = os.path.dirname(os.path.abspath(__file__))
    for r in (os.path.join(aqui, 'decor_tabla.h'), os.path.join(aqui, '..', 'demo', 'decor_tabla.h')):
        if os.path.isfile(r):
            T = {}
            for s, t, k in re.findall(r'\{\s*(\d+),\s*(\d+),\s*(\d+)\s*\}', open(r).read()): T.setdefault(int(s), {})[int(t)] = int(k)
            return T
    raise SystemExit('aligera.py: no encuentro decor_tabla.h (demo/ o junto a tools/)')


def parte(b):
    d = remip.parte(b); h = d['h']
    nm, nv, nr, pr, nt = h[0], h[1], h[2], h[3], h[4]
    o = 56 + 24 * nt + 32 * nm + 24 * nv
    d['R'] = np.frombuffer(b, '<f4', nr * pr, o).reshape(nr, pr); o += 4 * nr * pr
    d['N'] = np.frombuffer(b, NODO, h[10], o); d['oM'] = 56 + 24 * nt
    return d


def base(dat, w, h, fmt):
    """RGBA del nivel mayor de una textura (con o sin mipmaps, VQ o 16 bits)"""
    if fmt & 0x200:
        L = w.bit_length() - 1
        if fmt & 0x100:
            n = ((fmt >> 16) & 0xff) + 1; b0 = 1 + ((1 << (2 * (L - 1))) - 1) // 3 if L else 0
            return np.asarray(vq.decodifica(dat[:8 * n] + dat[8 * n + b0:8 * n + b0 + w * h // 4], w, h, fmt & 0xff, n))
        o = 6 + 2 * (((1 << (2 * L)) - 1) // 3)
        return np.asarray(vq.decodifica16(dat[o:o + 2 * w * h], w, h, fmt & 0xff))
    return remip.decodifica(dat, w, h, fmt)


def imagen(b, d, t):
    w, h, fmt, li, off, tam = d['texs'][t]
    return base(b[d['blob0'] + off:d['blob0'] + off + tam], w, h, fmt)


def firma(a):
    from PIL import Image
    im = Image.fromarray(np.ascontiguousarray(a), 'RGBA'); bg = Image.new('RGBA', im.size, (128, 128, 128, 255)); bg.alpha_composite(im)
    return np.asarray(bg.convert('RGB').resize((16, 8), Image.BILINEAR), float)


def firma_n(a):
    """la firma normalizada (media y contraste): los carteles de las zonas oscuras (CAST, FORE, GHOS, FLOR) son los
    mismos con otra luz"""
    f = firma(a); return (f - f.mean((0, 1))) / (f.std() + 1e-6) * 40.0


def h32(x, z, a, b):
    """el hash de demo/main.c (decor_reduce) de floorf(x), floorf(z)"""
    m = 0xFFFFFFFF
    h = ((int(np.floor(x)) & m) * a & m) ^ ((int(np.floor(z)) & m) * b & m)
    h ^= h >> 13; h = h * 0x5bd1e995 & m; h ^= h >> 15
    return h


def medidas(d):
    """por nodo (como decor_reduce): caja (float32), vertices, vertices de recorte, por tipo de la tabla"""
    M, V, N = d['M'], d['V'], d['N']; nn = len(N); ini = d['inicio']; P = V['p']
    lo = np.full((nn, 3), np.float32(1e30), np.float32); hi = np.full((nn, 3), np.float32(-1e30), np.float32)
    cv = np.zeros((nn, 2), np.int64)
    k3 = min(ini[3], len(M)); nodo = M['nodo'][:k3].astype(np.int64); n = M['n'][:k3].astype(np.int64); i0 = M['i0'][:k3].astype(np.int64)
    ok = (nodo >= 0) & (nodo < nn) & (n >= 3) & (i0 + n <= len(P))
    for k in np.nonzero(ok)[0]:
        p = P[i0[k]:i0[k] + n[k]]; j = nodo[k]
        lo[j] = np.minimum(lo[j], p.min(0)); hi[j] = np.maximum(hi[j], p.max(0))
    rec = (np.arange(k3) >= ini[1]) & (np.arange(k3) < ini[2])
    np.add.at(cv[:, 0], nodo[ok], n[ok]); np.add.at(cv[:, 1], nodo[ok & rec], n[ok & rec])
    return lo, hi, cv, ok, rec, nodo, n


def ruta_de(d, cx, cz):
    """punto de la ruta mas cercano (cada 4 m): s, lateral con signo, asfalto de ese lado"""
    R = d['R']; pr = R.shape[1]
    if len(R) < 3 or pr < 5: return None
    dd = (R[:, 0] - cx) ** 2 + (R[:, 2] - cz) ** 2; k = int(np.argmin(dd)); q = R[k + 1 if k + 1 < len(R) else k - 1]
    dx, dz = q[0] - R[k, 0], q[2] - R[k, 2]
    if k + 1 >= len(R): dx, dz = -dx, -dz
    nr = np.hypot(dx, dz) + 1e-6; l = ((cx - R[k, 0]) * dz - (cz - R[k, 2]) * dx) / nr
    return k * 4.0, float(l), float(max(R[k, 3] if l < 0 else R[k, 4], 3.0))


def reparto(cands, pct):
    """[(s, hash, clave)] -> las claves que se quitan: uno de cada 1 / pct seguidos a lo largo de la ruta"""
    cands = sorted(cands); fuera = []
    for i, (s, h, c) in enumerate(cands):
        if int((i + 1) * pct + 0.5) > int(i * pct + 0.5): fuera.append(c)
    return fuera


def decor(b, d, seg, tdec, refs, info=None):
    """las mallas que se quitan de la etapa seg: (lista de mallas, vegetacion vista, quitada, vallas, quitadas)"""
    M, N, texs = d['M'], d['N'], d['texs']; nn = len(N); ini = d['inicio']; ntex = len(texs)
    if nn == 0 or ini[3] > len(M): return [], 0, 0, 0, 0
    lo, hi, cv, ok, rec, nodo, n = medidas(d)
    tt = np.zeros(max(ntex, 1), np.int64)
    for t, k in tdec.get(seg, {}).items():
        if t < ntex: tt[t] = k
    tex = M['ti'][:len(ok)].astype(np.int64); tv = np.where((tex >= 0) & (tex < ntex), tt[np.clip(tex, 0, max(ntex - 1, 0))], 0)
    ct = np.zeros((nn, 3), np.int64)
    np.add.at(ct[:, 0], nodo[ok & rec & (tv == 1)], n[ok & rec & (tv == 1)])
    ext = hi - lo; fp = np.maximum(ext[:, 0], ext[:, 2]); ay = ext[:, 1]
    lej = (N['fl'] & NODO_LEJ) != 0; cart = N['cartel'] >= 0
    veg = (cv[:, 0] > 0) & ~cart & ~lej & (fp < 25) & (ay > 1.5) & (ay < 35) & (2 * ct[:, 0] > cv[:, 0])
    cx = (lo[:, 0] + hi[:, 0]) * np.float32(0.5); cz = (lo[:, 2] + hi[:, 2]) * np.float32(0.5)
    largo = len(d['R']) * 4.0
    quita = set(); cand = []; nveg = 0
    for i in np.nonzero(veg)[0]:
        if h32(cx[i], cz[i], 73856093, 19349663) % DECOR_VEG < DECOR_VEG_N: continue   # (ya lo quita la demo)
        nveg += 1; r = ruta_de(d, float(cx[i]), float(cz[i]))
        cand.append((r[0] if r else 0.0, h32(cx[i], cz[i], 2246822519, 3266489917), int(i)))
    fveg = reparto(cand, DECOR_PCT)
    # vallas: nodos con una malla de un anuncio (por la firma de su textura)
    anun = set()
    if refs is not None:
        usadas = set(int(t) for t in tex[ok] if 0 <= t < ntex)
        for t in usadas:
            w, h = texs[t][:2]
            if w < 32 or h < 16: continue
            if np.sqrt(((refs - firma(imagen(b, d, t))) ** 2).mean(axis=(1, 2, 3))).min() < ANUNCIO_UMBRAL: anun.add(t)
    ad = np.zeros(nn, bool)
    for k in np.nonzero(ok)[0]:
        if int(tex[k]) in anun: ad[nodo[k]] = True
    cand = []; nval = 0
    for i in np.nonzero(ad & ~veg & ~cart & ~lej & (cv[:, 0] >= 8) & (cv[:, 0] <= 64) & (fp <= 16) & (ay >= 2.5))[0]:
        r = ruta_de(d, float(cx[i]), float(cz[i]))
        if r is None or abs(r[1]) < r[2] + 1.0 or r[0] < PROTEGE or r[0] > largo - PROTEGE: continue
        nval += 1; cand.append((r[0], h32(cx[i], cz[i], 83492791, 2654435761), int(i)))
        if info is not None and os.environ.get('ALIGERA_VALLAS'):
            info.append('  valla nodo %d v %d planta %.1f alto %.1f s %.0f lateral %.1f' % (i, cv[i, 0], fp[i], ay[i], r[0], r[1]))
    fval = reparto(cand, DECOR_PCT)
    for i in fveg + fval:
        quita.add(i); c = int(N['cartel'][i])
        if c <= -2 and -2 - c < nn: quita.add(-2 - c)              # (su version lejana)
    for i in fveg:                                                 # el tronco, como decor_reduce
        for j in range(nn):
            if j == i or veg[j] or j in quita or not cv[j, 0] or 2 * cv[j, 1] > cv[j, 0] or fp[j] > 2.5 or cart[j]: continue
            if not (lo[i, 0] - 1 <= cx[j] <= hi[i, 0] + 1 and lo[i, 2] - 1 <= cz[j] <= hi[i, 2] + 1): continue
            if lo[j, 1] > lo[i, 1] + 0.5 or hi[j, 1] < lo[i, 1] - 1 or hi[j, 1] > hi[i, 1] + 1: continue
            quita.add(j)
    mallas = [k for k in range(len(M)) if 0 <= int(M['nodo'][k]) < nn and int(M['nodo'][k]) in quita and int(M['n'][k]) > 0]
    if info is not None: info.append('  anuncios: texturas %s' % sorted(anun))
    return mallas, nveg, len(fveg), nval, len(fval)


def edificios(b, d, lista, hechas=None, lado_fijo=None):
    """{textura: (w, h, fmt, datos)} de las de lista a LADO x LADO (las que ya lo son no cambian); hechas: huella ->
    resultado (para las uniones)"""
    import modelo
    texs = d['texs']; nv, cielo, suelo, lado, area, asp = remip.uso(d, True); res = {}
    for t in lista:
        if t >= len(texs): continue
        w, h, fmt, li, off, tam = texs[t]
        L = lado_fijo or lado_de(w, h)
        if (w <= L and h <= L) or (fmt & 0xff) > 2: continue
        dat = b[d['blob0'] + off:d['blob0'] + off + tam]; hu = remip.huella(texs[t], dat)
        if hechas is not None and hu in hechas: res[t] = hechas[hu]; continue
        a = base(dat, w, h, fmt); fb = fmt & 0xff
        a32 = remip.remuestrea(a, L, L, fmt)
        n0 = ((fmt >> 16) & 0xff) + 1 if fmt & 0x100 else 256
        dd, nf, err = remip.codifica(a32, fmt, n0)
        if dd is None: dd, nf = modelo.datos_mip(a32, fb), fb | 0x200
        dj = (fmt >> 24) & 0xF
        if not dj and w != h: dj = remip.sesgo(w, h, L, asp[t]); dj = 0 if dj == 4 else dj
        res[t] = (L, L, (nf & 0x00FF03FF) | (fmt & 0xFC00) | dj << 24, dd, err)
        if hechas is not None and hu not in hechas: hechas[hu] = res[t]
    return res


def mallas_caja(d, ok):
    """por malla dibujable: caja (min, max) de sus vertices"""
    M, P = d['M'], d['V']['p']; k3 = len(ok)
    lo = np.zeros((k3, 3), np.float32); hi = np.zeros((k3, 3), np.float32)
    for k in np.nonzero(ok)[0]:
        p = P[int(M['i0'][k]):int(M['i0'][k]) + int(M['n'][k])]; lo[k] = p.min(0); hi[k] = p.max(0)
    return lo, hi


def carteles(b, d, refs, info=None):
    """las mallas de los carteles de la carretera que se quitan (RECORTE_PCT): (mallas, carteles, quitados).
    Tablero: malla con una textura que se parece (firma, como las vallas) a una de REF_CARTEL (flecha de curva,
    SLOW DOWN, flechas rojas) y no a CHECK POINT (REF_NO_CARTEL). Cartel: el nodo entero si es pequeno (CARTEL_PLANTA
    x CARTEL_ALTO; las etapas de OR2SP, un nodo por cartel, con su version lejana) o, en el nodo de los objetos de la
    etapa (obj_course_obj_cs_<zona>: todos los carteles de la etapa y la pancarta en un nodo), los tableros a menos de
    8 m entre si y los postes (mallas de 3 m de planta como mucho) a menos de 7 m. Se quedan: lo que lleva CHECK POINT,
    lo que esta a menos de PROTEGE m del principio (salida, checkpoint) o del final (bifurcacion, meta) de la ruta y lo
    de mas de CARTEL_PLANTA m de ancho (porticos y arcos que cruzan la carretera)."""
    M, N, texs = d['M'], d['N'], d['texs']; nn = len(N); ntex = len(texs); ini = d['inicio']
    if nn == 0 or refs is None or ini[3] > len(M) or len(d['R']) < 3: return [], 0, 0
    rsi, rno = refs
    lo, hi, cv, ok, rec, nodo, n = medidas(d)
    tex = M['ti'][:len(ok)].astype(np.int64)
    tab, no = set(), set()
    for t in set(int(t) for t in tex[ok] if 0 <= t < ntex):
        w, h = texs[t][:2]
        if w < 16 or h < 8: continue
        f = firma_n(imagen(b, d, t))
        es = np.sqrt(((rsi - f) ** 2).mean(axis=(1, 2, 3))).min(); nc = np.sqrt(((rno - f) ** 2).mean(axis=(1, 2, 3))).min()
        if nc < CARTEL_UMBRAL and nc <= es: no.add(t)
        elif es < CARTEL_UMBRAL: tab.add(t)
    if not tab: return [], 0, 0
    mlo, mhi = mallas_caja(d, ok); mc = (mlo + mhi) * 0.5; ext = hi - lo
    lej = (N['fl'] & NODO_LEJ) != 0; largo = len(d['R']) * 4.0
    por_nodo = {}
    for k in np.nonzero(ok)[0]: por_nodo.setdefault(int(nodo[k]), []).append(int(k))
    unidades = []                                                  # (mallas, nodos enteros)
    for j in sorted(set(int(nodo[k]) for k in np.nonzero(ok)[0] if int(tex[k]) in tab)):
        ks = por_nodo[j]
        if any(int(tex[k]) in no for k in ks) and max(ext[j][0], ext[j][2]) <= CARTEL_PLANTA: continue
        if max(ext[j][0], ext[j][2]) <= CARTEL_PLANTA and ext[j][1] <= CARTEL_ALTO and not lej[j] and N['cartel'][j] < 0:
            unidades.append((ks, [j])); continue
        tb = [k for k in ks if int(tex[k]) in tab]; grupos = []
        for k in tb:                                               # tableros a menos de 8 m: un cartel
            for g in grupos:
                if min(np.hypot(mc[k][0] - mc[q][0], mc[k][2] - mc[q][2]) for q in g) < 8.0: g.append(k); break
            else: grupos.append([k])
        for g in grupos:
            cx, cz = np.mean([mc[q][0] for q in g]), np.mean([mc[q][2] for q in g]); ms = list(g)
            for k in ks:
                if k in ms or int(tex[k]) in no or int(tex[k]) in tab: continue
                e = mhi[k] - mlo[k]
                if max(e[0], e[2]) <= 3.0 and e[1] <= CARTEL_ALTO and np.hypot(mc[k][0] - cx, mc[k][2] - cz) < 7.0: ms.append(k)
            unidades.append((ms, []))
    cand = []
    for u, (ms, js) in enumerate(unidades):
        a = np.min([mlo[k] for k in ms], 0); z = np.max([mhi[k] for k in ms], 0)
        if max(z[0] - a[0], z[2] - a[2]) > CARTEL_PLANTA: continue
        cx, cz = float(a[0] + z[0]) * 0.5, float(a[2] + z[2]) * 0.5; r = ruta_de(d, cx, cz)
        if r is None or r[0] < PROTEGE or r[0] > largo - PROTEGE: continue
        cand.append((r[0], h32(cx, cz, 40503, 2654435761), u))
        if info is not None and os.environ.get('ALIGERA_CARTELES'):
            info.append('  cartel %s s %.0f lateral %.1f mallas %d texturas %s' % ('nodo %d' % js[0] if js else 'objetos', r[0], r[1], len(ms), sorted(set(int(tex[k]) for k in ms))))
    fuera = reparto(cand, RECORTE_PCT); quita = set(); qn = set()
    for u in fuera:
        ms, js = unidades[u]; quita.update(ms)
        for j in js:
            qn.add(j); c = int(N['cartel'][j])
            if c <= -2 and -2 - c < nn: qn.add(-2 - c)              # (su version lejana)
    mallas = sorted(set(k for k in quita if int(M['n'][k]) > 0) |
                    set(k for k in range(len(M)) if int(M['nodo'][k]) in qn and int(M['n'][k]) > 0))
    if info is not None: info.append('  carteles: texturas %s (CHECK POINT %s)' % (sorted(tab), sorted(no)))
    return mallas, len(cand), len(fuera)


def refs_cartel(E):
    def lista(L):
        R = []; cache = {}
        for s, t in L:
            f = os.path.join(E, 'SEG%02d.DAT' % s)
            if not os.path.isfile(f): continue
            if s not in cache: bb = remip.lee(f); cache[s] = (bb, parte(bb))
            bb, d = cache[s]
            if t < len(d['texs']): a = imagen(bb, d, t); R.append(firma_n(a)); R.append(firma_n(a[:, ::-1]))
        return np.array(R) if R else None
    si, no = lista(REF_CARTEL), lista(REF_NO_CARTEL)
    return None if si is None or no is None else (si, no)


def huellas(b, d):
    return [remip.huella(x, b[d['blob0'] + x[4]:d['blob0'] + x[4] + x[5]]) for x in d['texs']]


def farolas(b, d, hfar, rder, info=None):
    """las mallas de las farolas de la union que se quitan (RECORTE_PCT): (mallas, farolas, quitadas). Farola: nodo con
    mallas de las dos texturas de FAROLA_BK de BK00 (por contenido: la misma bifurcacion en todas las uniones) de mas de
    8 m de alto; repartido por ruta (la de la union, tronco y rama izquierda, o la de BKD.DAT, rama derecha: la mas
    cercana) y lado, una de cada 1 / RECORTE_PCT seguidas."""
    M, N = d['M'], d['N']; nn = len(N)
    if nn == 0 or not hfar or d['inicio'][3] > len(M): return [], 0, 0
    hs = huellas(b, d); ft = {t: hfar.index(h) for t, h in enumerate(hs) if h in hfar}
    if len(set(ft.values())) < len(hfar): return [], 0, 0
    lo, hi, cv, ok, rec, nodo, n = medidas(d); tex = M['ti'][:len(ok)]
    tiene = np.zeros((nn, len(hfar)), bool)
    for k in np.nonzero(ok)[0]:
        if int(tex[k]) in ft: tiene[nodo[k], ft[int(tex[k])]] = True
    ext = hi - lo; grupos = {}
    for j in np.nonzero(tiene.all(1) & (ext[:, 1] > 8.0))[0]:
        cx, cz = float(lo[j, 0] + hi[j, 0]) * 0.5, float(lo[j, 2] + hi[j, 2]) * 0.5
        rs = [r for r in (ruta_de(d, cx, cz), ruta_de({'R': rder}, cx, cz) if rder is not None else None)]
        i = min((q for q in range(len(rs)) if rs[q] is not None), key=lambda q: abs(rs[q][1]), default=None)
        s, l = (rs[i][0], rs[i][1]) if i is not None else (0.0, 0.0)
        grupos.setdefault((i, l < 0), []).append((s, h32(cx, cz, 19349663, 83492791), int(j)))
    fuera = []
    for g in sorted(grupos, key=str): fuera += reparto(grupos[g], RECORTE_PCT)
    total = sum(len(g) for g in grupos.values()); fuera = set(fuera)
    mallas = [k for k in range(len(M)) if int(M['nodo'][k]) in fuera and int(M['n'][k]) > 0]
    if info is not None: info.append('  farolas: %d en %s' % (total, {str(g): len(v) for g, v in grupos.items()}))
    return mallas, total, len(fuera)


def muros_bk00(E):
    """(huellas de las farolas, {huella: textura nueva} de los muros) de BK00.DAT de E"""
    import mmap, pak_union
    f = os.path.join(E, 'BK00.DAT')
    if not os.path.isfile(f): return [], {}
    b = open(f, 'rb').read()
    if b[:4] == b'PAKD':
        with open(os.path.join(E, b[20:36].rstrip(b'\0').decode()), 'rb') as fp:
            P = mmap.mmap(fp.fileno(), 0, access=mmap.ACCESS_READ); b = pak_union.recompone(b, P); P.close()
    d = parte(b); hs = huellas(b, d)
    nuevas = edificios(b, d, MUROS_BK, None, LADO_MURO)
    hmuro = {hs[t]: x for t, x in nuevas.items()}
    hmuro['firmas'] = np.array([firma(imagen(b, d, t)) for t in MUROS_BK if t < len(hs)])
    return [hs[t] for t in FAROLA_BK if t < len(hs)], hmuro


def muros(b, d, hmuro):
    """{textura: nueva} de los muros de la union: los mismos bytes que en BK00 o la misma imagen en otro tamano
    (firma a menos de MURO_UMBRAL: en BK02..BK09 el muro bajo es de 128x32); las nuevas se guardan en hmuro por huella"""
    res = {}; F = hmuro.get('firmas')
    for t, h in enumerate(huellas(b, d)):
        if h in hmuro: res[t] = hmuro[h]; continue
        w, hh, fmt = d['texs'][t][:3]
        if F is None or not len(F) or max(w, hh) <= LADO_MURO or (fmt & 0xff) > 2: continue
        if np.sqrt(((F - firma(imagen(b, d, t))) ** 2).mean(axis=(1, 2, 3))).min() < MURO_UMBRAL:
            x = edificios(b, d, [t], None, LADO_MURO)
            if t in x: hmuro[h] = res[t] = x[t]
    return res


def ruta_der(E):
    f = os.path.join(E, 'BKD.DAT')
    if not os.path.isfile(f): return None
    d = parte(remip.lee(f)); return d['R'] if len(d['R']) >= 3 else None


def ya_recortado(E):
    """la marca del recorte: los muros de BK00 ya son de LADO_MURO o menos"""
    import mmap, pak_union
    f = os.path.join(E, 'BK00.DAT')
    if not os.path.isfile(f) or not MUROS_BK: return False
    b = open(f, 'rb').read()
    if b[:4] == b'PAKD':
        with open(os.path.join(E, b[20:36].rstrip(b'\0').decode()), 'rb') as fp:
            P = mmap.mmap(fp.fileno(), 0, access=mmap.ACCESS_READ); b = pak_union.recompone(b, P); P.close()
    d = parte(b)
    return all(max(d['texs'][t][:2]) <= LADO_MURO for t in MUROS_BK if t < len(d['texs']))


def reescribe(b, d, mallas, nuevas):
    """bytes del ORD7 con las mallas a 0 vertices y las texturas nuevas"""
    b = bytearray(b)
    for k in mallas: struct.pack_into('<I', b, d['oM'] + 32 * k + 20, 0)      # (malla: c[3], r, i0, n, ti, nodo)
    if not nuevas: return bytes(b)
    blob = b''; tab = []; b0 = d['blob0']
    for t, (w, h, fmt, li, off, tam) in enumerate(d['texs']):
        if t in nuevas: w, h, fmt, dd = nuevas[t][0], nuevas[t][1], nuevas[t][2], nuevas[t][3]
        else: dd = bytes(b[b0 + off:b0 + off + tam])
        dd += b'\0' * (-len(dd) % 32)
        tab.append(struct.pack('<6I', w, h, fmt, li, len(blob), len(dd))); blob += dd
    nt = len(d['texs'])
    return bytes(b[:56]) + b''.join(tab) + bytes(b[56 + 24 * nt:b0]) + blob


def refs_de(E):
    R = []; cache = {}
    for s, t in REF_ANUNCIO:
        f = os.path.join(E, 'SEG%02d.DAT' % s)
        if not os.path.isfile(f): continue
        if s not in cache: bb = remip.lee(f); cache[s] = (bb, parte(bb))
        bb, d = cache[s]
        if t < len(d['texs']): a = imagen(bb, d, t); R.append(firma(a)); R.append(firma(a[:, ::-1]))
    return np.array(R) if R else None


def pareja_palm(E):
    """las uniones que conviven con SEG00 en algun arbol de CURSO.DAT / CURSOTA.DAT"""
    import vram as vr
    u = set()
    for cf in ('CURSO.DAT', 'CURSOTA.DAT'):
        if not os.path.isfile(os.path.join(E, cf)): continue
        for curs in vr.curso(E, cf).values():
            for c in curs:
                hs = [c] + [curs[k] for k in (c[3], c[4]) if 0 <= k < len(curs)]
                if any(h[1] == 0 for h in hs): u.add('BK%02d.DAT' % c[5])
    return u


def ya_hecho(E):
    f = os.path.join(E, 'SEG00.DAT')
    if not os.path.isfile(f): return False
    d = parte(remip.lee(f))
    ts = [max(d['texs'][t][:2]) for t in EDIF_PALM if t < len(d['texs'])]
    return bool(ts) and max(ts) <= LADO_MAX and min(ts) <= LADO      # (sin tocar: todas de 64 o 128)


def datos(E, D, W, solo=None, con_decor=True, con_palm=True, info_solo=False, con_rec=True):
    import shutil, subprocess, pak_union, mmap, time
    if con_palm and ya_hecho(E):
        print('aligera: SEG00 ya tiene los edificios reducidos (%d/%d): decoracion y edificios ya aplicados' % (LADO, LADO_MAX)); con_palm = con_decor = False
    if con_rec and ya_recortado(E): print('aligera: los muros de BK00 ya son de %d: recorte ya aplicado' % LADO_MURO); con_rec = False
    if not (con_decor or con_palm or con_rec): print('aligera: nada que hacer'); return
    tdec = tabla_decor() if con_decor else {}
    refs = refs_de(E) if con_decor else None
    print('anuncios de referencia: %d firmas' % (0 if refs is None else len(refs)))
    rcar = refs_cartel(E) if con_rec else None
    hfar, hmuro = muros_bk00(E) if con_rec else ([], {})
    rder = ruta_der(E) if con_rec else None
    if con_rec: print('recorte: carteles %d / %d firmas; farolas %d texturas; muros %d texturas (BK00 %s a %d)' % (
        0 if rcar is None else len(rcar[0]), 0 if rcar is None else len(rcar[1]), len(hfar), len(hmuro) - 1, MUROS_BK, LADO_MURO))
    hechas = {}; bks = pareja_palm(E) if con_palm else set()
    tot = [0, 0, 0, 0, 0, 0]; rt = [0, 0, 0, 0, 0, 0, 0]       # carteles, fuera, farolas, fuera, texturas de muro, uniones, VRAM
    if not info_solo: os.makedirs(D, exist_ok=True); os.makedirs(W, exist_ok=True)
    def tr(f, b):
        if solo is not None and f[:-4] not in solo and not f.startswith('BK'): return b
        t0 = time.time(); d = parte(b); lin = []
        if f.startswith('SEG'):
            seg = int(f[3:-4]); mallas = []; nuevas = {}
            if con_decor: mallas, nv, qv, na, qa = decor(b, d, seg, tdec, refs, lin); tot[0] += nv; tot[1] += qv; tot[2] += na; tot[3] += qa
            else: nv = qv = na = qa = 0
            if con_palm and seg == 0: nuevas = edificios(b, d, EDIF_PALM, hechas)
            if con_rec:
                mc, nc, qc = carteles(b, d, rcar, lin); rt[0] += nc; rt[1] += qc; mallas = sorted(set(mallas) | set(mc))
                print('%s: carteles %d, %d fuera (%d mallas)' % (f, nc, qc, len(mc)))
                for t, h in enumerate(huellas(b, d)):
                    if h in hmuro and t not in nuevas: nuevas[t] = hmuro[h]       # (los muros de BK00, si estan en la etapa)
            v0 = sum(remip.A32(x[5]) for x in d['texs']); v1 = v0 + sum(remip.A32(len(x[3])) - remip.A32(d['texs'][t][5]) for t, x in nuevas.items())
            tot[4] += v1 - v0
            print('%s: vegetacion %d a la vista, %d fuera; vallas %d, %d fuera; %d mallas a 0; texturas %d (VRAM %d -> %d KB) %.0f s'
                  % (f, nv, qv, na, qa, len(mallas), len(nuevas), v0 >> 10, v1 >> 10, time.time() - t0))
            for t, x in sorted(nuevas.items()):
                w, h, fm = d['texs'][t][:3]
                lin.append('  t%-3d %3dx%-3d %06x -> %dx%d %06x %s error %.1f' % (t, w, h, fm, x[0], x[1], x[2], 'VQ' if x[2] & 0x100 else '16b', x[4]))
            if os.environ.get('ALIGERA_DETALLE') or info_solo: print('\n'.join(lin))
            tot[5] += bool(mallas or nuevas)
            return reescribe(b, d, mallas, nuevas) if (mallas or nuevas) else b
        if (f in bks and hechas) or con_rec:                      # uniones: lo de PALM (su curso) y el recorte, por contenido
            nuevas = {}; mallas = []
            nm = 0
            if con_rec: nuevas = muros(b, d, hmuro); nm = len(nuevas)
            for t, h in enumerate(huellas(b, d)):
                if f in bks and h in hechas: nuevas[t] = hechas[h]
            if con_rec:
                mallas, nf, qf = farolas(b, d, hfar, rder, lin); rt[2] += nf; rt[3] += qf
                rt[4] += nm; rt[5] += bool(mallas or nm)
                rt[6] += sum(remip.A32(len(x[3])) - remip.A32(d['texs'][t][5]) for t, x in nuevas.items())
                if mallas or nm: print('%s: farolas %d, %d fuera (%d mallas); %d texturas de muro' % (f, nf, qf, len(mallas), nm))
                if os.environ.get('ALIGERA_DETALLE') and lin: print('\n'.join(lin))
            if nuevas or mallas:
                if f in bks and hechas: print('%s: texturas de los edificios de PALM reducidas' % f)
                return reescribe(b, d, mallas, nuevas)
        return b
    num = lambda f: int(''.join(c for c in f if c.isdigit()))
    sueltos = sorted((f for f in os.listdir(E) if remip.es_modelo(f, 'SEG') or remip.es_modelo(f, 'BK')), key=lambda f: (f[0] != 'S', num(f)))
    if not bks and not con_rec: sueltos = [f for f in sueltos if f.startswith('SEG')]
    for pak, pre, op in remip.PAQUETES:
        if pre == 'BK' and not bks and not con_rec: continue
        P = None
        if os.path.isfile(os.path.join(E, pak)):
            fp = open(os.path.join(E, pak), 'rb'); P = mmap.mmap(fp.fileno(), 0, access=mmap.ACCESS_READ)
        e, s = (os.path.join(W, 'ent_' + pak), os.path.join(W, 'sal_' + pak)) if W else (None, None)
        if not info_solo:
            for x in (e, s):
                if os.path.isdir(x): shutil.rmtree(x)
                os.makedirs(x)
        cambia = 0
        for f in sueltos:
            if not remip.es_modelo(f, pre): continue
            b = open(os.path.join(E, f), 'rb').read()
            if b[:4] == b'ORD7':
                if pak != 'SEGS.PAK' and pak != 'UNIONES.PAK': continue
                nb = tr(f, b)
                if nb != b and not info_solo: open(os.path.join(D, f), 'wb').write(nb)
                continue
            if b[:4] != b'PAKD' or b[20:36].rstrip(b'\0').decode() != pak or P is None: continue
            b = pak_union.recompone(b, P)
            nb = tr(f, b) if (pre == 'SEG' or f in bks or con_rec) else b; cambia += nb != b
            if not info_solo: open(os.path.join(e, f), 'wb').write(nb)
        if P is not None: P.close(); fp.close()
        print('%s: %d cambian' % (pak, cambia))
        if cambia and not info_solo:
            subprocess.run([sys.executable, '-u', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pak_union.py'), e, s] + op,
                           check=True, stdout=subprocess.DEVNULL)
            for f in os.listdir(s): shutil.copyfile(os.path.join(s, f), os.path.join(D, f))
        if not info_solo: shutil.rmtree(e); shutil.rmtree(s)
    print('total: vegetacion %d a la vista, %d fuera (%.1f %%); vallas %d, %d fuera (%.1f %%); VRAM de las etapas %+.0f KB; %d etapas cambian'
          % (tot[0], tot[1], 100 * tot[1] / max(tot[0], 1), tot[2], tot[3], 100 * tot[3] / max(tot[2], 1), tot[4] / 1024, tot[5]))
    if con_rec: print('recorte: carteles %d, %d fuera (%.1f %%); farolas %d, %d fuera (%.1f %%); texturas de muro %d en %d uniones cambiadas (VRAM de las uniones %+.0f KB en total)'
                      % (rt[0], rt[1], 100 * rt[1] / max(rt[0], 1), rt[2], rt[3], 100 * rt[3] / max(rt[2], 1), rt[4], rt[5], rt[6] / 1024))


def main():
    a = sys.argv[1:]
    if '--solo-recorte' in a: a = [x for x in a if x != '--solo-recorte'] + ['--sin-decor', '--sin-palm']
    cd, cp, cr = '--sin-decor' not in a, '--sin-palm' not in a, '--sin-recorte' not in a
    a = [x for x in a if x not in ('--sin-decor', '--sin-palm', '--sin-recorte')]
    solo = None
    if '--solo' in a: i = a.index('--solo'); solo = set(a[i + 1].split(',')); a = a[:i] + a[i + 2:]
    if a and a[0] == '--datos' and len(a) == 4: return datos(a[1], a[2], a[3], solo, cd, cp, con_rec=cr)
    if a and a[0] == '--info' and len(a) >= 2: return datos(a[1], None, None, set(x.replace('.DAT', '') for x in a[2:]) or None, cd, cp, True, cr)
    raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
