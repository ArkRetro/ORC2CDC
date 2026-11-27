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
} obj_t;
static uint8_t *cab;                     /* el primer sector de HAOSO.DAT */
static uint32_t lba_o, tam_o, nfich;
static obj_t *ob; static int nob;
static int toq_cl[16], ntoq;

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
    free(ob); ob = NULL; nob = 0; ntoq = 0;
    if (!cab || mision < 0 || mision > 40) return;
    f = cab[8 + (preset == 1 ? 41 : 0) + mision];      /* 45C470: el preset 1 (OutRun 2) o el 0 */
    if (f == 0xff || f >= nfich) return;
    memcpy(&off, cab + 8 + 84 + 512 + 8 * f, 4); memcpy(&n, cab + 8 + 84 + 512 + 8 * f + 4, 4);
    if (!n || off + 28 * n > tam_o) return;
    s0 = off & ~2047u; ns = (off + 28 * n - s0 + 2047) >> 11;
    if (!(b = memalign(32, ns << 11))) return;
    if (gd_lee(lba_o + (s0 >> 11), (int)ns, b) || !(ob = calloc(n, sizeof(obj_t)))) { free(b); return; }
    for (i = 0, k = 0; i < n; i++) {
        const uint8_t *r = b + (off - s0) + 28 * i; uint8_t cl = r[0]; uint16_t reg; float esc;
        if (cl >= 64) continue;
        memcpy(&reg, cab + 8 + 84 + 8 * cl, 2); memcpy(&esc, cab + 8 + 84 + 8 * cl + 4, 4);
        if (reg == 0xffff) continue;                    /* de otro recurso (la luz 56 de FLOR, la clase 3): no van */
        ob[k].cl = cl; ob[k].tipo = r[1]; ob[k].reg = (int16_t)reg; ob[k].esc = esc;
        memcpy(ob[k].l, r + 4, 12); memcpy(ob[k].a, r + 16, 12);
        k++;
    }
    nob = k; free(b);
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
    ntoq = 0;
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
            if (o->cl == 0x0a || o->cl == 0x22 || o->cl == 0x39) {
                int r; y = 9999.9f;
                if (!c->suelo || (r = c->suelo(o->p[0], o->p[2], &y)) == -2) continue;   /* sin la colision aun */
                if (r >= 0) o->p[1] = y;
            }
            if (o->cl >= 5 && o->cl <= 9) o->p[1] += 0.65f;   /* 0x5b0078 */
            euler(o->R, o->a, m3);
            o->est = 1;
#ifdef DIAG_HA
            if (i < 3) dbglog(DBG_INFO, "DIAGHA objeto %d clase %d: local (%.1f, %.1f, %.1f) mundo (%.1f, %.1f, %.1f); coche (%.1f, %.1f, %.1f), marco (%.1f, %.1f, %.1f, %.3f)\n",
                              i, o->cl, (double)o->l[0], (double)o->l[1], (double)o->l[2], (double)o->p[0], (double)o->p[1], (double)o->p[2],
                              (double)c->pos[0], (double)c->pos[1], (double)c->pos[2], (double)c->marco[0], (double)c->marco[1], (double)c->marco[2], (double)c->marco[3]);
#endif
        }
#ifdef PRUEBA_HA_TOQUE
        if (o->est == 1 && (o->cl == PRUEBA_HA_TOQUE || PRUEBA_HA_TOQUE < 0)) {   /* prueba: los de esa clase, a la linea del coche */
            float dx = o->p[0] - c->pos[0], dz = o->p[2] - c->pos[2], al = dx * fx + dz * fz, la = dx * fz - dz * fx;
            if (al > 25.0f && al < 40.0f && fabsf(la) < 25.0f && !o->rg) { o->p[0] = c->pos[0] + fx * al; o->p[2] = c->pos[2] + fz * al; o->rg = 1; }
        }
#endif
        if (o->est == 1) {                              /* toque del jugador (0x1d3720) */
            float dx = o->p[0] - c->pos[0], dz = o->p[2] - c->pos[2], dy = o->p[1] - c->pos[1], al, la;
            if (fabsf(dx) > 4.0f || fabsf(dz) > 4.0f || fabsf(dy) > 3.0f) continue;
            al = dx * fx + dz * fz; la = dx * fz - dz * fx;
            if (fabsf(al) < 2.3f + 0.3f && fabsf(la) < 1.1f + 0.3f) {
                if (ntoq < 16) toq_cl[ntoq++] = o->cl;
                o->est = 2; o->t = 0;
                o->v[0] = fx * c->v * 1.1f + (la >= 0 ? fz : -fz) * 3.0f; o->v[2] = fz * c->v * 1.1f - (la >= 0 ? fx : -fx) * 3.0f;
                o->v[1] = 5.0f + c->v * 0.1f; o->giro = 0;
            }
        } else if (o->est == 2) {                       /* SIMPLIFICADO: vuelo balistico y fuera a los 2 s */
            const float dt = 1.0f / 60.0f;
            o->p[0] += o->v[0] * dt; o->p[1] += o->v[1] * dt; o->p[2] += o->v[2] * dt; o->v[1] -= 9.8f * dt;
            o->giro += 0.25f;
            if (++o->t > 120) o->est = 3;
        }
    }
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
int haobj_cuenta(int clase) { int i, n = 0; for (i = 0; i < nob; i++) if (ob[i].cl == clase && ob[i].est < 2) n++; return n; }
int haobj_lista(haobj_dib_t *v, int max, const float ojo[3])
{
    static float dd[64]; int i, n = 0, j;
    if (max > 64) max = 64;
    for (i = 0; i < nob; i++) {
        const obj_t *o = ob + i; float dx, dy, dz, d; haobj_dib_t *e;
        if (o->est != 1 && o->est != 2) continue;
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
            if (o->est == 2) {                          /* girando sobre su eje x local */
                float c = cosf(o->giro), s = sinf(o->giro), T[3][3];
                for (a = 0; a < 3; a++) { T[a][0] = R[a][0]; T[a][1] = R[a][1] * c + R[a][2] * s; T[a][2] = -R[a][1] * s + R[a][2] * c; }
                memcpy(R, T, sizeof R);
            }
            for (a = 0; a < 3; a++) { for (b = 0; b < 3; b++) e->M[a][b] = R[b][a] * o->esc; e->M[a][3] = 0; }
            e->M[3][0] = o->p[0]; e->M[3][1] = o->p[1]; e->M[3][2] = o->p[2]; e->M[3][3] = 1;
        }
    }
    return n;
}
