#!/usr/bin/env python3
"""gdi.py: la imagen .gdi del GD-ROM (disc.gdi, track01.iso, track02.raw, track03.iso) solo con Python, sin
genisoimage, makeip ni sh-elf-objcopy: la misma disposicion que escribia tools/mkgdi.py con genisoimage (-C 0,45000
-G IP.BIN -l -no-pad -sort): zona de densidad simple con una pista de datos (track01, LBA 0, un ISO 9660 pequeno con
LEEME.TXT, 300 sectores) y una de audio (track02, 4 s de silencio), y la de alta densidad desde el LBA 45000
(track03): IP.BIN en los 16 primeros sectores y un ISO 9660 con los ficheros de --datos, un relleno de ceros (0.0),
las peliculas (.VQV) y el 1ST_READ.BIN el ultimo, al final del disco (ver mkgdi.py: la BIOS solo lo carga si esta
cerca del final; el GD-ROM lee mas deprisa hacia fuera).
Uso: gdi.py salida/ --bin 1ST_READ.BIN --ip IP.BIN [--datos carpeta]
El 1ST_READ.BIN es el ELF de la demo en binario (sh-elf-objcopy -R .stack -O binary demo.elf) y el IP.BIN el de makeip
de KallistiOS (mkgdi.py los hace con las herramientas de KallistiOS y llama a escribe).
Igual byte a byte que genisoimage salvo el identificador de la aplicacion del descriptor de volumen y las fechas."""
import argparse, hashlib, os, struct, time

SECTOR = 2048
LBA_ALTA = 45000                     # inicio de la zona de alta densidad
FIN_GD = 549150 - 32                 # ultimo sector de datos del GD-ROM, con margen (mkgdi.py)
APLICACION = b'ORC2CDC TOOLS/GDI.PY'

def _ambos16(x): return struct.pack('<H', x) + struct.pack('>H', x)
def _ambos32(x): return struct.pack('<I', x) + struct.pack('>I', x)

def _fecha_dir(t):
    """fecha de 7 bytes de un registro de directorio (hora local y su diferencia con UTC en cuartos de hora)"""
    lt = time.localtime(t)
    return struct.pack('<6Bb', lt.tm_year - 1900, lt.tm_mon, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec,
                       (lt.tm_gmtoff or 0) // 900)

def _fecha_vd(t):
    """fecha de 17 bytes del descriptor de volumen"""
    lt = time.localtime(t)
    return time.strftime('%Y%m%d%H%M%S', lt).encode() + b'00' + struct.pack('b', (lt.tm_gmtoff or 0) // 900)

def _registro(nombre, extension, tam, fecha, dir_=False):
    n = len(nombre); l = 33 + n + (1 - n % 2)
    r = struct.pack('<BB', l, 0) + _ambos32(extension) + _ambos32(tam) + fecha + bytes([2 if dir_ else 0, 0, 0]) + \
        _ambos16(1) + bytes([n]) + nombre
    return r.ljust(l, b'\0')

def _nombre_iso(x):
    x = x.upper()
    if '.' not in x: x += '.'
    return x + ';1'

def _clave(nombre_iso):
    """orden de los registros del directorio (el de genisoimage: nombre, luego extension)"""
    b, _, e = nombre_iso.partition(';')[0].partition('.')
    return (b, e)

class Fichero:
    def __init__(self, nombre, ruta=None, tam=None, peso=0, ceros=False, fecha=None):
        self.nombre = nombre; self.ruta = ruta; self.peso = peso; self.ceros = ceros
        self.tam = os.path.getsize(ruta) if tam is None else tam
        self.fecha = fecha if fecha is not None else (os.path.getmtime(ruta) if ruta else time.time())
        self.igual_que = None                                     # otro Fichero con el mismo contenido

def iso9660(salida, ficheros, volumen, base=0, ip=None, relleno_final=True, fecha=None, progreso=None):
    """escribe un ISO 9660 de un solo directorio (la raiz) como genisoimage -l [-C 0,base] [-G ip] [-sort]: los
    ficheros por peso (mas peso, antes) y en el orden del directorio; los que tienen igual_que comparten sus sectores.
    relleno_final: los 150 sectores de ceros del final (genisoimage sin -no-pad). Devuelve los sectores escritos"""
    fecha = time.time() if fecha is None else fecha
    fd = _fecha_dir(fecha)
    fs = sorted(ficheros, key=lambda f: _clave(_nombre_iso(f.nombre)))
    # el directorio raiz: '.', '..' y un registro por fichero, sin partir ninguno entre dos sectores
    tam_dir = 0
    for l in [34, 34] + [len(_registro(_nombre_iso(f.nombre).encode(), 0, 0, fd)) for f in fs]:
        if tam_dir % SECTOR + l > SECTOR: tam_dir = (tam_dir // SECTOR + 1) * SECTOR
        tam_dir += l
    sec_dir = -(-tam_dir // SECTOR)
    # 16 sectores del sistema (IP.BIN), descriptor primario, terminador, uno libre, tablas de rutas L y M (con uno
    # libre detras de cada una) y la raiz
    s_pvd, s_tl, s_tm, s_raiz = 16, 19, 21, 23
    sig = s_raiz + sec_dir
    orden = sorted(fs, key=lambda f: -f.peso)                     # (sorted es estable: el orden del directorio)
    for f in orden:
        if f.igual_que is not None: continue
        f.ext = base + sig; sig += -(-f.tam // SECTOR)
    for f in fs:
        if f.igual_que is not None: f.ext = f.igual_que.ext
    total = sig + (150 if relleno_final else 0)
    raiz = base + s_raiz
    # descriptores
    pvd = bytearray(SECTOR)
    pvd[0:8] = b'\x01CD001\x01\x00'
    pvd[8:40] = b'LINUX'.ljust(32); pvd[40:72] = volumen.encode().ljust(32)
    pvd[80:88] = _ambos32(total); pvd[120:124] = _ambos16(1); pvd[124:128] = _ambos16(1); pvd[128:132] = _ambos16(SECTOR)
    pvd[132:140] = _ambos32(10)
    pvd[140:144] = struct.pack('<I', base + s_tl); pvd[148:152] = struct.pack('>I', base + s_tm)
    pvd[156:190] = _registro(b'\0', raiz, sec_dir * SECTOR, fd, True)
    pvd[190:702] = b' ' * 512; pvd[574:574 + len(APLICACION)] = APLICACION
    pvd[702:813] = b' ' * 111
    fv = _fecha_vd(fecha)
    pvd[813:830] = fv; pvd[830:847] = fv; pvd[847:864] = b'0' * 16 + b'\0'; pvd[864:881] = fv
    pvd[881] = 1; pvd[883:1395] = b' ' * 512
    term = bytearray(SECTOR); term[0:7] = b'\xffCD001\x01'
    tl = struct.pack('<BBIH', 1, 0, raiz, 1) + b'\0\0'
    tm = struct.pack('>BBIH', 1, 0, raiz, 1) + b'\0\0'
    d = bytearray()
    for r in [_registro(b'\0', raiz, sec_dir * SECTOR, fd, True), _registro(b'\1', raiz, sec_dir * SECTOR, fd, True)] + \
             [_registro(_nombre_iso(f.nombre).encode(), f.ext, f.tam, _fecha_dir(f.fecha)) for f in fs]:
        if len(d) % SECTOR + len(r) > SECTOR: d += bytes(SECTOR - len(d) % SECTOR)
        d += r
    d = bytes(d).ljust(sec_dir * SECTOR, b'\0')
    hecho = [0]
    def pon(fh, b):
        fh.write(b); hecho[0] += len(b)
        if progreso: progreso(hecho[0], total * SECTOR)
    with open(salida, 'wb') as fh:
        sis = open(ip, 'rb').read() if ip else b''
        assert len(sis) <= 16 * SECTOR, 'IP.BIN de mas de 32 KB'
        pon(fh, sis.ljust(16 * SECTOR, b'\0'))
        pon(fh, bytes(pvd)); pon(fh, bytes(term)); pon(fh, bytes(SECTOR))
        pon(fh, tl.ljust(2 * SECTOR, b'\0')); pon(fh, tm.ljust(2 * SECTOR, b'\0')); pon(fh, d)
        for f in orden:
            if f.igual_que is not None: continue
            assert fh.tell() == (f.ext - base) * SECTOR
            n = -(-f.tam // SECTOR) * SECTOR
            if f.ceros:
                while n: k = min(n, 1 << 22); pon(fh, bytes(k)); n -= k
            else:
                with open(f.ruta, 'rb') as g:
                    q = 0
                    while True:
                        b = g.read(1 << 22)
                        if not b: break
                        pon(fh, b); q += len(b)
                    assert q == f.tam, f.ruta + ' ha cambiado de tamano'
                if n > q: pon(fh, bytes(n - q))
        if relleno_final: pon(fh, bytes(150 * SECTOR))
    return total

def ficheros_datos(datos):
    """los ficheros de la carpeta de datos (con su peso de mkgdi.py) y los repetidos byte a byte enlazados al primero
    (como los enlaces duros de mkgdi.py, que genisoimage escribe una vez): (lista, bytes ahorrados)"""
    r = []; vistos = {}; ahorro = 0
    for x in sorted(os.listdir(datos)) if datos else []:
        p = os.path.join(datos, x)
        if not os.path.isfile(p): continue
        f = Fichero(x, p, peso=2 if x.upper().endswith('.VQV') else 4)
        h = hashlib.sha1()
        with open(p, 'rb') as g:
            for b in iter(lambda: g.read(1 << 22), b''): h.update(b)
        h = h.digest()
        if h in vistos: f.igual_que = vistos[h]; ahorro += -(-f.tam // SECTOR) * SECTOR
        else: vistos[h] = f
        r.append(f)
    # el que da los sectores, el primero en el orden del directorio (el de genisoimage)
    grupos = {}
    for f in r: grupos.setdefault(id(f.igual_que or f), []).append(f)
    for g in grupos.values():
        g.sort(key=lambda f: _clave(_nombre_iso(f.nombre)))
        for f in g: f.igual_que = None if f is g[0] else g[0]
    return r, ahorro

def escribe(salida, primero, ip, datos=None, volumen='OUTRUN_DC', fecha=None, progreso=None, informe=print):
    """el GD-ROM en salida/: primero es el 1ST_READ.BIN, ip el IP.BIN"""
    os.makedirs(salida, exist_ok=True)
    fecha = time.time() if fecha is None else fecha
    fs, ahorro = ficheros_datos(datos)
    if ahorro: informe('ficheros repetidos: %d KB una sola vez' % (ahorro >> 10))
    nombres = {f.nombre.upper() for f in fs}
    for x in ('1ST_READ.BIN', '0.0'):
        if x in nombres: raise SystemExit('la carpeta de datos no puede tener %s' % x)
    fs.append(Fichero('1ST_READ.BIN', primero, peso=1, fecha=fecha))
    t3 = os.path.join(salida, 'track03.iso')
    # sin relleno, para medir; luego con el relleno de ceros que lleva el ejecutable al final del disco
    n3 = _mide(fs)
    if LBA_ALTA + n3 < FIN_GD:
        fs.append(Fichero('0.0', tam=(FIN_GD - LBA_ALTA - n3) * SECTOR, peso=3, ceros=True, fecha=fecha))
    iso9660(t3, fs, volumen, base=LBA_ALTA, ip=ip, relleno_final=False, fecha=fecha, progreso=progreso)
    # track01: un ISO pequeno (300 sectores) y track02: 4 s de silencio
    t1 = os.path.join(salida, 'track01.iso')
    leeme = os.path.join(salida, 'LEEME.TXT.tmp')
    with open(leeme, 'wb') as f: f.write(b'Disco de Dreamcast (GD-ROM). Uso personal.\r\n')
    try:
        n1 = iso9660(t1, [Fichero('LEEME.TXT', leeme, fecha=fecha)], volumen, fecha=fecha)
    finally:
        os.remove(leeme)
    if n1 < 300:
        with open(t1, 'ab') as f: f.write(bytes((300 - n1) * SECTOR))
        n1 = 300
    with open(os.path.join(salida, 'track02.raw'), 'wb') as f: f.write(bytes(302 * 2352))
    with open(os.path.join(salida, 'disc.gdi'), 'w', newline='\r\n') as f:
        f.write('3\n1 0 4 2048 track01.iso 0\n2 %d 0 2352 track02.raw 0\n3 45000 4 2048 track03.iso 0\n' % (n1 + 150))
    informe('gdi en %s (track03: %d KB)' % (salida, os.path.getsize(t3) >> 10))

def _mide(fs):
    """sectores del track03 sin escribirlo (lo que mide iso9660)"""
    tam_dir = 0
    for l in [34, 34] + [len(_registro(_nombre_iso(f.nombre).encode(), 0, 0, bytes(7))) for f in
                         sorted(fs, key=lambda f: _clave(_nombre_iso(f.nombre)))]:
        if tam_dir % SECTOR + l > SECTOR: tam_dir = (tam_dir // SECTOR + 1) * SECTOR
        tam_dir += l
    return 23 + -(-tam_dir // SECTOR) + sum(-(-f.tam // SECTOR) for f in fs if f.igual_que is None)

def main():
    p = argparse.ArgumentParser(description='GD-ROM (.gdi) sin herramientas externas')
    p.add_argument('salida'); p.add_argument('--bin', required=True, help='1ST_READ.BIN')
    p.add_argument('--ip', required=True, help='IP.BIN'); p.add_argument('--datos')
    p.add_argument('--fecha', type=float, help='segundos desde 1970 (fechas del ISO; por defecto ahora)')
    a = p.parse_args()
    escribe(a.salida, a.bin, a.ip, a.datos, fecha=a.fecha)

if __name__ == '__main__':
    main()
