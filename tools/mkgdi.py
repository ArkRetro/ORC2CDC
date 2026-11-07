#!/usr/bin/env python3
"""mkgdi.py: imagen .gdi para GDEMU / Flycast a partir de un ELF de KallistiOS.
Uso: mkgdi.py programa.elf carpeta_salida [--nombre "TITULO"] [--datos carpeta]
Disposicion de un GD-ROM: zona de densidad simple con una pista de datos (track01, LBA 0) y una de audio (track02,
silencio, 4 s), y la de alta densidad desde el LBA 45000 (track03): IP.BIN en los 16 primeros sectores y un ISO 9660
con el 1ST_READ.BIN (en GD-ROM sin desordenar: eso solo hace falta en CD-R) y los ficheros de --datos.
Herramientas: sh-elf-objcopy, makeip de KallistiOS y genisoimage."""
import argparse, os, shutil, subprocess, sys, tempfile

KOS = os.environ.get('KOS_BASE', '/opt/toolchains/dc/kos')
CC = os.environ.get('KOS_CC_BASE', '/opt/toolchains/dc/sh-elf')

def sh(*a):
    subprocess.run(a, check=True, stdout=subprocess.DEVNULL)

def main():
    p = argparse.ArgumentParser()
    p.add_argument('elf'); p.add_argument('salida')
    p.add_argument('--nombre', default='OUTRUN DC PRUEBA')
    p.add_argument('--datos')
    a = p.parse_args()
    os.makedirs(a.salida, exist_ok=True)
    t = tempfile.mkdtemp()
    try:
        raiz = os.path.join(t, 'raiz'); os.makedirs(raiz)
        if a.datos: shutil.copytree(a.datos, raiz, dirs_exist_ok=True)
        sh(os.path.join(CC, 'bin', 'sh-elf-objcopy'), '-R', '.stack', '-O', 'binary', a.elf, os.path.join(raiz, '1ST_READ.BIN'))
        ip = os.path.join(t, 'IP.BIN')
        sh(os.path.join(KOS, 'utils', 'makeip', 'makeip'), '-g', a.nombre[:128], '-c', 'HOMEBREW', '-f', ip)
        # track03: ISO 9660 que empieza en el LBA 45000, con el IP.BIN en la zona de sistema
        t3 = os.path.join(a.salida, 'track03.iso')
        sh('genisoimage', '-quiet', '-C', '0,45000', '-V', 'OUTRUN_DC', '-G', ip, '-l', '-r', '-J', '-o', t3, raiz)
        # track01: un ISO pequeno (300 sectores) y track02: 4 s de silencio
        r1 = os.path.join(t, 'r1'); os.makedirs(r1)
        open(os.path.join(r1, 'LEEME.TXT'), 'w').write('Disco de Dreamcast (GD-ROM). Uso personal.\r\n')
        t1 = os.path.join(a.salida, 'track01.iso')
        sh('genisoimage', '-quiet', '-V', 'OUTRUN_DC', '-o', t1, r1)
        n1 = os.path.getsize(t1) // 2048
        if n1 < 300:
            with open(t1, 'ab') as f: f.write(bytes((300 - n1) * 2048))
            n1 = 300
        with open(os.path.join(a.salida, 'track02.raw'), 'wb') as f: f.write(bytes(302 * 2352))
        lba2 = n1 + 150
        with open(os.path.join(a.salida, 'disc.gdi'), 'w', newline='\r\n') as f:
            f.write('3\n1 0 4 2048 track01.iso 0\n%d %d 0 2352 track02.raw 0\n3 45000 4 2048 track03.iso 0\n'.replace('%d %d', '2 %d') % lba2)
        print('gdi en', a.salida, '(track03: %d KB)' % (os.path.getsize(t3) >> 10))
    finally:
        shutil.rmtree(t)

if __name__ == '__main__':
    main()
