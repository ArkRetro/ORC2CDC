#!/usr/bin/env python3
"""mkgdi.py: imagen .gdi para GDEMU / Flycast a partir de un ELF de KallistiOS.
Uso: mkgdi.py programa.elf carpeta_salida [--nombre "TITULO"] [--datos carpeta]
Disposicion de un GD-ROM: zona de densidad simple con una pista de datos (track01, LBA 0) y una de audio (track02,
silencio, 4 s), y la de alta densidad desde el LBA 45000 (track03): IP.BIN en los 16 primeros sectores y un ISO 9660
con el 1ST_READ.BIN (en GD-ROM sin desordenar: eso solo hace falta en CD-R) y los ficheros de --datos.
Herramientas: sh-elf-objcopy y makeip de KallistiOS; el ISO 9660 y las pistas los escribe tools/gdi.py (Python, sin
genisoimage; es lo que usa el programa de Windows, con el 1ST_READ.BIN y el IP.BIN ya hechos)."""
import argparse, os, shutil, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gdi

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
        sh(os.path.join(CC, 'bin', 'sh-elf-objcopy'), '-R', '.stack', '-O', 'binary', a.elf, os.path.join(t, '1ST_READ.BIN'))
        ip = os.path.join(t, 'IP.BIN')
        sh(os.path.join(KOS, 'utils', 'makeip', 'makeip'), '-g', a.nombre[:128], '-c', 'HOMEBREW', '-n', 'ORC2CDC', '-f', ip)
        # numero de producto propio (no el T-00000 de makeip, comun a todo el homebrew): openMenu de GDEMU busca la
        # caratula del juego por el (GDMenu Card Manager la guarda con ese numero)
        # track03: ISO 9660 que empieza en el LBA 45000, con el IP.BIN en la zona de sistema. En un GD-ROM la BIOS solo
        # carga 1ST_READ.BIN si esta al final del disco (probado con Flycast y la BIOS real: con el fichero en 45 024 lee
        # el directorio y vuelve al menu; cerca de 549 150, arranca): primero los datos, luego un relleno de ceros (0.0)
        # y el ejecutable el ultimo (tools/gdi.py: la disposicion de antes con genisoimage, igual byte a byte salvo
        # las fechas y el identificador de la aplicacion)
        gdi.escribe(a.salida, os.path.join(t, '1ST_READ.BIN'), ip, a.datos)
    finally:
        shutil.rmtree(t)

if __name__ == '__main__':
    main()
