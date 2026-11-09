/* Eleccion del modo de video del arranque, como la pantalla del original PAL (SLES-53998, descriptor 0x3BF8C8: init
   0x104E40, paso 0x105110, dibujo 0x105390; ver tools/conv_modo_video.py, que compone los graficos de MODOV.DAT).

   Logica (por paso de 60000/1001 Hz, como el bucle del original; t = +0x1898, estado = +0x1894):
   - estado 0 (elegir): t = 0 en cada paso; aceptar (cruz o START del PS2: A o START aqui) con el cursor en "50Hz"
     termina en 50 Hz; en "60Hz - Prueba" pasa a 60 Hz ([0x328A90] = 0, el bucle principal cambia el modo de video en
     el acto), estado 1 y t = -30 (30 pasos sin aceptar). El texto de ayuda sigue al cursor (0x104D70).
   - estado 1 (prueba): lista "No", "Si" con el cursor en "No"; aceptar en "Si" termina en 60 Hz; aceptar en "No" o
     t > 900 (15 s) vuelve a 50 Hz, al estado 0 con t = -30 (cursor otra vez en la primera fila).
   - arriba / abajo mueven el cursor (con vuelta) tambien con t < 0; efecto 1 al moverlo (26AC98 / 26AD08, si hay
     mas de una fila) y 0x40 al aceptar (la ventana, 0x271B70).
   - "60Hz Progressive (480P) - Test" no se ofrece: sin cable VGA la Dreamcast no saca 480p (y con VGA no se pregunta).
   Dibujo: base de la ventana y ordenes de MODOV.DAT en un bufer (mezcla alfa, colores multiplicados) que se copia a la
   memoria de video cuando cambia algo. */
#include <kos.h>
#include <string.h>
#include <stdlib.h>
#include "gd.h"
#include "sfx.h"
#include "modo_video.h"

#define PERIODO_US (1001000.0 / 60.0)
enum { G_AYUDA = 0, G_PRUEBA = 3, G_PAL = 20, G_SINO = 26, N_PAL = 2 };

static uint8_t *dat;
static uint16_t bx, by, bw, bh, *base, *buf;
static uint32_t n_fu, n_gr;

static uint16_t u16(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint32_t u32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* ordenes del grupo g sobre buf */
static void grupo(int g)
{
    const uint8_t *G = dat + u32(dat + 28) + 8 * g, *o = dat + u32(dat + 28) + 8 * n_gr + 20 * u32(G);
    uint32_t n = u32(G + 4), k;
    for (k = 0; k < n; k++, o += 20) {
        const uint8_t *F = dat + u32(dat + 20) + 12 * u16(o);
        const uint32_t *src = (const uint32_t *)(dat + u32(F + 8));
        int fw = u16(F), sx = u16(o + 2), sy = u16(o + 4), w = u16(o + 6), h = u16(o + 8);
        int x = (int16_t)u16(o + 10) - bx, y = (int16_t)u16(o + 12) - by, i, j;
        uint32_t c = u32(o + 16), ca = c >> 24, cr = (c >> 16) & 255, cg = (c >> 8) & 255, cb = c & 255;
        for (j = 0; j < h; j++) {
            if (y + j < 0 || y + j >= bh) continue;
            for (i = 0; i < w; i++) {
                uint32_t s = src[(sy + j) * fw + sx + i], a = (s >> 24) * ca / 255, r, gg, b;
                uint16_t *d = buf + (y + j) * bw + x + i, v;
                if (!a || x + i < 0 || x + i >= bw) continue;
                v = *d; r = ((v >> 11) & 31) << 3; r |= r >> 5; gg = ((v >> 5) & 63) << 2; gg |= gg >> 6;
                b = (v & 31) << 3; b |= b >> 5;
                r = (((s >> 16) & 255) * cr / 255 * a + r * (255 - a) + 127) / 255;
                gg = (((s >> 8) & 255) * cg / 255 * a + gg * (255 - a) + 127) / 255;
                b = ((s & 255) * cb / 255 * a + b * (255 - a) + 127) / 255;
                *d = (r >> 3) << 11 | (gg >> 2) << 5 | (b >> 3);
            }
        }
    }
}

static void pinta(int estado, int cursor, int ayuda)
{
    int i, n = estado ? 2 : N_PAL;
    memcpy(buf, base, bw * bh * 2);
    grupo(ayuda);
    for (i = 0; i < n; i++) grupo((estado ? G_SINO : G_PAL) + 2 * i + (i == cursor));
    vid_waitvbl();
    for (i = 0; i < bh; i++) memcpy(vram_s + (by + i) * 640 + bx, buf + i * bw, bw * 2);
}

static void pon_modo(int m60)
{
    vid_set_mode(m60 ? DM_640x480 : DM_640x480_PAL_IL, PM_RGB565);
    vid_clear(0, 0, 0);
}

static uint32_t botones(void)
{
    maple_device_t *mc = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = mc ? (cont_state_t *)maple_dev_status(mc) : NULL;
    return st ? st->buttons : 0;
}

int modo_video_pantalla(void)
{
    uint32_t tam, ant = 0, nuevo = 0;
    int estado = 0, t = 0, cursor = 0, ayuda = G_AYUDA, m60 = 0, fin = 0, ver = -1, seg;
    uint64_t t0;
    double pend = 0;
    dat = gd_carga("MODOV.DAT", &tam);
    if (!dat || tam < 32 || memcmp(dat, "MDV1", 4)) { dbglog(DBG_ERROR, "modo_video: sin MODOV.DAT\n"); free(dat); return 0; }
    bx = u16(dat + 4); by = u16(dat + 6); bw = u16(dat + 8); bh = u16(dat + 10);
    base = (uint16_t *)(dat + u32(dat + 12)); n_fu = u32(dat + 16); n_gr = u32(dat + 24);
    buf = malloc(bw * bh * 2);
    if (!buf) { free(dat); return 0; }
    sfx_inicia();                                       /* los efectos del frontal (main lo vuelve a pedir: no hace nada) */
    pon_modo(0);
    t0 = timer_us_gettime64();
    while (!fin) {
        uint32_t b = botones(), code;
        uint64_t ahora = timer_us_gettime64();
        int pasos;
        nuevo |= b & ~ant; ant = b;
        pend += (double)(ahora - t0) / PERIODO_US; t0 = ahora;
        pasos = (int)pend; pend -= pasos;
        while (pasos-- > 0 && !fin) {
            int s2 = estado;
            /* 0x284990: aceptar antes que arriba / abajo */
            code = nuevo & (CONT_A | CONT_START) ? 0 : nuevo & CONT_DPAD_UP ? 2 : nuevo & CONT_DPAD_DOWN ? 4 : 12;
            nuevo = 0;
            if (code == 0) sfx_menu(SFX_ACEPTA);
            if (++t >= 0) {
                if (estado == 0) {
                    t = 0;
                    if (code == 0) {
                        if (cursor == 0) { s2 = 3; m60 = 0; }
                        else { m60 = 1; pon_modo(1); ver = -1; s2 = 1; t = -30; }
                    }
                } else if (code == 0 && cursor == 1) { s2 = 3; m60 = 1; }
                else if (code == 0 || t > 900) { m60 = 0; pon_modo(0); ver = -1; s2 = 0; t = -30; }
            }
            if (code == 2 || code == 4) {
                int n = estado ? 2 : N_PAL;
                cursor = (cursor + (code == 2 ? n - 1 : 1)) % n;
                if (n > 1) sfx_menu(SFX_CURSOR);
                if (!estado) ayuda = G_AYUDA + cursor;
            }
            if (s2 == 3) fin = 1;
            else if (s2 != estado) { estado = s2; cursor = 0; if (!estado) ayuda = G_AYUDA; }
        }
        if (fin) break;
        if (estado) {                                   /* 0x105390: segundos que quedan */
            seg = t < 0 ? 15 : t < 901 ? (900 - t) / 60 + 1 : 0;
            ayuda = G_PRUEBA + seg;
        }
        if (ver != ((estado << 16) | (cursor << 8) | ayuda)) { ver = (estado << 16) | (cursor << 8) | ayuda; pinta(estado, cursor, ayuda); }
        else vid_waitvbl();
    }
    while (botones() & (CONT_A | CONT_START)) thd_sleep(10);    /* que no llegue al juego */
    vid_clear(0, 0, 0);
    free(buf); free(dat); dat = NULL;
    return m60;
}
