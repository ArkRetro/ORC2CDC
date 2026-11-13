/* Graficos de la VMU: icono y eyecatch del fichero de guardado y pantalla LCD (ver vmu.h y tools/conv_vmu.py).

   LCD: maple_enum_type(i, MAPLE_FUNC_LCD) da cada unidad con pantalla (la VMU de cualquier ranura de cualquier mando).
   vmu_draw_lcd encola la escritura y vuelve sin esperar (bloqueo del marco con trylock: si la unidad esta ocupada,
   p.ej. guardando la licencia, devuelve MAPLE_EAGAIN y se reintenta en el paso siguiente). En una VMU real cada
   escritura es una trama maple de 200 bytes: se compone como mucho cada LCD_MS y solo se envia a las unidades cuya
   ultima imagen enviada es distinta (una VMU que se acaba de enchufar la recibe en el paso siguiente).
   La VMU va al reves en el mando: la imagen se compone como se ve y se gira 180 grados al enviarla (lo mismo que hace
   vmu_draw_lcd_xbm de KallistiOS). */
#include <kos.h>
#include <string.h>
#include <stdlib.h>
#include <dc/maple.h>
#include <dc/maple/vmu.h>
#include <dc/vmu_pkg.h>
#include "gd.h"
#include "vmu.h"
#include "vmuesc.h"

#define LCD_W 48
#define LCD_H 32
#define LCD_B (LCD_W / 8)
#define LCD_MS 250                          /* como mucho 4 escrituras por segundo y unidad */
#define LOGO_MS 2500                        /* cada imagen del logo */
#define N_RANURAS (MAPLE_PORT_COUNT * MAPLE_UNIT_COUNT)
enum { I_LOGO_A, I_LOGO_B, I_VEL, I_MARCHA, N_IMG };

static uint8_t *dat;                        /* VMU.DAT */
static uint32_t dat_tam;
static const uint8_t *img[N_IMG];           /* imagenes LCD: cabecera de 8 bytes y cuadros */
static int modo, l_kmh, l_marcha, l_marchas;   /* 0 nada, 1 logo, 2 carrera */
static uint64_t t_ult;                      /* ultima composicion */
static uint8_t raw[LCD_W * LCD_H / 8] __attribute__((aligned(4)));
static uint32_t enviada[N_RANURAS];         /* suma de la ultima imagen enviada a cada unidad (0: ninguna) */

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t u32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

int vmu_inicia(void)
{
    uint32_t n, i, o;
    vmuesc_letra();                                         /* letra de la pantalla de diagnostico (vmuesc.c) */
    dat = gd_carga("VMU.DAT", &dat_tam);
    if (!dat) { dbglog(DBG_INFO, "vmu: sin VMU.DAT\n"); return -1; }
    if (dat_tam < 32 || memcmp(dat, "VMU1", 4)) { free(dat); dat = NULL; return -2; }
    n = u16(dat + 10); o = u32(dat + 28);
    if (n < N_IMG || o + 4 * n > dat_tam) { free(dat); dat = NULL; return -3; }
    for (i = 0; i < N_IMG; i++) {
        uint32_t d = u32(dat + o + 4 * i);
        if (d + 8 > dat_tam) { free(dat); dat = NULL; return -4; }
        img[i] = dat + d;
    }
    dbglog(DBG_INFO, "vmu: VMU.DAT %u bytes, %u cuadros de icono, eyecatch %u\n", (unsigned)dat_tam, u16(dat + 4), u16(dat + 8));
    return 0;
}

/* ---------------------------------------------------------------- fichero de guardado */
void vmu_cabecera(vmu_pkg_t *pkg)
{
    static uint8_t liso[512];
    strcpy(pkg->desc_short, "OUTRUN 2006 C2C");
    strcpy(pkg->desc_long, "Licencia OutRun");
    strcpy(pkg->app_id, "OR2C2C");
    if (dat) {
        pkg->icon_cnt = u16(dat + 4); pkg->icon_anim_speed = u16(dat + 6);
        memcpy(pkg->icon_pal, dat + u32(dat + 12), sizeof pkg->icon_pal);
        pkg->icon_data = dat + u32(dat + 16);
        pkg->eyecatch_type = u16(dat + 8); pkg->eyecatch_data = dat + u32(dat + 20);
    } else {                                                /* sin VMU.DAT: el icono liso de antes */
        pkg->icon_cnt = 1; pkg->icon_anim_speed = 0; pkg->icon_pal[0] = 0xf000; pkg->icon_pal[1] = 0xfc00;
        memset(liso, 0x11, sizeof liso); pkg->icon_data = liso;
        pkg->eyecatch_type = VMUPKG_EC_NONE; pkg->eyecatch_data = NULL;
    }
}

/* ---------------------------------------------------------------- LCD */
void vmu_lcd_logo(void) { modo = 1; }
void vmu_lcd_carrera(int kmh, int marcha, int marchas) { modo = 2; l_kmh = kmh; l_marcha = marcha; l_marchas = marchas; }

/* pantalla como se ve: fila y de 6 bytes, bit 7 = pixel izquierdo */
static uint8_t ver[LCD_H][LCD_B];
static void pon(int x, int y) { if (x >= 0 && x < LCD_W && y >= 0 && y < LCD_H) ver[y][x >> 3] |= 0x80 >> (x & 7); }

/* cuadro c de la imagen i en (x, y) */
static void pinta(const uint8_t *im, int c, int x, int y)
{
    int w = im[2], h = im[3], bpr = (w + 7) / 8, i, j;
    const uint8_t *p;
    if (c < 0 || c >= im[4]) return;
    p = im + 8 + c * h * bpr;
    for (j = 0; j < h; j++, p += bpr)
        for (i = 0; i < w; i++) if (p[i >> 3] & (0x80 >> (i & 7))) pon(x + i, y + j);
}

static void compone(uint64_t ahora)
{
    memset(ver, 0, sizeof ver);
    if (modo == 1) {
        const uint8_t *im = img[(ahora / LOGO_MS) & 1 ? I_LOGO_B : I_LOGO_A];
        pinta(im, 0, im[0], im[1]);
    } else {
        const uint8_t *v = img[I_VEL], *m = img[I_MARCHA];
        int t = l_kmh < 0 ? 0 : l_kmh > 999 ? 999 : l_kmh, k, n = l_marchas >= 6 ? 6 : 5, x0, h = m[3];
        /* velocidad "%03d" como el HUD (hud.c) */
        pinta(v, t / 100, v[0], v[1]); pinta(v, (t / 10) % 10, v[0] + v[5], v[1]); pinta(v, t % 10, v[0] + 2 * v[5], v[1]);
        /* caja de cambios centrada; la marcha actual con un marco a 2 pixeles de la cifra (resalte MARCHA) */
        x0 = (LCD_W - n * m[5]) / 2;
        for (k = 0; k < n; k++) {
            int x = x0 + k * m[5] + m[0], y = m[1], w = m[2], i;
            pinta(m, k, x, y);
            if (k + 1 == l_marcha) {
                for (i = x - 2; i <= x + w + 1; i++) { pon(i, y - 2); pon(i, y + h + 1); }
                for (i = y - 2; i <= y + h + 1; i++) { pon(x - 2, i); pon(x + w + 1, i); }
            }
        }
    }
    /* girada 180 grados: fila 31 - y, bytes y bits al reves */
    {
        int y, b, i; uint8_t r, s;
        for (y = 0; y < LCD_H; y++)
            for (b = 0; b < LCD_B; b++) {
                s = ver[y][b]; r = 0;
                for (i = 0; i < 8; i++) if (s & (1 << i)) r |= 0x80 >> i;
                raw[(LCD_H - 1 - y) * LCD_B + (LCD_B - 1 - b)] = r;
            }
    }
}

/* mientras se guarda (comun/menu.c, vmu_escribe) no se envia nada a la LCD: la trama de la LCD y las de los bloques
   de la memoria son la misma trama maple de la unidad (dev->frame). Al pausar se espera (como mucho ~200 ms) a que
   las escrituras de LCD ya encoladas tengan respuesta, para que la VMU no reciba la primera escritura de bloque con
   una de LCD a medias; al seguir se reenvia la imagen a todas */
static int pausa;
void vmu_lcd_pausa(int p)
{
    maple_device_t *d; int i, t;
    pausa = p;
    if (p) {
        for (t = 0; t < 20; t++) {
            int ocupada = 0;
            for (i = 0; (d = maple_enum_type(i, MAPLE_FUNC_LCD)); i++)
                if (d->frame.state != MAPLE_FRAME_VACANT) ocupada = 1;
            if (!ocupada) break;
            thd_sleep(10);
        }
    } else memset(enviada, 0, sizeof enviada);
}

void vmu_lcd_paso(void)
{
    uint64_t ahora; uint32_t suma, vistas[N_RANURAS] = { 0 }; maple_device_t *d; int i, k;
    if (!dat || !modo || pausa) return;
    ahora = timer_ms_gettime64();
    if (t_ult && ahora - t_ult < LCD_MS) return;
    t_ult = ahora;
    compone(ahora);
    for (suma = 1, i = 0; i < (int)sizeof raw; i++) suma = suma * 31 + raw[i];
    if (!suma) suma = 1;
    for (i = 0; (d = maple_enum_type(i, MAPLE_FUNC_LCD)); i++) {
        k = d->port * MAPLE_UNIT_COUNT + d->unit;
        if (k < 0 || k >= N_RANURAS) continue;
        vistas[k] = 1;
        if (enviada[k] == suma) continue;
        if (vmu_draw_lcd(d, raw) == MAPLE_EOK) enviada[k] = suma;
    }
    for (k = 0; k < N_RANURAS; k++) if (!vistas[k]) enviada[k] = 0;   /* desenchufada: se le vuelve a enviar */
}
