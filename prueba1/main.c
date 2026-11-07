/* Prueba 1 del port de OutRun 2006 C2C a Dreamcast: un segmento de carretera (SEG00.DAT, convertido de tu DVD con
   tools/conv_tramo.py) dibujado con la PowerVR y una camara que lo recorre. Sin texturas todavia (color por parte con
   la luz ya aplicada). Mide lo que importa: cuadros por segundo, triangulos enviados y tiempo de CPU por cuadro.
   Mando: cruceta arriba/abajo distancia de dibujo, izquierda/derecha velocidad, START pausa, A ensena/oculta cifras. */
#include <kos.h>
#include <dc/matrix.h>
#include <dc/biosfont.h>
#include <math.h>
#include "../comun/gd.h"

typedef struct { float cx, cy, cz, r; uint32_t v0, n, reg; } malla_t;
typedef struct { float x, y, z; uint32_t c; } vert_t;

static malla_t *mallas; static vert_t *verts; static float *cam;
static uint32_t nmallas, nvert, ncam;

static int carga(const char *nombre)
{
    uint32_t sz; uint8_t *b = gd_carga(nombre, &sz); uint32_t h[4];
    if (!b) return -1;
    memcpy(h, b, 16);
    if (memcmp(b, "ORD1", 4)) return -3;
    nmallas = h[1]; nvert = h[2]; ncam = h[3];
    mallas = (malla_t *)(b + 16);
    verts = (vert_t *)(b + 16 + nmallas * sizeof(malla_t));
    cam = (float *)(b + 16 + nmallas * sizeof(malla_t) + nvert * sizeof(vert_t));
    return 0;
}

/* camara: ojo, base (derecha, arriba, adelante) y la matriz de pantalla: X = 320 W + fx xv, Y = 240 W - fx yv, W = zv */
static float ojo[3], der[3], arr[3], ade[3], fx = 320.0f / 0.70f, cerca = 1.0f, lejos = 600.0f;
static matrix_t mtx __attribute__((aligned(32)));

static void norm(float *v) { float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 0) { v[0] /= l; v[1] /= l; v[2] /= l; } }
static void pon_camara(float t)
{
    int i = (int)t, j; float a = t - i, mira[3], k;
    if (i >= (int)ncam - 12) i = ncam - 13;
    for (j = 0; j < 3; j++) ojo[j] = cam[3 * i + j] * (1 - a) + cam[3 * (i + 1) + j] * a;
    for (j = 0; j < 3; j++) mira[j] = cam[3 * (i + 10) + j];
    ojo[1] += 3.0f; mira[1] += 2.0f;                         /* a la altura de un coche (Y hacia arriba) */
    for (j = 0; j < 3; j++) ade[j] = mira[j] - ojo[j];
    norm(ade);
    arr[0] = 0; arr[1] = 1; arr[2] = 0;
    der[0] = ade[1] * arr[2] - ade[2] * arr[1]; der[1] = ade[2] * arr[0] - ade[0] * arr[2]; der[2] = ade[0] * arr[1] - ade[1] * arr[0];
    norm(der);
    arr[0] = der[1] * ade[2] - der[2] * ade[1]; arr[1] = der[2] * ade[0] - der[0] * ade[2]; arr[2] = der[0] * ade[1] - der[1] * ade[0];
    /* columnas de la matriz (ftrv: salida = suma de columna_j * entrada_j); salida (X, Y, Z, W) con Z = W */
    for (j = 0; j < 3; j++) {
        float X = fx * der[j] + 320.0f * ade[j], Y = -fx * arr[j] + 240.0f * ade[j], W = ade[j];
        mtx[j][0] = X; mtx[j][1] = Y; mtx[j][2] = W; mtx[j][3] = W;
    }
    k = -(ojo[0] * mtx[0][0] + ojo[1] * mtx[1][0] + ojo[2] * mtx[2][0]); mtx[3][0] = k;
    k = -(ojo[0] * mtx[0][1] + ojo[1] * mtx[1][1] + ojo[2] * mtx[2][1]); mtx[3][1] = k;
    k = -(ojo[0] * mtx[0][2] + ojo[1] * mtx[1][2] + ojo[2] * mtx[2][2]); mtx[3][2] = k; mtx[3][3] = k;
}

static pvr_poly_hdr_t cab;
static uint32_t tri_enviados;

static inline void envia(float x, float y, float w, uint32_t c, int fin)
{
    pvr_vertex_t *v = pvr_dr_target();
    float iw = 1.0f / w;
    v->flags = fin ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    v->x = x * iw; v->y = y * iw; v->z = iw; v->u = v->v = 0; v->argb = c; v->oargb = 0;
    pvr_dr_commit(v);
}

/* triangulo con algun vertice delante del plano cercano: recortado (hasta 4 vertices, enviado como tira) */
static void tri_recortado(const float *p, const uint32_t *c)
{
    float o[4][3]; uint32_t oc[4]; int n = 0, i;
    for (i = 0; i < 3; i++) {
        const float *a = p + 3 * i, *b = p + 3 * ((i + 1) % 3);
        int da = a[2] >= cerca, db = b[2] >= cerca;
        if (da) { o[n][0] = a[0]; o[n][1] = a[1]; o[n][2] = a[2]; oc[n++] = c[i]; }
        if (da != db) {
            float t = (cerca - a[2]) / (b[2] - a[2]);
            o[n][0] = a[0] + t * (b[0] - a[0]); o[n][1] = a[1] + t * (b[1] - a[1]); o[n][2] = cerca; oc[n++] = c[i];
        }
    }
    if (n < 3) return;
    if (n == 3) { envia(o[0][0], o[0][1], o[0][2], oc[0], 0); envia(o[1][0], o[1][1], o[1][2], oc[1], 0); envia(o[2][0], o[2][1], o[2][2], oc[2], 1); tri_enviados++; }
    else { envia(o[0][0], o[0][1], o[0][2], oc[0], 0); envia(o[1][0], o[1][1], o[1][2], oc[1], 0);
           envia(o[3][0], o[3][1], o[3][2], oc[3], 0); envia(o[2][0], o[2][1], o[2][2], oc[2], 1); tri_enviados += 2; }
}

static float pw[3 * 256];                                    /* la tira transformada (X, Y, W) */
static void dibuja_malla(const malla_t *m)
{
    const vert_t *v = verts + m->v0; uint32_t i, n = m->n; int detras = 0;
    if (n > 256) n = 256;
    for (i = 0; i < n; i++) {
        float x = v[i].x, y = v[i].y, z = v[i].z, w;
        mat_trans_single3_nodivw(x, y, z, w);
        pw[3 * i] = x; pw[3 * i + 1] = y; pw[3 * i + 2] = w;
        detras |= w < cerca;
    }
    if (!detras) {
        for (i = 0; i < n; i++) envia(pw[3 * i], pw[3 * i + 1], pw[3 * i + 2], v[i].c, i == n - 1);
        tri_enviados += n - 2;
        return;
    }
    for (i = 0; i + 2 < n; i++) {                            /* por triangulos, recortando */
        float p[9]; uint32_t c[3];
        if (pw[3 * i + 2] < cerca && pw[3 * i + 5] < cerca && pw[3 * i + 8] < cerca) continue;
        memcpy(p, pw + 3 * i, 9 * sizeof(float)); c[0] = v[i].c; c[1] = v[i + 1].c; c[2] = v[i + 2].c;
        tri_recortado(p, c);
    }
}

/* cifras: texto en una textura de 512x32 (RGB565) y un rectangulo arriba */
static pvr_ptr_t txt; static uint16_t txtbuf[512 * 32] __attribute__((aligned(32)));
static void cifras(const char *s)
{
    int i;
    for (i = 0; i < 512 * 32; i++) txtbuf[i] = 0;
    bfont_draw_str(txtbuf + 4 * 512 + 4, 512, 1, s);   /* el rectangulo va en (24, 16): fuera del borde que corta la tele */
    pvr_txr_load(txtbuf, txt, sizeof txtbuf);
}
static void dibuja_cifras(void)
{
    pvr_poly_cxt_t cx; pvr_poly_hdr_t h; pvr_vertex_t v;
    pvr_poly_cxt_txr(&cx, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 512, 32, txt, PVR_FILTER_NONE);
    pvr_poly_compile(&h, &cx); pvr_prim(&h, sizeof h);
    v.argb = 0xFFFFFFFF; v.oargb = 0; v.z = 10.0f;
    v.flags = PVR_CMD_VERTEX; v.x = 24; v.y = 16; v.u = 0; v.v = 0; pvr_prim(&v, sizeof v);
    v.x = 536; v.u = 1; pvr_prim(&v, sizeof v);
    v.x = 24; v.y = 48; v.u = 0; v.v = 1; pvr_prim(&v, sizeof v);
    v.flags = PVR_CMD_VERTEX_EOL; v.x = 536; v.u = 1; pvr_prim(&v, sizeof v);
}

int main(int argc, char **argv)
{
    pvr_poly_cxt_t cx; float t = 0, vel = 1.5f; int pausa = 0, ver = 1, r, k; uint32_t cuadro = 0, bot_ant = 0;
    uint64_t t0 = timer_ms_gettime64(), tcpu = 0; char s[128]; float fps = 0, ms_cpu = 0;
    (void)argc; (void)argv;
    pvr_init_defaults();
    pvr_set_bg_color(0.45f, 0.65f, 0.95f);
    txt = pvr_mem_malloc(512 * 32 * 2);
    r = carga("SEG00.DAT");
    if (r) {
        vid_set_mode(DM_640x480, PM_RGB565);
        sprintf(s, "No se pudo leer SEG00.DAT (%d)", r);
        bfont_draw_str(vram_s + 100 * 640 + 40, 640, 1, s);
        for (;;) thd_sleep(1000);
    }
    pvr_fog_table_color(1.0f, 0.45f, 0.65f, 0.95f);
    pvr_poly_cxt_col(&cx, PVR_LIST_OP_POLY);
    cx.gen.fog_type = PVR_FOG_TABLE; cx.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&cab, &cx);
    cifras("Cargado");
    for (;;) {
        maple_device_t *mc = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = mc ? (cont_state_t *)maple_dev_status(mc) : NULL;
        uint32_t b = st ? st->buttons : 0, nuevo = b & ~bot_ant; uint64_t c0;
        bot_ant = b;
        if (nuevo & CONT_START) pausa ^= 1;
        if (nuevo & CONT_A) ver ^= 1;
        if (nuevo & CONT_DPAD_UP && lejos < 2000) lejos += 100;
        if (nuevo & CONT_DPAD_DOWN && lejos > 200) lejos -= 100;
        if (nuevo & CONT_DPAD_RIGHT && vel < 6) vel += 0.5f;
        if (nuevo & CONT_DPAD_LEFT && vel > 0.5f) vel -= 0.5f;
        pvr_fog_table_linear(lejos * 0.6f, lejos);
        if (!pausa) { t += vel; if (t >= ncam - 14) t = 0; }

        pvr_wait_ready();
        c0 = timer_us_gettime64();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        pon_camara(t);
        mat_load(&mtx);
        tri_enviados = 0;
        pvr_prim(&cab, sizeof cab);
        {
            float tanh = 320.0f / fx;
            for (k = 0; k < (int)nmallas; k++) {
                const malla_t *m = mallas + k;
                float dx = m->cx - ojo[0], dy = m->cy - ojo[1], dz = m->cz - ojo[2];
                float zv = dx * ade[0] + dy * ade[1] + dz * ade[2], xv, yv;
                if (zv + m->r < cerca || zv - m->r > lejos) continue;
                xv = dx * der[0] + dy * der[1] + dz * der[2];
                yv = dx * arr[0] + dy * arr[1] + dz * arr[2];
                if (fabsf(xv) > zv * tanh + m->r * 1.3f || fabsf(yv) > zv * tanh * 0.75f + m->r * 1.3f) continue;
                dibuja_malla(m);
            }
        }
        if (ver) dibuja_cifras();
        pvr_list_finish();
        pvr_scene_finish();
        tcpu += timer_us_gettime64() - c0;
        if (++cuadro % 30 == 0) {
            uint64_t t1 = timer_ms_gettime64();
            fps = 30000.0f / (float)(t1 - t0); ms_cpu = tcpu / 30000.0f; t0 = t1; tcpu = 0;
            sprintf(s, "%.1f fps %lu tri %.1fms d%d", (double)fps, (unsigned long)tri_enviados, (double)ms_cpu, (int)lejos);
            if (ver) cifras(s);
        }
    }
    return 0;
}
