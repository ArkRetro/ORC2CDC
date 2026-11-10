#!/usr/bin/env python3
"""conv_meta.py <iso> <dir_salida>: la escena de la meta del original (modo 19 GOAL) -> META.DAT.

Todo se lee del disco (SLES-53998); aqui no hay datos del juego. Origen (PC OR2006C2C.EXE de la decompilacion,
platform/race_end_modes.cpp y race_goal_camera.cpp, contrastado con el codigo de PS2):
- 49C9C0 (modo 19, PS2 0x1b8e98): escena = 3; con variante != 4, 5, 6, 7 y [0x359d98] == 0x42 (PC 635F34), la del
  byte 0x3f99b0 + (meta + preset x 5) x 4 (PC 5C2370); meta = 450380(8) - 0xA. Musica de la meta 401000(0, pista,
  0): variante 1 tabla 0x3f9a18 (PC 5C23D4), variante 2 0x3f9a30 (PC 5C23E8) por meta.
- 4871A0 (PS2 0x1f4ef0): guion de la escena 0x36f2a0[escena] (PC 653788): registros de 0x2C bytes {op, w4, w8, wC,
  w10, f14, f18, ..., s16 cuadro +0x24, hecho +0x28} hasta op 0x1F; 486730 (PS2 0x1f43d0) los ejecuta una vez cuando
  el cuadro 82E7C4 llega a +0x24.
- 486EF0 (PS2 0x1f4c08): movimiento de camara 0x4229d0 + escena x 8 {puntero, duracion} (PC 5E61D0, 513730 / 513740 =
  PS2 0x2914f8 / 0x291518): 20 canales {s16 n, puntero a n claves de 4 floats {t, valor, tangente de entrada, de
  salida}} muestreados con 513650 (PS2 0x21bdd8).
- Efecto 0x96 del guion (op 5, 4249F0 = PS2 0x11b2b0): banco \\PAS3.PAK parte A (indice 6 de la tabla de bancos
  0x352840, cargado en el hueco 2 durante la carrera: 427700(0x206) del PC); pitch y volumen de la tabla SE de
  \\IRX\\SOUNDCD.IRX (0x4b98 + 12 x id).

META.DAT (little endian): 'MET1', u32 n escenas (36), u32 desp. de las escenas, u32 desp. de los datos;
  u8 escena[4 presets][5 metas] (20), u8 pista_v1[5], u8 pista_v2[5], u8 0 x 2;
  efecto: u16 id, u16 volumen SE, u32 hz, u32 muestras, u32 desp., u32 bytes (ADPCM de Yamaha);
  por escena (36) {u32 desp. del guion (0 = no esta), u32 n registros, u32 desp. del movimiento (0 = sin), f32
  duracion}; guion: n x {u32 op, u32 w[4], f32 f14, f32 f18, s16 cuadro, u16 0}; movimiento: 20 x {u32 n, u32 desp.
  de las claves} y las claves (4 floats)."""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.iso import Iso
import conv_sonido as S

GUIONES, MOVS, ESCENA_META = 0x36f2a0, 0x4229d0, 0x3f99b0
PISTA_V1, PISTA_V2 = 0x3f9a18, 0x3f9a30
NESC = 36
EFECTO = ('\\PAS3.PAK', 0, 0x96)


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
    escenas = [u32(ESCENA_META + 4 * k) & 0xff for k in range(20)]
    usadas = sorted(set(escenas) | {3})
    cab = bytearray(b'MET1')
    tabla = bytearray(bytes(escenas) + bytes(u32(PISTA_V1 + 4 * k) & 0xff for k in range(5)) +
                      bytes(u32(PISTA_V2 + 4 * k) & 0xff for k in range(5)) + b'\0\0')
    # efecto 0x96
    irx = S.fichero_paquete(iso, 'SYSTEM.PS2', '\\IRX\\SOUNDCD.IRX'); tb, td, _ = S.elf_seccion_irx(irx)
    p, v = struct.unpack_from('<HH', irx, td + S.IRX_SE - tb + 12 * EFECTO[2])
    A, B, t = S.banco_tablas(S.fichero_paquete(iso, 'SOUND.PS2', EFECTO[0]))
    pcm = S.muestra_banco((A, B)[EFECTO[1]], t[EFECTO[1]][EFECTO[2]])
    adp = S.enc_yamaha(pcm)
    assert len(pcm) < 65536
    o_dat = 4 + 12 + len(tabla) + 20 + 16 * NESC                # desplazamientos de los datos: desde el principio
    datos = bytearray(); escs = []
    def al():
        while len(datos) % 4: datos.append(0)
    for e in range(NESC):
        if e not in usadas: escs.append((0, 0, 0, 0.0)); continue
        r = u32(GUIONES + 4 * e); regs = []
        while True:
            op = u32(r); fr = i16(r + 0x24)
            if op == 0x1f or fr < 0: break
            assert op <= 0x1e
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
        print('escena %2d: %2d registros, movimiento %s (%.1f s, %d claves)' % (
            e, len(regs), 'si' if om else 'no', dur, sum(max(0, i16(d + 8 * k)) for k in range(20)) if d else 0))
    o_esc = 4 + 12 + len(tabla) + 20
    assert o_dat == o_esc + 16 * NESC
    al(); o_ef = len(datos); datos += adp
    out = cab + struct.pack('<3I', NESC, o_esc, o_dat) + tabla + struct.pack('<HH4I', EFECTO[2], v, S.frecuencia(p),
                                                                            len(pcm), o_ef, len(adp))
    assert len(out) == o_esc
    for og, n, om, dur in escs:
        out += struct.pack('<IIIf', og + o_dat if n else 0, n, om + o_dat if om else 0, dur)
    out += datos
    with open(os.path.join(sal, 'META.DAT'), 'wb') as fh: fh.write(out)
    print('META.DAT: %d bytes; escenas por preset y meta %s; pistas %s / %s; efecto 0x%x %d Hz %d muestras vol 0x%x' % (
        len(out), escenas, list(tabla[20:25]), list(tabla[25:30]), EFECTO[2], S.frecuencia(p), len(pcm), v))


if __name__ == '__main__':
    main()
