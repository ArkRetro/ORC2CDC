# menu.sh: el recorrido de nav.sh desde la pelicula de inicio hasta la carrera (lo cargan nav.sh, nav3.sh y pal.sh;
# P = prefijo de las capturas)
P=${P:-nv}
# (con todos los arboles en CURSO.DAT, curso_huecos tarda minutos en Flycast: cada gd_busca relee el directorio)
espera_log 'VIDEO MVTS.VQV: precarga' 900; espera 3; foto ${P}_1     # pelicula de inicio
tecla Return; espera_log 'MVTS.VQV: (saltado|fin)' 60; espera 4; foto ${P}_2   # titulo
tecla Return; espera_log 'musica: MUS30' 30; espera 5; foto ${P}_3    # licencia (crear una nueva)
tecla x; espera 5                                                    # editor de la licencia: nombre de fabrica
tecla Down; espera 1; tecla Down; espera 1; tecla Down; espera 1    # LISTO
tecla x; espera 6; foto ${P}_4                                       # menu principal
if [ -n "${MENU_SP:-}" ]; then                                       # MENU_SP=1: OUTRUN2SP (uno a la derecha; sin
    tecla Right; espera 3; foto ${P}_5                               # MULTIPLAYER): el frontal de recreativa (fsp.c)
    tecla x; espera_log 'FSP: frontal' 30; espera 5; foto ${P}_6     # elige coche (el coche 3D)
    tecla x; espera 5; foto ${P}_7                                   # elige transmision
    tecla x; espera 5; foto ${P}_8                                   # elige modo de juego (OutRun, lado 0: preset 0)
    tecla v; espera 3; foto ${P}_8b                                  # gatillo R: el otro lado (preset 1, arbol 1)
    tecla x; espera 5; foto ${P}_9                                   # elige la musica
    tecla x; espera 1; foto ${P}_carga                               # la pantalla de carga (el mapa, demo/pcarga.c)
    espera_log 'VRAM: zona del curso' 90; espera ${ESPERA_CARRERA:-15}
else
tecla x; espera 5; foto ${P}_5                                       # un jugador
for _m in $(seq 1 ${MODO_JUEGO:-0}); do tecla Right; espera 1; done # MODO_JUEGO=n: n modos a la derecha (1 OUTRUN, 2 INFARTO, 3 CONTRARRELOJ)
if [ "${MODO_JUEGO:-0}" = 0 ]; then tecla x; espera 4; foto ${P}_5b; fi   # COAST 2 COAST: la ventana de las misiones (la 1-1)
tecla x; espera 6; foto ${P}_6                                       # coche
tecla x; espera 5; foto ${P}_7                                       # cambio
tecla x; espera 6; foto ${P}_8                                       # musica
tecla x; espera 1; foto ${P}_carga                                   # la pantalla de carga (demo/pcarga.c)
espera_log 'VRAM: zona del curso' 90; espera ${ESPERA_CARRERA:-15}  # carga y salida de la carrera
fi
