/* Demo jugable del port de OutRun 2006 C2C a Dreamcast: el curso entero del modo (CURSO.DAT: 15 etapas SEGnn.DAT y sus
   uniones BKkk.DAT, BKD.DAT; ver curso_paso), que se leen y encadenan mientras se conduce, y los coches (COCHEnn.DAT), convertidos de tu DVD con tools/conv_tramo.py, tools/conv_union.py y tools/conv_coche.py
   (formato ORD6 con texturas VQ, ver tools/modelo.py y tools/vq.py). Necesidades de la consola real (presupuesto del TA, VRAM, GD, video): docs/DREAMCAST.md.
   Orden de carga: datos del GD a la RAM (con aviso en pantalla), PowerVR, texturas a la memoria de video, cabeceras
   de poligono, y a jugar. La conduccion va sobre la ruta de la etapa (distancia recorrida, desvio lateral y rumbo
   respecto a la carretera), con los bordes de la calzada como muros.
   Mando: stick o cruceta para girar, A o gatillo derecho acelera, B, X o gatillo izquierdo frena, START empieza o
   pausa, Y cambia la vista (en pausa ensena/oculta las cifras de rendimiento), cruceta arriba/abajo distancia de dibujo. Sin tocar nada, el titulo pasa a una demostracion automatica. */
#include <kos.h>
#include <dc/matrix.h>
#include <dc/biosfont.h>
#include <dc/flashrom.h>
#include <math.h>
#include <malloc.h>
#include "../comun/gd.h"
#include "../comun/musica.h"
#include "../comun/or2_fisica.h"
#include "../comun/hud.h"
#include "../comun/sfx.h"
#include "../comun/voces.h"
#include "../comun/menu.h"
#include "camara.h"

/* ---------------------------------------------------------------- modelos */
typedef struct { uint32_t w, h, fmt, lista, off, tam; } tex_t;
typedef struct { float cx, cy, cz, r; uint32_t v0, n; int32_t tex; int32_t nodo; } malla_t;   /* nodo del original o -1 */
typedef struct { float x, y, z, u, v; uint32_t c; } vert_t;
/* nodo del original (tools/modelo.py): esfera, banderas (bits 0-3 umbral de LOD, bit 4 orientado a la camara, bits 8-9
   rama de la union: 1 izquierda, 2 derecha) y su cartel (matriz del nodo) o -1 */
typedef struct { float cx, cy, cz, r; uint32_t fl; int32_t cartel; uint32_t relleno[2]; } nodo_t;
#define NODO_CARTEL 0x10
typedef struct {
    tex_t *texs; malla_t *mallas; vert_t *verts; float *ruta;
    uint32_t nmallas, nvert, nruta, paso_ruta, ntex, inicio[5];
    nodo_t *nodos; float (*carteles)[4][4];                      /* ORD7: nodos y matrices de los carteles (filas) */
    uint32_t nnodos, ncarteles, nsec;                             /* secciones de la tabla de visibilidad del original */
    const uint32_t *sec_ini; const uint16_t *sec_lista, *sec_mapa;   /* lista de nodos por seccion; seccion por punto */
    uint32_t *vis;                                                /* nodos visibles en el cuadro (bits; nodos_visibles) */
    uint32_t ruta_i;                                              /* punto de la ruta mas cercano al coche (ultimo) */
    uint8_t *datos, *blob; pvr_poly_hdr_t *cabs[2];               /* [0] con niebla, [1] sin niebla (cielo) */
    pvr_ptr_t *tv;                                                /* texturas en la memoria de video */
    uint32_t *ofs;                                                /* color de desplazamiento por vertice o NULL (coche) */
    const int32_t *remap;                                         /* textura -> la del color elegido, o NULL (trafico) */
} modelo_t;

static modelo_t coche;                                         /* el del jugador (abajo): no recibe su sombra */
static const uint32_t FMT[3] = { PVR_TXRFMT_ARGB1555, PVR_TXRFMT_RGB565, PVR_TXRFMT_ARGB4444 };
static const int LISTA[3] = { PVR_LIST_OP_POLY, PVR_LIST_PT_POLY, PVR_LIST_TR_POLY };
static pvr_poly_hdr_t cab_col[2][3];                            /* sin textura: [niebla][lista] */

/* Bytes que ocupa en el bufer de parametros del TA una tira de n vertices tal y como la mandamos (cabecera de tipo 0,
   vertice de tipo 3: textura con UV de 32 bits y color empaquetado; o: con color de desplazamiento, el coche). El TA
   parte cada tira en trozos de hasta 6 triangulos (8 vertices, 2 compartidos con el trozo siguiente) y guarda por trozo
   ISP/TSP/TCW (12 bytes) y sus vertices (x, y, z, u, v, color: 24 bytes; 28 con desplazamiento):
   k = techo((n - 2) / 6) trozos, (24 + 4 o) (n - 2) + (60 + 8 o) k bytes (docs/DREAMCAST.md) */
static inline uint32_t ta_teo(uint32_t n, int o)
{
    uint32_t t = n - 2, k = (t + 5) / 6;
    return (24 + 4 * o) * t + (60 + 8 * o) * k;
}

/* Huecos fijos de RAM para las etapas (la de salida y luego la siguiente, nunca a la vez) y para la union: se reservan
   al arrancar y no se liberan, para que la RAM no se trocee entre carreras (los coches del menu se cargan y liberan en
   medio) y siempre quepa una etapa entera (curso_huecos) */
static uint8_t *hueco_etapa, *hueco_union;
static uint32_t cap_etapa, cap_union;
/* lee un modelo en buf (cap bytes) o, con buf NULL, en memoria nueva */
static int lee_modelo_en(modelo_t *m, const char *nombre, uint8_t *buf, uint32_t cap)
{
    uint32_t sz, h[13], i, v7; uint8_t *b = buf ? (gd_carga_en(nombre, buf, cap, &sz) ? NULL : buf) : gd_carga(nombre, &sz), *p;
    if (!b) return -1;
    v7 = !memcmp(b, "ORD7", 4);                                   /* ORD7: con nodos y visibilidad del original */
    if (!v7 && memcmp(b, "ORD5", 4) && memcmp(b, "ORD6", 4)) { if (!buf) free(b); return -2; }   /* ORD6: VQ */
    memset(h, 0, sizeof h); memcpy(h, b + 4, (v7 ? 13 : 10) * 4);
    m->nmallas = h[0]; m->nvert = h[1]; m->nruta = h[2]; m->paso_ruta = h[3]; m->ntex = h[4];
    for (i = 0; i < 5; i++) m->inicio[i] = h[5 + i];
    m->nnodos = h[10]; m->ncarteles = h[11]; m->nsec = h[12];
    p = b + (v7 ? 56 : 44);
    m->texs = (tex_t *)p; p += m->ntex * sizeof(tex_t);
    m->mallas = (malla_t *)p; p += m->nmallas * sizeof(malla_t);
    m->verts = (vert_t *)p; p += m->nvert * sizeof(vert_t);
    m->ruta = (float *)p; p += m->nruta * m->paso_ruta * 4;
    m->nodos = (nodo_t *)p; p += m->nnodos * sizeof(nodo_t);
    m->carteles = (float (*)[4][4])p; p += m->ncarteles * 64;
    m->sec_ini = m->nsec ? (const uint32_t *)p : NULL; p += m->nsec ? (m->nsec + 1) * 4 : 0;
    m->sec_lista = m->nsec ? (const uint16_t *)p : NULL; p += m->nsec ? m->sec_ini[m->nsec] * 2 : 0;
    m->sec_mapa = m->nsec ? (const uint16_t *)p : NULL; p += m->nsec ? m->nruta * 2 : 0;
    p = b + ((p - b + 3) & ~3);
    if (!v7) for (i = 0; i < m->nmallas; i++) m->mallas[i].nodo = -1;   /* ORD5/6: distancia maxima, sin nodos */
    /* presupuesto del TA (nodos_visibles): relleno[0] = bytes teoricos del TA de las tiras del nodo (ta_teo),
       relleno[1] = sus vertices << 12 | las clases de distancia (<< 6) y de LOD del cuadro (nodos_visibles) */
    for (i = 0; i < m->nnodos; i++) m->nodos[i].relleno[0] = m->nodos[i].relleno[1] = 0;
    for (i = 0; i < m->nmallas; i++)
        if (m->mallas[i].nodo >= 0 && (uint32_t)m->mallas[i].nodo < m->nnodos && m->mallas[i].n >= 3) {
            uint32_t n = m->mallas[i].n > 256 ? 256 : m->mallas[i].n;
            m->nodos[m->mallas[i].nodo].relleno[0] += ta_teo(n, 0); m->nodos[m->mallas[i].nodo].relleno[1] += n << 12;
        }
    m->vis = m->nnodos ? calloc((m->nnodos + 31) / 32, 4) : NULL; m->ruta_i = 0;
    m->blob = b + ((p - b + 31) & ~31); m->datos = b;
    return 0;
}
static int lee_modelo(modelo_t *m, const char *nombre) { return lee_modelo_en(m, nombre, NULL, 0); }
/* malla de un nodo orientado a la camara (sus vertices van en el marco del nodo) */
static inline int malla_de_cartel(const modelo_t *m, const malla_t *ma)
{
    return ma->nodo >= 0 && (uint32_t)ma->nodo < m->nnodos && m->nodos[ma->nodo].cartel >= 0;
}

/* texturas a la memoria de video y cabeceras; el blob de texeles ya no hace falta en la RAM tras esto.
   Formato de la textura (tools/modelo.py): bits 0-7 el de 16 bits, bit 8 VQ, bits 16-23 entradas del libro - 1. Una
   VQ con libro reducido (N < 256 entradas, las ultimas) se direcciona 2048 - 8 N bytes antes de sus datos: la PowerVR
   lee el libro en la direccion de la textura y los indices 2 KB despues, y nunca toca las entradas que faltan. */
static void compila_cab(modelo_t *m, uint32_t i, int especular)   /* especular: 1 desplazamiento, 2 ademas sumada */
{
    pvr_poly_cxt_t cx; tex_t *t = m->texs + i; uint32_t f, fb = t->fmt & 0xff, vq = t->fmt & 0x100, n = ((t->fmt >> 16) & 0xff) + 1;
    pvr_ptr_t dir = vq ? (pvr_ptr_t)((uintptr_t)m->tv[i] - (2048 - 8 * n)) : m->tv[i];
    for (f = 0; f < 2; f++) {
        pvr_poly_cxt_txr(&cx, LISTA[t->lista], FMT[fb] | PVR_TXRFMT_TWIDDLED | (vq ? PVR_TXRFMT_VQ_ENABLE : 0), t->w, t->h, dir,
                         PVR_FILTER_BILINEAR);
        cx.gen.fog_type = f ? PVR_FOG_DISABLE : PVR_FOG_TABLE; cx.gen.culling = PVR_CULLING_NONE;
        cx.txr.env = PVR_TXRENV_MODULATE;
        if (especular) cx.gen.specular = PVR_SPECULAR_ENABLE;   /* textura x color + desplazamiento (oargb) */
        if (especular & 2) { cx.blend.src = PVR_BLEND_ONE; cx.blend.dst = PVR_BLEND_ONE; }   /* sumada a lo de detras */
        /* sombra del coche (sombra_dibuja): la recibe lo opaco y lo recortado de la escena y del trafico; el coche del
           jugador no (el original lo dibuja despues de oscurecer) */
        if (m != &coche && t->lista != 2) { cx.fmt.modifier = true; cx.gen.modifier_mode = false; }   /* cheap shadow */
        pvr_poly_compile(&m->cabs[f][i], &cx);
    }
}
static int sube_modelo(modelo_t *m)
{
    uint32_t i, f;
    for (f = 0; f < 2; f++) m->cabs[f] = memalign(32, (m->ntex + 1) * sizeof(pvr_poly_hdr_t));
    m->tv = calloc(m->ntex + 1, sizeof(pvr_ptr_t));
    for (i = 0; i < m->ntex; i++) {
        tex_t *t = m->texs + i; pvr_ptr_t v = pvr_mem_malloc(t->tam);
        uint32_t fb = t->fmt & 0xff;
        if (!v) return -3;
        if (fb > 2 || (t->tam & 31)) return -4;
        m->tv[i] = v;
        pvr_txr_load(m->blob + t->off, v, t->tam);
        compila_cab(m, i, 0);
    }
    return 0;
}

/* vertices del modelo (lod < 0: todas las mallas; si no, las de ese LOD: el trafico lleva en el campo del nodo la
   mascara de los LOD que dibujan la malla, tools/conv_trafico.py) y bytes teoricos del TA (ta_teo; o: con color de
   desplazamiento) */
static uint32_t modelo_vtx(const modelo_t *m, int lod)
{
    uint32_t i, c = 0;
    for (i = 0; i < m->nmallas; i++) if (m->mallas[i].n >= 3 && (lod < 0 || ((m->mallas[i].nodo >> lod) & 1))) c += m->mallas[i].n > 256 ? 256 : m->mallas[i].n;
    return c;
}
static uint32_t modelo_teo(const modelo_t *m, int lod, int o)
{
    uint32_t i, c = 0;
    for (i = 0; i < m->nmallas; i++)
        if (m->mallas[i].n >= 3 && (lod < 0 || ((m->mallas[i].nodo >> lod) & 1))) c += ta_teo(m->mallas[i].n > 256 ? 256 : m->mallas[i].n, o);
    return c;
}

/* libera un modelo: texturas de la memoria de video, cabeceras y datos */
static void libera_modelo(modelo_t *m)
{
    uint32_t i;
    for (i = 0; i < m->ntex; i++) if (m->tv && m->tv[i]) pvr_mem_free(m->tv[i]);
    free(m->tv); free(m->cabs[0]); free(m->cabs[1]); free(m->ofs); free(m->vis);
    if (m->datos != hueco_etapa && m->datos != hueco_union) free(m->datos);
    memset(m, 0, sizeof *m);
}

/* tras sube_modelo los texeles (al final del fichero) ya no hacen falta en la RAM: se devuelven (realloc a menos) */
static void compacta_modelo(modelo_t *m)
{
    uint8_t *d;
    if (!m->datos || !m->blob || m->datos == hueco_etapa || m->datos == hueco_union) return;
    d = realloc(m->datos, m->blob - m->datos);
    if (!d) return;
    if (d != m->datos) {                                          /* se ha movido: los punteros, al bloque nuevo */
        uintptr_t k = (uintptr_t)d - (uintptr_t)m->datos;
        m->texs = (tex_t *)((uintptr_t)m->texs + k); m->mallas = (malla_t *)((uintptr_t)m->mallas + k);
        m->verts = (vert_t *)((uintptr_t)m->verts + k); m->ruta = (float *)((uintptr_t)m->ruta + k);
        m->nodos = (nodo_t *)((uintptr_t)m->nodos + k); m->carteles = (float (*)[4][4])((uintptr_t)m->carteles + k);
        if (m->nsec) {
            m->sec_ini = (const uint32_t *)((uintptr_t)m->sec_ini + k); m->sec_lista = (const uint16_t *)((uintptr_t)m->sec_lista + k);
            m->sec_mapa = (const uint16_t *)((uintptr_t)m->sec_mapa + k);
        }
        m->datos = d;
    }
    m->blob = NULL;
}

/* Carga en segundo plano: un hilo lee del GD el fichero de un modelo (lee_modelo) mientras el bucle sigue dibujando;
   las texturas las sube luego el bucle (sube_modelo: la memoria de video y las Store Queues son del hilo principal).
   Las lecturas de este hilo son de primer plano para gd.c: el lector de la musica espera mientras tanto y la musica
   sigue del anillo (~21 s). Una carga a la vez; estado 0 libre, 1 leyendo, 2 leido (r: resultado de lee_modelo). */
typedef struct { modelo_t *m; const char *nombre; uint8_t *buf; uint32_t cap; volatile int estado; int r; uint64_t t0;
                 int con_marco; float marco[4]; } carga_t;
static carga_t carga;
static void modelo_coloca(modelo_t *m, const float *M);
static void *carga_hilo(void *p)
{
    carga_t *c = p;
    c->r = lee_modelo_en(c->m, c->nombre, c->buf, c->cap);
    if (!c->r && c->con_marco) modelo_coloca(c->m, c->marco);   /* en su sitio del marco de la carrera */
    c->estado = 2;
    return NULL;
}
/* marco: colocacion del fichero (x, y, z, giro y; modelo_coloca) o NULL */
static void carga_pide(modelo_t *m, const char *nombre, uint8_t *buf, uint32_t cap, const float *marco)
{
    carga.m = m; carga.nombre = nombre; carga.buf = buf; carga.cap = cap; carga.r = 0; carga.estado = 1; carga.t0 = timer_ms_gettime64();
    carga.con_marco = marco != NULL; if (marco) memcpy(carga.marco, marco, sizeof carga.marco);
    if (!thd_create(1, carga_hilo, &carga)) carga_hilo(&carga);   /* sin hilo: aqui mismo */
}
static void carga_espera(void) { while (carga.estado == 1) thd_sleep(5); }

/* ---------------------------------------------------------------- camara y dibujo */
/* planos cercano y lejano del original: la camara de carrera de SLES-53998 pone +0xBC = 0,1 y +0xC0 = 4000 (0x178580:
   lui 0x3dcc / 0x457a; en el PC camera_perspective_404310 con c+0xBC / c+0xC0). Lo que cruza el plano cercano se
   recorta (tri_recortado): el VU1 del original no lo dibuja si algun vertice sale de su volumen de recorte, pero la
   PowerVR no tiene banda de guarda ni la necesita */
#define CERCA_PS2 0.1f
#define LEJOS_PS2 4000.0f
static float ojo[3], der[3], arr[3], ade[3], fx = 320.0f / 0.75f, cerca = CERCA_PS2, lejos = LEJOS_PS2;
static matrix_t vista __attribute__((aligned(32))), vista_cielo __attribute__((aligned(32))),
                mod_coche __attribute__((aligned(32)));
static uint32_t tri_enviados;

static void norm(float *v) { float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 0) { v[0] /= l; v[1] /= l; v[2] /= l; } }

/* matriz de pantalla: X = 320 W + fx xv, Y = 240 W - fx yv, W = Z = zv (columnas: ftrv suma columna_j * entrada_j).
   bal: balanceo de la camara (rad, + = el lado derecho de la camara sube; el +130 del original, 482F20 Rz) */
static void pon_vista(const float *o, const float *mira, float bal)
{
    int j; float k;
    for (j = 0; j < 3; j++) { ojo[j] = o[j]; ade[j] = mira[j] - o[j]; }
    norm(ade);
    der[0] = -ade[2]; der[1] = 0; der[2] = ade[0]; norm(der);
    arr[0] = der[1] * ade[2] - der[2] * ade[1]; arr[1] = der[2] * ade[0] - der[0] * ade[2]; arr[2] = der[0] * ade[1] - der[1] * ade[0];
    if (bal != 0) {
        float c = cosf(bal), sn = sinf(bal), d;
        for (j = 0; j < 3; j++) { d = der[j]; der[j] = c * d + sn * arr[j]; arr[j] = c * arr[j] - sn * d; }
    }
    for (j = 0; j < 3; j++) {
        float X = fx * der[j] + 320.0f * ade[j], Y = -fx * arr[j] + 240.0f * ade[j], W = ade[j];
        vista[j][0] = X; vista[j][1] = Y; vista[j][2] = W; vista[j][3] = W;
    }
    memcpy(vista_cielo, vista, sizeof vista);
    vista_cielo[3][0] = vista_cielo[3][1] = vista_cielo[3][2] = vista_cielo[3][3] = 0;
    for (j = 0; j < 4; j++) {
        k = -(ojo[0] * vista[0][j] + ojo[1] * vista[1][j] + ojo[2] * vista[2][j]); vista[3][j] = k;
    }
}

typedef struct { float x, y, w, u, v; uint32_t c, o; } pt_t;   /* o: color de desplazamiento */
static uint32_t vtx_env, vtx_ult;                        /* vertices enviados en el cuadro (y en el anterior) */
static uint32_t vtx_et, vtx_et_ult;                      /* los de la etapa */
/* ---------------------------------------------------------------- presupuesto del TA (docs/DREAMCAST.md)
   La PowerVR real se cuelga si el TA llena el bufer de parametros (vertices) del cuadro (o el de punteros, OPB);
   Flycast no. Cada tira 3D cuesta ta_teo (el formato de los parametros del TA) por ta_f / 256. ta_f sale de la medida
   en la consola real (demo 12c, GDEMU): 24656 vertices y 22012 triangulos en unas 1322 tiras llenaron 1 MB (VB_POS = END,
   error 0x0C); con ta_teo ese cuadro eran unos 780 KB (los trozos de cada tira, estimados), asi que la consola gasto al
   menos 1,34 veces la cuenta. ta_f = 1,75 (448 / 256): ese cuadro costaria 56 bytes por vertice, el margen de antes
   (1,3 x 42,5); las tiras que el TA guarda mejor (de 8 vertices, 6 triangulos) cuestan menos que antes y las que guarda
   peor (triangulos sueltos, tiras de 9 a 13) mas. En la consola, ta_f sube sola si el TA gasta mas (registro
   TA_ITP_CURRENT del cuadro anterior / lo teorico x 1,2; Flycast no lo actualiza). Lo 2D (HUD, menus y textos: ta_prim,
   por el que pasan los pvr_prim de este fichero y, desde el Makefile, los de hud.c y menu.c) cuenta ta_bpv (56) bytes
   por vertice. Topes de cada cuadro (bytes):
     total  = TA_VB_BYTES - TA_MARGEN
     2D     = max(TA_RESERVA_2D, 2 x lo 2D del cuadro anterior)          (lo 2D va al final y no se puede cortar)
     3D     = total - 2D                                                 (ta_lim: nada 3D pasa de aqui)
     etapa  = 3D - max(coche+trafico+efectos del cuadro anterior + 2 coches de trafico con LOD 0,
                       coche + 3 coches de trafico con LOD 0)
   Lo que la etapa no puede gastar lo reparte nodos_visibles_todos (quita primero lo que el LOD del original quitaria
   antes); si aun asi una tira no cabe, no se envia (ta_corto). */
#define TA_VB_BYTES (1792 * 1024)                        /* bufer de parametros por cuadro (x2: doble bufer) */
#define TA_MARGEN (64 * 1024)                            /* cabeceras vacias de fin de lista y prudencia */
#define TA_RESERVA_2D (64 * 1024)                        /* minimo para lo 2D (medido: unos 450 vertices) */
#define TA_BPV_MIN 56                                    /* bytes por vertice 2D (1,3 x 42,5 medidos en la consola) */
#define TA_F_MIN 448                                     /* coste 3D = ta_teo x ta_f / 256 (1,75: ver arriba) */
/* tope de vertices de la etapa por cuadro (tiempo de CPU: transformar y enviar) y distancia de dibujo minima del
   recorte de seguridad por CPU: la distancia es la del original (LEJOS_PS2) y solo se acorta si la CPU pasa de 15 ms */
#define VTX_ETAPA_MAX 20000
#define LEJOS_MIN 600.0f
static uint32_t ta_bpv = TA_BPV_MIN, ta_f = TA_F_MIN, ta_vb, ta_et, ta_lim, ta_tope_etapa, ta_corto, ta_v2d, ta_v2d_ult;
static uint32_t ta_otros_ult, ta_min_otros_teo, ta_traf_teo;   /* bytes de lo que no es la etapa (cuadro anterior; minimo
                                                            teorico) y teoricos del coche de trafico mayor con LOD 0 */
static uint32_t ta_teo_3d, ta_tiras;                     /* bytes teoricos y tiras 3D del cuadro */
static uint32_t ta_real_ult, ta_opb_ult;                 /* lo que gasto de verdad el TA en el cuadro anterior */
static int ta_freno;                                     /* 0..3: OPB muy lleno, la etapa baja a (4 - freno) / 4 */
static int ta_en_etapa;                                  /* lo que se envia ahora es de la etapa (su tope) */
static int ta_et_corto;                                  /* en este cuadro no cupo alguna tira de la etapa */
static int ta_ofs;                                       /* las tiras que se envian llevan color de desplazamiento */
/* reserva una tira de n vertices; 0 si no cabe (y no se envia) */
static inline int ta_cabe(uint32_t n)
{
    uint32_t t = ta_teo(n, ta_ofs), c = t * ta_f >> 8;
#ifndef DIAG_SIN_TOPE                                    /* medida en Flycast: sin tope (en la consola se colgaria) */
    if (ta_vb + c > ta_lim || (ta_en_etapa && ta_et + c > ta_tope_etapa)) { ta_corto = 1; ta_et_corto |= ta_en_etapa; return 0; }
#endif
    ta_vb += c; if (ta_en_etapa) ta_et += c;
    ta_teo_3d += t; ta_tiras++;
    return 1;
}
/* topes del cuadro, con lo medido en el anterior */
static void ta_inicia_cuadro(void)
{
    uint32_t total = TA_VB_BYTES - TA_MARGEN, r2d = 2 * ta_v2d_ult * ta_bpv, ro = ta_otros_ult + (2 * ta_traf_teo * ta_f >> 8), rmin = ta_min_otros_teo * ta_f >> 8;
    if (r2d < TA_RESERVA_2D) r2d = TA_RESERVA_2D;
    if (ro < rmin) ro = rmin;
    ta_lim = total - r2d;
    ta_tope_etapa = ta_lim > ro ? (ta_lim - ro) / 4 * (4 - ta_freno) : 0;
    ta_vb = ta_et = 0; ta_v2d = 0; ta_teo_3d = 0; ta_tiras = 0;
}
/* fin del cuadro (tras la ultima lista): lo contado, y lo que gasto el TA si el registro lo dice (consola real) */
static void ta_fin_cuadro(void)
{
    uint32_t usado = PVR_GET(PVR_TA_VERTBUF_POS) - PVR_GET(PVR_TA_VERTBUF_START), teo = ta_teo_3d + ta_v2d * 27;   /* 2D: cuadrilateros, 108 / 4 */
    uint32_t opb = (PVR_GET(PVR_TA_OPB_POS) << 2) - PVR_GET(PVR_TA_OPB_START), opb_tot = PVR_GET(PVR_TA_OPB_END) - PVR_GET(PVR_TA_OPB_START);
    ta_v2d_ult = ta_v2d; ta_otros_ult = ta_vb - ta_et;
    ta_real_ult = usado < TA_VB_BYTES ? usado : 0;
    ta_opb_ult = opb <= opb_tot ? opb : 0;               /* Flycast no lleva estos registros: 0 */
    if (ta_opb_ult > opb_tot / 10 * 7) { if (ta_freno < 3) ta_freno++; ta_corto = 1; }
    else if (ta_opb_ult < opb_tot / 2 && ta_freno) ta_freno--;
    if (ta_real_ult && teo > 64 * 1024) {               /* con margen del 20 %; nunca baja de TA_F_MIN */
        uint32_t f = (uint32_t)((uint64_t)ta_real_ult * 256 * 6 / 5 / teo) + 1;
        if (f > ta_f) { ta_f = f > 1024 ? 1024 : f; ta_bpv = TA_BPV_MIN * ta_f / TA_F_MIN; }   /* lo 2D, en proporcion */
    }
}
static int (*const pvr_prim_real)(const void *, size_t) = pvr_prim;   /* sin contar (volumenes de sombra) */
/* pvr_prim contando los vertices (lo 2D) */
int ta_prim(const void *d, size_t sz)
{
    if ((*(const uint32_t *)d >> 29) == 7) ta_v2d++;
    return pvr_prim(d, sz);
}
#define pvr_prim ta_prim

static inline void envia(const pt_t *p, int fin)
{
    pvr_vertex_t *v = pvr_dr_target(); vtx_env++;
    float iw = 1.0f / p->w;
    v->flags = fin ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    v->x = p->x * iw; v->y = p->y * iw; v->z = iw; v->u = p->u; v->v = p->v; v->argb = p->c; v->oargb = p->o;
    pvr_dr_commit(v);
}
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
            o[n].u = a->u + t * (b->u - a->u); o[n].v = a->v + t * (b->v - a->v); o[n].c = a->c; o[n].o = a->o; n++;
        }
    }
    if (n < 3 || !ta_cabe(n)) return;
    if (n == 3) { envia(o, 0); envia(o + 1, 0); envia(o + 2, 1); tri_enviados++; }
    else { envia(o, 0); envia(o + 1, 0); envia(o + 3, 0); envia(o + 2, 1); tri_enviados += 2; }
}
static uint32_t color_fijo;
static pt_t pw[256];                                     /* depuracion: si no es 0, color de todas las tiras */
/* 1/w con FSRRA (1/raiz de w*w; w > 0): mucho mas rapida que la division */
static inline float inv_w(float w) { float r = w * w; __asm__("fsrra %0" : "+f"(r)); return r; }

/* malla entera delante del plano cercano: cada vertice se transforma y va directo a la PowerVR */
static void dibuja_malla_directa(const modelo_t *mo, const malla_t *m)
{
    const vert_t *v = mo->verts + m->v0; uint32_t i, n = m->n;
    if (!ta_cabe(n)) return;
    for (i = 0; i < n; i++, v++) {
        float x = v->x, y = v->y, z = v->z, w, iw; pvr_vertex_t *d;
        mat_trans_single3_nodivw(x, y, z, w);
        iw = inv_w(w);
        d = pvr_dr_target();
        d->flags = i == n - 1 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        d->x = x * iw; d->y = y * iw; d->z = iw; d->u = v->u; d->v = v->v;
        d->argb = color_fijo ? color_fijo : v->c; d->oargb = mo->ofs ? mo->ofs[m->v0 + i] : 0;
        pvr_dr_commit(d);
    }
    tri_enviados += n - 2; vtx_env += n;
}

/* si no es 0: color que multiplica el de los vertices (ARGB, 255 = 1), en dibuja_malla (fundido del cielo, sol) */
static uint32_t color_mul;
static inline uint32_t mul_argb(uint32_t c, uint32_t k)
{
    return (((c >> 24) * (k >> 24) / 255) << 24) | ((((c >> 16) & 255) * ((k >> 16) & 255) / 255) << 16) |
           ((((c >> 8) & 255) * ((k >> 8) & 255) / 255) << 8) | ((c & 255) * (k & 255) / 255);
}
static void dibuja_malla(const modelo_t *mo, const malla_t *m)
{
    const vert_t *v = mo->verts + m->v0; uint32_t i, n = m->n; int detras = 0;
    if (n > 256) n = 256;
    for (i = 0; i < n; i++) {
        float x = v[i].x, y = v[i].y, z = v[i].z, w;
        mat_trans_single3_nodivw(x, y, z, w);
        pw[i].x = x; pw[i].y = y; pw[i].w = w; pw[i].u = v[i].u; pw[i].v = v[i].v;
        pw[i].c = color_fijo ? color_fijo : color_mul ? mul_argb(v[i].c, color_mul) : v[i].c;
        pw[i].o = mo->ofs ? mo->ofs[m->v0 + i] : 0;
        detras |= w < cerca;
    }
    if (!detras) {
        if (!ta_cabe(n)) return;
        for (i = 0; i < n; i++) envia(pw + i, i == n - 1);
        tri_enviados += n - 2;
        return;
    }
    for (i = 0; i + 2 < n; i++) {
        if (pw[i].w < cerca && pw[i + 1].w < cerca && pw[i + 2].w < cerca) continue;
        tri_recortado(pw + i);
    }
}

/* un cartel orientado a la camara (bandera 0x800 del nodo, face_camera de 404D10 en la decompilacion del PC): giro R
   alrededor de y que lleva el eje z del padre (dir) a la direccion de la vista (to = ojo - mira, en xz); vertice del
   mundo = v R W (vectores fila, W la matriz del nodo); a la pantalla con la vista: se carga R W vista */
static void carga_cartel(const float (*W)[4])
{
    static matrix_t C __attribute__((aligned(32)));
    float to_x = -ade[0], to_z = -ade[2], l = sqrtf(to_x * to_x + to_z * to_z), dx = W[0][3], dz = W[2][3];
    float cs, sn, RW[4][4]; int i, j, k;
    if (l > 0.0001f) { to_x /= l; to_z /= l; }
    sn = dz * to_x - to_z * dx; cs = to_z * dz + to_x * dx;     /* eje y (con signo) y coseno del giro */
    if (fabsf(sn) < 1.1920929e-7f) { sn = 0; cs = cs < 0 ? -1.0f : 1.0f; }
    for (j = 0; j < 4; j++) {                                   /* R = [cs 0 -sn; 0 1 0; sn 0 cs] (filas) */
        RW[0][j] = cs * W[0][j] - sn * W[2][j]; RW[1][j] = W[1][j]; RW[2][j] = sn * W[0][j] + cs * W[2][j]; RW[3][j] = W[3][j];
    }
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) {
        float t = 0;
        for (k = 0; k < 3; k++) t += RW[i][k] * vista[k][j];
        C[i][j] = t + (i == 3 ? vista[3][j] : 0);
    }
    mat_load(&C);
}
/* mallas [k0, k1) de la lista l; sin_niebla para el cielo. Con recorta (etapas y uniones): las de nodos que no estan en
   mo->vis (nodos_visibles: visibilidad por secciones, LOD y recorte del original) no se dibujan, y las demas se
   recortan por su esfera contra la piramide de la vista */
static int solo_lod = -1;                                       /* trafico: solo las mallas de este LOD (traf_lod) */
static void dibuja_rango(const modelo_t *mo, int l, uint32_t k0, uint32_t k1, int recorta, int sin_niebla, int solo_lista)
{
    float tanh = 320.0f / fx, tanv = 240.0f / fx, kh = sqrtf(1 + tanh * tanh), kv = sqrtf(1 + tanv * tanv);
    uint32_t k, vtx_et0 = vtx_env; int ult = -2, seguro = 0, cartel;
    ta_ofs = mo->ofs != NULL;
    for (k = k0; k < k1; k++) {
        const malla_t *m = mo->mallas + k;
        if (m->n < 3) continue;                                 /* apagada (luces del coche: coche_luces) */
        if (solo_lod >= 0 && !((m->nodo >> solo_lod) & 1)) continue;   /* de otro LOD del coche de trafico */
        if (solo_lista && (m->tex < 0 || (int)mo->texs[m->tex].lista != l)) continue;
        cartel = 0;
        if (recorta) {
            float dx, dy, dz, zv, xv, yv;
            if (mo->vis && m->nodo >= 0 && !(mo->vis[m->nodo >> 5] & (1u << (m->nodo & 31)))) continue;
#ifndef DIAG_SIN_TOPE
            if (vtx_et + (vtx_env - vtx_et0) > VTX_ETAPA_MAX) { ta_corto = 1; ta_et_corto = 1; continue; }
#endif
            cartel = malla_de_cartel(mo, m);
            if (!cartel) {
                dx = m->cx - ojo[0]; dy = m->cy - ojo[1]; dz = m->cz - ojo[2];
                zv = dx * ade[0] + dy * ade[1] + dz * ade[2];
                if (zv + m->r < cerca || zv - m->r > lejos) continue;
                xv = dx * der[0] + dy * der[1] + dz * der[2];
                yv = dx * arr[0] + dy * arr[1] + dz * arr[2];
                if (fabsf(xv) - zv * tanh > m->r * kh || fabsf(yv) - zv * tanv > m->r * kv) continue;
                seguro = zv - m->r > cerca * 1.5f && m->n <= 256;
            }
        }
#ifdef DEP_TEX
        if (recorta && !sin_niebla) {                      /* depuracion: color = numero de textura */
            if (ult == -2) pvr_prim(&cab_col[1][l], sizeof(pvr_poly_hdr_t));
            ult = m->tex; color_fijo = 0xFF000000 | ((((m->tex + 1) & 31) << 3) << 16) | (((((m->tex + 1) >> 5) & 7) << 5) << 8) | 0x40;
            dibuja_malla(mo, m); color_fijo = 0; continue;
        }
#endif
        if (m->tex != ult) {
            ult = m->tex;
            if (m->tex >= 0) pvr_prim(&mo->cabs[sin_niebla][mo->remap ? mo->remap[m->tex] : m->tex], sizeof(pvr_poly_hdr_t));
            else pvr_prim(&cab_col[sin_niebla][l], sizeof(pvr_poly_hdr_t));
        }
        if (cartel) {                                           /* en el marco de su nodo, girado hacia la camara */
            carga_cartel((const float (*)[4])mo->carteles[mo->nodos[m->nodo].cartel]);
            dibuja_malla(mo, m);
            mat_load(&vista);
        }
        else if (recorta && seguro) dibuja_malla_directa(mo, m);
        else dibuja_malla(mo, m);
    }
    if (ta_en_etapa) vtx_et += vtx_env - vtx_et0;
    ta_ofs = 0;
}
/* las mallas idx[0..n) del modelo (las de una pieza animada en una lista: coche_dibuja, traf_dibuja), sin recorte, con
   la matriz ya cargada; respeta solo_lod y las apagadas como dibuja_rango */
static void dibuja_indices(const modelo_t *mo, int l, const uint16_t *idx, uint32_t n)
{
    uint32_t j; int ult = -2;
    ta_ofs = mo->ofs != NULL;
    for (j = 0; j < n; j++) {
        const malla_t *m = mo->mallas + idx[j];
        if (m->n < 3) continue;
        if (solo_lod >= 0 && !((m->nodo >> solo_lod) & 1)) continue;
        if (m->tex != ult) {
            ult = m->tex;
            if (m->tex >= 0) pvr_prim(&mo->cabs[0][mo->remap ? mo->remap[m->tex] : m->tex], sizeof(pvr_poly_hdr_t));
            else pvr_prim(&cab_col[0][l], sizeof(pvr_poly_hdr_t));
        }
        dibuja_malla(mo, m);
    }
    ta_ofs = 0;
}
/* piezas animadas de un modelo (tools/conv_coche.py animadas, conv_trafico.py: la pieza va en el campo del nodo << 8; 0
   = la que no se mueve): idx = las mallas ordenadas por lista y pieza, ini[l][p] = la primera de la pieza p en la lista
   l. Devuelve 1 si hay alguna malla de una pieza animada (0: sin piezas, idx = NULL) */
#define NPZ_MAX 12
static int piezas_indices(const modelo_t *mo, int np, uint16_t **idx, uint16_t ini[3][NPZ_MAX + 1])
{
    uint32_t k, n = 0; int l, p, hay = 0;
    *idx = NULL;
    if (mo->nmallas > 65535 || np > NPZ_MAX) return 0;
    for (k = 0; k < mo->nmallas; k++) { int q = mo->mallas[k].nodo < 0 ? 0 : (mo->mallas[k].nodo >> 8) & 0xFF; if (q > 0 && q < np) hay = 1; }
    if (!hay || !(*idx = malloc(mo->nmallas * sizeof(uint16_t)))) return 0;
    for (l = 0; l < 3; l++)
        for (p = 0; p <= np; p++) {
            ini[l][p] = (uint16_t)n;
            if (p == np) break;
            for (k = mo->inicio[l]; k < mo->inicio[l + 1]; k++) {
                int q = mo->mallas[k].nodo < 0 ? 0 : (mo->mallas[k].nodo >> 8) & 0xFF;
                if ((q < np ? q : 0) == p) (*idx)[n++] = (uint16_t)k;
            }
        }
    return 1;
}
/* matrices de fila de la pila del PC (p @ M; driving/pc_matrix_stack.cpp): cada operacion multiplica por la izquierda,
   M = op M. Con vectores fila la memoria es la misma que la de KOS (fila i = imagen del eje i): mat_apply la usa tal
   cual */
static void m_id(matrix_t M) { memset(M, 0, sizeof(matrix_t)); M[0][0] = M[1][1] = M[2][2] = M[3][3] = 1.0f; }
static void m_tras(matrix_t M, float x, float y, float z)
{
    int j;
    for (j = 0; j < 4; j++) M[3][j] += x * M[0][j] + y * M[1][j] + z * M[2][j];
}
static void m_rot(matrix_t M, int a, int b, float ang)        /* fila a' = c a + s b, fila b' = -s a + c b */
{
    float c = cosf(ang), s = sinf(ang), t; int j;
    for (j = 0; j < 4; j++) { t = c * M[a][j] + s * M[b][j]; M[b][j] = -s * M[a][j] + c * M[b][j]; M[a][j] = t; }
}
#define m_rx(M, ang) m_rot(M, 1, 2, ang)                       /* [[1,0,0],[0,c,s],[0,-s,c]] */
#define m_ry(M, ang) m_rot(M, 2, 0, ang)                       /* [[c,0,-s],[0,1,0],[s,0,c]] */
#define m_rz(M, ang) m_rot(M, 0, 1, ang)                       /* [[c,s,0],[-s,c,0],[0,0,1]] */

/* ---------------------------------------------------------------- ruta de la carrera
   Un punto cada PASO m (tools/conv_tramo.py: centro, anchos, perfil, carriles y columnas de la colision). Es la ruta de
   la etapa de salida seguida de la de la union (la rama elegida) y de la etapa siguiente en cuanto estan cargadas
   (curso_paso): la distancia s sigue sin cortes de una a otra. Sus ficheros ya vienen colocados en el marco de la etapa
   de salida (tools/conv_union.py). */
static modelo_t etapa, coche;
static float *ruta_p, *rumbo, longitud;                         /* puntos; rumbo de la carretera en cada punto (rad) */
/* La ruta es una ventana: su primer punto esta en s = s_base (al dejar atras una etapa se quitan sus puntos, ver
   ruta_recorta) y s sigue creciendo sin cortes; rumbo_ini: rumbo del primer punto (para que no salte 2 pi al recortar) */
static float s_base, rumbo_ini;
static uint32_t ruta_n, ruta_cap, ruta_pr;                      /* puntos, capacidad y floats por punto */
#define PASO 4.0f
#define PR ((int)ruta_pr)
#define NPERF 16
static void ruta_prepara(void)
{
    uint32_t i, n = ruta_n; float ant = rumbo_ini;
    free(rumbo); rumbo = malloc(n * sizeof(float));
    for (i = 0; i < n; i++) {
        const float *a = ruta_p + PR * (i ? i - 1 : 0), *b = ruta_p + PR * (i + 1 < n ? i + 1 : n - 1);
        float t = atan2f(b[0] - a[0], -(b[2] - a[2]));            /* t = (sin, 0, -cos): girar a la derecha suma */
        while (t - ant > M_PI) t -= 2 * M_PI;
        while (t - ant < -M_PI) t += 2 * M_PI;
        rumbo[i] = ant = t;
    }
    longitud = s_base + (n - 1) * PASO;
}
/* en la distancia s: posicion del centro, rumbo y anchos (izquierdo, derecho) */
static void ruta_en(float s, float *p, float *th, float *iz, float *de)
{
    int i; float f; const float *a, *b;
    if (s < s_base) s = s_base;
    if (s > longitud - 0.01f) s = longitud - 0.01f;
    f = (s - s_base) / PASO; i = (int)f; f -= i; a = ruta_p + PR * i; b = a + PR;
    p[0] = a[0] + f * (b[0] - a[0]); p[1] = a[1] + f * (b[1] - a[1]); p[2] = a[2] + f * (b[2] - a[2]);
    *th = rumbo[i] + f * (rumbo[i + 1] - rumbo[i]);
    if (iz) *iz = a[3] + f * (b[3] - a[3]);
    if (de) *de = a[4] + f * (b[4] - a[4]);
}

/* Columnas de la colision del original detras de los carriles (tools/conv_tramo.py superficies): limite transitable
   izquierdo y derecho (hasta el muro o el vacio), NEXT alturas a cada lado entre el perfil y el limite y NBANDA
   franjas de superficie (desvio donde empieza, indice de material de la COLI). Los ficheros antiguos no las tienen. */
#define NEXT 3
#define NBANDA 10
#define C_LIM (5 + NPERF + 6)                                   /* 6 = NCARRIL */
#define C_EXT (C_LIM + 2)
#define C_BANDA (C_EXT + 2 * NEXT)
#define CON_SUP (PR >= C_BANDA + 2 * NBANDA)
#define SUP_MURO 0x010f4060u                                    /* materiales de muro (CbwColiWall) */
/* altura en el desvio x de un punto de la ruta: el perfil (de -izq-2 a der+2) y fuera de el, hasta el limite, las
   alturas exteriores */
static float altura_punto(const float *a, float x)
{
    float g = (x + a[3] + 2) / (a[3] + a[4] + 4) * (NPERF - 1); int j;
    if (CON_SUP && (g < 0 || g > NPERF - 1)) {
        int lado = g < 0; float x0 = lado ? -a[3] - 2 : a[4] + 2, x1 = lado ? -a[C_LIM] : a[C_LIM + 1];
        const float *h = a + C_EXT + (lado ? 0 : NEXT);
        float h0 = lado ? a[5] : a[5 + NPERF - 1], t;
        if (fabsf(x1) <= fabsf(x0) + 0.01f) return h0;
        t = (x - x0) / (x1 - x0) * NEXT;                        /* 0 en el fin del perfil, NEXT en el limite */
        if (t <= 0) return h0;
        if (t >= NEXT) return h[NEXT - 1];
        j = (int)t; t -= j;
        return (j ? h[j - 1] : h0) + t * (h[j] - (j ? h[j - 1] : h0));
    }
    g = fmaxf(0, fminf(NPERF - 1.001f, g));
    j = (int)g; g -= j;
    return a[5 + j] + g * (a[6 + j] - a[5 + j]);
}
/* altura de la calzada en la distancia s y el desvio lateral x (perfil a lo ancho de cada punto de la ruta) */
static float altura(float s, float x)
{
    int i; float f; const float *a;
    if (s < s_base) s = s_base;
    if (s > longitud - 0.01f) s = longitud - 0.01f;
    f = (s - s_base) / PASO; i = (int)f; f -= i; a = ruta_p + PR * i;
    return altura_punto(a, x) + f * (altura_punto(a + PR, x) - altura_punto(a, x));
}
/* limites transitables (positivos) en s: los de la colision, o sin ella los bordes del asfalto */
static void limites(float s, float *li, float *ld)
{
    int i, c = CON_SUP ? C_LIM : 3; float f; const float *a;
    if (s < s_base) s = s_base;
    if (s > longitud - 0.01f) s = longitud - 0.01f;
    f = (s - s_base) / PASO; i = (int)f; f -= i; a = ruta_p + PR * i;
    *li = a[c] + f * (a[PR + c] - a[c]); *ld = a[c + 1] + f * (a[PR + c + 1] - a[c + 1]);
}
/* franjas del punto de la ruta mas cercano a s */
static const float *franjas(float s)
{
    int i;
    if (s < s_base) s = s_base;
    if (s > longitud - 0.01f) s = longitud - 0.01f;
    i = (int)((s - s_base) / PASO + 0.5f); if (i > (int)ruta_n - 1) i = ruta_n - 1;
    return ruta_p + PR * i + C_BANDA;
}
/* bandera de superficie (1 << material de la COLI) en (s, x), como wheel+0x14 del original; 0 sin datos. Fuera de
   los limites, vacio (1) */
static uint32_t superficie(float s, float x)
{
    const float *b; float li, ld; int k;
    if (!CON_SUP) return 0;
    limites(s, &li, &ld);
    if (x < -li || x > ld) return 1;
    b = franjas(s);
    for (k = NBANDA - 1; k > 0 && x < b[2 * k]; k--) ;
    return 1u << ((int)b[2 * k + 1] & 31);
}
/* franja de muro (o vacio) dentro de los limites que toca el intervalo [x - m, x + m]: 1 y sus bordes */
static int muro_interior(float s, float x, float m, float *xa, float *xb)
{
    const float *b; float li, ld; int k;
    if (!CON_SUP) return 0;
    limites(s, &li, &ld); b = franjas(s);
    for (k = 0; k < NBANDA; k++) {
        float a0 = b[2 * k], a1 = k < NBANDA - 1 ? b[2 * k + 2] : ld; uint32_t f = 1u << ((int)b[2 * k + 1] & 31);
        if (a1 - a0 < 0.01f || !(f & (SUP_MURO | 1u))) continue;
        if (x + m > a0 && x - m < a1) { *xa = a0; *xb = a1; return 1; }
    }
    return 0;
}

/* mueve el centro del punto a de la ruta o metros a la derecha (r: vector a la derecha) sin mover la calzada: los
   desvios se miden desde el centro nuevo (anchos, carriles, limites y franjas; el perfil y las alturas exteriores van
   de borde a borde y no cambian) */
static void ruta_recentra(float *a, float o, float rx, float rz)
{
    int k;
    a[0] += o * rx; a[2] += o * rz; a[3] += o; a[4] -= o;
    for (k = 0; k < C_LIM - 5 - NPERF; k++) a[5 + NPERF + k] -= o;  /* carriles */
    if (CON_SUP) {
        a[C_LIM] += o; a[C_LIM + 1] -= o;
        for (k = 0; k < NBANDA; k++) a[C_BANDA + 2 * k] -= o;
    }
}
static int ruta_reserva(uint32_t n)
{
    float *q;
    if (n <= ruta_cap) return 0;
    if (!(q = realloc(ruta_p, n * PR * sizeof(float)))) return -1;
    ruta_p = q; ruta_cap = n;
    return 0;
}
/* la ruta de la carrera = la del modelo (la etapa de salida), con sitio para la union y la etapa siguiente */
static int ruta_pon(const modelo_t *m)
{
    ruta_n = 0; ruta_pr = m->paso_ruta; s_base = 0; rumbo_ini = 0;
    if (ruta_reserva(m->nruta + 2000)) return -1;
    memcpy(ruta_p, m->ruta, m->nruta * PR * sizeof(float)); ruta_n = m->nruta;
    ruta_prepara();
    return 0;
}
/* Anade la ruta del modelo m detras de la actual. Empieza PASO m por delante del ultimo punto (sus puntos se toman en
   esa posicion fraccionaria, interpolando: el paso sigue siendo PASO y el coche no da un tiron) y su centro arranca en
   el de la actual y vuelve al suyo en RECENTRA puntos (dos ficheros no tienen por que coincidir en el centro: la ruta
   de una etapa sigue los carriles y la de la union el poligono de la calzada; el coche no debe saltar de lado).
   Si m empieza mas de 1,5 PASO por delante, el hueco se rellena con puntos interpolados (la ruta de la carrera no da
   saltos). Devuelve el primer punto de m en la ruta, o -1 */
#define RECENTRA 25
static uint32_t ruta_salto;                                     /* primer punto de m usado (la parte entera) */
static int ruta_anade(const modelo_t *m)
{
    const float *b = m->ruta; uint32_t j = 0, k, c, i0 = ruta_n, n; float ax, az, tx, tz, l, o0 = 0, d0 = 0, d1, f = 0;
    if ((int)m->paso_ruta != PR || m->nruta < 8 || !ruta_n) return -1;
    ax = ruta_p[PR * (ruta_n - 1)]; az = ruta_p[PR * (ruta_n - 1) + 2];
    tx = b[PR] - b[0]; tz = b[PR + 2] - b[2]; l = sqrtf(tx * tx + tz * tz) + 1e-6f; tx /= l; tz /= l;
    /* d: avance de cada punto de m sobre el ultimo de la actual; se busca d = PASO entre j y j + 1 */
    for (j = 0; j + 6 < m->nruta; j++) {
        d1 = (b[PR * (j + 1)] - ax) * tx + (b[PR * (j + 1) + 2] - az) * tz;
        d0 = (b[PR * j] - ax) * tx + (b[PR * j + 2] - az) * tz;
        if (d0 >= PASO) break;                                  /* m empieza lejos: tal cual */
        if (d1 >= PASO) { f = (PASO - d0) / (d1 - d0 + 1e-6f); break; }
    }
    n = m->nruta - j - 1;
    if (d0 >= 1.5f * PASO) {                                    /* hueco entre los dos ficheros: se rellena cada ~PASO m */
        uint32_t nf = (uint32_t)(d0 / PASO + 0.5f) - 1;
        if (ruta_reserva(ruta_n + nf + n)) return -1;
        for (k = 1; k <= nf; k++) {
            const float *p = ruta_p + PR * (ruta_n - 1), *q = b + PR * j; float *o = ruta_p + PR * (ruta_n - 1 + k), t = (float)k / (nf + 1);
            for (c = 0; c < ruta_pr; c++) o[c] = p[c] + t * (q[c] - p[c]);
            if (CON_SUP) for (c = 0; c < NBANDA; c++) o[C_BANDA + 2 * c + 1] = p[C_BANDA + 2 * c + 1];
        }
        ruta_n += nf; i0 = ruta_n;
        ax = ruta_p[PR * (ruta_n - 1)]; az = ruta_p[PR * (ruta_n - 1) + 2];
    }
    if (ruta_reserva(ruta_n + n)) return -1;
    for (k = 0; k < n; k++) {
        const float *p = b + PR * (j + k), *q = p + PR; float *o = ruta_p + PR * (ruta_n + k);
        for (c = 0; c < ruta_pr; c++) o[c] = p[c] + f * (q[c] - p[c]);
        if (CON_SUP) for (c = 0; c < NBANDA; c++) o[C_BANDA + 2 * c + 1] = (f < 0.5f ? p : q)[C_BANDA + 2 * c + 1];   /* material */
    }
    for (k = 0; k < RECENTRA && k < n; k++) {
        float *p = ruta_p + PR * (ruta_n + k);                  /* la derecha, de los puntos sin mover */
        const float *p0 = b + PR * (j + k - (j + k > 0)), *p1 = b + PR * (j + k + 1);
        float rx = -(p1[2] - p0[2]), rz = p1[0] - p0[0]; l = sqrtf(rx * rx + rz * rz) + 1e-6f; rx /= l; rz /= l;
        if (!k) o0 = (ax - p[0]) * rx + (az - p[2]) * rz;      /* el centro de la actual, a la derecha */
        ruta_recentra(p, o0 * 0.5f * (1 + cosf((float)M_PI * k / RECENTRA)), rx, rz);
    }
    ruta_salto = j; ruta_n += n;
    ruta_prepara();
    return i0;
}
/* quita los q primeros puntos de la ruta (lo que ya quedo atras): las s no cambian (s_base sube) */
static void ruta_recorta(uint32_t q)
{
    if (!q || q + 8 > ruta_n) return;
    rumbo_ini = rumbo[q];
    memmove(ruta_p, ruta_p + PR * q, (ruta_n - q) * PR * sizeof(float));
    ruta_n -= q; s_base += q * PASO;
    ruta_prepara();
}
/* coloca un modelo recien leido en el marco de la carrera: M = (x, y, z, giro y), como transforma de
   tools/conv_tramo.py (x' = x cos + z sen + x0, z' = -x sen + z cos + z0, y' = y + y0); el cielo (lista 3, alrededor
   de la camara) solo gira. En la ruta: posicion, perfil y alturas exteriores (el resto son desvios). Lo hace el hilo
   de carga, antes de dar el modelo por leido */
static void modelo_coloca(modelo_t *m, const float *M)
{
    float c = cosf(M[3]), sn = sinf(M[3]), x; uint32_t k, i, j;
    for (k = 0; k < m->nmallas; k++) {
        malla_t *ma = m->mallas + k; vert_t *v = m->verts + ma->v0;
        int cl = k >= m->inicio[3] && k < m->inicio[4];
        float tx = cl ? 0 : M[0], ty = cl ? 0 : M[1], tz = cl ? 0 : M[2];
        if (malla_de_cartel(m, ma)) continue;                   /* en el marco de su nodo: se coloca su matriz */
        for (i = 0; i < ma->n; i++, v++) { x = v->x; v->x = x * c + v->z * sn + tx; v->z = -x * sn + v->z * c + tz; v->y += ty; }
        x = ma->cx; ma->cx = x * c + ma->cz * sn + tx; ma->cz = -x * sn + ma->cz * c + tz; ma->cy += ty;
    }
    for (k = 0; k < m->nnodos; k++) {
        nodo_t *n = m->nodos + k;
        x = n->cx; n->cx = x * c + n->cz * sn + M[0]; n->cz = -x * sn + n->cz * c + M[2]; n->cy += M[1];
    }
    for (k = 0; k < m->ncarteles; k++) {                        /* filas 0-2 giran (y la columna 3: eje z del padre) */
        float (*W)[4] = m->carteles[k];
        for (i = 0; i < 4; i++) { x = W[i][0]; W[i][0] = x * c + W[i][2] * sn; W[i][2] = -x * sn + W[i][2] * c; }
        x = W[0][3]; W[0][3] = x * c + W[2][3] * sn; W[2][3] = -x * sn + W[2][3] * c;
        W[3][0] += M[0]; W[3][1] += M[1]; W[3][2] += M[2];
    }
    for (i = 0; i < m->nruta; i++) {
        float *a = m->ruta + m->paso_ruta * i;
        x = a[0]; a[0] = x * c + a[2] * sn + M[0]; a[2] = -x * sn + a[2] * c + M[2]; a[1] += M[1];
        for (j = 0; j < NPERF; j++) a[5 + j] += M[1];
        if (m->paso_ruta >= C_BANDA + 2 * NBANDA) for (j = 0; j < 2 * NEXT; j++) a[C_EXT + j] += M[1];
    }
}

/* ---------------------------------------------------------------- coche */
typedef struct { float s, x, a, v; float pos[3], fw[3], up[3]; int choque, derrape, t_derrape, t_muro; float deriva, freno_ant, gas_ant;
                 float pose_t[3], pose_r[3]; } coche_t;   /* pose: animacion de choque del original (solo al dibujar) */
static coche_t yo;
/* fisica del original para el coche del jugador (comun/or2_fisica.c) con la tabla de PARAM.DAT */
#ifndef MODELO
#define MODELO 6                                                /* modelo del coche (car+0x11): 6 = Testarossa */
#endif
#ifndef ACEL
#define ACEL 1.0f                                               /* multiplicador del par motor: 1 = el original (con
                                                                   -DACEL=1.35f, la aceleracion de mas que pidio el
                                                                   probador antes; no es del juego) */
#endif
static int manual_, pide_cambio, auto_piloto, sfx_ok, golpe_cd;
static int voz_ok, voz_contacto;                              /* voces.c; contacto con impulso en este paso */
static or2_param_t fpar; static or2_coche_t fis; static int fis_ok, columna = -1;
/* mando del jugador para or2_volante (volante del original: eje -128..127, cruceta, etapa para CheckWanderer); usa = 1
   solo en el paso de carrera con el jugador al mando */
static struct { int eje, izq, der, usa, nivel; } mando;
#define VEL_DIRECCION 4                                         /* opcion de velocidad de la direccion (4471A0: 4) */
/* coche elegido: ficha del menu (COCHEnn.DAT, tools/conv_coche.py) y su modelo (car+0x11) segun COCHES.DAT ('CCH1' y
   un byte por ficha, 0xFF = sin coche); el modelo da la columna de la fisica y el tacometro del HUD */
#define N_FICHAS 15
static uint8_t ficha_modelo[N_FICHAS]; static int coche_ficha = -1, coche_malo = -1, modelo_jug = MODELO;
static int fisica_modelo(int m);
static or2_choques_t choq;                                      /* choques del original (bloques de PARAM.DAT) */
static uint32_t sec_tab[16]; static int cjm_ok;                 /* efectos del golpe entre coches (SEC1 de TRAFC.DAT) */
static float *param_dat;                                        /* se queda en memoria: las animaciones apuntan aqui */
static const float *par_manual;                                 /* bloque PAR2 de PARAM.DAT (NULL si es antiguo) */
/* camara de carrera del original (demo/camara.c) con las tablas CAM1 de PARAM.DAT; sin ellas, la de antes */
#define CAM_BYTES (0x64c + 0x5460 + 0x438)
static cam_tablas_t cam_tab; static int cam_ok; static cam_t cam = { .elegida = 2 };
static float cam_t_choque;                                      /* +2C8 del paso anterior (para ver la entrada) */
#ifdef PRUEBA_GUION
/* prueba de la camara: conduce el piloto, toque de freno en GUION_DERRAPE, Y (vista) en los cuadros 700 y 760, y la
   imagen se congela 3 s en estos cuadros de carrera para capturas comparables */
#ifndef GUION_DERRAPE
#define GUION_DERRAPE 560
#endif
#ifdef PRUEBA_CHOQUE
static const int guion_cong[] = { 75, 90, 120, 200, 260 };     /* el choque contra el muro (cuadro 60) */
#else
static const int guion_cong[] = { 300, GUION_DERRAPE + 30, GUION_DERRAPE + 60, GUION_DERRAPE + 100, 740, 800 };
#endif
static int guion_i; static uint64_t guion_hasta;
#ifdef PRUEBA_EMBISTE
/* prueba de choques (con PRUEBA_TRAFICO y DIAG_CHOQUE): gas a fondo hacia el coche de trafico mas cercano por delante;
   tras el primer golpe la imagen se congela 3 s en el cuadro siguiente y 15, 40, 90 y 150 despues */
static int emb_golpe = -100000;
static const int emb_cong[] = { 1, 15, 40, 90, 150 };
#endif
#endif
/* "Posicion de carretera" del original (car+0x64): la seccion 5 de la colision COLI0200 del poligono bajo el coche
   (calc_ground_coli_face); una cada m_pos metros de ruta, el de CURSO.DAT de la etapa actual (tools/conv_union.py
   m_por_posicion: 7,91-7,99 m en las 15 etapas). Antes se suponian 2,5 m. */
static float m_pos = 7.95f;
/* grua del choque fuerte: PlWrecker (pc_pl_wrecker_delayed) avanza 10 posiciones (pc_advance_on_road_place, +8 del
   sitio = la posicion) */
#define GRUA_AVANCE (10.0f * m_pos)
/* rumbo adelante (+D4E, driving/pc_common_control.cpp): cuerda entre el sitio actual y el de 16 posiciones mas alla
   (r034_next_road 1 + 1 + 14, interpolados con la fraccion de la seccion) */
#define ADELANTE_POS 16.0f
static float grua_s, grua_x; static int grua_on;
/* Tiempo de la carrera como el original (race_manager 7D394C): un contador de cuadros de la logica que baja 1 por
   cuadro (450AC0) y se muestra como cuadros / 60,2 + 0,34. Inicial (450790) y ampliaciones (450CC0) con
   GetExtendedTime (44DC70 / SLES-53998 0x176730, mismo codigo instruccion a instruccion):
     t = tabla[fila][nivel][dificultad] + k636904[clave] (si 43F960 == 0 y 456D60 <= 1) + 60 (si 48B1A0 != 1 y nivel > 0)
   con la tabla TIM1 de PARAM.DAT (la que 44C020 / 0x176040 copia a 7D2E98 segun el preset [78024C]) y la correccion
   por clave de etapa KTE1 (k636904 / 0x35a938). Para el modo de la demo:
   - preset 0: el curso de OutRun 2 (csc_data_2; arcade_fn5 4C3DC0 pone +27 = 0 o 1 para los cursos de 5 niveles y
     2/3 para los de 15); con 0 o 1, 43F960 (preset 2 o 3) da 0.
   - fila 0: la variante del modo OutRun es 1 (4C3DC0: fila 0 de la lista -> +1F = 1; 2 Heart Attack, 0 Time
     Attack) y 44DC70 da fila 0 a la variante 1 (2 y 5 -> 1; 0 y 7 -> 2).
   - dificultad 0: [7C24C0] = licencia +E0, que 4471A0 (licencia nueva) pone a 0; 0 tambien si 43F860.
   - 456D60 <= 1 (un jugador): se suma k636904 de la clave de la etapa.
   - 48B1A0 (83036D) != 1 con el coche de serie (1 = preparado, 5051D0): +60 cuadros en los niveles 1..4.
   CONFIRMADO en el ELF: la formula (0x176730), la tabla k636904 y su uso. PROBABLE: variante 1, preset 0 y
   dificultad 0 (del frontal del PC, sin trazar en el de PS2). */
#define TIM_PRESET 0
#define TIM_FILA 0                                              /* variante 1 (OutRun) */
#define TIM_DIF 0
#define TIM_SERIE 1                                             /* coche sin preparar: 48B1A0 != 1 */
#define TIM_HZ 60.2f
static const int16_t *tim_tab;                                  /* 4 x 3 x 15 x 5 cuadros, NULL sin TIM1 */
static const int16_t *tim_kte;                                  /* 15 cuadros por clave de etapa, NULL sin KTE1 */
static int tim_cuadros;                                         /* 7D394C */
/* 44DC70 para la etapa de clave k y nivel (profundidad en el arbol, 44C850) nivel */
static int tim_zona(int k, int nivel)
{
    int t;
    if (!tim_tab) return (int)((nivel ? 60.0f : 80.0f) * TIM_HZ);   /* PARAM.DAT antiguo: los valores de antes */
    if (nivel < 0) nivel = 0;
    if (nivel > 14) nivel = 14;
    if (k < 0 || k >= 15) k = 14;                               /* 44DC70: clave fuera de 0..14 -> 0xE */
    t = tim_tab[((TIM_PRESET * 3 + TIM_FILA) * 15 + nivel) * 5 + TIM_DIF];
    if (tim_kte) t += tim_kte[k];                               /* un jugador, curso de 5 niveles */
    if (TIM_SERIE && nivel != 0) t += 60;
    return t;
}
static int fisica_carga(void)
{
    uint32_t sz; float *t = gd_carga("PARAM.DAT", &sz);
    if (!t || sz < (200 * 19 + 162 + 256) * 4) return -1;
    par_manual = NULL;
    if (sz >= 652 + 36 && !memcmp((const uint8_t *)t + sz - 652, "PAR2", 4)) {   /* curva de par del manual, al final */
        par_manual = (const float *)(const void *)((const uint8_t *)t + sz - 648);
        sz -= 652;
    }
    if (sz >= 36 + 4 + 4 * 0x1c2 && !memcmp((const uint8_t *)t + sz - 36, "KTE1", 4)) {   /* al final, si esta */
        tim_kte = (const int16_t *)(const void *)((const uint8_t *)t + sz - 32);
        sz -= 36;
    }
    if (sz >= 4 + 4 * 0x1c2 && !memcmp((const uint8_t *)t + sz - 4 - 4 * 0x1c2, "TIM1", 4))
        tim_tab = (const int16_t *)(const void *)((const uint8_t *)t + sz - 4 * 0x1c2);
#ifndef PRUEBA_CAM_ANTES                                                /* prueba: la camara de antes */
    if (tim_tab && sz >= 4 + 4 * 0x1c2 + 4 + CAM_BYTES &&
        !memcmp((const uint8_t *)t + sz - 4 - 4 * 0x1c2 - CAM_BYTES - 4, "CAM1", 4)) {
        const float *c = (const float *)(const void *)((const uint8_t *)t + sz - 4 - 4 * 0x1c2 - CAM_BYTES);
        cam_tab.vistas = c; cam_tab.vistas_mod = c + 31 * 13; cam_tab.modelos = c + 31 * 13 + 20 * 30 * 9; cam_ok = 1;
    }
#endif
    or2_carga_choques(t, sz, &choq);
    if (cjm_ok) { memcpy(choq.se, sec_tab, sizeof choq.se); choq.se_ok = 1; }   /* SEC1 de TRAFC.DAT */
    param_dat = t;
    return fisica_modelo(modelo_jug);
}
/* la columna de PARAM.DAT del modelo (peso, agarre, motor, marchas: los del original para ese coche) */
static int fisica_modelo(int m)
{
    if (!param_dat) return -1;
#ifdef COLUMNA
    columna = COLUMNA; (void)m;
#else
    columna = or2_columna_modelo(&choq, m);                     /* la tabla de columnas del juego (5051D0) */
    if (columna < 0) columna = 4;                               /* la del Testarossa si el fichero es antiguo */
#endif
    if (or2_carga_param(param_dat, 200, columna, &fpar) < 0) { fis_ok = 0; return -2; }
    or2_carga_curvas(&fpar, param_dat + 200 * 19, param_dat + 200 * 19 + 162);
    if (par_manual) or2_carga_par_manual(&fpar, par_manual);   /* torque_table 1 (event+0x13 = 1) */
    fpar.modelo = m;                                            /* car+0x11: >= 15 los preparados */
    if (ACEL != 1.0f) fpar.par_mult *= ACEL;                    /* opcional (ACEL): no es del original */
    fis_ok = 1; return 0;
}
static float dif_ang(float a) { while (a > M_PI) a -= 2 * M_PI; while (a < -M_PI) a += 2 * M_PI; return a; }
#define VMAX 100.0f                                          /* techo teorico; punta real unos 300 km/h */
/* posicion y orientacion en el mundo a partir de (s, x, a), apoyado en la calzada (altura, cabeceo, balanceo) */
static void coche_coloca(coche_t *c)
{
    float p[3], th, t[3], r[3], ca = cosf(c->a), sa = sinf(c->a), hd, ht, hi, hr, rr[3];
    float cv = cosf(c->a + c->deriva), sv = sinf(c->a + c->deriva);  /* la carroceria, cruzada al derrapar */
    ruta_en(c->s, p, &th, NULL, NULL);
    t[0] = sinf(th); t[1] = 0; t[2] = -cosf(th); r[0] = cosf(th); r[1] = 0; r[2] = sinf(th);
    c->pos[0] = p[0] + c->x * r[0]; c->pos[2] = p[2] + c->x * r[2];
    c->fw[0] = cv * t[0] + sv * r[0]; c->fw[2] = cv * t[2] + sv * r[2];
    hd = altura(c->s + 1.6f * ca, c->x + 1.6f * sa); ht = altura(c->s - 1.6f * ca, c->x - 1.6f * sa);
    hi = altura(c->s, c->x - 0.8f); hr = altura(c->s, c->x + 0.8f);
    c->pos[1] = 0.25f * (hd + ht + hi + hr);
    c->fw[1] = (hd - ht) / 3.2f; norm(c->fw);
    rr[0] = -c->fw[2]; rr[1] = (hr - hi) / 1.6f; rr[2] = c->fw[0]; norm(rr);
    c->up[0] = rr[1] * c->fw[2] - rr[2] * c->fw[1]; c->up[1] = rr[2] * c->fw[0] - rr[0] * c->fw[2];
    c->up[2] = rr[0] * c->fw[1] - rr[1] * c->fw[0]; norm(c->up);
}

/* ---------------------------------------------------------------- trafico */
#define NCARRIL 6
/* plazas de trafico: el original abre hasta [registro de aparicion +4] coches (2..12 en el curso; tope 20 = 0x14 de
   4787E0) mas los de los puntos fijos (478B90), en 24 sucesos; 16 plazas cubren el curso (el TA lo reparte ta_cabe) */
#define NTRAF 16
/* Campos del original (car work del PC): b08 desplazamiento lateral respecto al centro del carril (m del original:
   4 m = un carril, + hacia el carril siguiente), b14/b18 cuadros que faltan / totales del cambio de carril, dir su
   sentido, b58 deriva tras el golpe (0xB / 0xC), b60 cuadros de recuperacion, b64 cuadros sin choques, b6c golpeado */
typedef struct { coche_t c; int carril, modelo, destino; float v_des;
                 int golpe, lado; float t_golpe, vlat;          /* golpe: estado de choque (16..19) o -1 */
                 float t_recup, wa[3]; int d22;                 /* +2F8, pose del cuadro anterior (mundo de or2), +D22 */
                 float b08; int b14, b18, dir, b58, b60, b64, b6c;
                 int pt_marca, b72, color; } trafico_t;               /* puntuacion: +C5C (PT_*) y +B72 (rebufo reciente) */
static trafico_t traf[NTRAF];
/* +C5C del coche para la puntuacion (4F2860): 0x40 ya adelantado, 0x400000 tocado (puntos a la mitad), 0x800000
   chocado (sin puntos) */
#define PT_PASADO 0x40
#define PT_MITAD  0x400000
#define PT_SIN    0x800000
static int pt_toca;                                             /* contacto con el trafico en este cuadro: 1, 3 golpe */
static uint32_t sup_jug = 2u;                                   /* +244 del jugador: OR de las superficies de las ruedas */
static uint32_t sup_rueda[4];                                   /* superficie bajo cada rueda (marcas: efectos_paso) */
#define NMODTRAF 9                                              /* coches de calle (TRAFn.DAT) */
static modelo_t mtraf[NMODTRAF];
static float largo_traf[NMODTRAF];                              /* mitad del largo de cada modelo */
/* TRAFC.DAT (tools/conv_trafico.py): por modelo, el registro de 4866C0 (clase +324, radio) y la primera caja de choque
   (520950); y la tabla por clase de 472090 (5B3E08 del PC): referencia, aceleracion, frenada, aceleracion fuerte y
   frenada fuerte, en m/cuadro por cuadro */
typedef struct { int modelo, clase, se, k320; float radio, caja[7]; } trafc_t;   /* 48 bytes, como TRAFC.DAT */
static trafc_t trc[NMODTRAF];
static trafc_t cjm[30];                                         /* los coches del jugador (bloque CJM1), por modelo */
static float acl[7][5];
static int trc_ok, cambio_cd;                                   /* cambio_cd: cuadros desde el ultimo cambio (804350) */
static void traf_medidas(void)
{
    int m; uint32_t i;
    for (m = 0; m < NMODTRAF; m++) {
        float lo = 0, hi = 0;
        for (i = 0; i < mtraf[m].nvert; i++) { float z = mtraf[m].verts[i].z; if (z < lo) lo = z; if (z > hi) hi = z; }
        largo_traf[m] = (hi - lo) * 0.5f;
        trc[m].clase = 3; trc[m].caja[0] = 0.9f; trc[m].caja[2] = largo_traf[m]; trc[m].caja[5] = -largo_traf[m];
    }
}
/* colores del trafico (TCX1 de tools/conv_trafico.py, en el sitio de la ruta): el original da a cada coche nuevo de
   un modelo el color siguiente de ese modelo (0, 1, 2, 0...; 2 colores en el 38 y 1 en el 39 y el 41: PC 478B90) y
   cambia las texturas de su lista de colores (408C80). traf_remap[m][c]: textura -> la del color c (NULL en el 0) */
static int32_t *traf_remap[NMODTRAF][3]; static int traf_ncol[NMODTRAF]; static uint8_t traf_cuenta[NMODTRAF];
static void traf_colores(int m)
{
    const uint32_t *w = (const uint32_t *)mtraf[m].ruta; uint32_t c, k, i;
    traf_ncol[m] = 1;
    if (!w || mtraf[m].paso_ruta != 1 || mtraf[m].nruta < 3 || memcmp(w, "TCX1", 4) || 3 + 3 * w[2] > mtraf[m].nruta) return;
    for (k = 0; k < w[2]; k++) for (c = 0; c < 3; c++) if (w[3 + 3 * k + c] >= mtraf[m].ntex) return;
    for (c = 1; c < 3; c++) {
        if (!(traf_remap[m][c] = malloc(mtraf[m].ntex * sizeof(int32_t)))) return;
        for (i = 0; i < mtraf[m].ntex; i++) traf_remap[m][c][i] = i;
        for (k = 0; k < w[2]; k++) traf_remap[m][c][w[3 + 3 * k]] = w[3 + 3 * k + c];
    }
    traf_ncol[m] = w[1] < 1 ? 1 : w[1] > 3 ? 3 : (int)w[1];
}
/* Tablas de aparicion del original por etapa (bloque 'TAP1' de TRAFC.DAT, tools/conv_trafico.py aparicion: del
   descriptor de tramo +0x80 / +0x84 del ejecutable de PS2, PC +0x68 / +0x6C, que 4EFB50 / 4EFAF0 copian a 84BD08 /
   84BF60 al cargar la etapa; los puntos fijos de 478B90 (tabla 6A6CE0 del PC); el registro de la union por defecto
   6A5DC4; los pesos por clase de 46CC10 (5B3DE4) y la lista de modelos de 46CE30 (64E198)), en orden de clave del
   arbol (como CURSO.DAT). Registro de aparicion (12 bytes): u16 posicion de inicio, +4 coches como mucho, +5
   separacion en posiciones (minimo 3), +6..+0xA pesos de clase (4EFB90 los deja en 0,0,0,0,100: siempre la lista de
   modelos), +0xB carriles prohibidos (bit k = carril k). Registro de velocidad (0x46 bytes): u16 posicion, +0xC
   velocidad del coche nuevo en km/h del modelo (x 0,0046142 = m/cuadro; 100 en todo el curso). */
#define TAP_NFIJO 24
#define TAP_ETAPAS 16
typedef struct { uint16_t pos; int8_t carril; uint8_t modelo; int8_t color; uint8_t tipo; uint16_t r; } tap_fijo_t;
typedef struct { uint8_t ap[0x60]; uint8_t ve[0x160]; uint32_t nfijo; tap_fijo_t fijo[TAP_NFIJO]; } tap_etapa_t;
static tap_etapa_t tap[TAP_ETAPAS];
static int ntap;                                                /* 0: sin tablas (TRAFC.DAT viejo) */
static uint8_t tap_defecto[12], tap_modelos[8], tap_usado[TAP_ETAPAS][TAP_NFIJO];
static uint16_t tap_peso[8];
static void traf_tablas_aparicion(const uint8_t *d, uint32_t sz)
{
    uint32_t n;
    ntap = 0;
    if (sz < 8 + 12 + 16 + 8 || memcmp(d, "TAP1", 4)) return;
    memcpy(&n, d + 4, 4);
    if (n > TAP_ETAPAS || sz < 8 + 12 + 16 + 8 + n * sizeof(tap_etapa_t)) return;
    memcpy(tap_defecto, d + 8, 12); memcpy(tap_peso, d + 20, 16); memcpy(tap_modelos, d + 36, 8);
    memcpy(tap, d + 44, n * sizeof(tap_etapa_t));
    ntap = (int)n;
}
static void traf_tablas(void)
{
    uint32_t sz, n, m; const uint8_t *d = gd_carga("TRAFC.DAT", &sz);
    if (!d || sz < 8 || memcmp(d, "TRC1", 4)) return;
    memcpy(&n, d + 4, 4);
    /* antes se leia con registros de 52 bytes y el fichero los trae de 48 (sizeof trafc_t): TRAFC.DAT no se cargaba */
    if (n != NMODTRAF || sz < 8 + n * sizeof(trafc_t) + 8 + 35 * 4 || memcmp(d + 8 + n * sizeof(trafc_t), "ACL1", 4)) return;
    for (m = 0; m < n; m++) memcpy(&trc[m], d + 8 + m * sizeof(trafc_t), sizeof(trafc_t));
    memcpy(acl, d + 8 + n * sizeof(trafc_t) + 8, sizeof acl);
    trc_ok = 1;
    traf_tablas_aparicion(d + 8 + n * sizeof(trafc_t) + 8 + 35 * 4, sz - (8 + n * sizeof(trafc_t) + 8 + 35 * 4));
    /* al final: los coches del jugador (CJM1) y los efectos del golpe entre coches (SEC1) */
    if (sz >= 68 + 8 + 30 * sizeof(trafc_t) + 8 && !memcmp(d + sz - 68, "SEC1", 4) &&
        !memcmp(d + sz - 68 - 8 - 30 * sizeof(trafc_t), "CJM1", 4)) {
        memcpy(cjm, d + sz - 68 - 30 * sizeof(trafc_t), sizeof cjm);
        memcpy(sec_tab, d + sz - 64, sizeof sec_tab);
        cjm_ok = 1; memcpy(choq.se, sec_tab, sizeof choq.se); choq.se_ok = 1;
    }
}
/* LOD del coche de trafico por la distancia, como 46DF50 del PC (en SLES-53998 0x1dd728, constantes en lui): z de la
   camara (negativa delante) frente a base - 130 (LOD 4), base - 85 (3), base - 50 (2), base - 40 (1) y 5 (0; mas alla,
   detras de la camara, 4); base = -35 en los modelos 0x1f y 0x27 y -25 en el 0x28 (los grandes), 0 en los demas.
   zv es la distancia delante de la camara (z = -zv). TRAFn.DAT lleva en el campo del nodo de cada malla la mascara de
   los LOD que la dibujan (tools/conv_trafico.py); los ficheros viejos sin LOD (nodo -1) se dibujan enteros. */
static uint8_t traf_con_lod[NMODTRAF];
static int traf_lod(int m, float zv)
{
    int mod = trc_ok ? trc[m].modelo : 0; float base = mod == 0x1f || mod == 0x27 ? -35.0f : mod == 0x28 ? -25.0f : 0.0f, z = -zv;
    if (z <= base - 130.0f) return 4;
    if (z <= base - 85.0f) return 3;
    if (z <= base - 50.0f) return 2;
    if (z <= base - 40.0f) return 1;
    return z <= 5.0f ? 0 : 4;
}
static uint32_t semilla = 12345;
static float azar(void) { semilla = semilla * 1103515245u + 12345u; return ((semilla >> 8) & 0xFFFF) / 65536.0f; }
static float carril_x(float s, int k)
{
    int i; float f; const float *a;
    if (s < s_base) s = s_base;
    if (s > longitud - 0.01f) s = longitud - 0.01f;
    f = (s - s_base) / PASO; i = (int)f; f -= i; a = ruta_p + PR * i;
    return a[5 + NPERF + k] + f * (a[PR + 5 + NPERF + k] - a[5 + NPERF + k]);
}
/* carril mas cercano a x en s */
static int carril_cerca(float s, float x)
{
    int k, m = 0; float d = 1e9f;
    for (k = 0; k < NCARRIL; k++) { float e = fabsf(carril_x(s, k) - x); if (e < d) { d = e; m = k; } }
    return m;
}
/* separacion (con signo) entre el carril k y el siguiente: lo que en el original son 4 m */
static float carril_paso(float s, int k)
{
    return k < NCARRIL - 1 ? carril_x(s, k + 1) - carril_x(s, k) : carril_x(s, k) - carril_x(s, k - 1);
}
/* posicion lateral en carriles (carril + b08 / 4): la +268 del original, en unidades de carril */
static float traf_pos(const trafico_t *t) { return t->carril + t->b08 * 0.25f; }
static float yo_pos(float s)
{
    int k = carril_cerca(s, yo.x);
    return k + (yo.x - carril_x(s, k)) / carril_paso(s, k);
}
/* el carril k esta libre entre s-atras y s+delante (sin contar al propio coche) */
static int carril_libre(const trafico_t *yo_t, int k, float s, float atras, float delante)
{
    int i;
    for (i = 0; i < NTRAF; i++) {
        const trafico_t *t = traf + i; float d;
        if (t == yo_t) continue;
        if (t->carril != k && t->destino != k) continue;
        d = t->c.s - s;
        if (d > -atras && d < delante) return 0;
    }
    return 1;
}
/* punto (s, x) de la carretera en el mundo de or2_fisica (x a la derecha, y adelante = -z del dibujo) y rumbo */
static void mundo_de(float s, float x, float *wx, float *wy, float *th)
{
    float p[3];
    ruta_en(s, p, th, NULL, NULL);
    *wx = p[0] + x * cosf(*th); *wy = -(p[2] + x * sinf(*th));
}
/* 4755C0: la pose del cuadro anterior (+16C.., +17C..) para la prueba de choque */
static void traf_pose_ant(trafico_t *t)
{
    float th; mundo_de(t->c.s, t->c.x, &t->wa[0], &t->wa[1], &th); t->wa[2] = th + t->c.a;
}
static void traf_nace(trafico_t *t, float s)
{
    int k, n;
    if (!ntap) {                                                /* sin tablas del original (TRAFC.DAT viejo) */
        t->modelo = (int)(azar() * NMODTRAF) % NMODTRAF;
        t->color = traf_cuenta[t->modelo] >= traf_ncol[t->modelo] ? 0 : traf_cuenta[t->modelo];
        traf_cuenta[t->modelo] = t->color + 1;
        k = (int)(azar() * NCARRIL) % NCARRIL;
        for (n = 0; n < NCARRIL && !carril_libre(t, k, s, 25.0f, 25.0f); n++) k = (k + 1) % NCARRIL;
        t->carril = t->destino = k;
        t->v_des = t->c.v = 36.0f + azar() * 14.0f;
    }                                                           /* con tablas: modelo, color, carril y velocidad ya puestos */
    t->c.s = s; t->c.a = 0;
    t->golpe = -1; t->t_golpe = 0; t->vlat = 0; t->t_recup = 0; t->d22 = 0; memset(t->c.pose_t, 0, 12); memset(t->c.pose_r, 0, 12);
    t->b08 = 0; t->b14 = t->b18 = 0; t->dir = 0; t->b58 = t->b60 = t->b64 = t->b6c = 0;   /* 4704A0 */
    t->pt_marca = 0; t->b72 = 0;
    t->c.x = carril_x(s, t->carril); coche_coloca(&t->c);
    traf_pose_ant(t);
}
/* Aparicion y cierre del trafico arcade del original (479080: 477F60 cierra, 478B90 / 4787E0 abren; PS2 0x1dcdd0):
   - cierre (477F60): un coche de trafico (sin +4 0x20) se cierra si su distancia de carretera al jugador (46F990, en
     posiciones) es < -2 o > 60 (0x3C);
   - registro de aparicion (4EFB90) del sitio del jugador: en la etapa, el de su tabla cuyo tramo contiene la posicion
     del jugador; en la union, el primero de la etapa siguiente si el jugador pasa de su posicion 120 (0x78) y la
     etapa siguiente esta cargada (44BE30), si no el de la union por defecto (2 coches, separacion 20, carriles 2 y 3
     prohibidos);
   - puntos fijos (478B90), antes que los al azar: un coche cuando el jugador esta justo en la posicion de un punto de
     la lista de su etapa aun no usado (modelo, carril y color del punto);
   - al azar (4787E0): si el jugador ha avanzado al menos max(3, registro +5) posiciones desde la ultima aparicion
     (800D50, a 0 al cambiar de etapa a union o al reves: 46C950) y hay menos coches que [registro +4] (densidad
     46CD60 = 1 fuera de la red; tope 20); a 50 posiciones (0x32) por delante del jugador (35 si el jugador esta en la
     union entre su posicion 120 y el final - 15); carril de 478430 (el menos cargado por los pesos de clase, sin los
     prohibidos; a igualdad, el de menos aperturas que no este junto al de mas); modelo de la lista de 7 de 46CE30
     en orden; color, el siguiente del modelo; velocidad, la del registro de velocidad del sitio (4F0030 +0xC).
   No hay coches al empezar salvo los de los puntos fijos. SIMPLIFICADO: 46CDA0 (carriles sin datos) y 478640 en la
   union (el carril va por un contador de 12) no se portan: en la union tambien 478430; los puntos fijos de coches
   de carreras (modelos 0..29, funcion 0x55) y de modelos que la demo no tiene (39..43: camiones y autobuses) cuentan
   como abiertos (800D50, usado) sin coche; 43EB60 (sitio ocupado) es la prueba de 25 m de carril_libre. */
#define TR_CIERRA_ATRAS 2
#define TR_CIERRA_DELANTE 60
#define TR_APARECE 50
#define TR_APARECE_UNION 35
#define TR_APARECE_CADA 3                                       /* separacion minima (4787E0) */
#define TR_KMH (0.00461420696f * 60.0f)                         /* km/h del modelo -> m/s de la demo (K5B43A4 x 60) */
static float s_union, s_sig, s_etapa;                           /* del curso (definidos mas abajo) */
static int cur_nodo, cur_sig, cur_en_union;
static int traf_ult_pos, traf_lugar_ult = -1, traf_mcnt, traf_hist[NCARRIL];   /* 800D50, +0x184, 800D3C+4, 803718 */
static int traf_lejos_v = 10;                                   /* rec2 +4 del coche (4F0030: 7 con 10 o mas coches) */
/* el sitio de carretera de s: 1 si es la union; nodo del curso y posicion (car+0x64) */
static int traf_lugar(float s, int *nodo, int *pos)
{
    if (s_sig >= 0 && s >= s_sig && cur_sig >= 0) { *nodo = cur_sig; *pos = (int)floorf((s - s_sig) / m_pos); return 0; }
    *nodo = cur_nodo;
    if (s_union >= 0 && s >= s_union) { *pos = (int)floorf((s - s_union) / m_pos); return 1; }
    *pos = (int)floorf((s - s_etapa) / m_pos); return 0;
}
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
/* 4EFB90: registro de aparicion del sitio */
static const uint8_t *traf_reg_aparicion(int en_union, int nodo, int pos)
{
    static const uint8_t *ult; int k; const uint8_t *ap;
    if (nodo < 0 || nodo >= ntap) return tap_defecto;
    if (en_union) return pos >= 0x78 && s_sig >= 0 && cur_sig >= 0 && cur_sig < ntap ? tap[cur_sig].ap : tap_defecto;
    ap = tap[nodo].ap;
    for (k = 0; k < 7; k++) {
        if (rd16(ap + 12 * k) == 0xFFFF) break;
        if (rd16(ap + 12 * k) <= pos && rd16(ap + 12 * k + 12) > pos) return ult = ap + 12 * k;
    }
    return ult ? ult : ap;
}
/* 4F0030: registro de velocidad del sitio (solo se usa su +0xC: el resto lo pisan constantes salvo +2/+3/+8/+0xA) */
static const uint8_t *traf_reg_velocidad(int en_union, int nodo, int pos)
{
    static const uint8_t *ult; int k; const uint8_t *ve;
    if (nodo < 0 || nodo >= ntap) return NULL;
    ve = tap[nodo].ve;
    if (en_union) {
        if (pos >= 0x78 && s_sig >= 0 && cur_sig >= 0 && cur_sig < ntap) return ult = tap[cur_sig].ve;
        for (k = 0; k < 5; k++) if (rd16(ve + k * 0x46) == 0xFFFF) return ult = ve + 4 * 0x46;
        return ult;
    }
    ult = ve;
    for (k = 0; k < 5; k++) {
        const uint8_t *r = ve + k * 0x46;
        if (rd16(r) == 0xFFFF) break;
        if (rd16(r) > pos) continue;
        if (k + 1 < 5 && rd16(r + 0x46) > pos) { ult = r; break; }
    }
    return ult;
}
/* 4F0030 +4 del coche que va en s: 7 si el registro de aparicion de su sitio deja 10 coches o mas, si no 10 */
static int traf_lejos(float s)
{
    int nodo, pos, u = traf_lugar(s, &nodo, &pos); const uint8_t *r;
    if (!ntap) return 10;
    r = traf_reg_aparicion(u, nodo, pos);
    return (int8_t)r[4] >= 10 ? 7 : 10;
}
/* 46C950: cuentas a cero (al empezar y al pasar de etapa a union o al reves) */
static void traf_cuentas_cero(void)
{
    int k;
    traf_ult_pos = 0; traf_mcnt = 0;
    memset(traf_cuenta, 0, sizeof traf_cuenta);
    for (k = 0; k < NCARRIL; k++) traf_hist[k] = 0;
}
/* 478430: carril del coche nuevo (con los 6 carriles de la demo: un grupo por carril) */
static int traf_carril_478430(const uint8_t *rec)
{
    int c[NCARRIL], k, i, mn = 1 << 30, most = -1, top = 0, b = -1, low = 1 << 30;
    for (k = 0; k < NCARRIL; k++) c[k] = 0;
    for (i = 0; i < NTRAF; i++) {                               /* 46CC10: los que no estan en la union, por clase */
        int nd, ps, cl;
        if (traf[i].carril < 0 || traf_lugar(traf[i].c.s, &nd, &ps)) continue;
        cl = trc_ok ? trc[traf[i].modelo].clase : 3; if (cl < 0 || cl > 6) cl = 3;
        c[traf[i].carril] += tap_peso[cl];
    }
    for (k = 0; k < NCARRIL; k++) if (rec[11] >> k & 1) c[k] = 0x2A;   /* prohibido */
    for (k = 0; k < NCARRIL; k++) if (c[k] < mn) mn = c[k];
    for (k = 0; k < NCARRIL; k++) if (c[k] == mn && traf_hist[k] > top) { most = k; top = traf_hist[k]; }
    for (k = 0; k < NCARRIL; k++) if (c[k] == mn && traf_hist[k] < low && (most - k > 1 || k - most > 1)) { b = k; low = traf_hist[k]; }
    if (b >= 0) return b;
    for (k = 0; k < NCARRIL; k++) if (c[k] == mn && traf_hist[k] < low) { b = k; low = traf_hist[k]; }
    return b;
}
static int traf_modelo_de(int id)                               /* modelo del juego -> TRAFn.DAT (-1: no esta) */
{
    int m;
    for (m = 0; m < NMODTRAF && trc_ok; m++) if (trc[m].modelo == id) return m;
    return -1;
}
/* final comun de 478B90 / 4787E0 (abre el coche en s) */
static void traf_abre(trafico_t *t, float s, int carril, int m, int color, int pos_jug)
{
    int nd, ps, u = traf_lugar(s, &nd, &ps); const uint8_t *rv = traf_reg_velocidad(u, nd, ps);
    t->modelo = m; t->color = color < 0 || color >= traf_ncol[m] ? 0 : color;
    t->carril = t->destino = carril;
    t->v_des = t->c.v = (rv ? (int16_t)rd16(rv + 0xC) : 100) * TR_KMH;
    traf_nace(t, s);
    if (!u) traf_hist[carril]++;                                /* 803718: aperturas por carril fuera de la union */
    traf_ult_pos = pos_jug;
#ifdef DIAG_TRAF
    dbglog(DBG_INFO, "DIAGTRAF abre s %.0f nodo %d pos %d union %d carril %d modelo %d color %d v %.1f\n", (double)s, nd, ps, u, carril,
           trc_ok ? trc[m].modelo : m, t->color, (double)t->c.v);
#endif
}
/* 478B90: el punto fijo de la posicion del jugador; 1 si ha abierto (o gastado) uno */
static int traf_fijo(int en_union, int nodo, int pos)
{
    int k, m; const tap_fijo_t *f; float s;
    if (en_union || nodo < 0 || nodo >= ntap) return 0;
    for (k = 0; k < (int)tap[nodo].nfijo && k < TAP_NFIJO; k++) {
        f = tap[nodo].fijo + k;
        if (!tap_usado[nodo][k] && f->tipo == 0 && f->pos == pos) break;
    }
    if (k >= (int)tap[nodo].nfijo || k >= TAP_NFIJO) return 0;
    s = yo.s + TR_APARECE * m_pos;
    if (s >= longitud - 20.0f) return 0;
    m = f->modelo >= 0x1e ? traf_modelo_de(f->modelo) : -1;
    if (m >= 0) {
        int i, l = f->carril < 0 ? 0 : f->carril >= NCARRIL ? NCARRIL - 1 : f->carril;
        for (i = 0; i < NTRAF && traf[i].carril >= 0; i++) ;
        if (i >= NTRAF) return 0;                               /* sin suceso libre */
        if (!carril_libre(NULL, l, s, 25.0f, 25.0f)) return 0;  /* 43EB60 */
        traf_abre(traf + i, s, l, m, f->color, pos);
    } else traf_ult_pos = pos;                                  /* coche de carreras o modelo que no hay: gastado */
    tap_usado[nodo][k] = 1;
    return 1;
}
/* 4787E0: un coche al azar delante del jugador */
static void traf_azar(int en_union, int nodo, int pos, const uint8_t *rec)
{
    int i, n = 0, want, gap = (int8_t)rec[5], l, m, fin; float s;
    if (gap < TR_APARECE_CADA) gap = TR_APARECE_CADA;
    if (pos - traf_ult_pos < gap) return;
    for (i = 0; i < NTRAF; i++) if (traf[i].carril >= 0) n++;   /* 46CBA0: 800D52 */
    if (n >= 0x14) return;
    want = (int8_t)rec[4]; if (want < 0) want = 0; if (want > 0x14) want = 0x14;
    if (n >= want) return;
    for (i = 0; i < NTRAF && traf[i].carril >= 0; i++) ;
    if (i >= NTRAF) return;
    fin = (int)(((s_sig >= 0 ? s_sig : longitud) - s_union) / m_pos);
    s = yo.s + (en_union && pos >= 0x78 && pos <= fin - 0xF ? TR_APARECE_UNION : TR_APARECE) * m_pos;
    if (s >= longitud - 20.0f) return;
    if ((l = traf_carril_478430(rec)) < 0) return;
    if (!carril_libre(NULL, l, s, 25.0f, 25.0f)) return;        /* 43EB60 */
    traf_mcnt = (traf_mcnt + 1) % 7;                            /* 46CE30: clase 4, la lista de 7 en orden */
    if ((m = traf_modelo_de(tap_modelos[traf_mcnt])) < 0) m = 0;
    {
        int c = traf_cuenta[m] >= traf_ncol[m] ? 0 : traf_cuenta[m];   /* el siguiente color del modelo (804358) */
        traf_cuenta[m] = c + 1;
        traf_abre(traf + i, s, l, m, c, pos);
    }
    (void)nodo;
}
static void traf_aparca(trafico_t *t)
{
    t->carril = t->destino = -1; t->c.s = -1.0e6f; t->c.v = 0; t->golpe = -1; t->b72 = 0; t->b6c = 0; t->b14 = 0;
}
static float traf_ult_s;                                        /* sin tablas: s del jugador en la ultima aparicion */
static void traf_inicia(float s0)
{
    int i;
    for (i = 0; i < NTRAF; i++) traf_aparca(traf + i);
    traf_ult_s = s0 - TR_APARECE_CADA * m_pos;
    traf_cuentas_cero(); traf_lugar_ult = -1; memset(tap_usado, 0, sizeof tap_usado);
    cambio_cd = 0;
#ifdef PRUEBA_TRAFICO
    for (i = 0; i < 3; i++) { traf_nace(traf + i, s0 + 25.0f + i * 18.0f); traf[i].carril = traf[i].destino = 2 + i; traf[i].modelo = i * 4;
        traf[i].c.v = traf[i].v_des = 5.0f; traf[i].c.x = carril_x(traf[i].c.s, traf[i].carril); coche_coloca(&traf[i].c); }
#endif
}
/* 479080: aparicion de este cuadro */
static void traf_aparece(void)
{
    int nodo, pos, u;
    if (!ntap) {                                                /* sin tablas: la aparicion de antes (separacion 3) */
        int i;
        if (yo.s - traf_ult_s >= TR_APARECE_CADA * m_pos && yo.s + TR_APARECE * m_pos < longitud - 20.0f) {
            for (i = 0; i < NTRAF && traf[i].carril >= 0; i++) ;
            if (i < NTRAF) { traf_nace(traf + i, yo.s + TR_APARECE * m_pos); traf_ult_s = yo.s; }
        }
        return;
    }
    u = traf_lugar(yo.s, &nodo, &pos);
    if (traf_lugar_ult != u) { traf_cuentas_cero(); traf_lugar_ult = u; }   /* 46C950 (+0x184 != +0x5C) */
    if (!traf_fijo(u, nodo, pos)) traf_azar(u, nodo, pos, traf_reg_aparicion(u, nodo, pos));
}
/* --- Choques entre coches del original (4F9450: una pasada por cuadro despues de mover el trafico; la geometria y la
   logica estan en comun/or2_fisica.c, or2_contacto / or2_choque_coche / or2_choque_trafico):
   - cajas y radios del descriptor de 520950 y del registro de 4866C0 de cada modelo (TRC1 y CJM1 de TRAFC.DAT); las
     esquinas de cada caja desde su sitio del cuadro anterior al de ahora contra los bordes de la otra;
   - separacion (4F8490) con el reparto de la tabla por clases (el jugador es la clase 5: contra los coches de calle se
     la lleva entera); en el trafico solo la parte a lo largo de la carretera: su IA (47BC00 / 475040) lo lleva cada
     cuadro al punto de su carril + B08, asi que el empuje de lado no dura;
   - el original solo hace reaccionar e intercambiar velocidad al ultimo coche tocado de cada uno (la separacion es con
     todos); los golpeados (+B64 > 0, 120 cuadros) no entran en la pasada: ni con el jugador ni con el resto del trafico. */
static float yo_wa[3]; static int yo_wa_ok;                     /* pose del jugador en el cuadro anterior */
#ifdef DIAG_CHOQUE
static int diag_cuadro;                                         /* prueba: cuadros de la pasada de choques */
#endif
static void yo_pose_ant(void)
{
    float th; mundo_de(yo.s, yo.x, &yo_wa[0], &yo_wa[1], &th); yo_wa[2] = th + yo.a + yo.deriva; yo_wa_ok = 1;
}
/* la forma de un coche relativa al punto (ox, oy); sin pose anterior cercana (aparicion, grua) la de ahora */
static void forma_de(float s, float x, float rumbo_rel, const float *wa, const trafc_t *m, float pose_y, float ox, float oy,
                     or2_forma_t *f)
{
    float th;
    mundo_de(s, x, &f->x, &f->y, &th);
    f->rumbo = th + rumbo_rel;
    f->xa = wa[0]; f->ya = wa[1]; f->rumbo_a = wa[2];
    if (fabsf(f->xa - f->x) + fabsf(f->ya - f->y) > 10.0f) { f->xa = f->x; f->ya = f->y; f->rumbo_a = f->rumbo; }
    f->x -= ox; f->y -= oy; f->xa -= ox; f->ya -= oy;
    memcpy(f->caja, m->caja, sizeof f->caja); f->radio = m->radio; f->pose_y = pose_y;
}
static void forma_traf(const trafico_t *t, float ox, float oy, or2_forma_t *f)
{
    forma_de(t->c.s, t->c.x, t->c.a, t->wa, trc + t->modelo, t->c.pose_t[1], ox, oy, f);
}
/* el otro coche para or2_choque_coche / or2_choque_trafico (velocidad: +1C4 en su rumbo) */
static void otro_de(const trafico_t *t, int id, float ox, float oy, const or2_contacto_t *k, or2_otro_t *o)
{
    float th, wx, wy;
    memset(o, 0, sizeof *o);
    mundo_de(t->c.s, t->c.x, &wx, &wy, &th);
    o->dx = wx - ox; o->dy = wy - oy;
    o->vx = sinf(th + t->c.a) * t->c.v; o->vy = cosf(th + t->c.a) * t->c.v;
    o->clase = trc[t->modelo].clase; o->k320 = trc[t->modelo].k320; o->modelo = trc[t->modelo].modelo;
    o->se = trc[t->modelo].se; o->id = id; o->d22 = t->d22; o->b58 = t->b58; o->k = k;
    o->t_choque = t->golpe >= 0 ? fmaxf(choq.dur[t->golpe] - t->t_golpe, 0.0f) : 0.0f;
    o->t_recup = t->t_recup;
}
/* los efectos de un golpe (424940 con el id de 9 bits; las banderas 0x800 son del controlador del original) */
static void suena_golpe(const or2_otro_t *o)
{
    int i;
    if (!sfx_ok) return;
    for (i = 0; i < o->se_n; i++) sfx_menu(o->se_id[i] & 0x1ff);
}
/* 476040 en el coche golpeado: deriva hacia el lado contrario al del jugador en la carretera (+B58 0xB si +26C del
   jugador > 0), 120 cuadros de recuperacion (+B60) y otros tantos fuera de la pasada de choques (+B64), la velocidad a
   1,5 veces la del jugador (5B005C; la clase 6 no) y el estado de choque k con la animacion sin reflejar (4A6EA0 con
   lado 0) y su efecto (0x1ec7xx: 16 -> 0x4F, 17 -> 0x5E, 18 -> 0x864, 19 -> 0x65) */
static void traf_golpeado(trafico_t *t, int k, float v_jug)
{
    t->b58 = fis.ofs > 0.0f ? 0xB : 0xC;
    t->b6c = 1;
    if (trc[t->modelo].clase != 6) { t->c.v = v_jug * 1.5f; t->b60 = 120; }
    t->golpe = k; t->t_golpe = 0; t->lado = 0; t->t_recup = 0;
    t->b64 = 120;
    if (sfx_ok) sfx_menu(k == 16 ? 0x4f : k == 17 ? 0x5e : k == 18 ? 0x64 : 0x65);
}
/* reaccion e intercambio del jugador con el ultimo coche tocado (4F6FE0, 4F76E0, 4F7BF0) */
static void traf_choca(trafico_t *t, int id, const or2_contacto_t *k, float ox, float oy, float th)
{
    or2_otro_t o; float v_jug = sqrtf(fis.vx * fis.vx + fis.vy * fis.vy);
#ifdef DIAG_CHOQUE
    float v0 = v_jug;
#endif
    otro_de(t, id, ox, oy, k, &o);
    or2_choque_coche(&fis, &fpar, &choq, &o);
    t->d22 = o.d22;
    if (o.golpe >= 0) traf_golpeado(t, o.golpe, v_jug);
    if (o.cambia_vel) t->c.v = sqrtf(o.vx * o.vx + o.vy * o.vy);   /* +1C4 = |+20| (su IA lo vuelve a dirigir) */
    /* 4F2860: tocado el ultimo coche (+4 0x10000 y +DD4): sin puntos si el jugador ha cambiado de velocidad (+4
       0x4000000), si no a la mitad; la voz de choque de voces.c lee el mismo bit */
    if (!(t->pt_marca & PT_PASADO)) t->pt_marca |= o.impulso ? PT_SIN : PT_MITAD;
    if (o.impulso) voz_contacto = 1;
    pt_toca |= o.golpe >= 0 ? 3 : 1;
    if (o.impulso) {
        float h = atan2f(fis.vx, fis.vy);
        yo.v = v_jug = sqrtf(fis.vx * fis.vx + fis.vy * fis.vy);
        yo.a = dif_ang(h - th); yo.deriva = dif_ang(-fis.psi - h);
    }
    yo.choque = 2;
    suena_golpe(&o);
#if defined(PRUEBA_EMBISTE) && defined(PRUEBA_GUION) && defined(DIAG_CHOQUE)
    if ((o.golpe >= 0 || o.reaccion) && emb_golpe < 0) emb_golpe = diag_cuadro;
#endif
#ifdef DIAG_CHOQUE
    dbglog(DBG_INFO, "DIAGCHOQUE t %d modelo %d bordes %d%d%d%d/%d%d%d%d jugador %.0f -> %.0f km/h estado %d lado %d otro -> %.0f "
           "golpe %d roce %d impulso %d se %d %x %x %x b58 %x\n", diag_cuadro, trc[t->modelo].modelo, k->borde_a[0], k->borde_a[1],
           k->borde_a[2], k->borde_a[3], k->borde_b[0], k->borde_b[1], k->borde_b[2], k->borde_b[3],
           (double)(v0 * 3.6f * fpar.hud), (double)(yo.v * 3.6f * fpar.hud), fis.t_choque > 0 ? fis.choque : -1, fis.lado,
           (double)(t->c.v * 3.6f * fpar.hud), o.golpe, o.roce, o.impulso, o.se_n, o.se_id[0], o.se_id[1], o.se_id[2], t->b58);
#endif
}
static void traf_choques(void)
{
    int i, j, ult; or2_forma_t fa, fb; or2_contacto_t k, k_ult; float ox, oy, th, f[2], r[2];
#ifdef DIAG_CHOQUE
    diag_cuadro++;
#endif
    if (!fis_ok || !choq.ok || !trc_ok || !cjm_ok) return;      /* sin las cajas del original: sin choques */
    if (fpar.modelo >= 0 && fpar.modelo < 30) {                 /* el jugador contra el trafico */
        mundo_de(yo.s, yo.x, &ox, &oy, &th);
        f[0] = sinf(th); f[1] = cosf(th); r[0] = cosf(th); r[1] = -sinf(th);
        if (!yo_wa_ok) yo_pose_ant();
        forma_de(yo.s, yo.x, yo.a + yo.deriva, yo_wa, cjm + fpar.modelo, yo.pose_t[1], ox, oy, &fa);
        ult = -1;
        for (i = 0; i < NTRAF; i++) {
            trafico_t *t = traf + i; float rp, sl;
            if (t->carril < 0 || t->b64 > 0 || fabsf(t->c.s - yo.s) > 30.0f) continue;
            forma_traf(t, ox, oy, &fb);
            if (!or2_contacto(&fa, &fb, &k)) continue;
            rp = or2_reparto(&choq, 5, trc[t->modelo].clase, sqrtf(fis.vx * fis.vx + fis.vy * fis.vy), t->c.v);
            sl = k.sep[0] * f[0] + k.sep[1] * f[1];
            yo.s -= rp * sl; yo.x -= rp * (k.sep[0] * r[0] + k.sep[1] * r[1]);
            fa.x -= rp * k.sep[0]; fa.y -= rp * k.sep[1];
            t->c.s += (1.0f - rp) * sl; coche_coloca(&t->c);
            k_ult = k; ult = i;
        }
        if (ult >= 0) { traf_choca(traf + ult, ult, &k_ult, ox, oy, th); coche_coloca(&yo); }
    }
    for (i = 0; i < NTRAF; i++) {                               /* el trafico entre si */
        trafico_t *a = traf + i; or2_otro_t oa, ob;
        if (a->carril < 0 || a->b64 > 0) continue;
        mundo_de(a->c.s, a->c.x, &ox, &oy, &th);
        f[0] = sinf(th); f[1] = cosf(th);
        forma_traf(a, ox, oy, &fa);
        ult = -1;
        for (j = i + 1; j < NTRAF; j++) {
            trafico_t *b = traf + j; float rp, sl;
            if (b->carril < 0 || b->b64 > 0 || fabsf(b->c.s - a->c.s) > 30.0f) continue;
            forma_traf(b, ox, oy, &fb);
            if (!or2_contacto(&fa, &fb, &k)) continue;
            rp = or2_reparto(&choq, trc[a->modelo].clase, trc[b->modelo].clase, a->c.v, b->c.v);
            sl = k.sep[0] * f[0] + k.sep[1] * f[1];
            a->c.s -= rp * sl; b->c.s += (1.0f - rp) * sl;
            fa.y -= rp * sl * f[1]; fa.x -= rp * sl * f[0];
            coche_coloca(&a->c); coche_coloca(&b->c);
            k_ult = k; ult = j;
        }
        if (ult < 0) continue;
        otro_de(a, i, ox, oy, &k_ult, &oa); otro_de(traf + ult, ult, ox, oy, &k_ult, &ob);
        or2_choque_trafico(&choq, &oa, &ob);
        a->d22 = oa.d22; traf[ult].d22 = ob.d22;
#ifdef DIAG_CHOQUE
        dbglog(DBG_INFO, "DIAGCHOQUE trafico t %d %d-%d bordes %d%d%d%d/%d%d%d%d %.0f/%.0f -> %.0f/%.0f roce %d/%d\n", diag_cuadro, i, ult,
               k_ult.borde_a[0], k_ult.borde_a[1], k_ult.borde_a[2], k_ult.borde_a[3], k_ult.borde_b[0], k_ult.borde_b[1],
               k_ult.borde_b[2], k_ult.borde_b[3], (double)(a->c.v * 3.6f * fpar.hud), (double)(traf[ult].c.v * 3.6f * fpar.hud),
               (double)(oa.cambia_vel ? sqrtf(oa.vx * oa.vx + oa.vy * oa.vy) * 3.6f * fpar.hud : a->c.v * 3.6f * fpar.hud),
               (double)(ob.cambia_vel ? sqrtf(ob.vx * ob.vx + ob.vy * ob.vy) * 3.6f * fpar.hud : traf[ult].c.v * 3.6f * fpar.hud),
               oa.roce, ob.roce);
#endif
        if (oa.cambia_vel) a->c.v = sqrtf(oa.vx * oa.vx + oa.vy * oa.vy);
        if (ob.cambia_vel) traf[ult].c.v = sqrtf(ob.vx * ob.vx + ob.vy * ob.vy);
        /* 4F67A0: el efecto solo si el coche esta a menos de 100 m por delante de la camara (+300 >= -100; la
           profundidad se toma desde el jugador) */
        if (a->c.s - yo.s <= 100.0f) suena_golpe(&oa);
        if (traf[ult].c.s - yo.s <= 100.0f) suena_golpe(&ob);
    }
    yo_pose_ant();
}

/* --- IA del trafico del original (47E4A0 drive -> 472450 velocidad, 473F30 recuperacion, 47E110 carriles).
   Registro de velocidad del sitio (0x46 bytes, 4F0030; TAP1 de TRAFC.DAT): en el curso, seguimiento +8 = 50 m, +6 = 1
   y +4 = 7 o 10 posiciones (traf_lejos); 4F0030 pisa con constantes las velocidades por clase (+0xE.., +0x1C..) y los
   porcentajes (+0x2A..); la del coche nuevo (+0xC, 100 km/h del modelo en todo el curso) es v_des y el objetivo del
   camino "Plain" de 472450 (el coche de detras y el siguiente al jugador). PENDIENTE: el camino "Lead" (los demas por
   delante: 470B30, 471E70, 471F90, 472320, que van segun la velocidad del jugador) no se porta: todos van a v_des.
   Unidades del original: m/cuadro (60 por segundo). */
#define TR_HUECO 50.0f                                          /* rec2+8: distancia de seguimiento (m) */
#define TR_LEJOS traf_lejos_v                                   /* rec2+4 (posiciones): 4F0030, 7 o 10 */
#define TR_CERCA 1                                              /* rec2+6 */
#define TR_POS   m_pos                                          /* m por posicion de la carretera (car+0x64) */
#define TR_CAMBIO 0xB4                                          /* 470A30 / 470AB0: el cambio de carril dura 180 cuadros */
/* 472090: lleva la velocidad hacia el objetivo con la tabla de la clase; f0/f1 los dos indicadores del objetivo */
static void traf_acelera(trafico_t *t, float rate, float obj, int f0, int f1)
{
    const float *tb = acl[trc[t->modelo].clase < 0 ? 0 : (trc[t->modelo].clase > 6 ? 6 : trc[t->modelo].clase)];
    float s = t->c.v / 60.0f, o = obj / 60.0f;
    if (!trc_ok) {                                              /* sin TRAFC.DAT: aceleracion y frenada de la demo */
        t->c.v += fmaxf(-6.0f / 60.0f, fminf(2.5f / 60.0f, obj - t->c.v)); return;
    }
    if (o > s * 1.2f || (o > s && f0 == 1)) { s += tb[3] * rate; if (s > o) s = o; }      /* 5B43B4 = 1,2 */
    else if (o > s) { s += tb[1] * rate; if (s > o) s = o; }
    else {
        int fuerte = (s * 0.8f > o && !f1) || f0 == 1;          /* 62821C = 0,8 */
        s -= tb[fuerte ? 4 : 2] * rate; if (s < o) s = o;
    }
    t->c.v = s * 60.0f;
}
/* coches por carril (800D2C) y 4734C0: -1 si los carriles a la derecha de k llevan mas trafico que los de la izquierda */
static int traf_lado_libre(int k)
{
    int n[NCARRIL] = { 0 }, i; float a = 0, b = 0;
    for (i = 0; i < NTRAF; i++) if (traf[i].carril >= 0) n[traf[i].carril]++;
    for (i = k; i < NCARRIL; i++) a += n[i];
    for (i = 0; i <= k; i++) b += n[i];
    return a / (NCARRIL - k) > b / (k + 1) ? -1 : 1;
}
/* 471010: el carril k es seguro (el jugador por delante en ese carril, o uno por detras al que adelanto, a menos de la
   distancia de seguimiento y a mas de 4 m: no) */
static int traf_seguro(const trafico_t *t, int k)
{
    int i; float g = 1000.0f, d, py = yo_pos(t->c.s), pt = traf_pos(t);
    d = yo.s - t->c.s;
    if (d > 0 && fabsf(k - py) <= 1.0f && fabsf(pt - py) > fabsf(k - py) && yo.v > t->c.v) g = d;
    for (i = 0; i < NTRAF; i++) {
        const trafico_t *o = traf + i;
        if (o == t || (int)(fabsf(k - traf_pos(o))) != 0) continue;
        d = t->c.s - o->c.s;
        if (d <= 0 || d > TR_HUECO) continue;
        if (g > d && t->c.v > o->c.v) g = d;
    }
    return !(g > 4.0f && TR_HUECO > g);
}
/* 471380: 0 si el jugador tapa el cambio al carril k (le tengo delante a 4..50 m, al lado del carril destino) */
static int traf_jugador_deja(const trafico_t *t, int k)
{
    float py = yo_pos(t->c.s), d = yo.s - t->c.s;
    if ((int)fabsf(k - py) > 1) return 1;
    if (!(fabsf(traf_pos(t) - py) > fabsf(k - py))) return 1;
    return !(d > 4.0f && d < TR_HUECO);
}
/* 4736D0: el cambio de carril en curso (b08 hacia 4,5 m con t^2; al acabar, pasa al carril nuevo) */
static void traf_cambio(trafico_t *t)
{
    float f = (float)(t->b18 - t->b14) / (float)t->b18;
    if (!traf_jugador_deja(t, t->destino)) return;              /* espera (+B14 + 1) */
    t->b14--;
    t->b08 = (1.0f - f) * t->b08 + t->dir * 4.5f * f * f;       /* 5B43CC = 4,5 */
    if (t->b14 > 0) return;
    t->carril = t->destino; t->b14 = 0;
    t->b08 -= t->dir * 4.0f;                                    /* lo que queda respecto al carril nuevo (473620) */
    if (t->b08 > 4.0f) t->b08 = 4.0f;
    if (t->b08 < -4.0f) t->b08 = -4.0f;
}
static void traf_empieza_cambio(trafico_t *t, int k)
{
    t->destino = k; t->dir = k > t->carril ? 1 : -1; t->b14 = t->b18 = TR_CAMBIO; cambio_cd = 0;
}
/* 47DC10: decide un cambio de carril (clases 3 y 4, mas rapido que el de delante, uno por segundo en total); delante y
   detras: los vecinos de la lista ordenada por distancia (+318 / +314), de cualquier carril */
static void traf_decide(trafico_t *t, const trafico_t *delante, int jugador_delante, const trafico_t *detras)
{
    int cl = t->carril, lo = t->carril < 3 ? 0 : 3, hi = lo + 2, cls = trc[t->modelo].clase;
    float e18 = 1000.0f;
    if ((delante || jugador_delante) && (jugador_delante || t->c.v > delante->c.v) && (cls == 3 || cls == 4)
        && cambio_cd > 60) {
        int r = traf_lado_libre(t->carril);
        if (r > 0) { if (t->carril < hi) cl = t->carril + 1; }
        else if (t->carril > lo) cl = t->carril - 1;
    }
    if (cl == t->carril) return;
    if (!traf_seguro(t, cl)) return;
    if (detras && (int)fabsf(cl - traf_pos(detras)) == 0 && t->c.v > detras->c.v) e18 = t->c.s - detras->c.s;
    if (!(e18 > TR_HUECO)) return;
    traf_empieza_cambio(t, cl);
}
/* 473B90: se aparta del vecino (delante o detras, a menos de 4 m a lo largo y 2 m de lado) 0,05 m por cuadro */
static int traf_aparta(trafico_t *t)
{
    int i; const trafico_t *c = NULL; float dm = 100.0f;
    for (i = 0; i < NTRAF; i++) {
        const trafico_t *o = traf + i; float d = fabsf(o->c.s - t->c.s);
        if (o == t || d >= dm) continue;
        dm = d; c = o;
    }
    if (!c || !(4.0f > dm) || !(2.0f > fabsf(traf_pos(t) - traf_pos(c)) * 4.0f)) return 0;
    if (traf_pos(t) > traf_pos(c)) { if (1.0f > t->b08) t->b08 += 0.05f; }
    else if (t->b08 > -1.0f) t->b08 -= 0.05f;
    return 1;
}
/* +268 / +264 del coche (4A4010): distancia al borde izquierdo (> 0 dentro) y al derecho (< 0 dentro); en la union
   los de su calzada (carriles 0..2 o 3..5), en la etapa los de la carretera */
static void traf_bordes(const trafico_t *t, float *b268, float *b264)
{
    int nodo, pos; float p[3], th, iz, de;
    if (traf_lugar(t->c.s, &nodo, &pos)) {
        int lo = t->carril < 3 ? 0 : 3; float w = fabsf(carril_paso(t->c.s, lo)) * 0.5f;
        float x0 = carril_x(t->c.s, lo), x2 = carril_x(t->c.s, lo + 2);
        *b268 = t->c.x - (fminf(x0, x2) - w); *b264 = t->c.x - (fmaxf(x0, x2) + w);
        return;
    }
    ruta_en(t->c.s, p, &th, &iz, &de);
    *b268 = t->c.x + iz; *b264 = t->c.x - de;
}
/* 473F30 (PS2 0x1e7ef0): tras el golpe, 120 cuadros derivando 0,2 m del original por cuadro hacia +B58 (0xB a la
   izquierda mientras +268 > 0 y B08 > -12; 0xC a la derecha mientras +264 < 0 y B08 < 12); al acabar, los pesados
   (clases 0..2) pasan al carril que les toca por B08 (+66 += B08 / 4, dentro de su calzada) con el desplazamiento que
   les queda (473620); los coches de calle con un jugador (800D4D <= 1) siguen en su carril con su B08 (lo devuelven
   473D10 y los cambios de carril) */
static void traf_recupera(trafico_t *t)
{
    float p = traf_pos(t), b268, b264; int lo = t->carril < 3 ? 0 : 3;
    traf_bordes(t, &b268, &b264);
    if (t->b58 == 0xB) { if (b268 > 0.0f && t->b08 > -12.0f) t->b08 -= 0.2f; }
    else if (t->b58 == 0xC) { if (0.0f > b264 && 12.0f > t->b08) t->b08 += 0.2f; }
    if (--t->b60 > 0) return;
    t->b60 = 0; t->b58 = 0; t->b6c = 0;
    if (trc[t->modelo].clase > 2) return;
    {
        int k = t->carril + (int)(t->b08 * 0.25f);
        if (k < lo) k = lo;
        if (k > lo + 2) k = lo + 2;
        t->b08 = (p - k) * 4.0f; t->carril = t->destino = k;
    }
}
/* 473D10 (PS2 0x1e77e0): con el jugador cerca (el coche a menos de rec2+4 posiciones por delante de el y el jugador a
   menos de 50 + 200 m por delante del coche) se aparta de su lado 0,05 por cuadro hasta el empuje
   (200 - d) / 200 x min(1, separacion lateral en m del original), o B08 x 0,995; cerca del final de la etapa o del
   principio de la union, solo x 0,995 */
static void traf_esquiva(trafico_t *t)
{
    float d, push, ap, b, pt = traf_pos(t) * 4.0f, pj = yo_pos(t->c.s) * 4.0f;
    int lo = t->carril < 3 ? 0 : 3, hi = lo + 2, nodo, pos, u, lento = 0, mas = 0;
    if (!((int)floorf((t->c.s - yo.s) / TR_POS) < TR_LEJOS)) return;
    d = (yo.s - t->c.s) - TR_HUECO;
    if (d < 0.0f) d = 0.0f;
    else if (d > 200.0f) return;
    ap = fabsf(pt - pj); if (ap > 1.0f) ap = 1.0f;
    push = (200.0f - d) * 0.005f * ap;
    u = traf_lugar(t->c.s, &nodo, &pos);
    if (u) lento = !(pos > 15);
    else if (s_union >= 0 && pos >= (int)((s_union - s_etapa) / m_pos) - 15) lento = 1;
    if (lento) { t->b08 *= 0.995f; return; }
    if (pt > pj) {                                              /* el coche a la derecha del jugador */
        b = t->b08;
        if (b > -0.05f) { if (t->carril < hi && push > b) mas = 1; else if (push * 0.5f > b) mas = 1; }
        t->b08 = mas ? b + 0.05f : b * 0.995f;
    }
    if (!(pj > pt)) return;
    b = t->b08; mas = 0;
    if (0.05f > b) { if (t->carril > lo && b > -push) mas = 1; else if (b > push * -0.5f) mas = 1; }
    t->b08 = mas ? b - 0.05f : b * 0.995f;
}
#ifdef DIAG_TRAF
static int traf_vistos, traf_vistos_max, traf_act_max, diag_n; static float traf_vistos_sum, traf_act_sum;
static void traf_diag(void)                                     /* prueba: coches en la ruta y dibujados, cada 1 s */
{
    int i, n = 0;
    for (i = 0; i < NTRAF; i++) if (traf[i].carril >= 0) n++;
    if (n > traf_act_max) traf_act_max = n;
    if (traf_vistos > traf_vistos_max) traf_vistos_max = traf_vistos;
    traf_vistos_sum += traf_vistos; traf_act_sum += n; diag_n++;
    if (diag_n % 60 == 0)
        dbglog(DBG_INFO, "DIAGTRAF t %d s %.0f v %.0f ruta %d vistos %d max %d/%d media %.2f/%.2f\n", diag_n / 60, (double)yo.s,
               (double)(yo.v * 3.6f), n, traf_vistos, traf_act_max, traf_vistos_max, (double)(traf_act_sum / diag_n), (double)(traf_vistos_sum / diag_n));
}
#endif
static void traf_paso(float dt)
{
    int i, j;
    cambio_cd++;
#ifdef DIAG_TRAF
    traf_diag();
#endif
    traf_aparece();
    for (i = 0; i < NTRAF; i++) {
        trafico_t *t = traf + i; const trafico_t *del = NULL, *sig = NULL, *ant = NULL;
        float x0 = t->c.x, hueco = 1e9f, obj, rate, b0, dsig = 1e9f, dant = 1e9f;
        int d, aparte, f0 = 0, f1 = 0, jug_del;
        if (t->carril < 0) continue;                            /* plaza libre */
        traf_lejos_v = traf_lejos(t->c.s);
        traf_pose_ant(t);                                       /* 4755C0 */
        if (t->b64 > 0) t->b64--;
        if (t->d22 > 0) t->d22--;
        for (j = 0; j < NTRAF; j++) {                           /* el de delante en mi carril (o en el que voy) */
            const trafico_t *o = traf + j; float dd = o->c.s - t->c.s;
            if (o == t) continue;
            if (dd > 0 && dd < dsig) { dsig = dd; sig = o; }
            if (dd <= 0 && -dd < dant) { dant = -dd; ant = o; }
            if (dd <= 0 || dd > 60.0f) continue;
            if (o->carril != t->carril && o->carril != t->destino && o->destino != t->carril) continue;
            if (dd < hueco) { hueco = dd; del = o; }
        }
        jug_del = yo.s > t->c.s && yo.s - t->c.s < dsig;            /* el siguiente de la lista es el jugador */
        /* 472450: objetivo y ritmo segun la distancia al jugador (en posiciones de la carretera) */
        obj = t->v_des;
        if (del && hueco < TR_HUECO && t->c.v > del->c.v) obj = fminf(obj, del->c.v);   /* 472A50 */
        d = (int)floorf((t->c.s - yo.s) / TR_POS);
        aparte = (int)(fabsf(traf_pos(t) - yo_pos(t->c.s)));
        if (d > TR_LEJOS) rate = 1.0f;
        else {
            rate = 0;
            if (d >= 0 && d <= 3 && obj > t->c.v) {
                rate = (d * 0.2f / TR_LEJOS + 0.8f) * (1.1f - aparte * 0.0833333f);
                if (rate < 0) rate = 0;
                if (rate > 1.0f) rate = 1.0f;
            } else if (d < TR_CERCA && d > 0 && t->c.v > obj) rate = 1.0f;
            if (0.25f > rate) rate = 1.0f * ((d < 0 ? 0 : d) * 0.75f / TR_LEJOS + 0.25f);   /* 6282A0 = 0,75 */
        }
        if (aparte < 1 && t->c.v > obj && d >= 0 && d <= 3) { f0 = 0; f1 = 1; rate = 0.05f; }
        traf_acelera(t, rate, obj, f0, f1);
        /* 47E4A0: golpeado, recuperacion; si no, carriles (47E110) */
        b0 = t->b08;
        if (t->b6c) traf_recupera(t);
        else {
            int l0 = t->carril;
            if (t->b14 > 0) { traf_cambio(t); traf_aparta(t); }
            else { traf_decide(t, jug_del ? NULL : sig, jug_del, ant); if (!traf_aparta(t)) traf_esquiva(t); }
            if (l0 == t->carril) {                              /* 47E110 / 6280EC: B08 no se acerca a 0 mas de 0,02 por cuadro */
                if (b0 > 0) t->b08 = fmaxf(t->b08, b0 - 0.02f);
                if (b0 < 0) t->b08 = fminf(t->b08, b0 + 0.02f);
            }
        }
        if (t->golpe >= 0) {                                    /* 4A2400: la animacion del golpe (16..19) y luego +2F8 */
            t->t_golpe += 0.0166112948f;
            if (!or2_pose(&choq, t->golpe, t->t_golpe, t->lado, t->c.pose_t, t->c.pose_r)) {
                t->t_recup = choq.recup[t->golpe]; t->golpe = -1;
            }
        } else if (t->t_recup > 0.0f) t->t_recup -= 0.0166112948f;
        t->c.s += t->c.v * dt;
        if (t->c.s < yo.s - TR_CIERRA_ATRAS * m_pos || t->c.s > yo.s + TR_CIERRA_DELANTE * m_pos || t->c.s > longitud - 20.0f) {
            traf_aparca(t); continue;                           /* 477F60: cerrado */
        }
        if (t->c.s > longitud - 20.0f) t->c.s = longitud - 20.0f;
        t->c.x = carril_x(t->c.s, t->carril) + t->b08 * 0.25f * carril_paso(t->c.s, t->carril);
        t->vlat = (t->c.x - x0) / dt;
        t->c.a = atan2f(t->c.x - x0, t->c.v * dt + 1e-4f);
        coche_coloca(&t->c);
    }
    traf_choques();                                             /* 4F9450 (47EBA0: despues de mover a todos) */
}

/* Conduccion, siguiendo el modelo del original (SlipAngleCtrl / RearGripCtrl / CorneringCtrl de la decompilacion de
   PC), simplificado: a = direccion de avance respecto a la carretera; deriva = carroceria respecto al avance.
   - derrape: un toque de freno (o soltar el gas frenando) arranca un temporizador de 40 cuadros; se mantiene mientras
     la deriva pasa de 8,4 grados y acaba 12 cuadros despues de bajar de ahi.
   - en derrape la carroceria apunta hacia dentro de la curva: la deriva busca giro x 0,6 rad mas el sesgo de la
     curva que viene (la curva marca el angulo de equilibrio); contravolante la reduce; tope 73 grados.
   - el derrape casi no quita velocidad; una ayuda lleva el avance hacia la carretera en proporcion a |sin deriva|.
   - sin derrape la deriva vuelve deprisa a una pequena (giro x 0,05). */
#define DERIVA_MAX 1.276f                                       /* 0x3400 = 73 grados */
static void coche_paso(coche_t *c, float giro, float gas, float freno, float dt)
{
    float p[3], th0, th1, iz, de, g, yaw, lim, sesgo, obj, x0; int f;
    ruta_en(c->s, p, &th0, &iz, &de);
    if (c == &yo && fis_ok && dt > 0) {                        /* el modelo del original */
        or2_entrada_t e; float q[3], tq, dx, dz;
        ruta_en(c->s + ADELANTE_POS * m_pos, q, &tq, NULL, NULL);
        dx = q[0] - p[0]; dz = q[2] - p[2];
        memset(&e, 0, sizeof e);
        e.stick = giro; e.acelerador = gas; e.freno = freno; e.mu = 1.0f; e.en_suelo = 1;
        e.rumbo_carretera = th0; e.rumbo_adelante = th0 + dif_ang(atan2f(dx, -dz) - th0);
        e.dist_izq = iz + c->x; e.dist_der = de - c->x;
        e.manual = manual_ && !auto_piloto; e.cambio_pide = pide_cambio; pide_cambio = 0;
        e.nivel = mando.nivel;
        if (mando.usa) {                                        /* el mando como el original (zona muerta, filtro) */
            e.volumen = or2_volante(&fis, mando.eje, mando.izq, mando.der, VEL_DIRECCION, 1, 0); e.volumen_ok = 1;
        }
        {   /* superficie bajo cada rueda y bajo el centro (calc_ground_coli_face: wheel+0x14 y event+0x248): de las
               franjas de la COLI; la rueda i en (cg_del | -cg_tras, +-via/2) del cuerpo, que va a a + deriva de la
               carretera */
            float h = c->a + c->deriva, ch = cosf(h), sh = sinf(h); int i;
            for (i = 0; i < 4; i++) {
                float fx = i < 2 ? fpar.cg_del : -fpar.cg_tras, ly = fpar.via[i >> 1] * ((i & 1) ? -0.5f : 0.5f);
                e.sup[i] = superficie(c->s + fx * ch + ly * sh, c->x + fx * sh - ly * ch);
            }
            e.sup_centro = superficie(c->s, c->x);
            memcpy(sup_rueda, e.sup, sizeof sup_rueda);         /* para las marcas (efectos_paso) */
            sup_jug = CON_SUP ? e.sup[0] | e.sup[1] | e.sup[2] | e.sup[3] : 2u;   /* +244 (sin franjas: asfalto) */
        }
        if (fis.grua > 0) {                                     /* grua del choque fuerte (PlWrecker) */
            float g[3], tg, gx, gz;
            if (!grua_on) {                                     /* el punto: 10 posiciones de carretera por delante */
                grua_s = fminf(c->s + GRUA_AVANCE, longitud - 50.0f); if (grua_s < c->s) grua_s = c->s;
                /* el original toma el centro de la fila de poligonos de la carretera (GetCsRoadInfoByCsLen); aqui
                   la ruta abarca las dos calzadas y la mediana, asi que el centro del carril mas cercano */
                grua_x = carril_x(grua_s, carril_cerca(grua_s, c->x));
                grua_on = 1;
            }
            ruta_en(grua_s, g, &tg, NULL, NULL);
            gx = g[0] + grua_x * cosf(tg); gz = g[2] + grua_x * sinf(tg);
            e.grua_ok = 1; e.grua_dx = gx - c->pos[0]; e.grua_dy = -(gz - c->pos[2]); e.grua_rumbo = tg;
        } else grua_on = 0;
        {   /* rebufo (CheckSlipStream): los coches de trafico cercanos */
            or2_otro_t ot[NTRAF]; int i, n = 0, cual[NTRAF]; float f0 = sinf(th0), f1 = cosf(th0), r0 = cosf(th0), r1 = -sinf(th0);
            for (i = 0; i < NTRAF; i++) {
                const trafico_t *t = traf + i; float ds = t->c.s - c->s, dx = t->c.x - c->x, ht = th0 + t->c.a;
                if (ds < -60.0f || ds > 60.0f) continue;
                ot[n].dx = f0 * ds + r0 * dx; ot[n].dy = f1 * ds + r1 * dx;
                ot[n].vx = sinf(ht) * t->c.v + r0 * t->vlat; ot[n].vy = cosf(ht) * t->c.v + r1 * t->vlat; cual[n++] = i;
            }
            or2_rebufo(&fis, ot, n);
            /* +B72 de cada candidato: 180 al ser el mejor con rebufo y luego uno menos por cuadro (doble de puntos
               si se le adelanta mientras dura) */
            for (i = 0; i < n; i++) if (fis.rebufo_marca & (1u << i)) traf[cual[i]].b72 = 180;
            for (i = 0; i < NTRAF; i++) if (traf[i].b72 > 0) traf[i].b72--;
        }
        x0 = c->x;
        or2_paso(&fis, &fpar, &e, 0.0f);
#ifdef PRUEBA_FISICA
        {   /* prueba: un resumen de la fisica cada segundo */
            static int n_fis;
            if (++n_fis % 60 == 0)
                dbglog(DBG_INFO, "FISICA: %3.0f km/h m%d rpm %5d vol %4d temp %2d deriva %5.1f h %.3f cab %5.2f bal %5.2f "
                       "cargas %3.0f %3.0f %3.0f %3.0f\n", (double)fis.velocidad_hud, fis.marcha, fis.rpm_suave, fis.stick16 >> 8,
                       fis.temporizador, (double)fis.desliz_grados, (double)fis.alt, (double)(fis.cabeceo * 57.29578f),
                       (double)(fis.balanceo * 57.29578f), (double)fis.r[0].carga, (double)fis.r[1].carga,
                       (double)fis.r[2].carga, (double)fis.r[3].carga);
        }
#endif
        c->v = fis.velocidad; c->a = dif_ang(fis.rumbo_vel - th0); c->deriva = fis.beta;
        c->derrape = fis.temporizador > 0 ? (fis.beta >= 0 ? 1 : -1) : 0; c->t_derrape = fis.temporizador;
        c->s += c->v * cosf(c->a) * dt;
        c->x += c->v * sinf(c->a) * dt + fis.corr_lat;         /* + la correccion lateral del rebote */
        fis.corr_lat = 0;
        if (fis.recolocar) { fis.recolocar = 0; grua_on = 0; c->s = grua_s; c->x = grua_x; }   /* fin de la grua */
        ruta_en(c->s, p, &th1, &iz, &de);
        c->a = dif_ang(c->a - (th1 - th0));
        {   /* muros: CbwColiWall del original (roce o rebote) con la normal hacia la calzada. El limite es el de la
               colision del original (franjas de muro o el vacio pasado el arcen, la hierba o la arena); sin esos datos,
               el borde del asfalto */
            float nx = 0, ny = 0, pen = 0, h, li, ld, xa, xb; c->choque = 0; if (c->t_muro > 0) c->t_muro--;
            limites(c->s, &li, &ld);
            lim = li - 1.0f;
            if (c->x < -lim) { pen = -lim - c->x; c->x = -lim; nx = cosf(th1); ny = -sinf(th1); }
            lim = ld - 1.0f;
            if (c->x > lim) { pen = c->x - lim; c->x = lim; nx = -cosf(th1); ny = sinf(th1); }
            if (muro_interior(c->s, c->x, 1.0f, &xa, &xb)) {     /* un muro entre las calzadas */
                if (x0 < (xa + xb) * 0.5f) { pen = c->x + 1.0f - xa; c->x = xa - 1.0f; nx = -cosf(th1); ny = sinf(th1); }
                else { pen = xb - (c->x - 1.0f); c->x = xb + 1.0f; nx = cosf(th1); ny = -sinf(th1); }
            }
            if (nx != 0 || ny != 0) {
                int golpe = or2_choque_muro(&fis, &fpar, nx, ny, pen);   /* 2 = choque fuerte (estados 1, 2, 5) */
                c->choque = 1; c->t_muro = 30;
                if (golpe && sfx_ok && !golpe_cd) { golpe_cd = 20; sfx_golpe(fminf(c->v / 40.0f, 1.0f)); }
                h = atan2f(fis.vx, fis.vy);                     /* rumbo de la velocidad (+ derecha) */
                c->v = sqrtf(fis.vx * fis.vx + fis.vy * fis.vy);
                c->a = dif_ang(h - th1); c->deriva = dif_ang(-fis.psi - h);
            }
        }
        memcpy(c->pose_t, fis.pose_t, sizeof c->pose_t); memcpy(c->pose_r, fis.pose_r, sizeof c->pose_r);
        coche_coloca(c);
        return;
    }
    g = fminf(c->v / 8.0f, 1.0f) * (1.0f - 0.45f * c->v / VMAX);
    /* temporizador de derrape (RearGripCtrl): toque de freno con el gas no a fondo, o soltar gas frenando */
    f = c->t_derrape > 0 ? c->t_derrape - 1 : 0;
    if (c->v > 16.0f && fabsf(giro) > 0.2f &&
        ((freno > 0.04f && c->freno_ant <= 0.04f && gas < 0.98f) || (gas < 0.98f && c->gas_ant >= 0.98f && freno > 0.04f))) f = 40;
    if (f > 0 && fabsf(c->deriva) > 0.147f) f = f > 12 ? f : 12;      /* se mantiene mientras deriva > 8,4 grados */
    c->t_derrape = f; c->derrape = f > 0 ? (c->deriva >= 0 ? 1 : -1) : 0;
    c->freno_ant = freno; c->gas_ant = gas;
    {   /* sesgo de la curva que viene: el angulo de equilibrio del derrape */
        float q[3], tq; ruta_en(c->s + 40.0f, q, &tq, NULL, NULL);
        sesgo = fmaxf(-0.6f, fminf(0.6f, 2.5f * (tq - th0)));
    }
    if (f > 0) {
        obj = giro * 0.6f + sesgo * 0.8f;
        if (giro * c->deriva < 0) obj = c->deriva * (1.0f - 1.5f * fabsf(giro)) * 0.5f;   /* contravolante: endereza */
        c->deriva += (obj - c->deriva) * 2.2f * dt;
        if (c->deriva > DERIVA_MAX) c->deriva = DERIVA_MAX;
        if (c->deriva < -DERIVA_MAX) c->deriva = -DERIVA_MAX;
        freno *= 0.25f;                                         /* el derrape casi no frena */
        yaw = giro * 1.6f * g * 1.25f;                          /* derrapando se cierra mas la curva */
        c->a -= c->a * 1.5f * fabsf(sinf(c->deriva)) * dt;      /* ayuda: el avance va hacia la carretera */
    } else {
        c->deriva += (giro * 0.05f * fminf(c->v / 30.0f, 1.0f) - c->deriva) * 8.0f * dt;
        yaw = giro * 1.6f * g;
        c->a -= c->a * 0.15f * dt;
    }
    c->a += yaw * dt;
    c->v += (gas * 13.0f * (1.0f - c->v / VMAX) - freno * 20.0f - 0.00025f * c->v * c->v - 0.3f) * dt;
    if (c->v < 0) c->v = 0;
    c->s += c->v * cosf(c->a) * dt;
    c->x += c->v * sinf(c->a) * dt;
    ruta_en(c->s, p, &th1, &iz, &de);
    c->a -= th1 - th0;                                          /* la carretera gira bajo el coche */
    {   /* muros: un golpe al tocar (15 %) y un roce suave mientras se sigue pegado; el coche rebota hacia dentro */
        int antes = c->t_muro > 0; c->choque = 0; if (c->t_muro > 0) c->t_muro--;
        lim = iz - 1.0f;
        if (c->x < -lim) { c->x = -lim; if (c->a < 0) c->a = -c->a * 0.4f; c->choque = 1; }
        lim = de - 1.0f;
        if (c->x > lim) { c->x = lim; if (c->a > 0) c->a = -c->a * 0.4f; c->choque = 1; }
        if (c->choque) { c->v -= c->v * (antes ? 0.35f * dt : 0.15f); c->deriva *= 0.9f; c->t_muro = 30; }
    }
    if (c->a > 1.2f) c->a = 1.2f;
    if (c->a < -1.2f) c->a = -1.2f;
    coche_coloca(c);
}

/* piloto automatico: sigue el centro de la calzada y frena antes de las curvas cerradas */
#ifdef PRUEBA_RAMA
static int piloto_rama(float s);
#endif
static void piloto(const coche_t *c, float *giro, float *gas, float *freno)
{
    float p[3], th, th2, iz, de, kap = 0, xc, ad, need, g, vt; int k;
    ruta_en(c->s, p, &th, &iz, &de);
    for (k = 1; k <= 6; k++) {                                  /* curvatura maxima en los proximos 120 m */
        float pa[3], ta, pb[3], tb;
        ruta_en(c->s + (k - 1) * 20.0f + 20.0f, pa, &ta, NULL, NULL);
        ruta_en(c->s + k * 20.0f + 20.0f, pb, &tb, NULL, NULL);
        kap = fmaxf(kap, fabsf(tb - ta) / 20.0f);
    }
    ruta_en(c->s + 8.0f, p, &th2, NULL, NULL);
    {   /* el carril con el coche de delante mas lejos (y si empata, el mas cercano) */
        int k, i, mejor = 0; float dmej = -1;
        for (k = 0; k < NCARRIL; k++) {
            float d = 300.0f, xk = carril_x(c->s + 30.0f, k);
#ifdef PRUEBA_RAMA
            if (piloto_rama(c->s) && (k < NCARRIL / 2) == PRUEBA_RAMA) continue;   /* prueba: hacia esa rama */
#endif
            for (i = 0; i < NTRAF; i++)
                if (traf[i].c.s > c->s - 3.0f && fabsf(traf[i].c.x - xk) < 2.2f) d = fminf(d, traf[i].c.s - c->s);
            d -= fabsf(xk - c->x) * 0.8f;
            if (d > dmej) { dmej = d; mejor = k; }
        }
        xc = carril_x(c->s + 30.0f, mejor);
    }
    ad = fmaxf(-0.25f, fminf(0.25f, 0.06f * (xc - c->x)));
    g = fmaxf(0.05f, fminf(c->v / 8.0f, 1.0f) * (1.0f - 0.45f * c->v / VMAX));
    need = (th2 - th) / 8.0f * c->v + (ad - c->a) * 3.0f;
    *giro = fmaxf(-1.0f, fminf(1.0f, need / (1.6f * g)));
    vt = kap > 0.0005f ? fminf(sqrtf(40.0f / kap), 140.0f) : 140.0f;   /* aceleracion lateral prudente: unos 4 g */
    *gas = c->v < vt ? 1.0f : 0.0f;
    *freno = c->v > vt + 4.0f ? 1.0f : 0.0f;
}

/* matriz del coche: x del modelo a la derecha, y arriba, el morro (-z) hacia delante */
static void pon_coche(const coche_t *c, matrix_t mod_coche)
{
    float r[3], fw[3], up[3], r2[3], k, sn; int i;               /* derecha = adelante x arriba */
    memcpy(fw, c->fw, 12); memcpy(up, c->up, 12);
    r[0] = fw[1] * up[2] - fw[2] * up[1]; r[1] = fw[2] * up[0] - fw[0] * up[2]; r[2] = fw[0] * up[1] - fw[1] * up[0];
    /* animacion de choque (pc_sample_crash_pose): giro y (+ izquierda), x (morro arriba), z, y desplazamiento */
    if (c->pose_r[1] != 0) { k = cosf(c->pose_r[1]); sn = sinf(c->pose_r[1]);
        for (i = 0; i < 3; i++) { r2[i] = r[i] * k + fw[i] * sn; fw[i] = fw[i] * k - r[i] * sn; r[i] = r2[i]; } }
    if (c->pose_r[0] != 0) { k = cosf(c->pose_r[0]); sn = sinf(c->pose_r[0]);
        for (i = 0; i < 3; i++) { r2[i] = fw[i] * k + up[i] * sn; up[i] = up[i] * k - fw[i] * sn; fw[i] = r2[i]; } }
    if (c->pose_r[2] != 0) { k = cosf(c->pose_r[2]); sn = sinf(c->pose_r[2]);
        for (i = 0; i < 3; i++) { r2[i] = up[i] * k - r[i] * sn; r[i] = r[i] * k + up[i] * sn; up[i] = r2[i]; } }
    memset(mod_coche, 0, sizeof(matrix_t));
    mod_coche[0][0] = r[0]; mod_coche[0][1] = r[1]; mod_coche[0][2] = r[2];
    mod_coche[1][0] = up[0]; mod_coche[1][1] = up[1]; mod_coche[1][2] = up[2];
    mod_coche[2][0] = -fw[0]; mod_coche[2][1] = -fw[1]; mod_coche[2][2] = -fw[2];
    for (i = 0; i < 3; i++)
        mod_coche[3][i] = c->pos[i] + r[i] * c->pose_t[0] + up[i] * c->pose_t[1] - fw[i] * c->pose_t[2];
    mod_coche[3][3] = 1.0f;
}

/* ---------------------------------------------------------------- camara de carrera (demo/camara.c) */
/* 43EB60: altura del suelo bajo un punto cercano al coche (con el marco de la carretera en el coche) */
static float cam_suelo(const float *p)
{
    float q[3], th, dx = p[0] - yo.pos[0], dz = p[2] - yo.pos[2];
    ruta_en(yo.s, q, &th, NULL, NULL);
    return altura(yo.s + dx * sinf(th) - dz * cosf(th), yo.x + dx * cosf(th) + dz * sinf(th));
}
/* un paso de logica de la camara (4858D0) con el coche del jugador; boton = cambio de vista (entrada 0x40000) */
static void camara_paso(int boton, int salida)
{
    cam_entrada_t e; float th, q[3], h, v;
    memset(&e, 0, sizeof e);
    memcpy(e.pos, yo.pos, 12); memcpy(e.fw, yo.fw, 12); memcpy(e.up, yo.up, 12); memcpy(e.normal, yo.up, 12);
    ruta_en(yo.s, q, &th, NULL, NULL);
    h = th + yo.a;                                              /* rumbo de la velocidad (+ derecha) */
    v = fis_ok ? fis.vel_mf : yo.v / 60.2f;
    e.vel[0] = sinf(h) * v; e.vel[2] = -cosf(h) * v; e.vel_mf = v;
    e.modelo = modelo_jug;
    if (fis_ok) {
        e.choque_activo = fis.t_choque > 0;
        e.choque_nuevo = fis.t_choque > cam_t_choque;          /* +2F0 bit 0: el paso en que entra un estado */
        e.choque_estado = fis.choque;
        e.choque_dur = choq.ok && fis.choque >= 0 && fis.choque < OR2_ESTADOS_CHOQUE ? choq.dur[fis.choque] : 0;
        e.grua = fis.grua;
        cam_t_choque = fis.t_choque;
    }
    memcpy(e.pose_t, yo.pose_t, 12); memcpy(e.pose_r, yo.pose_r, 12);
    e.boton_vista = boton; e.salida = salida; e.suelo = cam_suelo;
    cam_paso(&cam, &cam_tab, &e);
#ifdef DIAG_CAM
    {   static int n;
        float dy = cam.ojo[1] - yo.pos[1];
        if (++n % 15 == 0 || yo.up[1] < 0.97f || fabsf(yo.fw[1]) > 0.2f || dy < 1.0f || dy > 3.6f) dbglog(DBG_INFO, "CAM s %.1f base %.1f pos %.1f %.1f %.1f ojo %.1f %.1f %.1f mira %.1f %.1f %.1f v%d suelo %.2f fw %.2f %.2f %.2f up %.2f %.2f %.2f\n",
            (double)yo.s, (double)s_base, (double)yo.pos[0], (double)yo.pos[1], (double)yo.pos[2], (double)cam.ojo[0], (double)cam.ojo[1],
            (double)cam.ojo[2], (double)cam.mira[0], (double)cam.mira[1], (double)cam.mira[2], cam.vista, (double)cam_suelo(cam.ojo),
            (double)yo.fw[0], (double)yo.fw[1], (double)yo.fw[2], (double)yo.up[0], (double)yo.up[1], (double)yo.up[2]); }
#endif
}

/* ---------------------------------------------------------------- marcas de las ruedas y humo del derrape */
/* Marcas como el original (tire_mark_req 41FB50 / 41FDD0 / 4862C0 del PC; en SLES-53998 0x10dcd0 / 0x10df38 /
   0x1198f0, tabla 0x344d90): por rueda (las 4), el borde nuevo es el punto de contacto +- media anchura del neumatico
   (fila 40 delante / 41 detras de la tabla de parametros, car+2B4) de traves al avance (avance x normal del suelo);
   no se anade nada hasta que el contacto se ha movido mas de 0,2 m (0x32b390) del borde anterior. Clase por la
   superficie bajo la rueda (car+24C, la bandera del material de la COLI): 0xC02 asfalto, solo si esa rueda derrapa;
   0x8004 tierra/arena y 0x18 hierba, siempre. Cada tramo es un cuadrilatero con la textura de su clase (0x570000..2:
   EFECTOS.DAT, tools/conv_efectos.py), alfa 0,5 (0x32b394) en el borde nuevo y la del anterior en el viejo (0 al
   abrir y al cerrar: los extremos se funden), v = longitud x 0,5 + la fraccion que quedo del tramo anterior,
   subido 0,05 m (0x32b398). Anillos por clase de 256, 64 y 64 cuadrilateros (0x344d90 +8). Sin dibujar en la vista
   interior (car+4 & 0xC000, como el original). Asfalto: el original marca con la mascara de 95AFA8 de PcTireSmokeReq
   (41C570: deslizamiento +E0 y agarre +E4 de cada rueda contra 74FF5C), que la fisica de la demo no da: aqui son las
   traseras mientras dura el derrape (PROVISIONAL). */
#define NMARCA_CLASES 3
static const int NMARCA[NMARCA_CLASES] = { 256, 64, 64 };      /* 0x344d90 +8 */
#define MARCA_MIN 0.2f                                          /* 0x32b390 */
#define MARCA_ALFA 0.5f                                         /* 0x32b394 */
#define MARCA_ALTO 0.05f                                        /* 0x32b398 */
typedef struct { float p[4][3], v0, v1; uint8_t a0, a1; } marca_t;   /* borde nuevo (0, 1) y viejo (2, 3) */
static marca_t marca_256[256], marca_64a[64], marca_64b[64];
static marca_t *const marcas[NMARCA_CLASES] = { marca_256, marca_64a, marca_64b };
static int marca_n[NMARCA_CLASES], marca_sig[NMARCA_CLASES];
typedef struct { float izq[3], der[3], largo, alfa, ant[3]; int activa, clase, hay; } rueda_marca_t;   /* TireMark */
static rueda_marca_t rmarca[4];
static pvr_poly_hdr_t cab_marca[NMARCA_CLASES]; static int marca_tex_ok;
#define NHUMO 32
typedef struct { float p[3], edad; } humo_t;
static humo_t humos[NHUMO]; static int humo_sig;
static pvr_ptr_t tex_humo; static pvr_poly_hdr_t cab_humo;

static void efectos_inicia(void)
{
    static uint16_t t[32 * 32] __attribute__((aligned(32))); int x, y; pvr_poly_cxt_t cx; uint32_t sz;
    uint8_t *e = gd_carga("EFECTOS.DAT", &sz);
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) {           /* mancha redonda y suave (ARGB4444, lineal) */
        float d = sqrtf((x - 15.5f) * (x - 15.5f) + (y - 15.5f) * (y - 15.5f)) / 15.5f, a = fmaxf(0, 1 - d);
        t[y * 32 + x] = ((int)(a * a * 15) << 12) | 0x0EEE;
    }
    tex_humo = pvr_mem_malloc(sizeof t); pvr_txr_load(t, tex_humo, sizeof t);
    pvr_poly_cxt_txr(&cx, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, 32, 32, tex_humo, PVR_FILTER_BILINEAR);
    cx.gen.fog_type = PVR_FOG_TABLE; cx.txr.env = PVR_TXRENV_MODULATEALPHA; pvr_poly_compile(&cab_humo, &cx);
    if (e && sz >= 8 && !memcmp(e, "EFE1", 4)) {                /* texturas de las marcas (tools/conv_efectos.py) */
        uint32_t n, k, o = 8, h[4];
        memcpy(&n, e + 4, 4);
        for (k = 0; k < n && k < NMARCA_CLASES && o + 16 <= sz; k++) {
            pvr_ptr_t v;
            memcpy(h, e + o, 16); o += 16;
            if (h[2] > 2 || o + h[3] > sz || !(v = pvr_mem_malloc(h[3]))) break;
            pvr_txr_load(e + o, v, h[3]); o += h[3];
            pvr_poly_cxt_txr(&cx, PVR_LIST_TR_POLY, FMT[h[2]] | PVR_TXRFMT_TWIDDLED, h[0], h[1], v, PVR_FILTER_BILINEAR);
            cx.gen.fog_type = PVR_FOG_TABLE; cx.gen.culling = PVR_CULLING_NONE; cx.txr.env = PVR_TXRENV_MODULATEALPHA;
            pvr_poly_compile(&cab_marca[k], &cx);
            marca_tex_ok = k + 1;
        }
    }
    free(e);
}
static void efectos_borra(void)
{
    memset(marca_n, 0, sizeof marca_n); memset(marca_sig, 0, sizeof marca_sig); memset(rmarca, 0, sizeof rmarca);
    memset(humos, 0, sizeof humos);
}
/* 41FDD0: un tramo de la marca de la rueda r (borde nuevo L, R) en el anillo de su clase; devuelve la fraccion de v */
static float marca_tramo(rueda_marca_t *r, const float *L, const float *R, float a_nuevo, float a_viejo, int clase)
{
    float d0 = L[0] - r->izq[0], d1 = L[1] - r->izq[1], d2 = L[2] - r->izq[2], u = sqrtf(d0 * d0 + d2 * d2 + d1 * d1) * 0.5f + r->largo;
    marca_t *m = marcas[clase] + marca_sig[clase]; int k;
    for (k = 0; k < 3; k++) { m->p[0][k] = L[k]; m->p[1][k] = R[k]; m->p[2][k] = r->izq[k]; m->p[3][k] = r->der[k]; }
    m->p[0][1] += MARCA_ALTO; m->p[1][1] += MARCA_ALTO; m->p[2][1] += MARCA_ALTO; m->p[3][1] += MARCA_ALTO;
    m->v0 = r->largo; m->v1 = u; m->a0 = (uint8_t)(a_nuevo * 255); m->a1 = (uint8_t)(a_viejo * 255);
    marca_sig[clase] = (marca_sig[clase] + 1) % NMARCA[clase]; if (marca_n[clase] < NMARCA[clase]) marca_n[clase]++;
    return u - floorf(u);
}

/* cada paso: marcas de las cuatro ruedas (41FB50) y humo mientras se derrapa */
static void efectos_paso(const coche_t *c, float dt)
{
    int i, k; float r[3], q[3], up[3];
    memcpy(up, c->up, 12);
    r[0] = c->fw[1] * up[2] - c->fw[2] * up[1]; r[1] = c->fw[2] * up[0] - c->fw[0] * up[2]; r[2] = c->fw[0] * up[1] - c->fw[1] * up[0];
    for (i = 0; i < NHUMO; i++) if (humos[i].edad > 0) { humos[i].edad += dt; humos[i].p[1] += 0.8f * dt; if (humos[i].edad > 1.0f) humos[i].edad = 0; }
    for (i = 0; i < 4; i++) {
        rueda_marca_t *mk = rmarca + i;
        float fx = i < 2 ? fpar.cg_del : -fpar.cg_tras, ly = fpar.via[i >> 1] * ((i & 1) ? -0.5f : 0.5f);
        float b[3], a[3], L[3], R[3], la, mov, ancho = param_dat ? param_dat[(40 + (i >> 1)) * 19 + columna] : 0.245f;
        uint32_t f = sup_rueda[i]; int pone = 0, clase = 0;
        for (k = 0; k < 3; k++) q[k] = c->pos[k] + c->fw[k] * fx + r[k] * ly;   /* contacto (car+130 en el mundo) */
        if (!mk->hay) { memcpy(mk->ant, q, 12); memcpy(mk->izq, q, 12); memcpy(mk->der, q, 12); mk->hay = 1; continue; }
        for (k = 0; k < 3; k++) b[k] = q[k] - mk->ant[k];
        memcpy(mk->ant, q, 12);
        a[0] = b[1] * up[2] - b[2] * up[1]; a[1] = b[2] * up[0] - b[0] * up[2]; a[2] = b[0] * up[1] - b[1] * up[0];   /* avance x normal */
        la = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
        if (la > 1e-4f) for (k = 0; k < 3; k++) a[k] *= ancho * 0.5f / la;
        for (k = 0; k < 3; k++) { L[k] = q[k] + a[k]; R[k] = q[k] - a[k]; }
        mov = sqrtf((mk->izq[0] - q[0]) * (mk->izq[0] - q[0]) + (mk->izq[1] - q[1]) * (mk->izq[1] - q[1]) + (mk->izq[2] - q[2]) * (mk->izq[2] - q[2]));
        if (mov <= MARCA_MIN) continue;
        if (f & 0xC02u) pone = c->derrape && i >= 2;             /* asfalto: PROVISIONAL (ver arriba) */
        else if (f & 0x8004u) { pone = 1; clase = 1; }
        else if (f & 0x18u) { pone = 1; clase = 2; }
        if (pone && clase == mk->clase && clase < marca_tex_ok) {
            if (mk->activa) { mk->largo = marca_tramo(mk, L, R, MARCA_ALFA, mk->alfa, clase); mk->alfa = MARCA_ALFA; }
            else { mk->activa = 1; mk->alfa = 0; mk->largo = mov; }
        } else {
            mk->largo = mk->activa && mk->clase < marca_tex_ok ? marca_tramo(mk, L, R, 0, mk->alfa, mk->clase) : mov;
            mk->activa = 0; mk->alfa = 0;
        }
        memcpy(mk->izq, L, 12); memcpy(mk->der, R, 12); mk->clase = clase;
    }
    if (c->derrape) {                                           /* humo: el de antes (PROVISIONAL), de las traseras */
        static int alterna;
        if (++alterna & 1) {
            humo_t *h = humos + humo_sig; int w = 2 + ((alterna >> 1) & 1);
            float fx = -fpar.cg_tras, ly = fpar.via[1] * ((w & 1) ? -0.5f : 0.5f);
            for (k = 0; k < 3; k++) h->p[k] = c->pos[k] + c->fw[k] * fx + r[k] * ly;
            h->edad = 0.01f; humo_sig = (humo_sig + 1) % NHUMO;
        }
    }
}

/* un cuadrilatero (tira de 4: p0 p1 p2 p3) entero o nada: si se saltara un vertice la tira quedaria sin su fin de
   tira (EOL) y la siguiente cabecera llegaria a media tira, un parametro ilegal para el TA de la consola real */
static void pt_cuad2(const float p[4][3], const float *uv, uint32_t c0, uint32_t c1)   /* c0: vertices 0-1, c1: 2-3 */
{
    pt_t t[4]; int k;
    for (k = 0; k < 4; k++) {
        float x = p[k][0], y = p[k][1], z = p[k][2], w;
        mat_trans_single3_nodivw(x, y, z, w);
        if (w < cerca) return;
        t[k].x = x; t[k].y = y; t[k].w = w; t[k].u = uv[2 * k]; t[k].v = uv[2 * k + 1]; t[k].c = k < 2 ? c0 : c1; t[k].o = 0;
    }
    if (!ta_cabe(4)) return;
    for (k = 0; k < 4; k++) envia(t + k, k == 3);
}
static void pt_cuad(const float p[4][3], const float *uv, uint32_t c) { pt_cuad2(p, uv, c, c); }
/* en la lista translucida, con la matriz de vista cargada */
static void efectos_dibuja(void)
{
    static const float UV1[8] = { 0, 0, 1, 0, 0, 1, 1, 1 }; int i;
    for (int cl = 0; cl < marca_tex_ok; cl++) {                 /* marcas (486450 tire_mark_disp) */
        if (!marca_n[cl]) continue;
        pvr_prim(&cab_marca[cl], sizeof(pvr_poly_hdr_t));
        for (i = 0; i < marca_n[cl]; i++) {
            const marca_t *h = marcas[cl] + i; float uv[8];
            float dx = h->p[0][0] - ojo[0], dz = h->p[0][2] - ojo[2];
            if (dx * dx + dz * dz > 250.0f * 250.0f || dx * ade[0] + dz * ade[2] < 1.0f) continue;
            uv[0] = 0; uv[1] = h->v0; uv[2] = 1; uv[3] = h->v0; uv[4] = 0; uv[5] = h->v1; uv[6] = 1; uv[7] = h->v1;
            pt_cuad2(h->p, uv, ((uint32_t)h->a0 << 24) | 0xFFFFFF, ((uint32_t)h->a1 << 24) | 0xFFFFFF);
        }
    }
    pvr_prim(&cab_humo, sizeof cab_humo);
    for (i = 0; i < NHUMO; i++) {
        const humo_t *h = humos + i; float t = h->edad, tam, p[4][3]; int k; uint32_t col;
        if (t <= 0) continue;
        tam = 0.6f + 2.6f * t; col = ((uint32_t)((1 - t) * 140) << 24) | 0xE0E0E0;
        for (k = 0; k < 4; k++) {
            float sx = (k & 1) ? tam : -tam, sy = (k & 2) ? -tam : tam;
            p[k][0] = h->p[0] + sx * der[0] + sy * arr[0]; p[k][1] = h->p[1] + sx * der[1] + sy * arr[1] + tam * 0.5f;
            p[k][2] = h->p[2] + sx * der[2] + sy * arr[2];
        }
        pt_cuad((const float (*)[3])p, UV1, col);
    }
}

/* ---------------------------------------------------------------- textos */
static pvr_ptr_t txt_arr, txt_cen;
static uint16_t buf_arr[512 * 128] __attribute__((aligned(32))), buf_cen[256 * 32] __attribute__((aligned(32)));
static void texto_arriba(const char *l1, const char *l2, const char *l3)
{
    memset(buf_arr, 0, sizeof buf_arr);
    bfont_draw_str(buf_arr + 4 * 512 + 4, 512, 1, l1);
    if (l2) bfont_draw_str(buf_arr + 32 * 512 + 4, 512, 1, l2);
    if (l3) bfont_draw_str(buf_arr + 60 * 512 + 4, 512, 1, l3);
    pvr_txr_load(buf_arr, txt_arr, sizeof buf_arr);
}
static void texto_centro(const char *s)
{
    int n = strlen(s);
    memset(buf_cen, 0, sizeof buf_cen);
    bfont_draw_str(buf_cen + 4 * 256 + (256 - 12 * n) / 2, 256, 1, s);
    pvr_txr_load(buf_cen, txt_cen, sizeof buf_cen);
}
static void rect_txr(pvr_ptr_t t, int w, int h, float x, float y, float W, float H, int lista)
{
    pvr_poly_cxt_t cx; pvr_poly_hdr_t hd; pvr_vertex_t v;
    pvr_poly_cxt_txr(&cx, lista, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, w, h, t, PVR_FILTER_NONE);
    if (lista == PVR_LIST_TR_POLY) { cx.blend.src = PVR_BLEND_ONE; cx.blend.dst = PVR_BLEND_ONE; }
    pvr_poly_compile(&hd, &cx); pvr_prim(&hd, sizeof hd);
    v.argb = 0xFFFFFFFF; v.oargb = 0; v.z = 10.0f;
    v.flags = PVR_CMD_VERTEX; v.x = x; v.y = y; v.u = 0; v.v = 0; pvr_prim(&v, sizeof v);
    v.x = x + W; v.u = 1; pvr_prim(&v, sizeof v);
    v.x = x; v.y = y + H; v.u = 0; v.v = 1; pvr_prim(&v, sizeof v);
    v.flags = PVR_CMD_VERTEX_EOL; v.x = x + W; v.u = 1; pvr_prim(&v, sizeof v);
}

/* rectangulo negro translucido (fondo del marcador) */
static void caja(float x, float y, float W, float H, uint32_t argb)
{
    pvr_poly_cxt_t cx; pvr_poly_hdr_t hd; pvr_vertex_t v;
    pvr_poly_cxt_col(&cx, PVR_LIST_TR_POLY); pvr_poly_compile(&hd, &cx); pvr_prim(&hd, sizeof hd);
    v.argb = argb; v.oargb = 0; v.z = 9.0f; v.u = v.v = 0;
    v.flags = PVR_CMD_VERTEX; v.x = x; v.y = y; pvr_prim(&v, sizeof v);
    v.x = x + W; pvr_prim(&v, sizeof v);
    v.x = x; v.y = y + H; pvr_prim(&v, sizeof v);
    v.flags = PVR_CMD_VERTEX_EOL; v.x = x + W; pvr_prim(&v, sizeof v);
}

/* Modo de video: con cable VGA, VGA a 60 Hz; si no, el generico de KallistiOS (NTSC entrelazado a 60 Hz, que casi
   todas las teles PAL por euroconector aceptan). En una consola PAL (region de la flash Europa) sin VGA se pregunta,
   a 50 Hz (lo que ve cualquier tele PAL): A = 60 Hz, B o nada en 10 s = 50 Hz. La logica va a 60 pasos por segundo con
   el reloj (salto de cuadros), asi que a 50 Hz el juego va a la misma velocidad. */
static int modo_video = DM_640x480;
static void elige_video(void)
{
    int t;
    if (vid_check_cable() == CT_VGA || flashrom_get_region() != FLASHROM_REGION_EUROPE) return;
    modo_video = DM_640x480_PAL_IL;
    vid_set_mode(modo_video, PM_RGB565); vid_clear(0, 0, 0);
    bfont_draw_str(vram_s + 200 * 640 + 120, 640, 1, "Consola PAL: A = 60 Hz   B = 50 Hz");
    for (t = 0; t < 1000; t++) {                                /* 10 s */
        maple_device_t *mc = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = mc ? (cont_state_t *)maple_dev_status(mc) : NULL;
        if (st && (st->buttons & CONT_A)) { modo_video = DM_640x480; break; }
        if (st && (st->buttons & CONT_B)) break;
        thd_sleep(10);
    }
    if (modo_video == DM_640x480) {                             /* NTSC entrelazado a 60 Hz (o el que diga el cable) */
        vid_set_mode(modo_video, PM_RGB565);
        while (1) {                                             /* que suelte A */
            maple_device_t *mc = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
            cont_state_t *st = mc ? (cont_state_t *)maple_dev_status(mc) : NULL;
            if (!st || !(st->buttons & CONT_A)) break;
            thd_sleep(10);
        }
    }
}

static void aviso(const char *s)
{
    vid_clear(0, 0, 0);
    bfont_draw_str(vram_s + 220 * 640 + 60, 640, 1, s);
}

/* ---------------------------------------------------------------- juego */
/* gancho del modulo de video: pelicula de inicio (MV.PS2) antes del titulo; sin el modulo, nada */
extern void video_titulo(void) __attribute__((weak));
extern int video_atraccion(void) __attribute__((weak));        /* comun/video.c */
enum { TITULO, CUENTA, CARRERA, META, AGOTADO };
#define NPISTAS 3
/* camara de la seleccion de coche: distancia, altura y punto de mira sobre el coche (m) */
#define MENU_CAM_D 6.0f
#define MENU_CAM_Y 1.4f
#define MENU_CAM_MIRA 0.2f
#define PERIODO 16683                                           /* us por paso de logica (59,94 Hz) */
static int t_cuenta = 9999, t_ext = 9999, t_bif;
static int etapa_n, hud_ok, crono, puntos; static float vmax;

/* ---------------------------------------------------------------- el curso: el arbol del modo
   Como en el original (tools/conv_union.py escribe CURSO.DAT): 15 etapas en 5 niveles (csc_data_2 de SCRIPTS). Al final
   de cada etapa esta su union (BKkk.DAT, kk = clave de la etapa: la bifurcacion con los carteles de sus dos destinos y
   la ruta de la rama izquierda, BKD.DAT la de la derecha; en las 5 etapas finales, la meta) y al final de cada rama
   empieza la etapa siguiente. Cada fichero viene en su marco y se coloca al leerlo (modelo_coloca, en el hilo de
   carga) con las colocaciones del ejecutable: la union en su etapa (+0x98 del descriptor de tramo) y la etapa
   siguiente en el final de su rama. Todo se lee en segundo plano mientras se conduce: la union cuando faltan
   UNION_ANTES m de etapa; la rama se elige al llegar a la separacion (s_decide) por el centro de rama mas cercano al
   coche; UNION_LIBERA m dentro de la union (y con la rama elegida) se libera la etapa que se deja, se quitan sus puntos
   de la ruta (ruta_recorta) y el marco se recentra en el principio de la union (curso_recentra: las coordenadas no
   crecen de etapa en etapa); entonces se pide la etapa de la rama, que se ve en cuanto esta subida. Al llegar a ella
   pasa a ser la etapa actual y la union se libera UNION_LIBERA m despues. Memoria: etapa + union o union + etapa
   siguiente, en dos huecos fijos. En las etapas finales la union es la meta: GOAL al pasar su pancarta (meta). */
#define UNION_ANTES 2500.0f
#define UNION_LIBERA 300.0f
#define RAMA_SEPARA 6.0f                                        /* m entre los centros de las ramas para elegir */
enum { CUR_RAMA = 1, CUR_UNION = 2, CUR_SIGUIENTE = 4, CUR_META = 8 };   /* sucesos de curso_paso */
#define CURSO_MAX 16
#define NIEBLA_MAX 8
/* CURSO.DAT ('CUR1', n, tamano de registro y n registros; tools/conv_union.py escribe_curso) */
typedef struct { uint16_t p0, p1; uint8_t modo, r[3]; float d0, d1; uint32_t c0, c1; } niebla_t;   /* 24 bytes */
typedef struct {
    char zona[4]; uint8_t seg, nivel; int8_t izq, der;          /* zona, SEGnn.DAT, nivel 0..4, hijos (-1: meta) */
    float bk[4];                                                /* union en la etapa: x, y, z, giro y */
    float rama[2][4];                                           /* principio de la etapa de cada rama en la union */
    float m_pos, meta;                                          /* m por posicion de carretera; GOAL en la union */
    uint32_t nniebla; niebla_t niebla[NIEBLA_MAX];              /* scn_env_fog_<zona>, lista de la carretera */
    float sol[2];                                               /* scn_env_sun_<zona>, luz 1: inclinacion, rumbo (grados) */
    /* entorno (ver "entorno del original"): banderas de efectos (+0x3C del descriptor), transicion al entrar en la
       etapa siguiente (posicion de la union, segundos; metros de la union hasta ella, -1 sin etapa siguiente), las
       tres luces (scn_env_sun listas 0 cielo, 1 escena, 2 coches: 16 floats) y la niebla del cielo (lista 0) */
    uint32_t clima; uint16_t trans_pos; int8_t trans_seg; uint8_t r_; float trans_m;
    float luz[3][16];
    uint32_t nniebla0; niebla_t niebla0[NIEBLA_MAX];
} etapa_t;
static etapa_t curso[CURSO_MAX]; static int ncurso;
#ifndef PRUEBA_NODO
#define PRUEBA_NODO 0                                           /* prueba: etapa de salida (clave del arbol) */
#endif
static modelo_t uni, uni_der, sig;                             /* union, ruta de su rama derecha, etapa siguiente */
static modelo_t *const tramos[3] = { &etapa, &uni, &sig };     /* lo que se dibuja (lo que este subido) */
static const modelo_t *cielo;                                   /* el cielo que se dibuja */
static int cur_nodo = -1, cur_sig = -1, cur_rama, cur_en_union, etapa_nodo = -1;   /* etapa actual, siguiente, rama */
static float s_union, s_decide, s_sig, s_meta, s_suelta, s_etapa, t_carga_ult;     /* sucesos en s */
static uint32_t i_union, j_union;                               /* primer punto de la union en la ruta y en su fichero */
static float marco_et[4], marco_un[4], marco_sig[4];            /* colocacion de la etapa actual, su union, la siguiente */
static char fich_union[16], fich_sig[16];
/* nombres de las zonas para los avisos (codigo de CURSO.DAT) */
static const char *const ZONA_NOMBRE[][2] = {
    { "PALM", "PALM BEACH" }, { "LAKE", "DEEP LAKE" }, { "INDU", "INDUSTRIAL COMPLEX" }, { "ALPI", "ALPINE" },
    { "SNOW", "SNOW MOUNTAIN" }, { "CLOU", "CLOUDY HIGHLAND" }, { "CAST", "CASTLE WALL" }, { "GHOS", "GHOST FOREST" },
    { "FORE", "CONIFEROUS FOREST" }, { "DESE", "DESERT" }, { "TULI", "TULIP GARDEN" }, { "METR", "METROPOLIS" },
    { "RUIN", "ANCIENT RUINS" }, { "CAPE", "CAPE WAY" }, { "IMPE", "IMPERIAL AVENUE" } };
static const char *zona_nombre(int k)
{
    static char z[5]; unsigned i;
    if (k < 0 || k >= ncurso) return "";
    for (i = 0; i < sizeof ZONA_NOMBRE / sizeof ZONA_NOMBRE[0]; i++) if (!memcmp(ZONA_NOMBRE[i][0], curso[k].zona, 4)) return ZONA_NOMBRE[i][1];
    memcpy(z, curso[k].zona, 4); z[4] = 0; return z;
}

/* RAM: (en uso, libre) en KB; libre = huecos del monton + lo que queda hasta el final de la RAM (16 MB, menos 64 KB
   para la pila del arranque) */
static void ram_kb(unsigned *uso, unsigned *libre)
{
    struct mallinfo mi = mallinfo(); uintptr_t tope = (uintptr_t)sbrk(0);
    *uso = mi.uordblks >> 10;
    *libre = (unsigned)((mi.fordblks + (0x8d000000u - 0x10000u - tope)) >> 10);
}
static void curso_error(const char *s) { dbglog(DBG_ERROR, "CURSO: %s\n", s); vid_set_mode(modo_video, PM_RGB565); aviso(s); for (;;) thd_sleep(1000); }
static void sube_etapa(modelo_t *m)
{
    if (sube_modelo(m)) curso_error("Sin memoria de video para la etapa");
}
/* el arbol (CURSO.DAT); 0 bien */
static int curso_lee(void)
{
    uint32_t sz, n, r; uint8_t *b = gd_carga("CURSO.DAT", &sz);
    if (!b || sz < 12 || memcmp(b, "CUR1", 4)) { free(b); return -1; }
    memcpy(&n, b + 4, 4); memcpy(&r, b + 8, 4);
    if (r != sizeof(etapa_t) || n > CURSO_MAX || sz < 12 + n * r) { free(b); return -2; }
    memcpy(curso, b + 12, n * r); ncurso = n; free(b);
    return 0;
}
/* M1 * M2 (primero M2, en el marco de M1; como compone de tools/conv_union.py) */
static void marco_compone(const float *m1, const float *m2, float *o)
{
    float c = cosf(m1[3]), sn = sinf(m1[3]), x = m2[0], z = m2[2];
    o[0] = m1[0] + x * c + z * sn; o[1] = m1[1] + m2[1]; o[2] = m1[2] - x * sn + z * c; o[3] = m1[3] + m2[3];
}
/* reserva los huecos de RAM de las etapas y de la union (el tamano del mayor fichero de cada uno, en el GD) */
static int curso_huecos(void)
{
    uint32_t lba, t; int i; char n[16];
    for (i = 0; i < ncurso; i++) {
        sprintf(n, "SEG%02d.DAT", curso[i].seg);
        if (!gd_busca(n, &lba, &t) && (t + 2047) / 2048 * 2048 > cap_etapa) cap_etapa = (t + 2047) / 2048 * 2048;
        sprintf(n, "BK%02d.DAT", i);
        if (!gd_busca(n, &lba, &t) && (t + 2047) / 2048 * 2048 > cap_union) cap_union = (t + 2047) / 2048 * 2048;
    }
    hueco_etapa = memalign(32, cap_etapa); hueco_union = cap_union ? memalign(32, cap_union) : NULL;
    dbglog(DBG_INFO, "CURSO: %d etapas, huecos etapa %uK union %uK\n", ncurso, (unsigned)(cap_etapa >> 10), (unsigned)(cap_union >> 10));
    return !hueco_etapa || (cap_union && !hueco_union);
}
/* lee la etapa de la clave k en el hueco (la de salida); 0 bien */
static int curso_lee_etapa(int k)
{
    char n[16]; int r;
    sprintf(n, "SEG%02d.DAT", curso[k].seg);
    r = lee_modelo_en(&etapa, n, hueco_etapa, cap_etapa);
    etapa_nodo = r ? -1 : k;
    return r;
}
/* al empezar una carrera: la etapa de salida (leida otra vez si ya se habia liberado o era otra) y nada mas */
static void curso_reinicia(void)
{
    int k = PRUEBA_NODO < ncurso ? PRUEBA_NODO : 0;
    carga_espera(); carga.estado = 0;
    libera_modelo(&uni); libera_modelo(&uni_der); libera_modelo(&sig);
    if (!etapa.tv || etapa_nodo != k) {
        int r;
        libera_modelo(&etapa);
        musica_pausa_lectura(1);                                /* el GD para la etapa (la musica tira del anillo) */
        r = curso_lee_etapa(k);
        musica_pausa_lectura(0);
        if (r) curso_error("No se pudo leer la etapa de salida");
        sube_etapa(&etapa);
    }
    if (ruta_pon(&etapa)) curso_error("Sin memoria para la ruta");
    cielo = &etapa; cur_nodo = k; cur_sig = -1; etapa_n = curso[k].nivel; cur_rama = -1; cur_en_union = 0;
    if (curso[k].m_pos > 1.0f) m_pos = curso[k].m_pos;
    s_union = s_decide = s_sig = s_meta = s_suelta = -1; s_etapa = 0;
    marco_et[0] = marco_et[1] = marco_et[2] = marco_et[3] = 0;
}
/* posicion del centro de la ruta del fichero m en su punto i (m->ruta) */
static float dist_centro(const modelo_t *m, uint32_t i, const float *p)
{
    const float *a = m->ruta + m->paso_ruta * (i < m->nruta ? i : m->nruta - 1);
    return sqrtf((a[0] - p[0]) * (a[0] - p[0]) + (a[2] - p[2]) * (a[2] - p[2]));
}
/* mueve un modelo ya colocado (dx, dz) hacia atras: vertices y centros (salvo el cielo, que va con la camara) y ruta */
static void modelo_desplaza(modelo_t *m, float dx, float dz)
{
    uint32_t k, i;
    for (k = 0; k < m->nmallas; k++) {
        malla_t *ma = m->mallas + k; vert_t *v = m->verts + ma->v0;
        if (k >= m->inicio[3] && k < m->inicio[4]) continue;
        if (malla_de_cartel(m, ma)) continue;                   /* en el marco de su nodo */
        for (i = 0; i < ma->n; i++, v++) { v->x -= dx; v->z -= dz; }
        ma->cx -= dx; ma->cz -= dz;
    }
    for (k = 0; k < m->nnodos; k++) { m->nodos[k].cx -= dx; m->nodos[k].cz -= dz; }
    for (k = 0; k < m->ncarteles; k++) { m->carteles[k][3][0] -= dx; m->carteles[k][3][2] -= dz; }
    for (i = 0; i < m->nruta; i++) { m->ruta[m->paso_ruta * i] -= dx; m->ruta[m->paso_ruta * i + 2] -= dz; }
}
/* recentra el marco de la carrera: resta (dx, dz) a todo lo que esta en coordenadas del mundo (ruta, modelos
   subidos, coches, efectos y camara); lo que va por la ruta (s, desvio) no cambia */
static void curso_recentra(float dx, float dz)
{
    uint32_t i; int k, j;
    for (i = 0; i < ruta_n; i++) { ruta_p[PR * i] -= dx; ruta_p[PR * i + 2] -= dz; }
    for (k = 0; k < 3; k++) if (tramos[k]->datos) modelo_desplaza(tramos[k], dx, dz);
    if (uni_der.datos) modelo_desplaza(&uni_der, dx, dz);
    yo.pos[0] -= dx; yo.pos[2] -= dz;
    for (k = 0; k < NTRAF; k++) { traf[k].c.pos[0] -= dx; traf[k].c.pos[2] -= dz; }
    for (k = 0; k < NHUMO; k++) { humos[k].p[0] -= dx; humos[k].p[2] -= dz; }
    for (j = 0; j < NMARCA_CLASES; j++)
        for (k = 0; k < marca_n[j]; k++) for (i = 0; i < 4; i++) { marcas[j][k].p[i][0] -= dx; marcas[j][k].p[i][2] -= dz; }
    for (k = 0; k < 4; k++) {
        rmarca[k].izq[0] -= dx; rmarca[k].izq[2] -= dz; rmarca[k].der[0] -= dx; rmarca[k].der[2] -= dz;
        rmarca[k].ant[0] -= dx; rmarca[k].ant[2] -= dz;
    }
    cam_traslada(&cam, dx, 0, dz);                              /* camara.c: solo el ojo y la mira van en el mundo */
    ojo[0] -= dx; ojo[2] -= dz;
}
/* una vez por paso de logica en carrera: cargas, eleccion de rama, liberaciones y cambio de etapa; devuelve los
   sucesos CUR_* */
static int curso_paso(void)
{
    int ev = 0; const etapa_t *e = curso + cur_nodo;
    if (carga.estado == 1 && yo.s > longitud - 60.0f) carga_espera();   /* GD lento: la carretera aun no esta */
    if (carga.estado == 2) {                                    /* leido: texturas a la VRAM y ruta detras */
        modelo_t *m = carga.m; carga.estado = 0;
        if (carga.r) curso_error("No se pudo leer la etapa siguiente");
        t_carga_ult = (timer_ms_gettime64() - carga.t0) / 1000.0f;
        if (m == &uni) {
            sube_etapa(m);
            if ((int)(i_union = ruta_anade(m)) < 0) curso_error("Sin memoria para la ruta");
            j_union = ruta_salto; s_union = s_base + i_union * PASO;
            if (e->izq >= 0) carga_pide(&uni_der, "BKD.DAT", NULL, 0, marco_un);
            else s_meta = s_union + e->meta;                   /* la meta: la pancarta GOAL */
        } else if (m == &uni_der) {
            uint32_t k = j_union;                              /* la separacion: donde los centros se apartan */
            while (k + 1 < uni.nruta && k + 1 < uni_der.nruta && dist_centro(&uni, k, uni_der.ruta + uni_der.paso_ruta * k) < RAMA_SEPARA) k++;
            s_decide = s_union + (k - j_union) * PASO;
        } else if (m == &sig) {
            int i;
            sube_etapa(m);
            if ((i = ruta_anade(m)) < 0) curso_error("Sin memoria para la ruta");
            s_sig = s_base + i * PASO;
        }
        {
            unsigned u, l; ram_kb(&u, &l);
            dbglog(DBG_INFO, "CURSO: %s en %.2f s (s %.0f), RAM en uso %uK libre %uK, VRAM libre %uK\n", m == &uni ? fich_union : m == &uni_der ? "BKD.DAT" : fich_sig,
                   (double)t_carga_ult, (double)yo.s, u, l, (unsigned)(pvr_mem_available() >> 10));
        }
    }
    if (s_union < 0 && !uni.datos && etapa.datos && carga.estado == 0 && yo.s > longitud - UNION_ANTES) {
        marco_compone(marco_et, e->bk, marco_un); sprintf(fich_union, "BK%02d.DAT", cur_nodo);
        carga_pide(&uni, fich_union, hueco_union, cap_union, marco_un);
    }
    if (s_union >= 0 && !cur_en_union && yo.s >= s_union) { cur_en_union = 1; ev |= CUR_UNION; }
    if (s_meta >= 0 && yo.s >= s_meta) { s_meta = -1; ev |= CUR_META; }
    if (s_decide >= 0 && cur_rama < 0 && yo.s >= s_decide) {   /* la rama: el centro mas cercano al coche */
        uint32_t k = j_union + (uint32_t)((yo.s - s_union) / PASO);
        cur_rama = dist_centro(&uni_der, k, yo.pos) < dist_centro(&uni, k, yo.pos);
        if (cur_rama) {                                         /* la derecha: su ruta en lugar de la izquierda */
            float p[3], th;
            ruta_n = i_union;
            if (ruta_anade(&uni_der) < 0) curso_error("Sin memoria para la ruta");
            ruta_en(yo.s, p, &th, NULL, NULL);                  /* el mismo sitio, desde el centro nuevo */
            yo.x = (yo.pos[0] - p[0]) * cosf(th) + (yo.pos[2] - p[2]) * sinf(th);
        }
        libera_modelo(&uni_der);
        cur_sig = cur_rama ? e->der : e->izq;
        ev |= CUR_RAMA;
    }
    if (etapa.datos && cur_en_union && cur_rama >= 0 && yo.s > s_union + UNION_LIBERA) {
        unsigned u, l;
        libera_modelo(&etapa); etapa_nodo = -1; cielo = &uni;
        ruta_recorta(i_union); i_union = 0;                     /* fuera los puntos de la etapa que se deja */
        curso_recentra(marco_un[0], marco_un[2]);               /* el marco, al principio de la union */
        marco_un[0] = marco_un[2] = 0;
        marco_compone(marco_un, e->rama[cur_rama], marco_sig); sprintf(fich_sig, "SEG%02d.DAT", curso[cur_sig].seg);
        ram_kb(&u, &l); dbglog(DBG_INFO, "CURSO: etapa %d liberada (s %.0f), recentrado, RAM libre %uK, VRAM libre %uK\n", cur_nodo, (double)yo.s, l, (unsigned)(pvr_mem_available() >> 10));
    }
    if (cur_rama >= 0 && !etapa.datos && !sig.datos && s_sig < 0 && carga.estado == 0) carga_pide(&sig, fich_sig, hueco_etapa, cap_etapa, marco_sig);
    if (s_sig >= 0 && yo.s >= s_sig) {                         /* la etapa siguiente pasa a ser la actual */
        etapa = sig; memset(&sig, 0, sizeof sig); etapa_nodo = cur_sig; cielo = &etapa;
        cur_nodo = cur_sig; cur_sig = -1; memcpy(marco_et, marco_sig, sizeof marco_et);
        etapa_n = curso[cur_nodo].nivel; s_etapa = s_sig; s_suelta = s_sig + UNION_LIBERA;
        if (curso[cur_nodo].m_pos > 1.0f) m_pos = curso[cur_nodo].m_pos;
        s_union = s_decide = s_sig = -1; cur_rama = -1; cur_en_union = 0;
        ev |= CUR_SIGUIENTE;
    }
    if (s_suelta >= 0 && uni.datos && yo.s > s_suelta) { libera_modelo(&uni); s_suelta = -1; }
    return ev;
}
#ifdef PRUEBA_RAMA
static int piloto_rama(float s) { return cur_rama < 0 && s_decide >= 0 && s > s_decide - 400.0f; }
#endif
/* ---------------------------------------------------------------- nodos visibles (como el original)
   Lo que dibuja el original de la etapa en cada cuadro (decompilacion del PC): de cada nodo (tools/modelo.py nodos),
   404B00: esfera contra el plano cercano, los cuatro laterales y el lejano, y el LOD por tamano en pantalla:
   w = r / (z tan(fov vertical / 2)); se descarta si 127 - exponente(w) >= umbral (4044E0(6) en la etapa,
   race_area_display_44f120; 7 con la bandera 0x40 del nodo), es decir, se dibuja si z <= r 2^(umbral - 1) /
   tan(fov vertical / 2). De las ramas de la union, solo la del coche una vez elegida (race_area_stage_draw_44e290:
   obj | 1 o | 2). La lista de nodos por seccion de la etapa (cs_CS_<zona>_bin) solo se usaria si el fichero la trae
   (nsec): el formato de PS2 de esas listas esta sin descifrar (tools/conv_tramo.py lee_visibilidad). */
static float dist2_ruta(const modelo_t *m, uint32_t i, const float *p)
{
    const float *a = m->ruta + m->paso_ruta * i;
    return (a[0] - p[0]) * (a[0] - p[0]) + (a[1] - p[1]) * (a[1] - p[1]) + (a[2] - p[2]) * (a[2] - p[2]);
}
/* Presupuesto de la etapa: lo que dibuja el original (16000-47000 vertices en PALM) no siempre cabe en el TA con el
   coche y el trafico. Reparto como lo veria el jugador:
   - lo cercano y la carretera, siempre: cada nodo visible tiene una clase de distancia por la parte mas cercana de su
     esfera, cd = 4 log2(z - r) (0..47, pasos del 19 %); se dibujan enteras las clases hasta prio_d, la mayor cuyo coste
     acumulado cabe en 3/4 del tope de la etapa;
   - de lo de mas alla, primero lo que el original quitaria antes: clase de LOD por p = z / (r 2^(umbral - 1) /
     tan(fov_v / 2)), la fraccion de la distancia hasta la que lo dibuja el LOD de 404B00 (p <= 1; los nodos sin umbral
     como si tuvieran el de la etapa, 6): cp = 40 + 8 log2(p) (0..63, pasos del 9 %). Lo pequeno y lejano tiene p alto y se
     quita antes; lo grande (montes, mar, calzada lejana) se queda. Se dibujan las clases 0..prio_p con lo que quede del
     tope;
   - lo que la niebla del original (scn_env_fog) ya tapa (63/64 o mas en la parte mas cercana del nodo) va a la ultima
     clase de LOD: es lo primero que se quita y no se nota.
   Los cortes bajan en el mismo cuadro si no cabe y suben de una clase en una (cada 4 cuadros y con al menos 2 clases de
   holgura), para que las mallas del limite no aparezcan y desaparezcan de un cuadro a otro. El coste de cada clase es el
   teorico de sus nodos (relleno[0]) por ta_f y por prio_alfa, lo enviado de verdad / lo estimado (las mallas de cada
   nodo se recortan una a una contra la vista). */
#define ND_PRIO 48
#define NB_PRIO 64
static uint32_t prio_coste[ND_PRIO][NB_PRIO], prio_vtx[ND_PRIO][NB_PRIO];   /* bytes teoricos y vertices por clase */
static int prio_d = ND_PRIO - 1, prio_p = NB_PRIO - 1, prio_esp_d, prio_esp_p;
static float prio_alfa = 1.0f;
static float niebla_d; static int niebla_modo;                   /* la niebla del cuadro (niebla_paso) */
#ifdef DIAG_VTX
static float diag_quitado_z, diag_quitado_r;                     /* el nodo quitado mas cercano (z - r) y su radio */
static uint32_t diag_orig_vtx, diag_quitado_n;                    /* vertices de lo que dibujaria el original; nodos quitados */
#endif
static inline int log2x(float x, int k)                         /* k log2(x) por los bits del float (x > 0), k = 4 u 8 */
{
    union { float f; int32_t i; } q; q.f = x;
    return (q.i - (127 << 23)) >> (k == 8 ? 20 : 21);
}
static void nodos_visibles(modelo_t *mo)
{
    float tanh = 320.0f / fx, tanv = 240.0f / fx, kh = sqrtf(1 + tanh * tanh), kv = sqrtf(1 + tanv * tanv), lod = fx / 240.0f;
    float z_tapa = niebla_d > 0 ? (niebla_modo == 2 ? 2.04f : 4.16f) / niebla_d : 1e30f;   /* 1 - e^-x >= 63/64 */
    uint32_t j, n; const uint16_t *L = NULL;
    if (!mo->vis) return;
    memset(mo->vis, 0, (mo->nnodos + 31) / 32 * 4);
    if (mo->nsec && mo->nruta) {                                /* la seccion del coche: la del punto mas cercano */
        uint32_t k = mo->ruta_i < mo->nruta ? mo->ruta_i : 0, a = k > 25 ? k - 25 : 0, b = k + 25 < mo->nruta ? k + 25 : mo->nruta - 1, i, s;
        float d = dist2_ruta(mo, k, yo.pos), e;
        for (i = a; i <= b; i++) if ((e = dist2_ruta(mo, i, yo.pos)) < d) { d = e; k = i; }
        if (d > 50.0f * 50.0f) for (i = 0; i < mo->nruta; i++) if ((e = dist2_ruta(mo, i, yo.pos)) < d) { d = e; k = i; }
        mo->ruta_i = k; s = mo->sec_mapa[k] < mo->nsec ? mo->sec_mapa[k] : mo->nsec - 1;
        L = mo->sec_lista + mo->sec_ini[s]; n = mo->sec_ini[s + 1] - mo->sec_ini[s];
    } else n = mo->nnodos;
    for (j = 0; j < n; j++) {
        uint32_t i = L ? L[j] : j, rama, u; nodo_t *d; float dx, dy, dz, zv, xv, yv, lim; int cp, cd;
        if (i >= mo->nnodos) continue;
        d = mo->nodos + i;
        dx = d->cx - ojo[0]; dy = d->cy - ojo[1]; dz = d->cz - ojo[2];
        zv = dx * ade[0] + dy * ade[1] + dz * ade[2];
        if (zv + d->r < cerca || zv - d->r > lejos) continue;
        xv = dx * der[0] + dy * der[1] + dz * der[2]; yv = dx * arr[0] + dy * arr[1] + dz * arr[2];
        if (fabsf(xv) - zv * tanh > d->r * kh || fabsf(yv) - zv * tanv > d->r * kv) continue;
        u = d->fl & 15;
        lim = d->r * lod * (float)(1u << ((u ? u : 6) - 1));
        if (u && zv > 0 && zv > lim) continue;                  /* LOD */
        rama = (d->fl >> 8) & 3;
        if (rama && cur_rama >= 0 && (int)rama - 1 != cur_rama) continue;   /* la otra rama de la union */
        mo->vis[i >> 5] |= 1u << (i & 31);
        cd = zv - d->r > 1.0f ? log2x(zv - d->r, 4) : 0; cd = cd > ND_PRIO - 1 ? ND_PRIO - 1 : cd;
        if (zv - d->r > z_tapa) cp = NB_PRIO - 1;               /* tapado por la niebla */
        else if (zv <= 0 || lim <= 0) cp = 0;
        else { cp = 40 + log2x(zv / lim, 8); cp = cp < 0 ? 0 : cp > NB_PRIO - 1 ? NB_PRIO - 1 : cp; }
        d->relleno[1] = (d->relleno[1] & ~4095u) | (uint32_t)(cd << 6 | cp);
        prio_coste[cd][cp] += d->relleno[0]; prio_vtx[cd][cp] += d->relleno[1] >> 12;
    }
}
/* quita de mo->vis los nodos de las clases que no caben */
static void nodos_quita(modelo_t *mo)
{
    uint32_t w, i;
    if (!mo->vis) return;
    for (w = 0; w < (mo->nnodos + 31) / 32; w++) {
        uint32_t b = mo->vis[w];
        while (b) {
            int k = __builtin_ctz(b), cl; b &= b - 1; i = w * 32 + k; cl = mo->nodos[i].relleno[1] & 4095;
            if ((cl >> 6) <= prio_d || (cl & 63) <= prio_p) continue;
            mo->vis[w] &= ~(1u << k);
#ifdef DIAG_VTX
            {
                const nodo_t *d = mo->nodos + i; float z = (d->cx - ojo[0]) * ade[0] + (d->cy - ojo[1]) * ade[1] + (d->cz - ojo[2]) * ade[2] - d->r;
                if (z < diag_quitado_z) { diag_quitado_z = z; diag_quitado_r = d->r; }
                diag_quitado_n++;
            }
#endif
        }
    }
}
/* corte con histeresis: baja ya; sube una clase cada 4 cuadros si el objetivo esta al menos 2 por encima */
static int prio_sube(int actual, int objetivo, int *espera, int max)
{
    if (objetivo < actual) { *espera = 0; return objetivo; }
    if (objetivo >= actual + 2 && actual < max) { if (++*espera >= 4) { *espera = 0; return actual + 1; } }
    else *espera = 0;
    return actual;
}
static uint32_t prio_est;                                       /* coste estimado de lo que se dibuja (bytes) */
static void nodos_visibles_todos(void)
{
    static uint32_t fila_c[ND_PRIO], fila_v[ND_PRIO], col_c[NB_PRIO], col_v[NB_PRIO];
    uint32_t i, tope = ta_tope_etapa / 100 * 95, acum, v, cerca_c, cerca_v, vmax = (uint32_t)(VTX_ETAPA_MAX / prio_alfa); int c, d, cabe;
    float k = (float)ta_f / 256.0f * prio_alfa;                 /* bytes enviados por byte teorico de los nodos */
    memset(prio_coste, 0, sizeof prio_coste); memset(prio_vtx, 0, sizeof prio_vtx);
    for (i = 0; i < 3; i++) if (tramos[i]->tv) nodos_visibles(tramos[i]);
    for (d = 0; d < ND_PRIO; d++) for (fila_c[d] = fila_v[d] = 0, c = 0; c < NB_PRIO; c++) { fila_c[d] += prio_coste[d][c]; fila_v[d] += prio_vtx[d][c]; }
    /* lo cercano: hasta 3/4 del tope (todo si cabe todo) */
    for (acum = v = 0, cabe = -1, d = 0; d < ND_PRIO; d++) {
        if ((float)(acum + fila_c[d]) * k > (float)tope * 0.75f || v + fila_v[d] > vmax / 4 * 3) break;
        acum += fila_c[d]; v += fila_v[d]; cabe = d;
    }
    {   /* si cabe todo, todo */
        uint32_t tc = 0, tv = 0;
        for (d = 0; d < ND_PRIO; d++) { tc += fila_c[d]; tv += fila_v[d]; }
        if ((float)tc * k <= (float)tope && tv <= vmax) cabe = ND_PRIO - 1;
    }
    prio_d = prio_sube(prio_d, cabe < 0 ? 0 : cabe, &prio_esp_d, ND_PRIO - 1);
    for (cerca_c = cerca_v = 0, d = 0; d <= prio_d; d++) { cerca_c += fila_c[d]; cerca_v += fila_v[d]; }
    /* lo de mas alla, por la clase de LOD */
    memset(col_c, 0, sizeof col_c); memset(col_v, 0, sizeof col_v);
    for (d = prio_d + 1; d < ND_PRIO; d++) for (c = 0; c < NB_PRIO; c++) { col_c[c] += prio_coste[d][c]; col_v[c] += prio_vtx[d][c]; }
    for (acum = cerca_c, v = cerca_v, cabe = -1, c = 0; c < NB_PRIO; c++) {
        if ((float)(acum + col_c[c]) * k > (float)tope || v + col_v[c] > vmax) break;
        acum += col_c[c]; v += col_v[c]; cabe = c;
    }
    if (prio_d == ND_PRIO - 1) cabe = NB_PRIO - 1;
    prio_p = prio_sube(prio_p, cabe, &prio_esp_p, NB_PRIO - 1);
    for (prio_est = cerca_c, c = 0; c <= prio_p; c++) prio_est += col_c[c];
    prio_est = prio_est * ta_f >> 8;
#ifdef DIAG_VTX
    diag_quitado_z = 1e9f; diag_quitado_r = 0; diag_quitado_n = 0;
    for (diag_orig_vtx = 0, d = 0; d < ND_PRIO; d++) diag_orig_vtx += fila_v[d];
#endif
    if (prio_d < ND_PRIO - 1) for (i = 0; i < 3; i++) if (tramos[i]->tv) nodos_quita(tramos[i]);
}
/* tras el cuadro: lo que de verdad costo la etapa frente a lo estimado; si alguna tira no cupo, mas prudencia */
static void prio_ajusta(int corto, uint32_t usado)
{
    if (prio_est > 16 * 1024) {
        float a = (float)usado / (float)prio_est;
        a = a < 0.2f ? 0.2f : a > 2.0f ? 2.0f : a;
        prio_alfa = prio_alfa * 0.8f + a * 0.2f;
    }
    if (corto) {
        prio_alfa *= 1.15f; if (prio_alfa > 2.0f) prio_alfa = 2.0f;
        prio_p = prio_p > 2 ? prio_p - 2 : 0; prio_esp_p = prio_esp_d = 0;
        if (prio_p == 0 && prio_d > 0) prio_d--;
    }
}

/* ---------------------------------------------------------------- entorno del original
   Como el original (course_environment_* de pc_environment_blend.cpp, SKY 451B40 / 451E90 / 4521C0, pase del sol
   40CBC0 de race_course_passes.cpp, SCN_EFC 4AFDC0 / 4AFCC0 y weather_44bd50, en la decompilacion del PC):
   - tres entornos a la vez (40D840(e), elegido por la capa del dibujo: tabla 5A27B4): 0 el cielo, 1 la escena, 2 los
     coches; cada uno con su niebla (scn_env_fog_<zona> lista e) y su luz (scn_env_sun_<zona> lista e). La luz es fija
     en cada etapa (un registro de posicion inicial = final); la niebla cambia con la posicion de carretera dentro de la
     etapa (registros de posicion inicial a final, mezcla lineal: course_environment_progress_44b020). La luz de la
     escena y del cielo va en los colores de los vertices (tools/modelo.py luz_vertices); la de los coches, en la
     pintura (coche_luz). La niebla de la escena va en la tabla de la PowerVR (una sola: los coches llevan la de la
     escena, no la lista 2; el cielo solo lleva niebla si su lista 0 la tiene: CLOU).
   - en la union (car+5C) la niebla y la luz se quedan como al salir de la etapa hasta que el coche pasa la posicion de
     la union del descriptor de la etapa siguiente (+0x58: 100 en las 15, unos 807 m) con la etapa siguiente elegida
     (y aqui leida); entonces se mezclan por tiempo (+0x58 + 2: 2 s, 120 cuadros: course_environment_phase_44a000 /
     blend_44ab10) hacia los valores del principio de la etapa nueva, y el cielo cambia en tres tercios (451E90): el
     cielo viejo (objeto +0x4C/+0x50 del descriptor, grupo 0) se funde a su objeto +0x54 (grupo 1), este al +0x54 del
     nuevo y este al +0x4C/+0x50 del nuevo.
   - el sol con destello (40CBC0) si la etapa lo tiene (bandera 2 de +0x3C): aparece y se va en 120 cuadros en la union
     (4AFCC0). Sin lluvia ni nieve en las 15 etapas (bandera 1 de +0x3C en ninguna: la lluvia de SCN_EFC es la unica del
     original). */
static float niebla_d_hecha = -1, niebla_lejos_hecho; static int niebla_modo_hecho; static uint32_t niebla_c, niebla_c_hecho;
static float niebla_cielo_d;                                    /* lista 0 (cielo) en la posicion del coche */
static void niebla_de_lista(const niebla_t *lst, uint32_t nl, float pos, float *d, int *modo, uint32_t *c)
{
    uint32_t i; const niebla_t *n = NULL;
    *d = 0; *modo = 0; *c = 0;
    for (i = 0; i < nl; i++) {
        n = lst + i;
        if (pos <= n->p1 || i + 1 == nl) break;
    }
    if (!n) return;
    {
        float t = n->p1 > n->p0 ? (pos - n->p0) / (float)(n->p1 - n->p0) : 0; int k; uint32_t col = 0xFF000000u;
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        *d = n->d0 + t * (n->d1 - n->d0); *modo = n->modo;
        for (k = 0; k < 24; k += 8) col |= (uint32_t)((((n->c0 >> k) & 0xff) * (1 - t) + ((n->c1 >> k) & 0xff) * t)) << k;
        *c = col;
    }
}
static void niebla_de(const etapa_t *e, float pos, float *d, int *modo, uint32_t *c)
{
    niebla_de_lista(e->niebla, e->nniebla, pos, d, modo, c);
}
/* estado de la transicion de entorno en la union */
static int ent_fase;                                            /* 0 en la etapa, 1 mezclando, 2 ya la nueva */
static int ent_nodo = -1, ent_nuevo = -1, ent_t, ent_n;         /* etapa de referencia, la nueva; cuadros que quedan */
static float ent_dur;                                           /* duracion (cuadros x 60,2 / 60: 7D28D8) */
static float ent_d0; static int ent_m0; static uint32_t ent_c0; /* niebla guardada al empezar (7D28E0) */
static int cie_b8, cie_ba, cie_2a, cie_tercio;                  /* cuentas de los tres tercios del cielo (451E90) */
static float cie_a[4]; static int cie_on[4];                    /* viejo 0, viejo 1, nuevo 0, nuevo 1: alfa y encendido */
static float sol_int;                                           /* intensidad del sol (SCN_EFC +9C) */
static void entorno_reinicia(void)
{
    ent_fase = 0; ent_nodo = cur_nodo; ent_nuevo = -1;
    sol_int = cur_nodo >= 0 && (curso[cur_nodo].clima & 2) ? 1.0f : 0.0f;   /* al empezar: sin fundido (4AFCC0) */
}
/* la niebla, la transicion de la union, el cielo y el sol (cada paso de logica) */
static void niebla_paso(void)
{
    float d0, d1, t, d; int m0, m1, k; uint32_t c0, c1;
    if (cur_nodo < 0) return;
    if (cur_nodo != ent_nodo) { ent_nodo = cur_nodo; ent_fase = 0; }   /* ya en la etapa nueva */
    if (ent_fase == 0 && cur_sig >= 0 && s_union >= 0 && curso[cur_nodo].trans_m >= 0 && sig.tv &&
        yo.s >= s_union + curso[cur_nodo].trans_m) {           /* 44A000 fase 1 -> 2 y 451E90 estado 0 -> 1 */
        ent_fase = 1; ent_nuevo = cur_sig;
        ent_n = ent_t = curso[cur_nodo].trans_seg * 60; ent_dur = curso[cur_nodo].trans_seg * 60.2f;
        niebla_de(curso + cur_nodo, (yo.s - s_etapa) / curso[cur_nodo].m_pos, &ent_d0, &ent_m0, &ent_c0);
        cie_tercio = ent_n / 3; cie_b8 = cie_ba = cie_2a = cie_tercio;      /* 451DD0 */
        cie_a[0] = cie_a[1] = cie_a[2] = cie_a[3] = 1; cie_on[0] = cie_on[1] = 1; cie_on[2] = cie_on[3] = 0;
        dbglog(DBG_INFO, "ENTORNO: transicion %.4s -> %.4s en s %.0f (%d cuadros)\n", curso[cur_nodo].zona, curso[cur_sig].zona, (double)yo.s, ent_n);
    }
    if (ent_fase == 1) {
        if (--ent_t < 0) ent_fase = 2;
        if (cie_tercio > 0) {                                   /* 451E90: un tercio tras otro */
            if (cie_b8 > 0) { cie_a[0] = (float)cie_b8 / cie_tercio; if (--cie_b8 <= 0) { cie_a[0] = 0; cie_on[0] = 0; cie_on[1] = 1; cie_on[3] = 1; } }
            else if (cie_ba > 0) { cie_a[1] = (float)cie_ba / cie_tercio; if (--cie_ba <= 0) { cie_a[1] = 0; cie_on[1] = 0; cie_on[3] = 1; cie_on[2] = 1; } }
            else if (cie_2a > 0) { cie_a[3] = (float)cie_2a / cie_tercio; if (--cie_2a <= 0) { cie_a[3] = 0; cie_on[3] = 0; cie_on[2] = 1; } }
        }
    }
    {   /* el sol: aparece o se va en 120 cuadros, solo en la union (4AFCC0, paso 5C518C) */
        int area = ent_fase ? ent_nuevo : cur_nodo, on = area >= 0 && (curso[area].clima & 2);
        if (s_union >= 0 && yo.s > s_union) {
            if (on && sol_int < 1) sol_int = fminf(1.0f, sol_int + 0.00830564741f);
            if (!on && sol_int > 0) sol_int = fmaxf(0.0f, sol_int - 0.00830564741f);
        }
    }
    niebla_de(curso + cur_nodo, (yo.s - s_etapa) / curso[cur_nodo].m_pos, &d0, &m0, &c0);
    {   int mc; uint32_t cc;
        niebla_de_lista(curso[cur_nodo].niebla0, curso[cur_nodo].nniebla0, (yo.s - s_etapa) / curso[cur_nodo].m_pos, &niebla_cielo_d, &mc, &cc); }
    if (ent_fase) {                                             /* blend_44ab10: guardada x t + nueva x (1 - t) */
        niebla_de(curso + ent_nuevo, 0, &d1, &m1, &c1);
        t = ent_fase == 1 && ent_t > 0 ? ent_t / ent_dur : 0;
        d = ent_d0 * t + d1 * (1 - t); c0 = 0xFF000000u;
        for (k = 0; k < 24; k += 8) c0 |= (uint32_t)(((ent_c0 >> k) & 0xff) * t + ((c1 >> k) & 0xff) * (1 - t)) << k;
        d0 = d; m0 = m1;
        if (ent_fase == 2) niebla_cielo_d = curso[ent_nuevo].nniebla0 ? curso[ent_nuevo].niebla0[0].d0 : 0;
    }
    niebla_d = d0; niebla_modo = m0; niebla_c = c0;
}
/* direccion (hacia donde va) de la luz e de la etapa k en el mundo: (0, 0, -1) girada en x por la inclinacion y en y
   por el rumbo (44A430), en el marco de la etapa (M) */
static void luz_dir(int k, int e, const float *M, float *d)
{
    float p = curso[k].luz[e][12] * (3.14159265f / 180.0f), y = curso[k].luz[e][13] * (3.14159265f / 180.0f);
    float x = -cosf(p) * sinf(y), z = -cosf(p) * cosf(y), c = cosf(M[3]), sn = sinf(M[3]);
    d[1] = sinf(p); d[0] = x * c + z * sn; d[2] = -x * sn + z * c;
}
/* angulos de un vector (44A480): inclinacion y rumbo */
static void luz_angulos(const float *v, float *p, float *y)
{
    float h = sqrtf(v[0] * v[0] + v[2] * v[2]);
    *p = atan2f(v[1], h > 1.1920929e-7f ? h : 1.1920929e-7f); *y = atan2f(-v[0], -v[2]);
}
/* luz e del momento (la de la etapa; en la transicion, mezclada como blend_44ab10): valores y direccion */
static void luz_actual(int e, float *v, float *d)
{
    int k;
    if (cur_nodo < 0) { for (k = 0; k < 16; k++) v[k] = 1; d[0] = 0; d[1] = -1; d[2] = 0; return; }
    memcpy(v, curso[cur_nodo].luz[e], 16 * sizeof(float));
    luz_dir(cur_nodo, e, marco_et, d);
    if (ent_fase && ent_nuevo >= 0) {
        float t = ent_fase == 1 && ent_t > 0 ? ent_t / ent_dur : 0, dn[3], p0, y0, p1, y1;
        for (k = 0; k < 16; k++) v[k] = v[k] * t + curso[ent_nuevo].luz[e][k] * (1 - t);
        luz_dir(ent_nuevo, e, marco_sig, dn);
        luz_angulos(d, &p0, &y0); luz_angulos(dn, &p1, &y1);
        p0 = p0 * t + p1 * (1 - t); y0 = y0 * t + y1 * (1 - t);
        d[0] = -cosf(p0) * sinf(y0); d[1] = sinf(p0); d[2] = -cosf(p0) * cosf(y0);
    }
}
/* tabla de niebla de la PowerVR (129 entradas, la 0 la mas lejana; la entrada i esta a lejos / t(i), con
   t(i) = 2^(i >> 4) (16 + (i & 15)) / 16, la escala de pvr_fog_table_linear de KallistiOS) */
static void niebla_pon(void)
{
    static float tab[129]; int i;
    if (niebla_d == niebla_d_hecha && niebla_modo == niebla_modo_hecho && niebla_c == niebla_c_hecho && lejos == niebla_lejos_hecho) return;
    niebla_d_hecha = niebla_d; niebla_modo_hecho = niebla_modo; niebla_c_hecho = niebla_c; niebla_lejos_hecho = lejos;
    if (niebla_d > 0) pvr_fog_table_color(1.0f, ((niebla_c >> 16) & 0xff) / 255.0f, ((niebla_c >> 8) & 0xff) / 255.0f, (niebla_c & 0xff) / 255.0f);
    else pvr_fog_table_color(1.0f, 0.55f, 0.70f, 0.92f);
    for (i = 0; i < 129; i++) {
        float t = i ? (float)(1 << (i >> 4)) * (16 + (i & 15)) / 16.0f : 1.0f, z = lejos / t, f, g;
        f = (z - lejos * 0.55f) / (lejos * 0.45f); f = f < 0 ? 0 : f > 1 ? 1 : f;
        if (lejos >= LEJOS_PS2) f = 0;                          /* la del original: sin fundido (no lo tiene) */
        g = niebla_d * z; g = niebla_d <= 0 ? 0 : niebla_modo == 2 ? 1 - expf(-g * g) : 1 - expf(-g);
        tab[i] = 1 - (1 - f) * (1 - g);
    }
    pvr_fog_far_depth(lejos);
    pvr_fog_table_custom(tab);
}
/* ---------------------------------------------------------------- cielo y sol (ver "entorno del original")
   Cielo: las mallas de la lista 3 llevan su grupo en el campo del nodo (-256 - grupo; tools/conv_tramo.py pon_cielo).
   El original lo dibuja sin z, antes de todo (4521C0, estado 7 = 0); aqui va en la escena a su distancia (lo tapa
   todo lo de delante). Un objeto con alfa >= 0,9 se dibuja entero (452080); con menos, en la lista transparente con
   el alfa del vertice (un 2 % mas cerca que el que tapa, para que la prueba de z no lo quite). */
static pvr_poly_hdr_t cie_tr_cab[8]; static pvr_ptr_t cie_tr_dir[8]; static int cie_tr_n;
static const pvr_poly_hdr_t *cielo_cab_tr(const modelo_t *mo, int i, int niebla)
{
    pvr_poly_cxt_t cx; const tex_t *t = mo->texs + i; int k;
    uint32_t fb = t->fmt & 0xff, vq = t->fmt & 0x100, n = ((t->fmt >> 16) & 0xff) + 1;
    pvr_ptr_t dir = vq ? (pvr_ptr_t)((uintptr_t)mo->tv[i] - (2048 - 8 * n)) : mo->tv[i];
    for (k = 0; k < cie_tr_n; k++) if (cie_tr_dir[k] == (pvr_ptr_t)((uintptr_t)dir + niebla)) return cie_tr_cab + k;
    if (cie_tr_n == 8) cie_tr_n = 0;                            /* pocas texturas de cielo: se reaprovecha */
    k = cie_tr_n++;
    pvr_poly_cxt_txr(&cx, PVR_LIST_TR_POLY, FMT[fb] | PVR_TXRFMT_TWIDDLED | (vq ? PVR_TXRFMT_VQ_ENABLE : 0), t->w, t->h, dir,
                     PVR_FILTER_BILINEAR);
    cx.gen.fog_type = niebla ? PVR_FOG_TABLE : PVR_FOG_DISABLE; cx.gen.culling = PVR_CULLING_NONE;
    cx.txr.env = PVR_TXRENV_MODULATEALPHA; cx.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(cie_tr_cab + k, &cx);
    cie_tr_dir[k] = (pvr_ptr_t)((uintptr_t)dir + niebla);
    return cie_tr_cab + k;
}
/* el grupo g del cielo de mo en la lista l con alfa a */
static void cielo_grupo(const modelo_t *mo, int g, int l, float a, int niebla)
{
    static matrix_t vc __attribute__((aligned(32)));
    uint32_t k; int j;
    if (!mo || !mo->tv || a <= 0.1f || (a < 0.9f && l != 2)) return;
    if (a < 0.9f) {                                             /* un poco mas cerca que el de debajo */
        memcpy(vc, vista_cielo, sizeof vc);
        for (j = 0; j < 4; j++) { vc[0][j] *= 0.98f; vc[1][j] *= 0.98f; vc[2][j] *= 0.98f; }
        mat_load(&vc); color_mul = ((uint32_t)(a * 255) << 24) | 0xFFFFFF;
    }
    for (k = mo->inicio[3]; k < mo->inicio[4]; k++) {
        const malla_t *m = mo->mallas + k; int gm = m->nodo <= -256 ? -256 - m->nodo : 0;
        if (m->n < 3 || gm != g) continue;
        if (a >= 0.9f) {
            if ((m->tex >= 0 ? (int)mo->texs[m->tex].lista : 0) != l) continue;
            if (m->tex >= 0) pvr_prim(&mo->cabs[!niebla][m->tex], sizeof(pvr_poly_hdr_t));
            else pvr_prim(&cab_col[!niebla][l], sizeof(pvr_poly_hdr_t));
        } else {
            if (m->tex >= 0) pvr_prim(cielo_cab_tr(mo, m->tex, niebla), sizeof(pvr_poly_hdr_t));
            else pvr_prim(&cab_col[!niebla][2], sizeof(pvr_poly_hdr_t));
        }
        dibuja_malla(mo, m);
    }
    color_mul = 0; mat_load(&vista_cielo);
}
/* el cielo del cuadro en la lista l (como 4521C0: primero el nuevo, grupo 0 y 1; luego el viejo, grupo 1 y 0) */
static void cielo_dibuja(int l)
{
    int niebla = niebla_cielo_d > 0;                           /* la lista 0 de la niebla (solo CLOU la tiene) */
    const modelo_t *nuevo = sig.tv ? &sig : NULL;
    mat_load(&vista_cielo);
    if (ent_fase == 0) cielo_grupo(cielo, 0, l, 1, niebla);
    else if (ent_fase == 2) cielo_grupo(nuevo ? nuevo : cielo, 0, l, 1, niebla);
    else {
        if (cie_on[2]) cielo_grupo(nuevo, 0, l, cie_a[2], niebla);
        if (cie_on[3]) cielo_grupo(nuevo, 1, l, cie_a[3], niebla);
        if (cie_on[1]) cielo_grupo(cielo, 1, l, cie_a[1], niebla);
        if (cie_on[0]) cielo_grupo(cielo, 0, l, cie_a[0], niebla);
    }
}

/* Sol y destello (40CBC0, con SCN_EFC +0 & 2 y su intensidad sol_int): objetos del banco 0x57 (SOL.DAT, tools/
   conv_union.py sol_dat; el registro en el campo del nodo) en el espacio de la camara del original (x derecha, y arriba,
   z atras), sumados a lo de detras (sus texturas son de fondo negro: PROBABLE) y sin niebla. El sol esta en -direccion
   de la luz 1 (la de la escena) con la inclinacion por cam+358 (las etapas < 15). Su parte visible la da en el original
   una consulta de oclusion del cuadro anterior (95AEF8, con tope 0,22): la PowerVR no tiene consultas de oclusion y
   aqui va siempre entero (0,22; DIFERENCIA: el destello no se apaga detras de lo que tapa el sol). Lo de lejos (0x10 y
   0x0F) va a 1000 m en vez de 2500 (el cielo de la demo esta en la escena, a 1500-1800 m, y lo taparia) con la prueba
   de z: lo tapa lo que haya delante. */
static modelo_t sol_m; static pvr_poly_hdr_t *sol_cab[2];      /* [0] sin z, [1] con la prueba de z */
static int sol_ok;
static void sol_carga(void)
{
    uint32_t i, z;
    if (lee_modelo(&sol_m, "SOL.DAT") || sube_modelo(&sol_m)) { libera_modelo(&sol_m); return; }
    compacta_modelo(&sol_m);
    for (z = 0; z < 2; z++) {
        sol_cab[z] = memalign(32, (sol_m.ntex + 1) * sizeof(pvr_poly_hdr_t));
        for (i = 0; i < sol_m.ntex; i++) {
            pvr_poly_cxt_t cx; tex_t *t = sol_m.texs + i; uint32_t fb = t->fmt & 0xff, vq = t->fmt & 0x100, n = ((t->fmt >> 16) & 0xff) + 1;
            pvr_ptr_t dir = vq ? (pvr_ptr_t)((uintptr_t)sol_m.tv[i] - (2048 - 8 * n)) : sol_m.tv[i];
            pvr_poly_cxt_txr(&cx, PVR_LIST_TR_POLY, FMT[fb] | PVR_TXRFMT_TWIDDLED | (vq ? PVR_TXRFMT_VQ_ENABLE : 0), t->w, t->h, dir,
                             PVR_FILTER_BILINEAR);
            cx.gen.fog_type = PVR_FOG_DISABLE; cx.gen.culling = PVR_CULLING_NONE; cx.txr.env = PVR_TXRENV_MODULATEALPHA;
            cx.blend.src = PVR_BLEND_SRCALPHA; cx.blend.dst = PVR_BLEND_ONE; cx.depth.write = PVR_DEPTHWRITE_DISABLE;
            if (!z) cx.depth.comparison = PVR_DEPTHCMP_ALWAYS;
            pvr_poly_compile(&sol_cab[z][i], &cx);
        }
    }
    sol_ok = 1;
}
/* el objeto r de SOL.DAT en el punto p del espacio de la camara del original, con escala (sx, sy, sz) y alfa k */
static void sol_objeto(int r, const float *p, float sx, float sy, float sz, float k, uint32_t tinte, int z)
{
    static matrix_t M __attribute__((aligned(32)));
    uint32_t i; int a = (int)(k * 255);
    if (a <= 0 || p[2] > -0.05f) return;
    if (a > 255) a = 255;
    memset(M, 0, sizeof M);
    M[0][0] = sx * fx; M[1][1] = -sy * fx;
    M[2][0] = -320.0f * sz; M[2][1] = -240.0f * sz; M[2][2] = M[2][3] = -sz;
    M[3][0] = fx * p[0] - 320.0f * p[2]; M[3][1] = -fx * p[1] - 240.0f * p[2]; M[3][2] = M[3][3] = -p[2];
    mat_load(&M); color_mul = ((uint32_t)a << 24) | (tinte & 0xFFFFFF);
    for (i = 0; i < sol_m.nmallas; i++) {
        const malla_t *m = sol_m.mallas + i;
        if (m->nodo != r || m->n < 3 || m->tex < 0) continue;
        pvr_prim(&sol_cab[z][m->tex], sizeof(pvr_poly_hdr_t));
        dibuja_malla(&sol_m, m);
    }
    color_mul = 0;
}
/* 40CBC0 (en la lista transparente, al final de la escena) */
static void sol_dibuja(void)
{
    float v[16], d[3], P[3], sx, sy, ax[3], fov, size, spread, bright, edge, flare, ring, facing, vis = 0.22f, p0, y0, q[3];
    float wv[3], lw, g0[3] = { 0, 0, -0.2f }, far = 1000.0f; uint32_t tinte; int k;
    if (!sol_ok || sol_int <= 0) return;
    luz_actual(1, v, d);
    luz_angulos(d, &p0, &y0);                                   /* 44A480 y la inclinacion por cam+358 (etapa < 15) */
    if (cam_ok) p0 *= cam.f358;
    d[0] = -cosf(p0) * sinf(y0); d[1] = sinf(p0); d[2] = -cosf(p0) * cosf(y0);
    for (k = 0; k < 3; k++) q[k] = -d[k];                       /* del ojo al sol */
    P[0] = q[0] * der[0] + q[1] * der[1] + q[2] * der[2];        /* en la camara del original */
    P[1] = q[0] * arr[0] + q[1] * arr[1] + q[2] * arr[2];
    P[2] = -(q[0] * ade[0] + q[1] * ade[1] + q[2] * ade[2]);
    if (P[2] >= 0) return;
    sx = fx * P[0] / -P[2]; sy = fx * P[1] / -P[2];              /* 449940: en la pantalla de 640 x 480 */
    if (!(sx > -470 && sx < 470 && sy > -440 && sy < 440)) return;
    fov = 2 * atanf(240.0f / fx) * 57.29578f * 0.01f;           /* cam+A0 en grados / 100 */
    size = fov; spread = fov * 0.13f;
    bright = 0.99f;                                             /* se apaga hacia los bordes de la pantalla */
    if (sx < -100) bright = (sx + 470) * 0.0027027f; else if (sx > 100) bright = (470 - sx) * 0.0027027f;
    if (sy < -100) bright = (sy + 520) * bright * 0.00294118f; else if (sy > 100) bright = (520 - sy) * bright * 0.00294118f;
    edge = bright; flare = bright * 0.65f;
    {   float lq = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
        facing = (ade[0] * q[0] + ade[1] * q[1] + ade[2] * q[2]) / (lq > 0 ? lq : 1); }
    ax[0] = P[0] / -P[2]; ax[1] = P[1] / -P[2]; ax[2] = -1;    /* el sol en el plano z = -1 */
    {   float a0 = curso[cur_nodo].luz[0][9], a1 = curso[cur_nodo].luz[0][10], a2 = curso[cur_nodo].luz[0][11];   /* 899C10 */
        tinte = 0xFF000000u | (uint32_t)(fminf(a0, 1) * 255) << 16 | (uint32_t)(fminf(a1, 1) * 255) << 8 | (uint32_t)(fminf(a2, 1) * 255); }
    sol_objeto(2, ax, vis * 0.1f, vis * 0.1f, vis * 0.1f, sol_int * vis, tinte, 0);   /* el disco */
    {   float at[3] = { far * ax[0], far * ax[1], far * ax[2] }, e = far * spread;   /* lo de lejos, con z */
        sol_objeto(0x10, at, e, e, e, sol_int, tinte, 1); sol_objeto(0x0f, at, e, e, e, sol_int, tinte, 1); }
    sol_objeto(6, g0, bright * 0.1f, bright * 0.1f, bright * 0.1f, sol_int * bright * vis, tinte, 0);   /* el brillo */
    for (k = 0; k < 3; k++) wv[k] = g0[k] - ax[k];              /* el eje de los destellos (40CAE0) */
    lw = sqrtf(wv[0] * wv[0] + wv[1] * wv[1] + wv[2] * wv[2]);
    ring = edge < 0.4f ? edge : 0.4f;                           /* las etapas 0x14, 0x19 y 0x1A (otro modo) no */
    if (facing > 0.92f && ring != 0) ring += (facing - 0.92f) * 6.8f;
    if (ring > 0.999f) ring = 0.999f;
    spread = lw * facing * 0.4f; size *= 0.4f;
    flare *= vis; bright *= vis;
    {   static const struct { int r; float a, t; int k; } F[] = {   /* objeto, posicion en el eje, tamano, alfa */
            { 0x0e, -1.7f, 0.18f, 0 }, { 3, -0.52f, 0.15f, 0 }, { 4, -0.47f, 0.15f, 0 }, { 5, -0.15f, 0.15f, 0 },
            { 7, 0.12f, 0.1f, 1 }, { 8, 0.16f, 0.09f, 0 }, { 0x0a, 0.17f, 0.06f, 0 }, { 9, 0.18f, 0.11f, 0 },
            { 0x0b, 0.21f, 0.12f, 0 }, { 0x0c, 0.27f, 0.05f, 0 }, { 0x0d, 0.3f, 0.03f, 0 } };
        uint32_t i;
        for (i = 0; i < sizeof F / sizeof F[0]; i++) {
            float al = spread * F[i].a / (lw > 0 ? lw : 1), p[3], t = size * F[i].t;
            for (k = 0; k < 3; k++) p[k] = g0[k] + al * wv[k];
            sol_objeto(F[i].r, p, t, t, 1, sol_int * (F[i].k ? bright : flare), tinte, 0);
        }
        {   float p[3]; for (k = 0; k < 3; k++) p[k] = g0[k] - 0.003f * wv[k] / (lw > 0 ? lw : 1);
            sol_objeto(8, p, 1.5f, 1.5f, 1, sol_int * ring * vis, tinte, 0); }   /* el brillo de cara al sol */
    }
    mat_load(&vista);
}
/* ---------------------------------------------------------------- puntuacion del modo OutRun
   Del original (PS2 0x28F8C8 / PC 4F2DF0 cada cuadro; 4F2860 adelantamientos; 4F2CD0 estadisticas; 4F2B20 bonus de
   etapa; 4B9770 suma y cola de mensajes; 4BD480 dibujo). puntos = [8447DC] del jugador. La posicion de la
   carretera (+64, +18C la del cuadro anterior): una cada m_pos metros (la seccion de la colision del original). Para
   los puntos por avance se acumula (pt_pos_f), asi que el cambio de m_pos de una etapa a otra no da un salto; en el
   original la posicion vuelve a 0 en cada curso (etapa, union) y |+64 - +18C| ahi no se ha comprobado (PENDIENTE). */
#define PT_MSGS 10
typedef struct { int fila, tipo, pts, edad, y, estilo; float alfa; } pt_msg_t;   /* 8444C8 (0x24 bytes) */
static pt_msg_t pt_msg[PT_MSGS];
static int pt_pos_ant, pt_odo_pos, pt_cifra, pt_estilo, pt_crono0;
static float pt_odo;
static int pt_est[7];                                           /* 84DF1C..84DF34 */
static float pt_pos_f, pt_s_ant;                                /* posicion acumulada y s del paso anterior */
static int pt_pos(void) { return (int)floorf(pt_pos_f); }
/* 4B9770(coche, otro, puntos, tipo): suma y, para los tipos 1..4 y 6, un mensaje en el primer hueco de los diez.
   estilo de las cifras del mensaje (+18): 0 normal (1 y 0x11), 1 rebufo (0xC, 0xE), 2 tocado (0xD, 0xF) */
static void pt_suma(int pts, int tipo, int estilo)
{
    int k, j, vivos = 0, viejos = 0; pt_msg_t *r;
    puntos += pts;
    if (tipo <= 0 || (tipo > 4 && tipo != 6)) return;
    for (k = 0; k < PT_MSGS && pt_msg[k].edad > 0; k++) ;
    if (k == PT_MSGS) return;
    for (j = 0; j < PT_MSGS; j++) if (pt_msg[j].edad > 0) { vivos++; if (pt_msg[j].edad >= 0x8c) viejos++; }
    r = pt_msg + k;
    r->fila = vivos; r->y = (vivos + 1) * 0x1a; r->tipo = tipo; r->pts = pts; r->edad = viejos + 0x8c; r->alfa = 0;
    r->estilo = estilo;
}
static void pt_reinicia_est(void) { memset(pt_est, 0, sizeof pt_est); pt_crono0 = crono; }
static void pt_inicia(void)
{
    puntos = 0; memset(pt_msg, 0, sizeof pt_msg); pt_reinicia_est();
    pt_pos_f = yo.s / m_pos; pt_s_ant = yo.s;
    pt_pos_ant = pt_odo_pos = pt_pos(); pt_odo = 0; pt_cifra = 0; pt_estilo = 0;
}
/* 4F2B20(segundos de la etapa): bonus de fin de etapa (tiempo por debajo de 60 s, conduccion limpia, rebufo) */
static void pt_bonus_etapa(void)
{
    int t = (int)((float)(crono - pt_crono0) / 60.2f), b = t < 60 ? (60 - t) * 1000 : 0;
    if (!pt_est[0]) b += 2500;                                  /* sin choques de estado 1 */
    if (!pt_est[1]) b += 2500;                                  /* sin choques de estados 2, 5, 6, 12, 13 */
    if (!pt_est[2]) b += 5000;                                  /* sin tocar el muro (+4 0x20000) */
    if (!pt_est[3]) b += 10000;                                 /* sin salirse del asfalto (+244 bit 2) */
    if (!pt_est[4]) b += 2500;                                  /* sin tocar el trafico (+4 0x10000) */
    if (!pt_est[5]) b += 10000;                                 /* sin golpes con el trafico (+4 0x400000) */
    if (pt_est[6]) b += pt_est[6] * 1000 / 60;                  /* 1000 por segundo de rebufo */
    pt_suma(b - b % 10 + 10, 4, 0);
    pt_reinicia_est();
}
/* un cuadro de carrera: 4F2860 (adelantamientos del trafico), 4F2CD0 (estadisticas) y los puntos por avance */
static void pt_paso(void)
{
    int i, pos, gap, rebufo, ang, fuera, py = (int)floorf(yo.s / m_pos);
    pt_pos_f += (yo.s - pt_s_ant) / m_pos; pt_s_ant = yo.s; pos = pt_pos();
    for (i = 0; i < NTRAF; i++) {                               /* 4F2860 */
        trafico_t *t = traf + i; int clase, pts, tipo;
        if (t->carril < 0 || (t->pt_marca & PT_PASADO)) continue;
        if (py - (int)floorf(t->c.s / m_pos) < -2 || t->c.s - yo.s > 0.0f) continue;   /* aun delante */
        clase = trc_ok ? trc[t->modelo].clase : 3;
        if (clase == 5 && py - (int)floorf(t->c.s / m_pos) < 2) continue;
        t->pt_marca |= PT_PASADO;
        if (t->pt_marca & PT_SIN) continue;
        if (clase == 5) { pts = (t->pt_marca & PT_MITAD) ? 12500 : t->b72 > 0 ? 50000 : 25000; tipo = 3; }
        else {
            tipo = 1; pts = 750;
            if (clase < 3) { tipo = 2; pts = 7500; }            /* pesados: autobuses, camiones */
            if (t->pt_marca & PT_MITAD) pts >>= 1; else if (t->b72 > 0) pts <<= 1;
        }
        pt_suma(pts, tipo, (t->pt_marca & PT_MITAD) ? 2 : t->b72 > 0 ? 1 : 0);
    }
    ang = fis_ok ? (int)(fabsf(fis.beta) * 10430.3779f) : 0x7fff;   /* |+162| */
    rebufo = fis_ok && fis.rebufo > 0.180000007f && ang < 1500;
    fuera = (sup_jug & 0x9cu) || yo.choque == 1;                /* +244 & 0x9C: superficies 2, 3, 4, 7 o muro */
    /* 4F2CD0 */
    if (fis_ok && fis.t_choque > 0 && fis.choque >= 1 && fis.choque <= 13) {
        static const uint8_t B[13] = { 0, 1, 2, 2, 1, 1, 2, 2, 2, 2, 2, 1, 1 };
        if (B[fis.choque - 1] < 2) pt_est[B[fis.choque - 1]]++;
    }
    if (!(sup_jug & 2u)) pt_est[3]++;
    if (pt_toca) pt_est[4]++;
    if (yo.choque == 1) pt_est[2]++;
    if (pt_toca & 2) pt_est[5]++;
    if (rebufo) pt_est[6]++;
    pt_toca = 0;
    /* PS2 0x28F984: |+64 - +18C| x 10, x 110 con rebufo por encima de la punta (+1C4 > +E64); nada fuera */
    gap = pos > pt_pos_ant ? pos - pt_pos_ant : pt_pos_ant - pos; pt_pos_ant = pos;
    rebufo = rebufo && fis.vel_mf > fpar.v_tope;
    if (!fuera) pt_suma((int)((float)gap * (rebufo ? 110.0f : 10.0f)), 0, 0);
    pt_estilo = rebufo ? 1 : fuera ? 2 : 0;
    /* 4B96F0: la ultima cifra corre con la velocidad (+1F8 x 0,00915) y vuelve a 0 en cada posicion */
    if (!fuera) {
        pt_odo += (fis_ok ? fis.velocidad_hud : yo.v * 3.6f) * 0.00915000029f;
        if (pt_odo_pos != pos) { pt_odo_pos = pos; pt_odo = 0; }
    }
    pt_cifra = (int)pt_odo % 10;
}
/* 4BD480: la cola de mensajes (orden, y, alfa y edad); una vez por cuadro fuera de la pausa */
static void pt_msg_paso(void)
{
    int k, j;
    for (k = 0; k < PT_MSGS; k++) {
        pt_msg_t *r = pt_msg + k; int ord = 0;
        if (r->edad <= 0) continue;
        for (j = 0; j < PT_MSGS; j++) if (j != k && pt_msg[j].edad > 0 && pt_msg[j].edad <= r->edad) ord++;
        r->fila = ord;
        if (r->edad < 10) r->y = (int)((float)r->y * 0.666666687f);
        else { int t = (ord + 1) * 0x1a; r->y = t - (int)((float)(r->y - t) * -0.666666687f); }
        if (r->edad < 60) {
            if (r->edad >= 50) r->alfa += 0.100000001f;
            if (r->edad < 10) r->alfa *= 0.800000012f;
        }
        r->edad--;
    }
}
/* lo que muestra el HUD: el total mas la cifra que corre, menos los puntos de los mensajes que aun estan en cola */
static int pt_visible(void)
{
    int k, v = puntos + pt_cifra;
    for (k = 0; k < PT_MSGS; k++) if (pt_msg[k].edad > 0) v -= pt_msg[k].pts;
    return v;
}
static void texto_arriba(const char *l1, const char *l2, const char *l3);
/* panel de resultados al terminar (meta o tiempo agotado) */
static void resultados(int meta)
{
    char a[64], b[64], c[64]; int ms = (int)(crono * 1000.0f / 60.0f);
    sprintf(a, meta ? "   GOAL!   %s" : "   TIEMPO AGOTADO", meta ? zona_nombre(cur_nodo) : "");
    sprintf(b, "   TIEMPO TOTAL %d'%02d\"%03d", ms / 60000, (ms / 1000) % 60, ms % 1000);
    sprintf(c, "   PUNTOS %07d   MAX %d km/h", puntos, (int)vmax);
    texto_arriba(a, b, c);
}                                             /* 0: la primera; 1: ya en una rama */
#define TIEMPO_ETAPA 80.0f
#ifndef VER
#define VER 0
#endif
#ifndef SALIDA
#define SALIDA 100.0f                                           /* metros desde el principio de la ruta */
#endif

/* La PowerVR no termina el cuadro (en la consola real, por un bufer lleno o un parametro ilegal): se escribe el
   diagnostico directamente en la imagen que se esta viendo, para que el probador lo pueda leer. */
static int pvr_fallos;
static void pvr_colgado(int estado)
{
    char s[96]; uint16_t *fb = (uint16_t *)(0xa5000000 + (PVR_GET(0x0050) & 0x7ffffc)); int i;
    uint32_t err = *(volatile uint32_t *)0xa05f6908;
    for (i = 0; i < 6 * 24 * 640; i++) fb[i] = 0;
    sprintf(s, "PVR COLGADO  estado %d  tri %u  vtx %u  err %08lx", estado, (unsigned)tri_enviados, (unsigned)vtx_ult, (unsigned long)err);
    bfont_draw_str(fb + 8 * 640 + 8, 640, 1, s);
    sprintf(s, "VB %lu/%lu  OPB %lu/%lu  lejos %d", (unsigned long)(PVR_GET(PVR_TA_VERTBUF_POS) - PVR_GET(PVR_TA_VERTBUF_START)),
            (unsigned long)(PVR_GET(PVR_TA_VERTBUF_END) - PVR_GET(PVR_TA_VERTBUF_START)),
            (unsigned long)((PVR_GET(PVR_TA_OPB_POS) << 2) - PVR_GET(PVR_TA_OPB_START)),
            (unsigned long)(PVR_GET(PVR_TA_OPB_END) - PVR_GET(PVR_TA_OPB_START)), (int)lejos);
    bfont_draw_str(fb + 36 * 640 + 8, 640, 1, s);
    sprintf(s, "fallos seguidos %d (haz una foto y mandala)", pvr_fallos);
    bfont_draw_str(fb + 64 * 640 + 8, 640, 1, s);
}

/* ---------------------------------------------------------------- coches del jugador
   COCHES.DAT dice que fichas tienen coche; sin el, el COCHE.DAT de siempre como Testarossa (ficha 9, modelo 6).
   Devuelve la mascara de fichas con coche. */
static unsigned coches_lee(void)
{
    uint32_t sz, i; unsigned m = 0; uint8_t *b = gd_carga("COCHES.DAT", &sz);
    memset(ficha_modelo, 0xFF, sizeof ficha_modelo);
    if (b && sz >= 4 + N_FICHAS && !memcmp(b, "CCH1", 4)) memcpy(ficha_modelo, b + 4, N_FICHAS);
    free(b);
    for (i = 0; i < N_FICHAS; i++) if (ficha_modelo[i] < 30) m |= 1u << i;
    return m;
}
/* Pintura, color elegido, reflejo y luces del coche (bloque CEX2 de tools/conv_coche.py, en el sitio de la ruta, que
   los coches no usan). Del original: los colores 1..8 de la lista de colores del modelo (texturas de 8x8 de un color de
   \COMMON\obj_pc_color_pmt.sz o del coche; PC 64D120 / 408C80, car+0x12 = color elegido + 1) y el mapa de entorno
   esferico de cada coche. Mallas especiales: tipo 1, la pintura es el color del vertice (pintura x luz); tipo 2 (3:
   ademas sumada a lo de detras, cristales),
   textura = mapa de entorno con UV calculadas cada cuadro por la normal en el espacio de la camara, color base =
   especular del material y color de desplazamiento = pintura (o el difuso del material) x luz: pintura + reflejo en
   una sola pasada (sin vertices de mas; el TA guarda 4 bytes mas por vertice con desplazamiento). Grupos: las luces
   que el original cambia (coche_luces). */
typedef struct { uint32_t malla, tipo; int32_t ranura; uint32_t fijo, v0; } cex_t;
/* volumenes de sombra del coche (bloque SVL1; sombra_dibuja): vertices en el marco del coche y aristas de 12 bytes
   {u16 a, b; s8 normal de la primera cara x 127, u8 caras (1 o 2); s8 normal de la segunda, 0} */
typedef struct { uint32_t nv, ne; const float *v; const uint8_t *e; } svol_t;
static svol_t svol[8]; static int nsvol;
static const uint32_t *cex_col, *cex_vd, *cex_grp; static const cex_t *cex_m; static uint32_t cex_nsl, cex_n, cex_ngrp;
static uint32_t *cex_n0; static int cex_color, cex_luces = -1;   /* cuenta de vertices de cada malla; grupos encendidos */
static uint32_t cex_mul(uint32_t c, uint32_t luz)
{
    return 0xFF000000u | ((((c >> 16) & 255) * luz / 255) << 16) | ((((c >> 8) & 255) * luz / 255) << 8) | ((c & 255) * luz / 255);
}
/* colores de pintura del color c (1..8) */
/* luz del coche en carrera (coche_luz): la del entorno 2 del original (scn_env_sun lista 2; 40D840(2) en 46BD30) en
   el marco del modelo; cex_luz_on = 0: la luz de tools/conv_coche.py (menu) */
static float cex_L[3], cex_lv[12]; static int cex_luz_on;
static void coche_color(int c)
{
    uint32_t k, i;
    if (c < 1 || c > 8) c = 1;
    for (k = 0; k < cex_n; k++) {
        const cex_t *e = cex_m + k; const malla_t *m = coche.mallas + e->malla;
        uint32_t p = e->ranura >= 0 && (uint32_t)e->ranura < cex_nsl ? cex_col[8 * e->ranura + c - 1] : e->fijo;
        for (i = 0; i < cex_n0[e->malla]; i++) {                /* tambien las luces apagadas (n = 0) */
            uint32_t q = cex_vd[e->v0 + i], col;
            if (cex_luz_on) {                                   /* tools/modelo.py luz_vertices (difuso y ambiente 1) */
                float nl = ((int8_t)q * cex_L[0] + (int8_t)(q >> 8) * cex_L[1] + (int8_t)(q >> 16) * cex_L[2]) * (1.0f / 127.0f), lr[3]; int j;
                for (j = 0; j < 3; j++) {
                    lr[j] = cex_lv[j] * (nl > 0 ? nl : 0) + cex_lv[9 + j] + (cex_lv[6 + j] - cex_lv[9 + j]) * (1 + nl) * 0.5f;
                    lr[j] = lr[j] < 0 ? 0 : lr[j] > 1 ? 1 : lr[j];
                }
                col = 0xFF000000u | (uint32_t)(((p >> 16) & 255) * lr[0]) << 16 | (uint32_t)(((p >> 8) & 255) * lr[1]) << 8 | (uint32_t)((p & 255) * lr[2]);
            } else col = cex_mul(p, q >> 24);
            if (e->tipo == 1) coche.verts[m->v0 + i].c = (coche.verts[m->v0 + i].c & 0xFF000000u) | (col & 0xFFFFFF);
            else coche.ofs[m->v0 + i] = col;
        }
    }
    cex_color = c;
}
/* la luz de los coches (entorno 2) en la pintura del coche del jugador (coche_color): en el marco del modelo (mc,
   filas = ejes del modelo en el mundo); se rehace si la luz cambia algo (como mucho cada 4 cuadros). Las normales del
   fichero apuntan hacia dentro como las de las etapas (tools/modelo.py luz_vertices): n.l = normal . direccion. El
   resto del coche y el trafico llevan la luz de tools/conv_coche.py (PENDIENTE). */
static void coche_luz(matrix_t mc, int on)
{
    static int espera; float v[16], d[3], L[3]; int j, cambia = 0;
    if (!on || cur_nodo < 0) { if (cex_luz_on) { cex_luz_on = 0; coche_color(cex_color); } return; }
    if (espera > 0 && cex_luz_on) { espera--; return; }
    luz_actual(2, v, d);
    for (j = 0; j < 3; j++) L[j] = mc[j][0] * d[0] + mc[j][1] * d[1] + mc[j][2] * d[2];
    for (j = 0; j < 3; j++) cambia |= fabsf(L[j] - cex_L[j]) > 0.02f;
    for (j = 0; j < 12; j++) cambia |= fabsf(v[j] - cex_lv[j]) > 0.004f;
    if (!cambia && cex_luz_on) return;
    memcpy(cex_L, L, sizeof L); memcpy(cex_lv, v, sizeof cex_lv); cex_luz_on = 1; espera = 3;
    coche_color(cex_color);
}
/* luces del coche como 46A560 / 469600 con vm = 1 (tools/conv_coche.py llamadas): car+4 (banderas) & 0x300 (noche o
   tunel, 4A25F0) cambia los pilotos +7C por los encendidos +80; el pedal de freno (car+38) != 0 pone los de +84 (y en
   el Enzo la luz del aleron); +60 (luz de los faros) con noche y faros abiertos (banderas & 0x100 y 0x800: sin
   escamoteables o abiertos del todo: coche_anima_paso). Discos (46AE70): el caliente +94 en vez del +90 con car+38 > 200
   y car+1F4 != 0 (caliente). Apagar = cuenta de vertices 0 (dibuja_rango la salta) */
#define G_PILOTOS 1
#define G_NOCHE 2
#define G_FRENO 3
#define G_FARO 4
#define G_DISCO 5
#define G_DISCO_CAL 6
static void coche_luces(uint32_t banderas, int freno, int caliente)
{
    int pil = freno ? G_FRENO : (banderas & 0x300) ? G_NOCHE : G_PILOTOS, est = pil | ((banderas & 0x900) == 0x900) << 3 |
        (caliente != 0) << 4;
    uint32_t k;
    if (est == cex_luces) return;
    for (k = 0; k < cex_ngrp; k++) {
        uint32_t mk = cex_grp[2 * k], g = cex_grp[2 * k + 1];
        coche.mallas[mk].n = (g == (uint32_t)pil || (g == G_FARO && (est & 8)) || (g == G_DISCO && !(est & 16)) ||
                              (g == G_DISCO_CAL && (est & 16))) ? cex_n0[mk] : 0;
    }
    cex_luces = est;
}
/* Piezas animadas del coche del jugador como 46AE70 / 469600 (bloque ANI1 de tools/conv_coche.py animadas; vertices de
   cada pieza en su marco). Cada cuadro (coche_anima_matrices), en el marco del coche:
     carroceria y lo que va con ella: car+F0 = Rz(balanceo) Rx(cabeceo) T(0, subida, 0) de CheckReverseCar 4A2910;
     volante: T(+4C) Rx(+58) Rz(-car+204 x 2 pi / 65536 x 0,41667) en car+F0; car+204 = media de 30 cuadros de
       -car+32 (direccion) acotada a +-5461 x 6 (4A4710);
     aleron del modelo 5: T(+1C) Rx(car+2FC de roof_angle: car+1C4 x 216,72 x 0,72917 = x; x <= 300: -x / 300 x
       0,30543; si no, min(0, -(350 - x) x 0,02 x 0,30543));
     faros escamoteables: T(+68) Rx(car+2FC): con car+4 & 0x500 suben +78 / +74 por cuadro hasta +78 (entonces car+4
       |= 0x800), si no bajan igual hasta 0;
     rueda k: T(car+130: x y z de +98, y = car+134 = fis.rueda_y de CopyCarWork 4A1140) Rz(caida +A4) Ry(rueda +32 =
       fis.r[k].ang) -> pinza; Rx(rueda +30, que avanza -omega x 65536 / 2 pi por segundo: 4A0000 / 501190) -> buje
       y disco (fuera de car+F0).
     Todas en el cuerpo de la fisica: altura del CG (fis.alt), cabeceo y balanceo (ActionForce2), menos alt_cg (+224).
   CheckReverseCar (4A2910, en GamePlCar_Ctrl por 4A7690): subida = base + base x (0,5 + azar / 32768) x 0,6667 x
   sin(fase x 4096) x (a / 2) con car+1F4 > 100 (a = min(car+1F4 x 0,01 - 1, 4), fase += a, vuelve a 0 al pasar de 255;
   base = car+30C = +1C de 4866C0 / 2); cabeceo = (x < 0 ? +20 t : +24 (1 - t)) x x^2 x 0,40528 acotado a [+20, +24]
   (t = car+1F4 / 480, x = car+1CC acotado a +-pi/2, car+1CC = media de 30 cuadros de 1000 x car+1C8 (4A2D70), car+1C8
   = cambio de car+1C4 en el cuadro); balanceo 0 (solo con car+4 & 1 = 0: el jugador lo tiene). En la seleccion el
   coche esta parado: todo en reposo (4A5B20 llama a 4A2910 y 4A4710 con car+1F4 = 0). */
#define NPZ 12
#define PZ_VOLANTE 1
#define PZ_ALERON 2
#define PZ_FAROS 3
#define PZ_PINZA 4
#define PZ_RUEDA 8
typedef struct {
    int ok; uint32_t modelo; float radio, base, p20, p24, p28, p2c, pz[NPZ][5];
    uint16_t *idx, ini[3][NPZ_MAX + 1];
    float giro[4];                                              /* rueda +30 (rad) */
    int16_t h204[30]; int i204, s204;                           /* 4A4710 */
    float roof, aleron;                                         /* car+2FC: faros; el aleron del modelo 5 */
    float fase, h1cc[30], v_ant; int i1cc;                      /* 4A2910 / 4A2D70 */
    uint32_t azar, ban;                                         /* rand() de la CRT (580F40); car+4 (0x800) */
    int caliente;                                               /* disco +94 */
} ani_t;
static ani_t ani;
static uint32_t coche_ban;                                      /* car+4 de 4A25F0 (0x100 noche, 0x200 tunel, 0x400): 0 */
static matrix_t ani_m[NPZ] __attribute__((aligned(32)));      /* matriz de cada pieza en el marco del coche */
/* lee el CEX2 del coche recien cargado: desplazamiento por vertice, cabeceras con desplazamiento para el entorno y
   grupos de luces */
static void coche_cex(int c)
{
    const uint32_t *w = (const uint32_t *)coche.ruta; uint32_t k;
    cex_n = 0; cex_ngrp = 0; cex_color = 0; cex_luces = -1; free(cex_n0); cex_n0 = NULL; nsvol = 0;
    if (!w || coche.paso_ruta != 1 || coche.nruta < 6 || memcmp(w, "CEX2", 4) || w[1] != 8) return;
    if (6 + 8 * w[2] + 5 * w[3] + w[4] + 2 * w[5] > coche.nruta) return;
    cex_nsl = w[2]; cex_col = w + 6; cex_m = (const cex_t *)(cex_col + 8 * cex_nsl); cex_vd = (const uint32_t *)(cex_m + w[3]);
    cex_grp = cex_vd + w[4];
    for (k = 0; k < w[3]; k++)
        if (cex_m[k].malla >= coche.nmallas || cex_m[k].v0 + coche.mallas[cex_m[k].malla].n > w[4]) return;
    for (k = 0; k < w[5]; k++) if (cex_grp[2 * k] >= coche.nmallas) return;
    if (!(cex_n0 = malloc(coche.nmallas * sizeof(uint32_t)))) return;
    for (k = 0; k < coche.nmallas; k++) cex_n0[k] = coche.mallas[k].n;
    if (!(coche.ofs = calloc(coche.nvert, sizeof(uint32_t)))) { free(cex_n0); cex_n0 = NULL; return; }
    cex_n = w[3]; cex_ngrp = w[5];
    for (k = 0; k < cex_n; k++) {
        int32_t t = coche.mallas[cex_m[k].malla].tex;
        if (cex_m[k].tipo >= 2 && t >= 0) compila_cab(&coche, t, cex_m[k].tipo == 3 ? 3 : 1);
    }
    coche_color(c);
    coche_luces(0, 0, 0);                                       /* pilotos sin luces ni freno */
    {   /* SVL1 (tools/conv_coche.py bloque_svl): partes convexas de los volumenes +08 y +10, detras de los grupos */
        const uint32_t *q = cex_grp + 2 * w[5], *fin = w + coche.nruta; uint32_t n, j;
        if (q + 2 <= fin && !memcmp(q, "SVL1", 4) && (n = q[1]) <= 8) {
            q += 2;
            for (j = 0; j < n; j++) {
                if (q + 2 > fin || q + 2 + 3 * q[0] + 3 * q[1] > fin) { nsvol = 0; break; }
                svol[j].nv = q[0]; svol[j].ne = q[1]; svol[j].v = (const float *)(q + 2);
                svol[j].e = (const uint8_t *)(q + 2 + 3 * q[0]); q += 2 + 3 * q[0] + 3 * q[1]; nsvol = j + 1;
            }
        }
    }
}
/* UV del mapa de entorno: la normal del modelo girada por el coche (mc, filas = ejes del modelo) y vista desde la
   camara (der, arr): u = 0,5 + 0,5 nx, v = 0,5 - 0,5 ny (mapa esferico) */
static void coche_entorno(matrix_t mc)
{
    float ac[3], bc[3], A[NPZ][3], B[NPZ][3], *a, *b; uint32_t k, i; int j, p;
    for (j = 0; j < 3; j++) {
        ac[j] = (mc[j][0] * der[0] + mc[j][1] * der[1] + mc[j][2] * der[2]) * (0.5f / 127.0f);
        bc[j] = (mc[j][0] * arr[0] + mc[j][1] * arr[1] + mc[j][2] * arr[2]) * (-0.5f / 127.0f);
    }
    for (p = 0; p < NPZ; p++)                                   /* piezas animadas: la normal girada por su matriz */
        for (j = 0; j < 3; j++) {
            const float *r = ani.ok ? ani_m[p][j] : (const float[3]){ j == 0, j == 1, j == 2 };
            A[p][j] = r[0] * ac[0] + r[1] * ac[1] + r[2] * ac[2]; B[p][j] = r[0] * bc[0] + r[1] * bc[1] + r[2] * bc[2];
        }
    for (k = 0; k < cex_n; k++) {
        const cex_t *e = cex_m + k; const malla_t *m = coche.mallas + e->malla; vert_t *v = coche.verts + m->v0;
        const uint32_t *q = cex_vd + e->v0;
        if (e->tipo < 2) continue;
        p = m->nodo < 0 ? 0 : (m->nodo >> 8) & 0xFF; if (p >= NPZ) p = 0;
        a = A[p]; b = B[p];
        for (i = 0; i < m->n; i++) {
            float x = (int8_t)q[i], y = (int8_t)(q[i] >> 8), z = (int8_t)(q[i] >> 16);
            v[i].u = 0.5f + x * a[0] + y * a[1] + z * a[2];
            v[i].v = 0.5f + x * b[0] + y * b[1] + z * b[2];
        }
    }
}

/* coche parado (seleccion, salida): ruedas, volante, faros y carroceria en reposo */
static void coche_anima_reposo(void)
{
    memset(ani.giro, 0, sizeof ani.giro); memset(ani.h204, 0, sizeof ani.h204); memset(ani.h1cc, 0, sizeof ani.h1cc);
    ani.i204 = ani.s204 = ani.i1cc = 0; ani.roof = ani.aleron = ani.fase = ani.v_ant = 0; ani.caliente = 0;
    ani.ban = ani.pz[PZ_FAROS][4] > 0 ? 0 : 0x800;              /* sin escamoteables: 0x800 siempre (46AE70) */
}
/* lee el bloque ANI1 del coche recien cargado (detras de los grupos y del SVL1 de CEX2) */
static void coche_ani_lee(void)
{
    const uint32_t *w = (const uint32_t *)coche.ruta, *q, *fin; uint32_t j, n;
    free(ani.idx); memset(&ani, 0, sizeof ani);
    if (!w || coche.paso_ruta != 1 || coche.nruta < 6 || memcmp(w, "CEX2", 4) || w[1] != 8) return;
    fin = w + coche.nruta;
    q = w + 6 + 8 * w[2] + 5 * w[3] + w[4] + 2 * w[5];
    if (q + 2 <= fin && !memcmp(q, "SVL1", 4)) {                /* volumenes de sombra: se saltan */
        n = q[1]; q += 2;
        for (j = 0; j < n && q + 2 <= fin; j++) q += 2 + 3 * q[0] + 3 * q[1];
    }
    if (q + 9 + 5 * NPZ > fin || memcmp(q, "ANI1", 4) || q[1] < 9 + 5 * NPZ) return;
    ani.modelo = q[2]; memcpy(&ani.radio, q + 3, 6 * 4); memcpy(ani.pz, q + 9, sizeof ani.pz);
    if (!(ani.radio > 0.05f)) ani.radio = 0.33f;
    if (!piezas_indices(&coche, NPZ, &ani.idx, ani.ini)) return;
    ani.azar = 1; coche_anima_reposo(); ani.ok = 1;
}
/* rand() de la CRT de Microsoft (el 580F40 del PC): 15 bits */
static int ani_rand(void) { ani.azar = ani.azar * 214013u + 2531011u; return (ani.azar >> 16) & 0x7FFF; }
static void traf_anima_paso(float dt);
/* un paso de logica (1/60 s) en carrera: el estado de las piezas como lo dejan los pasos del original */
static void coche_anima_paso(int titulo)
{
    float dt = 1.0f / 60, vmf, x; int k, kmh, smp, sum;
    traf_anima_paso(dt);
    if (!ani.ok) return;
    if (titulo) { coche_anima_reposo(); return; }
    vmf = fis_ok ? fis.vel_mf : yo.v / 60.2f;                   /* car+1C4 (m por cuadro) */
    kmh = fis_ok ? fis.kmh : (int)(yo.v * 3.6f);                /* car+1F4 */
    for (k = 0; k < 4; k++) {                                   /* giro: rueda +30 -= omega (rad/s) por el tiempo */
        float om = fis_ok ? fis.r[k].omega : yo.v / ani.radio;
        ani.giro[k] -= om * dt;
        if (ani.giro[k] > (float)M_PI) ani.giro[k] -= 2 * (float)M_PI; else if (ani.giro[k] < -(float)M_PI) ani.giro[k] += 2 * (float)M_PI;
    }
    smp = fis_ok ? -fis.s32 : 0;                                /* 4A4710: -car+32 acotado a +-5461, x 6 */
    if (smp < -5461) smp = -5461; else if (smp > 5461) smp = 5461;
    ani.h204[ani.i204] = (int16_t)(smp * 6); ani.i204 = (ani.i204 + 1) % 30;
    for (k = 0, sum = 0; k < 30; k++) sum += ani.h204[k];
    ani.s204 = (int16_t)(sum / 30);
    /* aleron del modelo 5 (roof_angle) */
    x = vmf * 216.720001f * 0.729166687f;
    if (300.0f >= x) ani.aleron = x * 0.00333333341f * -0.305432618f;
    else { float x0 = (350.0f - x) * 0.0199999996f * -0.305432618f; ani.aleron = x0 > 0 ? 0 : x0; }
    /* faros escamoteables (46AE70 / 469600 con car+4 & 0x500: 0x100 noche, 0x400 la bandera 8 del curso; la demo no
       tiene SCN_EFC: coche_ban = 0, se quedan cerrados) */
    if (ani.pz[PZ_FAROS][4] > 0) {
        float arriba = ani.pz[PZ_FAROS][3], paso = arriba / ani.pz[PZ_FAROS][4];
        ani.ban = (ani.ban & ~0x800u) | (coche_ban & 0x700u);
        if (coche_ban & 0x500) { if (arriba > ani.roof) { ani.roof += paso; if (ani.roof > arriba) ani.roof = arriba; } else ani.ban |= 0x800; }
        else if (ani.roof > 0) { ani.roof -= paso; if (ani.roof < 0) ani.roof = 0; }
    } else ani.ban = (coche_ban & 0x700u) | 0x800;
    /* 4A2D70: car+1CC = media de 30 cuadros de 1000 x car+1C8 */
    ani.h1cc[ani.i1cc] = (vmf - ani.v_ant) * 1000.0f; ani.i1cc = (ani.i1cc + 1) % 30; ani.v_ant = vmf;
    /* 4A2910: fase de la subida */
    if (kmh > 100) {
        float a = kmh * 0.00999999978f - 1.0f; if (a > 4.0f) a = 4.0f;
        ani.fase += a; if (ani.fase > 255.0f) ani.fase = 0;
    }
    ani.caliente = (fis_ok ? fis.freno_op : 0) > 200 && kmh != 0;   /* disco +94 (46AE70) */
}
/* matrices de las piezas para el cuadro (46AE70 / 469600 con el estado de coche_anima_paso) */
static void coche_anima_matrices(int titulo)
{
    const float U = 9.58738019e-05f;
    float ty = 0, x, t, v, s1cc = 0; int k, kmh; matrix_t cuerpo;
    if (!ani.ok) return;
    kmh = titulo ? 0 : fis_ok ? fis.kmh : (int)(yo.v * 3.6f);
    if (kmh > 100) {                                            /* 4A2910: subida */
        float a = kmh * 0.00999999978f - 1.0f, sn = sinf((float)(int32_t)(ani.fase * 4096.0f) * U); if (a > 4.0f) a = 4.0f;
        ty = (((float)ani_rand() * 3.05175781e-05f + 0.5f) * 0.666666687f * sn) * (a * 0.5f) * ani.base + ani.base;
    }
    for (k = 0; k < 30; k++) s1cc += ani.h1cc[k];
    x = s1cc * 0.0333333351f;                                  /* car+1CC */
    if (x < -1.57079637f) x = -1.57079637f; else if (x >= 1.57079637f) x = 1.57079637f;
    t = kmh * 0.00208333344f;
    v = x < 0 ? ani.p20 * t : (1.0f - t) * ani.p24;
    v = v * x * x * 0.405284703f;
    if (ani.p20 > v) v = ani.p20; else if (v > ani.p24) v = ani.p24;
    /* el cuerpo de la fisica (work+10): la demo pone el coche en la calzada (coche_coloca); el original lo dibuja en
       car+14 = CG - R (0, +224, 0) (CopyCarWork 4A1140) con la R del cuerpo: aqui la altura del CG sobre la calzada
       (fis.alt), el cabeceo y el balanceo de la suspension (ActionForce2) alrededor del CG, menos alt_cg (+224) */
    m_id(ani_m[0]);
    if (fis_ok && !titulo) {
        m_tras(ani_m[0], 0, fis.alt, 0); m_rx(ani_m[0], fis.cabeceo); m_rz(ani_m[0], fis.balanceo);
        m_tras(ani_m[0], 0, -fpar.alt_cg, 0);
    }
    memcpy(cuerpo, ani_m[0], sizeof(matrix_t));
    m_tras(ani_m[0], 0, ty, 0); m_rx(ani_m[0], v);             /* car+F0 (balanceo 0: car+4 & 1) */
    memcpy(ani_m[PZ_VOLANTE], ani_m[0], sizeof(matrix_t));
    m_tras(ani_m[PZ_VOLANTE], ani.pz[PZ_VOLANTE][0], ani.pz[PZ_VOLANTE][1], ani.pz[PZ_VOLANTE][2]);
    m_rx(ani_m[PZ_VOLANTE], ani.pz[PZ_VOLANTE][3]); m_rz(ani_m[PZ_VOLANTE], (float)(-ani.s204) * U * 0.416666657f);
    memcpy(ani_m[PZ_ALERON], ani_m[0], sizeof(matrix_t));
    m_tras(ani_m[PZ_ALERON], ani.pz[PZ_ALERON][0], ani.pz[PZ_ALERON][1], ani.pz[PZ_ALERON][2]);
    m_rx(ani_m[PZ_ALERON], titulo ? 0 : ani.aleron);
    memcpy(ani_m[PZ_FAROS], ani_m[0], sizeof(matrix_t));
    m_tras(ani_m[PZ_FAROS], ani.pz[PZ_FAROS][0], ani.pz[PZ_FAROS][1], ani.pz[PZ_FAROS][2]); m_rx(ani_m[PZ_FAROS], ani.roof);
    for (k = 0; k < 4; k++) {                                   /* ruedas: fuera de car+F0 */
        const float *p = ani.pz[PZ_RUEDA + k];
        float y = fis_ok && !titulo ? fis.rueda_y[k] : p[1];   /* car+134 */
        float dir = fis_ok && !titulo ? (float)fis.r[k].ang * U : 0;
        matrix_t *P = &ani_m[PZ_PINZA + k], *R = &ani_m[PZ_RUEDA + k];
        memcpy(*P, cuerpo, sizeof(matrix_t)); m_tras(*P, p[0], y, p[2]);
        if (p[3] != 0) m_rz(*P, p[3]);
        if (dir != 0) m_ry(*P, dir);
        memcpy(*R, *P, sizeof(matrix_t));
        if (ani.giro[k] != 0) m_rx(*R, ani.giro[k]);
    }
}
/* el coche del jugador en la lista l: cada pieza con su matriz (sin ANI1, entero con la del coche) */
static void coche_dibuja(int l, matrix_t *mc)
{
    int p;
    if (!ani.ok) { mat_load(&vista); mat_apply(mc); dibuja_rango(&coche, l, coche.inicio[l], coche.inicio[l + 1], 0, 0, 0); return; }
    for (p = 0; p < NPZ; p++) {
        uint32_t a = ani.ini[l][p], b = ani.ini[l][p + 1];
        if (b <= a) continue;
        mat_load(&vista); mat_apply(mc); mat_apply(&ani_m[p]);
        dibuja_indices(&coche, l, ani.idx + a, b - a);
    }
}
/* Ruedas del trafico cerca (LOD 0 y 1) como 4AD530: T(posicion del descriptor) Ry(car+32 = 0; + pi las del otro lado)
   Rx(-car+40 delante, -car+44 detras; con el signo cambiado las del otro lado). 47E780 suma a car+40 y +44 cada cuadro
   +1C4 / radio (+08 de 4866C0) en unidades de 65536 por vuelta: las dos igual. car+32 del trafico no se ha localizado
   (0: sin direccion). Bloque TAN1 detras de los pares de TCX1 (tools/conv_trafico.py bloque_tan) */
#define NTRUEDA 6
typedef struct { int ok; float radio, r[NTRUEDA][5]; uint16_t *idx, ini[3][NPZ_MAX + 1]; } tani_t;
static tani_t tani[NMODTRAF];
static float traf_giro[NTRAF];                                  /* -car+40 (rad) de cada coche de trafico */
static void traf_ani_lee(int m)
{
    const uint32_t *w = (const uint32_t *)mtraf[m].ruta, *q;
    tani_t *a = tani + m;
    free(a->idx); memset(a, 0, sizeof *a);
    if (!w || mtraf[m].paso_ruta != 1 || mtraf[m].nruta < 3 || memcmp(w, "TCX1", 4)) return;
    q = w + 3 + 3 * w[2];
    if (q + 3 + 5 * NTRUEDA > w + mtraf[m].nruta || memcmp(q, "TAN1", 4) || q[1] != NTRUEDA) return;
    memcpy(&a->radio, q + 2, 4); memcpy(a->r, q + 3, sizeof a->r);
    if (!(a->radio > 0.05f)) return;
    if (!piezas_indices(&mtraf[m], NTRUEDA + 1, &a->idx, a->ini)) return;
    a->ok = 1;
}
static void traf_anima_paso(float dt)
{
    int i;
    for (i = 0; i < NTRAF; i++) {
        const trafico_t *t = traf + i; float r = tani[t->modelo].ok ? tani[t->modelo].radio : 0.3f;
        if (t->carril < 0) continue;
        traf_giro[i] -= t->c.v * dt / r;
        if (traf_giro[i] < -(float)M_PI) traf_giro[i] += 2 * (float)M_PI;
    }
}
/* un coche de trafico en la lista l con la matriz mt (ya con solo_lod puesto): carroceria y ruedas */
static void traf_dibuja(int i, int l, matrix_t *mt)
{
    const trafico_t *t = traf + i; const tani_t *a = tani + t->modelo; modelo_t *mo = &mtraf[t->modelo]; int p;
    static matrix_t W __attribute__((aligned(32)));
    if (!a->ok) { mat_load(&vista); mat_apply(mt); dibuja_rango(mo, l, mo->inicio[l], mo->inicio[l + 1], 0, 0, 0); return; }
    for (p = 0; p <= NTRUEDA; p++) {
        uint32_t k0 = a->ini[l][p], k1 = a->ini[l][p + 1];
        if (k1 <= k0) continue;
        mat_load(&vista); mat_apply(mt);
        if (p > 0) {
            const float *r = a->r[p - 1];
            m_id(W); m_tras(W, r[0], r[1], r[2]);
            if (r[3] != 0) m_ry(W, (float)M_PI);
            m_rx(W, r[3] != 0 ? -traf_giro[i] : traf_giro[i]);
            mat_apply(&W);
        }
        dibuja_indices(mo, l, a->idx + k0, k1 - k0);
    }
}

/* Sombra del coche del jugador como el original: volumenes de sombra (+08 y +10 de la disposicion, bloque SVL1)
   extruidos por la luz del sol. SLES-53998 0x1893f8 dibuja en carrera (vm = 1) los dos volumenes con 0x1138b8
   (silueta 0x113bf8: aristas de 0x24 bytes, cuadrilateros extruidos por la luz en el marco del coche) en una
   plantilla emulada en el GS y oscurece con 0x114150 lo que queda dentro, antes de dibujar la carroceria; el PC hace lo
   mismo con la plantilla de D3D (422550 / 421740 / 421A90 y el cuadro 0x5F000000 de 4220F0). Luz: la 1 del entorno 0
   (4082B0(1, 0, 0) + 44), la de scn_env_sun_<zona> lista 1 (CURSO.DAT: inclinacion y rumbo), (0, 0, -1) girada en x y
   en y (44A430) en el marco de la etapa. Silueta (421740): una arista con una cara iluminada (n . l >= 0, l = -25 x
   direccion) o con sus dos caras distintas da un cuadrilatero (a, b, a - l, b - l); en el PC la extrusion es de 25 m
   (6282D0; la PS2 la lleva hasta el borde de la vista, 1000 como mucho). En la PowerVR va como volumen modificador
   (lista OP_MOD, "cheap shadow"): lo opaco y lo recortado de la escena y del trafico que queda dentro se multiplica por
   SOMBRA_ESCALA. Sin tapas, como el original: la PowerVR cuenta las caras del volumen delante de cada pixel (par/impar)
   y la camara de carrera nunca esta dentro (en la vista interior, vm = 2, el original no las dibuja: aqui tampoco, el
   coche no se dibuja). Si un extremo quedara detras del plano cercano, la extrusion se acorta hasta el. */
#define SOMBRA_EXT 25.0f                                        /* 6282D0 del PC */
#define SOMBRA_ESCALA (160.0f / 255.0f)                         /* 1 - 0x5F / 255: el cuadro de 4220F0 */
#define SOMBRA_TRI 512                                          /* triangulos por volumen como mucho */
static uint32_t sombra_tri, sombra_teo;                         /* triangulos y bytes teoricos del TA del cuadro */
/* direccion de la luz del sol en el mundo (hacia donde va la luz) */
static void sombra_luz(float *d)
{
    float p = 0, y = 0, x, z, c, sn;
    if (cur_nodo >= 0) { p = curso[cur_nodo].sol[0] * (3.14159265f / 180.0f); y = curso[cur_nodo].sol[1] * (3.14159265f / 180.0f); }
#ifdef SOL_PRUEBA
    y += SOL_PRUEBA * (3.14159265f / 180.0f);                   /* prueba: el sol girado (para ver la sombra) */
#endif
    x = -cosf(p) * sinf(y); d[1] = sinf(p); z = -cosf(p) * cosf(y);   /* (0, 0, -1) por Rx(p) y Ry(y): 44A430 */
    c = cosf(marco_et[3]); sn = sinf(marco_et[3]);              /* en el marco de la etapa (modelo_coloca) */
    d[0] = x * c + z * sn; d[2] = -x * sn + z * c;
}
/* en la lista OP_MOD, con la vista y el coche (mc) cargados */
static void sombra_dibuja(matrix_t mc)
{
    static pvr_modifier_vol_t t[SOMBRA_TRI] __attribute__((aligned(32)));
    pvr_mod_hdr_t h; float d[3], L[3]; int j, k;
    if (!nsvol || cur_nodo < 0) return;
    sombra_luz(d);
    for (k = 0; k < 3; k++) L[k] = d[0] * mc[k][0] + d[1] * mc[k][1] + d[2] * mc[k][2];   /* al marco del coche */
    for (j = 0; j < nsvol; j++) {
        const svol_t *sv = svol + j; uint32_t i, n = 0;
        for (i = 0; i < sv->ne && n + 2 <= SOMBRA_TRI; i++) {
            const uint8_t *e = sv->e + 12 * i; uint16_t ia, ib; int front, rev = 0, q;
            float P[4][3], S[4][3];
            memcpy(&ia, e, 2); memcpy(&ib, e + 2, 2);
            if (ia >= sv->nv || ib >= sv->nv) continue;
            /* iluminada: n . l >= 0 con l = -25 d (421850) */
            front = (int8_t)e[4] * L[0] + (int8_t)e[5] * L[1] + (int8_t)e[6] * L[2] <= 0;
            if (e[7] < 2) {
                if (!front) continue;
            } else {
                int back = (int8_t)e[8] * L[0] + (int8_t)e[9] * L[1] + (int8_t)e[10] * L[2] <= 0;
                if (front == back) continue;
                rev = back;
            }
            memcpy(P[0], sv->v + 3 * ia, 12); memcpy(P[1], sv->v + 3 * ib, 12);
            for (q = 0; q < 2; q++) {                           /* extremo lejano: a - l = a + 25 d */
                float x = P[q][0], y = P[q][1], z = P[q][2], w, x2, y2, z2, w2, s = SOMBRA_EXT;
                mat_trans_single3_nodivw(x, y, z, w);
                if (w < cerca + 0.05f) return;                  /* la camara en el coche: sin sombra */
                x2 = P[q][0] + s * L[0]; y2 = P[q][1] + s * L[1]; z2 = P[q][2] + s * L[2];
                mat_trans_single3_nodivw(x2, y2, z2, w2);
                if (w2 < cerca + 0.05f) {                       /* acortada hasta el plano cercano */
                    s *= (w - cerca - 0.05f) / (w - w2);
                    x2 = P[q][0] + s * L[0]; y2 = P[q][1] + s * L[1]; z2 = P[q][2] + s * L[2];
                    mat_trans_single3_nodivw(x2, y2, z2, w2);
                    if (w2 < cerca) w2 = cerca;
                }
                S[q][0] = x / w; S[q][1] = y / w; S[q][2] = 1.0f / w;
                S[q + 2][0] = x2 / w2; S[q + 2][1] = y2 / w2; S[q + 2][2] = 1.0f / w2;
            }
            {   /* 421850: (a, b, al), (b, bl, al); con la segunda cara iluminada (b, a, bl), (bl, a, al) */
                static const uint8_t O[2][6] = { { 0, 1, 2, 1, 3, 2 }, { 1, 0, 3, 3, 0, 2 } };
                const uint8_t *o = O[rev];
                for (q = 0; q < 2; q++, n++) {
                    pvr_modifier_vol_t *v = t + n;
                    v->flags = PVR_CMD_VERTEX_EOL;
                    v->ax = S[o[3 * q]][0]; v->ay = S[o[3 * q]][1]; v->az = S[o[3 * q]][2];
                    v->bx = S[o[3 * q + 1]][0]; v->by = S[o[3 * q + 1]][1]; v->bz = S[o[3 * q + 1]][2];
                    v->cx = S[o[3 * q + 2]][0]; v->cy = S[o[3 * q + 2]][1]; v->cz = S[o[3 * q + 2]][2];
                    v->d1 = v->d2 = v->d3 = v->d4 = v->d5 = v->d6 = 0;
                }
            }
        }
        if (n < 2) continue;
        {   /* TA: cada triangulo del volumen, 40 bytes (palabra ISP y 9 floats) por ta_f; entero o nada */
            uint32_t tb = 40 * n + 32, c = tb * ta_f >> 8;
            if (ta_vb + c > ta_lim) { ta_corto = 1; return; }
            ta_vb += c; sombra_teo += tb; sombra_tri += n;
        }
#ifdef DIAG_SOMBRA
        {   static int k_; if (++k_ % 120 == 1) dbglog(DBG_INFO, "SOMBRA vol %d n %u L %.2f %.2f %.2f t0 %.1f %.1f %.4f / %.1f %.1f %.4f / %.1f %.1f %.4f\n", j, (unsigned)n,
                (double)L[0], (double)L[1], (double)L[2], (double)t[0].ax, (double)t[0].ay, (double)t[0].az, (double)t[0].bx, (double)t[0].by, (double)t[0].bz, (double)t[0].cx, (double)t[0].cy, (double)t[0].cz); }
#endif
        pvr_mod_compile(&h, PVR_LIST_OP_MOD, PVR_MODIFIER_OTHER_POLY, PVR_CULLING_NONE);
        pvr_prim_real(&h, sizeof h);
        for (i = 0; i + 1 < n; i++) pvr_prim_real(t + i, sizeof(pvr_modifier_vol_t));
        pvr_mod_compile(&h, PVR_LIST_OP_MOD, PVR_MODIFIER_INCLUDE_LAST_POLY, PVR_CULLING_NONE);   /* cierra el volumen */
        pvr_prim_real(&h, sizeof h);
        pvr_prim_real(t + n - 1, sizeof(pvr_modifier_vol_t));
    }
}

/* el menu suelta el coche 3D cuando una pantalla no cabe en la VRAM (se vuelve a cargar al hacer falta) */
static void coche_suelta(void)
{
    if (!coche.tv) return;
    dbglog(DBG_INFO, "coche: ficha %d fuera de la VRAM (la pide el menu)\n", coche_ficha);
    libera_modelo(&coche); coche_ficha = -1; cex_n = 0; nsvol = 0; ani.ok = 0;
}

/* un coche cada vez: libera el anterior (RAM y VRAM) y carga el de la ficha k; en el menu, si no cabe en la VRAM, se
   echan las pantallas guardadas del menu. 0 bien */
static int coche_carga(int k)
{
    char nom[16]; int r; uint32_t ct = 0, c, i;
    if (k == coche_ficha && coche.tv) return 0;
    libera_modelo(&coche); coche_ficha = -1; cex_n = 0; nsvol = 0; ani.ok = 0;
    if (ficha_modelo[k] < 30) sprintf(nom, "COCHE%02d.DAT", k); else strcpy(nom, "COCHE.DAT");
    musica_pausa_lectura(1);                                    /* el GD para el coche (la musica tira del anillo) */
    r = lee_modelo(&coche, nom);
    musica_pausa_lectura(0);
    if (r) return r;
    if ((r = sube_modelo(&coche))) {                            /* sin VRAM: fuera la cache del menu y otra vez */
        for (i = 0; i < coche.ntex; i++) if (coche.tv[i]) pvr_mem_free(coche.tv[i]);
        free(coche.tv); free(coche.cabs[0]); free(coche.cabs[1]); coche.tv = NULL; coche.cabs[0] = coche.cabs[1] = NULL;
        menu_libera_vram();
        if ((r = sube_modelo(&coche))) { libera_modelo(&coche); ani.ok = 0; return r; }
    }
    compacta_modelo(&coche);                                    /* los texeles ya estan en la VRAM */
    coche_cex(menu_color());                                    /* pintura del color elegido y reflejo */
    coche_ani_lee();                                            /* piezas animadas (ANI1) */
    coche_ficha = k; modelo_jug = ficha_modelo[k] < 30 ? ficha_modelo[k] : MODELO;
    for (i = 0; i < NMODTRAF; i++) if ((c = modelo_teo(&mtraf[i], traf_con_lod[i] ? 0 : -1, 0)) > ct) ct = c;
    ta_min_otros_teo = modelo_teo(&coche, -1, 1) + 3 * ct; ta_traf_teo = ct;   /* lo reservado: este coche y tres de trafico (LOD 0) */
    dbglog(DBG_INFO, "coche: ficha %d %s modelo %d, %u vertices, %u texturas, VRAM libre %uK\n", k, nom, modelo_jug,
           (unsigned)modelo_vtx(&coche, -1), (unsigned)coche.ntex, (unsigned)(pvr_mem_available() >> 10));
    return 0;
}

int main(int argc, char **argv)
{
    /* bufer de parametros de TA_VB_BYTES (x2); opacos con bloques de 32 (el suelo cercano toca muchas teselas) y 4
       juegos de OPB de reserva (307 KB por cuadro; en la consola se midieron 65 KB usados en el cuadro que se colgo):
       en la consola real un bufer u OPB lleno cuelga el TA. Modificadores opacos (la sombra del coche, sombra_dibuja)
       con bloques de 8 y translucidos de 16 a 8: el OPB ocupa lo mismo que antes (64 palabras por tesela). Cuentas en
       docs/DREAMCAST.md */
    pvr_init_params_t pp = { { PVR_BINSIZE_32, PVR_BINSIZE_8, PVR_BINSIZE_8, 0, PVR_BINSIZE_16 }, TA_VB_BYTES, 0, 0, 0, 4 };
    int estado = TITULO, auto_ = 0, ver = VER, pausa = 0, l, i, r;
    int reintenta = 0;                                          /* REINTENTAR del menu de pausa: salida otra vez */
    unsigned coches_m; int ficha_ini;
    uint32_t cuadro = 0, bot_ant = 0, t_estado = 0; uint64_t t0, tcpu = 0, c0;
    float cam_y = 0, tiempo = TIEMPO_ETAPA, fps = 0, ms = 0, cam_fw[3] = { 0, 0, -1 }, ttit = 0;
    char s1[96], s2[96], s3[96], msg[32] = ""; int aviso_rama = 0, pista = 0, pasos = 1, paso, saltos = 0, saltos_ver = 0;
    uint64_t reloj = 0; float lejos_obj = lejos;
    uint64_t c_log = 0, t_log = 0, t_ini = timer_ms_gettime64(); float ms_log = 0, t_carga = 0;
    (void)argc; (void)argv;

    /* 1: datos del disco a la RAM (la pantalla de texto aun sin PowerVR) */
    elige_video();
    vid_set_mode(modo_video, PM_RGB565);
    aviso("OutRun 2006 C2C - demo Dreamcast. Cargando la etapa...");
    if ((r = curso_lee() ? -8 : curso_huecos() ? -9 : curso_lee_etapa(0))) {   /* el arbol y la primera etapa */
        sprintf(s1, "No se pudieron leer los datos (%d)", r); aviso(s1); for (;;) thd_sleep(1000);
    }
    coches_m = coches_lee();                                    /* fichas con coche; se empieza por el Testarossa */
    ficha_ini = (coches_m >> 9) & 1 || !coches_m ? 9 : __builtin_ctz(coches_m);
#ifdef PRUEBA_FICHA
    if ((coches_m >> PRUEBA_FICHA) & 1) ficha_ini = PRUEBA_FICHA;   /* prueba: sale con ese coche */
#endif
    if (ficha_modelo[ficha_ini] < 30) modelo_jug = ficha_modelo[ficha_ini];
    for (i = 0; i < NMODTRAF; i++) {
        char nom[16]; uint32_t j; sprintf(nom, "TRAF%d.DAT", i);
        if ((r = lee_modelo(&mtraf[i], nom))) { sprintf(s1, "No se pudo leer %s (%d)", nom, r); aviso(s1); for (;;) thd_sleep(1000); }
        traf_colores(i);
        for (j = 0; j < mtraf[i].nmallas; j++) if (mtraf[i].mallas[j].nodo > 0) traf_con_lod[i] = 1;   /* con LOD */
        traf_ani_lee(i);                                        /* ruedas animadas (TAN1) */
    }
    traf_medidas();
    traf_tablas();                                              /* clases y cajas del trafico (TRAFC.DAT) */
    if (ruta_pon(&etapa)) { aviso("Sin memoria para la ruta"); for (;;) thd_sleep(1000); }
    fisica_carga();
    /* 2: PowerVR, texturas a la memoria de video y cabeceras */
    pvr_init(&pp);
    pvr_set_shadow_scale(true, SOMBRA_ESCALA);                  /* lo que queda dentro de la sombra del coche */
    pvr_set_bg_color(0.45f, 0.65f, 0.95f);
    txt_arr = pvr_mem_malloc(sizeof buf_arr); txt_cen = pvr_mem_malloc(sizeof buf_cen);
    for (i = 0, r = 0; i < NMODTRAF; i++) { r |= sube_modelo(&mtraf[i]); if (!r) compacta_modelo(&mtraf[i]); }   /* texeles fuera de la RAM */
    if (r || sube_modelo(&etapa)) {
        vid_set_mode(modo_video, PM_RGB565); aviso("Sin memoria de video para las texturas"); for (;;) thd_sleep(1000);
    }
    sol_carga();                                                /* el sol y el destello (SOL.DAT, siempre en VRAM) */
    if ((r = coche_carga(ficha_ini))) {
        vid_set_mode(modo_video, PM_RGB565); sprintf(s1, "No se pudo cargar el coche (%d)", r); aviso(s1); for (;;) thd_sleep(1000);
    }
    cielo = &etapa;
    {   /* minimo que se guarda para lo que no es la etapa: el coche y tres coches de trafico (el primer cuadro de carrera
           no tiene medida del anterior) */
        uint32_t ct = 0, c, cv = 0;
        for (i = 0; i < NMODTRAF; i++) {
            if ((c = modelo_teo(&mtraf[i], traf_con_lod[i] ? 0 : -1, 0)) > ct) { ct = c; cv = modelo_vtx(&mtraf[i], traf_con_lod[i] ? 0 : -1); }
        }
        ta_min_otros_teo = modelo_teo(&coche, -1, 1) + 3 * ct; ta_traf_teo = ct;
        dbglog(DBG_INFO, "TA: bufer %uK, coche %u vertices, trafico hasta %u (LOD 0), minimo reservado %uK\n", TA_VB_BYTES >> 10,
               (unsigned)modelo_vtx(&coche, -1), (unsigned)cv, (unsigned)((ta_min_otros_teo * ta_f >> 8) >> 10));
    }
    hud_ok = !hud_carga("HUD.DAT", modelo_jug);                 /* el tacometro del modelo (6 = Testarossa) */
    for (l = 0; l < 3; l++) for (i = 0; i < 2; i++) {
        pvr_poly_cxt_t cx; pvr_poly_cxt_col(&cx, LISTA[l]);
        cx.gen.fog_type = i ? PVR_FOG_DISABLE : PVR_FOG_TABLE; cx.gen.culling = PVR_CULLING_NONE;
        if (l != 2) { cx.fmt.modifier = true; cx.gen.modifier_mode = false; }   /* recibe la sombra del coche */
        pvr_poly_compile(&cab_col[i][l], &cx);
    }
    efectos_inicia();
    pvr_fog_table_color(1.0f, 0.55f, 0.70f, 0.92f);
    niebla_pon();
    memset(&yo, 0, sizeof yo);
    coche_paso(&yo, 0, 0, 0, 0);
    t_carga = (timer_ms_gettime64() - t_ini) / 1000.0f;
    musica_inicia();                                            /* la musica la pone el menu (sin MENU.DAT, aqui) */
    sfx_ok = !sfx_inicia();
    voz_ok = sfx_ok && !voces_inicia();                         /* VOCES.DAT: voces de la carrera */
    texto_arriba("", NULL, NULL); texto_centro("");
    /* menus del original (MENU.DAT): usan la VRAM del HUD de carrera, que se baja mientras tanto */
    if (menu_inicia()) { sprintf(s1, "Sin MENU.DAT"); texto_centro(s1); musica_pon(pista); }
    if (hud_ok) hud_baja_tex();
    menu_coches(coches_m);
    menu_fichas(ficha_modelo, N_FICHAS);                        /* modelo de cada COCHEnn.DAT (orden del menu: MENU.DAT) */
    menu_suelta_coche = coche_suelta;                           /* si una pantalla del menu no cabe: fuera el coche 3D */
#if !defined(PRUEBA_AUTO) && !defined(PRUEBA_JUGADOR)              /* las pruebas automaticas van directas */
    if (video_titulo) video_titulo();                           /* gancho: pelicula de inicio antes del titulo */
#endif
    menu_entra(manual_, pista, NPISTAS);
    t0 = timer_ms_gettime64();

    for (;;) {
        maple_device_t *mc = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = mc ? (cont_state_t *)maple_dev_status(mc) : NULL;
        uint32_t b = st ? st->buttons : 0, nuevo = b & ~bot_ant;
        float giro = 0, gas = 0, freno = 0, ojo_[3], mira[3];
        bot_ant = b; auto_piloto = auto_; t_estado++; c_log = timer_us_gettime64();
        {   /* salto de cuadros: tantos pasos de logica (60 Hz) como haya pasado de tiempo real, hasta 3 */
            uint64_t ahora = timer_us_gettime64();
            if (!reloj) reloj = ahora;
            pasos = (int)((ahora - reloj + PERIODO / 2) / PERIODO);
            if (pasos < 1) pasos = 1;
            if (pasos > 3) { pasos = 3; reloj = ahora - 3 * PERIODO; }
            reloj += (uint64_t)pasos * PERIODO; saltos += pasos - 1;
#ifdef PRUEBA_GUION
            if (guion_hasta && timer_ms_gettime64() < guion_hasta) pasos = 0;   /* congelado para la captura */
            else guion_hasta = 0;
#endif
        }
        musica_poll();
        /* en carrera Y cambia la vista (camara del original); en pausa, o sin las tablas, ensena las cifras; en el
           menu, Y es el tema al azar */
        if (estado != TITULO && (nuevo & CONT_Y) && (!cam_ok || pausa)) ver ^= 1;
        if (estado != TITULO && manual_) {                      /* cambio manual: cruceta arriba sube, abajo baja */
            if (nuevo & CONT_DPAD_UP) pide_cambio = 1;
            if (nuevo & CONT_DPAD_DOWN) pide_cambio = -1;
        }
        if (st) {
            /* volante: el eje y la cruceta van a or2_volante en cada paso (zona muerta 0,25, rampa de la cruceta y
               filtro por cuadro del original); giro queda para el modelo simple y los menus */
            mando.eje = st->joyx < -128 ? -128 : (st->joyx > 127 ? 127 : st->joyx);
            mando.izq = (b & CONT_DPAD_LEFT) != 0; mando.der = (b & CONT_DPAD_RIGHT) != 0;
            giro = st->joyx / 128.0f;
            if (mando.izq) giro = -1;
            if (mando.der) giro = 1;
            if (giro > 1) giro = 1;
            if (giro < -1) giro = -1;
            /* pedales: volumenes 1 y 2 del original, 0..255 tal cual (el original lee la presion de los botones
               sin curva); A y B/X a fondo */
            gas = st->rtrig / 255.0f; if (b & CONT_A) gas = 1;
            freno = st->ltrig / 255.0f; if (b & (CONT_B | CONT_X)) freno = 1;
        }
        mando.nivel = etapa_n;

        /* estados (una vez por paso de logica) */
        for (paso = 0; paso < pasos; paso++) {
        if (paso) { nuevo = 0; t_estado++; }
        if (t_cuenta < 9999) t_cuenta++;
        if (t_ext < 9999) t_ext++;
        if (golpe_cd) golpe_cd--;
        t_bif++;
        if (pausa && estado == CARRERA) {                       /* menu de pausa del original (comun/menu.c) */
            int rp = menu_pausa_paso(nuevo);
            nuevo = 0;
            if (rp != PAUSA_SIGUE) { pausa = 0; menu_pausa_sale(); }
            if (rp == PAUSA_REINTENTA) { estado = TITULO; reintenta = 1; }   /* abajo: la salida con lo elegido */
            else if (rp == PAUSA_ABANDONA) {                    /* al frontal */
                estado = TITULO; auto_ = 0; texto_centro(""); hud_baja_tex(); menu_entra(manual_, pista, NPISTAS);
            }
        }
        if (estado == TITULO) {
            int rm = reintenta ? MENU_CARRERA : menu_paso(nuevo);   /* titulo, modos, coche, cambio y musica */
            ttit += 1.0f / 60;
            if (menu_coche3d() && menu_coche() >= 0 && menu_coche() != coche_ficha && menu_coche() != coche_malo &&
                coche_carga(menu_coche())) coche_malo = menu_coche();                              /* (sin reintentos) */
#ifdef PRUEBA_JUGADOR
            rm = MENU_CARRERA;                                  /* prueba: directo a la carrera con el mando */
#endif
#ifdef PRUEBA_AUTO
            rm = MENU_DEMO;                                     /* prueba: directo a la carrera con el piloto */
#endif
#ifndef PRUEBA_AUTO
            if (rm == MENU_DEMO && video_atraccion) {           /* inactividad en el titulo: las peliculas del modo */
                menu_sale();                                    /* ADVERTISE del original en vez de la demo de carrera */
                if (video_atraccion() >= 0) { menu_entra(manual_, pista, NPISTAS); rm = MENU_SIGUE; nuevo = 0; }
            }
#endif
            if (rm != MENU_SIGUE) {
                auto_ = rm == MENU_DEMO;
                if (!auto_) { manual_ = menu_manual(); pista = menu_pista(); }
                menu_sale();                                    /* la VRAM del menu vuelve al HUD */
                if (!auto_ && menu_coche() >= 0 && menu_coche() != coche_ficha && coche_carga(menu_coche())) coche_carga(9);
                if (coche_ficha < 0) coche_carga(9);            /* el menu lo solto (VRAM) */
                fisica_modelo(modelo_jug);                      /* peso, agarre, motor y marchas de ese coche */
                if (hud_ok) hud_pon_modelo(modelo_jug);         /* y su tacometro */
                if (hud_ok && hud_sube_tex()) hud_ok = 0;
                curso_reinicia();                               /* la etapa de salida (otra vez, si hacia falta) */
                memset(&yo, 0, sizeof yo); yo.s = SALIDA; yo.x = carril_x(SALIDA, 4);
#ifdef PRUEBA_FIN
                yo.s = longitud - PRUEBA_FIN; yo.x = carril_x(yo.s, 4);   /* prueba: a PRUEBA_FIN m del final */
#endif
#ifdef PRUEBA_X
                yo.x = PRUEBA_X;                                /* prueba: salida en otro desvio (p.ej. la hierba) */
#endif
                coche_paso(&yo, 0, 0, 0, 0); traf_inicia(SALIDA); efectos_borra();
                if (fis_ok) { float p[3], th; ruta_en(yo.s, p, &th, NULL, NULL); or2_coloca(&fis, &fpar, th, 0); }
                lejos = LEJOS_PS2; entorno_reinicia(); niebla_paso(); niebla_pon();   /* la distancia del original */
                tim_cuadros = tim_zona(cur_nodo, etapa_n); tiempo = tim_cuadros / TIM_HZ; estado = CUENTA; t_estado = 0; t_cuenta = 0; pausa = 0; crono = 0; vmax = 0; pt_inicia();
                reintenta = 0;
                if (voz_ok) voces_carrera();
                if (cam_ok) { cam_inicia(&cam, &cam_tab); cam_t_choque = 0; }   /* 484EE0 */
            }
        } else if (estado == CUENTA) {
            int n = 3 - (int)(t_estado / 60);
            sprintf(s1, n > 0 ? "%d" : "GO!", n);
            if (strcmp(s1, msg)) { strcpy(msg, s1); texto_centro(msg); }
            if (t_estado >= 240) { estado = CARRERA; t_estado = 0; if (voz_ok) voces_salida(); }   /* GO! a los 4 s; modo GAME */
            curso_paso();                                       /* la union ya se puede ir leyendo */
            if (auto_ && (nuevo & CONT_START)) { estado = TITULO; auto_ = 0; nuevo = 0; texto_centro(""); hud_baja_tex(); menu_entra(manual_, pista, NPISTAS); }
            coche_paso(&yo, 0, 0, 0, 1.0f / 60);
        } else if (estado == CARRERA) {
            if (t_estado == 50 && !aviso_rama) texto_centro(auto_ ? "DEMO" : "");
            if (auto_ && (nuevo & CONT_START)) { estado = TITULO; nuevo = 0; auto_ = 0; texto_centro(""); hud_baja_tex(); menu_entra(manual_, pista, NPISTAS); }
            else if (!pausa && (nuevo & CONT_START)) { pausa = 1; menu_pausa_entra(); nuevo = 0; }   /* START: pausa */
            if (!pausa) {
#ifdef PRUEBA_V
                if (t_estado == 1 && fis_ok) {                  /* prueba: sale lanzado a PRUEBA_V km/h */
                    float p[3], th; ruta_en(yo.s, p, &th, NULL, NULL);
                    or2_coloca(&fis, &fpar, th, PRUEBA_V / (fpar.hud * 3.6f));
                }
#endif
#ifdef PRUEBA_CHOQUE
                if (t_estado == 60 && fis_ok) {                 /* prueba: a ~280 km/h y 50 grados contra el muro derecho */
                    float p[3], th; ruta_en(yo.s, p, &th, NULL, NULL);
                    or2_coloca(&fis, &fpar, th + 0.87f, 280.0f / (fpar.hud * 3.6f));
                }
#endif
                if (auto_) piloto(&yo, &giro, &gas, &freno);
#if defined(PRUEBA_GUION) && defined(PRUEBA_EMBISTE)
                {   /* prueba: a por el coche de trafico mas cercano por delante (a menos de 80 m) */
                    int i; float xt = yo.x, dm = 80.0f, g = fmaxf(0.05f, fminf(yo.v / 8.0f, 1.0f) * (1.0f - 0.45f * yo.v / VMAX));
                    for (i = 0; i < NTRAF; i++)
                        if (traf[i].carril >= 0 && traf[i].b64 <= 0 && traf[i].c.s > yo.s + 2.0f && traf[i].c.s - yo.s < dm) {
                            dm = traf[i].c.s - yo.s; xt = traf[i].c.x;
                        }
                    gas = 1; freno = 0;
                    giro = fmaxf(-1.0f, fminf(1.0f, (0.06f * (xt - yo.x) - yo.a) * 3.0f / (1.6f * g)));
                }
#elif defined(PRUEBA_GUION)
                piloto(&yo, &giro, &gas, &freno);               /* prueba: el piloto, un toque de freno y la vista */
                if (crono >= GUION_DERRAPE && crono < GUION_DERRAPE + 12) { freno = 1; gas = 0; giro = giro < 0 ? -1 : 1; }
                if (crono == 700 || crono == 760) nuevo |= CONT_Y;
#endif
                mando.usa = st && !auto_;
#ifdef PRUEBA_GUION
                mando.usa = 0;
#endif
                coche_paso(&yo, giro, gas, freno, 1.0f / 60);
                mando.usa = 0;
                traf_paso(1.0f / 60);
                efectos_paso(&yo, 1.0f / 60);
                tim_cuadros--; tiempo = tim_cuadros / TIM_HZ;  /* 450AC0: un cuadro de logica */
                if (voz_ok) {                                   /* la acompanante (489A90) y el aviso de tiempo (4B9CD0) */
                    voces_coche_t vc;
                    vc.derrape = yo.t_derrape != 0; vc.choque = fis_ok ? fis.choque : 0;
                    vc.t_choque = fis_ok ? fis.t_choque : 0; vc.t_recup = fis_ok ? fis.t_recup : 0;
                    vc.contacto = voz_contacto; vc.en_etapa = (yo.s - s_etapa) / m_pos; vc.zona = curso[cur_nodo].nivel;
                    voces_paso(&vc); voces_tiempo(tim_cuadros);
                }
                voz_contacto = 0;
                crono++; if (fis_ok && fis.velocidad_hud > vmax) vmax = fis.velocidad_hud;
#if defined(PRUEBA_GUION) && defined(PRUEBA_EMBISTE) && defined(DIAG_CHOQUE)
                if (guion_i < (int)(sizeof emb_cong / sizeof emb_cong[0]) && emb_golpe >= 0 && diag_cuadro == emb_golpe + emb_cong[guion_i]) {
                    guion_hasta = timer_ms_gettime64() + 3000; guion_i++; pasos = paso + 1;
                    dbglog(DBG_INFO, "DIAGCHOQUE congelado %d (cuadro %d)\n", guion_i, diag_cuadro);
                }
#elif defined(PRUEBA_GUION)
                if (guion_i < (int)(sizeof guion_cong / sizeof guion_cong[0]) && crono == guion_cong[guion_i]) {
                    guion_hasta = timer_ms_gettime64() + 3000; guion_i++; pasos = paso + 1;
                }
#endif
                pt_paso(); pt_msg_paso();                       /* puntuacion del original */
                {   /* el curso: union, bifurcacion y etapa siguiente sin parar (curso_paso) */
                    int ev = curso_paso();
                    if (cur_rama < 0 && s_decide >= 0 && yo.s >= s_decide - 450.0f && t_estado % 30 == 0) {
                        char b[64]; const char *zi = zona_nombre(curso[cur_nodo].izq), *zd = zona_nombre(curso[cur_nodo].der);
                        if (strlen(zi) + strlen(zd) > 14) sprintf(b, "< %.4s  %.4s >", curso[curso[cur_nodo].izq].zona, curso[curso[cur_nodo].der].zona);   /* cabe en el rotulo */
                        else sprintf(b, "< %s  %s >", zi, zd);
                        texto_centro(b); aviso_rama = 1;
                    }
                    if (ev & CUR_RAMA) { texto_centro(zona_nombre(cur_sig)); aviso_rama = 0; t_estado = 0; }
                    /* El checkpoint del original: el principio de la etapa siguiente (no la entrada de la union).
                       race_manager_control 4515B0: branch_state 7D38E8 = 4 al empezar; pasa a 8 al avanzar en la etapa,
                       a 1 al pisar en la union las banderas de colision 0x400/0x800 (poligonos de la posicion 8 de
                       coli_BK_PALM: la eleccion de rama), a 2 con la 0x80 (posicion 24) y 44FF20 (stage clear) se
                       dispara con 0x10 o 0x100 fuera del curso 1 (la union): solo las llevan los poligonos de la
                       posicion 0 (los primeros ~8 m) de cada etapa (banderas +0x3C del poligono de COLI0200, car+0x25C,
                       calc_ground_coli_face). Entonces la clave pasa a la de la etapa nueva (car+0x68), 450CC0 suma
                       44DC70 de esa clave (EXTEND TIME: 84490C -> 4B9630, escena 2C00AC) y da el bonus de etapa
                       4F2B20 con los cuadros desde el anterior (7D3948). En la meta no hay ampliacion (4513C0: GOAL al
                       entrar en la union de meta). */
                    if (ev & CUR_SIGUIENTE) {
                        texto_centro(zona_nombre(cur_nodo)); t_estado = 0;
                        int amp = tim_zona(cur_nodo, curso[cur_nodo].nivel);
                        pt_bonus_etapa(); tim_cuadros += amp; tiempo = tim_cuadros / TIM_HZ; t_ext = 0;
                        if (voz_ok) voces_checkpoint();         /* EXTEND TIME: efecto 0x4d y voz 0x16c */
                        dbglog(DBG_INFO, "CHECKPOINT: etapa %d (%.4s) nivel %d s %.0f, +%d cuadros, quedan %d (%.1f s)\n", cur_nodo,
                               curso[cur_nodo].zona, curso[cur_nodo].nivel, (double)yo.s, amp, tim_cuadros, (double)tiempo);
                    }
                    niebla_paso();
                    if (ev & CUR_META) { estado = META; t_estado = 0; pt_bonus_etapa(); texto_centro("GOAL!"); resultados(1); if (voz_ok) voces_meta(); }
                }
                if (estado != META && tim_cuadros <= 0) { tiempo = 0; estado = AGOTADO; t_estado = 0; texto_centro("TIEMPO AGOTADO"); resultados(0); if (voz_ok) voces_tiempo_fuera(); }
            }
        } else {                                                /* META o AGOTADO: el coche se para solo */
            coche_paso(&yo, 0, 0, 0.6f, 1.0f / 60);
            pt_msg_paso();
            if ((nuevo & CONT_START) || t_estado > 600) {
                estado = TITULO; auto_ = 0; nuevo = 0; texto_centro(""); hud_baja_tex(); menu_entra(manual_, pista, NPISTAS);
            }
        }
        /* camara del original: un paso por paso de logica; la de la salida (485250) mientras el temporizador del
           original pasa de 180 (SUPUESTO: los 60 primeros cuadros de la cuenta atras, GO! a los 240) */
        if (cam_ok && estado != TITULO && !pausa)
            camara_paso((nuevo & CONT_Y) != 0, estado == CUENTA && t_estado < 60);
        if (!pausa) coche_anima_paso(estado == TITULO);                       /* ruedas, volante, faros, carroceria (46AE70) */
        }

        /* camara: en el menu gira alrededor del coche (seleccion de coche); en carrera, la del original (camara.c) */
        if (estado == TITULO) {
            float a = menu_giro();                              /* el coche gira +100/65536 de vuelta por paso (4A5B20) */
            yo.s = SALIDA; yo.x = carril_x(SALIDA, 4); yo.v = 0; coche_paso(&yo, 0, 0, 0, 0); cam_y = yo.pos[1];
            ojo_[0] = yo.pos[0] + MENU_CAM_D * sinf(a); ojo_[1] = yo.pos[1] + MENU_CAM_Y; ojo_[2] = yo.pos[2] + MENU_CAM_D * cosf(a);
            mira[0] = yo.pos[0]; mira[1] = yo.pos[1] + MENU_CAM_MIRA; mira[2] = yo.pos[2];
        } else if (cam_ok) {
            memcpy(ojo_, cam.ojo, sizeof ojo_); memcpy(mira, cam.mira, sizeof mira);
        } else {
            {   /* sin CAM1 (PARAM.DAT antiguo): la camara hecha a mano de antes, entre el avance y la carroceria */
                float p[3], th, d = yo.deriva * (0.5f + 0.3f * fabsf(sinf(yo.deriva))), cy, fwc[3];
                ruta_en(yo.s, p, &th, NULL, NULL); cy = th + yo.a + d;
                fwc[0] = sinf(cy); fwc[2] = -cosf(cy);
                for (i = 0; i < 3; i += 2) cam_fw[i] += (fwc[i] - cam_fw[i]) * 0.15f;
            }
            norm(cam_fw);
            cam_y += (yo.pos[1] - cam_y) * 0.15f;
            ojo_[0] = yo.pos[0] - 6.5f * cam_fw[0]; ojo_[1] = cam_y + 2.4f; ojo_[2] = yo.pos[2] - 6.5f * cam_fw[2];
            mira[0] = yo.pos[0] + 6.0f * cam_fw[0]; mira[1] = cam_y + 1.1f; mira[2] = yo.pos[2] + 6.0f * cam_fw[2];
        }

        /* efectos de sonido del coche (una vez por cuadro) */
        if (sfx_ok) {
            if (estado == TITULO || pausa || !fis_ok) sfx_para();
            else {
                sfx_motor((float)fis.rpm_i, fis.ac_motor / 256.0f, fis.marcha);
                sfx_derrape(yo.t_derrape ? fminf(fabsf(fis.desliz_grados) / 30.0f, 1.0f) : 0.0f);
            }
            {   static int voz_antes = -1;                      /* las voces se cortan al volver al menu o en pausa */
                int calla = estado == TITULO || pausa;
                if (voz_ok && calla && voz_antes == 0) voces_para();
                voz_antes = calla;
            }
        }

        /* marcador (cada 6 cuadros) */
        if (cuadro % 6 == 0 && estado != META && estado != AGOTADO) {
            if (estado == TITULO) sprintf(s1, "MENU  VRAM menu %uK libre %uK", menu_vram() >> 10, (unsigned)(pvr_mem_available() >> 10));
            else sprintf(s1, "TIEMPO %3d  %3d km/h %d  %3d%%%s", (int)ceilf(tiempo), fis_ok ? (int)fis.velocidad_hud : (int)(yo.v * 3.6f), fis.marcha,
                         (int)(100 * yo.s / longitud), yo.choque ? "  !" : "");
            if (ver) {
                pvr_stats_t ps; struct mallinfo mi = mallinfo();
                pvr_get_stats(&ps);
                sprintf(s2, "%.1ffps %lutri cpu %.1f+%.1f pvr %.1fms", (double)fps, (unsigned long)tri_enviados,
                        (double)ms_log, (double)ms, (double)(ps.rnd_last_time / 1000000.0));
                sprintf(s3, "d%d/%d salto%d der%d%s ram%uK vram%uK c%.1fs x%.1f mu%.1f%.1f", (int)lejos, (int)lejos_obj, saltos_ver, (int)(yo.deriva * 57.3f), yo.t_derrape ? "D" : "",
                        (unsigned)(mi.uordblks >> 10), (unsigned)(pvr_mem_available() >> 10), (double)t_carga,
                        (double)yo.x, (double)fis.r[0].mu, (double)fis.r[2].mu);   /* desvio y mu delante/detras izq. */
            }
            texto_arriba(s1, ver ? s2 : NULL, ver ? s3 : NULL);
        }

        t_log += timer_us_gettime64() - c_log;
        if (pvr_wait_ready() < 0) { if (++pvr_fallos >= 5) pvr_colgado(estado); } else pvr_fallos = 0;
        c0 = timer_us_gettime64();
        if (estado == TITULO) pvr_set_bg_color(54 / 255.0f, 62 / 255.0f, 74 / 255.0f);   /* fondo gris del frontal */
        else pvr_set_bg_color(0.45f, 0.65f, 0.95f);
        ta_fin_cuadro();                                        /* el TA ya termino el cuadro anterior: lo que gasto */
        niebla_pon();                                           /* niebla de la etapa y distancia de dibujo */
        pvr_scene_begin();
        /* proyeccion del original (483C10 / 484BD0): pantalla virtual de 640x480 en 4:3, FOV horizontal +A4 de la
           vista y pixeles cuadrados: fx = fy = 320 / tan(FOV / 2) (vertical 2 atan(0,75 tan(FOV / 2))) */
        if (estado != TITULO && cam_ok) fx = 320.0f / tanf(cam.fov * 0.5f);
        else fx = 320.0f / (0.75f + 0.07f * fminf(yo.v / 85.0f, 1.0f));
        pon_vista(ojo_, mira, estado != TITULO && cam_ok ? cam.balanceo : 0.0f);
        pon_coche(&yo, mod_coche);
        if (cex_n && menu_color() != cex_color) coche_color(menu_color());   /* color elegido (boton Y del menu) */
        /* luces: sin SCN_EFC (bits de noche y tunel de 4A25F0) en la demo, banderas 0; freno = pedal pisado */
        coche_anima_matrices(estado == TITULO);                 /* piezas animadas (46AE70) */
        if (cex_n) coche_luces(ani.ban, estado != TITULO && yo.freno_ant > 0.0f, estado != TITULO && ani.caliente);
        if (cex_n) coche_entorno(mod_coche);
        if (cex_n) coche_luz(mod_coche, estado != TITULO);     /* la luz del entorno 2 en la pintura */
        tri_enviados = 0; vtx_ult = vtx_env; vtx_env = 0; ta_inicia_cuadro(); vtx_et_ult = vtx_et; vtx_et = 0;
        if (estado != TITULO) nodos_visibles_todos();          /* los del original que caben en el TA */
        for (l = 0; l < 3; l++) {
            pvr_list_begin(LISTA[l]);
            if (estado != TITULO) {                             /* en el menu no se ve la etapa */
                ta_en_etapa = 1;
                cielo_dibuja(l);                                /* el cielo (y su fundido en la union) */
                mat_load(&vista);
#ifdef DEP_LISTA
                color_fijo = l == DEP_LISTA ? 0xFFFF00FF : 0;
#endif
                for (i = 0; i < 3; i++)                         /* etapa, union y etapa siguiente: las que esten subidas */
                    if (tramos[i]->tv) dibuja_rango(tramos[i], l, tramos[i]->inicio[l], tramos[i]->inicio[l + 1], 1, 0, 0);
                color_fijo = 0; ta_en_etapa = 0;
            }
            if ((estado != TITULO && (!cam_ok || cam_coche_visible(&cam))) || (estado == TITULO && menu_coche3d()))
                coche_dibuja(l, &mod_coche);
#ifdef DIAG_TRAF
            if (l == 0) traf_vistos = 0;
#endif
            for (i = 0; i < NTRAF; i++) {                       /* trafico: solo el que esta delante a la vista */
                static matrix_t mt __attribute__((aligned(32)));
                trafico_t *t = traf + i; float dx = t->c.pos[0] - ojo[0], dy = t->c.pos[1] - ojo[1], dz = t->c.pos[2] - ojo[2];
                float zv = dx * ade[0] + dy * ade[1] + dz * ade[2], xv = dx * der[0] + dy * der[1] + dz * der[2];
                if (estado == TITULO || t->carril < 0 || zv < -3.0f || zv > 450.0f || fabsf(xv) > zv * (320.0f / fx) + 4.0f) continue;
#ifdef DIAG_TRAF
                if (l == 0) traf_vistos++;
#endif
                pon_coche(&t->c, mt);
                mat_load(&vista); mat_apply(&mt);
                mtraf[t->modelo].remap = t->color > 0 && t->color < traf_ncol[t->modelo] ? traf_remap[t->modelo][t->color] : NULL;
                solo_lod = traf_con_lod[t->modelo] ? traf_lod(t->modelo, zv) : -1;
                traf_dibuja(i, l, &mt);                         /* carroceria y ruedas girando (4AD530) */
                mtraf[t->modelo].remap = NULL; solo_lod = -1;
            }
            if (l == 2 && estado != TITULO) { mat_load(&vista); efectos_dibuja(); }
            if (l == 2 && estado != TITULO) sol_dibuja();       /* el sol y su destello (40CBC0) */
            if (l == 2) {
                int texto = ver || (!hud_ok && estado != TITULO);
                if (estado == TITULO) menu_dibuja();
                if (hud_ok && estado != TITULO) {
                    hud_estado_t he;
                    he.tiempo = (int)(tim_cuadros / TIM_HZ + 0.34f); he.crono_ms = (int)(crono * 1000.0f / TIM_HZ); he.puntos = pt_visible(); he.puntos_estilo = pt_estilo;
                    he.kmh = fis_ok ? (int)fis.velocidad_hud : (int)(yo.v * 3.6f); he.marcha = fis_ok ? fis.marcha : 1;
                    he.etapa = etapa_n; he.cuadro = cuadro; he.rpm = fis_ok ? (float)fis.rpm_suave : 0;
                    he.rebufo = fis_ok && fis.rebufo > 0.18f; he.derrape = yo.t_derrape != 0; he.manual = manual_ && !auto_; he.rev_on = he.manual && fis_ok && fis.rpm_i > 7900 && fis.marcha < fpar.marchas; he.seis = fis_ok && fpar.marchas >= 6;
                    hud_dibuja(&he);
                    for (i = 0; i < PT_MSGS; i++)               /* mensajes de puntos (4BD480) */
                        if (pt_msg[i].edad > 0) hud_puntos_msg(pt_msg[i].pts, pt_msg[i].estilo, (float)pt_msg[i].y, pt_msg[i].alfa);
                    if (t_cuenta < 300) {                       /* 3, 2, 1, GO! (la escena dura 5 s; GO a los 4) */
                        static const char *const CU[5] = { "CUENTA_PRE", "CUENTA_3", "CUENTA_2", "CUENTA_1", "CUENTA_GO" };
                        hud_aviso(CU[t_cuenta / 60], t_cuenta % 60, 1);
                    }
                    if (estado == CARRERA && aviso_rama) hud_aviso("BIFURCA", t_bif % 60, 1);
                    if (t_ext < 240) hud_aviso("TIEMPO_EXT", t_ext / 5, 1);
                    if (estado == META) hud_aviso("META", t_estado < 59 ? t_estado : 59, 1);
                    if (estado == AGOTADO) hud_aviso("FIN_TIEMPO", t_estado < 40 ? t_estado : 40 + (t_estado - 40) % 8, 1);
                    if (he.rev_on) hud_aviso("CAMBIA", cuadro % 30, 1);
                }
                if (estado == META || estado == AGOTADO) {      /* resultados y fundido a negro al final */
                    caja(64, 150, 512, 110, 0xA0000000);
                    rect_txr(txt_arr, 512, 128, 68, 156, 512, 128, PVR_LIST_TR_POLY);
                    if (t_estado > 540) caja(0, 0, 640, 480, (uint32_t)(fminf((t_estado - 540) / 60.0f, 1.0f) * 255) << 24);
                    texto = 0;
                }
                if (pausa && estado == CARRERA) menu_pausa_dibuja();   /* menu de pausa encima de todo */
                if (texto) caja(36, 18, 568, ver ? 92 : 34, 0x90000000);
                if (texto) rect_txr(txt_arr, 512, 128, 40, 20, 512, 128, PVR_LIST_TR_POLY);
                if (estado != TITULO && (!hud_ok ? (estado != CARRERA || t_estado < 50 || auto_ || aviso_rama) : (estado == CARRERA && auto_ && t_estado >= 50))) rect_txr(txt_cen, 256, 32, 64, 180, 512, 64, PVR_LIST_TR_POLY);
            }
            pvr_list_finish();
            if (l == 0) {                                       /* volumenes de sombra (afectan a lo opaco y recortado) */
                pvr_list_begin(PVR_LIST_OP_MOD);
                sombra_tri = sombra_teo = 0;
                if (estado != TITULO && (!cam_ok || cam_coche_visible(&cam))) { mat_load(&vista); mat_apply(&mod_coche); sombra_dibuja(mod_coche); }
                pvr_list_finish();
            }
        }
#ifdef DIAG_VTX
        if (cuadro % 60 == 0) dbglog(DBG_INFO, "DIAG est%d s %.0f/%.0f ram %uK/%uK v%u (etapa %u) tri %u ms %.1f+%.1f lejos %d (%.2f) vram %u ta %uK etapa %uK/%uK 2d %u f %u real %uK opb %uK tiras %u teo %uK orig %u cerca %d m clase %d alfa %.2f quitados %u desde %d m (r %.1f) niebla %.5f sombra %u tri %uK%s%s\n", estado,
                                     (double)yo.s, (double)longitud, (unsigned)(mallinfo().uordblks >> 10), (unsigned)(mallinfo().fordblks >> 10),
                                     (unsigned)vtx_env, (unsigned)vtx_et, (unsigned)tri_enviados, (double)ms_log, (double)ms, (int)lejos, (double)(lejos / LEJOS_PS2), (unsigned)pvr_mem_available(), (unsigned)(ta_vb >> 10),
                                     (unsigned)(ta_et >> 10), (unsigned)(ta_tope_etapa >> 10), (unsigned)ta_v2d, (unsigned)ta_f, (unsigned)(ta_real_ult >> 10), (unsigned)(ta_opb_ult >> 10),
                                     (unsigned)ta_tiras, (unsigned)(ta_teo_3d >> 10), (unsigned)diag_orig_vtx, (int)(prio_d >= ND_PRIO - 1 ? lejos : powf(2.0f, (prio_d + 1) / 4.0f)), prio_p, (double)prio_alfa, (unsigned)diag_quitado_n,
                                     (int)(diag_quitado_n ? diag_quitado_z : lejos), (double)diag_quitado_r, (double)niebla_d, (unsigned)sombra_tri, (unsigned)(sombra_teo >> 10), ta_et_corto ? " CORTO" : "", ta_corto ? " TOPE" : "");
#endif
        if (estado != TITULO) prio_ajusta(ta_et_corto, ta_et);
        ta_et_corto = 0;
        pvr_scene_finish();
        hudd_fin_cuadro();                                      /* VRAM del HUD y los menus que ya no lee la PowerVR */
        tcpu += timer_us_gettime64() - c0;
        if (++cuadro % 30 == 0) {
            uint64_t t1 = timer_ms_gettime64();
            fps = 30000.0f / (float)(t1 - t0); ms = tcpu / 30000.0f; ms_log = t_log / 30000.0f; t0 = t1; tcpu = 0; t_log = 0;
            /* distancia adaptativa: si la geometria no cabe en el cuadro, se acorta (minimo 600 m) y vuelve a crecer */
            /* ta_corto: algun cuadro se quedo sin presupuesto (de vertices de la etapa o del TA) y se saltaron mallas;
               se acorta la distancia para que lo que falte sea lo de lejos (antes contaba el coche y el trafico en el
               tope de la etapa y la distancia acababa siempre en el minimo) */
            /* el presupuesto del TA ya lo reparte nodos_visibles_todos: aqui solo la CPU (DIAG_SIN_CPU: para medir en
               Flycast, que no da el tiempo del SH4 de la consola, sin este recorte) */
#ifndef DIAG_SIN_CPU
            if (ms + ms_log > 15.0f) {                      /* acorta (seguridad: la CPU no llega al cuadro) */
                if (lejos > LEJOS_MIN) lejos -= 50.0f;
            } else if (ms + ms_log < 11.5f) {
                if (lejos < lejos_obj) lejos += 50.0f;         /* vuelve a la del original */
            }
#endif
            ta_corto = 0;
            saltos_ver = saltos; saltos = 0;
        }
    }
    return 0;
}
