"""Modelos para la Dreamcast a partir de bloques de PS2 (etapas de STAGE.PS2 y coches de CARS.PS2), formato ORD7:
 'ORD7', nmallas, nvert, nruta, floats por punto de ruta, ntex, inicio de cada lista (opaca, recorte, transparente,
 cielo, fin), nnodos, ncarteles, nsecciones;
 texturas (ancho, alto, formato, lista, desplazamiento, tamano: 24 bytes); formato: bits 0-7 0 ARGB1555 / 1 RGB565 /
 2 ARGB4444, bit 8 comprimida VQ (vq.py: libro de N entradas de 8 bytes y un byte por bloque de 2x2), bits 16-23 N - 1;
 mallas ordenadas por lista y textura (cx, cy, cz, radio float, primer vertice, cuenta, textura o -1, nodo o -1: 32
 bytes);
 vertices (x, y, z, u, v float, color ARGB: 24 bytes); ruta (x, y, z, ancho izquierdo, ancho derecho y el perfil
 de alturas de la calzada a lo ancho: NPERF alturas de -izquierdo-2 a derecho+2...);
 nodos del original (cx, cy, cz, radio, banderas: bits 0-3 umbral de LOD, bit 4 orientado a la camara, bits 8-9 rama;
 cartel o -1, 2 de relleno: 32 bytes); carteles orientados a la camara (matriz 4x4 de vectores fila del nodo; en la
 columna 3 el eje z del padre: 64 bytes; las tiras de esos nodos van en el marco del nodo);
 si hay secciones: inicio de la lista de cada seccion (nsecciones + 1 u32), las listas (u16, nodos visibles en la
 seccion: la tabla de visibilidad del original) y la seccion de cada punto de la ruta (u16), relleno a 4;
 relleno a 32; texeles twiddled de 16 bits o VQ, cada textura alineada a 32.
(ORD6 era lo mismo sin nodos ni secciones y con una distancia maxima en lugar del nodo; ORD5, ademas sin VQ.)
Cada malla es una tira de triangulos que dibuja el GS: una tira del VU1 (un UNPACK de posiciones) partida por las
banderas ADC (vif.no_dibuja); su textura: la tabla 2 del registro da una entrada de la tabla 4 (lista de texturas del
registro, 36 bytes), cuyo cuarto campo es el indice de la textura en el bloque."""
import os, struct, time
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

NODO = 56                                                       # registro de nodo de la tabla 0 (PmtNode del PC)
UMBRAL_CURSO = 6                                                # 4044E0(6): umbral de LOD de la etapa (race_area_display_44f120)

def nodos(blq, reg):
    """los nodos de un registro como los recorre el original (walk_404d10 de la decompilacion del PC, pc_render_queue.cpp;
    la tabla 0 es el PmtNode de 56 bytes: +00 banderas, +04 esfera x, y, z, radio, +14 sesgo de LOD, +18 animacion,
    +1C matriz (tabla 6, -1 ninguna), +20 primer hijo, +24 siguiente hermano, +28 malla de cada LOD (tabla 1)):
    desde el nodo 0, hermanos en orden y cada hijo antes que el hermano siguiente; la matriz del nodo se multiplica por
    la del padre (409FD0: nueva = m * actual, vectores fila: v * m * padre). Tabla 1 (16 bytes, la malla del nodo):
    cuenta y primera de las tiras opacas (tabla 3) y de las de mezcla: (n_op, n_mez, primera_op, primera_mez).
    Devuelve [(indice, banderas, matriz 4x4 o None, esfera (c, r) en el marco del bloque, [tiras de la tabla 3])]"""
    m, A = blq.m, blq.A
    n0, o0 = reg[0]; n1, o1 = reg[1]; n6, o6 = reg[6]; n3 = reg[3][0]
    res = []; vistos = set()
    def mat(i):
        return np.frombuffer(m[A + o6 + 64 * i:A + o6 + 64 * i + 64], '<f4').reshape(4, 4).astype(float) if 0 <= i < n6 else None
    def anda(k, padre):
        while 0 <= k < n0 and k not in vistos:
            vistos.add(k)
            x = struct.unpack('<I4f9i', m[A + o0 + NODO * k:A + o0 + NODO * k + NODO])
            M = mat(x[7]); W = padre if M is None else (M if padre is None else M @ padre)
            c = np.array(x[1:4], float); r = float(x[4])
            if W is not None:
                c = c @ W[:3, :3] + W[3, :3]
                r *= float(np.linalg.norm(np.full(3, 0.577350259) @ W[:3, :3]))
            tiras = []
            g = x[10]                                            # malla del LOD 0 (los de PS2 no traen otros)
            if 0 <= g < n1:
                nop, nmez, pop, pmez = struct.unpack('<4i', m[A + o1 + 16 * g:A + o1 + 16 * g + 16])
                for n, p in ((nop, pop), (nmez, pmez)):
                    if n > 0 and p >= 0: tiras += [i for i in range(p, p + n) if i < n3]
            res.append((k, x[0], W, (c, r), tiras, x[7] >= 0))
            if x[8] >= 0: anda(x[8], W)
            k = x[9]
    anda(0, None)
    return res

def material_de(blq, reg, k):
    """color del material de la entrada k de la lista de texturas del registro (tabla 4, 36 bytes: el primer campo es
    el material): tabla 5, 64 bytes, difuso r, g, b, a en unidades del GS (128 = 1), luego ambiente, especular,
    potencia y emision. None si no lo hay"""
    m, A = blq.m, blq.A; n4, o4 = reg[4]; n5, o5 = reg[5]
    if not 0 <= k < n4: return None
    mi = struct.unpack('<i', m[A + o4 + 36 * k:A + o4 + 36 * k + 4])[0]
    if not 0 <= mi < n5: return None
    return tuple(float(x) for x in np.frombuffer(m[A + o5 + 64 * mi:A + o5 + 64 * mi + 16], '<f4'))

def texturas_de_carteles(blq):
    """texturas de los carteles del bloque: apaisadas (2:1) en mallas de 4 a 6 vertices, de 6 a 16 m de ancho y 3 a 9 de alto"""
    m, A = blq.m, blq.A; res = []
    for reg in blq.registros():
        c, o = reg[3]; t2 = reg[2][1]; n4, o4 = reg[4]
        for i in range(c):
            e = struct.unpack('<6I', m[A + o + 24 * i:A + o + 24 * i + 24])
            k = struct.unpack('<i', m[A + t2 + 16 * i:A + t2 + 16 * i + 4])[0]
            if not 0 <= k < n4: continue
            ti = struct.unpack('<i', m[A + o4 + 36 * k + 12:A + o4 + 36 * k + 16])[0]
            if ti < 0 or ti in res: continue
            h = tex.cabeceras(blq)[ti] if ti < len(tex.cabeceras(blq)) else None
            if h is None or (h[11] >> 24) < (h[12] & 0xff) + 1: continue      # los anuncios son apaisados (2:1)
            for pk in vif.datos(m, A + e[2], A + blq.w[2]):
                for tr in vif.tiras(pk):
                    P = tr[0x68]; ext = P.max(0) - P.min(0)
                    if 4 <= len(P) <= 6 and 6 < max(ext[0], ext[2]) < 16 and 3 < ext[1] < 9 and ti not in res: res.append(ti)
    return res

class Modelo:
    def __init__(self):
        self.texs = []; self.blob = b''; self.mallas = []; self.verts = []; self.nv = 0; self.ruta = np.zeros((0, 5))
        self.rgba = []                                          # imagen de cada textura (para la compresion VQ)
        self.vq = os.environ.get("VQ", "1") != "0"
        # nodos del original (bloque con objetos=True): esfera en el marco del fichero, banderas para la demo (bits 0-3
        # umbral de LOD, bit 4 orientado a la camara, bits 8-9 rama de la union: 1 izquierda, 2 derecha) e indice en
        # self.carteles (los orientados a la camara: matriz del nodo y eje z del padre) o -1
        self.nodos = []; self.carteles = []
        self.gid = {}                                           # (id del bloque, registro, nodo) -> indice en self.nodos
        self.secciones = []                                     # por seccion de carretera: lista de nodos visibles
        self.mapa = None                                        # por punto de la ruta: su seccion (u16)
        self.tri_adc = 0; self.tri_total = 0                    # triangulos sin dibujar por la bandera ADC / total

    def bloque(self, blq, registros=None, lista=None, objetos=False, sin_textura=int(os.environ.get("SINTEX", "0x60202020"), 16),
               copias=None, vis=None, umbral=UMBRAL_CURSO, rama=None):
        """anade los registros de un bloque (todos si registros es None). lista: fuerza la lista de sus mallas.
        sin_textura: color ARGB de las mallas sin textura, o None para iluminarlas en blanco por la normal, o
        'material' para darles el color de su material del original (material_de).
        copias: {registro: [desplazamiento (x, y, z), ...]} para repetir un registro (ruedas).
        objetos: como el original, solo las tiras de los nodos (nodos), cada una con la matriz de su nodo; las tiras que
        no son de ningun nodo no se dibujan. vis: {registro: conjunto de nodos} (los que nombra la tabla de visibilidad
        por secciones de la etapa, lee_visibilidad): los demas no se dibujan nunca. umbral: el de LOD del original (6 la
        etapa; la bandera 0x40 del nodo lo sube a 7). rama: {registro: 1 izquierda / 2 derecha} (uniones)."""
        m, A = blq.m, blq.A; base = len(self.texs)
        for h in tex.cabeceras(blq):
            a = tex.imagen(blq, h); fmt, li, d = textura_dc(a)
            self.texs.append((a.shape[1], a.shape[0], fmt, li, len(self.blob), len(d))); self.blob += d
            self.rgba.append(a)
        ntb = len(self.texs) - base
        for r, reg in enumerate(blq.registros()):
            if registros is not None and r not in registros: continue
            c, o = reg[3]; t2 = reg[2][1]
            def tira_de(i):
                e = struct.unpack('<6I', m[A + o + 24 * i:A + o + 24 * i + 24])
                k = struct.unpack('<i', m[A + t2 + 16 * i:A + t2 + 16 * i + 4])[0]
                ti = struct.unpack('<i', m[A + reg[4][1] + 36 * k + 12:A + reg[4][1] + 36 * k + 16])[0] \
                    if 0 <= k < reg[4][0] else -1                # la lista de texturas del registro (tabla 4)
                ti = base + ti if 0 <= ti < ntb else -1
                self._mat = material_de(blq, reg, k) if sin_textura == 'material' and ti < 0 else None
                self._origen = (r, i, k, e[0])                   # registro, malla, entrada de la tabla 4 y banderas
                return e, ti
            if not objetos:
                self._nodo = -1
                for i in range(c):
                    e, ti = tira_de(i)
                    for pk in vif.datos(m, A + e[2], A + blq.w[2]):
                        for tr in vif.tiras(pk):
                            for dx in (copias or {}).get(r, [(0, 0, 0)]):
                                self._tira(tr, None, ti, lista, sin_textura, np.array(dx, np.float32), blq)
                continue
            visibles = None if vis is None else vis.get(r, set())
            for k, fl, W, (cc, rad), tiras, con_matriz in nodos(blq, reg):
                if visibles is not None and k not in visibles: continue
                if not tiras: continue
                cartel = bool(fl & 0x800) and con_matriz          # 404D10: face_camera solo si el nodo tiene matriz
                u = (7 if fl & 0x40 else umbral) | (0x10 if cartel else 0) | ((rama or {}).get(r, 0) << 8)
                ib = -1
                if cartel:                                       # vertices en el marco del nodo; la demo gira y coloca
                    ib = len(self.carteles); Wc = W.astype(np.float32).copy()
                    Wc[:3, 3] = (0, 0, 1); Wc[3, 3] = 1                  # columna 3: eje z del padre (el del bloque)
                    self.carteles.append(Wc)
                    rad = rad + float(np.linalg.norm(W[3, :3] - cc))
                self.gid[(id(blq), r, k)] = len(self.nodos)
                self._nodo = len(self.nodos); self.nodos.append([cc.astype(np.float32), rad, u, ib])
                for i in tiras:
                    e, ti = tira_de(i)
                    for pk in vif.datos(m, A + e[2], A + blq.w[2]):
                        for tr in vif.tiras(pk):
                            self._tira(tr, None if cartel else W, ti, lista, sin_textura, np.zeros(3, np.float32), blq)

    def _tira(self, tr, M, ti, lista, sin_textura, dx, blq):
        """una tira del VU1 partida en las tiras que dibuja de verdad el GS: el triangulo que cierra un vertice con la
        bandera ADC (vif.no_dibuja: bit 1 de la y de la posicion) no se dibuja; los demas seguidos forman una tira.
        M: matriz del nodo (vectores fila) o None"""
        P = tr[0x68]
        if len(P) < 3 or not np.isfinite(P).all(): return
        dib = vif.triangulos_dibujados(P)
        self.tri_total += len(dib); self.tri_adc += int((~dib).sum())
        if M is not None:
            tr = dict(tr); tr[0x68] = P.astype(float) @ M[:3, :3] + M[3, :3]
            if 0x69 in tr: tr[0x69] = tr[0x69].astype(float) @ M[:3, :3]
        j = 0; n = len(dib)
        while j < n:
            if not dib[j]: j += 1; continue
            a = j
            while j < n and dib[j]: j += 1
            self._trozo(tr, ti, lista, sin_textura, dx, blq, a, j + 2)   # triangulos a..j-1: vertices a..j+1

    def _trozo(self, tr, ti, lista, sin_textura, dx, blq, a, b):
        tr = {k: (v[a:b] if v is not None and k != 0x6c and len(v) == len(tr[0x68]) else v) for k, v in tr.items()}
        P = tr[0x68].astype(np.float32) + dx; nn = len(P)
        uv = np.zeros((nn, 2)); con_uv = False
        for cl in (0x6d, 0x65, 0x64):                           # UV en 1/4096 (16 o 32 bits)
            if cl in tr and len(tr[cl]) == nn: uv = tr[cl][:, :2].astype(float) / 4096; con_uv = True; break
        N = tr[0x69].astype(float) / 32768 if 0x69 in tr and len(tr[0x69]) == nn else np.zeros_like(P)
        if 0x6e in tr and len(tr[0x6e]) == nn:
            col = np.clip(tr[0x6e][:, :3].astype(float) * 2, 0, 255)
        else:
            luz = 0.7 + 0.3 * np.clip(np.abs(N @ LUZ), 0, 1)
            col = np.repeat(255 * luz[:, None], 3, 1)
        li = self.texs[ti][3] if ti >= 0 else 2
        mat = getattr(self, '_mat', None) if isinstance(sin_textura, str) else None
        if ti < 0 and isinstance(sin_textura, str):            # color del material (GS: 255 = blanco, 128 = alfa 1)
            if mat is None: return                               # sin material no hay color que dar: fuera
            luz = 0.7 + 0.3 * np.clip(np.abs(N @ LUZ), 0, 1)
            col = np.clip(np.array(mat[:3])[None, :] * luz[:, None], 0, 255)
            li = 0 if mat[3] >= 127 else 2
        elif ti < 0 and sin_textura is None: li = 0             # sin textura y sin color dado: blanco con luz
        col = col.astype(np.uint32)
        argb = (0xFF << 24) | (col[:, 0] << 16) | (col[:, 1] << 8) | col[:, 2]
        if ti < 0 and isinstance(sin_textura, str) and li == 2:
            argb = (np.uint32(min(255, int(mat[3] * 2))) << 24) | (argb & 0xFFFFFF)
        elif ti < 0 and sin_textura is not None and not isinstance(sin_textura, str): argb[:] = sin_textura
        if lista is not None: li = lista
        cc = P.mean(0); rad = float(np.sqrt(((P - cc) ** 2).sum(1).max()))
        self.mallas.append([li, ti, cc, rad, self.nv, nn, blq, getattr(self, '_nodo', -1)])
        if getattr(self, 'extra', None) is not None:            # coches (conv_coche.py): origen, UV y normales
            self.extra.append(dict(origen=getattr(self, '_origen', None), uv=con_uv, N=N.astype(np.float32)))
        v = np.zeros(nn, dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('u', '<f4'), ('v', '<f4'), ('c', '<u4')])
        v['x'], v['y'], v['z'], v['u'], v['v'], v['c'] = P[:, 0], P[:, 1], P[:, 2], uv[:, 0], uv[:, 1], argb
        self.verts.append(v); self.nv += nn

    def recorta_texturas(self):
        """deja solo las texturas que usan las mallas (y reordena el blob)"""
        usadas = sorted({x[1] for x in self.mallas if x[1] >= 0}); nuevo = {}; blob = b''; texs = []
        for ti in usadas:
            w, h, fmt, li, off, tam = self.texs[ti]
            nuevo[ti] = len(texs); texs.append((w, h, fmt, li, len(blob), tam)); blob += self.blob[off:off + tam]
        for x in self.mallas: x[1] = nuevo.get(x[1], -1)
        self.rgba = [self.rgba[ti] for ti in usadas]
        self.texs, self.blob = texs, blob

    def comprime(self):
        """pasa a VQ las texturas que lo merecen (vq.elige) y rehace el blob; solo las que usa alguna malla. Con
        VQ_PNG=directorio guarda cada textura antes y despues para compararlas"""
        import vq
        usadas = {x[1] for x in self.mallas if x[1] >= 0}; blob = b''; texs = []; t0 = time.time(); antes = ahora = 0
        dpng = os.environ.get("VQ_PNG"); errs = []
        for ti, (w, h, fmt, li, off, tam) in enumerate(self.texs):
            d = self.blob[off:off + tam]
            if ti in usadas and ti < len(self.rgba):
                antes += tam
                r = vq.elige(self.rgba[ti], fmt)
                if r is not None:
                    d, n, err = r; fmt = fmt | 0x100 | (n - 1) << 16; errs.append(err)
                    if dpng: vq.compara(self.rgba[ti], d, fmt, os.path.join(dpng, 'tex%03d_%dx%d.png' % (ti, w, h)))
                ahora += len(d)
            d += b'\0' * (-len(d) % 32)
            texs.append((w, h, fmt, li, len(blob), len(d))); blob += d
        self.texs, self.blob = texs, blob
        if errs: print('VQ: %d de %d texturas, %d KB -> %d KB, error RMS medio %.1f (max %.1f) en %.0f s'
                       % (len(errs), len(usadas), antes >> 10, ahora >> 10, np.mean(errs), max(errs), time.time() - t0))

    def escribe(self, sal):
        if self.vq and not getattr(self, '_comprimido', False): self.comprime(); self._comprimido = True
        self.mallas.sort(key=lambda x: (x[0], x[1]))
        inicio = [sum(1 for x in self.mallas if x[0] < l) for l in range(5)]
        nsec = len(self.secciones) if self.mapa is not None else 0
        with open(sal, 'wb') as f:
            f.write(b'ORD7' + struct.pack('<13I', len(self.mallas), self.nv, len(self.ruta), self.ruta.shape[1],
                                          len(self.texs), *inicio, len(self.nodos), len(self.carteles), nsec))
            for t in self.texs: f.write(struct.pack('<6I', *t))
            for li, ti, cc, rad, i0, cnt, _, nodo in self.mallas:
                f.write(struct.pack('<4f2Iii', *cc, rad, i0, cnt, ti, nodo if isinstance(nodo, (int, np.integer)) else -1))
            for v in self.verts: f.write(v.tobytes())
            f.write(self.ruta.astype('<f4').tobytes())
            for cc, rad, u, ib in self.nodos: f.write(struct.pack('<4fIi2I', *cc, rad, u, ib, 0, 0))
            for W in self.carteles:
                f.write(W.astype('<f4').tobytes())
            if nsec:
                ini = np.cumsum([0] + [len(L) for L in self.secciones]).astype('<u4')
                f.write(ini.tobytes())
                f.write(np.array([g for L in self.secciones for g in L], '<u2').tobytes())
                f.write(np.asarray(self.mapa, '<u2').tobytes())
                f.write(b'\0' * (-f.tell() % 4))
            f.write(b'\0' * (-f.tell() % 32)); f.write(self.blob)
        return inicio
