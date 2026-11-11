/* Rankings de OUTRUN2SP (ver rksp.h). Como el PS2 (SLES-53998):
   - Tablas en el orden de la RAM del PS2 (0x5d7548..0x5da108): variante 1 presets 0 / 1 (+0, preset x 0x320 + cuenta x
     0xA0), variante 2 (+0x640, igual), variante 0 presets 0 / 1 (+0xC80, preset x 0xC80 + cambio x 0x640 + config x 0x320
     + cuenta x 0xA0), variante 1 presets 2 / 3 (+0x2440 + preset x 0xA0) y variante 0 presets 2 / 3 (+0x21C0 + preset x
     0x280 + cambio x 0x140 + config x 0xA0): los accesos 0x1fb970 / 0x1fb9f0 / 0x1fbad0 y la insercion 0x1fae90 (PC
     4F3810 / 4F3870 / 4F3910 / 4F3010).
   - Por defecto (0x1fa3b0, al arrancar sin datos guardados): las que da esa rutina del PS2 ejecutada en la conversion
     (tools/conv_sp.py tablas_nombre: nombres AM2 / OR2 / R&D, 100000 puntos, 4'50"000 y 14'40"000).
   - Se guardan en la VMU con las licencias (comun/menu.c: el fichero de la consola guarda los datos del sistema con
     ellas, 0x106288 / 0x1063d8 en el estado 7.. de la entrada de nombre). */
#include <string.h>
#include "rksp.h"

static uint8_t tab[RKSP_TAM];
static int hay;

uint8_t *rksp_tablas(void) { return tab; }
int rksp_hay(void) { return hay; }
void rksp_defecto(const uint8_t *def) { if (!hay && def) { memcpy(tab, def, RKSP_TAM); hay = 1; } }
void rksp_pon(const uint8_t *t) { memcpy(tab, t, RKSP_TAM); hay = 1; }

uint32_t rksp_tabla(int v, int p, int c, int a, int b)
{
    if (v == 1) return p == 2 || p == 3 ? 0x2440u + p * 0xa0 : (uint32_t)(p * 0x320 + c * 0xa0);
    if (v == 2) return 0x640u + p * 0x320 + c * 0xa0;
    if (p == 2 || p == 3) return 0x21c0u + p * 0x280 + a * 0x140 + b * 0xa0;
    return 0xc80u + p * 0xc80 + a * 0x640 + b * 0x320 + c * 0xa0;
}

static uint32_t rd(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

int rksp_inserta(const uint8_t rec[16], int v, int p, int c, int a, int b)
{
    uint32_t t = rksp_tabla(v, p, c, a, b), at;
    if (t + 160 > RKSP_TAM) return -1;
    for (at = 0; at < 10; at++) {                       /* 0x1fb0..: el puesto del registro */
        const uint8_t *e = tab + t + at * 16;
        if (v == 0) { if (rd(rec + 4) < rd(e + 4)) break; }
        else if (v == 1) { if ((rd(rec) & 0xffffff) > (rd(e) & 0xffffff)) break; }
        else if ((rd(rec + 8) & 0xfff) > (rd(e + 8) & 0xfff)) break;
    }
    if (at == 10) return -1;
    memmove(tab + t + (at + 1) * 16, tab + t + at * 16, (9 - at) * 16);   /* los de detras bajan, el decimo se pierde */
    memcpy(tab + t + at * 16, rec, 16);
    return (int)at;
}
