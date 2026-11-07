"""Texturas de un bloque de STAGE.PS2 (tu disco). Tras los 72*n bytes de registros hay una tabla de n_tex
desplazamientos (en A) a cabeceras de 56 bytes:
 0 datos (desde A; la parte B), 1 paleta (0 = sin paleta), 3 tamano del nivel 0 en qwords, 4 qwords de paleta,
 10 (alto/2 << 16 | ancho/2), 11 log2(ancho) << 24 | psm << 16 | ..., 12 log2(alto) | ..., 13 niveles de mip << 8.
psm 0x00 PSMCT32 lineal, 0x13 PSMT8 guardado como rectangulo PSMCT32 y 0x14 PSMT4 como PSMCT16 (ancho/2 x alto/2).
Paletas en CSM1: 256 colores con los bits 3 y 4 del indice cambiados; 16 colores = entradas 0-7 y 16-23.
Alfa de PS2: 0x80 = opaco."""
import struct
import numpy as np
from . import gs

_SW = np.arange(256); _SW = (_SW & 0xE7) | ((_SW & 8) << 1) | ((_SW & 16) >> 1)

def cabeceras(seg):
    m, A = seg.m, seg.A
    base = A + 72 * seg.w[0]; n = seg.w[1]
    offs = struct.unpack('<%dI' % n, m[base:base + 4 * n])
    res = []
    for o in offs:
        if o >= seg.w[2] or o < 72 * seg.w[0]: break        # despues de las texturas vienen otras tablas
        res.append(struct.unpack('<14I', m[A + o:A + o + 56]))
    return res

def imagen(seg, h):
    """RGBA uint8 (alto, ancho, 4) del nivel 0, alfa ya en 0-255"""
    m, A = seg.m, seg.A
    psm = (h[11] >> 16) & 0xff; W = 1 << (h[11] >> 24); H = 1 << (h[12] & 0xff); d = A + h[0]
    if psm == 0:
        a = np.frombuffer(m[d:d + W * H * 4], 'u1').reshape(H, W, 4).copy()
    else:
        pal = np.frombuffer(m[A + h[1]:A + h[1] + 1024], 'u1').reshape(256, 4)
        if psm == 0x13:
            a = pal[_SW[gs.desordena(m[d:d + W * H], 0x13, W, H)]]
        else:
            a = pal[list(range(8)) + list(range(16, 24))][gs.desordena(m[d:d + W * H // 2], 0x14, W, H)]
    a = a.copy(); a[..., 3] = np.minimum(255, a[..., 3].astype(np.int32) * 2)
    return a
