#!/usr/bin/env python3
"""conv_sonido.py <iso> <dir_salida> [--idioma SPANISH]: efectos del coche y de los menus -> SFX.DAT; voces -> VOCES.DAT.

De donde salen (todo se lee de tu ISO; aqui solo hay direcciones y formatos):
- SOUND.PS2 es un paquete de ficheros por hash: h = h*131 + toupper(c), recorriendo el nombre de atras adelante
  (funcion 0x170048 del ELF), sobre "\\NOMBRE.PAK". El banco de sonidos "ICS" (motor, derrape, golpes, rodadura) es
  \\ICS.PAK: cabecera de 16 bytes (u32 tamano, u32 1 = banco ICS, 0, 0) y 68 muestras PS-ADPCM (SPU2) seguidas, cada una
  en bucle entero (marca 6 en el primer bloque, 3 en el ultimo) y con un bloque mudo de 16 bytes detras.
- La tabla de muestras ICS esta en el modulo del IOP (\\IRX\\SOUNDCD.IRX en SYSTEM.PS2), .data 0x66a0: 68 entradas
  de 8 bytes {u32 desplazamiento en el banco, u16 pitch SPU (0x1000 = 48 kHz), 2 bytes}: 0x555 = 16 kHz,
  0x759 = 22.05 kHz, 0x3ac = 11.025 kHz.
- Las definiciones ICS estan en el ELF: cuenta en 0x3526a4 y 20 entradas {u32 capas, u32 puntero} en 0x3526a8; cada
  capa ocupa 86 bytes: +0 muestra, +3 nota (s8, resta nota*115/4096 octavas), +4 atenuacion (ganancia
  2^-((15-a)/2)), +6 curva de pitch 8 x {x, semitonos s8, fino s8 (1/64 de semitono), 0}, +38 curva de volumen
  8 x {x, v} (0..127), +54 curva de panorama, +70 otra (no usada aqui). Las curvas se interpolan en el tramo
  x0 < x <= x1 (fuera de todos, 0).
  Canales del juego (0x11ebc8): 0 = definicion 18 (motor con gas), 1 = 19 (motor sin gas), 3 = 0 (derrape),
  6 = 3 (golpe contra muro; tabla de muros en 0x3c1350, primera entrada mascara 0x20 -> definicion 3, ganancia 0.56).
- Efectos de los menus (bancos "SE"): \\MENU.pak y \\Fe.pak de SOUND.PS2. Cabecera {u32 tamano A, 0, u32 tamano B,
  0}, A = PS-ADPCM desde +16, B (aqui vacio) y detras la tabla {u32 n, n x {u32 id, u32 desplazamiento en A}}; cada
  muestra llega hasta el bloque con la marca de fin (bit 0). El id es el numero de efecto del juego (comandos del
  frontal del port de PC, menu_audio.cpp): 1 cursor, 0x40 aceptar, 3 bloqueado, 0 cancelar/volver. Su pitch y volumen
  estan en el .data de SOUNDCD.IRX, direccion 0x4b98 + 12*id: {u16 pitch SPU, u16 volumen (0x3fff = maximo), ...}.
  Sin bucle; en SFX.DAT se llaman "se%03x" (id) y llevan el volumen del IRX en el u16 final de la muestra.
- Umbrales de rpm para el parametro x del motor: 128 floats en 0x3c13b0 del ELF (0, 1100, 1125 ... 10000).

SFX.DAT (little endian):
  cabecera 32 bytes: 'SFX1', u32 n_muestras, u32 n_grupos, u32 off_muestras, u32 off_grupos, u32 off_datos,
                     u32 off_rpm (128 floats), u32 tamano total
  muestra 36 bytes:  char nombre[12], u32 desp (desde off_datos), u32 bytes, u32 frecuencia (Hz),
                     u32 ini_bucle, u32 fin_bucle (en muestras), u8 formato (0 = PCM 16, 2 = ADPCM Yamaha),
                     u8 bucle, u16 volumen (efectos de menu: el del IRX, 0..0x3fff; 0 en los ICS)
  grupo:             char nombre[12], u32 n_capas, y n_capas capas de 52 bytes:
                     u8 muestra, s8 atenuacion, s8 nota, u8 0, 8 x {s8 x, s8 semitonos, s8 fino, u8 0},
                     8 x {s8 x, s8 v}
  datos de las muestras (cada una alineada a 32 bytes).
Las muestras en bucle ADPCM llevan delante las ultimas K muestras del bucle (ini_bucle = K) para que el estado del
decodificador al volver al principio del bucle coincida con el del final (no hay chasquido ni con el AICA que guarda
el estado en LSA ni con uno que lo arrastre).
VOCES.DAT (voces de la acompanante y del locutor, y los efectos de carrera que las acompanan; --idioma, por defecto
SPANISH; se escribe a la vez que SFX.DAT):
- Bancos de voz: \\<IDIOMA>\\ORC.PAK (carrera del modo OutRun) y \\<IDIOMA>\\PERO.pak (GOAL / TIME OVER / GAME OVER).
  El original elige el directorio con la tabla de prefijos del ELF (0x3527a0: 5 juegos de voces x 6 idiomas;
  "%s\\%s" en 0x3c1220); en carrera normal usa el juego 4 (SPRACE, FRENCH, GERMAN, ITALIAN, SPANISH, SPRACE): con el
  idioma espanol, SPANISH. El banco: cabecera {u32 tamano A, 0, u32 tamano B, 0}, A y B PS-ADPCM y detras dos
  tablas {u32 n, n x {u32 id, u32 desplazamiento}}, la primera para A y la segunda para B (cada muestra llega hasta
  el bloque con la marca de fin). Pitch y volumen del id en la tabla SE del IRX (0x4b98 + 12*id): las voces van a
  0x400 = 12 kHz y volumen 0x3fff.
- Efectos: 0x4d (EXTEND TIME, parte B de \\IGC.PAK) y 0x18d (aviso de poco tiempo, parte A de \\IGC.PAK).
- Tablas de cuando habla (ELF; las mismas que lee el port de PC en race_robots.cpp, alli en 6549E0.. y 5C1xxx):
  0x36f958 comentarios de zona (16 bytes {id, retardo, prioridad s16, 0, zona}, hasta prioridad < 0),
  0x36f9a8 comentarios por numero de derrapes (24 bytes {id, retardo, prioridad, exacto, desde, hasta}),
  0x36fa80 comentarios de derrape largo (16 bytes {id, retardo, prioridad, cuadros}),
  0x40ef60 asustada (2), 0x40ef68 trompo (2), 0x40ef70 choque con otro coche (5),
  0x40eba8 / 0x40ebb8 al empezar un choque (3; la primera para los personajes 9 y 10),
  0x40eb10 al recuperarse de un choque grande (4) y 0x40eb20 de uno pequeno (2), con las cuentas de sus listas de
  movimientos en 0x40eb28 + 0x34 / + 0x3c (coche cerrado) y 0x40eb68 (descapotable).
  Los id llevan a veces 0x800 (o 0x5000) encima: banderas del controlador de sonido; el id es el de los 9 bits bajos.
VOCES.DAT (little endian):
  cabecera 32 bytes: 'VOZ1', u32 n_muestras, u32 off_muestras, u32 off_tablas, u32 off_datos, u32 total,
                     char idioma[8]
  muestra 20 bytes:  u16 id, u16 volumen (0..0x3fff), u32 desp (desde off_datos), u32 bytes, u32 frecuencia,
                     u32 n muestras (ADPCM Yamaha, sin bucle)
  tablas:            24 x {u16 id, u16 retardo, s16 prioridad, s16 0, s32 a, s32 b, s32 c} (8 de zona: a = zona;
                     12 de derrapes: a = exacto, b = desde, c = hasta; 4 de derrape largo: a = cuadros; cada lista
                     acaba en prioridad < 0) y 24 u16: asustada[2], trompo[2], choque[5], aire9[3], aire[3],
                     aterriza[4], salto[2], n_aterriza, n_salto, 0
"""
import os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.iso import Iso

ELF_DEF_N, ELF_DEF_T = 0x3526a4, 0x3526a8
ELF_RPM = 0x3c13b0
IRX_TABLA = 0x66a0
GRUPOS = [('motor_acel', 18), ('motor_suelta', 19), ('derrape', 0), ('golpe', 3)]
IRX_SE = 0x4b98         # tabla de efectos SE del IRX: 12 bytes por id
MENU_SE = [('\\MENU.pak', (0x00, 0x01, 0x40)), ('\\Fe.pak', (0x03,))]   # cancelar, cursor, aceptar, bloqueado
K_PREVIO = 256          # muestras de calentamiento del ADPCM antes del bucle


def hash_nombre(s):
    h = 0
    for c in reversed(s.upper().encode()):
        h = (h * 131 + c) & 0xffffffff
    return h


def fichero_paquete(iso, paquete, nombre):
    h = hash_nombre(nombre)
    for hh, o, s in iso.paquete(paquete):
        if hh == h:
            return iso.m[o:o + s]
    raise SystemExit('no esta %s en %s' % (nombre, paquete))


def elf_seccion_irx(irx):
    """(direccion, desplazamiento, tamano) de la .data del IRX (ELF del IOP)."""
    sh_off, = struct.unpack_from('<I', irx, 0x20)
    sh_n, sh_str = struct.unpack_from('<HH', irx, 0x30)
    secs = [struct.unpack_from('<10I', irx, sh_off + 40 * i) for i in range(sh_n)]
    st = secs[sh_str][4]
    for s in secs:
        nm = irx[st + s[0]:irx.index(b'\0', st + s[0])]
        if nm == b'.data':
            return s[3], s[4], s[5]
    raise SystemExit('IRX sin .data')


def dec_psx(d):
    """PS-ADPCM (bloques de 16 bytes) -> lista de enteros de 16 bits."""
    f0 = (0, 60, 115, 98, 122); f1 = (0, 0, -52, -55, -60)
    s1 = s2 = 0; out = []
    for b in range(0, len(d), 16):
        sh = d[b] & 15; fl = d[b] >> 4
        if fl > 4: fl = 0
        if sh > 12: sh = 9
        for i in range(28):
            n = (d[b + 2 + i // 2] >> (4 * (i & 1))) & 15
            if n >= 8: n -= 16
            v = ((n << 12) >> sh) + ((s1 * f0[fl] + s2 * f1[fl] + 32) >> 6)
            v = max(-32768, min(32767, v))
            out.append(v); s2 = s1; s1 = v
    return out


IDX_Y = (230, 230, 230, 230, 307, 409, 512, 614)
DIF_Y = (1, 3, 5, 7, 9, 11, 13, 15)


def enc_yamaha(pcm):
    """ADPCM Yamaha del AICA (nibble bajo = primera muestra). Estado inicial: prediccion 0, paso 127."""
    pr, paso = 0, 127; nib = []
    for s in pcm:
        dlt = s - pr
        n = min(7, abs(dlt) * 4 // paso)
        q = (paso * DIF_Y[n]) // 8
        if dlt < 0: n |= 8; pr -= q
        else: pr += q
        pr = max(-32768, min(32767, pr))
        paso = max(127, min(24576, (paso * IDX_Y[n & 7]) >> 8))
        nib.append(n)
    if len(nib) & 1: nib.append(0)
    return bytes(nib[i] | (nib[i + 1] << 4) for i in range(0, len(nib), 2))


def frecuencia(p):
    """pitch de la SPU (0x1000 = 48 kHz) a Hz, redondeado a la frecuencia estandar mas cercana"""
    if p == 0x1000: return 48000
    f = round(p * 48000 / 4096)
    return min((16000, 22050, 11025, 32000, 44100, 48000, 24000, 30000, 12000, 8000), key=lambda r: abs(r - f))


def efectos_menu(iso, irx, tb, td):
    """[(nombre, pcm, frecuencia, volumen)] de los efectos de menu de los bancos SE"""
    r = []
    for banco, ids in MENU_SE:
        d = fichero_paquete(iso, 'SOUND.PS2', banco)
        a, tipo, b, _ = struct.unpack_from('<4I', d, 0)
        assert tipo == 0, '%s no es un banco SE' % banco
        o = 16 + a + b; n, = struct.unpack_from('<I', d, o)
        tabla = dict(struct.unpack_from('<2I', d, o + 4 + 8 * i) for i in range(n))
        for i in ids:
            ini = 16 + tabla[i]; fin = ini
            while not d[fin + 1] & 1: fin += 16
            p, v = struct.unpack_from('<HH', irx, td + IRX_SE - tb + 12 * i)
            assert 0x100 <= p <= 0x3fff and 0 < v <= 0x3fff, 'tabla SE del IRX inesperada (id %d)' % i
            r.append(('se%03x' % i, dec_psx(d[ini:fin + 16]), frecuencia(p), v))
    return r


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__.split('\n')[0])
    iso = Iso(sys.argv[1]); sal = sys.argv[2]
    pcm16 = '--pcm16' in sys.argv
    os.makedirs(sal, exist_ok=True)
    lba, tam = iso.fich['SLES_539.98']
    elf = iso.m[lba:lba + tam]
    V = lambda a: a - 0x100000 + 0x80          # segmento unico: 0x100000 <- desplazamiento 0x80
    assert elf[:4] == b'\x7fELF' and struct.unpack_from('<I', elf, 0x18)[0] == 0x101100, 'ELF inesperado'
    ics = fichero_paquete(iso, 'SOUND.PS2', '\\ICS.PAK')
    irx = fichero_paquete(iso, 'SYSTEM.PS2', '\\IRX\\SOUNDCD.IRX')
    tb, td, _ = elf_seccion_irx(irx)
    t_irx = [struct.unpack_from('<IH', irx, td + IRX_TABLA - tb + 8 * k) for k in range(68)]
    datos_ics = ics[16:16 + struct.unpack_from('<I', ics, 0)[0]]
    assert struct.unpack_from('<I', ics, 4)[0] == 1, 'ICS.PAK sin la marca de banco ICS'
    # limites de cada muestra: hasta el bloque con marca de fin (el bloque mudo de detras no cuenta)
    muestras_ps = []
    for k, (o, p) in enumerate(t_irx):
        i = o
        while not datos_ics[i + 1] & 1: i += 16
        assert datos_ics[o + 1] & 4 and datos_ics[i + 1] & 2, 'muestra %d sin bucle' % k
        muestras_ps.append((o, i + 16 - o, p))
    n_def = struct.unpack_from('<I', elf, V(ELF_DEF_N))[0]
    assert n_def == 20, 'tabla ICS inesperada'
    rpm = struct.unpack_from('<128f', elf, V(ELF_RPM))
    assert rpm[1] == 1100.0 and rpm[127] == 10000.0, 'tabla de rpm inesperada'

    usadas = []; grupos = []
    for nombre, nd in GRUPOS:
        nc, pt = struct.unpack_from('<II', elf, V(ELF_DEF_T) + 8 * nd)
        capas = []
        for l in range(nc):
            b = elf[V(pt) + 86 * l:V(pt) + 86 * l + 86]
            m = b[0]
            if m not in usadas: usadas.append(m)
            capas.append(bytes([usadas.index(m), b[4], b[3], 0]) + bytes(b[6:38]) + bytes(b[38:54]))
        grupos.append((nombre, capas))

    tabla = []; blob = bytearray()
    for m in usadas:
        o, n, p = muestras_ps[m]
        pcm = dec_psx(datos_ics[o:o + n])
        frec = frecuencia(p)                    # 0x555 / 0x759 / 0x3ac son 16000 / 22050 / 11025 redondeados
        while len(blob) % 32: blob.append(0)
        if pcm16:
            datos = struct.pack('<%dh' % len(pcm), *pcm); ini, fin, fmt = 0, len(pcm), 0
        else:
            k = max(0, min(K_PREVIO, len(pcm), 65534 - len(pcm))) & ~1   # LSA/LEA del AICA: 16 bits
            datos = enc_yamaha(pcm[len(pcm) - k:] + pcm); ini, fin, fmt = k, k + len(pcm), 2
        assert fin < 65536, 'muestra %d demasiado larga para un bucle del AICA' % m
        tabla.append((('ics%02x' % m).encode(), len(blob), len(datos), frec, ini, fin, fmt, 1, 0))
        blob += datos
    for nm, pcm, frec, vol in efectos_menu(iso, irx, tb, td):  # sin bucle: sin muestras previas
        while len(blob) % 32: blob.append(0)
        if pcm16: datos = struct.pack('<%dh' % len(pcm), *pcm); fmt = 0
        else: datos = enc_yamaha(pcm); fmt = 2
        assert len(pcm) < 65536, '%s demasiado larga para el AICA' % nm
        tabla.append((nm.encode(), len(blob), len(datos), frec, 0, len(pcm), fmt, 0, vol))
        blob += datos

    n_m = len(tabla); off_m = 32; off_g = off_m + 36 * n_m
    g_bytes = b''.join(nm.encode().ljust(12, b'\0') + struct.pack('<I', len(c)) + b''.join(c) for nm, c in grupos)
    off_r = off_g + len(g_bytes); off_d = (off_r + 512 + 31) & ~31
    total = off_d + len(blob)
    out = bytearray(b'SFX1' + struct.pack('<7I', n_m, len(grupos), off_m, off_g, off_d, off_r, total))
    for nm, d, n, f, ini, fin, fmt, bucle, vol in tabla:
        out += nm.ljust(12, b'\0') + struct.pack('<5IBBH', d, n, f, ini, fin, fmt, bucle, vol)
    out += g_bytes + struct.pack('<128f', *rpm)
    out += bytes(off_d - len(out)) + blob
    with open(os.path.join(sal, 'SFX.DAT'), 'wb') as f:
        f.write(out)
    print('SFX.DAT: %d bytes, %d muestras (%s), grupos %s' % (len(out), n_m, 'PCM16' if pcm16 else 'ADPCM',
          ', '.join('%s=%d capas' % (g, len(c)) for g, c in grupos)))
    for nm, d, n, f, ini, fin, fmt, bucle, vol in tabla:
        print('  %s %6d bytes %5d Hz %s %d..%d' % (nm.decode(), n, f, 'bucle' if bucle else 'vol 0x%x' % vol, ini, fin))
    idioma = sys.argv[sys.argv.index('--idioma') + 1].upper() if '--idioma' in sys.argv else 'SPANISH'
    assert idioma in IDIOMAS, 'idioma: ' + ', '.join(IDIOMAS)
    voces(iso, irx, tb, td, elf, V, idioma, sal)


# ---------------------------------------------------------------- voces (VOCES.DAT)
VOZ_BANCOS = ('ORC.PAK', 'PERO.pak')
VOZ_SE = (('\\IGC.PAK', 1, 0x4d), ('\\IGC.PAK', 0, 0x18d))      # (banco, parte, id)
VOZ_AREA, VOZ_VUELTA, VOZ_DERRAPE = 0x36f958, 0x36f9a8, 0x36fa80
VOZ_LISTAS = ((0x40ef60, 2), (0x40ef68, 2), (0x40ef70, 5), (0x40eba8, 3), (0x40ebb8, 3), (0x40eb10, 4), (0x40eb20, 2))
VOZ_MOV = 0x40eb28                  # movimientos del coche cerrado: {lista, cuenta} x 8
IDIOMAS = ('ENGLISHC', 'FRENCH', 'GERMAN', 'ITALIAN', 'SPANISH', 'ENGLISHH', 'ENGLISHJ')


def banco_tablas(d):
    """(datos A, datos B, [tabla A, tabla B]) de un banco .PAK con sus tablas {id: desplazamiento}"""
    a, _, b, _ = struct.unpack_from('<4I', d, 0)
    o = 16 + a + b; t = []
    for _ in range(2):
        n = struct.unpack_from('<I', d, o)[0] if o + 4 <= len(d) else 0
        t.append(dict(struct.unpack_from('<2I', d, o + 4 + 8 * k) for k in range(n))); o += 4 + 8 * n
    return d[16:16 + a], d[16 + a:16 + a + b], t


def muestra_banco(datos, desp):
    fin = desp
    while not datos[fin + 1] & 1: fin += 16
    return dec_psx(datos[desp:fin + 16])


def voces(iso, irx, tb, td, elf, V, idioma, sal):
    se = lambda i: struct.unpack_from('<HH', irx, td + IRX_SE - tb + 12 * i)
    muestras = {}
    for nb in VOZ_BANCOS:
        A, B, (ta, tbb) = banco_tablas(fichero_paquete(iso, 'SOUND.PS2', '\\%s\\%s' % (idioma, nb)))
        for datos, t in ((A, ta), (B, tbb)):
            for i, o in t.items():
                p, v = se(i)
                if p != 0x400 or i in muestras: continue           # solo las voces (12 kHz); los SE van aparte
                muestras[i] = (muestra_banco(datos, o), frecuencia(p), v)
    for nb, parte, i in VOZ_SE:
        A, B, t = banco_tablas(fichero_paquete(iso, 'SOUND.PS2', nb))
        p, v = se(i)
        assert 0x100 <= p <= 0x3fff and 0 < v <= 0x3fff, 'tabla SE del IRX inesperada (id 0x%x)' % i
        muestras[i] = (muestra_banco((A, B)[parte], t[parte][i]), frecuencia(p), v)

    def u32(a): return struct.unpack_from('<I', elf, V(a))[0]
    def lista(a, tam, conv):
        r = []
        while True:
            e = conv(a); r.append(e); a += tam
            if e[2] < 0: return r
            assert len(r) < 12, 'tabla de comentarios sin final en 0x%x' % a
    area = lista(VOZ_AREA, 16, lambda a: (u32(a) & 0x1ff, u32(a + 4), struct.unpack_from('<h', elf, V(a + 8))[0],
                                          struct.unpack_from('<i', elf, V(a + 12))[0], 0, 0))
    vuelta = lista(VOZ_VUELTA, 24, lambda a: (u32(a) & 0x1ff, u32(a + 4), struct.unpack_from('<h', elf, V(a + 8))[0],
                                              *struct.unpack_from('<3i', elf, V(a + 12))))
    derrape = lista(VOZ_DERRAPE, 16, lambda a: (u32(a) & 0x1ff, u32(a + 4), struct.unpack_from('<h', elf, V(a + 8))[0],
                                                struct.unpack_from('<i', elf, V(a + 12))[0], 0, 0))
    assert len(area) <= 8 and len(vuelta) <= 12 and len(derrape) <= 4, 'tablas de comentarios inesperadas'
    listas = []
    for a, n in VOZ_LISTAS:
        listas += [u32(a + 4 * k) & 0x1ff for k in range(n)]
    n_at, n_sa = u32(VOZ_MOV + 0x34), u32(VOZ_MOV + 0x3c)
    assert 0 < n_at <= 4 and 0 < n_sa <= 2, 'listas de movimientos inesperadas'
    print('  zona %s\n  derrapes %s\n  derrape largo %s\n  listas %s, cuentas %d %d' % (
        area, vuelta, derrape, ['%x' % x for x in listas], n_at, n_sa))
    usados = set(e[0] for e in area + vuelta + derrape if e[2] >= 0) | set(x for x in listas if x)
    for x in sorted(usados):
        if x not in muestras: print('  aviso: la voz 0x%x no esta en los bancos de %s' % (x, idioma))
    tablas = bytearray()
    for t, n in ((area, 8), (vuelta, 12), (derrape, 4)):
        t = t + [(0, 0, -1, 0, 0, 0)] * (n - len(t))
        for i, r, pr, a, b, c in t:
            tablas += struct.pack('<HHhh3i', i, r, pr, 0, a, b, c)
    tablas += struct.pack('<24H', *(listas + [n_at, n_sa, 0]))
    blob = bytearray(); filas = []
    for i in sorted(muestras):
        pcm, f, v = muestras[i]
        while len(blob) % 32: blob.append(0)
        datos = enc_yamaha(pcm)
        assert len(pcm) < 65536, 'voz 0x%x demasiado larga para el AICA' % i
        filas.append(struct.pack('<HH4I', i, v, len(blob), len(datos), f, len(pcm)))
        blob += datos
    off_m = 32; off_t = off_m + 20 * len(filas); off_d = (off_t + len(tablas) + 31) & ~31
    out = bytearray(b'VOZ1' + struct.pack('<5I', len(filas), off_m, off_t, off_d, off_d + len(blob)) +
                    idioma.encode()[:8].ljust(8, b'\0'))
    out += b''.join(filas) + tablas
    out += bytes(off_d - len(out)) + blob
    with open(os.path.join(sal, 'VOCES.DAT'), 'wb') as fh:
        fh.write(out)
    print('VOCES.DAT (%s): %d bytes, %d muestras, %.1f s' % (idioma, len(out), len(filas),
          sum(len(m[0]) / m[1] for m in muestras.values())))


if __name__ == '__main__':
    main()
