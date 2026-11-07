/* Prueba 2 del port de OutRun 2006 C2C a Dreamcast: un segmento de carretera con sus texturas (SEG00.DAT, convertido
   de tu DVD con tools/conv_tramo.py) y una camara que lo recorre. Las tiras van en tres listas de la PowerVR: opacas,
   de recorte (alfa de 1 bit: arboles, vallas) y transparentes. Mide cuadros por segundo, triangulos y CPU por cuadro.
   Mando: cruceta arriba/abajo distancia de dibujo, izquierda/derecha velocidad, START pausa, A ensena/oculta cifras,
   B filtro bilineal si/no. */
#include <kos.h>
#include <dc/matrix.h>
#include <dc/biosfont.h>
#include <math.h>
#include "../comun/gd.h"

typedef struct { uint32_t w, h, fmt, lista, off, tam; } tex_t;
typedef struct { float cx, cy, cz, r; uint32_t v0, n; int32_t tex; } malla_t;
typedef struct { float x, y, z, u, v; uint32_t c; } vert_t;

static tex_t *texs; static malla_t *mallas; static vert_t *verts; static float *cam;
static uint32_t nmallas, nvert, ncam, ntex, inicio[5];
static pvr_ptr_t *tvram;
static pvr_poly_hdr_t *cabs[3];                              /* por textura: [0] sin filtro, [1] bilineal, [2] cielo */
static pvr_poly_hdr_t cab_col[3];                            /* sin textura, por lista */

static const uint32_t FMT[3] = { PVR_TXRFMT_ARGB1555, PVR_TXRFMT_RGB565, PVR_TXRFMT_ARGB4444 };
static const int LISTA[3] = { PVR_LIST_OP_POLY, PVR_LIST_PT_POLY, PVR_LIST_TR_POLY };

static int carga(const char *nombre)
{
    uint32_t sz, h[9], i, f; uint8_t *b = gd_carga(nombre, &sz), *p, *blob;
    pvr_poly_cxt_t cx;
    dbglog(DBG_INFO, "carga: %p %lu\n", b, (unsigned long)sz);
    if (!b) return -1;
    if (memcmp(b, "ORD3", 4)) return -3;
    memcpy(h, b + 4, sizeof h);
    nmallas = h[0]; nvert = h[1]; ncam = h[2]; ntex = h[3]; for (i = 0; i < 5; i++) inicio[i] = h[4 + i];
    p = b + 40;
    texs = (tex_t *)p; p += ntex * sizeof(tex_t);
    mallas = (malla_t *)p; p += nmallas * sizeof(malla_t);
    verts = (vert_t *)p; p += nvert * sizeof(vert_t);
    cam = (float *)p; p += ncam * 12;
    blob = b + ((p - b + 31) & ~31);
    tvram = malloc(ntex * sizeof(pvr_ptr_t));
    for (f = 0; f < 3; f++) cabs[f] = memalign(32, ntex * sizeof(pvr_poly_hdr_t));
    dbglog(DBG_INFO, "mallas %lu vert %lu tex %lu blob %p\n", nmallas, nvert, ntex, blob);
    for (i = 0; i < ntex; i++) {
        tex_t *t = texs + i;
        tvram[i] = pvr_mem_malloc(t->tam);
        if (!tvram[i]) return -4;
        pvr_txr_load(blob + t->off, tvram[i], t->tam);
        for (f = 0; f < 3; f++) {
            pvr_poly_cxt_txr(&cx, LISTA[t->lista], FMT[t->fmt] | PVR_TXRFMT_TWIDDLED, t->w, t->h, tvram[i],
                             f ? PVR_FILTER_BILINEAR : PVR_FILTER_NONE);
            cx.gen.fog_type = f == 2 ? PVR_FOG_DISABLE : PVR_FOG_TABLE; cx.gen.culling = PVR_CULLING_NONE;
            cx.txr.env = PVR_TXRENV_MODULATE;
            pvr_poly_compile(&cabs[f][i], &cx);
        }
    }
    for (i = 0; i < 3; i++) {
        pvr_poly_cxt_col(&cx, LISTA[i]);
        cx.gen.fog_type = PVR_FOG_TABLE; cx.gen.culling = PVR_CULLING_NONE;
        pvr_poly_compile(&cab_col[i], &cx);
    }
    return 0;
}

/* camara: ojo, base (derecha, arriba, adelante) y la matriz de pantalla: X = 320 W + fx xv, Y = 240 W - fx yv, W = zv */
static float ojo[3], der[3], arr[3], ade[3], fx = 320.0f / 0.70f, cerca = 1.0f, lejos = 600.0f;
static matrix_t mtx __attribute__((aligned(32))), mtx_cielo __attribute__((aligned(32)));

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
    for (j = 0; j < 3; j++) {
        float X = fx * der[j] + 320.0f * ade[j], Y = -fx * arr[j] + 240.0f * ade[j], W = ade[j];
        mtx[j][0] = X; mtx[j][1] = Y; mtx[j][2] = W; mtx[j][3] = W;
    }
    memcpy(mtx_cielo, mtx, sizeof mtx); mtx_cielo[3][0] = mtx_cielo[3][1] = mtx_cielo[3][2] = mtx_cielo[3][3] = 0;
    k = -(ojo[0] * mtx[0][0] + ojo[1] * mtx[1][0] + ojo[2] * mtx[2][0]); mtx[3][0] = k;
    k = -(ojo[0] * mtx[0][1] + ojo[1] * mtx[1][1] + ojo[2] * mtx[2][1]); mtx[3][1] = k;
    k = -(ojo[0] * mtx[0][2] + ojo[1] * mtx[1][2] + ojo[2] * mtx[2][2]); mtx[3][2] = k; mtx[3][3] = k;
}

static uint32_t tri_enviados;
typedef struct { float x, y, w, u, v; uint32_t c; } pt_t;     /* vertice transformado */

static inline void envia(const pt_t *p, int fin)
{
    pvr_vertex_t *v = pvr_dr_target();
    float iw = 1.0f / p->w;
    v->flags = fin ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    v->x = p->x * iw; v->y = p->y * iw; v->z = iw; v->u = p->u; v->v = p->v; v->argb = p->c; v->oargb = 0;
    pvr_dr_commit(v);
}

/* triangulo con algun vertice delante del plano cercano: recortado (hasta 4 vertices, enviado como tira) */
static void tri_recortado(const pt_t *p)
{
    pt_t o[4]; int n = 0, i;
    for (i = 0; i < 3; i++) {
        const pt_t *a = p + i, *b = p + (i + 1) % 3;
        int da = a->w >= cerca, db = b->w >= cerca;
        if (da) o[n++] = *a;
        if (da != db) {
            float t = (cerca - a->w) / (b->w - a->w);
            o[n].x = a->x + t * (b->x - a->x); o[n].y = a->y + t * (b->y - a->y); o[n].w = cerca;
            o[n].u = a->u + t * (b->u - a->u); o[n].v = a->v + t * (b->v - a->v); o[n].c = a->c; n++;
        }
    }
    if (n < 3) return;
    if (n == 3) { envia(o, 0); envia(o + 1, 0); envia(o + 2, 1); tri_enviados++; }
    else { envia(o, 0); envia(o + 1, 0); envia(o + 3, 0); envia(o + 2, 1); tri_enviados += 2; }
}

static pt_t pw[256];
static void dibuja_malla(const malla_t *m)
{
    const vert_t *v = verts + m->v0; uint32_t i, n = m->n; int detras = 0;
    if (n > 256) n = 256;
    for (i = 0; i < n; i++) {
        float x = v[i].x, y = v[i].y, z = v[i].z, w;
        mat_trans_single3_nodivw(x, y, z, w);
        pw[i].x = x; pw[i].y = y; pw[i].w = w; pw[i].u = v[i].u; pw[i].v = v[i].v; pw[i].c = v[i].c;
        detras |= w < cerca;
    }
    if (!detras) {
        for (i = 0; i < n; i++) envia(pw + i, i == n - 1);
        tri_enviados += n - 2;
        return;
    }
    for (i = 0; i + 2 < n; i++) {                            /* por triangulos, recortando */
        if (pw[i].w < cerca && pw[i + 1].w < cerca && pw[i + 2].w < cerca) continue;
        tri_recortado(pw + i);
    }
}

/* cifras: texto en una textura de 512x32 (RGB565) y un rectangulo arriba */
static pvr_ptr_t txt; static uint16_t txtbuf[512 * 32] __attribute__((aligned(32)));
static void cifras(const char *s)
{
    int i;
    for (i = 0; i < 512 * 32; i++) txtbuf[i] = 0;
    bfont_draw_str(txtbuf + 4 * 512 + 4, 512, 1, s);
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

static int filtro = 1;
/* el cielo: sus tiras de la lista l, alrededor de la camara (matriz sin traslacion) y sin niebla */
static void dibuja_cielo(int l)
{
    int k, ult = -2;
    mat_load(&mtx_cielo);
    for (k = inicio[3]; k < (int)inicio[4]; k++) {
        const malla_t *m = mallas + k;
        if (m->tex < 0 || (int)texs[m->tex].lista != l) continue;
        if (m->tex != ult) { ult = m->tex; pvr_prim(&cabs[2][m->tex], sizeof(pvr_poly_hdr_t)); }
        dibuja_malla(m);
    }
    mat_load(&mtx);
}
static void dibuja_lista(int l)
{
    float tanh = 320.0f / fx; int k, ult = -2;
    for (k = inicio[l]; k < (int)inicio[l + 1]; k++) {
        const malla_t *m = mallas + k;
        float dx = m->cx - ojo[0], dy = m->cy - ojo[1], dz = m->cz - ojo[2];
        float zv = dx * ade[0] + dy * ade[1] + dz * ade[2], xv, yv;
        if (zv + m->r < cerca || zv - m->r > lejos) continue;
        xv = dx * der[0] + dy * der[1] + dz * der[2];
        yv = dx * arr[0] + dy * arr[1] + dz * arr[2];
        if (fabsf(xv) > zv * tanh + m->r * 1.3f || fabsf(yv) > zv * tanh * 0.75f + m->r * 1.3f) continue;
        if (m->tex != ult) {
            ult = m->tex;
            if (m->tex >= 0) pvr_prim(&cabs[filtro][m->tex], sizeof(pvr_poly_hdr_t));
            else pvr_prim(&cab_col[l], sizeof(pvr_poly_hdr_t));
        }
        dibuja_malla(m);
    }
}

int main(int argc, char **argv)
{
    pvr_init_params_t pp = { { PVR_BINSIZE_16, 0, PVR_BINSIZE_16, 0, PVR_BINSIZE_16 }, 1024 * 1024, 0, 0, 0, 3 };
    float t = 0, vel = 1.5f; int pausa = 0, ver = 1, r, l; uint32_t cuadro = 0, bot_ant = 0;
    uint64_t t0 = timer_ms_gettime64(), tcpu = 0; char s[128]; float fps = 0, ms_cpu = 0;
    (void)argc; (void)argv;
    pvr_init(&pp);
    pvr_set_bg_color(0.45f, 0.65f, 0.95f);
    txt = pvr_mem_malloc(512 * 32 * 2);
    dbglog(DBG_INFO, "inicio\n");
    r = carga("SEG00.DAT");
    dbglog(DBG_INFO, "carga -> %d\n", r);
    if (r) {
        vid_set_mode(DM_640x480, PM_RGB565);
        sprintf(s, "No se pudo leer SEG00.DAT (%d)", r);
        bfont_draw_str(vram_s + 100 * 640 + 40, 640, 1, s);
        for (;;) thd_sleep(1000);
    }
    pvr_fog_table_color(1.0f, 0.45f, 0.65f, 0.95f);
    cifras("Cargado");
    for (;;) {
        maple_device_t *mc = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = mc ? (cont_state_t *)maple_dev_status(mc) : NULL;
        uint32_t b = st ? st->buttons : 0, nuevo = b & ~bot_ant; uint64_t c0;
        bot_ant = b;
        if (nuevo & CONT_START) pausa ^= 1;
        if (nuevo & CONT_A) ver ^= 1;
        if (nuevo & CONT_B) filtro ^= 1;
        if (nuevo & CONT_DPAD_UP && lejos < 2000) lejos += 100;
        if (nuevo & CONT_DPAD_DOWN && lejos > 200) lejos -= 100;
        if (nuevo & CONT_DPAD_RIGHT && vel < 6) vel += 0.5f;
        if (nuevo & CONT_DPAD_LEFT && vel > 0.5f) vel -= 0.5f;
        pvr_fog_table_linear(lejos * 0.6f, lejos);
        if (!pausa) { t += vel; if (t >= ncam - 14) t = 0; }

        pvr_wait_ready();
        c0 = timer_us_gettime64();
        pvr_scene_begin();
        pon_camara(t);
        mat_load(&mtx);
        tri_enviados = 0;
        for (l = 0; l < 3; l++) {
            pvr_list_begin(LISTA[l]);
            dibuja_cielo(l);
            dibuja_lista(l);
            if (l == 0 && ver) dibuja_cifras();
            pvr_list_finish();
        }
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
