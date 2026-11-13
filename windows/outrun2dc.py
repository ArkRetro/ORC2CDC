"""outrun2dc.py: el programa de Windows (OutRun2DC.bat lo lanza con el Python que va en el paquete; tambien funciona
en Linux con python3). Pide tu ISO de PS2 (OutRun 2006 Coast 2 Coast, PAL SLES-53998 v1.02) y una carpeta, rehace
alli los datos de la demo desde la ISO (tools/regenera.py: horas) y escribe el GD-ROM de Dreamcast (tools/gdi.py):
disc.gdi, track01.iso, track02.raw y track03.iso, con el 1ST_READ.BIN y el IP.BIN del paquete (programa/dc).
Uso: outrun2dc.py [tu.iso] [carpeta] [--jobs N] [--bin 1ST_READ.BIN --ip IP.BIN] [--sin-dialogos]
Sin la ISO o la carpeta, las pide con los dialogos de Windows (PowerShell) o, si no hay, por teclado.
Se puede cortar (cerrar la ventana o Ctrl+C) y volver a lanzar con la misma ISO y carpeta: sigue donde iba.
En la carpeta: <carpeta>/OutRun2006_DC/ con el GDI, y trabajo/ (datos/ convertidos, registros; los intermedios se
borran al acabar). No hace falta nada del PC (ni la decompilacion ni el recomp): todo sale de la ISO."""
import os, shutil, subprocess, sys, time

AQUI = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.join(AQUI, 'tools') if os.path.isdir(os.path.join(AQUI, 'tools')) else os.path.join(AQUI, '..', 'tools')
sys.path.insert(0, os.path.abspath(TOOLS))
SUB = 'OutRun2006_DC'
ESTADO = 'orc2cdc.txt'
GB = 1 << 30
WIN = sys.platform == 'win32'

_log = [None]
def informe(*a):
    s = ' '.join(str(x) for x in a)
    print(s, flush=True)
    if _log[0]:
        try:
            with open(_log[0], 'a', encoding='utf-8') as f: f.write(s + '\n')
        except OSError: pass

def powershell(script):
    """lo que escribe un script de PowerShell (los dialogos); None si no hay PowerShell o se cancela"""
    if not WIN: return None
    try:
        r = subprocess.run(['powershell', '-NoProfile', '-STA', '-ExecutionPolicy', 'Bypass', '-Command',
                            '[Console]::OutputEncoding = [Text.Encoding]::UTF8; ' + script],
                           capture_output=True, timeout=3600)
    except (OSError, subprocess.SubprocessError): return None
    s = r.stdout.decode('utf-8', 'replace').strip().lstrip('﻿')
    return s or None

DIALOGO = """Add-Type -AssemblyName System.Windows.Forms
$f = New-Object System.Windows.Forms.Form -Property @{TopMost = $true}
"""
def elige_iso():
    r = powershell(DIALOGO + """$d = New-Object System.Windows.Forms.OpenFileDialog
$d.Title = 'Elige tu ISO de OutRun 2006 Coast 2 Coast (PS2, PAL SLES-53998)'
$d.Filter = 'Imagen ISO (*.iso)|*.iso|Todos los ficheros (*.*)|*.*'
if ($d.ShowDialog($f) -eq 'OK') { Write-Output $d.FileName }""")
    return r or pregunta('Ruta de tu ISO de PS2 (SLES-53998): ')

def elige_carpeta():
    r = powershell(DIALOGO + """$d = New-Object System.Windows.Forms.FolderBrowserDialog
$d.Description = 'Carpeta donde se crea el GD-ROM (hacen falta unos 4 GB libres). Dentro se crea OutRun2006_DC.'
$d.ShowNewFolderButton = $true
if ($d.ShowDialog($f) -eq 'OK') { Write-Output $d.SelectedPath }""")
    return r or pregunta('Carpeta de salida (se crea dentro %s): ' % SUB)

def pregunta(texto):
    try: r = input(texto).strip().strip('"')
    except EOFError: r = ''
    if not r: raise SystemExit('cancelado')
    return r

def ram_gb():
    try:
        if WIN:
            import ctypes
            class M(ctypes.Structure):
                _fields_ = [('dwLength', ctypes.c_ulong), ('dwMemoryLoad', ctypes.c_ulong)] + \
                           [(x, ctypes.c_ulonglong) for x in ('total', 'libre', 'tp', 'tpl', 'tv', 'tvl', 'tve')]
            m = M(); m.dwLength = ctypes.sizeof(M); ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(m))
            return m.total / GB
        return os.sysconf('SC_PAGE_SIZE') * os.sysconf('SC_PHYS_PAGES') / GB
    except Exception:
        return 8.0

def comprueba_iso(iso):
    from ps2.iso import Iso
    try: i = Iso(iso)
    except Exception as e: raise SystemExit('no se puede leer la ISO %s: %s' % (iso, e))
    if 'SYSTEM.CNF' not in i.fich or 'SLES_539.98' not in i.fich:
        raise SystemExit('esta ISO no es OutRun 2006 Coast 2 Coast PAL (SLES-53998)')
    o, s = i.fich['SYSTEM.CNF']; cnf = bytes(i.m[o:o + s]).decode('latin1')
    if 'VER = 1.02' not in cnf:
        informe('AVISO: la ISO no es la version 1.02 (SYSTEM.CNF: %s); los conversores estan hechos para la 1.02'
                % ' '.join(cnf.split()))
    i.m.close(); i.f.close()

def main():
    a = sys.argv[1:]; op = {}; pos = []; k = 0
    while k < len(a):
        if a[k] in ('--jobs', '--bin', '--ip'): op[a[k][2:]] = a[k + 1]; k += 2
        elif a[k] == '--sin-dialogos': op['sin'] = True; k += 1
        else: pos.append(a[k]); k += 1
    informe('OutRun 2006 Coast 2 Coast para Dreamcast: GD-ROM desde tu ISO de PS2 (orc2cdc)')
    informe('')
    iso = pos[0] if pos else (pregunta('Ruta de tu ISO de PS2 (SLES-53998): ') if op.get('sin') else elige_iso())
    iso = os.path.abspath(iso)
    if not os.path.isfile(iso): raise SystemExit('no esta la ISO: %s' % iso)
    comprueba_iso(iso)
    informe('ISO: %s' % iso)
    car = pos[1] if len(pos) > 1 else (pregunta('Carpeta de salida: ') if op.get('sin') else elige_carpeta())
    car = os.path.abspath(car)
    if os.path.basename(car) != SUB and not os.path.isfile(os.path.join(car, ESTADO)): car = os.path.join(car, SUB)
    os.makedirs(car, exist_ok=True)
    W = os.path.join(car, 'trabajo'); D = os.path.join(W, 'datos')
    os.makedirs(os.path.join(W, 'registro'), exist_ok=True)
    _log[0] = os.path.join(W, 'registro', 'programa.txt')
    with open(os.path.join(car, ESTADO), 'w', encoding='utf-8') as f: f.write('iso=%s\n' % iso)
    informe('Carpeta: %s' % car)
    primero = op.get('bin') or os.path.join(AQUI, 'dc', '1ST_READ.BIN')
    ip = op.get('ip') or os.path.join(AQUI, 'dc', 'IP.BIN')
    for f in (primero, ip):
        if not os.path.isfile(f): raise SystemExit('falta %s (paquete incompleto)' % f)
    # partes en C ya compiladas y el ffmpeg de imageio-ffmpeg (paquete de Windows)
    if os.path.isdir(os.path.join(AQUI, 'nativo')): os.environ['OR2_NATIVO'] = os.path.join(AQUI, 'nativo')
    os.environ['OR2_CACHE'] = os.path.join(W, 'cache')
    libre = shutil.disk_usage(car).free / GB
    hecho = os.path.isdir(D) and sum(os.path.getsize(os.path.join(D, x)) for x in os.listdir(D)) / GB or 0
    if libre + hecho < 4:
        informe('AVISO: solo hay %.1f GB libres en %s; hacen falta unos 4 GB' % (libre, car))
    cpus = os.cpu_count() or 2
    jobs = int(op.get('jobs') or max(1, min(cpus, int(ram_gb() // 1.5), 16)))
    informe('Pasos a la vez: %d (%d nucleos, %.0f GB de RAM)' % (jobs, cpus, ram_gb()))
    informe('Tarda unas horas (la visibilidad de las etapas es lo mas largo). Se puede cortar y volver a lanzar:')
    informe('sigue donde iba.')
    informe('')
    import regenera, gdi
    t0 = time.time()
    regenera.regenera(iso, D, W, jobs, informe=informe)
    regenera.limpia(W)
    informe('== GD-ROM en %s' % car)
    ultimo = [-1]
    def progreso(n, total):
        p = 100 * n // max(total, 1)
        if p >= ultimo[0] + 10: ultimo[0] = p; informe('  track03: %d %%' % p)
    gdi.escribe(car, primero, ip, D, progreso=progreso, informe=informe)
    informe('')
    informe('LISTO en %s: %s' % (regenera.hms(time.time() - t0), os.path.join(car, 'disc.gdi')))
    informe('Abre disc.gdi con Flycast, o copia disc.gdi y las tres pistas (track01.iso, track02.raw, track03.iso)')
    informe('a una carpeta de la tarjeta SD del GDEMU. Los datos convertidos quedan en %s' % D)
    informe('(sirven para rehacer el GD-ROM con otro 1ST_READ.BIN sin volver a convertir; se pueden borrar).')

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        informe('\ncortado: vuelve a lanzarlo con la misma ISO y carpeta para seguir'); sys.exit(1)
    except SystemExit as e:
        if e.code not in (None, 0):
            informe('\nERROR: %s' % e.code if isinstance(e.code, str) else '\nERROR (%s)' % e.code); sys.exit(1)
