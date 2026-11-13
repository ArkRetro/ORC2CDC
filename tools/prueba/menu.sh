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
if [ "${MODO_JUEGO:-0}" = 0 ]; then                                  # COAST 2 COAST (C2C.DAT): categorias, mapa y detalle
    tecla x; espera_log 'C2C: C2C.DAT' 30; espera 4; foto ${P}_5b    # categorias (Flagman 1)
    if [ -n "${C2C_RECORRE:-}" ]; then                               # C2C_RECORRE=1: tambien los otros botones
        tecla Right; espera 2; foto ${P}_5c                          # Flagman 2 (bloqueado sin notas)
        tecla Down; espera 2; foto ${P}_5d                           # Jennifer (campeonato)
        tecla x; espera 2                                            # (bloqueado: no se entra)
        tecla Up; espera 1; tecla Left; espera 2                     # de vuelta a Flagman 1
    fi
    tecla x; espera 3; foto ${P}_5e                                  # mapa de Flagman 1 (cursor en la 1)
    if [ -n "${C2C_RECORRE:-}" ]; then tecla Right; espera 2; fi     # (el nodo 2 esta cerrado: no se mueve)
    tecla x; espera 3; foto ${P}_5f                                  # detalle: la carrera 1-1
    if [ -n "${C2C_RECORRE:-}" ]; then                               # B al mapa, B a las categorias, y otra vez
        tecla c; espera 2; tecla c; espera 3; foto ${P}_5g; tecla x; espera 3; tecla x; espera 3
    fi
fi
tecla x; espera 6; foto ${P}_6                                       # coche
tecla x; espera 5; foto ${P}_7                                       # cambio
tecla x; espera 6; foto ${P}_8                                       # musica
tecla x; espera 1; foto ${P}_carga                                   # la pantalla de carga (demo/pcarga.c)
espera_log 'VRAM: zona del curso' 90; espera ${ESPERA_CARRERA:-15}  # carga y salida de la carrera
fi
