#!/usr/bin/env bash
# construye.sh [salida/]: el paquete de Windows (x64) del programa que hace el GD-ROM de Dreamcast desde la ISO de PS2
# (windows/outrun2dc.py + tools/regenera.py + tools/gdi.py). Lleva solo nuestro codigo, el 1ST_READ.BIN de demo/
# (compilado aqui con KallistiOS), el IP.BIN de makeip y dependencias libres: Python embebido de python.org, numpy,
# scipy, Pillow e imageio-ffmpeg (con su ffmpeg.exe) de PyPI, y las partes en C de tools/c compiladas con mingw-w64.
# Nada del disco (ni de la ROM / BIOS): los datos los hace el programa en el PC del usuario desde su ISO.
# Salida: <salida>/OutRun2DC.zip y, si pasa de 29 MB, tambien partido (zip -s 29m: OutRun2DC_partes.zip + .z01...).
# Necesita: curl, pip (python3 -m pip download), zip/unzip, x86_64-w64-mingw32-gcc (apt install mingw-w64) y
# KallistiOS (tools/instala_entorno.sh). Los paquetes descargados se guardan en <salida>/descargas (se reutilizan).
set -euo pipefail
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SAL=$(mkdir -p "${1:-$REPO/windows/paquete}" && cd "${1:-$REPO/windows/paquete}" && pwd)
PYVER=3.13.17                                            # (la ultima 3.13 con el zip embebido; los conversores usan 3.13)
# las mismas versiones que el entorno de Linux donde se prueban los conversores (tools/instala_entorno.sh)
RUEDAS="numpy==2.5.3 scipy==1.18.1 pillow==12.3.0 imageio-ffmpeg==0.8.1"
KOS=${KOS_BASE:-/opt/toolchains/dc/kos}
NOMBRE=${NOMBRE:-"OUTRUN DC PRUEBA"}                    # (el titulo del IP.BIN, el de tools/mkgdi.py)
# PROGRAMA=taxi: el de OutRun Mod con el taxi (windows/outrun_taxi.py: rehace el GD-ROM desde el del usuario con el
# taxi, que no sale de la ISO de PS2); si no, el de siempre (windows/outrun2dc.py: todo desde la ISO)
PROGRAMA=${PROGRAMA:-iso}
if [ "$PROGRAMA" = taxi ]; then PAQ=OutRunMod_Taxi; BAT=OutRunTaxi.bat; LEEME=LEEME_TAXI.txt
else PAQ=OutRun2DC; BAT=OutRun2DC.bat; LEEME=LEEME.txt; fi
DL=$SAL/descargas; P=$SAL/$PAQ; G=$P/programa
rm -rf "$P"; mkdir -p "$DL/ruedas" "$G/python" "$G/tools/ps2" "$G/nativo" "$G/dc"

echo "== Python $PYVER embebido y ruedas ($RUEDAS)"
Z=python-$PYVER-embed-amd64.zip
[ -s "$DL/$Z" ] || curl -sSfL -o "$DL/$Z" "https://www.python.org/ftp/python/$PYVER/$Z"
unzip -q "$DL/$Z" -d "$G/python"
python3 -m pip download -q --platform win_amd64 --python-version 3.13 --implementation cp --only-binary=:all: \
    -d "$DL/ruedas" $RUEDAS
SP=$G/python/Lib/site-packages; mkdir -p "$SP"
for r in numpy scipy pillow imageio_ffmpeg; do
    w=$(ls "$DL"/ruedas/${r}-*.whl | sort -V | tail -1); unzip -q -o "$w" -d "$SP"
done
# fuera las pruebas de numpy / scipy (no hacen falta: decenas de MB; numpy.testing se queda, lo importa scipy)
find "$SP" -type d \( -name tests -o -name __pycache__ \) -prune -exec rm -rf {} +
# rutas del Python embebido (el ._pth manda: sin el, no ve ni la carpeta del script): las herramientas y site-packages
PTH=$(ls "$G"/python/python3*._pth)
printf '%s\r\n' "$(basename "$(ls "$G"/python/python3*.zip)")" . 'Lib\site-packages' '..\tools' '..' 'import site' > "$PTH"

echo "== partes en C (mingw-w64): pvs.dll, visible.dll, reflejo.dll, musica.exe, pvrtex.exe"
CCW=x86_64-w64-mingw32-gcc
# (sin -march: cualquier x86-64; -ffp-contract=off: sin FMA, las mismas cuentas en todos los PC)
$CCW -O3 -ffp-contract=off -shared -static -o "$G/nativo/pvs.dll" "$REPO/tools/c/pvs.c" -lpthread -lm
$CCW -O2 -ffp-contract=off -shared -static -o "$G/nativo/visible.dll" "$REPO/tools/c/visible.c" -lm
$CCW -O2 -ffp-contract=off -shared -static -o "$G/nativo/reflejo.dll" "$REPO/tools/c/reflejo.c" -lm
$CCW -O2 -w -static -o "$G/nativo/musica.exe" "$REPO/tools/c/musica.c"
# pvrtex de KallistiOS (utils/pvrtex: libros VQ reducidos, tools/libros.py); LGPL (codigo de FFmpeg): va su fuente
PVT=$(mktemp -d); (cd "$KOS/utils/pvrtex" && tar cf - --exclude='*.o' --exclude=./pvrtex .) | tar xf - -C "$PVT"
make -s -C "$PVT" CC=$CCW CXX=x86_64-w64-mingw32-g++ TARGET=pvrtex.exe CXXFLAGS="-O3 -w -static" \
    CFLAGS="-O3 -w -static -std=gnu17" pvrtex.exe > /dev/null
cp "$PVT/pvrtex.exe" "$G/nativo/"; rm -rf "$PVT"
mkdir -p "$G/fuentes"; (cd "$KOS/utils" && zip -qr "$G/fuentes/pvrtex.zip" pvrtex -x '*.o' 'pvrtex/pvrtex')

echo "== 1ST_READ.BIN (demo/ con KallistiOS y Little Bloste) e IP.BIN (makeip)"
set +u; . "$KOS/environ.sh"; set -u
# (desde la Rev 2.9.2.c sin SH4ZAM: las matematicas del SH-4 son Little Bloste, comun/lbloste.h, codigo propio)
make -s -C "$REPO/demo" > /dev/null
"$KOS_CC_BASE/bin/sh-elf-objcopy" -R .stack -O binary "$REPO/demo/demo.elf" "$G/dc/1ST_READ.BIN"
"$KOS/utils/makeip/makeip" -g "$NOMBRE" -c HOMEBREW -n ORC2CDC -f "$G/dc/IP.BIN" > /dev/null
git -C "$REPO" log -1 --format='demo de orc2cdc %h (%cs)' > "$G/dc/VERSION.txt"

echo "== codigo"
cp "$REPO"/tools/*.py "$G/tools/"; cp "$REPO"/tools/ps2/*.py "$G/tools/ps2/"
cp "$REPO/demo/decor_tabla.h" "$G/tools/"                       # (tools/aligera.py: la tabla de la demo)
mkdir -p "$G/tools/c"; cp "$REPO"/tools/c/{pvs,visible,musica,reflejo}.c "$G/tools/c/"
cp "$REPO/windows/outrun2dc.py" "$G/"
[ "$PROGRAMA" = taxi ] && cp "$REPO/windows/outrun_taxi.py" "$G/"
for f in $BAT $LEEME; do sed 's/\r*$/\r/' "$REPO/windows/$f" > "$P/$f"; done
{
    printf 'Componentes de este paquete y sus licencias\r\n\r\n'
    printf ' - programa/tools, programa/*.py, %s: orc2cdc (codigo propio).\r\n' "$BAT"
    printf ' - programa/dc/1ST_READ.BIN: la demo de orc2cdc compilada con KallistiOS (su licencia abajo); las matematicas\r\n'
    printf '   del SH-4 son Little Bloste (codigo propio de orc2cdc, comun/lbloste.h).\r\n'
    printf ' - programa/dc/IP.BIN: makeip de KallistiOS.\r\n'
    printf ' - programa/python: Python %s embebido de python.org (licencia en programa/python/LICENSE.txt).\r\n' $PYVER
    printf ' - programa/python/Lib/site-packages: %s, de PyPI; cada uno con su licencia en\r\n' "$RUEDAS"
    printf '   su carpeta *.dist-info. ffmpeg (imageio_ffmpeg/binaries) es software libre (GPL/LGPL, ffmpeg.org).\r\n'
    printf ' - programa/nativo: tools/c de orc2cdc compilado con mingw-w64 (estatico, con winpthreads).\r\n'
    printf ' - programa/nativo/pvrtex.exe: pvrtex de KallistiOS (utils/pvrtex), LGPL por el codigo de FFmpeg que lleva;\r\n'
    printf '   su codigo fuente, tal cual se compila, en programa/fuentes/pvrtex.zip.\r\n\r\n'
    printf '==== Licencia de KallistiOS ====\r\n'; sed 's/\r*$/\r/' "$KOS/doc/LICENSE.md"
} > "$P/LICENCIAS.txt"
# comprobacion: en el paquete no hay nada del disco (ni datos convertidos ni la ISO)
if find "$P" \( -iname '*.iso' -o -iname '*.DAT' -o -iname '*.ADP' -o -iname '*.VQV' -o -iname '*.PAK' -o -iname '*.gdi' \) | grep -q .; then
    echo "el paquete lleva ficheros de datos" >&2; exit 1
fi

echo "== zip"
cd "$SAL"; rm -f $PAQ.zip ${PAQ}_partes.z*
zip -q -r -9 $PAQ.zip $PAQ
if [ "$(stat -c %s $PAQ.zip)" -gt $((29 * 1024 * 1024)) ]; then
    zip -q -r -9 -s 29m ${PAQ}_partes.zip $PAQ
fi
ls -l "$SAL"/$PAQ*.z*
du -sh "$P"
