"""STAGE.PS2: 66 segmentos de carretera. Cada uno empieza por su colision (COLI) y tiene un bloque principal:
cabecera (n registros, ?, tamano A, tamano B) + A (registros de 72 bytes con 8 tablas (cuenta, desplazamiento) y la
marca 87658765 12341234; despues las tablas y los paquetes DMA/VIF de la geometria) + B (texturas de paleta).
Tablas de cada registro: 0 objetos de 56 bytes (esfera: x, y, z, radio en los floats 1-4), 1 y 2 de 16 bytes,
3 mallas de 24 bytes (el tercer word es el desplazamiento en A de su paquete DMA), 4 de 36 bytes.
Paquete de malla: GIFtag (UNPACK V4-32), posiciones (V3-32, float), normales (V3-16, /32768), UV (V2-16, /4096);
tiras de hasta 16 vertices. Registro 0: la calzada."""
import struct
from . import vif

class Segmento:
    def __init__(self, iso, entradas):
        self.iso = iso; self.m = iso.m; self.ent = entradas
        for h, o, s in entradas:
            w = struct.unpack('<4I', self.m[o:o + 16])
            if 1 <= w[0] <= 0x20 and w[2] + w[3] + 16 == s and s > 1_000_000:
                self.o, self.w = o, w; break
        else: raise ValueError('segmento sin bloque principal')
        self.A = self.o + 16

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

def segmentos(iso):
    g = []; cur = None
    for h, o, s in iso.paquete('STAGE.PS2'):
        if iso.m[o:o + 4] == b'COLI' or cur is None: cur = []; g.append(cur)
        cur.append((h, o, s))
    return [Segmento(iso, x) for x in g]
