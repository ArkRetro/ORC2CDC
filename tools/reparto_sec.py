#!/usr/bin/env python3
"""reparto_sec.py datos/ [salida.DAT] [--tope-kb=N] [--alfa=X] [--ta-f=X] [--lejos=M] [--fov=GRADOS] [--suave=N] [--solo=SEGnn]:
corte del reparto del TA precalculado por seccion de la PVS (Rev 2.9.8 P2, DESVIACION; tecnica tomada de Crazy Taxi:
lo que se dibuja depende de la posicion, decidido fuera de linea para un presupuesto fijo; docs/PARIDAD.md "Rev 2.9.8:
lecciones de Crazy Taxi", P2). No toca ningun SEGnn.DAT: lee los ya convertidos (sueltos o recompuestos de SEGS.PAK)
con su PVS (tools/pvs.py) y escribe un fichero nuevo, REPSEC.DAT por defecto. Sin ese fichero la demo hace lo de
antes (demo/main.c, repsec_lee y REPARTO_SEC).

Por cada seccion (SEC_PUNTOS puntos de la ruta) y cada punto de la ruta de ella, la camara de persecucion nominal
(CAM_ATRAS m detras del punto y CAM_ALTO m por encima, mirando a lo largo de la ruta, horizontal, con la fov dada) y
lo que la PVS deja ver desde la seccion y su ventana (s - 3 y s + 3, como pvs_ventana: el coste del nodo es el mayor de
los tres), se repite lo de nodos_visibles (lejos, plano cercano, laterales, LOD de 404B00, version lejana de la
vegetacion y lowpoly de lo grande, nodos cercanos enteros, clase de distancia y de LOD con el coste por vertices,
grupos) y el reparto de nodos_visibles_todos (REPARTO_CERCA del tope para lo cercano, lo enorme en la pantalla antes,
lo de mas alla por la clase de LOD) con un tope nominal: TOPE_KB del TA de la etapa al 95 % y k = ta_f x alfa bytes
enviados por byte teorico. Lo que no se repite (no hay datos fuera de linea): la niebla, la histeresis por nodo, la
gracia de la PVS, la reduccion de la decoracion (decor_reduce) y PVS_GRANDE. El corte de la seccion es el menor de
sus puntos y luego el menor de las SUAVE secciones a cada lado (cota que no se rebasa: en ejecucion es el objetivo y
solo se baja de el si el cuadro no cabe de verdad).

Calibracion (medida en Flycast, Rev 2.9.8 P2): con alfa 0,75 (el prio_alfa que se ve en carrera) lo pedido fuera de
linea salia ~1,4 veces menor que lo que pide el reparto en carrera (la gracia de la PVS, la union y la etapa siguiente,
el lejano adaptativo) y la tabla quedaba 8-9 clases de distancia y 4-13 de LOD por encima del corte de siempre en
GHOS, METR y PALM. Medidas con alfa 1,10 y suave 1 y con alfa 1,00 y suave 8 (por defecto, la ultima): con esta la
tabla queda en carrera 1-3 clases por encima del corte de siempre en GHOS e INDU y 7-13 por debajo en METR y PALM (el
lejano adaptativo de esas dos baja mucho lo que hay que repartir y la tabla no lo sabe).

Formato (little endian): 'RSC1', u32 numero de etapas, por etapa u32 SEG, u32 secciones, u32 desplazamiento; en el
desplazamiento, por seccion 2 bytes con signo: prio_d (-1..47) y prio_p (-1..63)."""
import os, struct, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # (con python -I: los modulos de tools/)
import numpy as np
import pvs, pak_union

SEC_PUNTOS = pvs.SEC_PUNTOS
ND_PRIO, NB_PRIO = 48, 64
REPARTO_CERCA = 0.25
REPARTO_COSTE = 4
PVS_VENTANA = 3
PVS_CERCA, PVS_CERCA_R = 120.0, 80.0
CERCA = 0.1
VTX_ETAPA_MAX = 20000
LOD_LEJ_PX = 48.0
LOD_GRANDE_D, LOD_GRANDE_FR = 900.0, 0.8
GRUPO_RMAX = 150.0
NODO_LEJ, NODO_LEJ3D = 0x20, 0x40
CAM_ATRAS, CAM_ALTO = 6.0, 2.5

def log2x(x, k):
    """log2x de demo/main.c: k log2(x) por los bits del float (x > 0), k = 4 u 8"""
    i = np.asarray(x, np.float32).view(np.int32).astype(np.int64)
    return (i - (127 << 23)) >> (20 if k == 8 else 21)

def ta_teo(n):
    t = n - 2; return 24 * t + 60 * ((t + 5) // 6)

def secciones(b, d):
    """las mascaras de mallas visibles por seccion (nsec x nmallas) de las listas XOR del ORD7 b (como pvs_aplica)"""
    h = d['h']; ns = h[12]
    if not (ns & pvs.XOR): return None, None
    ns &= ~pvs.XOR; nm = len(d['M']); nr = len(d['R'])
    p = 56 + sum(len(d[k]) for k in ('texs', 'mallas', 'verts', 'ruta', 'nodos', 'carteles'))
    ini = np.frombuffer(b, '<u4', ns + 1, p).astype(np.int64); p += 4 * (ns + 1)
    L = b[p:p + int(ini[-1])]; p += int(ini[-1]); p += p % 2
    mapa = np.frombuffer(b, '<u2', nr, p).astype(np.int64)
    S = np.zeros((ns, nm), bool); cur = np.zeros(nm, bool)
    for s in range(ns):
        q, f, i = int(ini[s]), int(ini[s + 1]), -1
        while q < f:
            x = L[q]; q += 1
            if x == 255: x = L[q] | L[q + 1] << 8; q += 2
            i += x
            if i >= nm: break
            cur[i] ^= True
        S[s] = cur
    return S, mapa

def corte(cost, vtx, tope, vmax, k):
    """el reparto de nodos_visibles_todos (sin histeresis): (prio_d, prio_p)"""
    fila_c = cost.sum(1); fila_v = vtx.sum(1)
    acum = v = 0.0; cabe = -1
    for d in range(ND_PRIO):
        if (acum + fila_c[d]) * k > tope * REPARTO_CERCA or (v + fila_v[d]) > vmax * REPARTO_CERCA: break
        acum += fila_c[d]; v += fila_v[d]; cabe = d
    if fila_c.sum() * k <= tope and fila_v.sum() <= vmax: cabe = ND_PRIO - 1
    else:
        while cabe >= 0:
            c0 = cost[cabe + 1:, 0].sum(); v0 = vtx[cabe + 1:, 0].sum()
            if (acum + c0) * k <= tope and v + v0 <= vmax: break
            acum -= fila_c[cabe]; v -= fila_v[cabe]; cabe -= 1
    d = -1 if cabe < 0 and (fila_c[0] * k > tope or fila_v[0] > vmax) else (0 if cabe < 0 else cabe)
    cerca_c = fila_c[:d + 1].sum(); cerca_v = fila_v[:d + 1].sum()
    col_c = cost[d + 1:].sum(0); col_v = vtx[d + 1:].sum(0)
    acum, v, p = cerca_c, cerca_v, -1
    for c in range(NB_PRIO):
        if (acum + col_c[c]) * k > tope or v + col_v[c] > vmax: break
        acum += col_c[c]; v += col_v[c]; p = c
    if d == ND_PRIO - 1: p = NB_PRIO - 1
    if d < 0 and p < 0: p = 0
    return d, p

def etapa(b, op):
    fd, tmp = tempfile.mkstemp(suffix='.dat', prefix='rsec_', dir='/dev/shm' if os.path.isdir('/dev/shm') else None)
    with os.fdopen(fd, 'wb') as fo: fo.write(b)                # (pvs.lee lee de un fichero)
    try: d = pvs.lee(tmp)
    finally: os.remove(tmp)
    S, mapa = secciones(b, d)
    if S is None: return None
    M, N, R = d['M'], d['N'], d['R']; nn = len(N); nm = len(M); ns = S.shape[0]
    if nn == 0 or len(R) < 3: return None
    g = M['nodo'].astype(np.int64); n = np.minimum(M['n'].astype(np.int64), 256)
    util = (g >= 0) & (g < nn) & (M['n'] >= 3)
    cm = np.where(util, ta_teo(np.maximum(n, 3)), 0).astype(np.float64); vm = np.where(util, n, 0).astype(np.float64)
    gg = np.where(util, g, 0)
    rel0 = np.bincount(gg, cm, nn); relv = np.bincount(gg, vm, nn)
    fl = N['fl'].astype(np.int64); cart = N['cartel'].astype(np.int64); r = N['r'].astype(np.float64)
    C = N['c'].astype(np.float64)
    nn0 = nn
    for i in range(nn):
        if fl[i] & NODO_LEJ: nn0 = i; break
    grp = fl >> 16; grp = np.where(r >= GRUPO_RMAX, 0, grp); grp[nn0:] = 0    # (lee_modelo_en: los grandes, sin grupo)
    # coste de cada nodo por seccion (pvs_teo) y vertices
    cs = np.zeros((ns, nn)); vs = np.zeros((ns, nn))
    for s in range(ns):
        m = S[s] & util
        cs[s] = np.bincount(g[m], cm[m], nn); vs[s] = np.bincount(g[m], vm[m], nn)
    fx = 320.0 / np.tan(np.radians(op['fov']) / 2); tanh, tanv = 320.0 / fx, 240.0 / fx
    kh, kv = np.sqrt(1 + tanh * tanh), np.sqrt(1 + tanv * tanv); lod = fx / 240.0
    lejos = op['lejos']; tope = op['tope_kb'] * 1024.0 * 0.95; alfa = op['alfa']; k = op['ta_f'] * alfa
    vmax = VTX_ETAPA_MAX / alfa
    u = fl & 15; lim = r * lod * np.exp2(np.where(u > 0, u, 6) - 1)
    jl = np.where(cart <= -2, -2 - cart, -1); jl = np.where(jl < nn, jl, -1)
    lej3d = np.where(jl >= 0, (fl[np.maximum(jl, 0)] & NODO_LEJ3D) != 0, False)
    dg = max(lejos * LOD_GRANDE_FR, LOD_GRANDE_D)
    orig = np.arange(nn) < nn0
    out = np.zeros((ns, 2), np.int64); out[:] = (ND_PRIO - 1, NB_PRIO - 1)
    est = []; VV = []
    for s in range(ns):
        sw = [s, max(0, s - PVS_VENTANA), min(ns - 1, s + PVS_VENTANA)]
        c_win = np.maximum.reduce([cs[x] for x in sw])
        v_win = np.where(c_win > cs[s], np.where(rel0 > 0, relv * c_win / np.maximum(rel0, 1), vs[s]), vs[s])
        pts = np.nonzero(mapa == s)[0]
        res = []
        for ip in pts:
            a = R[ip]; i0 = max(ip - 1, 0); i1 = min(ip + 1, len(R) - 1)
            t = np.array([R[i1, 0] - R[i0, 0], 0.0, R[i1, 2] - R[i0, 2]]); l = np.hypot(t[0], t[2])
            if l < 1e-3: continue
            t /= l; der = np.array([-t[2], 0.0, t[0]]); arr = np.array([0.0, 1.0, 0.0])
            ojo = np.array([a[0], a[1], a[2]], np.float64) - t * CAM_ATRAS + arr * CAM_ALTO
            D = C - ojo; zv = D @ t; xv = D @ der; yv = D @ arr
            vis = orig.copy()
            vis &= ~(zv - r > lejos); vis &= ~(zv + r < CERCA)
            vis &= ~((np.abs(xv) - zv * tanh > r * kh) | (np.abs(yv) - zv * tanv > r * kv))
            vis &= ~((u > 0) & (zv > 0) & (zv > lim))
            cerca = (zv < PVS_CERCA) & (r < PVS_CERCA_R) & (c_win > 0)
            c_teo = np.where(cerca, rel0, c_win); c_v = np.where(cerca, relv, v_win)
            vis &= c_teo > 0
            # version lejana (lod_veg.py / lod_grande.py): el coste del lejano; la clase de distancia con su radio
            rr = r.copy(); cp0 = np.zeros(nn, bool)
            sust = vis & (jl >= 0) & np.where(lej3d, zv - r > dg, (zv > r) & (r * fx < LOD_LEJ_PX * zv))
            if sust.any():
                j = jl[sust]; cj = np.maximum.reduce([cs[x][j] for x in sw])
                c_teo = c_teo.copy(); c_v = c_v.copy()
                c_teo[sust] = np.maximum(cj, 1); c_v[sust] = vs[s][j]; rr[sust] = r[j]; cp0[sust] = True
            zr = zv - rr
            cd = np.where(zr > 1.0, log2x(np.maximum(zr, 1.0), 4), 0); cd = np.clip(cd, 0, ND_PRIO - 1)
            with np.errstate(divide='ignore', invalid='ignore'):
                q = np.where((zv > 0) & (lim > 0), zv / np.where(lim > 0, lim, 1), 1.0)
            cp = 40 + log2x(np.maximum(q, 1e-30), 8)
            cp = cp + np.where(c_v > 1, (log2x(np.maximum(c_v, 1), 4) - 24) * REPARTO_COSTE // 4, 0)
            cp = np.minimum(cp, NB_PRIO - 2); cp = np.clip(cp, 0, NB_PRIO - 1)
            cp = np.where((zv <= 0) | (lim <= 0), 0, cp); cp = np.where(cp0, 0, cp)
            # grupos: la clase mas alta de los suyos visibles
            gi = np.nonzero(vis & (grp > 0) & (grp <= nn))[0]
            if len(gi):
                jefe = grp[gi] - 1; Gd = np.zeros(nn, np.int64); Gp = np.zeros(nn, np.int64)
                np.maximum.at(Gd, jefe, cd[gi]); np.maximum.at(Gp, jefe, cp[gi])
                cd = cd.copy(); cp = cp.copy(); cd[gi] = Gd[jefe]; cp[gi] = Gp[jefe]
            iv = np.nonzero(vis)[0]
            cost = np.zeros((ND_PRIO, NB_PRIO)); vtx = np.zeros((ND_PRIO, NB_PRIO))
            np.add.at(cost, (cd[iv], cp[iv]), c_teo[iv]); np.add.at(vtx, (cd[iv], cp[iv]), c_v[iv])
            res.append(corte(cost, vtx, tope, vmax, k)); est.append(cost.sum() * k); VV.append((vtx.sum(), len(iv)))
        if res: out[s] = (min(x[0] for x in res), min(x[1] for x in res))
    sv = op['suave']
    if sv > 0:
        o2 = out.copy()
        for s in range(ns):
            a0, a1 = max(0, s - sv), min(ns, s + sv + 1)
            o2[s] = out[a0:a1].min(0)
        out = o2
    print('  nodos %d (%d originales) mallas %d; vertices pedidos %.0f, nodos visibles %.0f' % (nn, nn0, nm, np.mean([x[0] for x in VV]), np.mean([x[1] for x in VV])))
    return ns, out, (float(np.mean(est)) if est else 0.0)

def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    op = dict(x[2:].split('=', 1) for x in sys.argv[1:] if x.startswith('--') and '=' in x)
    if not a: raise SystemExit(__doc__)
    datos = a[0]; sal = a[1] if len(a) > 1 else os.path.join(datos, 'REPSEC.DAT')
    o = dict(tope_kb=float(op.get('tope-kb', 800)), alfa=float(op.get('alfa', 1.0)), ta_f=float(op.get('ta-f', 1.75)),
             lejos=float(op.get('lejos', 3800)), fov=float(op.get('fov', 50)), suave=int(op.get('suave', 8)))
    solo = op.get('solo')
    pakf = os.path.join(datos, 'SEGS.PAK'); pak = None
    ent = []
    for f in sorted(os.listdir(datos)):
        if not (f.startswith('SEG') and f.endswith('.DAT') and f[3:-4].isdigit()): continue
        if solo and f[:-4] != solo: continue
        b = open(os.path.join(datos, f), 'rb').read()
        if b[:4] != b'ORD7':
            if b[:4] != b'PAKD': continue
            if pak is None: pak = open(pakf, 'rb').read()
            b = pak_union.recompone(b, pak)
        r = etapa(b, o)
        if r is None: print('%s: sin PVS' % f); continue
        ns, t, e = r
        print('%s: %d secciones, prio_d %.1f (%d..%d) prio_p %.1f (%d..%d), pedido medio %.0f KB, %d cambios' %
              (f, ns, t[:, 0].mean(), t[:, 0].min(), t[:, 0].max(), t[:, 1].mean(), t[:, 1].min(), t[:, 1].max(), e / 1024,
               int((np.diff(t, axis=0) != 0).any(1).sum())))
        sys.stdout.flush()
        ent.append((int(f[3:-4]), ns, t.astype(np.int8).tobytes()))
    cab = b'RSC1' + struct.pack('<I', len(ent)); off = len(cab) + 12 * len(ent); cuerpo = b''
    for nseg, ns, m in ent:
        cab += struct.pack('<3I', nseg, ns, off + len(cuerpo)); cuerpo += m + b'\0' * (-len(m) % 4)
    tmp = sal + '.tmp'
    with open(tmp, 'wb') as fo: fo.write(cab + cuerpo)
    os.replace(tmp, sal)
    print('%s: %d etapas, %d bytes (tope %.0f KB, alfa %.2f, ta_f %.2f, lejos %.0f, fov %.0f, suave %d)' %
          (sal, len(ent), len(cab) + len(cuerpo), o['tope_kb'], o['alfa'], o['ta_f'], o['lejos'], o['fov'], o['suave']))

if __name__ == '__main__':
    main()
