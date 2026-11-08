"""STAGE.PS2: 66 segmentos de carretera. Cada uno empieza por su colision (COLI) y tiene un bloque principal:
cabecera (n registros, ?, tamano A, tamano B) + A (registros de 72 bytes con 8 tablas (cuenta, desplazamiento) y la
marca 87658765 12341234; despues las tablas y los paquetes DMA/VIF de la geometria) + B (texturas de paleta).
Tablas de cada registro: 0 objetos de 56 bytes (esfera: x, y, z, radio en los floats 1-4), 1 y 2 de 16 bytes,
3 mallas de 24 bytes (el tercer word es el desplazamiento en A de su paquete DMA), 4 de 36 bytes.
Paquete de malla: GIFtag (UNPACK V4-32), posiciones (V3-32, float), normales (V3-16, /32768), UV (V2-16, /4096);
tiras de hasta 16 vertices. Registro 0: la calzada."""
import struct
from . import vif

class Bloque:
    """un bloque de geometria y texturas (cabecera n registros, n texturas, tamano A, tamano B)"""
    def __init__(self, iso, o, w):
        self.iso = iso; self.m = iso.m; self.o = o; self.w = w; self.A = o + 16

    def registros(self):
        r = []
        for i in range(self.w[0]):
            p = self.A + i * 72; ws = struct.unpack('<18I', self.m[p:p + 72])
            r.append([(ws[2 * k], ws[2 * k + 1]) for k in range(8)])
        return r

    def mallas(self, reg):
        """paquetes de las mallas del registro: lista de dicts tipo -> [(direccion, array)]"""
        c, off = self.registros()[reg][3]
        res = []
        for i in range(c):
            e = struct.unpack('<6I', self.m[self.A + off + 24 * i:self.A + off + 24 * i + 24])
            res.extend(vif.datos(self.m, self.A + e[2], self.A + self.w[2]))
        return res

class Segmento(Bloque):
    """un segmento: su bloque principal (el mayor) y la lista de todos sus bloques"""
    def __init__(self, iso, entradas):
        self.ent = entradas; self.bloques = []
        for h, o, s in entradas:
            w = struct.unpack('<4I', iso.m[o:o + 16])
            if 1 <= w[0] <= 0x20 and w[2] + w[3] + 16 == s: self.bloques.append(Bloque(iso, o, w))
        if not self.bloques: raise ValueError('segmento sin bloques')
        p = max(self.bloques, key=lambda b: b.w[2] + b.w[3])
        Bloque.__init__(self, iso, p.o, p.w)

def coli(seg):
    """colision del segmento (su primera entrada, 'COLI0200', el mismo formato que los coli*.bin del PC sin el prefijo de
    tamano): cuenta de poligonos (+8), los de la carretera principal (+12), tamano de cabecera (+16), 8 secciones (+20):
    0 listas de la rejilla, 1 un byte por poligono (material: su indice en los 5 bits bajos, la bandera es 1 << indice),
    2 poligonos de 64 bytes (4 vertices x, y, z float, centro, banderas +60), 3 normales, 4 banderas extra, 5 longitud
    de la carretera de cada poligono. Devuelve (vertices (n, 4, 3), material (n,)) o None"""
    import numpy as np
    for h, o, s in seg.ent:
        d = bytes(seg.m[o:o + s])
        if d[:8] != b'COLI0200': continue
        n = struct.unpack('<I', d[8:12])[0]; sec = struct.unpack('<8I', d[20:52])
        mat = np.frombuffer(d[sec[1]:sec[1] + n], np.uint8) & 31
        P = np.frombuffer(d[sec[2]:sec[2] + 64 * n], np.uint8).reshape(n, 64)
        return P[:, :48].copy().view('<f4').reshape(n, 4, 3).astype(float), mat.astype(int)
    return None

def segmentos(iso):
    g = []; cur = None
    for h, o, s in iso.paquete('STAGE.PS2'):
        if iso.m[o:o + 4] == b'COLI' or cur is None: cur = []; g.append(cur)
        cur.append((h, o, s))
    return [Segmento(iso, x) for x in g]
