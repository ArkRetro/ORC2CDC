"""outrun_taxi.py: el programa de Windows de OutRun Mod (la version con el taxi de Crazy Taxi; OutRunTaxi.bat lo lanza
con el Python del paquete; tambien funciona en Linux con python3).

El taxi, Axel y sus canciones no estan en la ISO de PS2 (salen de Crazy Taxi de Dreamcast): este programa no los hace,
los toma de TU GD-ROM de OutRun para Dreamcast que ya los lleva (el disc.gdi con sus pistas). Hace un GD-ROM nuevo con
los ficheros de ese disco y el programa de la Dreamcast de este paquete (programa/dc: 1ST_READ.BIN de la rama rev-1.0,
con los arreglos, e IP.BIN "OUTRUN MOD REV 1.0"). Si le das tambien tu ISO de PS2 (SLES-53998), rehace EFECTOS.DAT con
la textura del agua de las ruedas (tools/conv_efectos.py, segundos); sin ella se queda el EFECTOS.DAT del disco
(todo igual, sin el agua en el asfalto mojado). Despues pasa tools/libros.py (libros VQ reducidos, Rev 2.2 / Mod 1.8:
mas VRAM libre) con el pvrtex.exe del paquete, y rehace REFLEJO.DAT (el reflejo del entorno, Rev 2.4 / Mod 2.0).
Uso: outrun_taxi.py [disc.gdi] [carpeta] [--iso tu.iso | --sin-iso] [--bin 1ST_READ.BIN --ip IP.BIN] [--sin-dialogos]
Sin el disc.gdi o la carpeta, los pide con los dialogos de Windows (PowerShell) o, si no hay, por teclado."""
import os, shutil, struct, sys, time

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import outrun2dc as o2                                          # dialogos, informe y la comprobacion de la ISO

SUB = 'OutRunMod_Taxi'
NO_COPIA = {'1ST_READ.BIN', '0.0'}                              # el programa (va el del paquete) y el relleno del disco

def elige_gdi():
    r = o2.powershell(o2.DIALOGO + """$d = New-Object System.Windows.Forms.OpenFileDialog
$d.Title = 'Elige el disc.gdi de tu GD-ROM de OutRun para Dreamcast con el taxi'
$d.Filter = 'GD-ROM (*.gdi)|*.gdi|Todos los ficheros (*.*)|*.*'
if ($d.ShowDialog($f) -eq 'OK') { Write-Output $d.FileName }""")
    return r or o2.pregunta('Ruta del disc.gdi de tu OutRun de Dreamcast con el taxi: ')

def elige_iso_opcional():
    r = o2.powershell(o2.DIALOGO + """$d = New-Object System.Windows.Forms.OpenFileDialog
$d.Title = 'OPCIONAL: tu ISO de PS2 (SLES-53998) para el agua de las ruedas. Cancelar = sin agua'
$d.Filter = 'Imagen ISO (*.iso)|*.iso|Todos los ficheros (*.*)|*.*'
if ($d.ShowDialog($f) -eq 'OK') { Write-Output $d.FileName }""")
    if r or o2.WIN: return r
    try: r = input('Ruta de tu ISO de PS2 para el agua (Intro: sin agua): ').strip().strip('"')
    except EOFError: r = ''
    return r or None

def elige_carpeta():
    r = o2.powershell(o2.DIALOGO + """$d = New-Object System.Windows.Forms.FolderBrowserDialog
$d.Description = 'Carpeta donde se crea el GD-ROM nuevo (unos 2 GB libres). Dentro se crea OutRunMod_Taxi.'
$d.ShowNewFolderButton = $true
if ($d.ShowDialog($f) -eq 'OK') { Write-Output $d.SelectedPath }""")
    return r or o2.pregunta('Carpeta de salida (se crea dentro %s): ' % SUB)

def pista_datos(gdi):
    """(fichero, LBA de inicio, tamano de sector) de la pista de datos del GD (la ultima pista de datos, la 3)"""
    lin = [l.split() for l in open(gdi, encoding='latin1').read().splitlines() if l.strip()]
    pistas = []
    for l in lin[1:]:
        if len(l) < 5: continue
        nombre = ' '.join(l[4:-1]).strip('"') if len(l) > 5 else l[4]
        pistas.append((int(l[0]), int(l[1]), int(l[2]), int(l[3]), nombre))
    datos = [p for p in pistas if p[2] == 4 and p[1] >= 45000]
    if not datos: raise SystemExit('%s: no tiene la pista de datos del GD-ROM (la de la LBA 45000)' % gdi)
    _, lba, _, tam, nombre = datos[0]
    f = os.path.join(os.path.dirname(os.path.abspath(gdi)), nombre)
    if not os.path.isfile(f): raise SystemExit('falta la pista %s' % f)
    if tam not in (2048, 2352): raise SystemExit('%s: sectores de %d bytes (se esperan 2048 o 2352)' % (nombre, tam))
    return f, lba, tam

def extrae(pista, base, tam, salida, informe):
    """los ficheros del ISO 9660 de la pista (sin carpetas: el disco de OutRun los tiene todos en la raiz)"""
    f = open(pista, 'rb'); off = 16 if tam == 2352 else 0
    def sec(lba, n=1):
        out = bytearray()
        for k in range(n):
            f.seek((lba + k - base) * tam + off); out += f.read(2048)
        return bytes(out)
    pvd = sec(base + 16)
    if pvd[1:6] != b'CD001': raise SystemExit('%s: no es un ISO 9660 en la LBA %d' % (pista, base))
    raiz = pvd[156:190]; e, = struct.unpack('<I', raiz[2:6]); s, = struct.unpack('<I', raiz[10:14])
    d = sec(e, (s + 2047) // 2048)[:s]; i = 0; n = 0; total = 0
    while i < len(d):
        l = d[i]
        if l == 0: i = (i // 2048 + 1) * 2048; continue
        r = d[i:i + l]; i += l
        nom = r[33:33 + r[32]]
        if nom in (b'\0', b'\1') or r[25] & 2: continue
        nom = nom.decode('latin1').split(';')[0]
        if nom.upper() in NO_COPIA: continue
        ex, = struct.unpack('<I', r[2:6]); ta, = struct.unpack('<I', r[10:14])
        with open(os.path.join(salida, nom), 'wb') as o:
            k = ex; queda = ta
            while queda:
                if tam == 2048:
                    f.seek((k - base) * 2048); b = f.read(min(queda, 1 << 22)); k += (len(b) + 2047) // 2048
                else:
                    b = sec(k)[:min(queda, 2048)]; k += 1
                if not b: raise SystemExit('%s: la pista se acaba antes que %s' % (pista, nom))
                o.write(b); queda -= len(b)
        n += 1; total += ta
    f.close()
    informe('  %d ficheros (%.0f MB)' % (n, total / 1048576))
    return n

def main():
    a = sys.argv[1:]; op = {}; pos = []; k = 0
    while k < len(a):
        if a[k] in ('--iso', '--bin', '--ip'): op[a[k][2:]] = a[k + 1]; k += 2
        elif a[k] == '--sin-iso': op['sin_iso'] = True; k += 1
        elif a[k] == '--sin-dialogos': op['sin'] = True; k += 1
        else: pos.append(a[k]); k += 1
    informe = o2.informe
    informe('OutRun Mod Rev 1.0 (con el taxi) para Dreamcast: GD-ROM nuevo desde tu GD-ROM con el taxi (orc2cdc)')
    informe('')
    g = pos[0] if pos else (o2.pregunta('Ruta de tu disc.gdi con el taxi: ') if op.get('sin') else elige_gdi())
    g = os.path.abspath(g)
    if not os.path.isfile(g): raise SystemExit('no esta el GD-ROM: %s' % g)
    pista, base, tam = pista_datos(g)
    informe('GD-ROM: %s (%s, sectores de %d)' % (g, os.path.basename(pista), tam))
    iso = None
    if not op.get('sin_iso'):
        iso = op.get('iso') or (None if op.get('sin') else elige_iso_opcional())
    if iso:
        iso = os.path.abspath(iso)
        if not os.path.isfile(iso): raise SystemExit('no esta la ISO: %s' % iso)
        o2.comprueba_iso(iso); informe('ISO de PS2 (para el agua): %s' % iso)
    else:
        informe('Sin ISO de PS2: sin el agua de las ruedas (lo demas, igual)')
    car = pos[1] if len(pos) > 1 else (o2.pregunta('Carpeta de salida: ') if op.get('sin') else elige_carpeta())
    car = os.path.abspath(car)
    if os.path.basename(car) != SUB: car = os.path.join(car, SUB)
    if os.path.abspath(os.path.dirname(g)) == car: raise SystemExit('la carpeta de salida no puede ser la del GD-ROM de entrada')
    W = os.path.join(car, 'trabajo'); D = os.path.join(W, 'datos')
    if os.path.isdir(D): shutil.rmtree(D)
    os.makedirs(D); os.makedirs(os.path.join(W, 'registro'), exist_ok=True)
    o2._log[0] = os.path.join(W, 'registro', 'programa.txt')
    informe('Carpeta: %s' % car)
    if shutil.disk_usage(car).free / o2.GB < 2.2: informe('AVISO: hacen falta unos 2,2 GB libres en %s' % car)
    primero = op.get('bin') or os.path.join(AQUI, 'dc', '1ST_READ.BIN')
    ip = op.get('ip') or os.path.join(AQUI, 'dc', 'IP.BIN')
    for f in (primero, ip):
        if not os.path.isfile(f): raise SystemExit('falta %s (paquete incompleto)' % f)
    t0 = time.time()
    informe('== ficheros de tu GD-ROM')
    extrae(pista, base, tam, D, informe)
    for x in ('CURSO.DAT', 'COCHE07.DAT', 'EFECTOS.DAT'):
        if not os.path.isfile(os.path.join(D, x)): raise SystemExit('el GD-ROM no es de OutRun para Dreamcast (falta %s)' % x)
    if iso:
        informe('== EFECTOS.DAT desde la ISO (con la textura del agua)')
        import subprocess
        os.environ.setdefault('OR2_CACHE', os.path.join(W, 'cache'))
        nuevo = os.path.join(W, 'EFECTOS.DAT')
        r = subprocess.run([sys.executable, os.path.join(o2.TOOLS, 'conv_efectos.py'), iso, nuevo],
                           capture_output=True, text=True)
        if r.returncode or not os.path.isfile(nuevo):
            informe('AVISO: no se pudo rehacer EFECTOS.DAT (se queda el del disco, sin agua):\n' + (r.stderr or r.stdout)[-2000:])
        else:
            shutil.move(nuevo, os.path.join(D, 'EFECTOS.DAT')); informe('  hecho')
    informe('== libros VQ reducidos de las texturas pequenas (tools/libros.py con pvrtex; si ya lo estan, sin cambios)')
    if os.path.isdir(os.path.join(AQUI, 'nativo')): os.environ.setdefault('OR2_NATIVO', os.path.join(AQUI, 'nativo'))
    import subprocess
    for args in (['--datos', D, os.path.join(W, 'libros')],
                 [os.path.join(D, p) for p in ('TRAF*.DAT', 'CHR[0-9]*.DAT', 'BAND*.DAT', 'BO[0-9]*.DAT')]):
        r = subprocess.run([sys.executable, os.path.join(o2.TOOLS, 'libros.py')] + args, capture_output=True, text=True)
        if r.returncode: raise SystemExit('fallo tools/libros.py:\n' + (r.stderr or r.stdout)[-2000:])
        for l in r.stdout.splitlines()[-4:]: informe('  ' + l)
    informe('== reflejo del entorno en la carroceria (tools/conv_reflejo.py: unos minutos)')
    r = subprocess.run([sys.executable, os.path.join(o2.TOOLS, 'conv_reflejo.py'), D], capture_output=True, text=True)
    if r.returncode: informe('AVISO: no se pudo rehacer REFLEJO.DAT (sin reflejo):\n' + (r.stderr or r.stdout)[-2000:])
    else: informe('  ' + (r.stdout.splitlines() or [''])[-1])
    import gdi
    informe('== GD-ROM en %s' % car)
    ultimo = [-1]
    def progreso(n, total):
        p = 100 * n // max(total, 1)
        if p >= ultimo[0] + 10: ultimo[0] = p; informe('  track03: %d %%' % p)
    gdi.escribe(car, primero, ip, D, progreso=progreso, informe=informe)
    shutil.rmtree(D, ignore_errors=True)
    informe('')
    informe('LISTO en %.0f s: %s' % (time.time() - t0, os.path.join(car, 'disc.gdi')))
    informe('Abre disc.gdi con Flycast, o copia disc.gdi y las tres pistas (track01.iso, track02.raw, track03.iso)')
    informe('a una carpeta de la tarjeta SD del GDEMU.')

if __name__ == '__main__':
    try: main()
    except KeyboardInterrupt: raise SystemExit('cortado')
