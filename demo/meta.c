/* meta.c: la escena de la meta del original (modo 19 GOAL), con META.DAT (tools/conv_meta.py).

   Del PC (decompilacion, platform/race_end_modes.cpp 49C9C0 / 49CC50 y race_goal_camera.cpp 4871A0 / 486730 / 486EF0 /
   4874A0 / 475720), contrastado con SLES-53998: 49C9C0 = 0x1b8e98, 4871A0 = 0x1f4ef0, 486730 = 0x1f43d0 (tabla de
   saltos de los 31 op en 0x40bf70), 486EF0 = 0x1f4c08, 4874A0 = el final de 0x1f43a0..0x1f43c8.
   - 49C9C0: escena 3 o, en las variantes distintas de 4, 5, 6 y 7 con [0x359d98] == 0x42 (siempre), la del byte
     0x3f99b0 + (meta + preset x 5) x 4; meta = 450380(8) - 0xA (la clave de la etapa de meta - 10). En el modo OutRun
     (preset 0): metas A..E -> escenas 3, 33, 3, 35, 34; con el preset 1, 1..5. Musica 401000(0, pista, 0) (sin
     bucle): variante 1 tabla 0x3f9a18, 2 tabla 0x3f9a30; temporizador del modo 0x4B0 (1200 cuadros; 0x258 en la 2).
   - 4871A0: 82E7D4 = escena, 82E7C4 = 82E7C8 = 82E7DC = 82E7CC = 0, 82E7D8 = 1 (activa), 82E7E0 = 82E7E8 = 0,
     82E7E4 = 2, 82E7D0 = 1, 82E7C0 = 1; marca todos los registros del guion (+0x28 = 1); car+2F0 = (& ~0x46) | 0x38.
   - 4874A0 (suceso 7, antes del coche 8 y de la camara 385): si 82E7D8, 486730 y 486EF0; luego 82E7E0 += 1, 82E7C4 =
     (int) 82E7E0, 82E7E8 = 82E7E0 / 60 (0,016666668).
   - 486730: cada registro marcado con +0x24 <= 82E7C4 se ejecuta una vez (op <= 0x1E). Las de la escena de meta de
     SLES-53998 (las 8 escenas usan el mismo guion 0x36d8a0): 0 op 11(0) car+38 = 0 y car+1F4 = 0; op 6(1) el coche
     pasa a PasPlCar_Ctrl (475720: sin fisica, car+4 |= 0x800000); op 7(1) el coche al origen del marco de la union
     (7D3190) con sus angulos (449640); op 12(2, 0) 82E7E4 = 2 (seccion de curso que ve la camara: 487320 da 0xFFFF, la
     demo elige lo que dibuja a su manera); op 15(1) 82E7C0 = 1 (estilo del humo 41CED0); op 18(1) 2057C0(0x16A / 0x16B,
     1) (RET en PS2 y PC); op 19 / 20 planos cercano 0,5 y lejano 1900 (camara +BC / +C0); cuadro 60 op 5(0x96) el
     efecto 0x96 (4249F0; banco \PAS3.PAK, cargado durante la carrera); 120 op 8(3) 82E7C8 = 3 (humo y marcas de las
     ruedas 0 y 1: bits de 95AFAC en 41C570); 220 op 8(0); 599 cercano 0,1 y lejano 4000; 600 op 30: car+2DC = 0 y
     82E7D8 = 0 (fin: la camara y el coche se quedan como estan).
   - 486EF0 (si la escena tiene movimiento y 82E7E8 <= su duracion, 10 s): 20 canales (513650) en t = 82E7E8:
     car+2D8 c0, +2DC c1 (solo en el modo 16 sin 44FF10; si no 82EA70, que nadie escribe: 0), +2E0 c2, +2E8 c3,
     +2E4 c4, +2EC c5; palabras (x 65536 / 2pi, truncado) rueda 0 +32 c6, rueda 1 +32 c7, ruedas 0..3 +30 c8..c11;
     car+32 = 82E7CC = c6; con 82E7D0 == 1 la camara: +E0..+E8 c12..c14 (ojo), +EC..+F4 c15..c17 (mira), +130 c18
     (balanceo) y +A4 = +AC = 2 atan(0,70865 / (c19 x 0,03937)) (c19: focal en mm; en la variante 4, +E4 += 0,5). */
#include <kos.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "meta.h"
#include "../comun/gd.h"
#include "../comun/sfx.h"
#include "../comun/or2_fisica.h"

#define K_ANG 10430.3779296875f            /* 6282C0: 65536 / 2pi */
typedef struct { uint32_t op, w[4]; float f14, f18; int16_t cuadro; uint16_t r; } reg_t;   /* 32 bytes */
typedef struct { uint32_t og, n, om; float dur; } esc_t;

static uint8_t *D; static uint32_t Dn;
static const esc_t *esc; static uint32_t nesc;
static const uint8_t *tab;                 /* escena[20], pista_v1[5], pista_v2[5] */
static uint32_t ef_id, ef_vol, ef_hz, ef_n, ef_o, ef_b, ef_spu;
static uint8_t hecho[64];                  /* +0x28 de cada registro del guion de la escena */
static int escena = -1;
static float reloj;                        /* 82E7E0 */
static float t_mov;                        /* 82E7E8 */

int meta_carga(void)
{
    uint32_t h[4], i;
    if (D) return 0;
    if (!(D = gd_carga("META.DAT", &Dn))) return -1;
    memcpy(h, D, 16);
    if (Dn < 16 + 32 + 20 || memcmp(D, "MET1", 4) || h[2] + 16 * h[1] > Dn || h[1] > 64) { free(D); D = NULL; return -2; }
    nesc = h[1]; esc = (const esc_t *)(D + h[2]); tab = D + 16;
    { const uint8_t *e = D + 16 + 32; uint16_t a, b; memcpy(&a, e, 2); memcpy(&b, e + 2, 2); ef_id = a; ef_vol = b;
      memcpy(&ef_hz, e + 4, 4); memcpy(&ef_n, e + 8, 4); memcpy(&ef_o, e + 12, 4); memcpy(&ef_b, e + 16, 4); }
    for (i = 0; i < nesc; i++)
        if ((esc[i].n && (esc[i].og + 32 * esc[i].n > Dn || esc[i].n > 64)) || (esc[i].om && esc[i].om + 160 > Dn)) {
            free(D); D = NULL; return -3;
        }
    if (ef_o + ef_b > Dn) ef_b = 0;
    dbglog(DBG_INFO, "META: META.DAT %u KB, %u escenas, efecto 0x%x\n", (unsigned)(Dn >> 10), (unsigned)nesc, (unsigned)ef_id);
    return 0;
}
int meta_lista(void) { return D != NULL; }
void meta_suelta(void) { free(D); D = NULL; esc = NULL; tab = NULL; nesc = 0; escena = -1; }

int meta_escena(int variante, int preset, int meta)
{
    if (!D || variante == 4 || variante == 5 || variante == 6 || variante == 7) return 3;
    if (meta < 0 || meta > 4 || preset < 0 || preset > 3) return 3;
    return tab[meta + preset * 5];
}
int meta_pista(int variante, int meta)
{
    if (!D || meta < 0 || meta > 4) return -1;
    if (variante == 1) return tab[20 + meta];
    if (variante == 2) return tab[25 + meta];
    return -1;                             /* 0 (Time Attack del PC) y las demas: otra cosa o nada (PENDIENTE) */
}

void meta_empieza(meta_t *m, int e)
{
    memset(m, 0, sizeof *m);
    m->coloca = -1; m->cam = 1; m->activa = 1; m->pasplcar = 0;
    escena = e; reloj = 0; t_mov = 0; m->cuadro = 0;
    memset(hecho, 1, sizeof hecho);
    if (D && ef_b && !ef_spu) ef_spu = sfx_muestra_sube(D + ef_o, ef_b);
}
void meta_termina(void) { if (ef_spu) { sfx_muestra_baja(ef_spu); ef_spu = 0; } escena = -1; }

static float canal(const uint8_t *mv, int k, float t)
{
    uint32_t n, o;
    memcpy(&n, mv + 8 * k, 4); memcpy(&o, mv + 8 * k + 4, 4);
    if (!n || o + 16 * n > Dn) return 0.0f;
    return or2_canal((const float *)(D + o), (int)n, t);
}
static int16_t palabra(float v) { float x = v * K_ANG; return (int16_t)(int32_t)x; }   /* cvttss2si (16 bits bajos) */

static void guion(meta_t *m)                /* 486730 */
{
    const esc_t *E; const reg_t *r; uint32_t i;
    if (!D || escena < 0 || (uint32_t)escena >= nesc) return;
    E = esc + escena; if (!E->n) return;
    r = (const reg_t *)(D + E->og);
    for (i = 0; i < E->n; i++, r++) {
        if (!hecho[i] || r->cuadro > m->cuadro) continue;
        hecho[i] = 0;
        switch (r->op) {
        case 5:                             /* 4249F0(w4), salvo 0x16A con 4493C0 y las variantes 6 / 7 */
            if (r->w[0] == ef_id && ef_spu) sfx_muestra_toca(ef_spu, ef_n, ef_hz, (int)ef_vol);
            else dbglog(DBG_INFO, "META: efecto 0x%x sin muestra\n", (unsigned)r->w[0]);
            break;
        case 6: if (r->w[0] <= 1) m->pasplcar = (int)r->w[0]; break;   /* 486942 SelectPlCarCtrl */
        case 7: m->coloca = (int)r->w[0]; break;
        case 8: m->humo = r->w[0]; break;
        case 11: m->freno = r->w[0] == 1 ? 0xff : 0; m->kmh = r->w[0] == 1 ? 0x1e0 : 0; break;
        case 12: case 15: case 18: break;   /* ver arriba */
        case 14: m->cam = r->w[0] == 1; break;
        case 19: m->cerca = r->f14; break;
        case 20: m->lejos = r->f14; break;
        case 30: m->t[1] = 0; m->activa = 0; break;
        default: dbglog(DBG_INFO, "META: op %u sin portar (cuadro %d)\n", (unsigned)r->op, r->cuadro); break;
        }
    }
}

static int movimiento(meta_t *m)            /* 486EF0 */
{
    const esc_t *E; const uint8_t *mv; float t = t_mov, c19;
    if (!D || escena < 0 || (uint32_t)escena >= nesc) return 0;
    E = esc + escena; if (!E->om) return 0;
    if (t > E->dur) return 0;
    mv = D + E->om;
    m->t[0] = canal(mv, 0, t);
    m->t[1] = 0.0f;                         /* 82EA70 (modo 19) */
    m->t[2] = canal(mv, 2, t);
    m->r[1] = canal(mv, 3, t);
    m->r[0] = canal(mv, 4, t);
    m->r[2] = canal(mv, 5, t);
    m->s32 = palabra(canal(mv, 6, t));
    m->dir1 = palabra(canal(mv, 7, t));
    m->giro[0] = palabra(canal(mv, 8, t));
    m->giro[1] = palabra(canal(mv, 9, t));
    m->giro[2] = palabra(canal(mv, 10, t));
    m->giro[3] = palabra(canal(mv, 11, t));
    if (!m->cam) return 1;
    m->ojo[0] = canal(mv, 12, t); m->ojo[1] = canal(mv, 13, t); m->ojo[2] = canal(mv, 14, t);
    m->mira[0] = canal(mv, 15, t); m->mira[1] = canal(mv, 16, t); m->mira[2] = canal(mv, 17, t);
    m->balanceo = canal(mv, 18, t);
    c19 = canal(mv, 19, t);
    m->fov = 2.0f * atanf(0.7086499929428101f / (c19 * 0.03937000036239624f));
    return 1;
}

void meta_paso(meta_t *m)                   /* 4874A0 */
{
    m->coloca = -1; m->cerca = m->lejos = 0;
    if (!m->activa) { m->mueve = 0; return; }
    guion(m);
    m->mueve = movimiento(m);               /* tambien en el paso del op 30: 4874A0 ya miro 82E7D8 */
#ifdef DIAG_META_CUADRO
    if (m->cuadro >= DIAG_META_CUADRO) return;  /* prueba: la escena quieta en ese cuadro */
#endif
    reloj += 1.0f;
    m->cuadro = (int)reloj;
    t_mov = reloj * 0.01666666753590107f;
}
