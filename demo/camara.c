/* Camara de carrera clonada del original. Fuentes (decompilacion de PC, comprobadas en SLES-53998):
     SetCarCamera 4A1680 (PS2 0x1817c0..0x181ac0): marco de la camara = velocidad proyectada en el suelo, girada la
         mitad del angulo hasta la carroceria; de el salen +1FE (giro), +1FC (cabeceo) y +200 (balanceo del suelo).
     484810 (PS2 0x1793xx): matrices del coche +280 (carroceria), +200 (pivote, giro de la camara, cabeceo y
         balanceo de la carroceria y la animacion del choque) y +240 (pivote, giro de la camara, cabeceo de la
         carroceria y balanceo del suelo); el pivote esta a +2C de la vista (-1: un metro por delante).
     483110 (PS2 0x179d80): giro de la vista (velocidad baja: hacia la carroceria; vistas 0/1: mas hacia la
         carroceria con la deriva; tras un choque con grua: del giro guardado al de la carroceria).
     4832F0 / 484A40 (PS2 0x17a020 / 0x17a190): vistas de choque 0x1B/0x1C y cambio de vista con el boton.
     485590 / 483CE0 / 4834A0 / 483600 (PS2 0x17a3d0 / 0x17b3a8 / 0x17a640..): ojo y mira locales de la tabla,
         cabeceo medio de 30 cuadros que sube el ojo, mezcla de 30 cuadros entre vistas y ojo 0,75 m sobre el suelo.
     484DF0 / 482F20 / 483C10 / 484BD0: angulos de la vista, FOV y proyeccion. */
#include <math.h>
#include <string.h>
#include "camara.h"

#define UNIDAD 9.58738019e-05f              /* 628254: 2 pi / 65536 */
#define PI_F 3.14159274f
#define VISTA(t, v) ((t)->vistas + (v) * 13)

typedef struct { float f[3][3]; float t[3]; } mat_t;   /* filas: x, y, z locales en el mundo; t traslacion */

static float envuelve(float a) { while (a > PI_F) a -= 2 * PI_F; while (a < -PI_F) a += 2 * PI_F; return a; }
static void nrm(float *v) { float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 0) { v[0] /= l; v[1] /= l; v[2] /= l; } }
static float pr(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static void cruz(const float *a, const float *b, float *o)
{ o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0]; }

/* 4494D0: (1 - t) a' + t b con a' llevado hacia b, envuelto a [-pi, pi] */
static float mezcla_ang(float a, float b, float t)
{
    float d = a - b;
    if (d > PI_F) a -= 2 * PI_F; else if (d < -PI_F) a += 2 * PI_F;
    return envuelve((1.0f - t) * a + b * t);
}

/* matriz del PC (vectores fila): p * Rz(z) * Rx(x) * Ry(y) + t */
static void mat_ang(mat_t *m, float y, float x, float z, const float *t)
{
    float cy = cosf(y), sy = sinf(y), cx = cosf(x), sx = sinf(x), cz = cosf(z), sz = sinf(z); int i;
    float e[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    for (i = 0; i < 3; i++) {
        float a = e[i][0], b = e[i][1], c = e[i][2], a1, b1, b2, c2;
        a1 = a * cz - b * sz; b1 = a * sz + b * cz;           /* Rz */
        b2 = b1 * cx - c * sx; c2 = b1 * sx + c * cx;        /* Rx */
        m->f[i][0] = a1 * cy + c2 * sy; m->f[i][1] = b2; m->f[i][2] = -a1 * sy + c2 * cy;   /* Ry */
    }
    m->t[0] = t[0]; m->t[1] = t[1]; m->t[2] = t[2];
}
static void mat_punto(const mat_t *m, const float *p, float *o)
{
    int j;
    for (j = 0; j < 3; j++) o[j] = p[0] * m->f[0][j] + p[1] * m->f[1][j] + p[2] * m->f[2][j] + m->t[j];
}
/* un giro local antes de la matriz: m = R * m (R en vectores fila) */
static void mat_pre(mat_t *m, const float r[3][3])
{
    float f[3][3]; int i, j;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) f[i][j] = r[i][0] * m->f[0][j] + r[i][1] * m->f[1][j] + r[i][2] * m->f[2][j];
    memcpy(m->f, f, sizeof f);
}

/* angulos del PC de un marco (adelante fw, arriba up): giro y = atan2(-fw.x, -fw.z), cabeceo = asin fw.y,
   balanceo = atan2(der.y, up.y) con der = fw x up */
static void angulos(const float *fw, const float *up, float *gy, float *gx, float *gz)
{
    float d[3];
    cruz(fw, up, d);
    *gy = atan2f(-fw[0], -fw[2]);
    *gx = asinf(fmaxf(-1.0f, fminf(1.0f, fw[1])));
    *gz = atan2f(d[1], up[1]);
}

void cam_inicia(cam_t *c, const cam_tablas_t *t)
{
    const float *v = VISTA(t, 30);
    int8_t el = c->elegida >= 0 && c->elegida <= 2 ? c->elegida : 2;
    memset(c, 0, sizeof *c);
    c->elegida = el;                                         /* 7C24B8: la vista elegida se conserva */
    c->vista = c->vista_ant = 30;                            /* 484EE0: +34A = +34B = 0x1E */
    memcpy(c->e0, v, 12); memcpy(c->ec, v + 3, 12); memcpy(c->e110, v, 12); memcpy(c->e11c, v + 3, 12);
    c->fov_a4 = c->fov_a8 = 0.872664630f;                    /* 5BA964 (PS2 0x178618): 50 grados */
    c->dx_c4 = c->dx_cc = v[6]; c->dy_c8 = c->dy_d0 = v[7];
    c->f358 = c->f35c = v[10]; c->mat34c = ((const uint32_t *)v)[12] != 0;
}

/* 483CE0: historia del cabeceo (+300..+33A) y subida del ojo de las vistas 2..21 */
static void cabeceo_ojo(cam_t *c, const cam_tablas_t *t, int16_t cabeceo, int modelo, float *e)
{
    int k, suma = cabeceo; const float *m; float a, r;
    for (k = 29; k > 0; k--) { c->hist[k] = c->hist[k - 1]; suma += c->hist[k]; }
    c->hist[0] = cabeceo;
    if (c->vista < 2 || c->vista > 21) return;
    m = t->vistas_mod + ((c->vista - 2) * 30 + modelo) * 9;
    a = (float)suma / 30.0f * UNIDAD * m[6];                 /* PS2: suma / 30.0 (el PC trunca la media a entero) */
    if (a >= m[7] * 0.0174532924f) a = m[7] * 0.0174532924f;
    if (m[8] * 0.0174532924f >= a) a = m[8] * 0.0174532924f;
    r = fabsf(e[5] - e[2]);
    e[1] = sinf(a) * r + e[1];
    e[2] = cosf(a) * r;
}

/* 4833E0 */
static void guarda(cam_t *c, int16_t n)
{
    int16_t antes = c->n33c;
    c->vista_ant = c->vista;
    memcpy(c->e110, c->e0, 12); memcpy(c->e11c, c->ec, 12);
    c->rol134 = c->rol130; c->mat350 = c->mat34c; c->fov_a8 = c->fov_a4;
    c->dx_cc = c->dx_c4; c->dy_d0 = c->dy_c8; c->f35c = c->f358;
    c->n33c = n;
    c->n368 = antes > 0 ? c->n368 + 1 : 0;
}

/* 4F6580 * 60,2 + 1 cuadros (ftol de -dur * 60,2) */
static int16_t cuadros(float dur) { return (int16_t)(1 + (int)(dur * 60.2000008f)); }

void cam_paso(cam_t *c, const cam_tablas_t *t, const cam_entrada_t *in)
{
    float gyb, gxb, gzb, gyc, rolc, cabc, vproj[3], bproj[3], f[3], z[3], x[3], k, mezcla, giro;
    float piv[3], ent[13], la, lz, mitad; mat_t m280, m200, m240; const float *v; int modelo = in->modelo, j;
    int16_t cab16;
    if (modelo < 0 || modelo >= 30) modelo = 0;
    angulos(in->fw, in->up, &gyb, &gxb, &gzb);               /* +2E, +2C, +30 de la carroceria */

    /* SetCarCamera (4A1680): marco del suelo con la velocidad; se mezcla con el morro por debajo de 0,015 m/cuadro */
    k = pr(in->vel, in->normal);
    for (j = 0; j < 3; j++) vproj[j] = in->vel[j] - in->normal[j] * k;
    mezcla = 0.015f - sqrtf(pr(vproj, vproj)); if (mezcla < 0) mezcla = 0;
    k = pr(in->fw, in->normal);
    for (j = 0; j < 3; j++) bproj[j] = in->fw[j] - in->normal[j] * k;
    nrm(bproj);
    for (j = 0; j < 3; j++) f[j] = vproj[j] + bproj[j] * mezcla;
    nrm(f);
    for (j = 0; j < 3; j++) z[j] = -f[j];                    /* fila z del marco: hacia atras */
    cruz(in->normal, z, x);                                  /* fila x: derecha */
    la = -pr(bproj, x); lz = -pr(bproj, z);                  /* la carroceria (hacia atras) en el marco */
    mitad = (float)((int)(atan2f(la, lz) * 10430.3779f) / 2) * UNIDAD;
    for (j = 0; j < 3; j++) { float zz = sinf(mitad) * x[j] + cosf(mitad) * z[j], xx = cosf(mitad) * x[j] - sinf(mitad) * z[j]; z[j] = zz; x[j] = xx; }
    gyc = atan2f(z[0], z[2]);                                /* +1FE */
    rolc = atan2f(x[1], in->normal[1]);                      /* +200 */
    {   float s = sinf(rolc), co = cosf(rolc), r = fabsf(s) > 0.707f ? x[1] / s : in->normal[1] / co;
        cabc = atan2f(-z[1], r); }                           /* +1FC */
    cab16 = (int16_t)(int)(cabc * 10430.3779f);

    /* camara de la salida (485250): matriz de la carroceria, vista 0x1E, FOV de 484EE0 */
    if (in->salida) {
        if (in->guion) {                                     /* 486EF0 (suceso 7, antes de la camara): +E0..+F4, +130, +A4 = +AC */
            memcpy(c->e0, in->guion, 12); memcpy(c->ec, in->guion + 3, 12);
            c->rol130 = in->guion[6]; c->fov_a4 = in->guion[7];
        }
        mat_ang(&m280, gyb, gxb, gzb, in->pos);
        c->vista = 30; c->mat34c = 1;
        mat_punto(&m280, c->e0, c->ojo); mat_punto(&m280, c->ec, c->mira);
        c->fov = c->fov_a4; c->balanceo = c->rol130; c->despl[0] = c->dx_c4; c->despl[1] = c->dy_c8;
        return;
    }
    if (c->vista == 30 && !in->agotado) { c->vista = c->elegida; guarda(c, 0); }   /* temporizador 0x12B: la vista elegida; 0xB5: 4833E0(0) */

    /* 483110: giro de la vista */
    {   float va = gyc, gd = envuelve(gyb - gyc);
        if (c->vista == 0 || c->vista == 1)
            va = gyc + (float)(int)(gd / UNIDAD * (c->vista == 0 ? 0.6f : 0.8f) * fabsf(sinf(envuelve(atan2f(-in->vel[0], -in->vel[2]) - gyb)))) * UNIDAD;
        if (c->n346 > 0) giro = mezcla_ang(c->giro_d8c, gyb, 1.0f - (float)c->n346 / (float)c->n348);
        else if (in->vel_mf > 0.00922849774f) giro = va;
        else if (in->vel_mf > 0.00461424887f) {
            float r = 1.0f - (in->vel_mf - 0.00461424887f) * 216.720001f;
            giro = mezcla_ang(va, gyb, fminf(1.0f, fmaxf(0.0f, r)));
        } else giro = gyb;
    }
    if (in->choque_nuevo) c->giro_d8c = gyc;                 /* StartCrash: +D8C = +1FE */

    /* 484810: pivote y matrices +200 / +240 */
    v = VISTA(t, c->vista);
    {   mat_t mb; float p0[3] = { 0, 0, v[11] }, o[3] = { 0, 0, 0 };
        mat_ang(&mb, gyb, gxb, 0, o); mat_punto(&mb, p0, piv);
        for (j = 0; j < 3; j++) piv[j] += in->pos[j];
    }
    mat_ang(&m200, giro, gxb, c->vista <= 21 ? gzb : 0, piv);
    if (in->choque_activo) {                                 /* animacion del choque: T(+2D8) Rz(+2EC) Ry(+2E8) Rx(+2E4) */
        float r[3][3], cx = cosf(in->pose_r[0]), sx = sinf(in->pose_r[0]), cy = cosf(in->pose_r[1]), sy = sinf(in->pose_r[1]);
        float cz = cosf(in->pose_r[2]), sz = sinf(in->pose_r[2]), q[3];
        mat_punto(&m200, in->pose_t, q); memcpy(m200.t, q, 12);
        { float rz[3][3] = { { cz, sz, 0 }, { -sz, cz, 0 }, { 0, 0, 1 } }; mat_pre(&m200, rz); }
        { float ry[3][3] = { { cy, 0, -sy }, { 0, 1, 0 }, { sy, 0, cy } }; mat_pre(&m200, ry); }
        { float rx[3][3] = { { 1, 0, 0 }, { 0, cx, sx }, { 0, -sx, cx } }; memcpy(r, rx, sizeof r); mat_pre(&m200, r); }
    }
    mat_ang(&m240, giro, gxb, rolc, piv);

    /* 485830 (PS2 0x178e58, modos 20..22: fin del tiempo): tras 484810, sin 4832F0 ni 484A40 (ni el boton de la vista ni
       la cuenta +346); fuera de las vistas 0x1A..0x1D guarda la actual (4833E0 con 0x1E cuadros) y toma la 0x1A (luego
       2057C0(0x16A, 1) y (0x16B, 1): RET en el PS2); despues 485590 con 0x1E cuadros como la de carrera */
    if (in->agotado) {
        if (c->vista < 26 || c->vista > 29) { guarda(c, 30); c->vista = 26; }
        goto ojo_mira;
    }
    /* 4832F0: temporizadores de las vistas de choque */
    if (c->n340 == 0 && in->choque_nuevo) {
        int16_t n = cuadros(in->choque_dur);
        if (in->grua > 2) { c->n346 = c->n348 = n; }
        if (in->choque_estado == 1 || in->choque_estado == 2 || in->choque_estado == 5) {
            c->k360 = (uint8_t)in->choque_estado;
            c->n340 = c->n342 = n;
            c->n344 = 0;                                     /* 780258 == 0: sin la vista 0x1D */
        }
    }
    /* 484A40: cambio de vista */
    if (c->n340 != 0 && c->k360 != 14) {
        if (c->n340 == c->n342) {
            int8_t bl = c->vista_ant;
            guarda(c, 30);
            if (c->vista == 29) c->vista_ant = bl;
            c->n340--;
            c->vista = c->k360 == 1 ? 28 : 27;
        } else if (c->n340 == 1) {
            int8_t bl = c->vista_ant;
            guarda(c, 30);
            if (c->n344 != 0) { c->n340--; c->vista_ant = bl; c->vista = 29; }
            else { c->vista = bl; c->n340--; }
        } else c->n340--;
    } else if (c->n344 != 0) {
        if (c->n344 == 1) { int8_t bl = c->vista_ant; guarda(c, 30); c->vista = bl; }
        c->n344--;
    } else if (in->boton_vista) {
        guarda(c, 30);
        c->vista = c->vista > 0 ? c->vista - 1 : 2;
        c->elegida = c->vista;                               /* SUPUESTO: la siguiente carrera empieza con ella */
    }
    if (c->n346 > 0) c->n346--;

    /* 485590: ojo y mira locales de la vista */
ojo_mira:
    v = VISTA(t, c->vista);
    memcpy(ent, v, sizeof ent);
    if (c->vista == 1) {
        const float *mm = t->modelos + modelo * 9;
        memcpy(ent, mm, 12); memcpy(ent + 3, mm, 12); ent[5] -= 1.0f;
    } else if (c->vista >= 2 && c->vista <= 21) {
        const float *mm = t->vistas_mod + ((c->vista - 2) * 30 + modelo) * 9;
        memcpy(ent, mm, 24);
    } else if (c->vista == 27 || c->vista == 28) ent[4] += in->pose_t[1];
    ent[2] -= ent[11];
    cabeceo_ojo(c, t, cab16, modelo, ent);
    {   int con = ((const uint32_t *)v)[12] != 0;
        int vg = (c->vista >= 2 && c->vista <= 21) || c->vista == 27 || c->vista == 28 || c->vista == 29;
        if (c->n33c != 0) {                                  /* 483600: mezcla desde la vista anterior */
            float tt = (float)c->n33c / 30.0f, u = 1.0f - tt, a[3][3], ta[3], r; mat_t mm;
            const mat_t *pa = c->mat350 ? &m200 : &m240, *pb = con ? &m200 : &m240;
            int i;
            for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) a[i][j] = (pb->f[i][j] - pa->f[i][j]) * u + pa->f[i][j];
            nrm(a[2]); k = pr(a[0], a[2]);
            for (j = 0; j < 3; j++) a[0][j] -= k * a[2][j];
            nrm(a[0]); cruz(a[2], a[0], a[1]); nrm(a[1]);
            for (j = 0; j < 3; j++) ta[j] = pb->t[j] * u + pa->t[j] * tt;
            memcpy(mm.f, a, sizeof a); memcpy(mm.t, ta, 12);
            for (j = 0; j < 3; j++) { c->e0[j] = c->e110[j] * tt + ent[j] * u; c->ec[j] = c->e11c[j] * tt + ent[3 + j] * u; }
            mat_punto(&mm, c->e0, c->ojo); mat_punto(&mm, c->ec, c->mira);
            r = ent[8];
            if (!con && vg) r += rolc;                       /* con la carroceria el PC lo calcula y lo pisa */
            c->rol130 = c->rol134 * tt + r * u;
            c->fov_a4 = c->fov_a8 * tt + ent[9] * u;
            c->dx_c4 = c->dx_cc * tt + ent[6] * u; c->dy_c8 = c->dy_d0 * tt + ent[7] * u;
            c->f358 = c->f35c * tt + ent[10] * u;
            c->n33c--;
        } else {                                             /* 4834A0: directamente la vista */
            const mat_t *pm = con ? &m200 : &m240;
            c->mat34c = con;
            memcpy(c->e0, ent, 12); memcpy(c->ec, ent + 3, 12);
            mat_punto(pm, c->e0, c->ojo); mat_punto(pm, c->ec, c->mira);
            c->rol130 = ent[8];
            if (con) { if (c->vista <= 1) c->rol130 += (float)((int)(rolc / UNIDAD) - (int)(rolc / UNIDAD) / 4) * UNIDAD + in->pose_r[2]; }
            else if (vg) c->rol130 += rolc;
            c->fov_a4 = ent[9]; c->dx_c4 = ent[6]; c->dy_c8 = ent[7]; c->f358 = ent[10];
        }
    }
    if (in->suelo) { float y = in->suelo(c->ojo) + 0.75f; if (isfinite(y) && y >= c->ojo[1]) c->ojo[1] = y; }   /* 6282A0 */

    /* 482E80: el ojo dentro de la caja del coche */
    {   const float *b = t->modelos + modelo * 9 + 3; float zx = c->e0[2] - 1.0f;
        c->dentro = !(c->e0[0] > b[0] || c->e0[1] > b[1] || zx > b[2] || b[3] > c->e0[0] || b[4] > c->e0[1] || b[5] > zx); }
    c->fov = c->fov_a4; c->balanceo = c->rol130; c->despl[0] = c->dx_c4; c->despl[1] = c->dy_c8;
}

void cam_traslada(cam_t *c, float dx, float dy, float dz)
{
    c->ojo[0] -= dx; c->ojo[1] -= dy; c->ojo[2] -= dz;
    c->mira[0] -= dx; c->mira[1] -= dy; c->mira[2] -= dz;
}

int cam_coche_visible(const cam_t *c)
{
    if (c->n33c > 0) return !c->dentro;                      /* 4A2BA0 en la mezcla: por fuera salvo dentro de la caja */
    return c->vista >= 2;                                    /* 0: sin coche; 1: interior (no lo hay en la demo) */
}
