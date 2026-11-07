"""Modelos para la Dreamcast a partir de bloques de PS2 (etapas de STAGE.PS2 y coches de CARS.PS2), formato ORD4:
 'ORD4', nmallas, nvert, nruta, ntex, inicio de cada lista (opaca, recorte, transparente, cielo, fin);
 texturas (ancho, alto, formato 0 ARGB1555 / 1 RGB565 / 2 ARGB4444, lista, desplazamiento, tamano: 24 bytes);
 mallas ordenadas por lista y textura (cx, cy, cz, radio float, primer vertice, cuenta, textura o -1: 28 bytes);
 vertices (x, y, z, u, v float, color ARGB: 24 bytes); ruta (x, y, z, ancho izquierdo, ancho derecho float);
 relleno a 32; texeles twiddled de 16 bits.
Cada malla es una tira de triangulos (un UNPACK de posiciones del paquete VIF); su textura sale de la tabla 2 del
registro (cuenta desde 1; 0 = sin textura)."""
import os, struct
import numpy as np
from ps2 import tex, vif

LUZ = np.array([0.3, 0.8, 0.5]) / np.linalg.norm([0.3, 0.8, 0.5])

def twiddle(w, h):
    """orden de los texeles en una textura 'twiddled' de la PowerVR (y en los bits pares, x en los impares;
    las no cuadradas son bloques cuadrados de lado min(w, h) seguidos)"""
    s = min(w, h); y, x = np.mgrid[0:h, 0:w]
    def esp(v):
        r = np.zeros_like(v)
        for b in range(10): r |= ((v >> b) & 1) << (2 * b)
        return r
    blo = (x // s) if w > h else (y // s)
    return blo * s * s + esp(y % s) | (esp(x % s) << 1)

def textura_dc(a):
    """RGBA -> (formato, lista, datos twiddled de 16 bits). Lista 0 opaca, 1 recorte (alfa de 1 bit), 2 transparente"""
    al = a[..., 3].astype(int); h, w = al.shape
    if (al >= 240).all() or al.max() - al.min() < 64: lista, fmt = 0, 1   # RGB565 (alfa casi constante: no es
                                                                 # transparencia, lo usa el juego para otra cosa)
    elif ((al < 16) | (al >= 240)).mean() > 0.97: lista, fmt = 1, 0   # ARGB1555
    else: lista, fmt = 2, 2                                      # ARGB4444
    r, g, b = (a[..., k].astype(np.uint32) for k in range(3))
    if fmt == 1: v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    elif fmt == 0: v = ((al >= 128).astype(np.uint32) << 15) | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)
    else: v = ((al.astype(np.uint32) >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)
    out = np.zeros(w * h, np.uint16); out[twiddle(w, h).ravel()] = v.ravel()
    return fmt, lista, out.tobytes()

class Modelo:
    def __init__(self):
        self.texs = []; self.blob = b''; self.mallas = []; self.verts = []; self.nv = 0; self.ruta = np.zeros((0, 5))

    def bloque(self, blq, registros=None, lista=None, sin_textura=int(os.environ.get("SINTEX", "0x60202020"), 16), copias=None):
        """anade los registros de un bloque (todos si registros es None). lista: fuerza la lista de sus mallas.
        sin_textura: color ARGB de las mallas sin textura, o None para iluminarlas por la normal (opacas).
        copias: {registro: [desplazamiento (x, y, z), ...]} para repetir un registro (ruedas)."""
        m, A = blq.m, blq.A; base = len(self.texs)
        for h in tex.cabeceras(blq):
            a = tex.imagen(blq, h); fmt, li, d = textura_dc(a)
            self.texs.append((a.shape[1], a.shape[0], fmt, li, len(self.blob), len(d))); self.blob += d
        ntb = len(self.texs) - base
        for r, reg in enumerate(blq.registros()):
            if registros is not None and r not in registros: continue
            c, o = reg[3]; t2 = reg[2][1]
            for i in range(c):
                e = struct.unpack('<6I', m[A + o + 24 * i:A + o + 24 * i + 24])
                ti = struct.unpack('<i', m[A + t2 + 16 * i:A + t2 + 16 * i + 4])[0] - 1
                ti = base + ti if 0 <= ti < ntb else -1
                for pk in vif.datos(m, A + e[2], A + blq.w[2]):
                    for tr in vif.tiras(pk):
                        for dx in (copias or {}).get(r, [(0, 0, 0)]):
                            self._tira(tr, ti, lista, sin_textura, np.array(dx, np.float32), blq)

    def _tira(self, tr, ti, lista, sin_textura, dx, blq):
        """una tira; si une piezas separadas (una arista 6 veces mas larga que la mediana y de mas de 15 m: la PS2
        no dibuja esos triangulos de union), se corta en trozos"""
        P = tr[0x68].astype(float)
        if len(P) < 3 or not np.isfinite(P).all(): return
        e = np.linalg.norm(np.diff(P, axis=0), axis=1); med = np.median(e)
        cortes = [j for j in range(len(e)) if e[j] > 6 * med and e[j] > 15]
        if not cortes: return self._trozo(tr, ti, lista, sin_textura, dx, blq, 0, len(P))
        a = 0
        for j in cortes + [len(P) - 1]:
            if j + 1 - a >= 3: self._trozo(tr, ti, lista, sin_textura, dx, blq, a, j + 1)
            a = j + 1

    def _trozo(self, tr, ti, lista, sin_textura, dx, blq, a, b):
        tr = {k: (v[a:b] if v is not None and k != 0x6c and len(v) == len(tr[0x68]) else v) for k, v in tr.items()}
        P = tr[0x68].astype(np.float32) + dx; nn = len(P)
        uv = np.zeros((nn, 2))
        for cl in (0x6d, 0x65):
            if cl in tr and len(tr[cl]) == nn: uv = tr[cl][:, :2].astype(float) / 4096; break
        N = tr[0x69].astype(float) / 32768 if 0x69 in tr and len(tr[0x69]) == nn else np.zeros_like(P)
        if 0x6e in tr and len(tr[0x6e]) == nn:
            col = np.clip(tr[0x6e][:, :3].astype(float) * 2, 0, 255)
        else:
            luz = 0.7 + 0.3 * np.clip(np.abs(N @ LUZ), 0, 1)
            col = np.repeat(255 * luz[:, None], 3, 1)
        li = self.texs[ti][3] if ti >= 0 else 2
        if ti < 0 and sin_textura is None and P[:, 1].max() < 0.3:   # plano bajo el coche: su sombra
            col = np.zeros((nn, 3)); li = 2
        elif ti < 0 and sin_textura is None:                   # pintura: rojo con luz y un brillo por arriba
            luz = 0.35 + 0.65 * np.clip(N @ LUZ, 0, 1)
            col = np.stack([200 * luz + 50 * luz ** 8, 20 * luz + 60 * luz ** 8, 20 * luz + 60 * luz ** 8], 1)
            col = np.clip(col, 0, 255); li = 0
        col = col.astype(np.uint32)
        argb = (0xFF << 24) | (col[:, 0] << 16) | (col[:, 1] << 8) | col[:, 2]
        if ti < 0 and sin_textura is not None: argb[:] = sin_textura
        if ti < 0 and sin_textura is None and li == 2: argb[:] = 0x90000000
        if lista is not None: li = lista
        cc = P.mean(0); rad = float(np.sqrt(((P - cc) ** 2).sum(1).max()))
        self.mallas.append([li, ti, cc, rad, self.nv, nn, blq])
        v = np.zeros(nn, dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('u', '<f4'), ('v', '<f4'), ('c', '<u4')])
        v['x'], v['y'], v['z'], v['u'], v['v'], v['c'] = P[:, 0], P[:, 1], P[:, 2], uv[:, 0], uv[:, 1], argb
        self.verts.append(v); self.nv += nn

    def escribe(self, sal):
        self.mallas.sort(key=lambda x: (x[0], x[1]))
        inicio = [sum(1 for x in self.mallas if x[0] < l) for l in range(5)]
        with open(sal, 'wb') as f:
            f.write(b'ORD4' + struct.pack('<9I', len(self.mallas), self.nv, len(self.ruta), len(self.texs), *inicio))
            for t in self.texs: f.write(struct.pack('<6I', *t))
            for li, ti, cc, rad, i0, cnt, _ in self.mallas: f.write(struct.pack('<4f2Ii', *cc, rad, i0, cnt, ti))
            for v in self.verts: f.write(v.tobytes())
            f.write(self.ruta.astype('<f4').tobytes())
            f.write(b'\0' * (-f.tell() % 32)); f.write(self.blob)
        return inicio
