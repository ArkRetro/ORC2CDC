"""aligera.py: menos decoracion en todas las etapas y los edificios de PALM a la mitad (64->32, 128->64) (fps3; DESVIACION pedida por el
usuario para ganar fps; ver docs/PARIDAD.md, "fps3: decoracion -15 % y edificios de PALM 32x32").

 aligera.py --datos entrada/ salida/ trabajo/ [--solo SEG00,...] [--sin-decor] [--sin-palm]: sobre una carpeta de
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


def edificios(b, d, lista, hechas=None):
    """{textura: (w, h, fmt, datos)} de las de lista a LADO x LADO (las que ya lo son no cambian); hechas: huella ->
    resultado (para las uniones)"""
    import modelo
    texs = d['texs']; nv, cielo, suelo, lado, area, asp = remip.uso(d, True); res = {}
    for t in lista:
        if t >= len(texs): continue
        w, h, fmt, li, off, tam = texs[t]
        L = lado_de(w, h)
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


def datos(E, D, W, solo=None, con_decor=True, con_palm=True, info_solo=False):
    import shutil, subprocess, pak_union, mmap, time
    if con_palm and ya_hecho(E): print('aligera: SEG00 ya tiene los edificios reducidos (%d/%d): ya aplicado, nada que hacer' % (LADO, LADO_MAX)); return
    tdec = tabla_decor() if con_decor else {}
    refs = refs_de(E) if con_decor else None
    print('anuncios de referencia: %d firmas' % (0 if refs is None else len(refs)))
    hechas = {}; bks = pareja_palm(E) if con_palm else set()
    tot = [0, 0, 0, 0, 0, 0]
    if not info_solo: os.makedirs(D, exist_ok=True); os.makedirs(W, exist_ok=True)
    def tr(f, b):
        if solo is not None and f[:-4] not in solo and not f.startswith('BK'): return b
        t0 = time.time(); d = parte(b); lin = []
        if f.startswith('SEG'):
            seg = int(f[3:-4]); mallas = []; nuevas = {}
            if con_decor: mallas, nv, qv, na, qa = decor(b, d, seg, tdec, refs, lin); tot[0] += nv; tot[1] += qv; tot[2] += na; tot[3] += qa
            else: nv = qv = na = qa = 0
            if con_palm and seg == 0: nuevas = edificios(b, d, EDIF_PALM, hechas)
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
        if f in bks and hechas:                                   # uniones del curso de PALM: lo mismo por contenido
            nuevas = {}
            for t, x in enumerate(d['texs']):
                dat = b[d['blob0'] + x[4]:d['blob0'] + x[4] + x[5]]; hu = remip.huella(x, dat)
                if hu in hechas: nuevas[t] = hechas[hu]
            if nuevas:
                print('%s: %d texturas de los edificios de PALM reducidas' % (f, len(nuevas)))
                return reescribe(b, d, [], nuevas)
        return b
    num = lambda f: int(''.join(c for c in f if c.isdigit()))
    sueltos = sorted((f for f in os.listdir(E) if remip.es_modelo(f, 'SEG') or remip.es_modelo(f, 'BK')), key=lambda f: (f[0] != 'S', num(f)))
    if not bks: sueltos = [f for f in sueltos if f.startswith('SEG')]
    for pak, pre, op in remip.PAQUETES:
        if pre == 'BK' and not bks: continue
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
            nb = tr(f, b) if (pre == 'SEG' or f in bks) else b; cambia += nb != b
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


def main():
    a = sys.argv[1:]
    cd, cp = '--sin-decor' not in a, '--sin-palm' not in a; a = [x for x in a if x not in ('--sin-decor', '--sin-palm')]
    solo = None
    if '--solo' in a: i = a.index('--solo'); solo = set(a[i + 1].split(',')); a = a[:i] + a[i + 2:]
    if a and a[0] == '--datos' and len(a) == 4: return datos(a[1], a[2], a[3], solo, cd, cp)
    if a and a[0] == '--info' and len(a) >= 2: return datos(a[1], None, None, set(x.replace('.DAT', '') for x in a[2:]) or None, cd, cp, True)
    raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
