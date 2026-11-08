/*
 * prueba_fisica.c - banco de pruebas en PC para comun/or2_fisica.c
 *
 * gcc -O2 -std=c99 -Wall -I../../comun prueba_fisica.c ../../comun/or2_fisica.c -lm -o prueba_fisica
 * ./prueba_fisica tabla_ps2.bin [columna] [SLES_539.98]
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

static int carga_tabla(const char *ruta)
{
    FILE *f = fopen(ruta, "rb");
    size_t n;
    if (!f) return -1;
    n = fread(tabla, 4, sizeof(tabla) / 4, f);
    fclose(f);
    filas = (int)(n / OR2_COLUMNAS);
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

int main(int argc, char **argv)
{
    or2_param_t p;
    int col = argc > 2 ? atoi(argv[2]) : 6, r;
    if (argc < 2) {
        fprintf(stderr, "uso: %s tabla_ps2.bin [columna] [SLES_539.98]\n", argv[0]);
        return 1;
    }
    if (carga_tabla(argv[1])) { fprintf(stderr, "no puedo leer la tabla\n"); return 1; }
    r = or2_carga_param(tabla, filas, col, &p);
    if (r) { fprintf(stderr, "or2_carga_param: %d\n", r); return 1; }
    if (argc > 3) {
        r = carga_curvas(argv[3], &p);
        printf("curvas del ELF: %s\n", r ? "NO (aproximadas)" : "si");
    } else printf("curvas aproximadas (sin ELF)\n");
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
    prueba_derrape(&p, 1, 120.0f, -0.3f);
    prueba_derrape(&p, 1, 120.0f, 0.0f);
    prueba_derrape(&p, 0, 120.0f, -0.3f);
    prueba_viraje(&p, 1.0f);
    prueba_viraje(&p, 0.5f);
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
