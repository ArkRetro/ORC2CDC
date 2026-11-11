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
  'CAM1' + tablas de la camara de carrera (las que copia 484EE0 del PC / 0x178500 de SLES-53998): 31 vistas x 13
         f32 (ojo, mira, desplazamiento x/y de la proyeccion, balanceo, FOV horizontal en rad, +28, pivote z, u32
         matriz con o sin carroceria), 20 vistas (2..21) x 30 modelos x 9 f32 (ojo, mira, factor del cabeceo,
         tope alto y bajo en grados) y 30 modelos x 9 f32 (ojo de la vista interior y caja del coche). Se localiza
         por el codigo que las copia (memcpy de 0x5460 bytes con la direccion en lui/addiu): las otras dos van
         pegadas a esa (0x64c + 4 bytes antes y justo despues), como en el PC.
  'TIM1' + 4 x 3 x 15 x 5 i16: tablas de tiempo por zona (las que 44C020 del PC / 0x176040 de SLES-53998 copian a
         7D2E98 segun el preset: indices 0, 1, 5 y 6 de la tabla base), en cuadros de 60,2 Hz:
         [preset][fila de variante][nivel de etapa][dificultad]. Se localiza por el codigo que la copia (memcpy de
         0x1C2 bytes con la direccion en lui/addiu), no por su contenido.
  'KTE1' + 15 i16 + 2 bytes de relleno: correccion del tiempo por clave de etapa (k636904 del PC / 0x35a938 de
         SLES-53998) que GetExtendedTime (44DC70 / PS2 0x176730) suma en una partida de un jugador del curso de 5
         niveles. Se localiza por el codigo de esa funcion: 'addiu a0, zero, 0xa' y 'addiu a1, zero, 0x96' (indice
         nivel x 10 + fila x 150 de la tabla de tiempos) y, detras, el lui/addiu de la tabla que se suma con 'lh'.
  'CHA1' + 2 u32 + 3 f32 (al final, detras de PAR2): umbrales y restitucion de los choques entre coches con el
         preset 1 o 3 (cursos de OutRun 2 SP; PC 6AC8A0 / 6AC8B4, en SLES-53998 0x35b9d8 / 0x35b9f0, los que eligen
         0x191d20 / 0x191d70 con [0x44350c]). En la demo los lee or2_carga_choques_sp.
  'PAR2' + 162 f32: la segunda curva de par motor (la del cambio manual: torque_table con event+0x13 = 1). Va justo
         detras de la primera (0x288 bytes) y las dos estan en la tabla de punteros que lee 0x18d760 de SLES-53998
         ([automatico, manual]); se localiza por esa tabla. Va al final del fichero para no mover los demas bloques.
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

CHA = b''
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
        # la tabla del preset 1 / 3 (cursos de OutRun 2 SP): umbrales u[i + 2:i + 4] (PC 6AC8A0) y restitucion
        # f[i + 8:i + 11] (PC 6AC8B4); en SLES-53998 0x191d20 / 0x191d70 eligen +8 / +0x20 del bloque con el preset
        global CHA
        CHA = b'CHA1' + struct.pack('<2I', *u[i + 2:i + 4]) + f[i + 8:i + 11].astype('<f4').tobytes()
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

def busca_tiempos(d, segs):
    """codigo de 44C020 (PS2 0x176040): 'addiu a2, zero, 0x1c2' (tamano del memcpy) con la base en lui a1 / addiu a1
    unas instrucciones antes; devuelve las 4 tablas de 0x1c2 bytes (indices 0, 1, 5, 6)"""
    u = np.frombuffer(d[:len(d) // 4 * 4], '<u4')
    for i in np.where(u == 0x240601c2)[0]:
        lo = hi = None
        for k in range(1, 12):
            w = int(u[i - k])
            if lo is None and (w >> 16) == 0x24a5: lo = w & 0xffff          # addiu a1, a1, lo
            elif lo is not None and (w >> 16) == 0x3c05: hi = w & 0xffff    # lui a1, hi
            if hi is not None: break
        if lo is None or hi is None: continue
        va = (hi << 16) + (lo - 0x10000 if lo & 0x8000 else lo)
        o = va_off(segs, va)
        if o is None: continue
        t = np.frombuffer(d[o:o + 7 * 0x1c2], '<i2').reshape(7, 225)
        if np.all(t[[0, 1, 5, 6]] > 600) and np.all(t[[0, 1, 5, 6]] < 20000):
            return t[[0, 1, 5, 6]].astype('<i2').tobytes(), va
    return None, None

def busca_ajuste_etapa(d, segs):
    """codigo de 44DC70 (PS2 0x176730): 'addiu a0, zero, 0xa' + 'addiu a1, zero, 0x96'; en las 32 instrucciones
    siguientes, un lui rX / addiu rX, rX con la direccion de la tabla de 15 i16 (todas entre -600 y 0, la primera 0)"""
    u = np.frombuffer(d[:len(d) // 4 * 4], '<u4')
    for i in np.where((u[:-1] == 0x2404000a) & (u[1:] == 0x24050096))[0]:
        for k in range(i + 2, i + 34):
            w = int(u[k])
            if (w >> 26) != 0x0f: continue                                  # lui rt, hi
            rt = (w >> 16) & 31
            for j in range(k + 1, k + 6):
                x = int(u[j])
                if (x >> 26) == 0x09 and ((x >> 21) & 31) == rt and ((x >> 16) & 31) == rt:   # addiu rt, rt, lo
                    lo = x & 0xffff
                    va = ((w & 0xffff) << 16) + (lo - 0x10000 if lo & 0x8000 else lo)
                    o = va_off(segs, va)
                    if o is None: continue
                    t = np.frombuffer(d[o:o + 30], '<i2')
                    if t[0] == 0 and np.all((t >= -600) & (t <= 0)) and np.any(t < 0):
                        return t.astype('<i2').tobytes() + b'\0\0', va
    return None, None

def busca_camara(d, segs):
    """codigo de 484EE0 (PS2 0x178500): 'addiu a2, zero, 0x5460' con la fuente en lui a1 / addiu a1 antes; la tabla de
    vistas (0x64c) acaba 4 bytes antes y la de modelos (0x438) va detras. Se comprueba la estructura."""
    u = np.frombuffer(d[:len(d) // 4 * 4], '<u4')
    for i in np.where(u == 0x24065460)[0]:
        lo = hi = None
        for k in range(1, 12):
            w = int(u[i - k])
            if lo is None and (w >> 16) == 0x24a5: lo = w & 0xffff          # addiu a1, a1, lo
            elif lo is not None and (w >> 16) == 0x3c05: hi = w & 0xffff    # lui a1, hi
            if hi is not None: break
        if lo is None or hi is None: continue
        va = (hi << 16) + (lo - 0x10000 if lo & 0x8000 else lo)
        o = va_off(segs, va)
        if o is None or o < 0x650: continue
        vis = np.frombuffer(d[o - 4 - 0x64c:o - 4], '<f4').reshape(31, 13)
        ban = np.frombuffer(d[o - 4 - 0x64c:o - 4], '<u4').reshape(31, 13)[:, 12]
        mod = np.frombuffer(d[o:o + 0x5460], '<f4').reshape(20, 30, 9)
        if not (np.all(ban <= 1) and np.all((vis[:, 9] > 0.3) & (vis[:, 9] < 1.8))): continue
        if not (np.all(mod[0, :, 2] > 2) and np.all(mod[0, :, 7] >= mod[0, :, 8])): continue
        return d[o - 4 - 0x64c:o - 4] + d[o:o + 0x5460 + 0x438], va
    return None, None

def busca_par_manual(d, segs, b):
    """tabla de dos punteros [curva automatico, curva manual] con la primera = la curva de par de main (b - 0x690)"""
    va0 = next((v + (b - 0x690 - o) for v, o, s in segs if o <= b - 0x690 < o + s), None)
    if va0 is None: return None
    u = np.frombuffer(d[:len(d) // 4 * 4], '<u4')
    for i in np.where(u[:-1] == va0)[0]:
        o = va_off(segs, int(u[i + 1]))
        if o is None: continue
        c = np.frombuffer(d[o:o + 162 * 4], '<f4')
        if abs(c[0] - 0.12732396) < 1e-5 and 1.0 < c[1] < 10.0 and np.all(np.isfinite(c)):
            return d[o:o + 162 * 4]
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
    cam, cva = busca_camara(d, segs)
    if cam is None: sys.exit('no encuentro las tablas de la camara')
    tim, tva = busca_tiempos(d, segs)
    if tim is None: sys.exit('no encuentro las tablas de tiempo por zona')
    kte, kva = busca_ajuste_etapa(d, segs)
    if kte is None: sys.exit('no encuentro la correccion de tiempo por etapa')
    par2 = busca_par_manual(d, segs, b)
    if par2 is None: sys.exit('no encuentro la curva de par del cambio manual')
    open(sys.argv[2], 'wb').write(d[b:b + 0x4c * FILAS] + par + fre + b'MAP1' + mapas.tobytes() +
                                  b'CHQ1' + chq + b'POS1' + pos + b'CAM1' + cam + b'TIM1' + tim + b'KTE1' + kte +
                                  b'PAR2' + par2 + CHA)
    print('tablas de tiempo en 0x%x, correccion por etapa en 0x%x, de la camara en 0x%x' % (tva, kva, cva))
    print('tabla de %s en 0x%x: %d filas de %d coches, curvas de par y freno, mapas, choques y %d animaciones' %
          (nombre, b, FILAS, C, struct.unpack('<I', pos[:4])[0]))

if __name__ == '__main__':
    main()
