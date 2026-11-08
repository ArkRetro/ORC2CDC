#!/usr/bin/env python3
"""conv_video.py <iso> <dir_salida> [--hz 50|60] [--segundos S] [--solo ts|attr]: las peliculas de MV.PS2 -> MVTS.VQV
(pantalla de titulo, \\ts50.pss) y MVATTR.VQV (atraccion, \\attr50.pss) para comun/video.c.

Origen (todo se lee de tu ISO): MV.PS2 lleva \\ts50.pss, \\attr50.pss, \\ts60.pss y \\attr60.pss (rutas \\mv\\...pss en el
ELF, 0x352bb8; 0x12d480 elige la de 50 o 60 Hz y la de atraccion o la de titulo). PSS = MPEG-PS de Sony: video
MPEG-2 640x480 (25 fps las de 50 Hz, 29,97 las de 60) y audio en paquetes privados 0xBD (4 bytes de subcabecera
"ff a0 00 00"; al principio 'SShd' {tipo 1 = PCM de 16 bits, 48000 Hz, 2 canales, intercalado de 0x200 bytes} y 'SSbd'
{tamano}). Sin pistas de idioma ni subtitulos: el audio es musica y efectos.
La Dreamcast no tiene decodificador de MPEG; el video se guarda como texturas VQ de la PowerVR (la descomprime el propio
chip al dibujar: el SH4 solo copia 34 KB por cuadro a la memoria de video). Cada cuadro se escala a 512x256 (una
textura VQ rectangular RGB565: libro de 256 entradas de 2x2 texeles + un byte por bloque de 2x2, en orden twiddled,
tools/vq.py) y se dibuja estirado a 640x480. El libro se calcula por cuadro con k-means arrancando del libro del cuadro
anterior. El audio va a ADPCM de Yamaha estereo a 48 kHz intercalado por nibbles (como la musica, tools/c/musica.c), con
1 s de adelanto respecto al video para que el lector del GD vaya siempre por delante de lo que suena.

Fichero .VQV (little endian, todo alineado a sectores de 2048 bytes):
  cabecera (sector 0): 'OVQ1', u32 1, u16 ancho, u16 alto (textura), u32 fps*1000, u32 n_cuadros, u32 frecuencia,
                       u32 bytes de audio por cuadro, u32 cuadros de adelanto L, u32 tamano de un cuadro (bytes),
                       u32 desplazamiento del audio previo, u32 bytes del audio previo, u32 desplazamiento del cuadro 0
  audio previo:        el audio de los L primeros cuadros
  cuadro k:            VQ (2048 + ancho*alto/4 bytes) y el audio del cuadro k + L (ceros al final)"""
import os, struct, subprocess, sys, tempfile
from multiprocessing import Pool
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.iso import Iso
from conv_sonido import fichero_paquete
import vq
from modelo import twiddle

ANCHO, ALTO = 512, 256
ADELANTO_S = 1.0
ITER = 6


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
    """como yam_cod de tools/c/musica.c"""
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


def audio_adpcm(pcm):
    l = yamaha(pcm[:, 0]); r = yamaha(pcm[:, 1])
    return bytes(a | (b << 4) for a, b in zip(r, l))   # nibble bajo: derecho; alto: izquierdo


TW = None


def codifica_trozo(args):
    """[cuadros RGB uint8 (ALTO x ANCHO x 3)] -> [bytes VQ]; k-means con arranque en el libro anterior"""
    global TW
    cuadros, semilla = args
    if TW is None: TW = twiddle(ANCHO, ALTO).ravel()
    pes = np.tile(np.sqrt(vq.PESO[:3]), 4); C = None; r = []
    for a in cuadros:
        P = np.zeros((ANCHO * ALTO, 3)); P[TW] = a.reshape(-1, 3)
        X = P.reshape(-1, 12) * pes
        if C is None:
            U = np.unique(X, axis=0)
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
        idx, _ = vq._asigna(X, dec[:, :, :3].reshape(256, 12) * pes)
        r.append(pal.astype('<u2').tobytes() + idx.astype(np.uint8).tobytes())
    return r


def cuadros_rgb(ruta, n_max):
    p = subprocess.Popen(['ffmpeg', '-v', 'error', '-i', ruta, '-vf', 'scale=%d:%d:flags=lanczos' % (ANCHO, ALTO),
                          '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'], stdout=subprocess.PIPE)
    t = ANCHO * ALTO * 3; r = []
    while len(r) < n_max:
        b = p.stdout.read(t)
        if len(b) < t: break
        r.append(np.frombuffer(b, np.uint8).reshape(ALTO, ANCHO, 3))
    p.stdout.close(); p.wait()
    return r


def convierte(iso, nombre_pss, salida, segundos):
    d = fichero_paquete(iso, 'MV.PS2', '\\' + nombre_pss)
    pcm, frec = demux_pss(d)
    tmp = tempfile.NamedTemporaryFile(suffix='.pss', dir='/dev/shm' if os.path.isdir('/dev/shm') else None, delete=False)
    tmp.write(d); tmp.close()
    try:
        r = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries', 'stream=r_frame_rate',
                            '-of', 'default=nw=1:nk=1', tmp.name], capture_output=True, text=True).stdout.split()[0]
        fps = float(r.split('/')[0]) / float(r.split('/')[1])
        n_max = int(segundos * fps) if segundos else 1 << 30
        frames = cuadros_rgb(tmp.name, n_max)
    finally:
        os.unlink(tmp.name)
    n = len(frames)
    por = frec / fps; assert abs(por - round(por)) < 1e-6, 'audio por cuadro no entero'
    por = int(round(por)); L = int(round(ADELANTO_S * fps))
    pcm = pcm[:n * por]
    pcm = np.concatenate([pcm, np.zeros((n * por - len(pcm), 2), pcm.dtype)]) if len(pcm) < n * por else pcm
    adp = audio_adpcm(pcm.astype(np.int64))                     # 1 byte por muestra estereo
    trozos = [(frames[i:i + 24], i) for i in range(0, n, 24)]
    with Pool(min(4, os.cpu_count() or 1)) as pool:
        vqs = [v for r in pool.map(codifica_trozo, trozos) for v in r]
    t_vq = 2048 + ANCHO * ALTO // 4
    t_cuadro = (t_vq + por + 2047) & ~2047
    previo = adp[:L * por]; t_prev = (len(previo) + 2047) & ~2047
    cab = b'OVQ1' + struct.pack('<IHHIIIIIIIII', 1, ANCHO, ALTO, int(round(fps * 1000)), n, frec, por, L, t_cuadro,
                                2048, len(previo), 2048 + t_prev)
    with open(salida, 'wb') as f:
        f.write(cab.ljust(2048, b'\0'))
        f.write(previo.ljust(t_prev, b'\0'))
        for k in range(n):
            a = adp[(k + L) * por:(k + L + 1) * por].ljust(por, b'\0')
            f.write((vqs[k] + a).ljust(t_cuadro, b'\0'))
    tam = os.path.getsize(salida)
    print('%s: %s, %d cuadros %dx%d a %.3f fps (%.1f s), audio %d Hz ADPCM, %.1f MB, %.0f KB/s' % (
        os.path.basename(salida), nombre_pss, n, ANCHO, ALTO, fps, n / fps, frec, tam / 1e6, t_cuadro * fps / 1024))


def main():
    if len(sys.argv) < 3: raise SystemExit(__doc__.split('\n')[0])
    iso = Iso(sys.argv[1]); sal = sys.argv[2]; os.makedirs(sal, exist_ok=True)
    hz = sys.argv[sys.argv.index('--hz') + 1] if '--hz' in sys.argv else '50'
    seg = float(sys.argv[sys.argv.index('--segundos') + 1]) if '--segundos' in sys.argv else 0
    solo = sys.argv[sys.argv.index('--solo') + 1] if '--solo' in sys.argv else None
    for clave, fich in (('ts', 'MVTS.VQV'), ('attr', 'MVATTR.VQV')):
        if solo and solo != clave: continue
        convierte(iso, '%s%s.pss' % (clave, hz), os.path.join(sal, fich), seg)


if __name__ == '__main__':
    main()
