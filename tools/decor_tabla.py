#!/usr/bin/env python3
"""decor_tabla.py datos/ [demo/decor_tabla.h]: la tabla de texturas de vegetacion decorativa y de publico de cada
SEGnn.DAT para demo/main.c (decor_reduce, Rev 2.1). Solo numeros (etapa, textura, tipo): nada del disco.

Pedido del usuario (2026-11-21, tras la auditoria del popping): quitar el 10 % de las palmeras, arboles y arbustos de
decoracion de los margenes y simplificar el publico de las gradas para que cargue a la vez que ellas. DESVIACION.

Candidatos (como tools/lod_veg.py): nodos con mas de la mitad de sus vertices en la lista de recorte, que no son
carteles ni nodos lejanos, de planta < 25 m y de 1,5 a 35 m de alto. Su textura de recorte principal:
 - vegetacion (tipo 1): hojas verdes con huecos irregulares: media G / B de los texeles opacos >= 1,15 y densidad de
   bordes del alfa >= 0,16 (bordes entre texeles opacos y transparentes por texel). Comprobado a mano con 35 texturas
   de 6 etapas: separa todas (las fachadas con ventanas, los carteles y el publico tienen G / B ~1 o pocos bordes);
 - publico (tipo 2): no vegetacion, todo cuadrilateros sueltos (90 % o mas de tiras de 4 vertices, 8 o mas) de menos
   de 3 m (figuras; los paneles de las fachadas son mayores) y con el centro sobre la planta de un nodo opaco de mas de
   3 m de alto (la grada). Con los datos de hoy: las gradas de la salida de PALM (y sus copias).
 - fuera, a mano (FUERA_REF, y el mismo contenido de imagen en cualquier etapa): almenas (CAST), una verja (METR), la
   barandilla con jardineras (BEAC), un cartel a rayas (FLOR) y una estructura y una palmera con tronco (EAST).
Sale una linea por etapa en la que hay algo. Lento (decodifica las texturas): una vez por juego de datos."""
import hashlib, os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vq

FUERA_MD5 = set()                                                # md5 de la imagen (RGBA) de FUERA_REF (main)
VEG_MD5 = set(); FAROLA_MD5 = set()                              # de VEG_REF y FAROLA_REF

# Rev 2.6 (pedido: "eliminar un 5 % de los arboles, arbustos, farolas, arboles con nieve, arbustos de desierto,
# palmeras"): vegetacion que el color no reconoce (nevada, de invierno sin hojas, de zonas secas, palmeras y algunos
# arboles verdes oscuros), elegida a mano en una hoja de las texturas de recorte sin clasificar de todas las etapas
VEG_REF = [(5, 37), (5, 56), (6, 81), (6, 79), (6, 82), (6, 80), (9, 26), (10, 67), (20, 29), (23, 123), (26, 32),
           (31, 66), (31, 74), (31, 76), (33, 62), (33, 26), (33, 25), (38, 37), (38, 56), (39, 81), (39, 79), (39, 82),
           (39, 80), (42, 30), (43, 66), (52, 20), (55, 124), (58, 31), (63, 66), (63, 74), (63, 72), (65, 66), (65, 28),
           (65, 27)]
# farolas (tipo 3): nodos opacos de planta < 2,5 m y 5-16 m de alto repetidos en la etapa, elegidos a mano por su
# silueta (de brazo, de doble globo, de farol); su textura principal (opaca)
FAROLA_REF = [(0, 116), (1, 14), (3, 5), (11, 24), (13, 61), (30, 88), (30, 105), (31, 82), (34, 110), (44, 25),
              (46, 61), (48, 118), (57, 38), (62, 105), (62, 88), (63, 81)]

# (etapa, textura) de referencia de los falsos positivos (SEGnn.DAT de la conversion de SLES-53998 de este repositorio)
FUERA_REF = [(8, 69), (13, 16), (17, 67), (18, 26), (32, 103), (33, 55),
             (64, 95), (33, 26), (65, 28)]                       # (el cartel de FLOR con otra compresion; una palmera de
                                                                 #  EAST con copa y tronco en la misma textura, que no es publico)

MALLA = np.dtype([('c', '<f4', 3), ('r', '<f4'), ('v0', '<u4'), ('n', '<u4'), ('tex', '<i4'), ('nodo', '<i4')])
NODO = np.dtype([('c', '<f4', 3), ('r', '<f4'), ('fl', '<u4'), ('cartel', '<i4'), ('rel', '<u4', 2)])

def lee(datos, n):
    b = open(os.path.join(datos, n), 'rb').read()
    if b[:4] == b'PAKD':
        import pak_union
        pk = b[20:36].split(b'\0')[0].decode(); b = pak_union.recompone(b, open(os.path.join(datos, pk), 'rb').read())
    if b[:4] != b'ORD7': return None
    nm, nv, nr, pr, nt = struct.unpack_from('<5I', b, 4); li = struct.unpack_from('<5I', b, 24); nn, nc, ns = struct.unpack_from('<3I', b, 44)
    o = 56; tex = [struct.unpack_from('<6I', b, o + 24 * i) for i in range(nt)]; o += 24 * nt
    M = np.frombuffer(b, MALLA, nm, o); o += 32 * nm
    V = np.frombuffer(b, '<f4', nv * 6, o).reshape(nv, 6)[:, :3]; o += 24 * nv + 4 * nr * pr
    N = np.frombuffer(b, NODO, nn, o)
    blob = len(b) - max(((t[4] + t[5] + 31) & ~31) for t in tex) if nt else len(b)
    def imagen(i):
        w, h, fmt, lista, off, tam = tex[i]; k = ((fmt >> 16) & 255) + 1; d = b[blob + off:blob + off + tam]
        return np.asarray(vq.decodifica(d, w, h, fmt & 255, k) if fmt & 0x100 else vq.decodifica16(d, w, h, fmt & 255))
    return dict(M=M, V=V, N=N, li=li, imagen=imagen)

def rasgos(im):
    a = im[..., 3] >= 128
    e = ((a[:, 1:] != a[:, :-1]).sum() + (a[1:] != a[:-1]).sum()) / a.size
    c = im[..., :3][a].astype(float)
    return e, (c[:, 1].mean() / (c[:, 2].mean() + 1) if len(c) else 0.0)

def clasifica(d):
    M, V, N, li = d['M'], d['V'], d['N'], d['li']; nn = len(N)
    nv = np.zeros(nn); nr = np.zeros(nn); lo = np.full((nn, 3), 1e9); hi = np.full((nn, 3), -1e9)
    tx = [dict() for _ in range(nn)]; q4 = np.zeros(nn); qs = np.zeros(nn); nst = np.zeros(nn)
    for k in range(li[3]):
        n = int(M['nodo'][k]); c = int(M['n'][k])
        if not 0 <= n < nn or c < 3: continue
        P = V[int(M['v0'][k]):int(M['v0'][k]) + c]; nv[n] += c
        lo[n] = np.minimum(lo[n], P.min(0)); hi[n] = np.maximum(hi[n], P.max(0))
        if li[1] <= k < li[2]:
            nr[n] += c; t = int(M['tex'][k]); tx[n][t] = tx[n].get(t, 0) + c; nst[n] += 1
            if c == 4: q4[n] += 1; qs[n] += np.linalg.norm(P[3] - P[0])
    ext = hi - lo; fp = np.maximum(ext[:, 0], ext[:, 2]); lej = (N['fl'] & 0x20) != 0
    cand = (nv > 0) & (2 * nr > nv) & (N['cartel'] < 0) & ~lej & (fp < 25) & (ext[:, 1] > 1.5) & (ext[:, 1] < 35)
    est = (nv > 0) & (2 * nr <= nv) & (N['cartel'] < 0) & ~lej & (ext[:, 1] > 3) & (fp < 120)
    cache = {}; tipo = {}
    def md5(t):
        if t not in cache:
            im = d['imagen'](t); cache[t] = (rasgos(im), hashlib.md5(im.tobytes()).hexdigest())
        return cache[t]
    for n in np.nonzero(cand)[0]:
        if not tx[n]: continue
        t = max(tx[n], key=tx[n].get)
        (e, gb), h = md5(t)
        if h in VEG_MD5: tipo[t] = 1; continue                   # (antes que FUERA: 33/26 y 65/28 son plantas)
        if h in FUERA_MD5: continue
        if gb >= 1.15 and e >= 0.16: tipo[t] = 1; continue
        if nst[n] >= 8 and q4[n] >= 0.9 * nst[n] and qs[n] / max(q4[n], 1) < 3.0:
            cx, cz = (lo[n, 0] + hi[n, 0]) / 2, (lo[n, 2] + hi[n, 2]) / 2
            if (est & (lo[:, 0] - 1 < cx) & (hi[:, 0] + 1 > cx) & (lo[:, 2] - 1 < cz) & (hi[:, 2] + 1 > cz)).any(): tipo[t] = tipo.get(t, 2)
    # farolas: opacas, planta < 2,5 m, 5-16 m de alto, con una textura de FAROLA_REF (por su contenido)
    to = [dict() for _ in range(nn)]
    for k in range(li[3]):
        n = int(M['nodo'][k]); c = int(M['n'][k])
        if 0 <= n < nn and c >= 3 and not li[1] <= k < li[2]: to[n][int(M['tex'][k])] = to[n].get(int(M['tex'][k]), 0) + c
    far = (nv > 0) & (2 * nr <= nv) & (N['cartel'] < 0) & ~lej & (fp < 2.5) & (ext[:, 1] > 5) & (ext[:, 1] < 16)
    for n in np.nonzero(far)[0]:
        if not to[n]: continue
        t = max(to[n], key=to[n].get)
        if t >= 0 and md5(t)[1] in FAROLA_MD5: tipo[t] = 3
    return tipo

def main():
    if len(sys.argv) < 2: raise SystemExit(__doc__)
    datos = sys.argv[1]; sal = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), '..', 'demo', 'decor_tabla.h')
    for ref, conj in ((FUERA_REF, FUERA_MD5), (VEG_REF, VEG_MD5), (FAROLA_REF, FAROLA_MD5)):
        for s, t in ref:
            d = lee(datos, 'SEG%02d.DAT' % s)
            if d: conj.add(hashlib.md5(d['imagen'](t).tobytes()).hexdigest())
    filas = []
    for s in range(100):
        f = 'SEG%02d.DAT' % s
        if not os.path.exists(os.path.join(datos, f)): continue
        d = lee(datos, f)
        if d is None: continue
        tipo = {t: k for t, k in clasifica(d).items() if (s, t) not in FUERA_REF or (s, t) in VEG_REF}
        for t in sorted(tipo): filas.append((s, t, tipo[t]))
        if tipo: print('%s: vegetacion %s publico %s farolas %s' % (f, [t for t in sorted(tipo) if tipo[t] == 1], [t for t in sorted(tipo) if tipo[t] == 2],
                                                                    [t for t in sorted(tipo) if tipo[t] == 3]))
    with open(sal, 'w') as o:
        o.write('/* decor_tabla.h: hecho por tools/decor_tabla.py (no editar): etapa (SEGnn), textura, 1 vegetacion / 2 publico / 3 farola */\n')
        o.write('static const uint16_t DECOR_TAB[][3] = {\n')
        for i in range(0, len(filas), 6): o.write('    ' + ' '.join('{ %d, %d, %d },' % x for x in filas[i:i + 6]) + '\n')
        o.write('};\n')
    print('%d entradas en %s' % (len(filas), sal))

if __name__ == '__main__':
    main()
