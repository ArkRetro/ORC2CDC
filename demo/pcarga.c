/* Pantalla de carga al empezar un modo (ver pcarga.h). DESVIACION pedida por el usuario (demo 41, docs/PARIDAD.md,
   Menu principal sin MULTIPLAYER y pantallas de carga):
   - OUTRUN2SP: la escena del mapa del preset (0x1b8228, banco 0x2F, demo/sp.c), que el original crea al acabar de
     leer el curso en START y se ve mientras START sigue cargando. Aqui desde que se sale del frontal, con toda la carga
     de la carrera por debajo (voces, coche, pareja, HUD, etapa y sus texturas) y como poco PCARGA_MINIMO.
   - Los modos del juego de la consola (OutRun, Heart Attack, Time Attack, misiones de Coast 2 Coast): una pantalla
     propia con lo del titulo de MENU.DAT y una barra (comun/menu.c, menu_carga_*). El original pone ahi el icono de
     carga 0x480000 (modos 28 / 30, banco 0x48 SUMO_LOADING, sin convertir).
   La carga no cambia de orden ni de sitio (nada en la RAM fija: las tablas del mapa en el final del hueco de la union,
   prestado como el del frontal de recreativa; la propia solo usa lo que MENU.DAT ya tiene en la RAM). Sus texturas
   salen mientras se sube lo que se queda en la VRAM (pcarga_pausa / pcarga_sigue) y antes de reservar la zona del
   curso: la VRAM de la carrera sale como sin pantalla. La escena se dibuja entre los pasos de la carga y tras cada
   lectura del GD del hilo del juego (gd_latido), como mucho un cuadro cada 16 ms; lo que tarda en dibujarse no es
   tiempo de la carrera (el reloj de la logica empieza al acabar). Mientras esta en pausa y despues de quitarla (la
   zona del curso, el banderero, la salida) se ve su ultimo cuadro. */
#include <kos.h>
#include "pcarga.h"
#include "sp.h"
#include "../comun/gd.h"
#include "../comun/hud.h"
#include "../comun/menu.h"
#include "../comun/musica.h"

static int tipo = PCARGA_NADA, dentro, pausa;
static uint64_t t0, t_ult; static uint32_t t_min; static float avance;

int pcarga_tipo(int variante, int juego_sp)
{
    if (juego_sp) return variante >= 3 && variante <= 5 ? PCARGA_NADA : PCARGA_MAPA_SP;   /* 0x3f9720: sin la red */
    return PCARGA_PROPIA;                       /* 1, 2, 7 y 6 (Coast 2 Coast: las misiones) */
}
int pcarga_activa(void) { return tipo != PCARGA_NADA; }

static void dibuja(void)
{
    if (pvr_wait_ready() < 0) return;
    pvr_set_bg_color(tipo == PCARGA_PROPIA ? 1.0f : 0, tipo == PCARGA_PROPIA ? 1.0f : 0, tipo == PCARGA_PROPIA ? 1.0f : 0);
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    if (tipo == PCARGA_MAPA_SP) sp_carga_dibuja();
    else {                                      /* la barra: lo cargado, sin pasar de la parte del minimo ya cumplida (la
                                                   pantalla se va cuando las dos estan enteras) */
        float f = avance, m = t_min ? (float)(timer_us_gettime64() - t0) / (t_min * 1000.0f) : 1.0f;
        menu_carga_dibuja(f < m ? f : m);
    }
    pvr_list_finish();
    pvr_scene_finish();
    t_ult = timer_us_gettime64();
}
void pcarga_cuadro(void)
{
    if (tipo == PCARGA_NADA || pausa || dentro || timer_us_gettime64() - t_ult < 16000) return;
    dentro = 1;
    musica_poll();                              /* (el bucle no llama mientras dura la carga) */
    dibuja();
    dentro = 0;
}
static void latido(void) { pcarga_cuadro(); }

void pcarga_entra(int t, int variante, int preset, void *ram, uint32_t cap, uint32_t minimo)
{
    int r;
    pcarga_suelta();
    if (t == PCARGA_NADA) return;
    hudd_vacia();                               /* (lo que el menu solto: fuera ya) */
    r = t == PCARGA_MAPA_SP ? sp_carga_entra(variante, preset, ram, cap) : menu_carga_entra();
    if (r) { dbglog(DBG_ERROR, "PCARGA: sin la pantalla de carga %d (%d)\n", t, r); return; }
    tipo = t; pausa = 0; t_min = minimo; avance = 0; t_ult = 0; t0 = timer_us_gettime64();
    gd_latido = latido;
    dbglog(DBG_INFO, "PCARGA: pantalla de carga %s (minimo %u ms), VRAM libre %uK\n",
           t == PCARGA_MAPA_SP ? "del mapa (OUTRUN2SP)" : "propia", (unsigned)minimo, (unsigned)(pvr_mem_available() >> 10));
    dibuja();
}
/* Lo que se queda en la VRAM para la carrera (coche, pareja y su orden, HUD, coches del rival) se sube sin la pantalla:
   entre pcarga_pausa y pcarga_sigue sus texturas estan fuera y se ve su ultimo cuadro (la imagen se queda en la
   pantalla), asi la VRAM de la carrera queda como sin ella (con sus texturas puestas antes, el dlmalloc de pvr_mem
   ponia lo de despues en otros huecos y al quitarlas la zona del curso de OUTRUN2SP bajaba de 1344 a 1312 KB); vuelve a
   subirlas (64..110 KB del GD, con el hueco de la union aun prestado) para la etapa */
void pcarga_pausa(void)
{
    if (tipo == PCARGA_NADA || pausa) return;
    dibuja();
    pvr_wait_ready();                           /* (la PowerVR ya no lee sus texturas) */
    if (tipo == PCARGA_MAPA_SP) sp_carga_baja(); else menu_carga_baja();
    hudd_vacia();
    pausa = 1;
}
void pcarga_sigue(void)
{
    if (tipo == PCARGA_NADA || !pausa) return;
    if (tipo == PCARGA_MAPA_SP ? sp_carga_sube() : menu_carga_sube()) return;   /* (sin VRAM: el ultimo cuadro) */
    pausa = 0;
    dibuja();
}
void pcarga_avance(float f)
{
    if (f > avance) avance = f > 1 ? 1 : f;
    pcarga_cuadro();
}
void pcarga_suelta(void)
{
    uint64_t t;
    if (tipo == PCARGA_NADA) return;
    gd_latido = NULL;
    avance = 1;
    while ((t = timer_us_gettime64()) - t0 < (uint64_t)t_min * 1000) {   /* el minimo: la escena sigue (o su ultimo
                                                                            cuadro, si sigue sin sus texturas) */
        if (t - t_ult >= 16000) { musica_poll(); if (!pausa) dibuja(); else t_ult = t; } else thd_sleep(2);
    }
    if (!pausa) dibuja();                       /* el ultimo cuadro se queda en la pantalla hasta la carrera */
    dbglog(DBG_INFO, "PCARGA: fuera tras %u ms\n", (unsigned)((timer_us_gettime64() - t0) / 1000));
    if (tipo == PCARGA_MAPA_SP) sp_carga_sale(); else menu_carga_sale();
    tipo = PCARGA_NADA; pausa = 0;
}
