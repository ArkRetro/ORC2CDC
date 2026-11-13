"""conv_bo.py tu.iso salida/: los objetos animados del fondo de las uniones (pajaros, halcon, flamencos, el dragon...)
para la demo: BO.DAT (tablas y trayectorias) y BOnn.DAT (el modelo del conjunto nn, formato ORD7 de modelo.py).
Sale de tu disco: no se distribuye.

Como lo hace el original (decompilacion del PC race_area.cpp race_area_objects_44f190 / race_area_ending_44d410 y
race_traffic_objects.inc 4A9BD0 / 4A9C80 / 4A9DF0; SLES-53998 0x174328 / 0x174a48, comprobados instruccion a
instruccion):
 - al entrar el coche en una union que no es la meta (car+0x5C pasa a 1) 0x174a48 elige un conjunto con las cuentas de
   la etapa que se deja (44EDE0): fila de la variante (0x443508) de la tabla 0x35a738 (7 x u32 por variante): en la
   variante 1 (OutRun) [2 pajaros sueltos, 1 bandada, 13 halcones, 12 flamencos, 11 dragon (obj_bo_waib), 7 pegaso, 6 tanque].
 - conjuntos: 14 registros de 0x24 bytes en 0x35a540 (PC 6369C0): +0 recurso del modelo (0x136.. = BK\\ANIMS\\obj_bo_*),
   +4 colocacion (0: ninguna), +8 ruta de la trayectoria (BK\\ANIMS\\oso_dyn_bo_*_bin.sz), +0xC objetos, +0x10 posicion
   de carretera de la union a la que salen (i16), +0x12 banco de sonido, +0x14 velocidad lateral (m/cuadro), +0x18 /
   +0x1C funciones de sonido por cuadro y al acabar, +0x20 los objetos (0x40 bytes: el registro de 0x28 de
   colocacion con su clase +0x18 y su trayectoria +0x24, banderas +0x28 (1 sigue al coche en x, 2 z fija, 4 cruza a
   +0x14 por cuadro, 8 en bucle), retraso +0x2C en segundos).
 - la clase de cada objeto da su modelo en la tabla 0x40a958 (8 bytes por clase: recurso << 16 | registro, radio).
 - trayectoria (oso_dyn: entradas de 8 bytes {desplazamiento, duracion} y en cada una 10 canales {i16 n, u32 claves}
   de los que se usan 6: x, y, z, giro x, y, z; claves de 16 bytes como las animaciones de choque, 513650).
 - los nodos con la bandera 0x20 llevan animacion de vertices: sus tiras (bandera 0x800 en la entrada de la tabla 3)
   traen 4 paquetes (cuadros); el cuadro sale de la tabla de 0x328aa8 (la morph_var del PC, 40ED70: grupos {indice,
   k, m, m floats}; el indice de animacion del nodo +0x18 da {base, m, valores}: f = valores[(base + reloj) % m], reloj
   +1 por cuadro dibujado, 0x1030d0 desde el bucle 0x20d7f8).
BO.DAT: 'BOT1' u32 n, por conjunto 64 bytes: u16 posicion, u8 objetos, u8 recurso - 0x136, f32 velocidad lateral,
  4 x {u8 registro del modelo, u8 trayectoria, u8 banderas, u8 0, f32 retraso}, u32 desplazamiento de su trayectoria
  en el fichero (0 sin ella), relleno; 'VAR1' 7 u32 la fila de la variante 1; 'MOR1' u32 grupos y los grupos tal cual
  (u32 indice, k, m, m f32); al final del fichero 'VAR6' 7 u32 la fila de la variante 6 (misiones de C2C, 0x174cd0).
  Trayectoria: u32 n entradas, por entrada f32 duracion y 6 canales (u32 n claves, n x 4 f32).
BOnn.DAT: ORD7 sin ruta: mallas del cuadro 0 de cada malla del original g en el marco de su malla, campo del nodo
  de la malla = g (los cuadros 1..3 en la cola BOF1, compacta_cuadros); nodos = los nodos de los registros que usa el conjunto (esfera, banderas del original << 16 |
  registro, cartel = su matriz en carteles (vectores fila, con las de los padres), relleno = g | indice de animacion
  << 16 (0xFFFF sin animacion))."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.assets import Assets
import modelo, conv_coche

NODO = 56

def u32(E, a): return E.u32(a)

def conjuntos(A):
    """(direccion de los registros de 0x24, de la tabla por variante, de la tabla de clases), localizados por contenido:
    el registro 1 lleva la ruta de la trayectoria de la bandada y el 2 la de los sueltos"""
    E = A.elf
    ra = E.busca(b'\\BK\\ANIMS\\oso_dyn_bo_bird_group_bin.sz\0')
    if not ra: raise SystemExit('no se encuentra la ruta de la trayectoria de los pajaros')
    ref = E.referencias(ra[0])
    if not ref: raise SystemExit('nadie apunta a la trayectoria de los pajaros')
    regs = ref[0] - 8 - 0x24                    # el campo +8 del registro 1
    var = regs + 14 * 0x24                       # la tabla por variante va detras de los 14 registros
    clases = None
    for h in E.busca(struct.pack('<If', 0x1370000, 15.5)):  # la clase 0x24 (la bandada) en la tabla de clases
        if h % 4 == 0: clases = h - 0x24 * 8; break
    if clases is None: raise SystemExit('no se encuentra la tabla de clases de objetos')
    return regs, var, clases

def morph(A):
    """la tabla de animacion de vertices (0x328aa8): u32 grupos y los grupos {indice, k, m, m floats}; se localiza por
    el grupo de los pajaros (indice 80, k 7, m 16): la cuenta de grupos es la palabra desde la que, grupo a grupo, se
    llega a el"""
    E = A.elf
    for h in E.busca(struct.pack('<3I', 80, 7, 16)):
        for a in range(h - 4, h - 0x8000, -4):
            n = u32(E, a)
            if not 0 < n < 256: continue
            b = a + 4; inicios = []; ok = True
            for g in range(n):
                idx, k, m = u32(E, b), u32(E, b + 4), u32(E, b + 8)
                if not (idx < 0x400 and k <= 64 and 0 < m < 4096): ok = False; break
                inicios.append(b); b += 12 + 4 * m
            if ok and h in inicios: return E.lee(a, b - a)
    raise SystemExit('no se encuentra la tabla de animacion de vertices')

def trayectoria(d):
    """oso_dyn (ya sin la palabra de tamano del .sz): entradas {desplazamiento, duracion} hasta la palabra 0"""
    out = b''; ents = []; o = 0
    while True:
        p, dur = struct.unpack_from('<If', d, o)
        ents.append((p, dur))
        if o + 8 + 4 > len(d) or struct.unpack_from('<I', d, o + 8)[0] == 0: break
        o += 8
    out += struct.pack('<I', len(ents))
    for p, dur in ents:
        out += struct.pack('<f', dur)
        for c in range(6):
            n, _, k = struct.unpack_from('<hhI', d, p + 8 * c)
            if n <= 0: out += struct.pack('<I', 0); continue
            out += struct.pack('<I', n) + d[k:k + 16 * n]
    return out

def nodos(blq, reg):
    """nodos del registro como modelo.nodos, con su malla y su indice de animacion: [(k, banderas, W, (c, r), g, anim)]"""
    m, A = blq.m, blq.A
    n0, o0 = reg[0]; n6, o6 = reg[6]
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
            res.append((k, x[0], W, (c, r), x[10], x[6]))
            if x[8] >= 0: anda(x[8], W)
            k = x[9]
    anda(0, None)
    return res

def tiras_de_malla(blq, reg, g):
    """tiras (tabla 3) de la malla g (tabla 1: n opacas, n de mezcla, primera opaca, primera de mezcla)"""
    m, A = blq.m, blq.A; n1, o1 = reg[1]; n3 = reg[3][0]
    if not 0 <= g < n1: return []
    nop, nmez, pop, pmez = struct.unpack('<4i', m[A + o1 + 16 * g:A + o1 + 16 * g + 16])
    t = []
    for n, p in ((nop, pop), (nmez, pmez)):
        if n > 0 and p >= 0: t += [i for i in range(p, p + n) if i < n3]
    return t

def cuadros_de(blq, reg, tiras):
    m, A = blq.m, blq.A; c, o = reg[3]; nf = 1
    for i in tiras:
        e = struct.unpack('<6I', m[A + o + 24 * i:A + o + 24 * i + 24])
        nf = max(nf, 1 + sum(1 for x in e[3:6] if x))
    return nf

def modelo_conjunto(A, blq, registros, sal):
    """BOnn.DAT con los registros usados del bloque"""
    mod = modelo.Modelo(); base = None; hechas = {}; firmas = {}
    regs = blq.registros()
    for r in sorted(registros):
        reg = regs[r]
        for k, fl, W, (cc, rad), g, anim in nodos(blq, reg):
            t = tiras_de_malla(blq, reg, g)
            if not t: continue
            if (r, g) not in hechas:                 # la malla g del registro r en su marco, en cada cuadro
                nf = cuadros_de(blq, reg, t) if fl & 0x20 else 1
                codigo = len(set(hechas.values())); k0, v0 = len(mod.mallas), mod.nv
                for f in range(nf):
                    mod.bloque(blq, registros={r}, sin_textura='material', nodo_fijo=codigo | (f << 8),
                               tiras=set(t), cuadro=f, base_tex=base, con_gs=True)
                    base = mod.base_tex
                # la misma malla en otro registro (los tres halcones): se usa la que ya estaba
                firma = hash(b''.join(v.tobytes() for v in mod.verts[k0:]) + bytes(str([x[:2] for x in mod.mallas[k0:]]), 'ascii'))
                if firma in firmas:
                    del mod.mallas[k0:]; del mod.verts[k0:]; mod.nv = v0; codigo = firmas[firma]
                else: firmas[firma] = codigo
                hechas[(r, g)] = codigo
            Wc = (np.eye(4) if W is None else W).astype(np.float32)
            ib = len(mod.carteles); mod.carteles.append(Wc)
            mod.nodos.append([cc.astype(np.float32), rad, (fl << 16) | r, ib,
                              hechas[(r, g)] | ((anim if fl & 0x20 and anim >= 0 else 0xffff) << 16)])
    mod.recorta_texturas()
    mod.escribe(sal)
    compacta_cuadros(sal)
    return mod

def compacta_cuadros(sal):
    """deja en el ORD7 solo las mallas del cuadro 0 y pone los cuadros 1..3 al final (BOF1): por vertice de las mallas
    del cuadro 0 de cada malla del original g (en el orden del fichero) su posicion en int16 x escala y su color. Las
    mallas de cada cuadro casan una a una (en el orden del fichero) en vertices y textura con las del cuadro 0; las UV
    son las mismas. El dragon (BO11) pasa de 276 a 167 KB (no cabia en la RAM en la union).
    Cola: 'BOF1' u32 grupos, f32 escala; por grupo u32 g, cuadros, vertices del cuadro 0 y, por cuadro 1..cuadros-1,
    vertices x (3 int16 + 2 de relleno) y vertices x u32 color; al final u32 desplazamiento de 'BOF1' y 'BOFT'."""
    d = open(sal, 'rb').read()
    h = list(struct.unpack_from('<13I', d, 4))
    nm, nv, nr, pr, nt = h[:5]; nn, nc, ns = h[10:13]
    assert nr == 0 and ns == 0
    o = 56 + 24 * nt; texs = d[56:o]
    M = [list(struct.unpack_from('<4f2Iii', d, o + 32 * i)) for i in range(nm)]; o += 32 * nm
    V = np.frombuffer(d, dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('u', '<f4'), ('v', '<f4'), ('c', '<u4')],
                      count=nv, offset=o); o += 24 * nv
    resto = d[o:o + 32 * nn + 64 * nc]; o += 32 * nn + 64 * nc
    o = (o + 31) & ~31; blob = d[o:]
    grupos = {}
    for i, m in enumerate(M): grupos.setdefault(m[7] & 0xff, {}).setdefault((m[7] >> 8) & 3, []).append(i)
    if all(len(fs) == 1 for fs in grupos.values()): return
    mx = max(float(np.abs(np.stack([V['x'], V['y'], V['z']])).max()), 1e-6); esc = mx / 32767.0
    # los grupos cuyos cuadros no casan malla a malla con el 0 (obj_bo_figh, el caza de las misiones: las tiras de
    # cada paquete salen distintas) se quedan con el cuadro 0, sin animacion de vertices (DESVIACION, docs/PARIDAD.md)
    casan = [g for g in sorted(grupos) if sorted(grupos[g]) == list(range(len(grupos[g]))) and
             all([(M[i][5], M[i][6]) for i in grupos[g][f]] == [(M[i][5], M[i][6]) for i in grupos[g][0]]
                 for f in range(1, len(grupos[g])))]
    for g in sorted(set(grupos) - set(casan)): print('  grupo %d: %d cuadros que no casan, solo el 0' % (g, len(grupos[g])))
    cola = b'BOF1' + struct.pack('<If', len(casan), esc)
    for g in casan:
        fs = grupos[g]; nf = len(fs); m0 = fs[0]
        n0 = sum(M[i][5] for i in m0)
        cola += struct.pack('<3I', g, nf, n0)
        for f in range(1, nf):
            v = np.concatenate([V[M[i][4]:M[i][4] + M[i][5]] for i in fs[f]])
            q = np.zeros((n0, 4), '<i2')
            for k, c in enumerate('xyz'): q[:, k] = np.clip(np.round(v[c] / esc), -32767, 32767)
            cola += q.tobytes() + v['c'].astype('<u4').tobytes()
    # el ORD7 con las mallas del cuadro 0 (en su orden) y sus vertices
    nuevas, verts, nv2 = [], [], 0
    for i, m in enumerate(M):
        if (m[7] >> 8) & 3: continue
        verts.append(V[m[4]:m[4] + m[5]]); m = list(m); m[4] = nv2; nv2 += m[5]; nuevas.append(m)
    inicio = []
    # las listas: los inicios se cuentan otra vez (las mallas siguen ordenadas por lista)
    lis = [h[5 + l] for l in range(5)]
    for l in range(5): inicio.append(sum(1 for i in range(nm) if i < lis[l] and not (M[i][7] >> 8) & 3))
    h[0], h[1] = len(nuevas), nv2; h[5:10] = inicio
    out = bytearray(b'ORD7' + struct.pack('<13I', *h) + texs)
    for m in nuevas: out += struct.pack('<4f2Iii', *m)
    for v in verts: out += v.tobytes()
    out += resto; out += b'\0' * (-len(out) % 32); out += blob
    out += b'\0' * (-len(out) % 4)
    pos = len(out); out += cola; out += b'\0' * (-len(out) % 4); out += struct.pack('<I', pos) + b'BOFT'
    with open(sal, 'wb') as fh: fh.write(out)

def main():
    arg = [a for a in sys.argv[1:] if not a.startswith('--')]
    A = Assets(arg[0]); dsal = arg[1]; E = A.elf
    regs, var, clases = conjuntos(A)
    R = conv_coche.recursos(A)
    fila1 = [u32(E, var + 0x1c * 1 + 4 * i) for i in range(7)]   # variante 1 (OutRun)
    fila6 = [u32(E, var + 0x1c * 6 + 4 * i) for i in range(7)]   # variante 6 (misiones de COAST 2 COAST)
    print('variante 1:', fila1, 'variante 6:', fila6)
    cab = b''; tray = b''; cab_tam = 64; usados = set(fila1) | set(fila6)
    for c in range(14):
        a = regs + 0x24 * c
        res, colo, ruta, n = u32(E, a), u32(E, a + 4), u32(E, a + 8), u32(E, a + 0xc)
        pos = struct.unpack('<h', E.lee(a + 0x10, 2))[0]; lim = E.f32(a + 0x14); arr = u32(E, a + 0x20)
        assert colo == 0 and n <= 4
        insts = []
        for i in range(n):
            q = arr + 0x40 * i
            kind, anim, fl, toff = u32(E, q + 0x18), u32(E, q + 0x24), u32(E, q + 0x28), E.f32(q + 0x2c)
            mid = u32(E, clases + 8 * kind)
            assert mid >> 16 == res, 'clase %x de otro recurso' % kind
            insts.append((mid & 0xffff, anim, fl, toff))
        b = struct.pack('<hBBf', pos, n, res - 0x136, lim)
        for i in range(4):
            r, an, fl, toff = insts[i] if i < n else (0, 0, 0, 0.0)
            b += struct.pack('<3Bxf', r, an, fl, toff)
        off = 0
        if ruta and c in usados:
            nm = E.cadena(ruta).split('\\')[-1]
            d = A.datos(A.entrada('BK.PS2', '\\ANIMS\\' + nm))[4:]
            off = len(tray); tray += trayectoria(d)
        b += struct.pack('<I', off if ruta and c in usados else 0xffffffff)
        cab += b + b'\0' * (cab_tam - len(b))
        nmod = E.cadena(u32(E, R + 4 * res)).split('\\')[-1]
        if c in usados:
            blq = A.bloque(A.entrada('BK.PS2', '\\ANIMS\\' + nmod))
            mod = modelo_conjunto(A, blq, {x[0] for x in insts}, os.path.join(dsal, 'BO%02d.DAT' % c))
            print('conjunto %2d %-28s pos %3d objetos %d lateral %.3f: %d mallas, %d vertices, %d nodos, %d texturas (%d KB), '
                  '%d KB' % (c, nmod, pos, n, lim, len(mod.mallas), mod.nv, len(mod.nodos), len(mod.texs),
                             len(mod.blob) >> 10, os.path.getsize(os.path.join(dsal, 'BO%02d.DAT' % c)) >> 10))
    mo = morph(A)
    cuerpo = b'BOT1' + struct.pack('<I', 14) + cab + b'VAR1' + struct.pack('<7I', *fila1) + b'MOR1' + mo
    cuerpo += b'\0' * (-len(cuerpo) % 4)
    base = len(cuerpo) + 8
    out = cuerpo + b'TRA1' + struct.pack('<I', len(tray)) + tray
    # los desplazamientos de las trayectorias, desde el principio del fichero
    out = bytearray(out)
    for c in range(14):
        p = 8 + c * cab_tam + 8 + 4 * 8                  # tras u16 pos, u8 n, u8 recurso, f32 y los 4 objetos
        o, = struct.unpack_from('<I', out, p)
        if o != 0xffffffff: struct.pack_into('<I', out, p, o + base)
    out += b'VAR6' + struct.pack('<7I', *fila6)       # (al final: la demo de antes no lo lee)
    with open(os.path.join(dsal, 'BO.DAT'), 'wb') as f: f.write(out)
    print('BO.DAT: %d bytes (trayectorias %d)' % (len(out), len(tray)))

if __name__ == '__main__':
    main()
