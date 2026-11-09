"""conv_coli.py tu.iso salida/: la colision del suelo del original para la consulta de altura de la demo (GetYPositionProg
43EB60 / GetRoadCond / CalcYPosSpl, demo/coli.c). Sale de tu disco: no se distribuye.

 COLnn.DAT   la de la etapa nn de STAGE.PS2 (\\Stage\\ZONA\\coli_CS_*, raiz 0 del original: load_type 0)
 COLBK.DAT   la de la bifurcacion (\\BK\\coli_BK_PALM_bin, las dos ramas: la misma en todas; raiz 1)
 COLBKkk.DAT la de la meta de la etapa de clave kk (\\BK\\coli_BK_<zona>_bin; raiz 1)

Del COLI0200 (cabecera de 64 bytes, rejilla de 256 x 256 celdas de 24 m desde -3072, y las secciones 0 listas de cada
celda, 1 tipo por poligono, 2 poligonos de 0x40, 3 normales de 0x30, 5 posicion de carretera por poligono) se guarda
lo que lee la consulta, sin cambiar ningun valor (floats tal cual):
 - los vertices de los poligonos sin repetir (los cuadrilateros vecinos comparten esquinas) y sus indices;
 - el tipo (byte & 31: la clase 1 << tipo) y las banderas (+0x3C; la consulta usa los bits 1, 2 y 4);
 - los vecinos que da ColiGetForward / Back / Left / RightPolygonNumber (pc_course_query.cpp) para la raiz del fichero:
   son funcion fija de las tablas (posicion de carretera, tramos de 43D4D0, tipo y la distancia de 0,5 entre esquinas),
   asi que se calculan aqui y la demo no necesita la seccion 5 ni los tramos; en la raiz 1 no hay (devuelven 0);
 - las normales de las esquinas (seccion 3) solo de los poligonos que las usan: CalcHermite2 toma CalcHermiteTangent
   (con normales) cuando falta el vecino de delante o el de detras;
 - la rejilla recortada al rectangulo de celdas con lista (fuera, la palabra 0: sin poligonos) y las listas enteras
   (las celdas guardan el desplazamiento en palabras de su lista).

Formato COL1 (little endian; cada seccion alineada a 4):
 'COL1', n poligonos, nv vertices, nn normales, nr registros de normales, x0, z0, ancho, alto de la rejilla,
 palabras de las listas, raiz (load_type), 0
 vertices nv x 3 f32 | normales nn x 3 f32 | poligonos n x {u16 v[4]; u8 tipo; u8 banderas; u16 vecinos; u16 normal}
 | registros de normales nr x u16[4] | rejilla ancho x alto u16 | listas u16
 vecinos: bits 0-5 paso hacia delante (0 sin vecino; destino i + paso), 6-11 paso hacia atras (i - paso), bit 12
 izquierda (i - 1), bit 13 derecha (i + 1); normal: registro o 0xFFFF"""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def _tablas(d):
    n, nprim = struct.unpack_from('<2I', d, 8); sec = struct.unpack_from('<8I', d, 20)
    assert d[:8] == b'COLI0200' and struct.unpack_from('<I', d, 16)[0] == 64 and sec[0] == 64 + 131072
    P = np.frombuffer(d[sec[2]:sec[2] + 64 * n], np.uint8).reshape(n, 64)
    V = P[:, :48].copy().view('<f4').reshape(n, 4, 3)
    fl = P[:, 60:62].copy().view('<u2').ravel().astype(int)
    N = np.frombuffer(d[sec[3]:sec[3] + 48 * n], '<f4').reshape(n, 4, 3)
    kd = np.frombuffer(d[sec[1]:sec[1] + n], np.uint8).astype(int)
    L = np.frombuffer(d[sec[5]:sec[5] + 2 * n], '<u2').astype(int)
    g = np.frombuffer(d[64:64 + 131072], '<u2').reshape(256, 256)          # [z][x]
    li = np.frombuffer(d[sec[0]:sec[1]], '<u2')
    return n, nprim, V, fl, N, kd, L, g, li

def _rangos(L, nprim):
    """PC 0x43DCD2..0x43DD78 (pc_coli0200.cpp): {primero, ultimo} por posicion de carretera"""
    cur, abierto, pares = 0, True, [[0, 0]]
    for i in range(nprim):
        l = L[i]
        if abierto and l != cur: pares[cur][1] = i - 1; abierto = False
        if l > cur: cur += 1; pares.append([i, 0]); abierto = True
    pares[cur][1] = nprim - 1
    return pares

def _s16(v): v &= 0xffff; return v - 0x10000 if v & 0x8000 else v

class _Raiz:
    def __init__(self, d, tipo):
        (self.n, self.nprim, self.V, self.fl, self.N, self.kd, self.L, self.g, self.li) = _tablas(d)
        self.tipo = tipo; self.rangos = _rangos(self.L, self.nprim) if self.nprim else []
        self.total = len(self.rangos)
    def bit(self, i): return 1 << (self.kd[i] & 31)
    def primario(self, pedido, pista):
        """find_primary_course_run (PC 0x43D4D0): (cuantos, primero, ultimo) o (0, ...)"""
        if not self.nprim: return 0, 0, 0
        obj = min(pedido, self.total - 1); cnt = self.nprim
        if pista >= cnt: pista = -1
        if pista == -1:
            a, b = self.rangos[obj]; return b - a + 1, a, b
        lg = lambda i: self.L[i]
        cur = lg(pista); a = b = pista
        if cur == obj:
            while a > 0 and lg(a - 1) == obj: a -= 1
            while b < cnt - 1 and lg(b + 1) == obj: b += 1
        elif cur < obj:
            a = pista + 1
            while a < cnt and lg(a) != obj: a += 1
            if a >= cnt: return 0, 0, 0
            b = a
            while b < cnt - 1 and lg(b + 1) == obj: b += 1
        else:
            b = pista
            while b >= 0 and lg(b) != obj: b -= 1
            if b < 0: return 0, 0, 0
            a = b
            while a > 0 and lg(a - 1) == obj: a -= 1
        return b - a + 1, a, b
    def _dist(self, p, q): return float(np.sqrt(((p.astype(np.float64) - q.astype(np.float64)) ** 2).sum()))
    def _une(self, i, j):
        a, b = self.V[i], self.V[j]
        return self._dist(a[0], b[3]) <= 0.5 and self._dist(a[1], b[2]) <= 0.5
    def adelante(self, i):
        if self.tipo == 1: return None
        aqui = _s16(self.L[i]); maximo = self.L[self.nprim - 1] if self.nprim else 0
        if aqui >= maximo: return None
        bit = self.bit(i)
        if bit & 0xf00002:
            paso = self.primario(aqui, i)[0]
            if paso != self.primario(_s16(aqui + 1), i)[0]: return None
        else:
            if not (bit & 0x3b00) or not self.nprim: return None
            if i >= self.n - 1: return None
            if self.L[i + 1] != aqui + 1: return None
            paso = 1
        j = i + paso
        return paso if self._une(i, j) else None
    def atras(self, i):
        if self.tipo == 1: return None
        aqui = _s16(self.L[i])
        if aqui <= 0: return None
        bit = self.bit(i)
        if bit & 0xf00002:
            paso = self.primario(aqui, i)[0]
            if paso != self.primario(_s16(aqui - 1), i)[0]: return None
        else:
            if not (bit & 0x3b00) or i == 0: return None
            if self.L[i - 1] != aqui - 1: return None
            paso = 1
        j = i - paso
        return paso if self._une(j, i) else None
    def lado(self, i, der):
        if not (self.bit(i) & 0xf00002) or self.tipo == 1: return False
        c, a, b = self.primario(_s16(self.L[i]), i)
        return c > 1 and i != (b if der else a)

def _al4(b): return b + b'\0' * (-len(b) & 3)

def escribe(d, fn, tipo):
    """COL1 de la colision COLI0200 d para la raiz tipo (0 etapa, 1 union); devuelve el tamano"""
    r = _Raiz(d, tipo); n = r.n
    vu, vi = np.unique(r.V.reshape(-1, 3).view('<u4'), axis=0, return_inverse=True)
    assert len(vu) < 0xffff
    polys = bytearray(); recs = []; nidx = []
    for i in range(n):
        f = r.adelante(i) if r.nprim else None
        a = r.atras(i) if r.nprim else None
        assert (f or 0) < 64 and (a or 0) < 64 and f != 0 and a != 0
        nb = (f or 0) | ((a or 0) << 6) | (0x1000 if r.nprim and r.lado(i, False) else 0) | (0x2000 if r.nprim and r.lado(i, True) else 0)
        if f and a: rec = 0xffff                            # CalcHermiteTangent2: sin normales
        else: rec = len(recs); recs.append(i)
        assert rec < 0xffff or (f and a)
        v = vi.ravel()[4 * i:4 * i + 4]
        polys += struct.pack('<4HBBHH', *[int(x) for x in v], r.kd[i] & 31, r.fl[i] & 0xff, nb, rec)
    if recs:
        nu, ni = np.unique(r.N[recs].reshape(-1, 3).view('<u4'), axis=0, return_inverse=True)
    else:
        nu, ni = np.zeros((0, 3), '<u4'), np.zeros(0, int)
    assert len(nu) < 0xffff
    z, x = np.nonzero(r.g)
    x0, z0, x1, z1 = int(x.min()), int(z.min()), int(x.max()), int(z.max())
    rej = np.ascontiguousarray(r.g[z0:z1 + 1, x0:x1 + 1]).astype('<u2')
    cab = b'COL1' + struct.pack('<12I', n, len(vu), len(nu), len(recs), x0, z0, x1 - x0 + 1, z1 - z0 + 1, len(r.li), tipo, 0, 0)
    out = cab + vu.astype('<u4').tobytes() + nu.astype('<u4').tobytes() + _al4(bytes(polys)) \
        + _al4(np.asarray(ni, '<u2').tobytes()) + _al4(rej.tobytes()) + _al4(r.li.astype('<u2').tobytes())
    with open(fn, 'wb') as fh: fh.write(out)
    print('%s: %d poligonos (%d primarios), %d vertices, %d normales en %d registros, rejilla %dx%d, %d KB'
          % (os.path.basename(fn), n, r.nprim, len(vu), len(nu), len(recs), x1 - x0 + 1, z1 - z0 + 1, len(out) >> 10))
    return len(out)

def curso(A, dsal):
    """las colisiones de las etapas y uniones del arbol del curso (conv_union.curso)"""
    import conv_union
    E = A.elf; base, nprim, base2 = conv_union.tablas(A)
    descs = [conv_union.u32(E, base + 4 * i) for i in range(nprim)]
    segs = A.segmentos(); hechos = set()
    for clave, ti, ui, hi, hd in conv_union.arbol(A, 0):
        n = conv_union.segmento_de(A, conv_union.cadena(E, descs[ti] + 0x0c))
        if n not in hechos:
            for h, o, s in segs[n].ent:
                dd = bytes(segs[n].m[o:o + s])
                if dd[:8] == b'COLI0200': escribe(dd, os.path.join(dsal, 'COL%02d.DAT' % n), 0); break
            hechos.add(n)
        dbk = conv_union.u32(E, base2 + 4 * ui); c_todo = conv_union.cadena(E, dbk + 0x0c)
        if hi >= 0:
            if 'BK' not in hechos:
                escribe(conv_union.coli_bytes(A, '\\BK\\coli_BK_PALM_bin'), os.path.join(dsal, 'COLBK.DAT'), 1); hechos.add('BK')
        else:
            escribe(conv_union.coli_bytes(A, c_todo), os.path.join(dsal, 'COLBK%02d.DAT' % clave), 1)

def main():
    from ps2.assets import Assets
    curso(Assets(sys.argv[1]), sys.argv[2])

if __name__ == '__main__':
    main()
