#!/usr/bin/env bash
# regenera_datos.sh <iso> <datos/> [trabajo/]: rehace desde tu ISO (OutRun 2006 C2C, PS2 PAL SLES-53998 v1.02) la carpeta
# de datos entera de la demo (la que se pasa a mkgdi.py --datos) con los conversores del repositorio:
#   etapas SEGnn/COLnn de todos los arboles (modo OutRun, OUTRUN2SP y las 94 misiones de Races.bin), uniones BKnn
#   empaquetadas (UNIONES.PAK + descriptores, pak_union.py; las etapas en SEGS.PAK igual), BKD, CURSO, SOL, COLBK*, COLLR, musica MUSn.ADP (28 temas,
#   28 avances, menu y metas), peliculas MVTS/MVATTR, sonido SFX/VOCES (espanol), menus, HUD, VMU, efectos, personajes (CHRPAR y una CHRppaa por pareja),
#   meta, salida, fin de carrera FIN/GOVER, OUTRUN2SP CARGASP0-3/GOVSP/SPFE/RUTASP/RANKSP/NOM5A-E/NOMTX, el final de recreativa FINAL/FINE0-9/FINPROP/STAFF (conv_final.py), modo de video, INFARTO/HAHUD/VOCESH, objetos de las uniones BO, trafico, camiones TRAFK0-2 (OUTRUN2SP), coches COCHE00-14 + COCHES y PARAM,
#   misiones de COAST 2 COAST (MISION.DAT, conv_mision.py), sus coches de carreras RC00-29 (conv_trafico.py --rivales) y
#   sus pantallas del menu (C2C.DAT, conv_c2c.py: categorias, mapa y detalle; datos de antes sin el: solo hace ese paso).
#   El SALON (showroom: SALON.DAT, conv_salon.py; datos de antes del 2026-11-13 sin el: solo hace ese paso).
#   Los tableros de cursos de INFARTO / CONTRARRELOJ (TABLERO.DAT, conv_tablero.py) y los cursos de la Contrarreloj
#   (CURSOTA.DAT con los arboles 200 + codigo y sus uniones nuevas BK262.. en UNIONESTA.PAK, conv_union.py --curso=
#   CURSOTA.DAT --bk-desde=262; ~2 h en un nucleo por cada 60 uniones): datos de antes sin ellos, solo hace esos pasos.
# Datos de antes del 2026-11-13 sin MISION.DAT ni RC00-29.DAT: basta volver a llamarlo (solo hace esos pasos).
# Datos sin TRAFK0-2.DAT (los camiones de OUTRUN2SP, conv_trafico.py --camiones): lo mismo, solo hace ese paso.
# Segunda pasada de la VQ (tools/revq.py, vq.UMBRAL2) en las etapas y uniones antes de empaquetarlas y en los modelos
# sueltos (salvo los coches): con los conversores de ahora no tiene nada que hacer; con unos datos de antes del
# 2026-11-12 basta borrar SEGS.PAK y UNIONES.PAK con las SEGnn.DAT enteras en datos/ y las uniones sueltas en trabajo/u0, u/.
# Coches de antes del 2026-11-12 por la noche (sombra plana P_SOMBRA con su CLAMP y volumenes de sombra en trozos
# convexos, conv_coche.py): borrar COCHES.DAT y COCHE00-14.DAT de datos/ y trabajo/coche/.
# CURSO.DAT de antes del 2026-11-13 (registros de 680 bytes; la demo pide 684, con las posiciones +0x72 / +0x74 de la
# union, conv_union.py union_pos): basta rehacer solo ese fichero con
#   python3 tools/conv_union.py <iso> <dir> --arboles=1,100,...,193 --sin-uniones --parte=0/1000   (y copiar <dir>/CURSO.DAT)
# PVS de antes del 2026-11-13 (secciones llenas en las puntas de la ruta, tools/pvs.py): basta pasar tools/pvs.py a las
# SEGnn.DAT enteras (y a las BKkk.DAT sueltas, con --ruta BKD.DAT en las bifurcaciones) y empaquetarlas de nuevo (borrar
# SEGS.PAK / UNIONES.PAK de datos/); con los conversores de ahora ya salen asi.
# MENU.DAT de antes de la revision 42 (barra de ayuda con FRIENDS y los iconos grises del PS2; sin los textos de guardar
# con Y): borrar MENU.DAT de datos/ y volver a llamarlo (solo hace ese paso, conv_menu.py, ~1 min); los mismos iconos en
# FIN.DAT, GOVER.DAT, FINC2C.DAT y MODOV.DAT: borrarlos tambien (conv_fin.py, conv_modo_video.py; mismos tamanos).
# Ahora lo hace tools/regenera.py (Python, el mismo en Linux y en el programa de Windows de windows/: mismos pasos y
# salidas); este script solo lo llama con los mismos argumentos (JOBS: pasos a la vez, por defecto 4).
# Lo que sale del disco no va al repositorio. Idempotente: lo que ya esta en datos/ (o en trabajo/) no se rehace; si se
# corta, se vuelve a llamar y sigue. En paralelo (JOBS, por defecto 4). Sale con error si falla cualquier paso.
# trabajo/ (por defecto /dev/shm/or2_regenera): intermedios (uniones sueltas, ~350 MB) y registros de cada paso.
# Tiempo: unas horas en 4 nucleos (la visibilidad PVS de cada etapa y union es lo que mas tarda).
set -euo pipefail
[ $# -ge 2 ] || { echo "uso: $0 <iso> <datos/> [trabajo/]" >&2; exit 2; }
export TMPDIR=${TMPDIR:-/dev/shm}
exec python3 "$(dirname "${BASH_SOURCE[0]}")/regenera.py" "$@" --jobs "${JOBS:-4}"
