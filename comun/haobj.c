/* haobj.c: los objetos de las peticiones del INFARTO (Rev 1.1).

   Del original (PS2 SLES-53998 / PC):
   - 45C470 (PS2 0x1af428): el fichero oso_qt_* de la mision de la etapa (HAOSO.DAT lleva la eleccion ya hecha por
     preset y mision); 4F0D10 abre un suceso por registro con la funcion de su tipo (+1C) y el modelo de su clase
     (0x40a958: banco 0x57, la escala de la clase).
   - 45DB70 (PS2 0x1b3458): las clases 0x0A / 0x22 / 0x39 a la altura del suelo bajo (x, 9999,9, z) (43F2F0 = la
     consulta del suelo de la demo, coli_altura modo 0x100); las 5..9 con el marco de la carretera y (0, 0,65, 0).
   - Toque (0x1d3720 = 4AA140): solo el jugador; aqui la caja del coche (2,3 x 1,1 m, PROBABLE: la +D28 del original
     no esta leida) contra el punto del objeto agrandada 0,3 m; el toque es de un paso (+3F8, +3FC == 8).
   SIMPLIFICADO (DESVIACION): los conos golpeados no llevan el cuerpo rigido 516E10 (4ABA80 / 4ACD50): salen volando
     con la velocidad del coche y hacia arriba, girando, y se quitan a los 2 s; las clases 5..9 van con la vertical del
     mundo y no con el marco de la carretera 45D950 (los conos son simetricos: solo cambia la inclinacion). */
#include <kos.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include "gd.h"
#include "haobj.h"

typedef struct {
    uint8_t cl, tipo, est, rg;           /* est: 0 sin colocar, 1 en su sitio, 2 volando, 3 fuera */
    int16_t reg; int16_t t;
    float l[3], a[3];                    /* del fichero: posicion y angulos en el marco de la etapa */
    float p[3], R[3][3], v[3], esc, giro;
    /* estrellas (tipo 8, control 0x1d4d80): +A4 reposo, +B0 paso del vuelo, +C0 activa, +BC volando, +74 tiempo del
       bote, +CC seccion (-32768: sin calcular); pieza: uno de los 40 trozos 0x134.. (est 4: abierto sin usar) */
    float r0[3], b0[3], t74; uint8_t act, vuela, pieza; int16_t sec;
} obj_t;
static uint8_t *cab;                     /* el primer sector de HAOSO.DAT */
static uint32_t lba_o, tam_o, nfich;
static obj_t *ob; static int nob;
static int toq_cl[16], ntoq, cru_cl[8], ncru; static uint32_t reloj_obj;
#define NPIEZA 40                        /* 0x134..0x15B */
static int n_est, n_pie, base_pie, toq_g, toq_p, npos; static int (*sec_cb)(float x, float z);
static int (*suelo_cb)(float x, float z, float *y);
static struct { int reg; float p[3], R[3][3]; } marca[2]; static int n_marca; static float marca_a;   /* 7F24D0 / D4, [0x58a140] */
void haobj_marcas(int n, const int *reg, const float (*p)[3], const float *fx, const float *fz)
{
    int i;
    n_marca = 0;
    for (i = 0; i < n && i < 2; i++) {
        float l = sqrtf(fx[i] * fx[i] + fz[i] * fz[i]), c, s;
        if (l < 1e-4f) continue;
        c = -fz[i] / l; s = -fx[i] / l;               /* Ry: el eje z del modelo contra la carretera (45B000; con +z el
                                                         texto se leia al reves en las capturas: el eje +x del original
                                                         va al lado contrario del de la demo, PROBABLE) */
        marca[n_marca].reg = reg[i]; memcpy(marca[n_marca].p, p[i], sizeof marca[0].p);
        marca[n_marca].p[1] += 4.0f;                  /* 0x1a868c: + 4 */
        memset(marca[n_marca].R, 0, sizeof marca[0].R);
        marca[n_marca].R[0][0] = c; marca[n_marca].R[0][2] = s; marca[n_marca].R[1][1] = 1; marca[n_marca].R[2][0] = -s; marca[n_marca].R[2][2] = c;
        n_marca++;
    }
#ifdef DIAG_HA
    dbglog(DBG_INFO, "HAOBJ: %d marcas (registros 0x%x / 0x%x) en (%.0f, %.0f, %.0f) y (%.0f, %.0f, %.0f)\n", n_marca, n > 0 ? reg[0] : 0,
           n > 1 ? reg[1] : 0, (double)(n_marca ? marca[0].p[0] : 0), (double)(n_marca ? marca[0].p[1] : 0), (double)(n_marca ? marca[0].p[2] : 0),
           (double)(n_marca > 1 ? marca[1].p[0] : 0), (double)(n_marca > 1 ? marca[1].p[1] : 0), (double)(n_marca > 1 ? marca[1].p[2] : 0));
#endif
}

int haobj_inicia(void)
{
    if (cab) return 0;
    if (gd_busca("HAOSO.DAT", &lba_o, &tam_o) || tam_o < 1024) { dbglog(DBG_ERROR, "HAOBJ: sin HAOSO.DAT\n"); return -1; }
    if (!(cab = memalign(32, 2048))) return -1;
    if (gd_lee(lba_o, 1, cab) || memcmp(cab, "HAO1", 4)) { free(cab); cab = NULL; return -1; }
    memcpy(&nfich, cab + 4, 4);
    if (8 + 84 + 512 + 8 * nfich > 2048) { free(cab); cab = NULL; return -1; }
    return 0;
}
void haobj_libera(void) { free(ob); ob = NULL; nob = 0; free(cab); cab = NULL; }

void haobj_mision(int preset, int mision)
{
    uint32_t f, off, n, s0, ns, i; uint8_t *b; int k;
    free(ob); ob = NULL; nob = 0; ntoq = 0; n_est = n_pie = 0; n_marca = 0;
    if (!cab || mision < 0 || mision > 40) return;
    f = cab[8 + (preset == 1 ? 41 : 0) + mision];      /* 45C470: el preset 1 (OutRun 2) o el 0 */
    if (f == 0xff || f >= nfich) return;
    memcpy(&off, cab + 8 + 84 + 512 + 8 * f, 4); memcpy(&n, cab + 8 + 84 + 512 + 8 * f + 4, 4);
    if (!n || off + 28 * n > tam_o) return;
    s0 = off & ~2047u; ns = (off + 28 * n - s0 + 2047) >> 11;
    if (!(b = memalign(32, ns << 11))) return;
    if (gd_lee(lba_o + (s0 >> 11), (int)ns, b) || !(ob = calloc(n + NPIEZA, sizeof(obj_t)))) { free(b); return; }
    for (i = 0, k = 0; i < n; i++) {
        const uint8_t *r = b + (off - s0) + 28 * i; uint8_t cl = r[0]; uint16_t reg; float esc;
        if (cl >= 64) continue;
        memcpy(&reg, cab + 8 + 84 + 8 * cl, 2); memcpy(&esc, cab + 8 + 84 + 8 * cl + 4, 4);
        if (reg == 0xffff) continue;                    /* de otro recurso (la luz 56 de FLOR, la clase 3): no van */
        ob[k].cl = cl; ob[k].tipo = r[1]; ob[k].reg = (int16_t)reg; ob[k].esc = esc; ob[k].sec = -32768;
        memcpy(ob[k].l, r + 4, 12); memcpy(ob[k].a, r + 16, 12);
        k++;
    }
    nob = k; free(b);
    for (i = 0, n_est = 0; i < (uint32_t)nob; i++) if (ob[i].tipo == 8) n_est++;
    n_pie = 0; base_pie = 0;
    if (n_est) {                                       /* los 40 trozos de las estrellas (cerrados hasta la -10) */
        for (i = 0; i < NPIEZA; i++) { obj_t *o = ob + nob + i; memset(o, 0, sizeof *o); o->cl = 0x12; o->tipo = 8; o->pieza = 1; o->est = 3; o->sec = -32768;
            {   uint16_t rg; memcpy(&rg, cab + 8 + 84 + 8 * 0x12, 2); o->reg = (int16_t)rg; } }
        n_pie = NPIEZA; nob += NPIEZA;
    }
    dbglog(DBG_INFO, "HAOBJ: mision %d (preset %d): fichero %u, %d objetos de %u\n", mision, preset, (unsigned)f, nob, (unsigned)n);
}
static void euler(float R[3][3], const float *a, float yaw)   /* Ry(yaw) Ry(ay) Rx(ax) Rz(az), columnas */
{
    float cy = cosf(a[1] + yaw), sy = sinf(a[1] + yaw), cx = cosf(a[0]), sx = sinf(a[0]), cz = cosf(a[2]), sz = sinf(a[2]);
    float X[3][3] = { { 1, 0, 0 }, { 0, cx, -sx }, { 0, sx, cx } }, Y[3][3] = { { cy, 0, sy }, { 0, 1, 0 }, { -sy, 0, cy } },
          Z[3][3] = { { cz, -sz, 0 }, { sz, cz, 0 }, { 0, 0, 1 } }, T[3][3];
    int i, j, k;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) { T[i][j] = 0; for (k = 0; k < 3; k++) T[i][j] += Y[i][k] * X[k][j]; }
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) { R[i][j] = 0; for (k = 0; k < 3; k++) R[i][j] += T[i][k] * Z[k][j]; }
}
void haobj_paso(const haobj_coche_t *c)
{
    int i; float fx = c->fw[0], fz = c->fw[2], fl = sqrtf(fx * fx + fz * fz);
    ntoq = 0; ncru = 0; reloj_obj++; toq_g = toq_p = 0; npos = 0; sec_cb = c->sec; suelo_cb = c->suelo;
    marca_a += 0.2094395f; if (marca_a > 3.14159274f) marca_a -= 6.28318548f;   /* 0x1a7f78: el vaiven de las marcas */
    if (!ob) return;
#ifdef DIAG_HA
    {   static int t; float dm = 1e9f; int k, est = 0;
        for (k = 0; k < nob; k++) { float dx = ob[k].p[0] - c->pos[0], dz = ob[k].p[2] - c->pos[2]; if (ob[k].est == 1) est++; if (ob[k].est && dx * dx + dz * dz < dm) dm = dx * dx + dz * dz; }
        if (++t % 120 == 0) dbglog(DBG_INFO, "DIAGHA haobj: %d de %d en su sitio, el mas cercano a %.1f m del coche (%.1f, %.1f)\n", est, nob, (double)sqrtf(dm), (double)c->pos[0], (double)c->pos[2]);
    }
#endif
    if (fl > 1e-4f) { fx /= fl; fz /= fl; } else { fx = 0; fz = 1; }
    for (i = 0; i < nob; i++) {
        obj_t *o = ob + i;
        if (o->est == 0 && c->marco) {                  /* 45DB70 y la colocacion de la etapa */
            float m3 = c->marco[3], cs = cosf(m3), sn = sinf(m3), y;
            o->p[0] = o->l[0] * cs + o->l[2] * sn + c->marco[0];
            o->p[2] = -o->l[0] * sn + o->l[2] * cs + c->marco[2];
            o->p[1] = o->l[1] + c->marco[1];
            if (o->cl == 0x0a || o->cl == 0x22 || o->cl == 0x39 || o->tipo == 8) {   /* (8: 4AADC0, 43EB60 modo 0x100) */
                int r; y = 9999.9f;
                if (!c->suelo || (r = c->suelo(o->p[0], o->p[2], &y)) == -2) continue;   /* sin la colision aun */
                if (r >= 0) o->p[1] = y;
            }
            if (o->cl >= 5 && o->cl <= 9) o->p[1] += 0.65f;   /* 0x5b0078 */
            if (o->tipo == 6) o->p[1] += o->cl == 0x39 ? 1.0f : 1.2f;   /* 0x1d3d48: la moneda / el corazon, a 1,2 (la 57: 1,0) */
            euler(o->R, o->a, m3);
            o->est = 1;
            if (o->tipo == 8) {                         /* 4AADC0 (PS2 0x1d4b60): de lado (el eje de la seccion) y hacia */
                float fx2 = 0, fz2 = 1, d[3], l;        /* abajo (-0,5 - rand), al azar a un lado; +B0 = 6 D, sale de -200 D */
                int q;
                memcpy(o->r0, o->p, sizeof o->r0);
                if (c->dir) c->dir(o->p[0], o->p[2], &fx2, &fz2);
                l = sqrtf(fx2 * fx2 + fz2 * fz2); if (l < 1e-4f) { fx2 = 0; fz2 = 1; l = 1; }
                d[0] = fz2 / l; d[2] = -fx2 / l;
                d[1] = -0.5f - (float)rand() * 4.65661287e-10f;
                if (rand() <= 0x3ffffffe) { d[0] = -d[0]; d[2] = -d[2]; }
                l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                for (q = 0; q < 3; q++) { d[q] /= l; o->b0[q] = d[q] * 6.0f; o->p[q] = o->r0[q] - 200.0f * d[q]; }
                o->act = 0; o->vuela = 1; o->t74 = 0; o->giro = 0;
            }
#ifdef DIAG_HA
            if (i < 3) dbglog(DBG_INFO, "DIAGHA objeto %d clase %d: local (%.1f, %.1f, %.1f) mundo (%.1f, %.1f, %.1f); coche (%.1f, %.1f, %.1f), marco (%.1f, %.1f, %.1f, %.3f)\n",
                              i, o->cl, (double)o->l[0], (double)o->l[1], (double)o->l[2], (double)o->p[0], (double)o->p[1], (double)o->p[2],
                              (double)c->pos[0], (double)c->pos[1], (double)c->pos[2], (double)c->marco[0], (double)c->marco[1], (double)c->marco[2], (double)c->marco[3]);
#endif
        }
#ifdef PRUEBA_HA_TOQUE
        if (o->est == 1 && (o->cl == PRUEBA_HA_TOQUE || PRUEBA_HA_TOQUE < 0) && !(o->tipo == 8 && (o->vuela || !o->act))) {   /* prueba: los de esa clase, a la linea del coche */
            float dx = o->p[0] - c->pos[0], dz = o->p[2] - c->pos[2], al = dx * fx + dz * fz, la = dx * fz - dz * fx;
            if (al > 25.0f && al < 40.0f && fabsf(la) < 25.0f && !o->rg) { o->p[0] = c->pos[0] + fx * al; o->p[2] = c->pos[2] + fz * al; o->rg = 1;
#ifdef PRUEBA_HA_POSTE
                if (o->tipo == 7) { o->p[0] += fz * 3.0f * o->esc; o->p[2] -= fx * 3.0f * o->esc; }   /* prueba: un poste en la linea */
#endif
                if (o->tipo == 8) { o->r0[0] = o->p[0]; o->r0[2] = o->p[2]; } }
        }
#endif
        if (o->est == 1 && o->tipo == 7) {              /* el arco (0x1d42f0): cruce del paso del coche entre los postes */
            float ax, az, bx, bz, d1, d2, e1, e2, c0, s0, h = 3.0f * o->esc;
            if (fabsf(o->p[0] - c->pos[0]) > 30.0f || fabsf(o->p[2] - c->pos[2]) > 30.0f) continue;
            c0 = o->R[0][0]; s0 = -o->R[2][0];          /* el eje x del arco en el mundo: los postes en (+-h, 0, 0) */
            ax = o->p[0] + c0 * h; az = o->p[2] - s0 * h; bx = o->p[0] - c0 * h; bz = o->p[2] + s0 * h;
            d1 = (bx - ax) * (c->ant[2] - az) - (bz - az) * (c->ant[0] - ax); d2 = (bx - ax) * (c->pos[2] - az) - (bz - az) * (c->pos[0] - ax);
            e1 = (c->pos[0] - c->ant[0]) * (az - c->ant[2]) - (c->pos[2] - c->ant[2]) * (ax - c->ant[0]);
            e2 = (c->pos[0] - c->ant[0]) * (bz - c->ant[2]) - (c->pos[2] - c->ant[2]) * (bx - c->ant[0]);
            if (d1 * d2 <= 0 && e1 * e2 <= 0 && (d1 != 0 || d2 != 0) && o->p[1] + h > c->pos[1]) {
                if (ncru < 8) cru_cl[ncru++] = o->cl;
#ifdef DIAG_HA
                dbglog(DBG_INFO, "DIAGHA arco %d cruzado\n", i);
#endif
                o->est = 2; o->t = 0; o->giro = 0; o->v[0] = 0;   /* +7C = 3: sin giro (+94 = 0 de 0x1d41d0) */
                continue;
            }
            {   int q;                                  /* cada poste (0x1d4434): (+-h, y + 0,3), radio 0,3 x escala, 0xCD */
                for (q = 0; q < 2; q++) {
                    float px = q ? ax : bx, pz = q ? az : bz, dx = px - c->pos[0], dz = pz - c->pos[2], r = 0.3f * o->esc;
                    float al = dx * fx + dz * fz, la = dx * fz - dz * fx;
                    if (fabsf(o->p[1] + 0.3f - c->pos[1]) > 3.0f || !(fabsf(al) < 2.3f + r && fabsf(la) < 1.1f + r)) continue;
                    o->est = 2; o->t = 0; o->v[0] = q ? 0.2f : -0.2f;   /* 0x1d4508: +7C 1 (-h) -> -0,2, 2 (+h) -> +0,2 */
                    npos++;
#ifdef DIAG_HA
                    dbglog(DBG_INFO, "DIAGHA poste %d del arco %d tocado (giro %.1f)\n", q + 1, i, (double)o->v[0]);
#endif
                    break;
                }
            }
            continue;
        }
        if (o->est == 1 && o->tipo == 8) o->giro += 0.1f;   /* 0x1d4d80: +6C + 0,1 por paso */
        if (o->est == 1) {                              /* toque del jugador (0x1d3720) */
            float dx = o->p[0] - c->pos[0], dz = o->p[2] - c->pos[2], dy = o->p[1] - c->pos[1], al, la;
            if (fabsf(dx) > 4.0f || fabsf(dz) > 4.0f || fabsf(dy) > 3.0f) continue;
            al = dx * fx + dz * fz; la = dx * fz - dz * fx;
#ifdef DIAG_HA

#endif
            {   float r = o->tipo == 6 ? 0.8f : o->tipo == 8 ? 1.5f : 0.3f;  /* (la moneda: su radio, PROBABLE; la estrella: +90) */
                if (!(fabsf(al) < 2.3f + r && fabsf(la) < 1.1f + r)) continue;
            }
            {
                if (ntoq < 16) toq_cl[ntoq++] = o->cl;
#ifdef DIAG_HA
                dbglog(DBG_INFO, "DIAGHA toque objeto %d clase %d tipo %d (ntoq %d)\n", i, o->cl, o->tipo, ntoq);
#endif
                o->est = 2; o->t = 0;
                if (o->tipo == 8) {                     /* 0x1d4d80: +7C = 1, control 0x1d50e0, escala x 0,7 */
                    if (o->pieza) toq_p++; else toq_g++;
                    o->esc *= 0.7f; o->giro = 0; continue;
                }
                if (o->tipo == 6) {                     /* 0x1d3fc8: la escala a 0,3 y sube con el coche (y = 25 t - 39,2 t^2) */
                    o->v[0] = c->pos[1]; o->esc *= 0.3f; continue;
                }
                o->v[0] = fx * c->v * 1.1f + (la >= 0 ? fz : -fz) * 3.0f; o->v[2] = fz * c->v * 1.1f - (la >= 0 ? fx : -fx) * 3.0f;
                o->v[1] = 5.0f + c->v * 0.1f; o->giro = 0;
            }
        } else if (o->est == 2 && o->tipo == 7) {       /* 0x1d4508: gira (+94), y a los 3 s se cierra (parpadea el ultimo) */
            o->giro += o->v[0];
            if (++o->t >= 180) o->est = 3;
        } else if (o->est == 2 && o->tipo == 8) {       /* 0x1d50e0: sube con el coche y a los 0,319 s se encoge 0,03 */
            float t = (float)++o->t / 60.0f;
            o->p[0] = c->pos[0]; o->p[2] = c->pos[2]; o->p[1] = c->pos[1] + 25.0f * t - 39.2f * t * t;
            o->giro += 20.0f;
            if (t > 0.319f && (o->esc -= 0.03f) <= 0.0f) { o->esc = 0; o->est = o->pieza ? 4 : 3; }
        } else if (o->est == 2 && o->tipo == 6) {
            float t = (float)++o->t / 60.0f;
            o->p[0] = c->pos[0]; o->p[2] = c->pos[2]; o->p[1] = o->v[0] + 25.0f * t - 39.2f * t * t;
            if (t > 0.319f) o->esc -= 0.015f;
            if (o->esc <= 0.0f || o->t > 60) o->est = 3;
        } else if (o->est == 2) {                       /* SIMPLIFICADO: vuelo balistico y fuera a los 2 s */
            const float dt = 1.0f / 60.0f;
            o->p[0] += o->v[0] * dt; o->p[1] += o->v[1] * dt; o->p[2] += o->v[2] * dt; o->v[1] -= 9.8f * dt;
            o->giro += 0.25f;
            if (++o->t > 120) o->est = 3;
        }
    }
}
/* 461940 (PS2 0x1b11c8) */
void haobj_est_paso(int sec_jug, int lim, int *activas, const float *pz, haobj_est_t *r)
{
    int i, k, b0, calc = 0; const float dt = 0.016666668f;   /* 0x3c888889 */
    r->rotas = toq_g; r->cogidas = toq_p; r->primera = 0;
    if (!ob || !n_est) return;
    for (i = 0; i < nob - n_pie; i++) {
        obj_t *o = ob + i;
        if (o->tipo != 8 || o->est == 0) continue;
        if (!o->act) {                                  /* +C0 = 0: hasta 40 secciones por delante (+CC) */
            if (o->sec == -32768) { if (calc < 4 && sec_cb) { o->sec = (int16_t)sec_cb(o->r0[0], o->r0[2]); calc++; } continue; }
            if (*activas < lim && sec_jug + 0x28 >= o->sec) { if (!*activas) r->primera = 1; o->act = 1; ++*activas; }
            continue;
        }
        if (o->vuela) {                                 /* +BC: +B0 por paso hasta el suelo; alli + 0,05 */
            if (o->p[1] > o->r0[1]) {
                o->p[0] += o->b0[0]; o->p[1] += o->b0[1]; o->p[2] += o->b0[2];
                if (o->p[1] <= o->r0[1]) { memcpy(o->p, o->r0, sizeof o->p); o->p[1] += 0.05f; o->vuela = 0; o->t74 = 0; }
            }
        } else if (o->est != 2 && o->p[1] > o->r0[1]) {   /* el bote: y = 0,05 + 20 t - 39,2 t^2 y al caer los trozos */
            float t = o->t74, y = o->r0[1] + 0.05f + t * 20.0f - t * t * 39.2f;
            o->p[1] = y;
            if (!(y <= o->r0[1])) o->t74 = t + dt;
            else {
                o->p[1] = o->r0[1]; o->t74 = 0;
#ifdef DIAG_HA
                dbglog(DBG_INFO, "DIAGHA estrella %d en el suelo (%.1f, %.1f, %.1f) seccion %d (jugador %d); trozos desde %d\n", i,
                       (double)o->p[0], (double)o->p[1], (double)o->p[2], o->sec, sec_jug, base_pie);
#endif
                if (base_pie >= NPIEZA) base_pie = 0;
                for (k = 0, b0 = base_pie; k < 5 && pz; k++) {   /* tabla 0x3f7348 por el carril (+CE): todos la misma */
                    const float *t7 = pz + 7 * k; int j = b0 + k; obj_t *e; float y2 = 9999.9f;
                    if (j >= NPIEZA) continue;
                    e = ob + (nob - n_pie) + j;
                    if (e->est == 3) continue;          /* sin abrir (la -10) */
                    e->p[0] = o->p[0] + t7[0]; e->p[1] = o->p[1] + t7[1]; e->p[2] = o->p[2] + t7[2];
                    if (suelo_cb && suelo_cb(e->p[0], e->p[2], &y2) >= 0) e->p[1] = y2;   /* 43EB60 modo 0x100 */
                    memcpy(e->r0, e->p, sizeof e->r0); e->p[1] += 0.05f;
                    euler(e->R, t7 + 3, 0); e->esc = t7[6];
                    e->est = 1; e->act = 1; e->vuela = 0; e->t74 = 0; e->giro = 0; e->t = 0;
                    base_pie++;
                }
            }
        }
    }
    for (i = nob - n_pie; i < nob; i++) {               /* los trozos botan igual */
        obj_t *o = ob + i; float t, y;
        if (o->est != 1 || !(o->p[1] > o->r0[1])) continue;
        t = o->t74; y = o->r0[1] + 0.05f + t * 20.0f - t * t * 39.2f; o->p[1] = y;
        if (y <= o->r0[1]) { o->p[1] = o->r0[1]; o->t74 = 0; } else o->t74 = t + dt;
    }
}
void haobj_est_abre(int si)
{
    int i;
    if (!ob) return;
    for (i = nob - n_pie; i < nob; i++) ob[i].est = si ? (ob[i].est == 3 ? 4 : ob[i].est) : 3;
    if (!si) base_pie = 0;
}
int haobj_postes(void) { return npos; }
int haobj_cruces(int *cl, int max) { int i; for (i = 0; i < ncru && i < max; i++) cl[i] = cru_cl[i]; return ncru; }
void haobj_arcos(int lo, int hi, const float *t, int (*carriles)(float x, float z), int (*sec)(float x, float z),
                 void (*dir)(float x, float z, float *fx, float *fz))
{
    static float T[64]; int i, n = 0, k = 0, n1, j;
    if (!ob || !t || !sec) return;
    for (i = 0; i < nob; i++) if (ob[i].cl == 0x0b && ob[i].est == 1 && (j = sec(ob[i].p[0], ob[i].p[2])) >= lo && j <= hi) n++;
    if (n > 64) n = 64;
    for (i = 0; i < nob && k < n; i++) {
        obj_t *o = ob + i;
        if (o->cl != 0x0b || o->est != 1) continue;
        if (k == 0) {                                   /* los anchos de todos con los carriles del primero */
            float w = carriles ? (float)carriles(o->p[0], o->p[2]) : 3.0f, st;
            n1 = (int)((float)n * t[3]);
            st = n1 ? (t[1] - t[0]) / (float)n1 : 0;
            for (j = 0; j < n1; j++) T[j] = ((float)j * st + t[0]) * w;
            st = n - n1 ? (t[2] - t[1]) / (float)(n - n1) : 0;
            for (j = 0; j < n - n1; j++) T[n1 + j] = ((float)j * st + t[1]) * w;
        }
        o->esc = T[k];
        if (dir) {                                      /* 45DB70: la clase 0x0B con el marco de la carretera (45D950) */
            float fx, fz, a[3] = { 0, 0, 0 }; dir(o->p[0], o->p[2], &fx, &fz); a[1] = atan2f(fx, fz); euler(o->R, a, 0);
        }
        if (2.5f > o->esc * 3.0f) o->esc = 0.833333f;   /* 0x1afc7c..0x1afcb4 */
        k++;
    }
    dbglog(DBG_INFO, "HAOBJ: %d arcos en %d..%d, anchos %.2f..%.2f\n", n, lo, hi, (double)(n ? T[0] : 0), (double)(n ? T[n - 1] : 0));
}
int haobj_toques(int *cl, int max) { int i; for (i = 0; i < ntoq && i < max; i++) cl[i] = toq_cl[i]; return ntoq; }
int haobj_cercano(const float pos[3], const float fw[3], int cl, float *lat)
{
    int i, k = -1; float best = 1e9f, fl = sqrtf(fw[0] * fw[0] + fw[2] * fw[2]), fx, fz;
    if (fl < 1e-4f) return 0;
    fx = fw[0] / fl; fz = fw[2] / fl;
    for (i = 0; i < nob; i++) {
        float dx = ob[i].p[0] - pos[0], dz = ob[i].p[2] - pos[2], al = dx * fx + dz * fz;
        if (ob[i].est != 1 || (cl >= 0 && ob[i].cl != cl) || al < 4.0f || al > 70.0f || fabsf(-dx * fz + dz * fx) > 25.0f) continue;
        if (al < best) { best = al; k = i; *lat = -dx * fz + dz * fx; }
    }
    return k >= 0;
}
int haobj_cuenta_sec(int clase, int lo, int hi, int (*sec)(float x, float z))
{
    int i, n = 0, k;
    for (i = 0; i < nob; i++) {
        obj_t *o = ob + i;
        if (o->cl != clase || o->est != 1 || o->pieza || !sec) continue;
        if (o->tipo == 8) { if (o->sec == -32768) o->sec = (int16_t)sec(o->r0[0], o->r0[2]); k = o->sec; }   /* (en el suelo) */
        else k = sec(o->p[0], o->p[2]);
        if (k >= lo && k <= hi) n++;
    }
    return n;
}
int haobj_cuenta(int clase) { int i, n = 0; for (i = 0; i < nob; i++) if (ob[i].cl == clase && ob[i].est < 2) n++; return n; }
int haobj_lista(haobj_dib_t *v, int max, const float ojo[3])
{
    static float dd[64]; int i, n = 0, j;
    if (max > 64) max = 64;
    for (i = 0; i < n_marca && n < max; i++) {         /* 45B200: + sen del vaiven en y */
        float dx = marca[i].p[0] - ojo[0], dz = marca[i].p[2] - ojo[2]; haobj_dib_t *e = v + n; int a, b;
        if (dx * dx + dz * dz > HAOBJ_LEJOS * HAOBJ_LEJOS) continue;
        e->reg = marca[i].reg;
        for (a = 0; a < 3; a++) { for (b = 0; b < 3; b++) e->M[a][b] = marca[i].R[b][a]; e->M[a][3] = 0; }
        e->M[3][0] = marca[i].p[0]; e->M[3][1] = marca[i].p[1] + sinf(marca_a); e->M[3][2] = marca[i].p[2]; e->M[3][3] = 1;
        dd[n++] = 0;                                    /* (siempre: no las sustituye un objeto mas cerca) */
    }
    for (i = 0; i < nob; i++) {
        const obj_t *o = ob + i; float dx, dy, dz, d; haobj_dib_t *e;
        if (o->est != 1 && o->est != 2) continue;
        if (o->est == 2 && o->tipo == 7 && o->t >= 120 && (o->t & 1)) continue;   /* parpadeo del ultimo segundo */
        dx = o->p[0] - ojo[0]; dy = o->p[1] - ojo[1]; dz = o->p[2] - ojo[2]; d = dx * dx + dy * dy + dz * dz;
        if (d > HAOBJ_LEJOS * HAOBJ_LEJOS) continue;
        if (n == max) {                                 /* lleno: sustituye al mas lejano si este esta mas cerca */
            int k = 0; for (j = 1; j < n; j++) if (dd[j] > dd[k]) k = j;
            if (d >= dd[k]) continue;
            j = k;
        } else j = n++;
        dd[j] = d; e = v + j; e->reg = o->reg;
        {   float R[3][3]; int a, b;
            memcpy(R, o->R, sizeof R);
            if (o->tipo == 8) {                         /* 0x1d4d80: (0, 2, 0) + pos, giro +6C, escala +50 x 1,5; al subir */
                float c = cosf(o->giro), s = sinf(o->giro), T[3][3]; int q;   /* (0x1d50e0) sin el 2 ni el 1,5 */
                for (q = 0; q < 3; q++) { T[q][0] = R[q][0] * c - R[q][2] * s; T[q][1] = R[q][1]; T[q][2] = R[q][0] * s + R[q][2] * c; }
                memcpy(R, T, sizeof R);
            } else if (o->tipo == 6 && o->est == 1) {   /* 0x1d3d48: gira 0,05 por paso sobre la vertical */
                float a = 0.05f * (float)reloj_obj, c = cosf(a), s = sinf(a), T[3][3];
                { int q; for (q = 0; q < 3; q++) { T[q][0] = R[q][0] * c - R[q][2] * s; T[q][1] = R[q][1]; T[q][2] = R[q][0] * s + R[q][2] * c; } }
                memcpy(R, T, sizeof R);
            }
            if (o->est == 2 && o->tipo == 7) {          /* el arco cruzado gira sobre la vertical */
                float c = cosf(o->giro), s = sinf(o->giro), T[3][3]; int q;
                for (q = 0; q < 3; q++) { T[q][0] = R[q][0] * c - R[q][2] * s; T[q][1] = R[q][1]; T[q][2] = R[q][0] * s + R[q][2] * c; }
                memcpy(R, T, sizeof R);
            } else if (o->est == 2 && o->tipo != 6 && o->tipo != 8) {   /* girando sobre su eje x local */
                float c = cosf(o->giro), s = sinf(o->giro), T[3][3];
                for (a = 0; a < 3; a++) { T[a][0] = R[a][0]; T[a][1] = R[a][1] * c + R[a][2] * s; T[a][2] = -R[a][1] * s + R[a][2] * c; }
                memcpy(R, T, sizeof R);
            }
            {   float k = o->tipo == 8 && o->est == 1 ? o->esc * 1.5f : o->esc;
                for (a = 0; a < 3; a++) { for (b = 0; b < 3; b++) e->M[a][b] = R[b][a] * k; e->M[a][3] = 0; }
            }
            e->M[3][0] = o->p[0]; e->M[3][1] = o->p[1] + (o->tipo == 8 && o->est == 1 ? 2.0f : 0.0f); e->M[3][2] = o->p[2]; e->M[3][3] = 1;
        }
    }
    return n;
}
