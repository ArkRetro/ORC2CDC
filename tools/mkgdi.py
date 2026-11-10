#!/usr/bin/env python3
"""mkgdi.py: imagen .gdi para GDEMU / Flycast a partir de un ELF de KallistiOS.
Uso: mkgdi.py programa.elf carpeta_salida [--nombre "TITULO"] [--datos carpeta]
Disposicion de un GD-ROM: zona de densidad simple con una pista de datos (track01, LBA 0) y una de audio (track02,
silencio, 4 s), y la de alta densidad desde el LBA 45000 (track03): IP.BIN en los 16 primeros sectores y un ISO 9660
con el 1ST_READ.BIN (en GD-ROM sin desordenar: eso solo hace falta en CD-R) y los ficheros de --datos.
Herramientas: sh-elf-objcopy, makeip de KallistiOS y genisoimage."""
import hashlib, argparse, os, shutil, struct, subprocess, sys, tempfile

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
        # ficheros iguales byte a byte (9 COLBKnn.DAT de las misiones, descriptores de uniones repetidas): enlaces duros, y
        # genisoimage (-cache-inodes, lo normal en Unix) los escribe una vez; las entradas del directorio apuntan al
        # mismo LBA
        vistos = {}; ahorro = 0
        for x in sorted(os.listdir(raiz)):
            f = os.path.join(raiz, x)
            if not os.path.isfile(f): continue
            h = hashlib.sha1(open(f, 'rb').read()).digest()
            if h in vistos:
                os.remove(f); os.link(vistos[h], f); ahorro += -(-os.path.getsize(f) // 2048) * 2048
            else: vistos[h] = f
        if ahorro: print('ficheros repetidos: %d KB una sola vez' % (ahorro >> 10))
        sh(os.path.join(CC, 'bin', 'sh-elf-objcopy'), '-R', '.stack', '-O', 'binary', a.elf, os.path.join(raiz, '1ST_READ.BIN'))
        ip = os.path.join(t, 'IP.BIN')
        sh(os.path.join(KOS, 'utils', 'makeip', 'makeip'), '-g', a.nombre[:128], '-c', 'HOMEBREW', '-f', ip)
        # track03: ISO 9660 que empieza en el LBA 45000, con el IP.BIN en la zona de sistema. En un GD-ROM la BIOS solo
        # carga 1ST_READ.BIN si esta al final del disco (probado con Flycast y la BIOS real: con el fichero en 45 024 lee
        # el directorio y vuelve al menu; cerca de 549 150, arranca): primero los datos, luego un relleno de ceros (0.0)
        # y el ejecutable el ultimo. Dos pasadas: la primera mide donde acaba sin relleno
        t3 = os.path.join(a.salida, 'track03.iso')
        orden = os.path.join(t, 'orden.txt')
        def iso(relleno):
            pr = os.path.join(raiz, '0.0')
            if os.path.exists(pr): os.remove(pr)
            if relleno: open(pr, 'wb').truncate(relleno * 2048)
            # mas peso, antes: los datos (4), el relleno (3), las peliculas (2) y el ejecutable (1). El GD-ROM gira a
            # velocidad angular constante: hacia fuera (LBA alto) pasan mas sectores por vuelta y se lee mas deprisa
            # que junto al LBA 45000; las .VQV, que se leen sin parar a ~450 KB/s, van fuera
            with open(orden, 'w') as f:
                for dp, _, fs in os.walk(raiz):
                    for x in fs:
                        r = os.path.relpath(os.path.join(dp, x), raiz)
                        f.write('%s %d\n' % (os.path.join(dp, x), 1 if r == '1ST_READ.BIN' else 3 if r == '0.0' else
                                              2 if r.upper().endswith('.VQV') else 4))
            sh('genisoimage', '-quiet', '-C', '0,45000', '-V', 'OUTRUN_DC', '-G', ip, '-l', '-no-pad', '-sort', orden, '-o', t3, raiz)
            return os.path.getsize(t3) // 2048
        n3 = iso(0)
        fin = 549150 - 32                        # ultimo sector de datos del GD-ROM, con margen
        if 45000 + n3 < fin: n3 = iso(fin - 45000 - n3)
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
