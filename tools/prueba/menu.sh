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
tecla x; espera 5; foto ${P}_5                                       # un jugador
tecla x; espera 6; foto ${P}_6                                       # coche
tecla x; espera 5; foto ${P}_7                                       # cambio
tecla x; espera 6; foto ${P}_8                                       # musica
tecla x; espera_log 'VRAM: zona del curso' 90; espera 15     # carga y salida de la carrera
