/* Juego de OUTRUN2SP ([0x59acac] = 0, 1b4878 = 0) fuera del frontal de recreativa (SLES-53998):

   - Pantalla de carga (0x1b8228, PC 499CC0): START, al acabar de leer el curso (0x1b64ac), fuera de la consola y de
     las variantes 3..5 (red; tabla 0x3f9720), crea la escena del preset del banco 0x2F (LOADING_CVT) en la capa 2,
     modo 0 (en bucle), cuadros 0..0x77: preset 0 -> 2F0002, 1 -> 2F0003, 2 -> 2F0000, 3 -> 2F0001. El original la
     anima mientras START sigue cargando (los estados 3..59 de START). La demo carga de un tiron (GD, VRAM y AICA en
     el mismo paso): la escena se dibuja entre los pasos de la carga con el cuadro del reloj de la logica (60000 / 1001
     por segundo, como el bucle 0x1078c8) y la pantalla se queda con el ultimo cuadro mientras dura cada paso. Sus
     texturas salen de la VRAM antes de reservar la zona del curso.
   Graficos: CARGASPn.DAT, uno por preset (tools/conv_sp.py), abierto parcial (solo las tablas en la RAM) y cerrado al
   acabar; sus tablas y los sectores de la lectura de sus texturas en la RAM prestada (el hueco de la union) para no dejar
   nada en el monton bajo lo que START reserva despues (solo las piezas compactas de su grupo, ~8 KB). */
#include <kos.h>
#include <stdio.h>
#include "sp.h"
#include "../comun/hud.h"

static hud_dat_t *cd;                                   /* CARGASP.DAT mientras se carga */
static int u_carga = -1;
static uint64_t t0_carga;

#define SP_TABLAS (128 * 1024)                          /* para las tablas; el resto, sectores de la lectura */
void sp_carga_entra(int variante, int preset, void *ram, uint32_t cap)
{
    char n[16], f[16]; int r;
    sp_carga_sale();
    if (variante >= 3 && variante <= 5) return;         /* 0x3f9720: sin pantalla en la red */
    if (preset < 0 || preset > 3) return;
    if (cap < SP_TABLAS + 64 * 1024) ram = NULL;
    sprintf(f, "CARGASP%d.DAT", preset);
    if (ram) hudd_tablas_en(ram, SP_TABLAS);
    cd = hudd_abre_gd(f);
    hudd_tablas_en(NULL, 0);
    if (!cd) { dbglog(DBG_ERROR, "CARGA: no se pudo abrir %s\n", f); return; }
    sprintf(n, "CARGA_P%d", preset);
    u_carga = hudd_uso(cd, n);
    if (ram) hudd_bufer((uint8_t *)ram + SP_TABLAS, cap - SP_TABLAS);
    r = hudd_sube_uso(cd, u_carga);
    if (ram) hudd_bufer(NULL, 0);
    if (u_carga < 0 || r) {
        dbglog(DBG_ERROR, "CARGA: sin %s o sin VRAM\n", n);
        sp_carga_sale(); return;
    }
    t0_carga = timer_us_gettime64();
    dbglog(DBG_INFO, "CARGA: escena del preset %d (%s, tablas %s), VRAM libre %uK\n", preset, f, ram ? "prestadas" : "en el monton",
           (unsigned)(pvr_mem_available() >> 10));
    sp_carga_cuadro();
}

void sp_carga_cuadro(void)
{
    int f;
    if (!cd) return;
    /* pasos de logica desde que se creo (60000 / 1001 por segundo), modo 0: en bucle en 0..0x77 */
    f = (int)((timer_us_gettime64() - t0_carga) * 60 / 1001000) % 0x78;
    if (pvr_wait_ready() < 0) return;
    pvr_set_bg_color(0, 0, 0);
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    hudd_dibuja(cd, u_carga, f, 0, 0, 1, 100, 0);
    pvr_list_finish();
    pvr_scene_finish();
}

void sp_carga_sale(void)
{
    if (!cd) return;
    pvr_wait_ready();                                   /* la PowerVR ya no lee sus texturas */
    hudd_cierra(cd); cd = NULL; u_carga = -1;
    hudd_vacia();
}
