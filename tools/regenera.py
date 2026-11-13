#!/usr/bin/env python3
"""regenera.py <iso> <datos/> [trabajo/] [--jobs N] [--gdi salida/ --bin 1ST_READ.BIN --ip IP.BIN]: rehace desde tu ISO
(OutRun 2006 C2C, PS2 PAL SLES-53998 v1.02) la carpeta de datos entera de la demo con los conversores del
repositorio, como hacia tools/regenera_datos.sh (que ahora solo llama a este), en Python para que funcione igual en
Linux y en Windows (el programa de windows/): mismos pasos, mismas salidas.
 - Idempotente: lo que ya esta en datos/ (o en trabajo/) no se rehace; si se corta, se vuelve a llamar y sigue.
 - En paralelo: --jobs pasos a la vez (por defecto JOBS o 4), cada uno un proceso de Python con un hilo (los dos
   largos que no se reparten, con dos); los registros de cada paso en trabajo/registro/<paso>.txt.
 - trabajo/ (por defecto /dev/shm/or2_regenera en Linux, si no datos/../trabajo): intermedios (uniones sueltas,
   ~350 MB) y registros. --limpia: al acabar bien, borra los intermedios de trabajo/ (no los registros).
 - --gdi: despues, el GD-ROM (disc.gdi y las tres pistas) en esa carpeta con tools/gdi.py (sin herramientas externas).
Lo que sale del disco no va al repositorio. Tiempo: unas horas en 4 nucleos (la visibilidad PVS de cada etapa y union
es lo que mas tarda). Sale con error si falla cualquier paso.
Las partes en C (tools/c: musica, pvs, visible) y ffmpeg: tools/nativo.py (en Windows, ya compiladas: OR2_NATIVO)."""
import glob, os, shutil, subprocess, sys, threading, time
from concurrent.futures import ThreadPoolExecutor

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
ARBOLES = '1,' + ','.join(str(i) for i in range(100, 194))       # OUTRUN2SP y los cursos de Races.bin (100 + registro)
# CONTRARRELOJ (tableros de cursos, conv_union.arbol_ta): los arboles 200 + codigo (0..0x53) en CURSOTA.DAT y sus
# uniones nuevas (BK262.. en UNIONESTA.PAK; las de los arboles de arriba conservan su numero)
ARBOLES_TA = ARBOLES + ',' + ','.join(str(200 + c) for c in range(0x54))
UNIONES_TA = ['UNIONESTA.PAK', 'CURSOTA.DAT']
# temas, resultados de OUTRUN2SP (28), menu, avances y metas (sin la 64: no la usa la meta)
MUSICA = list(range(0, 29)) + [30] + list(range(33, 61)) + [61, 62, 63, 65, 66]
PARTES = 4                                                       # conv_union.py --parte=k/n (al menos; o --jobs)
SP = ['CARGASP%d.DAT' % k for k in range(4)] + ['GOVSP.DAT', 'SPFE.DAT', 'RUTASP.DAT', 'RANKSP.DAT', 'NOMTX.DAT'] + \
     ['NOM5%s.DAT' % x for x in 'ABCDE']                       # (los de conv_sp.py)
FINAL = ['FINAL.DAT', 'FINPROP.DAT', 'STAFF.DAT'] + ['FINE%d.DAT' % k for k in range(10)] + \
        ['FINSKY%d.DAT' % k for k in range(5, 10)] + ['FINCHR%02X.DAT' % g for g in (0xb, 0xd, 0xe, 0xf, 0x11, 0x2a, 0x2c, 0x2d, 0x2e, 0x37)]   # (conv_final.py)
UNIONES = ['UNIONES.PAK', 'CURSO.DAT', 'COLLR.DAT', 'COLBK.DAT', 'BKD.DAT', 'SOL.DAT']
# los demas ficheros con numero que salen de la ISO PAL v1.02 (para la comprobacion final; auditoria de la revision 42
# contra la carpeta de datos completa): TRAF0..8 (conv_trafico.MODELOS, 9 modelos), BOnn de los conjuntos de objetos
# usados (conv_bo.py), COLBKkk de las uniones del modo OutRun con colision propia (conv_coli.py via conv_union.py)
TRAF = ['TRAF%d.DAT' % n for n in range(9)]
BO = ['BO%02d.DAT' % c for c in range(14)]   # (filas 1 y 6 de 0x35a738: OutRun y misiones de C2C, conv_bo.py)
COLBK = ['COLBK%02d.DAT' % k for k in (10, 11, 12, 13, 14, 25, 26, 27, 28, 29)]
FIN = ['FIN.DAT', 'GOVER.DAT', 'FINC2C.DAT']                    # (conv_fin.py; FINC2C: el modo 35 de una mision de C2C)
# segundos aproximados de cada paso en un nucleo (para el porcentaje y el tiempo que queda; medidos en Linux)
PESO = {'union': 4500, 'unionta': 3000, 'curso': 650, 'seg': 450, 'video_ts': 150, 'video_attr': 450, 'mus': 20, 'coche': 15,
        'menu': 40, 'trafico': 30, 'camiones': 25}
PESO_FIN = 300                                                   # empaquetado, revq y comprobacion (al final)

def peso(n):
    for k in sorted(PESO, key=len, reverse=True):
        if n.startswith(k): return PESO[k]
    return 5

# ---------------------------------------------------------------- utilidades
def py(*a): return [sys.executable, '-u'] + [str(x) for x in a]

def hay(D, *fs): return all(os.path.isfile(os.path.join(D, f)) and os.path.getsize(os.path.join(D, f)) > 0 for f in fs)

def mueve(D, *rutas):
    """a datos/ (solo cuando el paso ha acabado bien); con patrones"""
    for r in rutas:
        fs = sorted(glob.glob(r)) if any(c in r for c in '*?[') else [r]
        if not fs: raise RuntimeError('no se ha hecho %s' % r)
        for f in fs: os.replace(f, os.path.join(D, os.path.basename(f)))

def enlaza(src, dst):
    if os.path.exists(dst): os.remove(dst)
    try: os.link(src, dst)
    except OSError: shutil.copy2(src, dst)

def borra(d):
    if os.path.isdir(d): shutil.rmtree(d)

def hms(s):
    s = int(s); return '%d:%02d:%02d' % (s // 3600, s // 60 % 60, s % 60)

class Fallo(Exception): pass

class Contexto:
    def __init__(self, iso, D, W, jobs):
        self.iso, self.D, self.W, self.jobs = iso, D, W, jobs
        self.env = dict(os.environ)
        # un hilo por paso: numpy / scipy (OMP, OpenBLAS, MKL) y os.cpu_count() de Python 3.13 (hilos de pvs.py, Pool
        # de conv_video.py)
        self.env.update(OR2_ISO=iso, TOOLS=TOOLS, PYTHONDONTWRITEBYTECODE='1', OMP_NUM_THREADS='1',
                        OPENBLAS_NUM_THREADS='1', MKL_NUM_THREADS='1', PYTHON_CPU_COUNT='1', PYTHONIOENCODING='utf-8',
                        PYTHONHASHSEED='0')               # (conv_sp.py recorre conjuntos de cadenas: sin esto cambia
                                                          # de una vez a otra; con 0, los mismos bytes en Linux y Windows)
        self.env.setdefault('OR2_CACHE', os.path.join(W, 'cache'))

    def ejecuta(self, cmd, log, env=None):
        log.write('$ %s\n' % ' '.join(cmd)); log.flush()
        r = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, env=env or self.env, cwd=self.W)
        if r.returncode: raise Fallo('%s: salida %d' % (os.path.basename(cmd[2]) if len(cmd) > 2 else cmd[0], r.returncode))

    def saca(self, cmd):
        r = subprocess.run(cmd, capture_output=True, text=True, env=self.env, cwd=self.W)
        if r.returncode: raise Fallo('%s\n%s' % (' '.join(cmd), r.stdout + r.stderr))
        return r.stdout

# ---------------------------------------------------------------- un paso
def paso(C, n, log):
    D, W, ISO = C.D, C.W, C.iso
    t = os.path.join(W, 'tmp', n); borra(t); os.makedirs(t)
    env = dict(C.env)
    if n == 'curso' or n.startswith('video_'): env['PYTHON_CPU_COUNT'] = '2'   # (los dos pasos largos que no se reparten)
    ej = lambda *a: C.ejecuta(py(*a), log, env)
    T = lambda x: os.path.join(TOOLS, x)
    tj = lambda *x: os.path.join(t, *x)
    prev = lambda x: os.path.join(W, 'previas', x)
    if n.startswith('union'):
        ej(T('conv_union.py'), ISO, os.path.join(W, 'u'), '--arboles=' + ARBOLES, '--parte=%s/%d' % (n[5:], C.partes))
    elif n.startswith('unionta'):                        # los de la Contrarreloj (CURSOTA.DAT, BK262..)
        ej(T('conv_union.py'), ISO, os.path.join(W, 'uta'), '--arboles=' + ARBOLES_TA, '--bk-desde=262', '--curso=CURSOTA.DAT',
           '--parte=%s/%d' % (n[7:], C.partes))
    elif n == 'curso':                                   # BK00..14, BKD, SOL, COLBK*, CURSO del modo
        ej(T('conv_union.py'), ISO, t, '--sin-tramos')
        u0 = os.path.join(W, 'u0'); os.makedirs(u0, exist_ok=True)
        for f in os.listdir(t): os.replace(tj(f), os.path.join(u0, f))
        open(os.path.join(u0, '.hecho'), 'w').close()
    elif n.startswith('seg'):
        ej(T('regenera.py'), '--interno', 'seg', ISO, t, n[3:]); mueve(D, tj('SEG*.DAT'), tj('COL*.DAT'))
    elif n.startswith('mus'):
        k = n[3:]
        ej(T('regenera.py'), '--interno', 'mus', ISO, tj('in.ps2'), k)
        C.ejecuta([C.musica, tj('in.ps2'), tj('MUS%s.ADP' % k), '-', '0', '0x2000'], log, env)
        mueve(D, tj('MUS%s.ADP' % k))
    elif n.startswith('video_'):
        ej(T('conv_video.py'), ISO, t, '--hz', '50', '--solo', n[6:]); mueve(D, tj('*.VQV'))
    elif n == 'sonido':
        ej(T('conv_sonido.py'), ISO, t, '--idioma', 'SPANISH'); mueve(D, tj('SFX.DAT'), tj('VOCES.DAT'), tj('VOCESC.DAT'))
    elif n == 'menu': ej(T('conv_menu.py'), ISO, t, prev('menu')); mueve(D, tj('MENU.DAT'))
    elif n == 'hud': ej(T('conv_hud.py'), ISO, t, prev('hud'), prev('hud')); mueve(D, tj('HUD.DAT'))
    elif n == 'vmu': ej(T('conv_vmu.py'), ISO, t, prev('vmu')); mueve(D, tj('VMU.DAT'))
    elif n == 'modov': ej(T('conv_modo_video.py'), ISO, t, prev('modov')); mueve(D, tj('MODOV.DAT'))
    elif n == 'efectos': ej(T('conv_efectos.py'), ISO, tj('EFECTOS.DAT')); mueve(D, tj('EFECTOS.DAT'))
    elif n == 'faros': ej(T('conv_faros.py'), ISO, tj('FAROS.DAT')); mueve(D, tj('FAROS.DAT'))   # luz de los faros (0x106a98)
    elif n == 'chr': ej(T('conv_chr.py'), ISO, t, '--pares'); mueve(D, tj('CHR*.DAT'))   # CHRPAR y una CHRppaa por pareja
    elif n == 'band':
        ej(T('conv_chr.py'), ISO, tj('BAND.DAT'), '--banderero'); mueve(D, tj('BAND.DAT'))
        ej(T('conv_chr.py'), ISO, tj('BAND1.DAT'), '--banderero-sp'); mueve(D, tj('BAND1.DAT'))
    elif n == 'param': ej(T('conv_param.py'), ISO, tj('PARAM.DAT')); mueve(D, tj('PARAM.DAT'))
    elif n == 'meta': ej(T('conv_meta.py'), ISO, t); mueve(D, tj('META.DAT'))
    elif n == 'salida': ej(T('conv_salida.py'), ISO, t); mueve(D, tj('SALIDA.DAT'))
    elif n == 'infarto':
        ej(T('conv_infarto.py'), ISO, t); mueve(D, tj('INFARTO.DAT'), tj('INFARTOP.DAT'), tj('HAHUD.DAT'), tj('VOCESH.DAT'))
    elif n == 'fin': ej(T('conv_fin.py'), ISO, t, prev('fin')); mueve(D, *[tj(f) for f in FIN])
    elif n == 'sp':
        ej(T('conv_sp.py'), ISO, t, prev('sp'))
        mueve(D, tj('CARGASP?.DAT'), tj('GOVSP.DAT'), tj('SPFE.DAT'), tj('RUTASP.DAT'), tj('RANKSP.DAT'), tj('NOM5?.DAT'),
              tj('NOMTX.DAT'))
    elif n == 'final':                                   # el final de recreativa de OUTRUN2SP (modo 24, demo/final.c)
        ej(T('conv_final.py'), ISO, t); mueve(D, *[tj(f) for f in FINAL])
    elif n == 'bo': ej(T('conv_bo.py'), ISO, t); mueve(D, tj('BO*.DAT'))
    elif n == 'trafico': ej(T('conv_trafico.py'), ISO, t); mueve(D, tj('TRAF*.DAT'), tj('TAPD.DAT'))
    elif n == 'rivales':                                 # COAST 2 COAST: los coches de carreras (obj_rc, modelos 0..29)
        ej(T('conv_trafico.py'), ISO, t, '--rivales=' + ','.join(str(k) for k in range(30))); mueve(D, tj('RC*.DAT'))
    elif n == 'mision': ej(T('conv_mision.py'), ISO, t); mueve(D, tj('MISION.DAT'))   # Races.bin y tablas de 47CF40
    elif n == 'c2chud': ej(T('conv_hud.py'), ISO, t, '--c2c'); mueve(D, tj('C2CHUD.DAT'))   # HUD de las misiones de C2C
    elif n == 'c2c': ej(T('conv_c2c.py'), ISO, t); mueve(D, tj('C2C.DAT'))   # pantallas de C2C (banco 0x44, menu.c)
    elif n == 'salon': ej(T('conv_salon.py'), ISO, t); mueve(D, tj('SALON.DAT'))   # el SALON (showroom, banco 0x44, menu.c)
    elif n == 'tablero': ej(T('conv_tablero.py'), ISO, t); mueve(D, tj('TABLERO.DAT'))   # tableros de cursos (banco 0x44, menu.c)
    elif n == 'camiones':                                        # los camiones con remolque de OUTRUN2SP (39/40, 41, 43)
        ej(T('conv_trafico.py'), ISO, t, '--camiones'); mueve(D, tj('TRAFK0.DAT'), tj('TRAFK1.DAT'), tj('TRAFK2.DAT'))
    elif n.startswith('coche'):
        ej(T('conv_coche.py'), ISO, '--todos', t, '--fichas', n[5:])
        c = os.path.join(W, 'coche'); os.makedirs(c, exist_ok=True)
        for f in glob.glob(tj('COCHE[0-9]*.DAT')): os.replace(f, os.path.join(c, os.path.basename(f)))
    else: raise Fallo('paso desconocido: %s' % n)
    borra(t)

# ---------------------------------------------------------------- lo que se hace dentro de un proceso aparte
def interno(a):
    from ps2.assets import Assets
    if a[0] == 'seg':                                    # seg <iso> <dir> <ti>: SEGnn.DAT y COLnn.DAT de la etapa ti
        import conv_union as U
        U.etapa_ficheros(Assets(a[1]), int(a[3]), a[2])
    elif a[0] == 'mus':                                  # mus <iso> <salida> <k>: la pista k de MUSIC.PS2
        A = Assets(a[1])
        with open(a[2], 'wb') as f: f.write(A.datos(A.entrada('MUSIC.PS2', '\\%02d.ps2' % int(a[3]))))
    elif a[0] == 'prepara':                              # prepara <iso>: nombres de los paquetes y pvs.c compilado
        import pvs
        A = Assets(a[1]); A.resolver(); pvs.lib()
    elif a[0] == 'segs':                                 # segs <iso>: ti:segmento de las etapas de todos los arboles
        import conv_union as U
        A = Assets(a[1]); E = A.elf; base, nprim, base2 = U.tablas(A)
        ts = {r[1] for r in U.arbol(A, 0)} | {r[1] for r in U.arbol(A, 0, 'csc_data_cvt.bin')}
        for i in range(94): ts |= {r[1] for r in U.arbol_mision(A, i)}
        for ti in sorted(ts): print('%d:%d' % (ti, U.segmento_de(A, U.cadena(E, U.u32(E, base + 4 * ti) + 0x0c))))
    elif a[0] == 'revq':                                 # revq <patron>...: tools/revq.py con los ficheros de los patrones
        import revq
        sys.argv = ['revq.py'] + sorted(f for p in a[1:] for f in glob.glob(p)); revq.main()
    else: raise SystemExit('interno: %s' % a[0])

# ---------------------------------------------------------------- principal
def regenera(iso, D, W, jobs=4, informe=print):
    """rehace lo que falta en D; devuelve True si los datos estan completos"""
    iso = os.path.abspath(iso)
    if not os.path.isfile(iso): raise SystemExit('no esta la ISO: %s' % iso)
    os.makedirs(D, exist_ok=True); os.makedirs(W, exist_ok=True)
    D, W = os.path.abspath(D), os.path.abspath(W)
    C = Contexto(iso, D, W, jobs)
    C.partes = max(PARTES, jobs)                         # (las uniones son independientes: f_bk % partes)
    for x in ('u', 'tmp', 'previas', 'registro'): os.makedirs(os.path.join(W, x), exist_ok=True)
    reg = lambda x: os.path.join(W, 'registro', x)
    t0 = time.time()

    informe('== preparacion (nombres de los paquetes, pvs.c, musica.c, ffmpeg)')
    import nativo
    try:
        subprocess.run([nativo.ffmpeg(), '-version'], capture_output=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        raise SystemExit('no se encuentra ffmpeg (%s)' % nativo.ffmpeg())
    with open(reg('preparacion.txt'), 'w') as log:
        try:
            C.ejecuta(py(os.path.join(TOOLS, 'regenera.py'), '--interno', 'prepara', iso), log)
        except Fallo:
            raise SystemExit('FALLO la preparacion (ver %s): la ISO tiene que ser la PAL SLES-53998 v1.02' % reg('preparacion.txt'))
    os.environ.setdefault('OR2_CACHE', C.env['OR2_CACHE'])
    C.musica = nativo.programa('musica')

    # lista de pasos que faltan (lo hecho no se repite)
    falta = []
    if not hay(D, *UNIONES):                             # (conv_union.py no rehace las uniones sueltas que ya estan)
        if not os.path.isfile(os.path.join(W, 'u0', '.hecho')): falta.append('curso')
        falta += ['union%d' % k for k in range(C.partes)]
    if not hay(D, *UNIONES_TA): falta += ['unionta%d' % k for k in range(C.partes)]
    # primero lo que hace falta para arrancar y correr (coches, HUD, menus, sonido, musica, etapas); las peliculas al final
    for k in range(15):
        if not hay(D, 'COCHES.DAT') and not hay(os.path.join(W, 'coche'), 'COCHE%02d.DAT' % k): falta.append('coche%d' % k)
    for f, n in (('MENU.DAT', 'menu'), ('HUD.DAT', 'hud'), ('VMU.DAT', 'vmu'), ('MODOV.DAT', 'modov')):
        if not hay(D, f): falta.append(n)
    if not hay(D, 'SFX.DAT', 'VOCES.DAT', 'VOCESC.DAT'): falta.append('sonido')
    falta += ['mus%d' % k for k in MUSICA if not hay(D, 'MUS%d.ADP' % k)]
    for l in C.saca(py(os.path.join(TOOLS, 'regenera.py'), '--interno', 'segs', iso)).split():
        ti, n = l.split(':')
        if not hay(D, 'SEG%02d.DAT' % int(n), 'COL%02d.DAT' % int(n)): falta.append('seg' + ti)
    chrs = C.saca(py(os.path.join(TOOLS, 'conv_chr.py'), iso, '--lista')).split()   # CHRPAR y las parejas
    for fs, n in ((['EFECTOS.DAT'], 'efectos'), (['FAROS.DAT'], 'faros'), (chrs, 'chr'), (['BAND.DAT', 'BAND1.DAT'], 'band'), (['PARAM.DAT'], 'param'),
                  (['META.DAT'], 'meta'), (FIN, 'fin'),
                  (SP, 'sp'), (FINAL, 'final'),
                  (['SALIDA.DAT'], 'salida'), (['INFARTO.DAT', 'INFARTOP.DAT', 'HAHUD.DAT', 'VOCESH.DAT'], 'infarto'), (['BO.DAT'] + BO, 'bo'),
                  (['TRAFC.DAT', 'TAPD.DAT'] + TRAF, 'trafico'), (['TRAFK0.DAT', 'TRAFK1.DAT', 'TRAFK2.DAT'], 'camiones'),
                  (['MVTS.VQV'], 'video_ts'), (['MVATTR.VQV'], 'video_attr'),
                  (['RC%02d.DAT' % k for k in range(30)], 'rivales'), (['MISION.DAT'], 'mision'), (['C2CHUD.DAT'], 'c2chud'),
                  (['C2C.DAT'], 'c2c'), (['SALON.DAT'], 'salon'), (['TABLERO.DAT'], 'tablero')):
        if not hay(D, *fs): falta.append(n)

    informe('== %d pasos en %d a la vez (registros en %s)' % (len(falta), jobs, os.path.join(W, 'registro')))
    total = sum(peso(n) for n in falta) + PESO_FIN; hecho = [0.0]; t1 = time.time(); mal = []
    cerrojo = threading.Lock(); en_marcha = {}; acabado = threading.Event()
    def avance():
        """fraccion hecha: los pasos acabados y lo que llevan los que estan en marcha (por su tiempo, hasta el 90 %)"""
        ahora = time.time()
        f = (hecho[0] + sum(min(ahora - t, 0.9 * peso(n)) for n, t in en_marcha.items())) / total
        tr = ahora - t1
        return f, ' quedan ~%s' % hms(tr * (1 - f) / f) if f > 0.01 and tr > 120 else ''
    def uno(n):
        with cerrojo: en_marcha[n] = time.time()
        ok = True
        try:
            with open(reg(n + '.txt'), 'w', encoding='utf-8', errors='replace') as log:
                try: paso(C, n, log)
                except Exception as e:
                    ok = False; log.write('\nFALLO: %s\n' % e)
        finally:
            with cerrojo:
                del en_marcha[n]; hecho[0] += peso(n)
                if not ok: mal.append(n)
                f, queda = avance()
                informe('  [%3d %%] %s %s  (%s%s)%s' % (int(100 * f), 'hecho' if ok else 'FALLO', n, hms(time.time() - t0), queda,
                        '' if ok else '  ver ' + reg(n + '.txt')))
    def latido():                                        # cada 5 min, lo que esta en marcha (los pasos largos tardan ~1 h)
        while not acabado.wait(300):
            with cerrojo:
                f, queda = avance()
                informe('  [%3d %%] en marcha: %s  (%s%s)' % (int(100 * f), ' '.join(sorted(en_marcha)), hms(time.time() - t0), queda))
    threading.Thread(target=latido, daemon=True).start()
    try:
        with ThreadPoolExecutor(max_workers=jobs) as ex:
            list(ex.map(uno, falta))
    finally:
        acabado.set()
    if mal: raise SystemExit('fallo algun paso: %s (registros en %s)' % (' '.join(mal), os.path.join(W, 'registro')))

    def post(nombre, cmd, ultimas=1):
        with open(reg(nombre + '.txt'), 'w', encoding='utf-8', errors='replace') as log:
            try: C.ejecuta(cmd, log)
            except Fallo: raise SystemExit('FALLO %s (ver %s)' % (nombre, reg(nombre + '.txt')))
        ls = open(reg(nombre + '.txt'), encoding='utf-8', errors='replace').read().splitlines()
        for l in ls[len(ls) - ultimas:] if ultimas else []: informe(l)
    informe('== uniones: UNIONES.PAK y descriptores (pak_union.py)')
    if not hay(D, *UNIONES):
        bk, pak = os.path.join(W, 'bk'), os.path.join(W, 'pak')
        borra(bk); borra(pak); os.makedirs(bk); os.makedirs(pak)
        for f in glob.glob(os.path.join(W, 'u0', 'BK[0-9]*.DAT')) + glob.glob(os.path.join(W, 'u', 'BK[0-9]*.DAT')):
            enlaza(f, os.path.join(bk, os.path.basename(f)))
        # segunda pasada de la VQ (tools/revq.py; sin nada que hacer si las uniones se convirtieron con el modelo.py de ahora)
        post('revq_bk', py(os.path.join(TOOLS, 'regenera.py'), '--interno', 'revq', os.path.join(bk, 'BK[0-9]*.DAT')))
        C.env['PYTHON_CPU_COUNT'] = str(jobs)
        try: post('pak', py(os.path.join(TOOLS, 'pak_union.py'), bk, pak), 3)
        finally: C.env['PYTHON_CPU_COUNT'] = '1'
        for f in glob.glob(os.path.join(W, 'u0', 'COLBK*.DAT')) + glob.glob(os.path.join(W, 'u', 'COLBK*.DAT')) + \
                 [os.path.join(W, 'u', 'COLLR.DAT')]:
            shutil.copy2(f, D)
        for f in os.listdir(pak): os.replace(os.path.join(pak, f), os.path.join(D, f))
        for f in (os.path.join(W, 'u0', 'BKD.DAT'), os.path.join(W, 'u0', 'SOL.DAT'), os.path.join(W, 'u', 'CURSO.DAT')):
            shutil.copy2(f, D)                           # (CURSO.DAT: el de todos los arboles)
    informe('== uniones de la Contrarreloj: UNIONESTA.PAK y descriptores (pak_union.py --pak=UNIONESTA.PAK)')
    if not hay(D, *UNIONES_TA):
        bk, pak = os.path.join(W, 'bkta'), os.path.join(W, 'pakta')
        borra(bk); borra(pak); os.makedirs(bk); os.makedirs(pak)
        for f in glob.glob(os.path.join(W, 'uta', 'BK[0-9]*.DAT')): enlaza(f, os.path.join(bk, os.path.basename(f)))
        C.env['PYTHON_CPU_COUNT'] = str(jobs)
        try: post('pakta', py(os.path.join(TOOLS, 'pak_union.py'), bk, pak, '--pak=UNIONESTA.PAK'), 3)
        finally: C.env['PYTHON_CPU_COUNT'] = '1'
        for f in glob.glob(os.path.join(W, 'uta', 'COLBK*.DAT')):
            if not os.path.exists(os.path.join(D, os.path.basename(f))): shutil.copy2(f, D)
        for f in os.listdir(pak): os.replace(os.path.join(pak, f), os.path.join(D, f))
        shutil.copy2(os.path.join(W, 'uta', 'CURSOTA.DAT'), D)
    informe('== etapas: SEGS.PAK y descriptores (pak_union.py --prefijo=SEG; tope = HUECO_ETAPA_MAX de demo/main.c)')
    if not hay(D, 'SEGS.PAK'):
        seg, segpak = os.path.join(W, 'seg'), os.path.join(W, 'segpak')
        borra(seg); borra(segpak); os.makedirs(seg); os.makedirs(segpak)
        for f in glob.glob(os.path.join(D, 'SEG[0-9]*.DAT')): enlaza(f, os.path.join(seg, os.path.basename(f)))
        post('revq_seg', py(os.path.join(TOOLS, 'regenera.py'), '--interno', 'revq', os.path.join(seg, 'SEG[0-9]*.DAT')))
        post('segpak', py(os.path.join(TOOLS, 'pak_union.py'), seg, segpak, '--pak=SEGS.PAK', '--prefijo=SEG', '--tope=7842'))
        for f in os.listdir(segpak): os.replace(os.path.join(segpak, f), os.path.join(D, f))
    if not hay(D, 'VISO.DAT'):                           # nodos del original en la seccion 0 y la ultima de cada etapa
        post('viso', py(os.path.join(TOOLS, 'conv_viso.py'), iso, D))   # (conv_viso.py: lee SEGnn.DAT y SEGS.PAK)
    if not hay(D, 'COCHES.DAT'):
        for f in glob.glob(os.path.join(W, 'coche', 'COCHE[0-9]*.DAT')): os.replace(f, os.path.join(D, os.path.basename(f)))
        post('coches', py(os.path.join(TOOLS, 'conv_coche.py'), iso, '--todos', D, '--fichas', '99'), 0)   # solo COCHES.DAT

    informe('== segunda pasada de la VQ en los modelos sueltos (revq.py: trafico, personajes, banderero, objetos; no los coches)')
    post('revq', py(os.path.join(TOOLS, 'regenera.py'), '--interno', 'revq', *(os.path.join(D, p) for p in
                    ('TRAF[0-9]*.DAT', 'CHR[0-9]*.DAT', 'BAND*.DAT', 'BO[0-9]*.DAT'))))

    # ------------------------------------------------------------ comprobacion
    informe('== comprobacion de %s' % D)
    ok = True
    nec = ['BAND.DAT', 'BAND1.DAT', 'BKD.DAT', 'BO.DAT'] + chrs + \
          'COCHES.DAT COLBK.DAT COLLR.DAT CURSO.DAT EFECTOS.DAT FAROS.DAT FIN.DAT GOVER.DAT HAHUD.DAT HUD.DAT INFARTO.DAT MENU.DAT META.DAT MODOV.DAT MVATTR.VQV MVTS.VQV PARAM.DAT SFX.DAT SEGS.PAK SOL.DAT TAPD.DAT TRAFC.DAT UNIONES.PAK VMU.DAT VOCES.DAT VOCESH.DAT VOCESC.DAT MISION.DAT C2CHUD.DAT C2C.DAT SALON.DAT TABLERO.DAT CURSOTA.DAT UNIONESTA.PAK'.split() + ['RC%02d.DAT' % k for k in range(30)] + \
          ['COCHE%02d.DAT' % k for k in range(15)] + ['MUS%d.ADP' % k for k in MUSICA] + SP + FINAL + \
          ['TRAFK%d.DAT' % k for k in range(3)] + FIN + TRAF + BO + COLBK + ['SALIDA.DAT', 'CHRPAR.DAT', 'VISO.DAT']
    for f in nec:
        if not hay(D, f): informe('FALTA %s' % f); ok = False
    ls = sorted(os.listdir(D))
    nseg = sum(1 for f in ls if f.startswith('SEG') and f[3:-4].isdigit() and f.endswith('.DAT'))
    nbk = sum(1 for f in ls if f.startswith('BK') and f[2:-4].isdigit() and f.endswith('.DAT'))
    if nseg != 62: informe('SEG: %d etapas (se esperan 62)' % nseg); ok = False
    if nbk != 262: informe('BK: %d uniones (se esperan 262)' % nbk); ok = False
    for f in ls:
        if f.startswith('BK') and f[2:-4].isdigit() and f.endswith('.DAT'):
            with open(os.path.join(D, f), 'rb') as h:
                if h.read(4) != b'PAKD': informe('%s no es un descriptor PAKD' % f); ok = False
    grupos = {}
    for f in ls:
        g = f.rsplit('.', 1)[0].rstrip('0123456789'); x = grupos.setdefault(g, [0, 0])
        x[0] += 1; x[1] += os.path.getsize(os.path.join(D, f))
    for g in sorted(grupos): informe('  %-10s %4d ficheros %10.1f MiB' % (g, grupos[g][0], grupos[g][1] / 1048576))
    b = sum(x[1] for x in grupos.values())
    informe('== %d ficheros, %d bytes (%.1f MiB); SEG %d, BK %d; %s' % (len(ls), b, b / 1048576, nseg, nbk, hms(time.time() - t0)))
    if not ok: raise SystemExit('datos incompletos')
    return True

def limpia(W):
    """los intermedios de trabajo/ (no los registros ni la cache)"""
    for x in ('u', 'u0', 'bk', 'pak', 'seg', 'segpak', 'coche', 'tmp', 'previas'): borra(os.path.join(W, x))

def main():
    a = sys.argv[1:]
    if a and a[0] == '--interno': return interno(a[1:])
    op = {}; pos = []; i = 0
    while i < len(a):
        if a[i] in ('--jobs', '--gdi', '--bin', '--ip'): op[a[i][2:]] = a[i + 1]; i += 2
        elif a[i] == '--limpia': op['limpia'] = True; i += 1
        else: pos.append(a[i]); i += 1
    if len(pos) < 2: raise SystemExit(__doc__)
    iso, D = pos[0], pos[1]
    W = pos[2] if len(pos) > 2 else ('/dev/shm/or2_regenera' if os.path.isdir('/dev/shm') else
                                      os.path.join(os.path.dirname(os.path.abspath(D)), 'trabajo'))
    jobs = int(op.get('jobs') or os.environ.get('JOBS') or 4)
    regenera(iso, D, W, jobs)
    if op.get('limpia'): limpia(W)
    if op.get('gdi'):
        import gdi
        if not (op.get('bin') and op.get('ip')): raise SystemExit('--gdi necesita --bin 1ST_READ.BIN y --ip IP.BIN')
        gdi.escribe(op['gdi'], op['bin'], op['ip'], D)

if __name__ == '__main__':
    main()
