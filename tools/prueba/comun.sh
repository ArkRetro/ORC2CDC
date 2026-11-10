# comun.sh: funciones de las pruebas en Flycast (lo cargan nav.sh, nav3.sh y pal.sh; no se llama solo).
# Variables: GDI (disc.gdi de tools/mkgdi.py), PANTALLA (numero del display de Xvfb), SAL (carpeta de las capturas y el
# registro), FLYCAST (por defecto /opt/flycast/build/flycast). Teclado de Flycast por defecto: X = A, C = B, S = X,
# D = Y, Return = START, flechas = cruceta, F / V = gatillos izquierdo / derecho.
set -euo pipefail
FLYCAST=${FLYCAST:-/opt/flycast/build/flycast}

uso() { echo "uso: $0 <disc.gdi> [pantalla (91)] [salida/ (/dev/shm/prueba_<script>)]" >&2; exit 2; }

arranca() {   # arranca [opciones de Flycast...]: Xvfb (si no esta) y Flycast con la consola serie en $SAL/flycast.txt
    [ -f "$GDI" ] || uso
    mkdir -p "$SAL"; rm -f "$SAL"/*.png
    export DISPLAY=:$PANTALLA
    if ! xdpyinfo > /dev/null 2>&1 && ! xdotool getmouselocation > /dev/null 2>&1; then
        Xvfb :$PANTALLA -screen 0 1024x768x24 -nolisten tcp > "$SAL/xvfb.txt" 2>&1 & XVFB=$!
        sleep 2
    fi
    # configuracion y VMU nuevas en cada prueba (guardado vacio, como una consola recien encendida)
    rm -rf "$SAL/cfg" "$SAL/dat"; mkdir -p "$SAL/cfg" "$SAL/dat"
    XDG_CONFIG_HOME="$SAL/cfg" XDG_DATA_HOME="$SAL/dat" "$FLYCAST" -config config:Debug.SerialConsoleEnabled=yes \
        -config audio:backend=null "$@" "$GDI" > "$SAL/flycast.txt" 2>&1 & FLY=$!
    trap termina EXIT
    xdotool search --sync --name Flycast > /dev/null
}

termina() {
    kill $FLY 2> /dev/null || true; wait $FLY 2> /dev/null || true
    [ -n "${XVFB:-}" ] && { kill $XVFB 2> /dev/null || true; }
    if grep -a -q -i 'out of memory' "$SAL/flycast.txt"; then echo "AVISO: 'Out of memory' en $SAL/flycast.txt" >&2; ERROR=1; fi
    echo "capturas en $SAL: $(cd "$SAL" && ls *.png 2> /dev/null | tr '\n' ' ')"
    exit ${ERROR:-0}
}

espera() { sleep "$1"; kill -0 $FLY 2> /dev/null || { echo "Flycast se ha parado" >&2; ERROR=1; exit 1; }; }
tecla() { xdotool keydown "$1"; sleep 0.3; xdotool keyup "$1"; }     # una pulsacion (el mando se lee cada cuadro)
foto() { import -window root "$SAL/$1.png"; }
# espera_log patron [segundos (60)]: hasta que la consola serie escriba una linea nueva con el patron (desde la ultima
# llamada a marca); asi los pasos no dependen de lo que tarde Flycast (la pelicula, las cargas del GD)
marca() { MARCA=$(wc -l < "$SAL/flycast.txt"); }
espera_log() {
    local t=0 max=${2:-60}
    until tail -n +$((${MARCA:-0} + 1)) "$SAL/flycast.txt" | grep -a -q -E "$1"; do
        sleep 1; t=$((t + 1)); kill -0 $FLY 2> /dev/null || { echo "Flycast se ha parado" >&2; ERROR=1; exit 1; }
        [ $t -lt $max ] || { echo "no sale '$1' en $max s" >&2; ERROR=1; return 1; }
    done
    marca
}
log() { grep -a -E "$1" "$SAL/flycast.txt" | tail -${2:-3} || true; }
