#!/usr/bin/env bash
# regenera_datos.sh <iso> <datos/> [trabajo/]: rehace desde tu ISO (OutRun 2006 C2C, PS2 PAL SLES-53998 v1.02) la carpeta
# de datos entera de la demo (la que se pasa a mkgdi.py --datos) con los conversores del repositorio:
#   etapas SEGnn/COLnn de todos los arboles (modo OutRun, OUTRUN2SP y las 94 misiones de Races.bin), uniones BKnn
#   empaquetadas (UNIONES.PAK + descriptores, pak_union.py; las etapas en SEGS.PAK igual), BKD, CURSO, SOL, COLBK*, COLLR, musica MUSn.ADP (28 temas,
#   28 avances, menu y metas), peliculas MVTS/MVATTR, sonido SFX/VOCES (espanol), menus, HUD, VMU, efectos, personajes (CHRPAR y una CHRppaa por pareja),
#   meta, salida, fin de carrera FIN/GOVER, OUTRUN2SP CARGASP0-3/GOVSP/SPFE/RUTASP, modo de video, INFARTO/HAHUD/VOCESH, objetos de las uniones BO, trafico, coches COCHE00-14 + COCHES y PARAM.
# Lo que sale del disco no va al repositorio. Idempotente: lo que ya esta en datos/ (o en trabajo/) no se rehace; si se
# corta, se vuelve a llamar y sigue. En paralelo (JOBS, por defecto nproc). Sale con error si falla cualquier paso.
# trabajo/ (por defecto /dev/shm/or2_regenera): intermedios (uniones sueltas, ~350 MB) y registros de cada paso.
# Tiempo: unas horas en 4 nucleos (la visibilidad PVS de cada etapa y union es lo que mas tarda).
set -euo pipefail

TOOLS=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
YO="$TOOLS/$(basename "${BASH_SOURCE[0]}")"
ARBOLES=1$(printf ',%d' $(seq 100 193))                  # OUTRUN2SP y los cursos de Races.bin (100 + registro)
MUSICA="$(seq 0 28) 30 $(seq 33 60) 61 62 63 65 66"      # temas, resultados de OUTRUN2SP (28), menu, avances y metas (sin la 64: no la usa la meta)
PARTES=4                                                 # conv_union.py --parte=k/n

# ---------------------------------------------------------------- un paso (lo llama xargs: regenera_datos.sh --paso n)
paso() {
    local n=$1 t="$W/tmp/$1"; rm -rf "$t"; mkdir -p "$t"
    case $n in curso|video_*) export PYTHON_CPU_COUNT=2;; esac   # (los dos pasos largos que no se reparten)
    mueve() { for f in "$@"; do mv -f "$f" "$D/"; done; }      # a datos/ solo cuando el paso ha acabado bien
    case $n in
    union*)   python3 "$TOOLS/conv_union.py" "$ISO" "$W/u" --arboles=$ARBOLES --parte=${n#union}/$PARTES ;;
    curso)    python3 "$TOOLS/conv_union.py" "$ISO" "$t" --sin-tramos     # BK00..14, BKD, SOL, COLBK*, CURSO del modo
              mkdir -p "$W/u0"; mv -f "$t"/* "$W/u0/"; touch "$W/u0/.hecho" ;;
    seg*)     python3 - "$ISO" "$t" "${n#seg}" <<'EOF'
import sys, os; sys.path.insert(0, os.environ['TOOLS'])
import conv_union as U
from ps2.assets import Assets
A = Assets(sys.argv[1]); U.etapa_ficheros(A, int(sys.argv[3]), sys.argv[2])
EOF
              mueve "$t"/SEG*.DAT "$t"/COL*.DAT ;;
    mus*)     local k=${n#mus}
              python3 - "$ISO" "$t/in.ps2" "$k" <<'EOF'
import sys, os; sys.path.insert(0, os.environ['TOOLS'])
from ps2.assets import Assets
A = Assets(sys.argv[1]); open(sys.argv[2], 'wb').write(A.datos(A.entrada('MUSIC.PS2', '\\%02d.ps2' % int(sys.argv[3]))))
EOF
              "$W/musica" "$t/in.ps2" "$t/MUS$k.ADP" - 0 0x2000; mueve "$t/MUS$k.ADP" ;;
    video_ts|video_attr) python3 "$TOOLS/conv_video.py" "$ISO" "$t" --hz 50 --solo ${n#video_}; mueve "$t"/*.VQV ;;
    sonido)   python3 "$TOOLS/conv_sonido.py" "$ISO" "$t" --idioma SPANISH; mueve "$t/SFX.DAT" "$t/VOCES.DAT" ;;
    menu)     python3 "$TOOLS/conv_menu.py" "$ISO" "$t" "$W/previas/menu"; mueve "$t/MENU.DAT" ;;
    hud)      python3 "$TOOLS/conv_hud.py" "$ISO" "$t" "$W/previas/hud" "$W/previas/hud"; mueve "$t/HUD.DAT" ;;
    vmu)      python3 "$TOOLS/conv_vmu.py" "$ISO" "$t" "$W/previas/vmu"; mueve "$t/VMU.DAT" ;;
    modov)    python3 "$TOOLS/conv_modo_video.py" "$ISO" "$t" "$W/previas/modov"; mueve "$t/MODOV.DAT" ;;
    efectos)  python3 "$TOOLS/conv_efectos.py" "$ISO" "$t/EFECTOS.DAT"; mueve "$t/EFECTOS.DAT" ;;
    chr)      python3 "$TOOLS/conv_chr.py" "$ISO" "$t" --pares; mueve "$t"/CHR*.DAT ;;   # CHRPAR y una CHRppaa por pareja
    band)     python3 "$TOOLS/conv_chr.py" "$ISO" "$t/BAND.DAT" --banderero; mueve "$t/BAND.DAT"
              python3 "$TOOLS/conv_chr.py" "$ISO" "$t/BAND1.DAT" --banderero-sp; mueve "$t/BAND1.DAT" ;;
    param)    python3 "$TOOLS/conv_param.py" "$ISO" "$t/PARAM.DAT"; mueve "$t/PARAM.DAT" ;;
    meta)     python3 "$TOOLS/conv_meta.py" "$ISO" "$t"; mueve "$t/META.DAT" ;;
    salida)   python3 "$TOOLS/conv_salida.py" "$ISO" "$t"; mueve "$t/SALIDA.DAT" ;;
    infarto)  python3 "$TOOLS/conv_infarto.py" "$ISO" "$t"; mueve "$t/INFARTO.DAT" "$t/HAHUD.DAT" "$t/VOCESH.DAT" ;;
    fin)      python3 "$TOOLS/conv_fin.py" "$ISO" "$t" "$W/previas/fin"; mueve "$t/FIN.DAT" "$t/GOVER.DAT" ;;
    sp)       python3 "$TOOLS/conv_sp.py" "$ISO" "$t" "$W/previas/sp"; mueve "$t"/CARGASP?.DAT "$t/GOVSP.DAT" "$t/SPFE.DAT" "$t/RUTASP.DAT" ;;
    bo)       python3 "$TOOLS/conv_bo.py" "$ISO" "$t"; mueve "$t"/BO*.DAT ;;
    trafico)  python3 "$TOOLS/conv_trafico.py" "$ISO" "$t"; mueve "$t"/TRAF*.DAT "$t"/TAPD.DAT ;;
    coche*)   python3 "$TOOLS/conv_coche.py" "$ISO" --todos "$t" --fichas ${n#coche}
              mkdir -p "$W/coche"; mv -f "$t"/COCHE[0-9]*.DAT "$W/coche/" ;;
    *)        echo "paso desconocido: $n" >&2; return 1 ;;
    esac
    rm -rf "$t"
}

if [ "${1:-}" = "--paso" ]; then
    n=$2; mkdir -p "$W/registro"
    if paso "$n" > "$W/registro/$n.txt" 2>&1; then echo "  hecho $n"; else echo "  FALLO $n (ver $W/registro/$n.txt)" >&2; exit 1; fi
    exit 0
fi

# ---------------------------------------------------------------- principal
[ $# -ge 2 ] || { echo "uso: $0 <iso> <datos/> [trabajo/]" >&2; exit 2; }
ISO=$(readlink -f "$1"); D=$(mkdir -p "$2" && cd "$2" && pwd); W=${3:-/dev/shm/or2_regenera}
mkdir -p "$W"; W=$(cd "$W" && pwd)
[ -f "$ISO" ] || { echo "no esta la ISO: $ISO" >&2; exit 2; }
JOBS=${JOBS:-4}                                          # pasos a la vez (cada uno con un hilo)
export ISO D W TOOLS OR2_ISO="$ISO" TMPDIR=${TMPDIR:-/dev/shm} PYTHONDONTWRITEBYTECODE=1
# un hilo por paso: numpy / scipy (OMP, OpenBLAS, MKL) y os.cpu_count() de Python 3.13 (hilos de pvs.py, Pool de conv_video.py)
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1 PYTHON_CPU_COUNT=1
mkdir -p "$W/u" "$W/tmp" "$W/previas" "$W/registro"
t0=$(date +%s)

echo "== preparacion (nombres de los paquetes, pvs.c, musica.c)"
python3 - <<'EOF'
import sys, os; sys.path.insert(0, os.environ['TOOLS'])
from ps2.assets import Assets
import pvs
A = Assets(os.environ['ISO'])
A.resolver(); pvs.lib()
EOF
cc -O2 -w -o "$W/musica" "$TOOLS/c/musica.c"

# lista de pasos que faltan (lo hecho no se repite)
falta=()
hay() { for f in "$@"; do [ -s "$D/$f" ] || return 1; done; }
UNIONES="UNIONES.PAK CURSO.DAT COLLR.DAT COLBK.DAT BKD.DAT SOL.DAT"
if ! hay $UNIONES; then                                  # (conv_union.py no rehace las uniones sueltas que ya estan)
    [ -f "$W/u0/.hecho" ] || falta+=(curso)
    for k in $(seq 0 $((PARTES - 1))); do falta+=(union$k); done
fi
# primero lo que hace falta para arrancar y correr (coches, HUD, menus, sonido, musica, etapas); las peliculas al final
for k in $(seq 0 14); do hay COCHES.DAT || [ -s "$W/coche/$(printf COCHE%02d.DAT $k)" ] || falta+=(coche$k); done
hay MENU.DAT || falta+=(menu); hay HUD.DAT || falta+=(hud); hay VMU.DAT || falta+=(vmu); hay MODOV.DAT || falta+=(modov)
hay SFX.DAT VOCES.DAT || falta+=(sonido)
for k in $MUSICA; do hay MUS$k.ADP || falta+=(mus$k); done
for ti in $(python3 - <<'EOF'
import sys, os; sys.path.insert(0, os.environ['TOOLS'])
import conv_union as U
from ps2.assets import Assets
A = Assets(os.environ['ISO']); E = A.elf; base, nprim, base2 = U.tablas(A)
ts = {r[1] for r in U.arbol(A, 0)} | {r[1] for r in U.arbol(A, 0, 'csc_data_cvt.bin')}
for i in range(94): ts |= {r[1] for r in U.arbol_mision(A, i)}
for ti in sorted(ts): print('%d:%d' % (ti, U.segmento_de(A, U.cadena(E, U.u32(E, base + 4 * ti) + 0x0c))))
EOF
); do n=${ti#*:}; hay $(printf 'SEG%02d.DAT COL%02d.DAT' $n $n) || falta+=(seg${ti%:*}); done
CHRS=$(python3 "$TOOLS/conv_chr.py" "$ISO" --lista)        # CHRPAR.DAT y las parejas del piloto y la acompanante
hay EFECTOS.DAT || falta+=(efectos); hay $CHRS || falta+=(chr); hay BAND.DAT BAND1.DAT || falta+=(band); hay PARAM.DAT || falta+=(param)
hay META.DAT || falta+=(meta); hay FIN.DAT GOVER.DAT || falta+=(fin); hay CARGASP0.DAT CARGASP1.DAT CARGASP2.DAT CARGASP3.DAT GOVSP.DAT SPFE.DAT RUTASP.DAT || falta+=(sp); hay SALIDA.DAT || falta+=(salida); hay INFARTO.DAT HAHUD.DAT VOCESH.DAT || falta+=(infarto)
hay BO.DAT || falta+=(bo); hay TRAFC.DAT TAPD.DAT TRAF0.DAT || falta+=(trafico)
for v in ts attr; do f=MVTS.VQV; [ $v = attr ] && f=MVATTR.VQV; hay $f || falta+=(video_$v); done

echo "== ${#falta[@]} pasos en $JOBS a la vez (registros en $W/registro)"
printf '%s\n' "${falta[@]}" | xargs -P "$JOBS" -I{} "$YO" --paso {} || { echo "fallo algun paso" >&2; exit 1; }

echo "== uniones: UNIONES.PAK y descriptores (pak_union.py)"
if ! hay $UNIONES; then
    rm -rf "$W/bk" "$W/pak"; mkdir -p "$W/bk" "$W/pak"
    ln -f "$W"/u0/BK[0-9]*.DAT "$W"/u/BK[0-9]*.DAT "$W/bk/"
    PYTHON_CPU_COUNT=$JOBS python3 "$TOOLS/pak_union.py" "$W/bk" "$W/pak" > "$W/registro/pak.txt" 2>&1 || { echo "FALLO pak_union (ver $W/registro/pak.txt)" >&2; exit 1; }
    tail -3 "$W/registro/pak.txt"
    cp -f "$W"/u0/COLBK*.DAT "$W"/u/COLBK*.DAT "$W/u/COLLR.DAT" "$D/"
    mv -f "$W"/pak/* "$D/"; cp -f "$W/u0/BKD.DAT" "$W/u0/SOL.DAT" "$W/u/CURSO.DAT" "$D/"   # (CURSO.DAT: el de todos los arboles)
fi
echo "== etapas: SEGS.PAK y descriptores (pak_union.py --prefijo=SEG; tope = HUECO_ETAPA_MAX de demo/main.c)"
if ! hay SEGS.PAK; then
    rm -rf "$W/seg" "$W/segpak"; mkdir -p "$W/seg" "$W/segpak"
    ln -f "$D"/SEG[0-9]*.DAT "$W/seg/"
    python3 "$TOOLS/pak_union.py" "$W/seg" "$W/segpak" --pak=SEGS.PAK --prefijo=SEG --tope=7842 > "$W/registro/segpak.txt" 2>&1 || { echo "FALLO pak_union de las etapas (ver $W/registro/segpak.txt)" >&2; exit 1; }
    tail -1 "$W/registro/segpak.txt"
    mv -f "$W"/segpak/* "$D/"
fi
if ! hay COCHES.DAT; then
    mv -f "$W"/coche/COCHE[0-9]*.DAT "$D/" 2>/dev/null || true
    python3 "$TOOLS/conv_coche.py" "$ISO" --todos "$D" --fichas 99 > "$W/registro/coches.txt" 2>&1   # solo COCHES.DAT
fi

# ---------------------------------------------------------------- comprobacion
echo "== comprobacion de $D"
mal=0
nec="BAND.DAT BAND1.DAT BKD.DAT BO.DAT $CHRS COCHES.DAT COLBK.DAT COLLR.DAT CURSO.DAT EFECTOS.DAT FIN.DAT GOVER.DAT HAHUD.DAT HUD.DAT INFARTO.DAT MENU.DAT
     META.DAT MODOV.DAT MVATTR.VQV MVTS.VQV PARAM.DAT SFX.DAT SEGS.PAK SOL.DAT TAPD.DAT TRAFC.DAT UNIONES.PAK VMU.DAT VOCES.DAT VOCESH.DAT"
for k in $(seq 0 14); do nec+=" $(printf COCHE%02d.DAT $k)"; done
for k in $MUSICA; do nec+=" MUS$k.ADP"; done
for f in $nec; do [ -s "$D/$f" ] || { echo "FALTA $f" >&2; mal=1; }; done
nseg=$(ls "$D" | grep -c '^SEG[0-9]*\.DAT$' || true); nbk=$(ls "$D" | grep -c '^BK[0-9]*\.DAT$' || true)
[ "$nseg" -eq 62 ] || { echo "SEG: $nseg etapas (se esperan 62)" >&2; mal=1; }
[ "$nbk" -eq 262 ] || { echo "BK: $nbk uniones (se esperan 262)" >&2; mal=1; }
for f in "$D"/BK[0-9]*.DAT; do [ "$(head -c4 "$f")" = PAKD ] || { echo "$(basename "$f") no es un descriptor PAKD" >&2; mal=1; }; done
(cd "$D" && ls -l | awk 'NR > 1 { g = $9; sub(/[0-9]*\.[A-Z]+$/, "", g); n[g]++; b[g] += $5 }
    END { for (g in n) printf "  %-10s %4d ficheros %10.1f MiB\n", g, n[g], b[g] / 1048576 }' | sort)
n=$(ls "$D" | wc -l); b=$(du -sb "$D" | cut -f1)
printf '== %d ficheros, %d bytes (%.1f MiB); SEG %d, BK %d; %d s\n' "$n" "$b" "$(echo "$b / 1048576" | bc -l)" "$nseg" "$nbk" $(( $(date +%s) - t0 ))
[ $mal -eq 0 ] || { echo "datos incompletos" >&2; exit 1; }
