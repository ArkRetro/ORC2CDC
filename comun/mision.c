/* El gestor de las misiones de COAST 2 COAST en la carrera, como el original.
   Logica: 496A30 de SLES-53998 (0x1f8d0c, el mission_manager_control_496a30 del port de PC outrun2-decomp:
   platform/mission_manager.cpp, comprobado instruccion a instruccion en el ELF de PS2 donde se indica) y lo que llama:
   477B60 (0x1f13c8: puntos y relojes de los coches de carreras), 477430 (0x1f0da8: el rival eliminado o pillado),
   495C20 (0x1f85d0: la nota), 495E90 (0x1f8930: las marcas) y 495D90 (el punto de las marcas, en demo/main.c).
   Tipos (+0x20 del registro de Races.bin): 0 carrera (solo la nota y los rotulos: lo demas en rival.c), 1 eliminacion por
   tiempo (cada [+0x28] segundos sale el ultimo; si es el jugador, mision fallada), 2 derrape (puntos del derrape del
   jugador contra los del rival: 0x1f90d0), 3 rebufo (puntos con +E68 > 0,18 sin choques: 0x1f9050), 6 pillar al rival
   (el jugador en el ultimo puesto gasta su tiempo; el rival en el ultimo puesto gasta el suyo y si llega a 0 esta
   pillado). Los tokens de los rotulos (PS2 0x36f3f8 tipos, 0x36f418 eliminado, 0x36f430 pillado, 0x36f470 notas,
   2B0064 los ultimos segundos) van como usos de C2CHUD.DAT / C2CH1.DAT / C2CH23.DAT / C2CH6.DAT (tools/conv_hud.py
   --c2c). */
#include <kos.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "rival.h"
#include "hud.h"
#include "mision.h"

int  (*mision_cuadros)(const char *nombre);
void (*mision_voz)(int id);
void (*mision_se)(int id);
void (*mision_graba)(int clave, int sub, int nota);

static mision_el_t el[MISION_NEL];
static int tipo, activa;
static int32_t t388, t38c, cd390, t394, ref3a0, seg3a8, aviso_ac, nota_b0;
static int fin_7d38f0, susp_3a4; static char fin_67e6b8[12];
static int32_t umb[3]; static float segundos;
/* PS2 0x40c6b8: la voz de cada tipo con la cuenta en 0xB4 */
static const uint16_t VOZ_TIPO[8] = { 0x1e7, 0x219, 0x1e8, 0x1ea, 0x1ea, 0x1ea, 0x218, 0 };
/* PS2 0x36f450: los cortes de la nota de los tipos 2 y 3 sin llegar al rival */
static const float CORTE[4] = { 0.3f, 0.5f, 0.7f, 1.0f };

const mision_el_t *mision_el(int i) { return i >= 0 && i < MISION_NEL ? el + i : NULL; }
int  mision_fin(void) { return fin_7d38f0; }
int  mision_suspende(void) { return susp_3a4; }
const char *mision_rotulo_fin(void) { return fin_67e6b8[0] ? fin_67e6b8 : NULL; }
int  mision_nota(void) { return nota_b0; }
int  mision_puntos(void) { return t388; }
float mision_escala(void) { return (float)t38c * 0.0016666667f * 0.17f + 1.0f; }   /* 0x44160000, 0x3e2e147b */

/* 465250: suelta el elemento */
static void suelta(mision_el_t *e) { e->vivo = 0; }
/* 465860(token, primero, ultimo, capa, modo, x, y, sx, sy, velocidad) + 465970 */
static void pon(mision_el_t *e, const char *nombre, int primero, int ultimo, int capa, int modo, float x, float y,
                float sx, float sy, float vel)
{
    int n = mision_cuadros ? mision_cuadros(nombre) : 0;
    memset(e, 0, sizeof *e);
    if (n <= 0) return;
    snprintf(e->nombre, sizeof e->nombre, "%s", nombre);
    e->n = n; e->primero = primero < 0 ? 0 : primero; e->ultimo = ultimo < 0 || ultimo >= n ? n - 1 : ultimo;
    e->capa = capa; e->modo = modo; e->vel = vel; e->cuadro = (float)e->primero;
    e->x = e->x0 = e->x1 = x; e->y = e->y0 = e->y1 = y; e->sx = e->sx0 = e->sx1 = sx; e->sy = e->sy0 = e->sy1 = sy;
    e->vivo = 1;
}
/* 4653C0(x, y, t) y 465310(sx, sy, t): hacia ahi en t segundos (12 cuadros los 0,2 s de 449E30) */
static void mueve(mision_el_t *e, float x, float y, float t)
{
    e->x0 = e->x; e->y0 = e->y; e->x1 = x; e->y1 = y; e->tm = 0; e->nm = (int)(t * 60.0f + 0.5f);
}
static void escala(mision_el_t *e, float sx, float sy, float t)
{
    e->sx0 = e->sx; e->sy0 = e->sy; e->sx1 = sx; e->sy1 = sy; e->ts = 0; e->ns = (int)(t * 60.0f + 0.5f);
}
/* 465310(1, 1, 0,2) y 4653C0(170, -80, 0,2): el movimiento de los rotulos del principio */
static void sube(mision_el_t *e) { escala(e, 1.0f, 1.0f, 0.2f); mueve(e, 170.0f, -80.0f, 0.2f); }

void mision_ui_paso(void)
{
    int k;
    for (k = 0; k < MISION_NEL; k++) {
        mision_el_t *e = el + k;
        if (!e->vivo) continue;
        if (e->nm > 0 && e->tm < e->nm) { float a = (float)++e->tm / (float)e->nm; e->x = e->x0 + (e->x1 - e->x0) * a; e->y = e->y0 + (e->y1 - e->y0) * a; }
        if (e->ns > 0 && e->ts < e->ns) { float a = (float)++e->ts / (float)e->ns; e->sx = e->sx0 + (e->sx1 - e->sx0) * a; e->sy = e->sy0 + (e->sy1 - e->sy0) * a; }
        e->cuadro += e->vel;
        if (e->cuadro > (float)e->ultimo) {                     /* (sprani.c: 0 da la vuelta, 1 / 5 se quedan, 3 se suelta) */
            if (e->modo == 0) e->cuadro = (float)e->primero;
            else if (e->modo == 3) e->vivo = 0;
            else e->cuadro = (float)e->ultimo;
        }
    }
}

static const rival_mision_t *M;

void mision_inicia(void)
{
    M = rival_mis();
    memset(el, 0, sizeof el);
    activa = M != NULL; tipo = M ? M->tipo : 0;
    t388 = t38c = t394 = ref3a0 = seg3a8 = 0; cd390 = 0x168; aviso_ac = 1; nota_b0 = 0;   /* 495B90 */
    fin_7d38f0 = 0; susp_3a4 = 0; fin_67e6b8[0] = 0;
    if (!M) return;
    segundos = M->segundos; memcpy(umb, M->umbral, sizeof umb);
    if (tipo == 6 || tipo == 1) t388 = (int32_t)(segundos * 60.0f);   /* 4961F0 */
}

/* 496460 (variante != 4): el jugador en el ultimo puesto */
static int ultimo_jugador(void) { return rival_puesto() == rival_total() - 1; }
/* 495A20 */
static void graba(void) { if (M && mision_graba) mision_graba(M->clave, M->sub, nota_b0); }
/* 0x1f95e8 / 0x1f9588 (496580): 20 puntos por encima del tope del coche (el 95 % fuera del tipo 3), si no 10 */
static int puntos_496580(const mision_jug_t *j)
{
    int rapido = tipo == 3 ? j->kmh > j->tope_kmh : j->kmh > j->tope_kmh * 0.95f;
    return rapido ? 20 : 10;
}
static void rotulo(const char *pre, int k)
{
    char n[12];
    if (k < 0 || k >= 6) k = 0;
    snprintf(n, sizeof n, "%s%d", pre, k);
    pon(el + 1, n, -1, -1, 0xa, 3, 0, 0, 1, 1, 1);
}
/* 496230: el rival pillado (tipo 6) */
static void pillado_496230(const rival_t *r)
{
    t394 = 0xb4;
    if (tipo == 6) rotulo("C2C_P", r->rival);
}

/* 477B60 (0x1f13c8): los puntos (2, 3) y los relojes (1, 6) de los coches de carreras */
static void rivales_477b60(void)
{
    int i, n = rival_n();
    for (i = 0; i < n; i++) {
        rival_t *r = rival_reg(i); int reloj = 0;
        if (!r) continue;
        if (tipo == 2 || tipo == 3) {                           /* +0x70: 0x200 derrape, 0x100 rebufo, 0x800 rapido */
            int bit = tipo == 2 ? 2 : 1;
            if ((r->fl71 & bit) && r->d6c == 0) r->d98 += (r->fl71 & 8) ? 0x14 : 0xa;
            continue;
        }
        if (tipo == 6) {
            if (r->vivo && r->puesto == rival_total() - 1) {
                if (r->d98 > 0) r->d98--;
                if (r->d98 == 0) {
                    pillado_496230(r);
                    if (rival_total() == 2) {                   /* el ultimo: mision cumplida */
                        graba(); fin_7d38f0 = 1; susp_3a4 = 1;  /* 495A20, 1cf188(1), 1f7da0(1) */
                        snprintf(fin_67e6b8, sizeof fin_67e6b8, "C2C_P%d", r->rival < 6 ? r->rival : 0);   /* 1f7dd0 */
                    }
                    dbglog(DBG_INFO, "MISION: rival %d (%d) pillado, quedan %d en carrera\n", i, r->rival, rival_total() - 1);
                    rival_elimina(i);                           /* 477430 */
                }
            }
            reloj = 1;
        } else reloj = tipo == 1;
        if (!reloj || !r->vivo) continue;
        if (r->puesto == rival_total() - 1) { if (++r->d9c >= 600) r->d9c = 600; }
        else { if (--r->d9c <= 0) r->d9c = 0; }
    }
}

/* 495C20 (0x1f85d0): la nota */
static void nota_495c20(void)
{
    int pos = rival_puesto(), ult = rival_total() - 1;
    if (tipo == 0) { int v = 6 - pos; nota_b0 = v <= 0 ? 0 : v; if (pos == ult) nota_b0 = 0; return; }
    if (tipo == 1) { int v = 6 - pos; nota_b0 = v <= 0 ? 0 : v; return; }
    if (tipo == 2 || tipo == 3) {
        int ri = rival_47da80(); const rival_t *r = rival_reg(ri); int32_t obj = r ? r->d98 : 0, k;
        if (t388 > obj) {
            nota_b0 = 4;
            if (t388 > umb[2]) nota_b0 = 6; else if (t388 > umb[1]) nota_b0 = 5;
            return;
        }
        {   float q = obj > 0 ? (float)t388 / (float)obj : 0.0f;
            for (k = 0; k < 4; k++) if (CORTE[k] > q) { nota_b0 = k; return; } }
        return;
    }
    if (tipo == 6) {
        int32_t v = (t388 + 5) / 6;
        nota_b0 = v > umb[2] ? 6 : 4 + (v > umb[1]);
        if (pos == ult) nota_b0 = 0;
    }
}

/* 495E90 (0x1f8930): las marcas de los puntos sobre el coche */
static void marcas_495e90(const mision_jug_t *j)
{
    float x, y;
    if (tipo == 3 || tipo == 2) {
        int32_t d = t388 - ref3a0; float v; int i; char n[12];
        x = j->interior ? 234.0f : j->mx[1]; y = j->interior ? 108.0f : j->my[1];
        if (d > 500) {                                          /* la nota grande, que vuela a los puntos */
            pon(el + 2, "C2C_G6", 0x3d, 0x69, 5, 1, x, y, 1, 1, 2);
            mueve(el + 2, 210.0f, -160.0f, 0.25f);
            if (mision_se) mision_se(0x52);
            ref3a0 = t388; return;
        }
        if (d < 0) { pon(el + 2, "C2C_MAS", -1, -1, 4, 1, x, y, 1, 1, 3); ref3a0 = t388; return; }   /* (choque: -2000) */
        v = (float)d * 0.002f * 7.0f; if (!(6.0f > v)) v = 6.0f;
        i = (int)v; snprintf(n, sizeof n, "C2C_G%d", i);
        if (!el[3].vivo || strcmp(el[3].nombre, n)) pon(el + 3, n, 0x1f, 0x3c, 3, 0, x, y, 1, 1, 1);
        else { el[3].x = el[3].x0 = el[3].x1 = x; el[3].y = el[3].y0 = el[3].y1 = y; }
        return;
    }
    if (tipo == 6) {
        int32_t d = t388 - ref3a0; if (d < 0) d = -d;
        x = j->interior ? 234.0f : j->mx[0]; y = j->interior ? 108.0f : j->my[0];
        if (d > 0x46) { pon(el + 2, "C2C_MAS", -1, -1, 4, 1, x, y, 1, 1, 3); ref3a0 = t388; }
    }
}

void mision_paso(const mision_jug_t *j)
{
    int e;
    if (!activa || fin_7d38f0) return;
    if (cd390) {                                                /* los rotulos del principio */
        if (cd390 == 0x168) {
            pon(el, "C2C_B20", -1, -1, 9, 3, 170, 20, 1, 4, 1); sube(el);
            pon(el + 1, "C2C_B3D", -1, -1, 0xa, 3, 170, 20, 1, 4, 1); sube(el + 1);
        } else if (cd390 == 0xb4) {
            char n[12];
            if (tipo >= 0 && tipo < 8 && VOZ_TIPO[tipo] && mision_voz) mision_voz(VOZ_TIPO[tipo]);   /* 45BD30(voz, 0xB4, 0xF) */
            pon(el, "C2C_B20", -1, -1, 9, 5, 170, 20, 1, 4, 1); sube(el);
            snprintf(n, sizeof n, "C2C_T%d", tipo); pon(el + 1, n, -1, -1, 0xa, 3, 170, 20, 1, 4, 1); sube(el + 1);
        }
        e = cd390 - 1;
        if (e <= 0) { cd390 = 0; suelta(el); suelta(el + 1); }
        else { cd390 = e - 1; if (!cd390) { suelta(el); suelta(el + 1); } }
    }
    e = t394 - 1; t394 = e <= 0 ? 0 : e - 1;
    switch (tipo) {
    case 3:                                                     /* 0x1f9050 */
        if (j->fuera || *j->da4 > 0) break;
        if (j->choque) { t388 -= 2000; if (t388 < 0) t388 = 0; *j->da4 = 0x1e; break; }
        if (j->rebufo > 0.18f && j->beta > -1500 && j->beta < 1500) t388 += puntos_496580(j);   /* 0x1bdd90 */
        break;
    case 2:                                                     /* 0x1f90d0 */
        if (j->d36 <= 0 || !(j->kmh > 10.0f) || j->fuera || *j->da4 > 0) break;
        t388 += puntos_496580(j);
        break;
    case 6:                                                     /* 0x1f9144 */
        if (ultimo_jugador()) {
            if (t388 > 0) t388--;
            if (t388 == 0) { nota_495c20(); graba(); fin_7d38f0 = 2; susp_3a4 = 1; }
        }
        break;
    case 1: {                                                   /* 0x1f91b0 */
        int ri = rival_ultimo(), c, s;
        t388--; if (t388 <= 0) t388 = 0;
        if (ri >= 0 && rival_total() == 2) snprintf(fin_67e6b8, sizeof fin_67e6b8, "C2C_E%d", rival_reg(ri)->rival < 6 ? rival_reg(ri)->rival : 0);
        c = t388; s = c / 60;
        if (s < 6) {
            if (seg3a8 != s) { seg3a8 = s; if (mision_se) mision_se(0x518d); }
            else if (c == 1) { seg3a8 = s; if (mision_se) mision_se(0x518d); }
        }
        if (c == 0x168) pon(el + 1, "C2C_ULT", -1, -1, 0xa, 3, 0, 0, 1, 1, 1);
        else if (c < 0x168 && c > 0x3c) {
            if (aviso_ac && ultimo_jugador()) { if (mision_voz) mision_voz(0x1f2); aviso_ac = 0; }
        } else if (c == 0) {
            if (ultimo_jugador()) { nota_495c20(); graba(); fin_7d38f0 = 2; }
            else if (ri >= 0) {
                const rival_t *r = rival_reg(ri);
                if (rival_total() == 2) { fin_7d38f0 = 1; susp_3a4 = 1; }
                t394 = 0xb4;
                snprintf(fin_67e6b8, sizeof fin_67e6b8, "C2C_E%d", r->rival < 6 ? r->rival : 0);
                rotulo("C2C_E", r->rival);
                if (mision_voz) mision_voz(0x1f0);
                dbglog(DBG_INFO, "MISION: eliminado el rival %d (%d), quedan %d en carrera\n", ri, r->rival, rival_total() - 1);
                rival_elimina(ri);
            }
            t388 = (int32_t)(segundos * 60.0f); aviso_ac = 1;   /* 4961F0 */
        }
        break; }
    default: break;
    }
    if (tipo == 6 || tipo == 1) {                               /* 0x1f9438: el jugador detras, mas deprisa */
        if (rival_ultimo() < 0) { if (++t38c >= 600) t38c = 600; }
        else { if (--t38c <= 0) t38c = 0; }
    }
    rivales_477b60();
    nota_495c20();
    marcas_495e90(j);
    {   static int n;
        if (++n % 120 == 0) {
            int ri = rival_47da80(); const rival_t *r = rival_reg(ri);
            dbglog(DBG_INFO, "MISION: tipo %d puntos %d rival %d (%d, +71 %x) puesto %d/%d nota %d escala %.3f\n", tipo, (int)t388, ri,
                   r ? (int)r->d98 : -1, r ? r->fl71 : 0, rival_puesto() + 1, rival_total(), (int)nota_b0, (double)mision_escala());
        }
    }
}

/* ---------------------------------------------------------------- HUD de la mision
   NAVI_PUB 0x1bd6f4 de SLES-53998 (4BED13 del PC) en el modo 16 con la variante 6: el panel 2C0046 (capa 3) y el
   2C0029 en los tipos 2 / 3 / 6 (0x1bd748), el avance 0x1bfd78 (4B9B40: icono del curso por su profundidad, marcas
   del rival de 47DA80 y del jugador, posicion / largo x 180), los puntos de los tipos 2 / 3 (0x1bf108 = 4BD230: "%07d"
   del jugador en (0x1E1, 0x49) con el estilo 1, o el 0xD fuera del asfalto / tras un toque, y los del rival en
   (0x1E1, 0x67) con su imagen 2C024F) y del tipo 6 (0x1bf260 = 4BD340: "%04d" de (cuadros + 5) / 6 del jugador y del
   rival en (0x227, 0x49 / 0x67), 2C00F1 delante de cada uno y el estilo 1 el del que va primero), el puesto de los
   tipos 0 y 1 (0x1c02d8 = 4B9D80: 2B0061 y las cifras de 42D280; idioma del disco 4 = espanol: sufijo 0 en el primero y
   3 en los demas, y = 0x38) y los cuatro elementos de 496A30 (rotulos, marca y nota). Las cifras de 0x1c3c68 (4BA9D0):
   el cuadro de la escena del estilo (0x404818: el 1 = 2C00B9, el 0xD = 2C00C0) es la cifra, 0x10 px por cifra. */
static hud_dat_t *H, *H2;
static int uso_de(const char *n, hud_dat_t **h)
{
    int u;
    if (H && (u = hudd_uso(H, n)) >= 0) { *h = H; return u; }
    if (H2 && (u = hudd_uso(H2, n)) >= 0) { *h = H2; return u; }
    return -1;
}
static int cuadros_uso(const char *n) { hud_dat_t *h; int u = uso_de(n, &h); return u >= 0 ? hudd_cuadros(h, u) : 0; }
void mision_hud(hud_dat_t *h, hud_dat_t *h2) { H = h; H2 = h2; mision_cuadros = cuadros_uso; }
static void dib(const char *n, int cuadro, float x, float y, float z)
{
    hud_dat_t *h; int u = uso_de(n, &h);
    if (u >= 0) hudd_dibuja(h, u, cuadro, x, y, 1, z, 0);
}
static void cifras(int estilo, int x, int y, const char *t)
{
    const char *g = estilo == 0xd ? "C2C_DD" : "C2C_D1"; int k;
    for (k = 0; t[k]; k++) if (t[k] >= '0' && t[k] <= '9') dib(g, t[k] - '0', (float)(x + 0x10 * k), (float)y, 92);
}
void mision_dibuja(int prof, int jug_pos, int puntos_vivos)
{
    static const int8_t OD[10] = { 0, 18, 0, 0, 0, 0, 0, 0, 0, 0 }, OU[10] = { 0, 18, 0, 0, 0, 0, 0, 8, 0, 0 };   /* 0x369888 / 0x369878 */
    int k, ri, lar = rival_largo(); char b[16];
    if (!H || !M) return;
    dib("C2C_PANEL", 0, 0, 0, 30);                              /* 2C0046, capa 3 */
    if (tipo == 2 || tipo == 3 || tipo == 6) dib("C2C_PANEL2", 0, 0, 0, 30.5f);   /* 2C0029 */
    if (prof >= 1 && prof <= 3) dib("C2C_ICONO", prof - 1, 0, 0, 10);
    else if (prof == 4 || prof == 14) dib("C2C_ICONO", prof == 4 ? 3 : 4, 0, 0, 10);
    ri = rival_47da80();
    if (lar > 0) {
        if (ri >= 0) dib("C2C_MARCA_R", (int)((float)rival_reg(ri)->pos_i / (float)lar * 180.0f), 0, 0, 5);
        dib("C2C_MARCA_J", (int)((float)jug_pos / (float)lar * 180.0f), 0, 0, 5.5f);
    }
    if (tipo == 2 || tipo == 3) {                               /* 0x1bf108 */
        sprintf(b, "%07d", t388 > 9999999 ? 9999999 : (int)t388);
        cifras(puntos_vivos ? 1 : 0xd, 0x1e1, 0x49, b);
        if (ri >= 0) {
            int32_t v = rival_reg(ri)->d98;
            sprintf(b, "%07d", v > 9999999 ? 9999999 : (int)v); cifras(1, 0x1e1, 0x67, b);
            dib("C2C_RIV", 0, 0x1e1, 0x67, 40);
        }
    } else if (tipo == 6) {                                     /* 0x1bf260 */
        int prim = ri >= 0 && rival_reg(ri)->puesto == 0;
        sprintf(b, "%04d", (int)((t388 + 5) / 6)); dib("C2C_ETQ", 0, 0x1e1, 0x36, 40); cifras(prim ? 0xd : 1, 0x227, 0x49, b);
        if (ri >= 0) {
            sprintf(b, "%04d", (int)((rival_reg(ri)->d98 + 5) / 6)); dib("C2C_ETQ", 0, 0x1e1, 0x54, 40);
            cifras(prim ? 1 : 0xd, 0x227, 0x67, b);
            dib("C2C_RIV", 0, 0x1f5, 0x67, 40);
        }
    } else if (tipo == 0 || tipo == 1) {                        /* 0x1c02d8 */
        int n = rival_total(), p = rival_puesto() + 1, dec, uni, cd, cu;
        dib("C2C_PUESTO", 0, 0, 0, 10);
        if (p > n) p = n;
        dec = p / 10; uni = p % 10; cd = n / 10; cu = n % 10;
        if (dec > 0) { dib("C2C_DPOS", dec, OD[dec] + OU[uni] + 0x1be, 0x42, 92); dib("C2C_DPOS", uni, OU[uni] + 0x1e6, 0x42, 92); }
        else dib("C2C_DPOS", uni, OU[uni] + 0x1e6, 0x42, 92);
        dib("C2C_SUF", p == 1 ? 0 : 3, 0x206 + 4, 0x38, 92);
        if (cd > 0) { dib("C2C_DN", cd, 0x22c, 0x46, 92); dib("C2C_DN", cu, 0x23c, 0x46, 92); }
        else dib("C2C_DN", cu, 0x22c, 0x46, 92);
        dib("C2C_BARRA", 0, 0x21c, 0x44, 92);
    }
    for (k = 0; k < MISION_NEL; k++) {                          /* los elementos de 496A30 (capa x 10) */
        const mision_el_t *e = el + k;
        if (!e->vivo) continue;
        hudd_escala(e->sx, e->sy);
        dib(e->nombre, (int)e->cuadro, e->x, e->y, (float)(e->capa * 10 + 10 + k));
        hudd_escala(1.0f, 1.0f);
    }
}
/* el rotulo de la meta de los tipos 6 y 1 ganados antes de la meta (498670: 67E6B8 en vez de GOAL) */
int mision_dibuja_fin(int cuadro)
{
    const char *n = mision_rotulo_fin();
    if (!n || !susp_3a4) return 0;
    dib(n, cuadro, 0, 0, 110);
    return 1;
}
