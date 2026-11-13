"""nativo.py: las partes en C de los conversores (tools/c) y ffmpeg, en Linux y en Windows.
- lib(nombre, opciones): la biblioteca de tools/c/<nombre>.c cargada con ctypes. Si OR2_NATIVO apunta a una carpeta
  con <nombre>.dll / .so ya compilada (el programa de Windows: windows/construye.sh las compila con mingw-w64), esa;
  si no, se compila con cc en la cache (OR2_CACHE o ~/.cache/or2_assets) cuando falta o el .c es mas nuevo.
- programa(nombre): igual con un ejecutable (tools/c/<nombre>.c -> <nombre> / <nombre>.exe).
- ffmpeg(): OR2_FFMPEG, el de imageio-ffmpeg si esta instalado, o el ffmpeg del sistema.
- video_info(ruta): (cuadros por segundo, duracion en s) del primer flujo de video, de lo que escribe ffmpeg -i (sin
  ffprobe, que no viene con imageio-ffmpeg)."""
import ctypes, os, re, subprocess, sys

C = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'c')
WIN = sys.platform == 'win32'

def _cache():
    d = os.environ.get('OR2_CACHE') or os.path.expanduser('~/.cache/or2_assets')
    os.makedirs(d, exist_ok=True); return d

def _hecho(nombre):
    d = os.environ.get('OR2_NATIVO')
    if d:
        p = os.path.join(d, nombre)
        if os.path.exists(p): return p
    return None

def _compila(src, dst, opciones):
    if not os.path.exists(dst) or os.path.getmtime(dst) < os.path.getmtime(src):
        tmp = dst + '.%d.tmp' % os.getpid()                # (varios procesos a la vez: el ultimo gana, entero)
        subprocess.check_call(['cc'] + opciones + ['-o', tmp, src, '-lm'])
        os.replace(tmp, dst)
    return dst

def lib(nombre, opciones=('-O2',)):
    p = _hecho(nombre + ('.dll' if WIN else '.so'))
    if not p:
        if WIN: raise SystemExit('falta %s.dll (OR2_NATIVO)' % nombre)
        p = _compila(os.path.join(C, nombre + '.c'), os.path.join(_cache(), nombre + '.so'),
                     list(opciones) + ['-shared', '-fPIC'])
    return ctypes.CDLL(os.path.abspath(p))

def programa(nombre, opciones=('-O2', '-w')):
    p = _hecho(nombre + ('.exe' if WIN else ''))
    if not p:
        if WIN: raise SystemExit('falta %s.exe (OR2_NATIVO)' % nombre)
        p = _compila(os.path.join(C, nombre + '.c'), os.path.join(_cache(), nombre), list(opciones))
    return os.path.abspath(p)

def ffmpeg():
    if os.environ.get('OR2_FFMPEG'): return os.environ['OR2_FFMPEG']
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except Exception:
        return 'ffmpeg'

def video_info(ruta):
    r = subprocess.run([ffmpeg(), '-hide_banner', '-i', ruta], capture_output=True, text=True, errors='replace').stderr
    fps = dur = 0.0
    m = re.search(r'Duration: (\d+):(\d+):(\d+(?:\.\d+)?)', r)
    if m: dur = int(m.group(1)) * 3600 + int(m.group(2)) * 60 + float(m.group(3))
    for l in r.splitlines():
        if 'Video:' in l:
            m = re.search(r'([\d.]+)(k?) tbr', l) or re.search(r'([\d.]+)(k?) fps', l)
            if m: fps = float(m.group(1)) * (1000 if m.group(2) else 1)
            break
    # ffmpeg escribe 29.97 / 59.94 / 23.98 redondeados: los NTSC exactos (30000/1001...), como los da ffprobe
    for n in (24, 30, 60):
        if abs(fps - n * 1000 / 1001) < 0.006: fps = n * 1000 / 1001
    return fps, dur
