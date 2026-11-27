#!/usr/bin/env python3
"""conv_camp.py <iso> <dir_salida>: los campeonatos de COAST 2 COAST (variante 5, las peticiones de la acompanante en
los grupos 4..6 de las categorias) -> CAMP.DAT para comun/campeonato.c. Sale de tu disco: no se distribuye.

Del original (SLES-53998): el gestor es un suceso (0x190: inicio 0x229e88, control 0x22a048, dibujo 0x22a2b0, fin
0x22b470; objeto en 0x616238) que por cada etapa lee el guion Req_<etapa>.bin de SCRIPTS.PS2 (nombres 0x374010 por el
tipo de la etapa, listas REQUEST_NORMAL / SPECIAL1..3 de 0x374138, 0x22ac00 / 0x22b600): cada peticion es un
registro de 0x74 bytes {s16 inicio, s16 fin (posiciones de carretera de la etapa), ..., +0x58 puntero a sus
parametros}; los parametros (bloques de 0x88 con su propio nombre en el guion) {+0 tipo 1..24, +4 subtipo, +0x5c
puntero a los coches de la peticion, +0x60.. enteros / flotantes que lee el tipo, p. ej. +0x7c la velocidad del tipo
1 o el % del rebufo del tipo 12, +0x80 / +0x84 los coches del tipo 3}. Aqui no se copian guiones ni tablas al codigo:
se sacan del disco y del ELF (cada tabla con su direccion, comprobada por el codigo que la lee).

CAMP.DAT (little endian):
  'CMP1', u32 version 1, u32 n_pet, u32 desplazamiento de las peticiones
  f32 umbral[7]   (0x374190: nota k si el medidor menos la penalizacion >= umbral[k]; 0x22e670)
  i32 tope        (0x3741c8: tope de los corazones de la peticion; 0x22e728)
  i32 extra_lista[4] (0x3741d0: corazones de mas con nota >= 4 por la lista de la etapa)
  i32 extra_dif[5]   (0x3741e0: por la dificultad +0xc de la licencia, tope 4)
  i32 millas[7]      (0x3741f8: millas por la nota de la etapa, x (lista + 1); 0x22edd8)
  f32 penal[17][2][6] (0x374240: penalizaciones por tipo / subtipo {choque, golpe de uno mas rapido, golpe, muro x
                       dt, rueda fuera x dt, roce x dt}; 0x2323e8 elige la fila, 0x22c818 las suma)
  u16 voz_nota[4][7] (0x423df0: voz del fin de la peticion por fila +0xf0 del gestor y nota; 0x2a65a8)
  u16 voz_animo[2][5] (0x423ee0 tipos 1 y 2, 0x423ef8 los demas; 0x2a6240)
  u8  rot_nota[7]   (0x22f7a0: texto de la nota, indice HA_Tnn de HAHUD.DAT (tabla 0x3f0aa8) o 0xff)
  u8  marco_nota[2] (2B001F con nota < 4, 2B001E con 4 o mas: indice HA_Mn de 0x3f0b40)
  u8  marco_anuncio (2B0020), u8 rot_generico (2B003D), u8 0  (rotulos: 0x40 + k = el CA_Tkk de CAMPHUD.DAT, banco 0x46)
  u16 lista[60][4]  (por tipo de etapa (0x374010) y lista: primera peticion << 4 | cuenta; 0xffff sin guion)
  u8  arbol[3][15]  (el tipo de la etapa de cada registro de Request_Course_SP / _OR2 / Req_Course_MIX_PS2)
  peticiones de 52 bytes: s16 inicio, s16 fin, u8 tipo, u8 subtipo, u8 rotulo (HA_Tnn del anuncio, 0x22faec; 0xff
     ninguno), u8 banco46 (1: el rotulo es del banco 0x46, que no esta en la DC), u16 voz del anuncio (0x2a6358; 0 ninguna),
     u16 0, i32 p[10] (+0x60..+0x84 de sus parametros)
  detras de las peticiones, los objetos de las peticiones de los tipos de OBJ_TIPOS (conos, monedas, arcos...):
  'OBJ1', u32 n y n filas de 24 bytes {u16 peticion, s16 seccion inicial, s16 final, u8 cuantos, u8 clase de objeto
     (0x297e20: 0 / 1 conos rojo / azul, 2 / 3 moneda / corazon, 4 arco...), u8 su clase del banco 0x57 (tabla 0x423080,
     la de comun/haobj.c), u8 0, 2 bytes 0, f32 lateral inicial, final (fraccion del
     ancho de la carretera), f32 +0x18 (el ancho del arco)}: las filas del guion (0x293968: cuantos objetos repartidos de
     la seccion inicial a la final, con el lateral de una punta a otra, sin llegar a la final) y los puntos sueltos
     (0x2938d0: cuantos = 1); las secciones ya con el inicio de la peticion sumado si el guion lo pide (+0xc / +8)
  detras, 'CHI1' y la reaccion de la acompanante a la nota (0x2a66a8 en la fase 6): u32 movimiento[2][7] (tablas
     0x423e60 y 0x423e80 por la nota) y u8 tabla[8] (por el modelo del coche - 2, el switch 0x423ec0: 1 la segunda)
  detras, 'CLS1', u32 n y n x {u8 clase, u8 registro de obj_tour_pmt, u16 0, f32 escala}: las clases de los objetos que
     son de ese recurso (tabla 0x40a958); sus modelos en CAMPOBJ.DAT (ORD7, como HAOBJ.DAT)
  detras, 'SND1' y u16 efectos[19] (0x11c090): la pelota 7 (0x422cd0), el meteorito que cae 3 (0x423820), llega 3
     (0x423830) y golpea 3 + 3 (0x423840, 0x423850)
  detras, 'ZON1', u32 n y n x {u16 peticion, u16 0, f32 de0, de1, hasta0, hasta1}: la zona lateral de las peticiones del
     tipo 5 (+0x10..+0x1c del registro de 0x74 del guion; 0x232768 la interpola de la seccion inicial a la final)
  detras, 'C111' u16[3] 2 bytes 0 (0x422e00) y 'LET1' u16[2]: los registros del banco 0x57 (HAOBJ.DAT) del letrero de
     inicio y del de fin de la peticion (0x22a2b0 -> 0x232e60 con 0x1a7c80 / 0x1a7cb0, el idioma de conv_infarto.IDIOMA)
  detras, 'C141', u32 n, las tablas del tipo 14 (i32 cuantos[3], valores[3][10] los negativos (0x3b55c0 / 0x3b55d0) y lo
     mismo los positivos (0x3b5538 / 0x3b5548)) y n x {u16 peticion, u8 cuantos, u8 nivel (+0x74 de los parametros) y
     cuantos x {s16 seccion desde el inicio, s8 carril, u8 0}}: los coches de la lista +0x5c de los parametros (0x29d410)
  detras, 'C241', u32 n y n x {u16 peticion, u8 cuantos, u8 relativa (+0x74 de los parametros: secciones desde el inicio)
     y cuantos x {f32 x, y, z (el objetivo en el marco de la etapa del PS2), s16 seccion desde, hasta}}: los objetivos de las
     fotos del tipo 24 (lista +0x70, filas de 0x20 hasta +0x18 == -1; 0x2a5fd8 / 0x2a60a0)
  detras de 'M161', 'C131' (c2c8): los sprites y voces de las clases del tipo 13 y por peticion sus coches (ver main)
  detras, 'M163' (c2c9): los coches del subtipo 6 de los minijuegos (sus modelos y por peticion sus coches; ver main)
  detras, 'C221', u32 n y n x {u16 peticion, u8 cuantas, u8 0 y cuantas x {s16 seccion desde el inicio, hasta, f32 +8,
     +0xc (lo que se suma a cada una), i32 +0x10}}: las ventanas de la bocina del tipo 22 (lista +0x7c, filas de 0x18
     hasta -1; 0x29bfc0 / 0x29c058)

VOCESM.DAT (`voces_mision`; tambien solo con --solo-voces): las voces de los bancos que la carrera del campeonato
  carga por etapa (0x22ac00, estado 0: por cada peticion de la lista, el switch 0x415280 por el tipo da el banco de la
  tabla 0x352840 en los huecos 6, 7, 8 de 0x374180, sin repetir; estado 1: +0xf0 = rand % 4 la fila de las voces de la
  nota; estado 2: 0x415268[+0xf0] = HAM_MIS\\RANK1..4 en el hueco 4). La variante 5 carga ademas COMMON.pak en el hueco 1
  y PERH.pak en el 9 (0x1b7f74; el INFARTO: HC.PAK y PERH): VOCESH.DAT ya trae HC y PERH; de COMMON van aqui las voces que
  no estan en HC.PAK. Little endian:
  'VZM1', u32 n_bancos, u32 desplazamiento de la tabla de tipos, u32 tamano total
  n_bancos x {char[12] nombre, u32 desplazamiento (de sus filas; datos detras, en un sector de 2048), u32 n, u32 bytes de
     datos}: el 0 las de COMMON, 1..4 RANK1..4, despues los HAM_MIS de las peticiones
  tabla de tipos: 25 x 8 x {u8 banco, u8 cuantos}: el del (tipo, subtipo); cuantos > 1: uno al azar de cuantos seguidos
     (13 FOOD1..6 con rand % 6, 16/6 CARS con rand % 3 en +8 de los parametros; 16/2 SOUND con una peticion en la etapa
     (0x22bd40 == 0x22bd60) y si no SOUND2..8 con rand % 7: {0xfe, el banco de SOUND}, los 7 detras); 0xff ninguno
  cada banco: n filas de 20 bytes como VOZ1 {u16 id, u16 volumen, u32 desplazamiento en sus datos, u32 bytes, u32 Hz,
     u32 muestras} y sus datos (ADPCM de Yamaha) desde el siguiente multiplo de 2048 del fichero
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conv_hud as ch
import conv_menu as cm

LISTAS = ['REQUEST_NORMAL', 'REQUEST_SPECIAL1', 'REQUEST_SPECIAL2', 'REQUEST_SPECIAL3']   # 0x374138
# tipo -> (filas, puntos): los campos de sus parametros que leen sus funciones de objetos (0x293968 / 0x2938d0): 6
# 0x295ca0 / 0x295bf0, 7 0x2956a0 / 0x2955f8, 8 0x29a5c8 / 0x29a518, 15 0x293448 / 0x293390, 19 0x29df78 / 0x29ded0
OBJ_TIPOS = {6: (0x74, 0x78), 7: (0x78, 0x7c), 8: (0x78, 0x7c), 15: (0x70, 0x74), 19: (0x78, 0x7c)}
REGS_TOUR = (1, 4)                                           # obj_tour_pmt: la pelota (tipo 15) y el meteorito (19)


def tablas_banco(d):
    """como conv_sonido.banco_tablas, pero las tablas pueden ir detras de un relleno de ceros hasta el siguiente
    multiplo de 2048 (RANK3, ITEMS, PERR)"""
    a, _, b, _ = struct.unpack_from('<4I', d, 0)
    o = 16 + a + b
    if o + 4 <= len(d) and struct.unpack_from('<I', d, o)[0] == 0 and any(d[o:]): o = (o + 2047) & ~2047
    t = []
    for _ in range(2):
        n = struct.unpack_from('<I', d, o)[0] if o + 4 <= len(d) else 0
        t.append(dict(struct.unpack_from('<2I', d, o + 4 + 8 * k) for k in range(n))); o += 4 + 8 * n
    return d[16:16 + a], d[16 + a:16 + a + b], t


def voces_mision(D, u, par, sal):
    import conv_sonido as cs
    iso = D.iso; V = lambda a: a - 0xfff80
    irx = cs.fichero_paquete(iso, 'SYSTEM.PS2', '\\IRX\\SOUNDCD.IRX')
    tb, td, _ = cs.elf_seccion_irx(irx)
    se = lambda i: struct.unpack_from('<HH', irx, td + cs.IRX_SE - tb + 12 * i)
    nombre = lambda k: bytes(D.elf[V(u(0x352840 + 4 * k)):V(u(0x352840 + 4 * k)) + 40]).split(b'\0')[0].decode()
    # el switch de 0x22ac00 (estado 0): tipo - 1 -> 0x415280; los ori s1 = 0x80nn de cada caso (y sus switch de subtipo)
    assert par(0x22adec, 0x22adf4) == 0x415280 and par(0x22aea0, 0x22aea8) == 0x4152e0 and par(0x22af08, 0x22af10) == 0x415300 \
        and par(0x22af80, 0x22af88) == 0x415320 and par(0x22b178, 0x22b17c) == 0x415268, 'switch de los bancos (0x22ac00)'
    assert [u(0x374180 + 2 * k) & 0xffff for k in range(3)] == [0x600, 0x700, 0x800], 'huecos de los bancos (0x374180)'
    def caso(a):                                       # b 0x22b044 con ori s1 en el hueco (o ori s1 y b)
        for q in (a, a + 4):
            w = u(q)
            if w >> 16 == 0x3411: return w & 0xff
        return None
    tipos = {}
    for t in range(1, 25):
        a = u(0x415280 + 4 * (t - 1))
        if a == 0x22b044: continue
        k = caso(a)
        if k is not None: tipos[t] = {s: (k, 1) for s in range(8)}
    tipos[11] = {0: (caso(0x22ae6c), 1), 1: (caso(0x22ae5c), 1)}          # +4 == 0 HCARS, 1 GHOST
    assert nombre(tipos[11][0][0]).endswith('HCARS.pak') and nombre(tipos[11][1][0]).endswith('GHOST.PAK')
    tipos[13] = {s: (caso(u(0x4152e0)), 6) for s in range(8)}             # FOOD1..6, rand % 6
    assert [caso(u(0x4152e0 + 4 * k)) for k in range(6)] == list(range(tipos[13][0][0], tipos[13][0][0] + 6))
    t16 = {}
    for s in range(7):
        a = u(0x415300 + 4 * s)
        if a == 0x22af34: t16[s] = (0xfe, 8)                              # SOUND (sola) o SOUND2..8 (rand % 7)
        elif a == 0x22afec: t16[s] = (u(0x22aff0) & 0xff, 3)              # CARS, rand % 3 en +8
        else: t16[s] = (caso(a), 1)
    tipos[16] = t16
    sonido = [u(0x22af50) & 0xff] + [caso(u(0x415320 + 4 * k)) for k in range(7)]
    rank = [u(0x415268 + 4 * k) & 0xff for k in range(4)]
    assert [nombre(k) for k in rank] == ['HAM_MIS\\RANK%d.pak' % (i + 1) for i in range(4)]
    # los bancos: 0 COMMON (sin lo de HC.PAK), 1..4 RANK, despues los de las peticiones
    usados = []
    for t in sorted(tipos):
        for s, (b, n) in sorted(tipos[t].items()):
            for k in (sonido if b == 0xfe else range(b, b + n)):
                if k not in usados: usados.append(k)
    lista = [0x96] + rank + usados
    hc = tablas_banco(cs.fichero_paquete(iso, 'SOUND.PS2', '\\SPANISH\\HC.PAK'))[2]
    en_hc = set(hc[0]) | set(hc[1])
    bancos = []
    for k in lista:
        nm = nombre(k)
        try: d = cs.fichero_paquete(iso, 'SOUND.PS2', '\\SPANISH\\' + nm)
        except SystemExit: print('  VOCESM: no esta %s (tabla 0x%x)' % (nm, k)); bancos.append((nm, [])); continue
        A_, B_, (ta, tbb) = tablas_banco(d); mu = {}
        for datos, tt in ((A_, ta), (B_, tbb)):
            for i, o in tt.items():
                p, v = se(i)
                if not (0x100 <= p <= 0x3fff) or v == 0 or i in mu or (k == 0x96 and i in en_hc): continue
                m_ = cs.muestra_banco(datos, o)
                if len(m_) >= 65536:                   # (el AICA no toca mas de 65535 muestras de una vez: fuera)
                    print('  VOCESM: %s 0x%x de %d muestras (%.1f s): no cabe en un canal del AICA, fuera' % (nm, i, len(m_), len(m_) / cs.frecuencia(p))); continue
                mu[i] = (m_, cs.frecuencia(p), v)
        bancos.append((nm, [(i,) + mu[i] for i in sorted(mu)]))
    idx = {k: j for j, k in enumerate(lista)}
    assert [idx[k] for k in sonido] == list(range(idx[sonido[0]], idx[sonido[0]] + 8)), 'SOUND..SOUND8 seguidos'
    tab = bytearray()
    for t in range(25):
        for s in range(8):
            b, n = tipos.get(t, {}).get(s, (0xff, 0))
            tab += bytes((0xfe, idx[sonido[0]]) if b == 0xfe else (idx[b] if b != 0xff else 0xff, n))
    nb = len(bancos); cab = 16 + 24 * nb; off_t = cab; pos = off_t + len(tab)
    filas = []; datos = []
    for nm, mu in bancos:
        pos = (pos + 3) & ~3; o_f = pos; pos += 20 * len(mu); pos = (pos + 2047) & ~2047
        blob = bytearray(); fl = bytearray()
        for i, pcm, f, v in mu:
            while len(blob) % 32: blob.append(0)
            e = cs.enc_yamaha(pcm); assert len(pcm) < 65536, 'voz 0x%x demasiado larga' % i
            fl += struct.pack('<HH4I', i, v, len(blob), len(e), f, len(pcm)); blob += e
        while len(blob) % 32: blob.append(0)
        filas.append((nm, o_f, len(mu), fl, pos, blob)); pos += len(blob)
    out = bytearray(pos)
    out[0:16] = b'VZM1' + struct.pack('<3I', nb, off_t, pos)
    for j, (nm, o_f, n, fl, od, blob) in enumerate(filas):
        out[16 + 24 * j:40 + 24 * j] = nm.split('\\')[-1].encode()[:12].ljust(12, b'\0') + struct.pack('<3I', o_f, n, len(blob))
        out[o_f:o_f + len(fl)] = fl; out[od:od + len(blob)] = blob
    out[off_t:off_t + len(tab)] = tab
    open(os.path.join(sal, 'VOCESM.DAT'), 'wb').write(out)
    for nm, o_f, n, fl, od, blob in filas:
        print('  VOCESM: %-12s %2d voces %4d KB  %s' % (nm.split('\\')[-1], n, len(blob) >> 10,
              ' '.join('%x' % struct.unpack_from('<H', fl, 20 * q)[0] for q in range(n))))
    # lo que mas ocupa una etapa en el AICA: la mayor de RANK + COMMON + los 3 mayores bancos distintos de peticion
    tam = [len(f[5]) for f in filas]
    print('VOCESM.DAT: %d bytes, %d bancos; COMMON %d KB, RANK hasta %d KB, el mayor de peticion %d KB' % (
        len(out), nb, tam[0] >> 10, max(tam[1:5]) >> 10, max(tam[5:]) >> 10))


def minijuegos(u, par, m16, out):
    """el bloque 'M161' de CAMP.DAT (detras de lo demas) para comun/minijuego.c: u32 bytes, u16 voz_aviso2[7] (0x423c30),
    u16 animo[10] (0x4239c0: 2 de los subtipos 0 / 1, 3 del 2 en 0x4239c8, 3 de los demas en 0x4239d8), u8 sprite[34] (el
    grupo MJ_kk: 6 del subtipo 0 (0x423ae0), 9 + 12 del 1 (0x423b10), 7 del 2 (0x423b68)), u8 cuantos1[2] (0x423ad8), 2 bytes,
    f32 columna[9] y altura[9] (0x423b88 / 0x423bb0, por (columnas - 1) x 3 + columna), u32 n y n peticiones {u16 peticion, u8
    subtipo, u8 registros y cada uno {u8 clase, u8 +0x24, u8 cuantos, u8 0, s16 seccion, s16 hasta}} (secciones de la etapa); devuelve los tokens del banco 0x46 de sus
    sprites (los grupos MJ_kk de MJHUD.DAT). Cada tabla comprobada por el codigo que la lee (clase 0x29f520, vtable 0x423a80)"""
    assert par(0x2a1e58, 0x2a1e60) == 0x423c30, 'voces de los avisos del 16/2 (0x2a1e00)'
    assert par(0x2a055c, 0x2a0564) == 0x423b68, 'simbolos del 16/2 (0x2a0138)'
    assert par(0x29f994, 0x29f99c) == 0x4239c0 and par(0x29fa0c, 0x29fa14) == 0x4239c8 and par(0x29fa70, 0x29fa74) == 0x4239d8, 'animo (0x29f7a0)'
    assert u(0x2a0d40) == 0x8e05007c and u(0x2a0d3c) == 0x84af0000, 'registros de +0x7c (0x2a0d08)'
    toks = []
    def tk(t):
        assert t >> 16 == 0x46, hex(t)
        if t not in toks: toks.append(t)
        return toks.index(t)
    assert par(0x2a050c, 0x2a0514) == 0x423ae0 and par(0x2a052c, 0x2a0534) == 0x423b10, 'simbolos de los subtipos 0 / 1 (0x2a0138)'
    assert par(0x2a03e0, 0x2a03e8) == 0x423b88 and par(0x2a04ac, 0x2a04b4) == 0x423bb0, 'columnas (0x2a0138)'
    assert u(0x2a165c) == 0x3c020042 and u(0x2a1660) == 0x90433ad8, 'cuantos de cada clase del 16/1 (0x2a1618: 0x423ad8)'
    f_ = lambda a: struct.unpack('<f', struct.pack('<I', u(a)))[0]
    sp = [tk(u(0x423ae0 + 4 * k)) for k in range(6)] + [tk(u(0x423b10 + 4 * k)) for k in range(u(0x423ad8) + u(0x423ad8 + 4))] + \
         [tk(u(0x423b68 + 4 * k)) for k in range(7)]
    assert len(sp) == 34, len(sp)
    blk = struct.pack('<7H', *[u(0x423c30 + 4 * k) for k in range(7)]) + struct.pack('<10H', *[u(0x4239c0 + 4 * k) for k in range(10)])
    blk += bytes(sp) + bytes([u(0x423ad8), u(0x423ad8 + 4)]) + b'\0\0'
    blk += struct.pack('<18f', *[f_(0x423b88 + 4 * k) for k in range(9)], *[f_(0x423bb0 + 4 * k) for k in range(9)])
    blk += struct.pack('<I', len(m16)) + b''.join(m16)
    while len(blk) % 4: blk += b'\0'
    out += b'M161' + struct.pack('<I', len(blk)) + blk
    # 'M162' (detras de 'M161', agente c2c16b): los subtipos 3 (la suma), 4 (los regalos) y 5 (las ovejas): i32 nivel[3][2]
    # (0x3b5648: {de, hasta} del numero de un aviso del 3, por +0x24), u8 signo[2] (el grupo MJ_kk del + y del -: 0x423bdc
    # y 0x423bd8), cifra[2][10] (las cifras de los negativos 0x423be0 y de los demas 0x423c08, 0x2a0838), regalo[5]
    # (0x423af8), oveja (0x460028, 0x2a2e80) y zzz (0x460027, 0x2322a0), 3 bytes, u16 efecto_oveja (0x188) y salto[3]
    # (0x423a70): 64 bytes
    assert par(0x2a20c4, 0x2a20cc) == 0x3b5648, 'niveles del 16/3 (0x2a2080)'
    assert u(0x2a0b64) == 0x3c020042 and u(0x2a0b6c) == 0x8c443bdc and u(0x2a0b30) == 0x3c020042 and u(0x2a0b38) == 0x8c443bd8, 'signos (0x2a0838)'
    assert par(0x2a0b98, 0x2a0bac) == 0x423bd8 and u(0x2a0c20) == 0x24420002 and u(0x2a0c64) == 0x2442000c, 'cifras (0x2a0838)'
    assert par(0x2a06b0, 0x2a06b8) == 0x423af8, 'regalos del 16/4 (0x2a0138)'
    assert u(0x2a2fdc) == 0x3c050046 and u(0x2a2fe8) == 0x34a50028, 'la oveja (0x2a2e80)'
    assert u(0x2322b0) == 0x3c050046 and u(0x2322c8) == 0x34a50027, 'zzz (0x2322a0)'
    assert u(0x2a3108) == 0x24040188 and par(0x2a311c, 0x2a3120) == 0x423a70, 'efectos de la oveja (0x2a2e80)'
    b2 = struct.pack('<6i', *[struct.unpack('<i', struct.pack('<I', u(0x3b5648 + 4 * k)))[0] for k in range(6)])
    b2 += bytes([tk(u(0x423bdc)), tk(u(0x423bd8))] + [tk(u(0x423be0 + 4 * k)) for k in range(10)] + [tk(u(0x423c08 + 4 * k)) for k in range(10)])
    b2 += bytes([tk(u(0x423af8 + 4 * k)) for k in range(5)] + [tk(0x460028), tk(0x460027)]) + b'\0\0\0'
    b2 += struct.pack('<4H', 0x188, *[u(0x423a70 + 4 * k) for k in range(3)])
    out += b'M162' + struct.pack('<I', len(b2)) + b2
    return toks


def main():
    if len(sys.argv) < 3: raise SystemExit(__doc__.split('\n')[0])
    D = ch.Disco(sys.argv[1]); sal = sys.argv[2]; os.makedirs(sal, exist_ok=True)
    E = D.elf
    u = lambda a: struct.unpack_from('<I', E, a - 0xfff80)[0]
    i_ = lambda a: struct.unpack_from('<i', E, a - 0xfff80)[0]
    f = lambda a: struct.unpack_from('<f', E, a - 0xfff80)[0]
    def par(a, b):
        x, y = u(a), u(b); assert x >> 26 == 15 and y >> 26 == 9, 'codigo inesperado en 0x%x / 0x%x' % (a, b)
        return ((x & 0xffff) << 16) + (y & 0xffff) - (0x10000 if y & 0x8000 else 0)
    # las tablas, por el codigo que las lee
    assert par(0x22e674, 0x22e688) == 0x374190, 'umbrales (0x22e670)'
    assert u(0x22e74c) >> 16 == 0x3c03 and u(0x22e754) & 0xffff == 0x41c8, 'tope (0x22e728)'
    assert par(0x22e77c, 0x22e784) == 0x3741d0 and par(0x22e7ac, 0x22e7b4) == 0x3741e0, 'extras (0x22e728)'
    assert par(0x22ee10, 0x22ee20) == 0x3741f8, 'millas (0x22edd8)'
    assert par(0x232518, 0x23251c) == 0x374240, 'penalizaciones (0x2323e8)'
    assert par(0x2a6668, 0x2a6670) == 0x423df0, 'voces de la nota (0x2a65a8)'
    assert par(0x2a62e8, 0x2a62f0) == 0x423ee0 and par(0x2a6310, 0x2a6318) == 0x423ef8, 'voces de animo (0x2a6240)'
    assert par(0x22ac94, 0x22ac98) == 0x374010 and par(0x22b63c, 0x22b648) == 0x374138, 'nombres (0x22ac00 / 0x22b600)'
    assert u(0x3741a8) == 0x42a00000, 'medidor lleno 80 (0x3741a8)'
    voces_mision(D, u, par, sal)
    if '--solo-voces' in sys.argv: return
    out = bytearray(b'CMP1') + struct.pack('<3I', 1, 0, 0)
    W = lambda fmt, *v: out.extend(struct.pack('<' + fmt, *v))
    W('7f', *[f(0x374190 + 4 * k) for k in range(7)])
    W('i', i_(0x3741c8)); W('4i', *[i_(0x3741d0 + 4 * k) for k in range(4)]); W('5i', *[i_(0x3741e0 + 4 * k) for k in range(5)])
    W('7i', *[i_(0x3741f8 + 4 * k) for k in range(7)])
    W('204f', *[f(0x374240 + 4 * k) for k in range(204)])
    W('28H', *[u(0x423df0 + 4 * k) for k in range(28)])
    W('10H', *([u(0x423ee0 + 4 * k) for k in range(5)] + [u(0x423ef8 + 4 * k) for k in range(5)]))
    # rotulos: los del banco 0x2B que convierte conv_infarto.py en HAHUD.DAT (HA_Tnn = tabla 0x3f0aa8, HA_Mn = 0x3f0b40)
    tt = [u(0x3f0aa8 + 4 * k) for k in range(37)]; tm = [u(0x3f0b40 + 4 * k) for k in range(3)]
    ti = lambda tok: tt.index(tok) if tok in tt else 0xff
    from ps2 import mips
    from ps2.assets import Assets, hash_nombre
    As = Assets(sys.argv[1])
    def corre(pc, regs, paradas, mem=None):
        m = mips.Mips(As.elf)
        for a, k in paradas.items(): m.escribe(a, struct.pack('<2I', 0x03e00008, 0x24030000 | k))   # jr ra; addiu v1, zero, k
        for k, v in regs.items(): m.r[k] = v
        if mem:
            for a, d in mem.items(): m.escribe(a, d)
        m.r[29] = m.PILA - 0x100; m.r[31] = 0xDEAD0000
        m._corre(pc, 20000); return m
    # el texto de la nota (0x22f7a0: el switch de 0x415590 deja el token en v0)
    assert u(0x22f8c0) & 0xffff == 0x5590
    rn = []
    for nota in range(7):
        m = corre(0x22f8b0, {16: nota}, {0x22f95c: 1, 0x22f964: 0}); rn.append(ti(m.r[2] & 0xffffffff) if m.r[3] == 1 else 0xff)
    W('7B', *rn); W('2B', tm.index(0x2b001f), tm.index(0x2b001e)); W('3B', tm.index(0x2b0020), ti(0x2b003d), 0)
    print('  notas: textos %s' % rn)
    # los guiones
    por_hash = {x.hash: x for x in As.paquete('SCRIPTS.PS2')}
    nombres = []
    for k in range(60):
        p = u(0x374010 + 4 * k); nombres.append(bytes(E[p - 0xfff80:p - 0xfff80 + 40]).split(b'\0')[0].decode())
    assert nombres[0] == 'Req_Palm_Beach' and nombres[59] == 'Req_Easter_Island_r'
    usadas = set()
    for n_ in cm.CURSOS_REQ:
        d_ = bytes(As.datos(por_hash[hash_nombre('\\bin\\%s.bin' % n_)])); nn, oo = cm._r077(d_, n_)
        usadas.update(struct.unpack_from('<I', d_, oo + 0x78 * k + 0x1c)[0] for k in range(nn))
    PAR = mips.Mips.PILA - 0x8000                            # parametros en la memoria del interprete (la pila)
    lista = []; pets = bytearray(); npet = 0; tipos = {}; objs = []; t46 = []; zon = []; c14 = []; m16 = []; c24 = []; c22 = []; c13 = []; c166 = []
    # 0x297e20: +0x18 del objeto = 0x423080[clase] (la clase del banco 0x57); comprobado por el codigo que la lee
    assert par(0x29800c, 0x298014) == 0x423080, 'clases de los objetos (0x297e20)'
    banco = lambda c: u(0x423080 + 4 * c) & 0xff
    for k in range(60):
        h_ = hash_nombre('\\bin\\%s.bin' % nombres[k])
        if h_ not in por_hash: assert k not in usadas, nombres[k]; lista += [0xffff] * 4; continue
        d = bytes(As.datos(por_hash[h_])); nc, nr = struct.unpack_from('<2I', d, 0)
        rel = dict(struct.unpack_from('<2I', d, 8 + 8 * q) for q in range(nr))
        for li, nom in enumerate(LISTAS):
            try: c_, o_ = cm._r077(d, nom)
            except SystemExit: c_, o_ = 0, 0
            assert c_ <= 3 and npet < 0x1000; lista.append(npet << 4 | c_)
            for q in range(c_):
                r = o_ + 0x74 * q; ini, fin = struct.unpack_from('<2h', d, r)
                pp = rel[r + 0x58]; blk = d[pp:pp + 0x88]; tipo, sub = struct.unpack_from('<2I', blk, 0)
                assert 1 <= tipo <= 24 and 0 <= ini < fin, (nombres[k], q, tipo, ini, fin)
                # el rotulo del anuncio (0x22f9d0: switch 0x4155b0 por el tipo, deja el token en a1 y el banco en a2)
                m = corre(0x22faec, {19: tipo, 16: sub, 17: PAR}, {0x22fe10: 1}, {PAR: blk})   # (el 20 pasa por 0x22fe08: 2B0039)
                tok = m.r[5] & 0xffffffff if m.r[3] == 1 else 0
                # la voz del anuncio (0x2a6358: switch 0x423d70 por el tipo, a0 = la voz en 0x2a6548)
                try:
                    mv = corre(0x2a636c, {16: PAR}, {0x2a6548: 1, 0x2a656c: 0, 0x2a6570: 0}, {PAR: blk})
                    voz = mv.r[4] & 0xffff if mv.r[3] == 1 else 0
                except RuntimeError:                          # (la del tipo que la elige al azar: 0x21d3c8, sin portar)
                    voz = 0xffff
                if tok >> 16 == 0x46 and tok not in t46: t46.append(tok)
                rot = ti(tok) if tok >> 16 == 0x2b else 0x40 + t46.index(tok) if tok >> 16 == 0x46 else 0xff
                pets.extend(struct.pack('<2h4B2H10i', ini, fin, tipo, sub, rot, 1 if tok >> 16 == 0x46 else 0, voz, 0,
                                        *struct.unpack_from('<10i', blk, 0x60)))
                tipos.setdefault(tipo, set()).add((sub, hex(tok), hex(voz)))
                if tipo in OBJ_TIPOS:
                    fo, po = OBJ_TIPOS[tipo]
                    if pp + fo in rel:                        # filas de 0x20 hasta cuantos == -1
                        q_ = rel[pp + fo]
                        while struct.unpack_from('<i', d, q_)[0] != -1:
                            n_, cl_, f8, re_, s0, s1, l0, l1 = struct.unpack_from('<2ifi2i2f', d, q_)
                            assert 0 < n_ < 256 and 0 <= cl_ < 15
                            objs.append(struct.pack('<H2h4B2x3f', npet, s0 + (ini if re_ else 0), s1 + (ini if re_ else 0), n_, cl_, banco(cl_), 0, l0, l1, f8))
                            q_ += 0x20
                    if pp + po in rel:                        # puntos de 0x2c hasta seccion == -1
                        q_ = rel[pp + po]
                        while struct.unpack_from('<i', d, q_ + 4)[0] != -1:
                            l0, s0, re_ = struct.unpack_from('<f2i', d, q_); cl_, = struct.unpack_from('<i', d, q_ + 0x10)
                            f8, = struct.unpack_from('<f', d, q_ + 0x18); assert 0 <= cl_ < 15
                            objs.append(struct.pack('<H2h4B2x3f', npet, s0 + (ini if re_ else 0), s0 + (ini if re_ else 0), 1, cl_, banco(cl_), 0, l0, l0, f8))
                            q_ += 0x2c
                if tipo == 14 and pp + 0x5c in rel:           # los coches de la cuenta (0x29d410: +0x5c, 0x298268 los cuenta)
                    q_ = rel[pp + 0x5c]; cs14 = []
                    while len(cs14) < 100:
                        sc_, lc_ = struct.unpack_from('<hb', d, q_ + 8)
                        if sc_ < 0 or lc_ < 0: break
                        cs14.append((sc_, lc_)); q_ += 0x14
                    c14.append(struct.pack('<H2B', npet, len(cs14), blk[0x74]) + b''.join(struct.pack('<hbx', *x) for x in cs14))
                if tipo == 13 and pp + 0x5c in rel:           # (c2c8) los coches de la comida (0x299e80: +0x5c como el 14; +0x78
                    q_ = rel[pp + 0x5c]; cs13 = []            # clases, +0x98 / +0x99 umbrales, +0x9a fases; +0x64 / +0x66)
                    while len(cs13) < 100:
                        sc_, lc_ = struct.unpack_from('<hb', d, q_ + 8)
                        if sc_ < 0 or lc_ < 0: break
                        cs13.append((sc_, lc_)); q_ += 0x14
                    e_ = d[pp:pp + 0xa0]; m64, m66 = struct.unpack_from('<2h', e_, 0x64)
                    c13.append(struct.pack('<H6B2h', npet, len(cs13), min(e_[0x78], 3), e_[0x98], e_[0x99], min(e_[0x9a], 3), 0, m64, m66) +
                               b''.join(struct.pack('<hbx', *x) for x in cs13))
                if tipo == 16:                                # los minijuegos (0x2a0d08: los registros de 0x2c de +0x7c, +0x80 relativos)
                    regs = []
                    if pp + 0x7c in rel:
                        q_ = rel[pp + 0x7c]; r80 = struct.unpack_from('<i', blk, 0x80)[0] != 0
                        while struct.unpack_from('<h', d, q_ + 0x26)[0] != -1 and len(regs) < 100:
                            cl_, b24, s26, s28, n2a = struct.unpack_from('<iBxhhB', d, q_ + 0x20)
                            regs.append(struct.pack('<4B2h', cl_ & 0xff, b24, n2a, 0, s26 + (ini if r80 else 0), s28 + (ini if r80 else 0)))
                            q_ += 0x2c
                    m16.append(struct.pack('<H2B', npet, sub, len(regs)) + b''.join(regs))
                    if sub == 6:                              # (c2c9) los coches del subtipo 6 (0x2a3bd0: la lista +0x5c, filas
                        cs6 = []                              # de 0x14 hasta +8 < 0 o +0xa < 0); +0x64 / +0x66 (0x2a3300)
                        if pp + 0x5c in rel:
                            q_ = rel[pp + 0x5c]
                            while len(cs6) < 100:
                                sc_, lc_ = struct.unpack_from('<hb', d, q_ + 8)
                                if sc_ < 0 or lc_ < 0: break
                                cs6.append((sc_, lc_)); q_ += 0x14
                        m64, m66 = struct.unpack_from('<2h', blk, 0x64)
                        c166.append(struct.pack('<H2B2h', npet, len(cs6), 0, m64, m66) + b''.join(struct.pack('<hbx', *x) for x in cs6))
                if tipo == 24 and pp + 0x70 in rel:           # los objetivos de las fotos (0x2a5fd8: +0x70, filas de 0x20 hasta
                    q_ = rel[pp + 0x70]; cs24 = []            # +0x18 == -1; +0x74 != 0: las secciones desde el inicio)
                    while len(cs24) < 16:
                        x_, y_, z_, s0_, s1_ = struct.unpack_from('<3f2i', d, q_ + 0xc)
                        if s0_ == -1: break
                        cs24.append((x_, y_, z_, s0_, s1_)); q_ += 0x20
                    c24.append(struct.pack('<H2B', npet, len(cs24), 1 if struct.unpack_from('<i', blk, 0x74)[0] else 0) +
                               b''.join(struct.pack('<3f2h', *x) for x in cs24))
                if tipo == 22 and pp + 0x7c in rel:           # las ventanas de la bocina (0x29c058: +0x7c, filas de 0x18 hasta -1)
                    q_ = rel[pp + 0x7c]; cs22 = []
                    while len(cs22) < 32:
                        s0_, s1_, f8_, fc_, v_ = struct.unpack_from('<2i2fi', d, q_)
                        if s0_ == -1: break
                        cs22.append((s0_, s1_, f8_, fc_, v_)); q_ += 0x18
                    c22.append(struct.pack('<H2B', npet, len(cs22), 0) + b''.join(struct.pack('<2h2fi', *x) for x in cs22))
                if tipo == 5:                                 # la zona del 5 (0x232768: +0x10 / +0x14 de, +0x18 / +0x1c hasta)
                    zon.append(struct.pack('<2H4f', npet, 0, *struct.unpack_from('<4f', d, r + 0x10)))
                npet += 1
    W('240H', *lista)
    # los tipos de las etapas de los tres arboles (grupos 4, 5, 6: registros de 0x78, +0x1c el descriptor de tramo; el
    # nodo del mapa i es el registro i - 1). Request_Course_OR2 es el arbol del modo OutRun (csc_data_2) y
    # Request_Course_SP el de OUTRUN2SP (csc_data_cvt, el 1 de CURSO.DAT): la carrera los usa
    arb = []
    for n_ in cm.CURSOS_REQ:
        d_ = bytes(As.datos(por_hash[hash_nombre('\\bin\\%s.bin' % n_)])); nn, oo = cm._r077(d_, n_); assert nn == 15
        arb.append([struct.unpack_from('<30i', d_, oo + 0x78 * k) for k in range(15)])
        W('15B', *[r[7] for r in arb[-1]])
    d2 = bytes(As.datos([x for x in As.busca('*csc_data_2.bin') if x.paquete.startswith('SCRIPTS')][0]))
    n2, o2 = cm._r077(d2, 'csc_data_2_course'); assert n2 == 15
    c2 = [struct.unpack_from('<30i', d2, o2 + 0x78 * k) for k in range(15)]
    # (las 5 metas de Request_Course_OR2 llevan el hijo izquierdo 0 y el derecho -1; las de csc_data_2, -1 y -1: la
    # carrera de la DC acaba en la meta como el modo OutRun, PROBABLE que el 0 no se use)
    q = lambda r: (r[1], r[7], r[11], r[12]) if r[12] >= 0 else (r[1], r[7], -1, -1)
    assert [q(r) for r in c2] == [q(r) for r in arb[1]], 'Request_Course_OR2 no es csc_data_2'
    d1 = bytes(As.datos([x for x in As.busca('*csc_data_cvt.bin') if x.paquete.startswith('SCRIPTS')][0]))
    n1, o1 = cm._r077(d1, 'csc_data_cvt_course'); assert n1 == 15
    c1 = [struct.unpack_from('<30i', d1, o1 + 0x78 * k) for k in range(15)]
    assert [q(r) for r in c1] == [q(r) for r in arb[0]], 'Request_Course_SP no es csc_data_cvt'
    while len(out) % 4: out.append(0)
    struct.pack_into('<2I', out, 8, npet, len(out)); out += pets
    out += b'OBJ1' + struct.pack('<I', len(objs)) + b''.join(objs)
    assert u(0x2a66e8) == 0x24703e60 and u(0x2a6724) == 0x24503e80 and par(0x2a6704, 0x2a670c) == 0x423ec0, 'nota (0x2a66a8)'
    sel = [1 if u(0x423ec0 + 4 * k) == 0x2a6720 else 0 for k in range(8)]
    out += b'CHI1' + struct.pack('<14I', *[u(0x423e60 + 4 * k) for k in range(7)], *[u(0x423e80 + 4 * k) for k in range(7)]) + bytes(sel)
    print('  acompanante: %s / %s, tabla %s' % ([hex(u(0x423e60 + 4 * k)) for k in range(7)], [hex(u(0x423e80 + 4 * k)) for k in range(7)], sel))
    # las clases de los objetos de C2C que no son del banco 0x57 sino de obj_tour_pmt (recurso 0x1ee): la pelota del tipo
    # 15 (0x11 -> registro 1) y los meteoritos del 19 (0x3d -> registro 4); tabla de clases 0x40a958 (recurso << 16 |
    # registro, f32 escala), la misma que lee tools/conv_infarto.py
    assert u(0x40a958 + 8 * 0x11) == 0x1ee0001 and u(0x40a958 + 8 * 0x3d) == 0x1ee0004, 'tabla de clases 0x40a958'
    cls = [(c, u(0x40a958 + 8 * c) & 0xffff, f(0x40a958 + 8 * c + 4)) for c in range(64) if u(0x40a958 + 8 * c) >> 16 == 0x1ee
           and u(0x40a958 + 8 * c) & 0xffff in REGS_TOUR]
    out += b'CLS1' + struct.pack('<I', len(cls)) + b''.join(struct.pack('<BBHf', c, r, 0, e) for c, r, e in cls)
    print('  clases de obj_tour_pmt: %s' % ', '.join('0x%x -> %d (x %g)' % x for x in cls))
    # los efectos (0x11c090) de los tipos 15 y 19, por el codigo que lee cada tabla (tabla[rand % n])
    assert par(0x29365c, 0x293660) == 0x422cd0, 'efectos de la pelota (0x2934e8)'
    assert par(0x29e16c, 0x29e170) == 0x423820 and par(0x29e254, 0x29e258) == 0x423830, 'efectos del meteorito (0x29e018)'
    assert par(0x29e390, 0x29e394) == 0x423840 and par(0x29e3b8, 0x29e3bc) == 0x423850, 'efectos del golpe (0x29e018)'
    snd = [u(0x422cd0 + 4 * k) for k in range(7)] + [u(t + 4 * k) for t in (0x423820, 0x423830, 0x423840, 0x423850) for k in range(3)]
    out += b'SND1' + struct.pack('<19H', *snd)
    out += b'ZON1' + struct.pack('<I', len(zon)) + b''.join(zon)
    # el tipo 11 (0x294a90): los efectos de 0x422e00 (rand % 3, 0x11c090) con un coche de la peticion 7 secciones por
    # delante y cada 60 pasos como poco
    assert par(0x294d98 + 4, 0x294da4) == 0x422e00, 'efectos del tipo 11 (0x294a90)'
    out += b'C111' + struct.pack('<3H2x', *[u(0x422e00 + 4 * k) for k in range(3)])
    # el letrero de inicio / fin de la peticion (0x22a2b0 -> 0x232e60 con 0x1a7c80 / 0x1a7cb0): los registros del banco
    # 0x57 por idioma, los mismos de las marcas del INFARTO (HAOBJ.DAT); el 2B0039 del 20 ya va en el rotulo (0x22fe08)
    import conv_infarto as ci
    assert u(0x22a42c) == 0x0c000000 | (0x1a7c80 >> 2) and u(0x22a490) == 0x0c000000 | (0x1a7cb0 >> 2) and u(0x22a448) == 0x0c000000 | (0x232e60 >> 2)
    out += b'LET1' + struct.pack('<2H', *ci.marcas(u, par))
    # el tipo 14 (la suma, 0x29d410): por cada peticion sus coches {seccion desde el inicio, carril} y el nivel (+0x74 de los
    # parametros); las tablas de los valores por nivel: con rand < 0,2 los negativos (0x3b55c0 cuantos, 0x3b55d0 + 0x28 x
    # nivel), si no los positivos (0x3b5538, 0x3b5548 + 0x28 x nivel)
    assert par(0x29d478, 0x29d488) == 0x3b5548 and par(0x29d47c, 0x29d498) == 0x3b5538, 'tablas del tipo 14 (0x29d410)'
    t14 = b''.join(struct.pack('<3i', *[i_(a + 4 * k) for k in range(3)]) + struct.pack('<30i', *[i_(b + 4 * k) for k in range(30)])
                   for a, b in ((0x3b55c0, 0x3b55d0), (0x3b5538, 0x3b5548)))
    out += b'C141' + struct.pack('<I', len(c14)) + t14 + b''.join(c14)
    print('  tipo 14: %d peticiones, %s coches' % (len(c14), [x[2] for x in c14]))
    out += b'C241' + struct.pack('<I', len(c24)) + b''.join(c24)
    print('  tipo 24: %d peticiones, %s objetivos' % (len(c24), [x[2] for x in c24]))
    out += b'C221' + struct.pack('<I', len(c22)) + b''.join(c22)
    print('  tipo 22: %d peticiones, %s ventanas' % (len(c22), [x[2] for x in c22]))
    t16 = minijuegos(u, par, m16, out)
    # (c2c8) el tipo 13 (0x2991b8, la comida): el sprite de cada clase (0x4231d8[+8 de los parametros][clase], 0x115ae0 en
    # 0x299dd8 y 0x115870 en el dibujo) y su voz (0x423220, 0x11c090 al cambiar de objetivo); los sprites, grupos de
    # MJHUD.DAT (los que no lleva el 16 van detras de los suyos). Bloque 'C131' (detras de 'M161'): u8 sprite[6][3], u8 0 x 2,
    # u16 voz[6][3], u32 n y n x {u16 peticion, u8 coches, clases (+0x78), p98, p99, fases (+0x9a), 0, s16 +0x64, +0x66 y
    # coches x {s16 seccion desde el inicio, s8 carril, u8 0}} (la lista +0x5c, filas de 0x14)
    assert par(0x299e08, 0x299e10) == 0x4231d8 and par(0x29942c, 0x299434) == 0x423220, 'tablas del tipo 13 (0x2991b8)'
    for k in range(18):
        if u(0x4231d8 + 4 * k) not in t16: t16.append(u(0x4231d8 + 4 * k))
    b13 = bytes(t16.index(u(0x4231d8 + 4 * k)) for k in range(18)) + b'\0\0' + struct.pack('<18H', *[u(0x423220 + 4 * k) for k in range(18)])
    b13 += struct.pack('<I', len(c13)) + b''.join(c13)
    while len(b13) % 4: b13 += b'\0'
    out += b'C131' + struct.pack('<I', len(b13)) + b13
    print('  tipo 13: %d peticiones, %s coches' % (len(c13), [x[2] for x in c13]))
    # (c2c9) el subtipo 6 de los minijuegos (los coches, 0x2a3300 / 0x2a3510): 0x2a3bd0 da a cada coche de la lista la clase
    # +0x78 + 3 (+0x78 = +8 de los parametros, el azar del banco CARS) con rand < 0,5 o una de las otras dos (3..5);
    # 0x22d0a0 al abrir cada coche de la lista cuenta en +0x7a los de esa clase (0x22d278) y le pone el modelo 0x415500
    # [plaza % 3] (los contados) o 0x415510 / 0x415530 [plaza % 7] (los demas, por la clave de la etapa); 0x19c6f0 los
    # tine por la clase (+0xd10: 3 rojo, 4 verde, 5 azul; 0x111bb0). El trafico de la peticion con los modelos +0x64 o
    # 0x104. Bloque 'M163' (detras de 'C131'): u8 contados[3], otros[7], otros_1a[7], 0 x 3, u32 n y n x {u16 peticion, u8
    # coches, 0, s16 +0x64, +0x66 y coches x {s16 seccion desde el inicio, s8 carril, u8 0}}
    assert u(0x22d278) == 0x94820078 and u(0x22d280) == 0x24420003 and u(0x22d28c) == 0x9482007a, 'la cuenta del 16/6 (0x22d0a0)'
    assert par(0x22d2e0, 0x22d2e8) == 0x415500 and par(0x22d318, 0x22d320) == 0x415530, 'modelos del 16/6 (0x22d0a0)'
    assert u(0x22d33c) == 0x3c050041 and u(0x22d344) == 0x24a55510, 'modelos de los demas (0x22d0a0: 0x415510)'
    assert u(0x2a3474) == 0x24080104 and u(0x2a3384) == 0x0c000000 | (0x2a3bd0 >> 2), 'el subtipo 6 (0x2a3300)'
    assert u(0x19c708) == 0x2442a970 and u(0x3ea970 + 8) == 0x19c74c and u(0x3ea970 + 16) == 0x19c77c, 'el tinte (0x19c6f0)'
    b6 = bytes([u(0x415500 + 4 * k) for k in range(3)] + [u(0x415510 + 4 * k) for k in range(7)] + [u(0x415530 + 4 * k) for k in range(7)]) + b'\0\0\0'
    b6 += struct.pack('<I', len(c166)) + b''.join(c166)
    while len(b6) % 4: b6 += b'\0'
    out += b'M163' + struct.pack('<I', len(b6)) + b6
    print('  minijuego 16/6: %d peticiones, %s coches, modelos %s' % (len(c166), [x[2] for x in c166], list(b6[:17])))
    print('  tipo 16: %d peticiones, %d sprites del banco 0x46' % (len(m16), len(t16)))
    print('  zonas del tipo 5: %s' % ' '.join('%d:(%.2f..%.2f)-(%.2f..%.2f)' % ((struct.unpack('<2H4f', z)[0],) + tuple(struct.unpack('<2H4f', z)[2:])) for z in zon))
    print('  efectos de la pelota y los meteoritos: %s' % ' '.join('%x' % x for x in snd))
    open(os.path.join(sal, 'CAMP.DAT'), 'wb').write(out)
    # CAMPOBJ.DAT: sus modelos (ORD7 de conv_bo.modelo_conjunto, como HAOBJ.DAT; nodo: banderas << 16 | registro)
    import conv_bo
    mod = conv_bo.modelo_conjunto(As, As.bloque(As.entrada('COMMON.PS2', 'obj_tour_pmt.sz')), set(REGS_TOUR), os.path.join(sal, 'CAMPOBJ.DAT'))
    print('CAMPOBJ.DAT: %d mallas, %d vertices (%d triangulos), %d texturas, %d KB' % (len(mod.mallas), mod.nv, mod.nv - 2 * len(mod.mallas),
          len(mod.texs), os.path.getsize(os.path.join(sal, 'CAMPOBJ.DAT')) >> 10))
    for t in sorted(tipos): print('  tipo %2d: %s' % (t, sorted(tipos[t])))
    # CAMPHUD.DAT: los rotulos del anuncio del banco SPRANI 0x46 (el rotulo 0x40 + k de la peticion es el grupo CA_Tkk),
    # cada uno con su escena entera, como los HA_Tnn de HAHUD.DAT (tools/conv_infarto.py)
    import numpy as np
    A46, T46, _ = D.banco(0x46); hud = ch.Hud(); hud.taco = [0xff] * 16
    hud.vq = lambda i, a, fmt, l_: ch.vq_pequeno(i, a, fmt, l_, 'CAMPHUD: ')
    for k_, tok in enumerate(t46):
        sc = A46.escenas[tok & 0xffff]; nc, pc = struct.unpack_from('<2I', A46.a, sc); raiz = pc + (nc - 1) * 0x24
        n_ = max(1, int(struct.unpack_from('<f', A46.a, raiz + 8)[0])); nm = 'CA_T%02d' % k_
        g_ = ch.grupo_escena(hud, nm, A46, T46, 0x46, tok & 0xffff, n_, prefijo='ca_'); hud.uso(nm, g_, 0, 0, capa=4)
    hud.escribe(os.path.join(sal, 'CAMPHUD.DAT'))
    print('CAMPHUD.DAT: %d rotulos del banco 0x46 (%s), %d bytes, %d texturas' % (len(t46), ' '.join('%X' % t for t in t46),
          os.path.getsize(os.path.join(sal, 'CAMPHUD.DAT')), len(hud.tex)))
    print('CAMP.DAT: %d peticiones, %d filas de objetos, %d bytes' % (npet, len(objs), len(out)))
    # MJHUD.DAT: los sprites 3D de los minijuegos (0x2a0138 -> 0x115870, banco 0x46): el grupo MJ_kk es el token t16[k]
    hud = ch.Hud(); hud.taco = [0xff] * 16; hud.vq = lambda i, a, fmt, l_: ch.vq_pequeno(i, a, fmt, l_, 'MJHUD: ')
    for k_, tok in enumerate(t16):
        sc = A46.escenas[tok & 0xffff]; nc, pc = struct.unpack_from('<2I', A46.a, sc); raiz = pc + (nc - 1) * 0x24
        n_ = max(1, int(struct.unpack_from('<f', A46.a, raiz + 8)[0])); nm = 'MJ_%02d' % k_
        g_ = ch.grupo_escena(hud, nm, A46, T46, 0x46, tok & 0xffff, n_, prefijo='mj_'); hud.uso(nm, g_, 0, 0, capa=4)
    hud.escribe(os.path.join(sal, 'MJHUD.DAT'))
    print('MJHUD.DAT: %d sprites (%s), %d bytes' % (len(t16), ' '.join('%X' % t for t in t16), os.path.getsize(os.path.join(sal, 'MJHUD.DAT'))))


if __name__ == '__main__':
    main()
