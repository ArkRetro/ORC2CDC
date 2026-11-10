#!/usr/bin/env python3
"""conv_salida.py <iso> <dir_salida>: la escena de la salida del original (modo 13 START) -> SALIDA.DAT.

Todo se lee del disco (SLES-53998); aqui no hay datos del juego. Origen (PC OR2006C2C.EXE de la decompilacion,
platform/native_runtime.cpp START 49DB20 / 49DD40 y race_goal_camera.cpp 4871A0 / 486730 / 486EF0, contrastado con el
codigo de PS2):
- START, etapa 60 (PS2 0x1b6824..0x1b69ac): con un jugador (0x1cb240() <= 1) escena = byte de la tabla 0x3f9338
  [0x1bcfb0()] (30 palabras, la eleccion de coche [0x369268] = el modelo); si sale 12 o 13 y la palabra 0 del
  descriptor de la etapa (0x1766f0) no es 0 ni 15, la 11; con mas jugadores la tabla local 0x3f9328 [n - 1]
  (0, 10, 10, 10). Luego 0x1f4ef0 (4871A0) con la escena.
- Guion 0x36f2a0[escena] y movimiento 0x4229d0 + escena x 8, como la escena de la meta (tools/conv_meta.py).
- Efectos del guion (op 5, 4249F0 = PS2 0x11b2b0): banco \\PAS1.PAK parte A (0x97..0x9d y 0x16a); tono y volumen de
  la tabla SE de \\IRX\\SOUNDCD.IRX (0x4b98 + 12 x id). 0x16a solo si [0x5d7220] == 0 (0x1f6548: el idioma; 0x1071d8
  lo pone a 0 con el ingles).
- El AICA no toca mas de 65535 muestras por canal (LSA / LEA de 16 bits): una muestra mas larga va en dos trozos
  ADPCM; el segundo empieza en `corte` (un paso por cero de la muestra, unos 40 ms antes del final del primero) y
  comun/sfx.c lo arranca a su hora (y corta el primero). Limite del hardware, no del original.

SALIDA.DAT (little endian): 'SAL1', u32 n escenas (15), u32 desp. de las escenas, u32 desp. de los datos, u32 n
  efectos, u32 desp. de los efectos, u32 bytes de la parte residente (cabecera, tablas, guiones, movimientos y
  efectos; las muestras van detras); u8 rutas[30] (0x3f9338), u8 rutas_varios[4] (0x3f9328), u8 0 x 2;
  por escena (15) {u32 desp. del guion (0 = no esta), u32 n registros, u32 desp. del movimiento (0 = sin), f32
  duracion} (las 10..14; como META.DAT); efecto: {u16 id, u16 volumen SE, u32 hz, u32 muestras, u32 desp., u32 bytes,
  u32 corte, u32 muestras 2, u32 desp. 2, u32 bytes 2} (ADPCM de Yamaha)."""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.iso import Iso
import conv_sonido as S

GUIONES, MOVS = 0x36f2a0, 0x4229d0
RUTAS, RUTAS_VARIOS = 0x3f9338, 0x3f9328
ESCENAS = range(10, 15)
NESC = 15
BANCO = '\\PAS1.PAK'
MAX_AICA = 65534                        # muestras por trozo (par: 2 por byte)
SOLAPE = 1200                           # ~40 ms a 30 kHz: margen del segundo trozo


def corte_de(pcm, n):
    """paso por cero de la muestra mas cercano por debajo de n - SOLAPE"""
    s = pcm; c = n - SOLAPE
    for k in range(c, c - 600, -1):
        if (s[k - 1] <= 0 <= s[k]) or (s[k - 1] >= 0 >= s[k]): return k & ~1
    return c & ~1


def main():
    if len(sys.argv) < 3: raise SystemExit(__doc__.split('\n')[0])
    iso = Iso(sys.argv[1]); sal = sys.argv[2]
    lba, tam = iso.fich['SLES_539.98']
    elf = bytes(iso.m[lba:lba + tam])
    assert elf[:4] == b'\x7fELF' and struct.unpack_from('<I', elf, 0x18)[0] == 0x101100, 'ELF inesperado'
    V = lambda a: a - 0x100000 + 0x80
    u32 = lambda a: struct.unpack_from('<I', elf, V(a))[0]
    f32 = lambda a: struct.unpack_from('<f', elf, V(a))[0]
    i16 = lambda a: struct.unpack_from('<h', elf, V(a))[0]
    rutas = bytes(u32(RUTAS + 4 * k) & 0xff for k in range(30))
    varios = bytes(u32(RUTAS_VARIOS + 4 * k) & 0xff for k in range(4))
    assert all(r in ESCENAS for r in rutas) and all(r == 0 or r in ESCENAS for r in varios), 'tabla de rutas inesperada'
    tabla = rutas + varios + b'\0\0'
    o_esc = 4 + 24 + len(tabla); o_dat = o_esc + 16 * NESC
    datos = bytearray(); escs = []; ids = set()

    def al():
        while len(datos) % 4: datos.append(0)
    for e in range(NESC):
        if e not in ESCENAS: escs.append((0, 0, 0, 0.0)); continue
        r = u32(GUIONES + 4 * e); regs = []
        while True:
            op = u32(r); fr = i16(r + 0x24)
            if op == 0x1f or fr < 0: break
            assert op <= 0x1e
            if op == 5: ids.add(u32(r + 4))
            regs.append(struct.pack('<5I2fhH', op, u32(r + 4), u32(r + 8), u32(r + 12), u32(r + 16), f32(r + 0x14),
                                    f32(r + 0x18), fr, 0))
            r += 0x2c
        al(); og = len(datos); datos += b''.join(regs)
        d, dur = u32(MOVS + 8 * e), f32(MOVS + 8 * e + 4)
        om = 0
        if d:
            al(); om = len(datos); datos += bytes(160); claves = []
            for k in range(20):
                n = max(0, i16(d + 8 * k)); pk = u32(d + 8 * k + 4)
                claves.append((n, bytes(elf[V(pk):V(pk) + 16 * n]) if n else b''))
            for k, (n, b) in enumerate(claves):
                struct.pack_into('<2I', datos, om + 8 * k, n, o_dat + len(datos)); datos += b
        escs.append((og, len(regs), om, dur))
        print('escena %2d: %2d registros, movimiento %s (%.1f s)' % (e, len(regs), 'si' if om else 'no', dur))
    # efectos (tabla en la parte residente, muestras detras)
    irx = S.fichero_paquete(iso, 'SYSTEM.PS2', '\\IRX\\SOUNDCD.IRX'); tb, td, _ = S.elf_seccion_irx(irx)
    A, B, t = S.banco_tablas(S.fichero_paquete(iso, 'SOUND.PS2', BANCO))
    ids = sorted(ids)
    assert all(i in t[0] for i in ids), 'efecto de la salida fuera de %s' % BANCO
    al(); o_ef = o_dat + len(datos); datos += bytes(36 * len(ids))
    residente = o_dat + len(datos)
    muestras = bytearray(); filas = []
    for i in ids:
        p, v = struct.unpack_from('<HH', irx, td + S.IRX_SE - tb + 12 * i)
        assert 0x100 <= p <= 0x3fff and 0 < v <= 0x3fff, 'tabla SE del IRX inesperada (id 0x%x)' % i
        pcm = S.muestra_banco(A, t[0][i]); n = len(pcm)
        trozos = [(0, n)] if n <= MAX_AICA else [(0, MAX_AICA), (corte_de(pcm, MAX_AICA), n)]
        fila = [i, v, S.frecuencia(p)]
        for k, (a, b) in enumerate(trozos):
            adp = S.enc_yamaha(pcm[a:b])
            while len(muestras) % 32: muestras.append(0)
            if k == 1: fila.append(a)
            fila += [b - a, residente + len(muestras), len(adp)]
            muestras += adp
        if len(trozos) == 1: fila += [0, 0, 0, 0]
        filas.append(struct.pack('<HH8I', *fila))
        print('  efecto 0x%x: %d Hz, %d muestras, vol 0x%x%s' % (i, fila[2], n, v,
              ', en dos trozos (corte %d)' % fila[6] if len(trozos) > 1 else ''))
    datos[o_ef - o_dat:o_ef - o_dat + 36 * len(ids)] = b''.join(filas)
    out = bytearray(b'SAL1' + struct.pack('<6I', NESC, o_esc, o_dat, len(ids), o_ef, residente)) + tabla
    assert len(out) == o_esc
    for og, n, om, dur in escs:
        out += struct.pack('<IIIf', og + o_dat if n else 0, n, om + o_dat if om else 0, dur)
    out += datos
    assert len(out) == residente
    out += muestras
    with open(os.path.join(sal, 'SALIDA.DAT'), 'wb') as fh: fh.write(out)
    print('SALIDA.DAT: %d bytes (%d residentes); rutas %s, varios %s' % (len(out), residente, list(rutas), list(varios)))


if __name__ == '__main__':
    main()
