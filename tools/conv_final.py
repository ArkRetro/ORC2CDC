#!/usr/bin/env python3
"""conv_final.py <iso> <dir_salida>: el final de recreativa de OUTRUN2SP (modo 24 del original) para demo/final.c.

Salida (nombres nuevos; no toca ningun fichero que ya hubiera):
 - FINAL.DAT: tablas del modo 24 del ELF y los guiones AUTOSCENE (AS.PS2 \\AS\\as_*_bin.sz) de los finales del juego
   OutRun de OUTRUN2SP (variante 1, presets 0 / 1: las columnas 0..9).
 - FINEc.DAT (c = columna 0..9): el modelo del sitio del final (obj_end_5a..5e, east, maya, prin, flor, newy de OBJ.PS2;
   44B890 lo dibuja en el origen) en ORD7 (tools/modelo.py, como las uniones de la meta).
 - FINPROP.DAT: obj_end_property (recurso 0xD0: cajas, MOAI, PILLAR, OBENTO...), un registro por objeto (en el campo del
   nodo de cada malla el registro).
 - STAFF.DAT: el banco SPRANI 0x43 (STAFFROLL_CVT: los creditos que pasan sobre la escena, capa 7) entero para
   comun/sprani.c (tools/conv_sprani.py), un grupo de atlas por escena.

Origen (SLES-53998; PC race_ending.cpp / race_autoscene.cpp contrastados instruccion a instruccion):
 - Modo 24: tabla de modos 0x3f8e50 = 0x1ba4d8 / 0x1ba5f0 / 0x1ba6a0 (PC 49A660 / 49A710 / 4527A0). 0x19b6d0 (452B10)
   pone el temporizador [0x5a2020] = 8; al acabar, 0x19bae0 (4527F0) elige el final: columna = (450380(8) - 0xA) % 5
   ([0x35c67c]), + 5 con los presets 0 / 2 (0x19bb54..0x19bb6c); en la variante 1 id = [0x3e9048 + columna x 4]
   (lui 0x3f / addiu -0x6fb8 en 0x19bbb4 / 0x19bbb8: PC 5A4624) y el juego de movimientos [0x35c678] = 1.
 - Fila del final (PS2 0x3e7910 + id x 0x4C, PC 5A4830): +0 la escena AUTOSCENE, +8.. 15 recursos, +0x44 el grupo de
   movimientos (4F2020), +0x48 el coche (suceso 8 con la funcion 0x2D si no es 0).
 - Escena 2D: 0x19b874..0x19b890: 0x114b68([0x35c5c8 + id x 4], 7, 1) (PC 638DF0: 428320(token, capa 7, modo 1)).
 - Sitio: 0x19be14..0x19be3c: el recurso de [0x3e7780 + (b + columna x 2) x 8] (PC 5A46A0) y su nombre (+4: el
   END_*_STAGE de object_db_bin, PC 452340).
 - AUTOSCENE: filas {tipo, ruta, extra} en 0x3b5680 + escena x 12 (lui 0x3b / addiu 0x5680 en 0x28b8f8 / 0x28b900; PC
   717D68). El guion (.sz sin su palabra de tamano: los punteros son relativos a ese punto) se guarda tal cual salvo el
   campo +8 de los nodos de las entradas de tipo 1, que lleva ya el token de 4B6510 (lo que el original resuelve al
   empezar la escena): piezas del coche (nombres con "CAR": BD0000..BD001A por la lista de 0x422438..), ojos (con
   "EYE": BD001C..BD001F), "KEM" (BD0020, el humo) y si no, el de object_db_bin (\\COMMON\\object_db_bin.sz, PC 448CD0:
   43 grupos {nombre, token} hasta -1).
Bloque de FINAL.DAT (u32 little endian): 'FNL1', n ids (0x26), por id {escena, grupo, coche, token 2D}, 10 ids de la
variante 1 (por columna), 10 x 2 {recurso del sitio, 0}, n escenas (0x38), por escena {desplazamiento, bytes} (0 si no
esta) y los guiones (cada uno alineado a 4).
La pareja del final (CHR_MAL / CHR_FAL con los movimientos del grupo de la fila, sus caras 514830, manos 514800 y ojos
514880) no se convierte: SIN PORTAR (docs/PARIDAD.md, OUTRUN2SP: Final de recreativa).
Sale de tu disco: no se distribuye."""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.assets import Assets
import modelo

IDS_V1, FILAS, TOK2D, SITIO, AS_TABLA = 0x3e9048, 0x3e7910, 0x35c5c8, 0x3e7780, 0x3b5680
N_IDS, N_AS = 0x26, 0x38
COCHE_PIEZAS = 0x422438          # "CAR", "_BODY", "_DOOR_L"... (cadenas de 8 / 16 bytes, PC 5C6A54.. al reves)

def ins(E, va): return E.u32(va)
def comprueba(E):
    """las direcciones salen del codigo del PS2"""
    hi = lambda va: ins(E, va) & 0xffff
    assert (hi(0x19bbb4), hi(0x19bbb8)) == (0x3f, (-0x6fb8) & 0xffff), 'ids v1'
    assert hi(0x19bdb8) == 0x7910 and hi(0x19bdac) == 0x3e, 'filas'
    assert hi(0x19b87c) == (-0x3a38) & 0xffff and hi(0x19b878) == 0x36, 'tokens 2D'
    assert hi(0x19be28) == 0x7780 and hi(0x19be20) == 0x3e, 'sitio'
    assert hi(0x28b8f8) == 0x3b and hi(0x28b900) == 0x5680, 'filas de AUTOSCENE'

def object_db(A):
    d = A.datos(A.entrada('COMMON.PS2', 'object_db_bin'))[4:]
    db = {}
    for g in range(0, 0xac, 4):
        e = struct.unpack_from('<I', d, g)[0]
        while True:
            o, h = struct.unpack_from('<2I', d, e)
            if h == 0xffffffff: break
            db[d[o:d.index(b'\0', o)].decode('latin1').upper()] = h; e += 8
    return db

def piezas_coche(E):
    """[(subcadena, token)] de 4B5FF0 (PS2: las cadenas de 0x4223f0; el orden y los tokens del PC)"""
    s = []; va = COCHE_PIEZAS
    while len(s) < 28:
        c = E.cadena(va); s.append(c); va += (len(c) + 8) & ~7
    assert s[0] == 'CAR' and s[1] == '_BODY', s[:3]
    tok = [0, 1, 2, 3, 4, 5, 6, 0x1a, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
           0x17, 0x18, 0x19]
    return [(c, 0xbd0000 | t) for c, t in zip(s[1:], tok)]

OJOS = (('MAL_R', 0xbd001c), ('MAL_L', 0xbd001d), ('FAL_R', 0xbd001e), ('FAL_L', 0xbd001f), ('GAL_R', 0xbd001e),
        ('GAL_L', 0xbd001f))     # 4B6320: las cadenas de 0x5c6a80.. del PC (el orden de las comparaciones)

def token_nodo(nombre, db, piezas):
    n = nombre.upper()
    t = db.get(n, 0xffffffff)
    if 'CAR' in n:
        for c, k in piezas:
            if c in n: t = k; break
    if 'EYE' in n:
        for c, k in OJOS:
            if c in n: t = k; break
    if 'KEM' in n: t = 0xbd0020
    return t

def guion(d, db, piezas, props, movs):
    """el guion con los tokens de sus nodos; props: registros de 0xD0 usados; movs: ids de los sucesos 1"""
    d = bytearray(d)
    U = lambda o: struct.unpack_from('<I', d, o)[0]
    def cs(o): return d[o:d.index(b'\0', o)].decode('latin1')
    def nodo(o):
        nm = U(o + 0xc)
        if nm:
            t = token_nodo(cs(nm), db, piezas); struct.pack_into('<I', d, o + 8, t)
            if t >> 16 == 0xd0: props.add(t & 0xffff)
        for i in range(struct.unpack_from('<i', d, o + 0x3c)[0]): nodo(U(o + 0x40) + 0x44 * i)
    def sucesos(o):
        if not o: return
        while U(o) != 0xffffffff:
            if U(o + 4) == 1: movs.add(U(o + 0xc))
            o += U(o + 8) * 4
    n, ent = U(8), U(0xc)
    for i in range(n):
        k, p1, p2 = struct.unpack_from('<3I', d, ent + 12 * i)
        if k == 1: nodo(p1)
        if k == 2: sucesos(p2)
    return bytes(d)

def main():
    if len(sys.argv) < 3: raise SystemExit(__doc__)
    A = Assets(sys.argv[1]); E = A.elf; sal = sys.argv[2]; os.makedirs(sal, exist_ok=True)
    comprueba(E)
    db = object_db(A); piezas = piezas_coche(E)
    R = 0x369c38                                       # tabla de ficheros por recurso (conv_coche.recursos)
    ids = list(E.u32(IDS_V1, 10))
    filas = [E.u32(FILAS + 0x4c * i, 19) for i in range(N_IDS)]
    tok2d = list(E.u32(TOK2D, N_IDS))
    out = bytearray(b'FNL1') + struct.pack('<I', N_IDS)
    for i in range(N_IDS): out += struct.pack('<4I', filas[i][0], filas[i][17], filas[i][18], tok2d[i])
    out += struct.pack('<10I', *ids)
    sitios = []
    for c in range(10):
        r0, n0 = E.u32(SITIO + 16 * c, 2); sitios.append(r0)
        out += struct.pack('<2I', r0, 0)
        assert db.get(E.cadena(n0).upper()) == r0 << 16, 'sitio %d: %s' % (c, E.cadena(n0))
    # guiones de las columnas 0..9 de la variante 1
    escenas = {filas[i][0]: filas[i][17] for i in ids}
    datos = {}; props = set()
    for esc, grupo in sorted(escenas.items()):
        k, ruta, _ = E.u32(AS_TABLA + 12 * esc, 3)
        assert k == 1 and ruta, 'escena %x' % esc
        nm = E.cadena(ruta).split('\\')[-1].replace('.sz', '')
        d = A.datos(A.entrada('AS.PS2', nm))[4:]
        movs = set(); datos[esc] = guion(d, db, piezas, props, movs)
        print('escena %02x %s: %d bytes, grupo %x, movimientos %s' % (esc, nm, len(d), grupo, ' '.join('%x' % m for m in sorted(movs))))
    out += struct.pack('<I', N_AS)
    tab = len(out); out += b'\0' * 8 * N_AS
    for esc, d in sorted(datos.items()):
        while len(out) % 4: out.append(0)
        struct.pack_into('<2I', out, tab + 8 * esc, len(out), len(d)); out += d
    open(os.path.join(sal, 'FINAL.DAT'), 'wb').write(out)
    print('FINAL.DAT: %d bytes' % len(out))
    # sitios
    for c, r0 in enumerate(sitios):
        nm = E.cadena(E.u32(R + 4 * r0)).split('\\')[-1].replace('.sz', '')
        mod = modelo.Modelo()
        mod.bloque(A.bloque(A.entrada('OBJ.PS2', nm)), objetos=True, sin_textura='material')
        mod.recorta_texturas(); f = os.path.join(sal, 'FINE%d.DAT' % c); mod.escribe(f)
        print('%s: %s, %d mallas, %d texturas, %d KB' % (os.path.basename(f), nm, len(mod.mallas), len(mod.texs), os.path.getsize(f) >> 10))
    # objetos (recurso 0xD0)
    blq = A.bloque(A.entrada('OBJ.PS2', 'obj_end_property_pmt'))
    mod = modelo.Modelo(); base = None
    for r in sorted(props):
        mod.bloque(blq, registros={r}, sin_textura='material', nodo_fijo=r, base_tex=base); base = mod.base_tex
    mod.recorta_texturas(); f = os.path.join(sal, 'FINPROP.DAT'); mod.escribe(f)
    print('FINPROP.DAT: registros %s, %d mallas, %d KB' % (sorted(props), len(mod.mallas), os.path.getsize(f) >> 10))
    # creditos
    import conv_hud as ch, conv_sprani as cs
    D = ch.Disco(sys.argv[1])
    nb, _, _ = D.banco(0x43)
    cs.escribe(D, 0x43, os.path.join(sal, 'STAFF.DAT'), [('S%d' % k, [k]) for k in range(len(nb.escenas))], vq=ch.vq_pequeno)

if __name__ == '__main__':
    main()
