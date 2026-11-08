/*
 * prueba_fisica.c - banco de pruebas en PC para comun/or2_fisica.c
 *
 * gcc -O2 -std=c99 -Wall -I../../comun prueba_fisica.c ../../comun/or2_fisica.c -lm -o prueba_fisica
 * ./prueba_fisica PARAM.DAT [columna|m<modelo>] [SLES_539.98|-] [choques]
 *
 * PARAM.DAT de tools/conv_param.py (tabla, curvas y bloques de choques). "m6" elige la columna del modelo 6
 * (Testarossa) con la tabla de columnas del juego. "choques" solo hace la aceleracion y las pruebas de choques.
 *
 * Si se da el ELF de PS2 se leen las curvas reales:
 *   par motor (automatico): desplazamiento de fichero 0x2db5a0 (VA 0x3db520), 162 floats
 *   presion de freno:       desplazamiento de fichero 0x2db1a0 (VA 0x3db120), 256 floats
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "or2_fisica.h"

#define HZ 60.2f
#define DT (1.0f / 60.2f)

static float tabla[200 * OR2_COLUMNAS];
static int filas;
static float fichero[65536];         /* PARAM.DAT entero (alineado a 4) */
static uint32_t tam_fichero;
static or2_choques_t choques;
static int hay_choques;

static int carga_tabla(const char *ruta)
{
    FILE *f = fopen(ruta, "rb");
    size_t n;
    if (!f) return -1;
    n = fread(fichero, 1, sizeof(fichero), f);
    fclose(f);
    tam_fichero = (uint32_t)n;
    if (n > sizeof(tabla)) n = sizeof(tabla);
    memcpy(tabla, fichero, n);
    filas = (int)(n / 4 / OR2_COLUMNAS);
    if (filas == 200 && (uint32_t)(200 * OR2_COLUMNAS + 162 + 256) * 4 <= tam_fichero) {
        const float *c = fichero + 200 * OR2_COLUMNAS;
        if (fabsf(c[0] - 0.12732396f) < 1e-5f) { /* curvas en el fichero: se usan en main */ }
    }
    hay_choques = or2_carga_choques(fichero, tam_fichero, &choques) == 0;
    return filas >= OR2_FILAS_MIN ? 0 : -2;
}

static int carga_curvas(const char *elf, or2_param_t *p)
{
    float par[162], fr[256];
    FILE *f = fopen(elf, "rb");
    if (!f) return -1;
    if (fseek(f, 0x2db5a0, SEEK_SET) || fread(par, 4, 162, f) != 162 ||
        fseek(f, 0x2db1a0, SEEK_SET) || fread(fr, 4, 256, f) != 256) { fclose(f); return -2; }
    fclose(f);
    if (fabsf(par[0] - 0.12732396f) > 1e-5f) return -3;
    or2_carga_curvas(p, par, fr);
    return 0;
}

static void entrada_libre(or2_entrada_t *e)
{
    memset(e, 0, sizeof(*e));
    e->mu = 1.0f;
    e->en_suelo = 1;
}

/* fisico (m/s) para una velocidad de velocimetro (km/h) */
static float fis_de_hud(const or2_param_t *p, float hud)
{
    float k = p->hud * 3.6f;
    if (!(p->arrastre_lat > 0.0f)) k *= 1.0101f;
    return hud / k;
}

static void prueba_aceleracion(const or2_param_t *p)
{
    or2_coche_t c;
    or2_entrada_t e;
    int i, g = 0, hito = 0;
    const float hitos[3] = {100.0f, 200.0f, 250.0f};
    float vmax = 0.0f, hudmax = 0.0f, t;
    printf("\n== Aceleracion a fondo desde parado (recta, cambio automatico) ==\n");
    or2_inicia(&c, p);
    entrada_libre(&e);
    e.acelerador = 1.0f;
    for (i = 0; i < (int)(70.0f * HZ); ++i) {
        or2_paso(&c, p, &e, 0.0f);
        t = (float)(i + 1) * DT;
        if (c.marcha != g) {
            printf("  t=%6.2f s  marcha %d->%d  hud=%6.1f km/h  fis=%6.1f km/h  rpm=%5.0f\n",
                   t, g, c.marcha, c.velocidad_hud, c.velocidad * 3.6f, c.rpm);
            g = c.marcha;
        }
        while (hito < 3 && c.velocidad_hud >= hitos[hito]) {
            printf("  0-%3.0f km/h (velocimetro): %6.2f s   (fisico %6.1f km/h)\n",
                   hitos[hito], t, c.velocidad * 3.6f);
            ++hito;
        }
        if (c.velocidad > vmax) { vmax = c.velocidad; hudmax = c.velocidad_hud; }
    }
    printf("  velocidad punta (70 s): %.1f m/s = %.1f km/h fisicos, velocimetro %.1f km/h, rpm %.0f marcha %d\n",
           vmax, vmax * 3.6f, hudmax, c.rpm, c.marcha);
    /* frenada desde la punta */
    {
        int n = 0;
        float v0 = c.velocidad;
        e.acelerador = 0.0f; e.freno = 1.0f;
        while (c.velocidad > fis_de_hud(p, 100.0f) && n < 2000) { or2_paso(&c, p, &e, 0.0f); ++n; }
        printf("  frenada a fondo %.0f -> 100 km/h (velocimetro): %.2f s, %.2f g medios\n",
               v0 * p->hud * 3.6f, n * DT, (v0 - c.velocidad) / (n * DT) / 9.80665f);
    }
}

/* carretera circular de radio R hacia la derecha, para la prueba de derrape */
typedef struct { float cx, cy, R, ancho; } curva_t;

static void carretera(const curva_t *k, float x, float y, or2_entrada_t *e)
{
    /* el coche recorre la circunferencia en sentido horario (giro a derecha) */
    float dx = x - k->cx, dy = y - k->cy, r = sqrtf(dx * dx + dy * dy);
    float th = atan2f(dy, dx);                 /* angulo polar */
    float rumbo = atan2f(sinf(th), -cosf(th)); /* tangente horaria, + der. */
    float th2 = th - 40.0f / k->R;             /* punto 40 m por delante */
    float ax = k->cx + k->R * cosf(th), ay = k->cy + k->R * sinf(th);
    float bx = k->cx + k->R * cosf(th2), by = k->cy + k->R * sinf(th2);
    e->rumbo_carretera = rumbo;
    e->rumbo_adelante = atan2f(bx - ax, by - ay);
    /* borde exterior (izquierda en giro a derecha) a R+ancho/2 */
    e->dist_izq = (k->R + k->ancho * 0.5f) - r;
    e->dist_der = r - (k->R - k->ancho * 0.5f);
}

static void prueba_derrape(const or2_param_t *p, int con_carretera, float R, float st_hold)
{
    or2_coche_t c;
    or2_entrada_t e;
    curva_t k;
    int i;
    float x = 0.0f, y = 0.0f, v0 = fis_de_hud(p, 200.0f);
    printf("\n== Derrape: 200 km/h (velocimetro), volante a la derecha + toque de freno, "
           "stick %.2f desde 0.8 s, %s R=%.0f m ancho 14 m ==\n", st_hold,
           con_carretera ? "curva" : "SIN datos de carretera (rumbos 0, sin bordes)", R);
    printf("  cuadro  t(s)  stick acel fren temp derr  desliz(gr) beta(gr) guin(gr/s)  hud(km/h)  marcha  dist_izq\n");
    or2_coloca(&c, p, 0.0f, v0);
    entrada_libre(&e);
    k.R = R; k.ancho = 14.0f; k.cx = k.R; k.cy = 0.0f;   /* centro a la derecha */
    for (i = 0; i < (int)(3.2f * HZ); ++i) {
        float t = i * DT;
        e.stick = 0.0f; e.acelerador = 1.0f; e.freno = 0.0f;
        if (t >= 0.10f) e.stick = 1.0f;
        if (t >= 0.10f && t < 0.20f) { e.acelerador = 0.0f; e.freno = 1.0f; }  /* toque */
        if (t >= 0.20f && t < 0.35f) e.acelerador = 0.0f;
        if (t >= 0.8f) e.stick = st_hold;
        if (t >= 2.4f) { e.stick = -0.4f; e.acelerador = 0.0f; }   /* salida: algo de contravolante y soltar */
        if (con_carretera) carretera(&k, x, y, &e);
        or2_paso(&c, p, &e, 0.0f);
        x += sinf(c.rumbo_vel) * c.velocidad * DT;
        y += cosf(c.rumbo_vel) * c.velocidad * DT;
        printf("  %5d %5.2f  %5.2f %4.2f %4.2f %4d %4d  %9.2f %8.2f %10.1f  %9.1f  %5d  %7.2f\n",
               i, t, e.stick, e.acelerador, e.freno, c.temporizador, c.derrape,
               c.desliz_grados, c.beta * 57.29578f, c.guinada * 57.29578f,
               c.velocidad_hud, c.marcha, con_carretera ? e.dist_izq : 0.0f);
    }
}

static void prueba_viraje(const or2_param_t *p, float stick)
{
    int s;
    printf("\n== Viraje estable sin derrape (stick=%.2f, velocidad mantenida 6 s) ==\n", stick);
    printf("  hud(km/h) fis(m/s)  guinada(gr/s)  radio(m)  ac.lat(g)  beta(gr)  desliz tr.(gr) derrape\n");
    for (s = 40; s <= 280; s += 20) {
        or2_coche_t c;
        or2_entrada_t e;
        int i, n = 0;
        float v = fis_de_hud(p, (float)s), sw = 0.0f, sb = 0.0f, sd = 0.0f, sv = 0.0f;
        int dr = 0;
        or2_coloca(&c, p, 0.0f, v);
        entrada_libre(&e);
        e.stick = stick;
        for (i = 0; i < (int)(6.0f * HZ); ++i) {
            /* regulador proporcional del acelerador para mantener v */
            float err = v - c.velocidad;
            e.acelerador = err > 0.0f ? (err * 0.6f > 1.0f ? 1.0f : err * 0.6f + 0.25f) : 0.0f;
            if (e.acelerador > 1.0f) e.acelerador = 1.0f;
            or2_paso(&c, p, &e, 0.0f);
            dr |= c.derrape;
            if (i >= (int)(4.0f * HZ)) {
                sw += c.guinada; sb += c.beta; sd += c.desliz_grados; sv += c.velocidad; ++n;
            }
        }
        sw /= n; sb /= n; sd /= n; sv /= n;
        printf("  %8d %8.1f %12.1f %10.1f %9.2f %9.2f %13.2f %6d\n", s, sv, sw * 57.29578f,
               fabsf(sw) > 1e-4f ? sv / fabsf(sw) : 0.0f, sv * fabsf(sw) / 9.80665f,
               sb * 57.29578f, sd, dr);
    }
}

/* rpm del motor y rpm que corresponden a la velocidad (sin deslizamiento) al subir cada marcha */
static void prueba_cambios(const or2_param_t *p)
{
    or2_coche_t c;
    or2_entrada_t e;
    int i, g = 0;
    printf("\n== Cambios a fondo (automatico): velocidad del velocimetro, rpm del motor y de la carretera ==\n");
    or2_inicia(&c, p);
    entrada_libre(&e);
    e.acelerador = 1.0f;
    for (i = 0; i < (int)(40.0f * HZ); ++i) {
        float rpm_ant = c.rpm, hud_ant = c.velocidad_hud, rc;
        int g0 = c.marcha;
        or2_paso(&c, p, &e, 0.0f);
        if (c.marcha != g && g0 > 0) {
            rc = c.velocidad / p->radio[1] * p->rel[g0] * p->rel_final * p->rel_extra * 9.5493f;
            printf("  %d->%d  %6.1f km/h  motor %5.0f rpm  carretera %5.0f rpm  (umbral %5.0f rpm, corte %5.0f)\n",
                   g0, c.marcha, hud_ant, rpm_ant, rc, p->w_sube * 9.5493f, p->w_corte * 9.5493f);
        }
        g = c.marcha;
    }
}

/* un coche de trafico delante a v_otro (m/s), el jugador llega a hud km/h; desfase lateral dx (m) */
static void prueba_alcance(const or2_param_t *p, float hud, float v_otro, float dx, int clase, float stick)
{
    or2_coche_t c;
    or2_entrada_t e;
    or2_otro_t o;
    int i;
    float v = fis_de_hud(p, hud), antes;
    or2_coloca(&c, p, 0.0f, v);
    entrada_libre(&e);
    e.acelerador = 1.0f; e.stick = stick;
    for (i = 0; i < (stick != 0.0f ? 60 : 30); ++i) or2_paso(&c, p, &e, 0.0f);
    antes = c.velocidad_hud;
    memset(&o, 0, sizeof o);
    o.dx = dx + sinf(c.rumbo_vel) * 4.5f; o.dy = cosf(c.rumbo_vel) * 4.5f; o.vx = 0.0f; o.vy = v_otro; o.clase = clase; o.frontal = 1;
    or2_choque_coche(&c, p, &choques, &o);
    printf("  %5.0f km/h contra %4.1f m/s (%3.0f km/h) clase %d dx %.1f: queda a %5.1f km/h, estado %d (%.2f s), "
           "otro a %4.1f m/s lat %4.1f, golpe %d, reparto %.2f\n", antes, v_otro, v_otro * p->hud * 3.6f, clase,
           dx, sqrtf(c.vx * c.vx + c.vy * c.vy) * p->hud * 3.6f, c.t_choque > 0 ? c.choque : -1, c.t_choque, o.vy, o.vx, o.golpe, o.reparto);
    printf("      despues (gas a fondo, volante %.1f):", stick);
    for (i = 1; i <= (int)(2.0f * HZ); ++i) {
        or2_paso(&c, p, &e, 0.0f);
        if (i % 20 == 0)
            printf(" [%.1fs %3.0fkm/h beta %4.1f guin %5.1f pose(y %.2f g %.2f)]", i * DT, c.velocidad_hud,
                   c.beta * 57.29578f, c.guinada * 57.29578f, c.pose_t[1], c.pose_r[1] * 57.29578f);
    }
    printf("\n");
}

/* muro a la izquierda (normal +x), el coche a hud km/h con angulo ang grados hacia el muro */
static void prueba_muro(const or2_param_t *p, float hud, float ang)
{
    or2_coche_t c;
    or2_entrada_t e;
    int i, r = 0, toques = 0;
    float v = fis_de_hud(p, hud), x = 0.5f, antes;
    or2_coloca(&c, p, -ang / 57.29578f, v);
    entrada_libre(&e);
    e.acelerador = 1.0f;
    or2_paso(&c, p, &e, 0.0f);
    antes = c.velocidad_hud;
    printf("  %3.0f km/h a %4.1f gr:", antes, ang);
    for (i = 0; i < (int)(2.0f * HZ); ++i) {
        e.dist_izq = x + 7.0f; e.dist_der = 7.0f - x;   /* calzada de 14 m, muro izquierdo en x = -7 */
        or2_paso(&c, p, &e, 0.0f);
        x += sinf(c.rumbo_vel) * c.velocidad * DT + c.corr_lat;   /* correccion lateral del rebote */
        c.corr_lat = 0.0f;
        if (x < -6.0f) {
            r |= or2_choque_muro(&c, p, 1.0f, 0.0f, -6.0f - x);
            x = -6.0f; ++toques;
        }
        if (i % 15 == 14)
            printf(" [%.2fs %3.0f %5.1fgr x%5.1f]", (i + 1) * DT, c.velocidad_hud, c.rumbo_vel * 57.29578f, x);
    }
    printf("  %s, %d cuadros de contacto\n", r ? "rebote" : "roce", toques);
}

/* choque fuerte: recta con muro a la izquierda (x = -7), el coche a hud km/h con angulo ang grados hacia el muro,
   gas a fondo. La grua lleva el coche al centro de la calzada 25 m por delante (10 posiciones de la carretera
   del original, ~2,5 m cada una, como en main.c) */
static void prueba_choque_fuerte(const or2_param_t *p, float hud, float ang, float x0)
{
    or2_coche_t c;
    or2_entrada_t e;
    int i, r, estado = -1, golpe = 0, fin = -1;
    float v = fis_de_hud(p, hud), x = x0, y = 0.0f, ys = 0.0f, antes, pmax = 0.0f, gmax = 0.0f;
    or2_coloca(&c, p, -ang / 57.29578f, v);
    entrada_libre(&e);
    e.acelerador = 1.0f;
    or2_paso(&c, p, &e, 0.0f);
    antes = c.velocidad_hud;
    printf("  %3.0f km/h (%3d del modelo) a %4.1f gr:", antes, c.kmh, ang);
    for (i = 0; i < (int)(4.0f * HZ); ++i) {
        e.dist_izq = x + 7.0f; e.dist_der = 7.0f - x;
        e.grua_ok = c.grua > 0; e.grua_dx = 0.0f - x; e.grua_dy = ys - y; e.grua_rumbo = 0.0f;
        or2_paso(&c, p, &e, 0.0f);
        x += sinf(c.rumbo_vel) * c.velocidad * DT + c.corr_lat; c.corr_lat = 0.0f;
        y += cosf(c.rumbo_vel) * c.velocidad * DT;
        if (c.recolocar) { c.recolocar = 0; x = 0.0f; y = ys; fin = i; }
        if (x < -6.0f) {
            r = or2_choque_muro(&c, p, 1.0f, 0.0f, -6.0f - x);
            x = -6.0f;
            if (r == 2 && !golpe) { golpe = 1; estado = c.choque; ys = y + 25.0f;
                printf(" estado %d (%.2f s, lado %d, grua %d cuadros) a %.2fs;", c.choque, c.t_choque, c.lado, c.grua, i * DT); }
        }
        if (c.t_choque > 0.0f) {
            if (fabsf(c.pose_t[1]) > pmax) pmax = fabsf(c.pose_t[1]);
            if (fabsf(c.pose_r[0]) + fabsf(c.pose_r[1]) + fabsf(c.pose_r[2]) > gmax)
                gmax = fabsf(c.pose_r[0]) + fabsf(c.pose_r[1]) + fabsf(c.pose_r[2]);
        }
        if (i % 30 == 29)
            printf(" [%.1fs %3.0f x%5.1f y%6.1f%s]", (i + 1) * DT, c.velocidad_hud, x, y, c.grua ? " G" : "");
    }
    if (!golpe) printf("  sin choque fuerte\n");
    else printf("\n      estado %d: pose max. altura %.2f m, giro %.2f rad; grua acaba a %.2fs en x %.1f y %.1f, "
                "despues %.0f km/h\n", estado, pmax, gmax, fin * DT, 0.0f, ys, c.velocidad_hud);
}

/* punta a fondo 60 s con un coche 15 m delante a la misma velocidad (rebufo) o sin el */
static void prueba_rebufo(const or2_param_t *p)
{
    int k, i;
    for (k = 0; k < 2; ++k) {
        or2_coche_t c;
        or2_entrada_t e;
        or2_otro_t o;
        or2_coloca(&c, p, 0.0f, fis_de_hud(p, 250.0f));
        entrada_libre(&e);
        e.acelerador = 1.0f;
        for (i = 0; i < (int)(60.0f * HZ); ++i) {
            memset(&o, 0, sizeof o);
            o.dy = 15.0f; o.vy = c.velocidad;
            if (k) or2_rebufo(&c, &o, 1);
            or2_paso(&c, p, &e, 0.0f);
        }
        printf("  %s: punta %.1f km/h (velocimetro), rebufo %.2f, aire quitado %.2f\n", k ? "con rebufo" : "sin rebufo",
               c.velocidad_hud, c.rebufo, c.rebufo_k);
    }
}

static void prueba_choques(const or2_param_t *p)
{
    if (!hay_choques) { printf("\n(sin bloques de choques en el fichero)\n"); return; }
    printf("\n== Choques: umbrales %.3f/%.3f m/cuadro, restitucion %.2f %.2f %.2f, masa %.2f %.2f %.2f ==\n",
           choques.umbral[0], choques.umbral[1], choques.rest[0], choques.rest[1], choques.rest[2],
           choques.masa[0], choques.masa[1], choques.masa[2]);
    printf("  estado 7: %.2f s; trafico 16..19: %.2f %.2f %.2f %.2f s\n", choques.dur[7], choques.dur[16],
           choques.dur[17], choques.dur[18], choques.dur[19]);
    printf("-- alcanzar a un coche (por detras):\n");
    prueba_alcance(p, 120.0f, 36.0f, 0.0f, 3, 0.0f);
    prueba_alcance(p, 200.0f, 36.0f, 0.0f, 3, 0.0f);
    prueba_alcance(p, 280.0f, 40.0f, 0.0f, 3, 0.0f);
    prueba_alcance(p, 280.0f, 40.0f, 1.2f, 3, 0.0f);
    prueba_alcance(p, 200.0f, 36.0f, 0.0f, 0, 0.0f);
    prueba_alcance(p, 200.0f, 36.0f, 0.0f, 3, 0.6f);
    printf("-- muro:\n");
    prueba_muro(p, 200.0f, 2.0f);
    prueba_muro(p, 200.0f, 8.0f);
    prueba_muro(p, 200.0f, 25.0f);
    prueba_muro(p, 60.0f, 25.0f);
    printf("-- choque fuerte contra el muro (punta al corte %d km/h del modelo: nada hasta %d, estado 1 desde %d):\n",
           (int)(p->v_tope * 216.72f), (int)(p->v_tope * 216.72f) - 70, (int)(p->v_tope * 216.72f) - 20);
    prueba_choque_fuerte(p, 290.0f, 50.0f, 0.0f);
    prueba_choque_fuerte(p, 265.0f, 50.0f, 0.0f);
    prueba_choque_fuerte(p, 265.0f, 30.0f, 0.0f);
    prueba_choque_fuerte(p, 200.0f, 60.0f, 0.0f);
    printf("-- rebufo:\n");
    prueba_rebufo(p);
}

/* fuera del asfalto: velocidad tras 5 s a fondo partiendo de 200 km/h y radio de giro con el volante a 0,5, en
   asfalto (material 1) y en tierra (material 3: mu 0,7 y resistencia de SetRunningResistance) */
static void prueba_tierra(const or2_param_t *p)
{
    static const uint32_t sup[2] = { 1u << 1, 1u << 3 };
    static const char *nom[2] = { "asfalto", "tierra " };
    int k, i;
    printf("\n== Superficie (GetRoadMu + SetRunningResistance) ==\n");
    for (k = 0; k < 2; ++k) {
        or2_coche_t c;
        or2_entrada_t e;
        float v5, gmax = 0.0f;
        entrada_libre(&e);
        for (i = 0; i < 4; ++i) e.sup[i] = sup[k];
        e.sup_centro = sup[k];
        or2_coloca(&c, p, 0.0f, fis_de_hud(p, 200.0f));
        e.acelerador = 1.0f;
        for (i = 0; i < (int)(5.0f * HZ); ++i) or2_paso(&c, p, &e, 0.0f);
        v5 = c.velocidad_hud;
        e.acelerador = 0.0f;
        for (i = 0; i < (int)(3.0f * HZ); ++i) or2_paso(&c, p, &e, 0.0f);
        printf("  %s: a fondo 5 s desde 200 -> %5.1f km/h; sin gas 3 s mas -> %5.1f km/h; mu %.2f/%.2f",
               nom[k], v5, c.velocidad_hud, c.r[0].mu, c.r[2].mu);
        or2_coloca(&c, p, 0.0f, fis_de_hud(p, 120.0f));
        e.acelerador = 0.6f; e.stick = 0.5f;
        for (i = 0; i < (int)(4.0f * HZ); ++i) {
            or2_paso(&c, p, &e, 0.0f);
            if (i > 60) { float g = fabsf(c.guinada * c.velocidad) / 9.80665f; if (g > gmax) gmax = g; }
        }
        printf("; giro a 0,5 desde 120: %.2f g max, %5.1f km/h a los 4 s\n", gmax, c.velocidad_hud);
    }
}

int main(int argc, char **argv)
{
    or2_param_t p;
    int col = 6, r, solo_choques = argc > 4 && !strcmp(argv[4], "choques");
    if (argc < 2) {
        fprintf(stderr, "uso: %s PARAM.DAT [columna|m<modelo>] [SLES_539.98|-] [choques]\n", argv[0]);
        return 1;
    }
    if (carga_tabla(argv[1])) { fprintf(stderr, "no puedo leer la tabla\n"); return 1; }
    if (argc > 2) {
        if (argv[2][0] == 'm') {
            col = or2_columna_modelo(&choques, atoi(argv[2] + 1));
            printf("modelo %d -> columna %d\n", atoi(argv[2] + 1), col);
            if (col < 0) return 1;
        } else col = atoi(argv[2]);
    }
    r = or2_carga_param(tabla, filas, col, &p);
    if (r) { fprintf(stderr, "or2_carga_param: %d\n", r); return 1; }
    if (tam_fichero >= (uint32_t)(200 * OR2_COLUMNAS + 162 + 256) * 4) {
        or2_carga_curvas(&p, fichero + 200 * OR2_COLUMNAS, fichero + 200 * OR2_COLUMNAS + 162);
        printf("curvas de PARAM.DAT\n");
    } else if (argc > 3 && strcmp(argv[3], "-")) {
        r = carga_curvas(argv[3], &p);
        printf("curvas del ELF: %s\n", r ? "NO (aproximadas)" : "si");
    } else printf("curvas aproximadas (sin ELF)\n");
    if (getenv("ACEL")) { p.par_mult *= (float)atof(getenv("ACEL")); printf("par motor x%s\n", getenv("ACEL")); }
    printf("columna %d: masa %.0f kg, batalla %.2f m, a/b %.3f/%.3f m, marchas %d, final %.3f, radio %.4f m\n",
           col, p.masa, p.batalla, p.cg_del, p.cg_tras, p.marchas, p.rel_final, p.radio[1]);
    printf("  relaciones:");
    for (r = 1; r <= p.marchas; ++r) printf(" %.3f", p.rel[r]);
    printf("\n  empuje por marcha:");
    for (r = 1; r <= p.marchas; ++r) printf(" %.3f", p.empuje[r]);
    printf("\n  subir a (km/h fisicos):");
    for (r = 1; r < p.marchas; ++r) printf(" %.0f", p.sube[r] * 216.72f);
    printf("\n  bajar en (km/h fisicos):");
    for (r = 2; r <= p.marchas; ++r) printf(" %.0f", p.baja[r] * 216.72f);
    printf("\n  dir_g=%.0f unid. -> giro max de rueda %.2f gr; factor velocimetro %.4f\n",
           p.dir_g, p.dir_k1 * p.dir_g * 32512.0f / (p.dir_k2 * 32768.0f) * 360.0f / 65536.0f,
           p.hud * 3.6f);

    prueba_aceleracion(&p);
    prueba_cambios(&p);
    prueba_choques(&p);
    if (solo_choques) return 0;
    prueba_derrape(&p, 1, 120.0f, -0.3f);
    prueba_derrape(&p, 1, 120.0f, 0.0f);
    prueba_derrape(&p, 0, 120.0f, -0.3f);
    prueba_viraje(&p, 1.0f);
    prueba_viraje(&p, 0.5f);
    prueba_tierra(&p);
    /* coste */
    {
        or2_coche_t c;
        or2_entrada_t e;
        int i;
        clock_t t0;
        or2_coloca(&c, &p, 0.0f, 50.0f);
        entrada_libre(&e);
        e.acelerador = 1.0f; e.stick = 0.7f;
        t0 = clock();
        for (i = 0; i < 200000; ++i) or2_paso(&c, &p, &e, 0.0f);
        printf("\ncoste en este PC: %.2f us por paso\n",
               (double)(clock() - t0) / CLOCKS_PER_SEC * 1e6 / 200000.0);
    }
    return 0;
}
