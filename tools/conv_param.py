#!/usr/bin/env python3
"""conv_param.py tu.iso salida.dat: la tabla de parametros de los coches del ejecutable de PS2 (SLES_*), 19 coches por
fila (0x4c bytes), la misma que la del PC (event+0x2b4). Se localiza por su firma: fila 4 (batalla) entre 2 y 3,1 m,
fila 5 (via) entre 1,2 y 2 m y fila 85 mayor que la batalla (radio de giro). Se guardan 200 filas y despues las
curvas de par motor (162 floats) y de presion de freno (256 floats) que estan justo antes. Detras van tres bloques
con etiqueta para los choques y la eleccion de columna:
  'MAP1' + 4 x 16 u32: columna de la tabla por modelo de coche (las cuatro tablas de 5051D0, justo antes de la tabla;
         la 1 es la normal: coche sin preparar, partida empezada con START).
  'CHQ1' + bloque de choques entre coches (el de 6AC898 del PC): 2 u32 umbrales de velocidad de cierre (km/h del
         modelo), 3 f32 restitucion (cierre bajo, medio, alto), 8 u32 estados de choque del trafico, 3 f32 factor de
         masa (jugador, coche, pesado) y 7x7 f32 reparto de la separacion por clases. Se localiza por su firma.
  'POS1' + u32 n y por estado de choque: f32 duracion, f32 recuperacion y 6 canales (u32 cuenta y cuenta x 4 f32:
         tiempo, valor, tangentes) de la animacion (desplazamiento x, y, z y giro x, y, z del cuerpo); tablas de
         4F6580 / 4F6590 del PC, localizadas por su firma.
Sale de tu disco: no se distribuye."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2 import iso as isomod

C, FILAS = 19, 200

def busca(d):
    f = np.nan_to_num(np.frombuffer(d[:len(d) // 4 * 4], '<f4').astype(np.float64), nan=-1e9, posinf=-1e9, neginf=-1e9)
    for b in range(0, len(f) - 86 * C - C):
        r4 = f[b + 4 * C:b + 5 * C]
        if not np.all((r4 > 2.0) & (r4 < 3.1)): continue
        r5 = f[b + 5 * C:b + 6 * C]
        if not np.all((r5 > 1.2) & (r5 < 2.0)): continue
        if np.all(f[b + 85 * C:b + 86 * C] > r4) and np.all(f[b:b + C] > 500): return b * 4
    return None

def segmentos(d):
    """(vaddr, offset, filesz) de los segmentos LOAD del ELF"""
    phoff, = struct.unpack('<I', d[28:32]); phsz, phn = struct.unpack('<HH', d[42:46])
    r = []
    for i in range(phn):
        t, off, va, _, fs = struct.unpack('<5I', d[phoff + i * phsz:phoff + i * phsz + 20])
        if t == 1: r.append((va, off, fs))
    return r

def va_off(segs, va):
    for v, o, s in segs:
        if v <= va < v + s: return va - v + o
    return None

def busca_choques(d):
    """bloque de 6AC898: u32 a, b, a, b (umbrales normal y alterno), 4+4 f32 restitucion, 8 u32 estados, 4 f32 masa,
    49 f32 reparto"""
    u = np.frombuffer(d[:len(d) // 4 * 4], '<u4'); f = np.frombuffer(d[:len(d) // 4 * 4], '<f4')
    cand = np.where((u[:-4] == u[2:-2]) & (u[1:-3] == u[3:-1]) & (u[:-4] > 10) & (u[:-4] < u[1:-3]) &
                    (u[1:-3] < 1000) & (u[4:] == 0x3f800000))[0]
    for i in cand:
        if i + 4 + 8 + 8 + 4 + 49 > len(u): continue
        e = f[i + 4:i + 7]; st = u[i + 12:i + 20]; m = f[i + 20:i + 23]; t = f[i + 24:i + 73]
        if not (np.all((e > 0.3) & (e <= 1.0)) and np.all(st < 32) and np.all((m > 0) & (m <= 1.0))): continue
        if not np.all((t >= 0) & (t <= 1.0)): continue
        return (struct.pack('<2I', *u[i:i + 2]) + e.astype('<f4').tobytes() + st.astype('<u4').tobytes() +
                m.astype('<f4').tobytes() + t.astype('<f4').tobytes())
    return None

def busca_poses(d, segs):
    """20 filas (puntero a la pose, duracion) seguidas de 20 filas de 12 bytes (recuperacion, ...); la fila 14 vacia"""
    N = 20
    def pose(va):
        o = va_off(segs, va)
        if o is None: return None
        ch = []
        for c in range(6):
            n, p = struct.unpack('<2I', d[o + c * 8:o + c * 8 + 8])
            if not 1 <= n <= 256: return None
            q = va_off(segs, p)
            if q is None: return None
            k = np.frombuffer(d[q:q + n * 16], '<f4')
            if len(k) != n * 4 or not np.all(np.isfinite(k)) or np.any(np.abs(k) > 100): return None
            ch.append(k)
        return ch
    u = np.frombuffer(d[:len(d) // 4 * 4], '<u4'); f = np.nan_to_num(u.view('<f4'), nan=-1.0)
    n = len(u) - 5 * N
    ok = (u[28:28 + n] == 0) & (u[29:29 + n] == 0) & (u[:n] != 0)
    for k in range(N):
        ok &= (f[2 * k + 1:2 * k + 1 + n] >= 0) & (f[2 * k + 1:2 * k + 1 + n] <= 10)
    for i in np.where(ok)[0]:
        o = int(i) * 4
        ptr = struct.unpack('<%dI' % (2 * N), d[o:o + 8 * N])[0::2]
        dur = np.frombuffer(d[o:o + 8 * N], '<f4')[1::2]
        if np.count_nonzero(dur) < 18: continue
        rec = np.frombuffer(d[o + 8 * N:o + 20 * N], '<f4')[0::3]
        if not np.all((rec >= 0) & (rec <= 5)): continue
        poses = []
        for i in range(N):
            if ptr[i] == 0: poses.append(None); continue
            ps = pose(ptr[i])
            if ps is None: break
            poses.append(ps)
        else:
            b = bytearray(struct.pack('<I', N))
            for i in range(N):
                b += struct.pack('<2f', dur[i], rec[i])
                for c in range(6):
                    k = poses[i][c] if poses[i] else np.zeros(4, '<f4')
                    b += struct.pack('<I', len(k) // 4) + k.astype('<f4').tobytes()
            return bytes(b)
    return None

def main():
    iso = isomod.Iso(sys.argv[1])
    nombre = next(k for k in iso.fich if k.startswith('SLES') or k.startswith('SLUS') or k.startswith('SLPM'))
    o, s = iso.fich[nombre]; d = bytes(iso.m[o:o + s])
    b = busca(d)
    if b is None: sys.exit('no encuentro la tabla de parametros en ' + nombre)
    # curvas del cambio automatico justo antes de la tabla: par motor (162 floats: muestras por rad/s, ganancia y
    # 160 muestras) 0x690 bytes antes, y presion de freno (256 floats) 0xa90 bytes antes
    par, fre = d[b - 0x690:b - 0x690 + 162 * 4], d[b - 0xa90:b - 0xa90 + 256 * 4]
    # tablas de columna por modelo (4 x 16 u32) justo antes de la tabla de parametros
    mapas = np.frombuffer(d[b - 0x100:b], '<u4')
    if not np.all(mapas < C): sys.exit('no encuentro las tablas de columna por modelo')
    segs = segmentos(d)
    chq = busca_choques(d)
    if chq is None: sys.exit('no encuentro el bloque de choques entre coches')
    pos = busca_poses(d, segs)
    if pos is None: sys.exit('no encuentro las animaciones de choque')
    open(sys.argv[2], 'wb').write(d[b:b + 0x4c * FILAS] + par + fre + b'MAP1' + mapas.tobytes() +
                                  b'CHQ1' + chq + b'POS1' + pos)
    print('tabla de %s en 0x%x: %d filas de %d coches, curvas de par y freno, mapas, choques y %d animaciones' %
          (nombre, b, FILAS, C, struct.unpack('<I', pos[:4])[0]))

if __name__ == '__main__':
    main()
