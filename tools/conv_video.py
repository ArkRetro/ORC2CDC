#!/usr/bin/env python3
"""conv_video.py <iso> <dir_salida> [--hz 50|60] [--segundos S] [--solo ts|attr] [--ancho W --alto H] [--frec F]
[--gop G]: las peliculas de MV.PS2 -> MVTS.VQV (pantalla de titulo, \\ts50.pss) y MVATTR.VQV (atraccion, \\attr50.pss)
para comun/video.c.

Origen (todo se lee de tu ISO): MV.PS2 lleva \\ts50.pss, \\attr50.pss, \\ts60.pss y \\attr60.pss (rutas \\mv\\...pss en el
ELF, 0x352bb8; 0x12d480 elige la de 50 o 60 Hz y la de atraccion o la de titulo). PSS = MPEG-PS de Sony: video
MPEG-2 640x480 (25 fps las de 50 Hz, 29,97 las de 60) y audio en paquetes privados 0xBD (4 bytes de subcabecera
"ff a0 00 00"; al principio 'SShd' {tipo 1 = PCM de 16 bits, 48000 Hz, 2 canales, intercalado de 0x200 bytes} y 'SSbd'
{tamano}). Sin pistas de idioma ni subtitulos: el audio es musica y efectos.

La Dreamcast no tiene decodificador de MPEG; el video se guarda como texturas VQ de la PowerVR (la descomprime el propio
chip al dibujar: el SH4 solo copia los indices a la memoria de video). Cada cuadro se escala a ANCHO x ALTO (por
defecto 256x256: textura VQ RGB565, un byte por bloque de 2x2 texeles en orden twiddled, tools/vq.py) y se dibuja
estirado a 640x480. El libro (256 entradas, 2 KB) es comun a un grupo de cuadros (GOP: hasta G cuadros, y uno nuevo en
cada corte de plano): k-means sobre todos los bloques del grupo arrancando del libro del grupo anterior. El audio va a
ADPCM de Yamaha a FREC Hz (32 kHz por defecto), un flujo mono por canal (como los separa snd_adpcm_split: primera
muestra en el nibble bajo), con 1 s de adelanto respecto al video para que el lector del GD vaya siempre por delante
de lo que suena.

Caudal (2026-11-08): la version 1 (512x256, libro por cuadro, 48 kHz, cuadros de 36 KB alineados a sector) pedia
0,9 MB/s al GD; esta (256x256, libro por GOP de 12, 32 kHz) ~446 KB/s, que cabe holgado en la zona interior del GD.

Fichero .VQV version 2 (little endian):
  cabecera (sector 0): 'OVQ2', u32 2, u16 ancho, u16 alto, u32 fps*1000, u32 n_cuadros, u32 frecuencia,
                       u32 bytes de audio por cuadro y canal, u32 cuadros de adelanto L, u32 desplazamiento del audio
                       previo, u32 bytes del audio previo por canal, u32 desplazamiento de los registros (sector),
                       u32 bytes de los registros, u32 tamano del mayor registro
  audio previo:        el audio de los L primeros cuadros: canal izquierdo y luego el derecho
  registros (seguidos, cada uno de tamano multiplo de 32): cabecera de 32 bytes {u32 tamano, u32 banderas (bit 0:
                       lleva libro), u32 bytes de audio por canal, u32 numero de cuadro, 16 de ceros}, [libro de 2048
                       bytes], indices (ancho*alto/4), audio izquierdo y derecho del cuadro k + L, ceros hasta 32"""
import os, struct, subprocess, sys, tempfile
from multiprocessing import Pool
import numpy as np
from scipy.signal import resample_poly

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nativo
from ps2.iso import Iso
from conv_sonido import fichero_paquete
import vq
from modelo import twiddle

ANCHO, ALTO = 256, 256
FREC = 32000
GOP = 12
ADELANTO_S = 1.0
ITER = 6
CORTE = 28.0          # diferencia media (0-255, gris 64x64) entre dos cuadros seguidos que se toma por corte de plano
SILENCIO = 0x80       # ADPCM de Yamaha: +1 y -1 paso (los ceros suben la senal poco a poco)


def demux_pss(d):
    """(datos de video MPEG-PS tal cual, PCM int16 (n, 2), frecuencia)"""
    i = 0; aud = bytearray()
    while i < len(d) - 4:
        if d[i:i + 3] != b'\x00\x00\x01': i += 1; continue
        s = d[i + 3]
        if s == 0xba: i += 14 + (d[i + 13] & 7); continue
        if s == 0xb9: break
        n = struct.unpack_from('>H', d, i + 4)[0]
        if s == 0xbd:
            hl = d[i + 8]; p = d[i + 9 + hl:i + 6 + n]
            assert p[:2] == b'\xff\xa0', 'audio PSS inesperado'
            aud += p[4:]
        i += 6 + n
    assert aud[:4] == b'SShd', 'sin SShd'
    tam, tipo, frec, can, inter = struct.unpack_from('<5I', aud, 4)
    assert tipo == 1 and can == 2, 'audio PSS no es PCM estereo (tipo %d, %d canales)' % (tipo, can)
    o = 8 + tam; assert aud[o:o + 4] == b'SSbd'
    nb = struct.unpack_from('<I', aud, o + 4)[0]; pcm = aud[o + 8:o + 8 + nb]
    pcm = pcm[:len(pcm) // (2 * inter) * 2 * inter]
    b = np.frombuffer(bytes(pcm), '<i2').reshape(-1, 2, inter // 2)      # bloques de 0x200 bytes: izq, der
    return np.stack([b[:, 0, :].ravel(), b[:, 1, :].ravel()], 1), frec


DIF = (1, 3, 5, 7, 9, 11, 13, 15, -1, -3, -5, -7, -9, -11, -13, -15)
ESC = (0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266)


def yamaha(x):
    """como yam_cod de tools/c/musica.c: un nibble por muestra"""
    senal, paso = 0, 0x7f; out = bytearray(len(x))
    for i, v in enumerate(x.tolist()):
        dd = (v - senal) * 8
        dd = int(dd / paso)                       # division de C (hacia cero)
        n = min(abs(dd) // 2, 7)
        if dd < 0: n += 8
        senal += int(paso * DIF[n] / 8)
        senal = max(-32768, min(32767, senal))
        paso = max(0x7f, min(0x6000, paso * ESC[n & 7] >> 8))
        out[i] = n
    return out


def adpcm_mono(x):
    """un canal -> bytes ADPCM (dos muestras por byte, la primera en el nibble bajo)"""
    if len(x) & 1: x = np.concatenate([x, x[-1:]])
    n = np.frombuffer(bytes(yamaha(x)), np.uint8)
    return (n[0::2] | (n[1::2] << 4)).astype(np.uint8).tobytes()


TW = None


def _bloques(a, pes):
    P = np.zeros((ANCHO * ALTO, 3)); P[TW] = a.reshape(-1, 3)
    return P.reshape(-1, 12) * pes


def codifica_trozo(args):
    """(cuadros RGB uint8 (ALTO x ANCHO x 3), grupos [(inicio, fin)] relativos, semilla) -> [(libro o None, indices)]"""
    global TW
    cuadros, grupos, semilla = args
    if TW is None: TW = twiddle(ANCHO, ALTO).ravel()
    pes = np.tile(np.sqrt(vq.PESO[:3]), 4); C = None; r = []
    for g0, g1, corte in grupos:
        Xs = [_bloques(a, pes) for a in cuadros[g0:g1]]
        X = np.concatenate(Xs)
        if C is None or corte:                               # al principio y en cada corte de plano: libro nuevo
            rng = np.random.default_rng(semilla + g0)
            S = X[rng.choice(len(X), min(len(X), 50000), replace=False)]
            U = np.unique(S, axis=0)
            C = vq.kmeans(U, np.ones(len(U)), 256, iters=12, tiempo=10.0, semilla=semilla)
            if len(C) < 256: C = np.concatenate([C, np.repeat(C[:1], 256 - len(C), 0)])
        for _ in range(ITER):
            idx, d = vq._asigna(X, C)
            sw = np.bincount(idx, minlength=256).astype(float)
            for j in range(12): C[:, j] = np.where(sw > 0, np.bincount(idx, X[:, j], 256) / np.maximum(sw, 1), C[:, j])
            vac = np.where(sw == 0)[0]
            if len(vac): C[vac] = X[np.argsort(-d)[:len(vac)]]
        C4 = np.full((256, 4, 4), 255.0); C4[:, :, :3] = (C / pes).reshape(256, 4, 3)
        pal, dec = vq._cuantifica(C4, 1)
        D = dec[:, :, :3].reshape(256, 12) * pes
        for k, Xk in enumerate(Xs):
            idx, _ = vq._asigna(Xk, D)
            r.append((pal.astype('<u2').tobytes() if k == 0 else None, idx.astype(np.uint8).tobytes()))
    return r


def cuadros_rgb(ruta, n_max):
    p = subprocess.Popen([nativo.ffmpeg(), '-v', 'error', '-i', ruta, '-vf', 'scale=%d:%d:flags=lanczos' % (ANCHO, ALTO),
                          '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'], stdout=subprocess.PIPE)
    t = ANCHO * ALTO * 3; r = []
    while len(r) < n_max:
        b = p.stdout.read(t)
        if len(b) < t: break
        r.append(np.frombuffer(b, np.uint8).reshape(ALTO, ANCHO, 3))
    p.stdout.close(); p.wait()
    return r


def grupos(frames):
    """[(inicio, fin, corte)]: hasta GOP cuadros, cortando en los cambios de plano"""
    g = [f[::ALTO // 64, ::ANCHO // 64].astype(float).mean(2) for f in frames]
    cortes = {0} | {i for i in range(1, len(g)) if np.abs(g[i] - g[i - 1]).mean() > CORTE}
    r = []; i = 0
    while i < len(frames):
        j = i + 1
        while j < len(frames) and j - i < GOP and j not in cortes: j += 1
        r.append((i, j, i in cortes)); i = j
    return r


def convierte(iso, nombre_pss, salida, segundos):
    d = fichero_paquete(iso, 'MV.PS2', '\\' + nombre_pss)
    pcm, frec0 = demux_pss(d)
    tmp = tempfile.NamedTemporaryFile(suffix='.pss', dir='/dev/shm' if os.path.isdir('/dev/shm') else None, delete=False)
    tmp.write(d); tmp.close()
    try:
        fps = nativo.video_info(tmp.name)[0]
        n_max = int(segundos * fps) if segundos else 1 << 30
        frames = cuadros_rgb(tmp.name, n_max)
    finally:
        os.unlink(tmp.name)
    n = len(frames)
    por = FREC / fps; assert abs(por - round(por)) < 1e-6 and round(por) % 64 == 0, 'audio por cuadro no valido'
    por = int(round(por)); L = int(round(ADELANTO_S * fps)); pb = por // 2      # pb: bytes por cuadro y canal
    # audio: 48 kHz -> FREC (filtro polifasico), un flujo ADPCM por canal
    from math import gcd
    g = gcd(FREC, frec0)
    x = resample_poly(pcm.astype(np.float64), FREC // g, frec0 // g, axis=0)
    x = np.clip(np.rint(x), -32768, 32767).astype(np.int64)
    x = x[:n * por]
    if len(x) < n * por: x = np.concatenate([x, np.zeros((n * por - len(x), 2), np.int64)])
    canal = [adpcm_mono(x[:, 0]), adpcm_mono(x[:, 1])]
    def audio(k):                       # bytes del cuadro k (izquierdo, derecho); silencio pasado el final
        return [c[k * pb:(k + 1) * pb].ljust(pb, bytes([SILENCIO])) for c in canal]
    # video: grupos por trozos de unos 96 cuadros (un trozo por proceso; el libro arranca del grupo anterior)
    gs = grupos(frames); trozos = []; cur = []
    for gi in gs:
        cur.append(gi)
        if sum(b - a for a, b, _ in cur) >= 96: trozos.append(cur); cur = []
    if cur: trozos.append(cur)
    tareas = [(frames[t[0][0]:t[-1][1]], [(a - t[0][0], b - t[0][0], c) for a, b, c in t], t[0][0]) for t in trozos]
    with Pool(min(4, os.cpu_count() or 1)) as pool:
        vqs = [v for r in pool.map(codifica_trozo, tareas) for v in r]
    assert len(vqs) == n and vqs[0][0] is not None
    regs = []
    for k in range(n):
        libro, idx = vqs[k]
        cuerpo = (libro or b'') + idx + b''.join(audio(k + L))
        tam = (32 + len(cuerpo) + 31) & ~31
        regs.append((struct.pack('<IIII', tam, 1 if libro else 0, pb, k).ljust(32, b'\0') + cuerpo).ljust(tam, b'\0'))
    datos = b''.join(regs)
    previo = b''.join(audio(k)[0] for k in range(L)) + b''.join(audio(k)[1] for k in range(L))
    t_prev = (len(previo) + 2047) & ~2047
    cab = b'OVQ2' + struct.pack('<IHHIIIIIIIIII', 2, ANCHO, ALTO, int(round(fps * 1000)), n, FREC, pb, L, 2048,
                                L * pb, 2048 + t_prev, len(datos), max(len(r) for r in regs))
    with open(salida, 'wb') as f:
        f.write(cab.ljust(2048, b'\0'))
        f.write(previo.ljust(t_prev, b'\0'))
        f.write(datos.ljust((len(datos) + 2047) & ~2047, b'\0'))
    tam = os.path.getsize(salida)
    print('%s: %s, %d cuadros %dx%d a %.3f fps (%.1f s), %d grupos, audio %d Hz ADPCM, %.1f MB, %.0f KB/s' % (
        os.path.basename(salida), nombre_pss, n, ANCHO, ALTO, fps, n / fps, len(gs), FREC, tam / 1e6,
        len(datos) / (n / fps) / 1024))


def main():
    global ANCHO, ALTO, FREC, GOP
    if len(sys.argv) < 3: raise SystemExit(__doc__.split('\n')[0])
    a = sys.argv
    def opc(k, d): return type(d)(a[a.index(k) + 1]) if k in a else d
    iso = Iso(a[1]); sal = a[2]; os.makedirs(sal, exist_ok=True)
    hz = opc('--hz', '50'); seg = opc('--segundos', 0.0); solo = opc('--solo', '')
    ANCHO = opc('--ancho', ANCHO); ALTO = opc('--alto', ALTO); FREC = opc('--frec', FREC); GOP = opc('--gop', GOP)
    for clave, fich in (('ts', 'MVTS.VQV'), ('attr', 'MVATTR.VQV')):
        if solo and solo != clave: continue
        convierte(iso, '%s%s.pss' % (clave, hz), os.path.join(sal, fich), seg)


if __name__ == '__main__':
    main()
