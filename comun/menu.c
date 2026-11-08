/* Menus del modo principal con MENU.DAT (tools/conv_menu.py): mismo formato que HUD.DAT mas el bloque MNU1 con las
   pantallas (rango de texturas propio) y los tramos de animacion (uso, cuadro inicial, final, modo).
   Flujo del original (port de PC con los tokens del PS2):
     titulo (START/A) -> menu principal -> un jugador -> coche -> cambio AT/MT -> musica -> carrera; B vuelve atras.
   Cursores con los tramos MUEVEij (20 cuadros por paso; 40 en la musica), entradas ENTRA, cambio con AT/MT en bucle y
   AT_OK/MT_OK o AT_ATRAS/MT_ATRAS al confirmar o volver, radio de la musica ENTRA/SALE.
   Demo: solo el Testarossa (coche 8), los modos COAST 2 COAST y OUTRUN (los dos a la misma carrera) y los temas con
   pista en el disco (MUSn); lo demas queda bloqueado (candado o sacudida).
   Sonido (como el original): efectos del frontal con sfx_menu (cursor, aceptar, bloqueado, cancelar); el titulo va
   sin musica (en el original suena la pelicula), al salir de el arranca la musica de menu (pista 30 de MUSIC.PS2,
   MUS30.ADP); en la musica cada tema suena con su avance (pista 33 + tema, MUSnn.ADP; si no esta, el tema entero) y
   al confirmar empieza el tema elegido; al volver de la musica vuelve la de menu. Las pistas se convierten con
   tools/c/musica.c (desplazamiento 0, intercalado 0x2000), como MUS0-2.
   VRAM: en la RAM solo las tablas de MENU.DAT; las texturas de cada pantalla se leen del GD al entrar y se quedan
   mientras quepan (se echa la pantalla usada hace mas tiempo); menu_sale() lo libera todo antes de la carrera. */
#include <kos.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "hud.h"
#include "musica.h"
#include "sfx.h"
#include "menu.h"

typedef struct { char nombre[16]; uint16_t g0, ng, u0, nu, t0, nt; uint32_t vram, _r[4]; } m_pant_t;
typedef struct { char nombre[16]; uint16_t uso, ini, fin, modo; uint32_t _r[2]; } m_tramo_t;

enum { P_TITULO, P_COMUN, P_MENU, P_MODO, P_COCHE, P_CAMBIO, P_MUSICA, NP };
static const char *const PNOM[NP] = { "TITULO", "COMUN", "MENU", "MODO", "COCHE", "CAMBIO", "MUSICA" };
/* pantallas de VRAM que necesita cada pantalla del menu */
static const int MASCARA[NP] = { 1 << P_TITULO, 0, (1 << P_COMUN) | (1 << P_MENU), (1 << P_COMUN) | (1 << P_MODO),
                                 (1 << P_COMUN) | (1 << P_COCHE), (1 << P_COMUN) | (1 << P_COCHE) | (1 << P_CAMBIO),
                                 (1 << P_COMUN) | (1 << P_MUSICA) };
enum { U_TFONDO, U_TLOGO, U_TSTART, U_FONDO, U_CABMENU, U_PRINC, U_CABMODO, U_UNJUG, U_CABCOCHE, U_INFO, U_CAND,
       U_CAMBIO, U_RADIO, U_AYUDA, U_L0, U_T0, U_LISTA, U_MCAND, NU };
static const char *const UNOM[NU] = { "TITULO_FONDO", "TITULO_LOGO", "TITULO_START", "FONDO", "CABECERA_MENU",
    "MODO_PRINCIPAL", "CABECERA_MODO", "MODO_UN_JUGADOR", "CABECERA_COCHE", "COCHE_INFO", "COCHE_CANDADO", "CAMBIO",
    "MUSICA_RADIO", "MUSICA_AYUDA", "MUSICA_L0", "MUSICA_T0", "MUSICA_LISTA", "MUSICA_CANDADO" };

#define N_OPC 5                                             /* opciones del menu principal y de un jugador */
#define N_COCHES 15
#define COCHE_DEMO 8                                        /* Testarossa */
#define N_TEMAS 9                                           /* lista 0 */
#define T_DEMO (8 * 60)                                     /* inactividad en el titulo hasta la demo */
#define T_FUNDIDO 12
#define PRECARGA_MARGEN (128 * 1024)                       /* VRAM que se deja libre al precargar */
#define PISTA_MENU 30                                       /* musica de los menus */
#define PISTA_AVANCE 33                                     /* avance del tema k: pista 33 + k */

static hud_dat_t *md;
static const m_pant_t *pan; static const m_tramo_t *tra; static int n_pan, n_tra;
static int pidx[NP], cargada[NP], us[NU]; static uint32_t edad[NP], reloj;

typedef struct { int u; float c; int ini, fin, modo; } anim_t;
enum { F_ENTRA, F_QUIETO, F_SALE };
static anim_t a_cab, a_sel, a_aux, a_radio, a_ayuda;
static int pant, fase, t, sig, inact, t_bloq, entra_atras;
static int c_menu, c_modo, c_coche = COCHE_DEMO, manual, tema, n_ok, pista_sonando, sale_ok;

/* ---------------------------------------------------------------- tramos */
static int tramo(int u, const char *n, int *ini, int *fin, int *modo)
{
    int i;
    for (i = 0; i < n_tra; i++)
        if (tra[i].uso == u && !strncmp(tra[i].nombre, n, 16)) { *ini = tra[i].ini; *fin = tra[i].fin; *modo = tra[i].modo; return 1; }
    return 0;
}
static void an_va(anim_t *a, int ini, int fin, int modo) { a->ini = ini; a->fin = fin; a->modo = modo; a->c = ini; }
static void an_tramo(anim_t *a, const char *n)
{
    int i, f, m;
    if (tramo(a->u, n, &i, &f, &m)) an_va(a, i, f, m); else an_va(a, 0, 0, 2);
}
static void an_tramo_n(anim_t *a, const char *fmt, int k) { char b[32]; sprintf(b, fmt, k); an_tramo(a, b); }
/* MUEVEij hacia delante o hacia atras (de j a i) */
static void an_mueve(anim_t *a, int de, int a_) {
    char b[32]; int i, f, m, lo = de < a_ ? de : a_;
    sprintf(b, "MUEVE%d%d", lo, lo + 1);
    if (!tramo(a->u, b, &i, &f, &m)) return;
    if (de < a_) an_va(a, i, f, 0); else an_va(a, f, i, 0);
}
static void an_paso(anim_t *a)
{
    if (a->modo == 2) { a->c = a->ini; return; }
    if (a->fin >= a->ini) { if (++a->c > a->fin) a->c = a->modo == 1 ? a->ini : a->fin; }
    else if (--a->c < a->fin) a->c = a->modo == 1 ? a->ini : a->fin;
}
static int an_hecho(const anim_t *a) { return a->modo == 2 || (a->modo == 0 && (int)a->c == a->fin); }
static void an_dibuja(const anim_t *a, float dx, float alfa, float z, int fondo)
{
    hudd_dibuja(md, a->u, (int)a->c, dx, 0, alfa, z, fondo);
}

/* ---------------------------------------------------------------- VRAM */
static int sube_p(int p)
{
    const m_pant_t *q = pan + pidx[p];
    if (pidx[p] < 0) return -1;
    if (!cargada[p]) {
        if (hudd_sube(md, q->t0, q->nt)) return -1;
        cargada[p] = 1;
    }
    edad[p] = ++reloj;
    return 0;
}
static void baja_p(int p)
{
    if (cargada[p]) { hudd_baja(md, pan[pidx[p]].t0, pan[pidx[p]].nt); cargada[p] = 0; }
}
/* sube las pantallas de la mascara; si no caben, echa la cargada hace mas tiempo que no haga falta */
static void asegura(int mascara)
{
    int p, q, v;
    for (p = 0; p < NP; p++) {
        if (!(mascara & (1 << p)) || pidx[p] < 0) continue;
        for (;;) {
            if (cargada[p]) { edad[p] = ++reloj; break; }
            if (pvr_mem_available() >= pan[pidx[p]].vram + 16384 && !sube_p(p)) break;
            for (q = 0, v = -1; q < NP; q++)
                if (cargada[q] && !(mascara & (1 << q)) && (v < 0 || edad[q] < edad[v])) v = q;
            if (v < 0) { sube_p(p); break; }                /* ultimo intento (si falla, se ve lo que haya) */
            baja_p(v);
        }
    }
}
unsigned menu_vram(void)
{
    unsigned s = 0; int p;
    for (p = 0; p < NP; p++) if (cargada[p]) s += pan[pidx[p]].vram;
    return s;
}
void menu_sale(void) { int p; for (p = 0; p < NP; p++) baja_p(p); }

/* ---------------------------------------------------------------- carga */
int menu_inicia(void)
{
    const uint8_t *x; int i, p;
    if (!(md = hudd_abre("MENU.DAT", 0, 1))) return -1;
    x = hudd_extra(md);
    if (memcmp(x, "MNU1", 4)) { md = NULL; return -2; }
    n_pan = ((const uint32_t *)x)[1]; n_tra = ((const uint32_t *)x)[2];
    pan = (const m_pant_t *)(x + 16); tra = (const m_tramo_t *)(x + 16 + 48 * n_pan);
    for (p = 0; p < NP; p++)
        for (pidx[p] = -1, i = 0; i < n_pan; i++) if (!strncmp(pan[i].nombre, PNOM[p], 16)) pidx[p] = i;
    for (i = 0; i < NU; i++) us[i] = hudd_uso(md, UNOM[i]);
    return 0;
}

/* ---------------------------------------------------------------- pantallas */
static int cabecera(int p) { return p == P_MENU ? U_CABMENU : p == P_MODO ? U_CABMODO : (p == P_COCHE || p == P_CAMBIO) ? U_CABCOCHE : -1; }

/* pista n de la musica (MUSn) si no es la que suena; -1 = silencio. Devuelve 0 si suena */
static int pon_pista(int n)
{
    if (n == pista_sonando) return 0;
    if (n < 0) { musica_para(); pista_sonando = -1; return 0; }
    if (musica_pon(n)) { pista_sonando = -1; return -1; }
    pista_sonando = n;
    return 0;
}
/* en la pantalla de musica: el avance del tema k (o el tema entero si no esta) */
static void pon_musica(int k)
{
    if (k < n_ok && pista_sonando != PISTA_AVANCE + k && pista_sonando != k && pon_pista(PISTA_AVANCE + k)) pon_pista(k);
}
/* musica de los menus (sin MUS30: el tema elegido) */
static void musica_menu(void)
{
    if (pon_pista(PISTA_MENU) && pista_sonando < 0) pon_pista(tema < n_ok ? tema : 0);
}

static void entra(int p, int atras)
{
    int cab = cabecera(p);
    asegura(MASCARA[p]);
    pant = p; fase = F_ENTRA; t = 0; entra_atras = atras; t_bloq = 0;
    if (cab < 0) a_cab.u = -1;
    else if (us[cab] != a_cab.u) { a_cab.u = us[cab]; an_tramo(&a_cab, "ENTRA"); }   /* cabecera nueva */
    switch (p) {
    case P_TITULO: fase = F_QUIETO; pon_pista(-1); break;
    case P_MENU: case P_MODO: {
        int c = p == P_MENU ? c_menu : c_modo;
        a_sel.u = us[p == P_MENU ? U_PRINC : U_UNJUG];
        if (atras || c) an_tramo_n(&a_sel, "SEL%d", c); else an_tramo(&a_sel, "ENTRA");
        break; }
    case P_COCHE: a_sel.u = us[U_INFO]; an_tramo_n(&a_sel, "COCHE%d", c_coche); break;
    case P_CAMBIO: a_sel.u = us[U_CAMBIO]; an_tramo(&a_sel, "ENTRA"); break;
    case P_MUSICA:
        a_radio.u = us[U_RADIO]; an_tramo(&a_radio, "ENTRA");
        a_ayuda.u = us[U_AYUDA]; an_tramo(&a_ayuda, "ENTRA");
        a_sel.u = us[U_L0]; if (tema) an_tramo_n(&a_sel, "TEMA%d", tema); else an_tramo(&a_sel, "ENTRA");
        a_aux.u = us[U_T0]; an_tramo_n(&a_aux, "TEMA%d", tema);
        pon_musica(tema);
        break;
    }
}

void menu_entra(int man, int pista, int npistas)
{
    manual = man; tema = pista; pista_sonando = -2; n_ok = npistas < N_TEMAS ? npistas : N_TEMAS;
    inact = 0; a_cab.u = -1; pant = -1;
    if (md) entra(P_TITULO, 0);
}

int menu_manual(void) { return manual; }
int menu_pista(void) { return tema < n_ok ? tema : 0; }
int menu_coche3d(void) { return md && pant == P_COCHE && c_coche == COCHE_DEMO && fase != F_SALE; }

static void sale(int destino, int ok) { fase = F_SALE; t = 0; sig = destino; sale_ok = ok; sfx_menu(ok ? SFX_ACEPTA : SFX_CANCELA); }
static void bloqueado(void) { t_bloq = 14; sfx_menu(SFX_BLOQUEADO); }

/* entrada en las pantallas en reposo */
static void control(unsigned n)
{
    int ok = n & (CONT_A | CONT_START), atras = n & CONT_B, izq = n & CONT_DPAD_LEFT, der = n & CONT_DPAD_RIGHT;
    switch (pant) {
    case P_TITULO:
        if (ok) { sale(P_MENU, 1); musica_menu(); }
        break;
    case P_MENU: case P_MODO: {
        int *c = pant == P_MENU ? &c_menu : &c_modo;
        if (!an_hecho(&a_sel)) break;
        if (izq && *c > 0) { an_mueve(&a_sel, *c, *c - 1); (*c)--; sfx_menu(SFX_CURSOR); }
        else if (der && *c < N_OPC - 1) { an_mueve(&a_sel, *c, *c + 1); (*c)++; sfx_menu(SFX_CURSOR); }
        else if (ok) {
            if (pant == P_MENU) { if (*c == 0) sale(P_MODO, 1); else bloqueado(); }   /* SINGLE PLAYER */
            else if (*c <= 1) sale(P_COCHE, 1); else bloqueado();                    /* COAST 2 COAST, OUTRUN */
        } else if (atras) sale(pant == P_MENU ? P_TITULO : P_MENU, 0);
        break; }
    case P_COCHE:
        if (!an_hecho(&a_sel)) break;
        if (izq && c_coche > 0) { an_mueve(&a_sel, c_coche, c_coche - 1); c_coche--; sfx_menu(SFX_CURSOR); }
        else if (der && c_coche < N_COCHES - 1) { an_mueve(&a_sel, c_coche, c_coche + 1); c_coche++; sfx_menu(SFX_CURSOR); }
        else if (ok) { if (c_coche == COCHE_DEMO) sale(P_CAMBIO, 1); else bloqueado(); }
        else if (atras) sale(P_MODO, 0);
        break;
    case P_CAMBIO:
        if (izq && manual) { manual = 0; an_tramo(&a_sel, "AT"); sfx_menu(SFX_CURSOR); }
        else if (der && !manual) { manual = 1; an_tramo(&a_sel, "MT"); sfx_menu(SFX_CURSOR); }
        else if (ok) { an_tramo(&a_sel, manual ? "MT_OK" : "AT_OK"); sale(P_MUSICA, 1); }
        else if (atras) { an_tramo(&a_sel, manual ? "MT_ATRAS" : "AT_ATRAS"); sale(P_COCHE, 0); }
        break;
    case P_MUSICA:
        if (!an_hecho(&a_sel)) break;
        if ((izq && tema > 0) || (der && tema < N_TEMAS - 1)) {
            int k = tema + (der ? 1 : -1);
            an_mueve(&a_sel, tema, k); tema = k; an_tramo_n(&a_aux, "TEMA%d", tema); sfx_menu(SFX_CURSOR);
        } else if (n & CONT_Y) {                            /* tema al azar (de los que hay) */
            int k = n_ok > 1 ? (tema + 1 + rand() % (n_ok - 1)) % n_ok : 0;
            if (tema >= n_ok) k = rand() % n_ok;
            tema = k; an_tramo_n(&a_sel, "TEMA%d", tema); an_tramo_n(&a_aux, "TEMA%d", tema); pon_musica(tema);
            sfx_menu(SFX_CURSOR);
        } else if (n & CONT_X) bloqueado();                 /* otras listas: no estan en la demo */
        else if (ok) { if (tema < n_ok) { an_va(&a_radio, 25, 0, 0); sale(-1, 1); } else bloqueado(); }
        else if (atras) { an_va(&a_radio, 25, 0, 0); sale(P_CAMBIO, 0); musica_menu(); }
        break;
    }
}

int menu_paso(unsigned n)
{
    if (n) inact = 0; else inact++;
    if (!md) return (n & CONT_START) ? MENU_CARRERA : inact > T_DEMO ? MENU_DEMO : MENU_SIGUE;
    t++;
    if (t_bloq) t_bloq--;
    if (a_cab.u >= 0) { an_paso(&a_cab); if (an_hecho(&a_cab) && a_cab.modo == 0) an_tramo(&a_cab, "QUIETA"); }
    {   int estaba = an_hecho(&a_sel);
        an_paso(&a_sel); an_paso(&a_aux); an_paso(&a_radio); an_paso(&a_ayuda);
        /* fin de un movimiento del cursor: se queda quieto en la opcion; en la musica suena el tema */
        if (!estaba && an_hecho(&a_sel) && fase != F_SALE) {
            if (pant == P_MENU || pant == P_MODO) an_tramo_n(&a_sel, "SEL%d", pant == P_MENU ? c_menu : c_modo);
            else if (pant == P_COCHE) an_tramo_n(&a_sel, "COCHE%d", c_coche);
            else if (pant == P_MUSICA) { an_tramo_n(&a_sel, "TEMA%d", tema); pon_musica(tema); }
            else if (pant == P_CAMBIO) an_tramo(&a_sel, manual ? "MT" : "AT");
        }
    }
    if (fase == F_ENTRA && t >= T_FUNDIDO && (an_hecho(&a_sel) || a_sel.modo == 1)) fase = F_QUIETO;
    if (pant == P_TITULO && fase == F_QUIETO && inact > T_DEMO) { inact = 0; pon_pista(menu_pista()); return MENU_DEMO; }
    /* precarga de la pantalla siguiente cuando sobra VRAM (la lectura del GD se nota menos con la pantalla quieta) */
    if (fase == F_QUIETO && t == 40 && pant >= 0 && pant < P_MUSICA) {
        int p, sigp = pant == P_TITULO ? P_MENU : pant + 1, falta = 0;
        for (p = 0; p < NP; p++) if ((MASCARA[sigp] & (1 << p)) && pidx[p] >= 0 && !cargada[p]) falta += pan[pidx[p]].vram;
        if (falta && pvr_mem_available() > (size_t)(falta + PRECARGA_MARGEN)) asegura(MASCARA[sigp] | MASCARA[pant]);
    }
    if (fase == F_QUIETO) control(n);
    else if (fase == F_SALE) {
        int fin = t >= (pant == P_TITULO ? 30 : T_FUNDIDO);
        if (pant == P_CAMBIO || pant == P_MUSICA) fin = fin && an_hecho(pant == P_CAMBIO ? &a_sel : &a_radio);
        if (fin) {
            if (sig < 0) { pista_sonando = -2; pon_pista(menu_pista()); return MENU_CARRERA; }   /* el tema, desde el principio */
            entra(sig, !sale_ok);
        }
    }
    return MENU_SIGUE;
}

/* ---------------------------------------------------------------- dibujo
   capas z de 1 a 8 (por debajo del panel de depuracion de la demo, z 9-10); el fondo, detras del coche 3D */
void menu_dibuja(void)
{
    float a_ent = fase == F_ENTRA ? fminf(t / (float)T_FUNDIDO, 1.0f) : 1.0f;
    float a_sal = fase == F_SALE ? fmaxf(1.0f - t / (float)T_FUNDIDO, 0.0f) : 1.0f;
    float sacude = t_bloq ? sinf(t_bloq * 1.6f) * t_bloq * 0.6f : 0.0f;
    int cab = cabecera(pant), cab_sale = fase == F_SALE && cabecera(sig < 0 ? P_MUSICA : sig) != cab;
    if (!md || pant < 0) return;
    if (pant == P_TITULO) {
        float a = 1;
        if (fase == F_SALE) a = (t / 3) & 1 ? 0.2f : 1.0f; /* parpadeo rapido al pulsar */
        else if ((t % 64) >= 44) a = 0;
        hudd_dibuja(md, us[U_TFONDO], 0, 0, 0, 1, 1, 0);
        hudd_dibuja(md, us[U_TLOGO], 0, 0, 0, 1, 2, 0);
        hudd_dibuja(md, us[U_TSTART], 0, 0, 0, a, 3, 0);
        return;
    }
    hudd_dibuja(md, us[U_FONDO], 0, 0, 0, 1, 0.0002f, 1);  /* detras del coche 3D */
    if (cab >= 0) an_dibuja(&a_cab, 0, cab_sale ? a_sal : 1, 6, 0);
    switch (pant) {
    case P_MENU: case P_MODO:
        an_dibuja(&a_sel, sacude, (entra_atras ? a_ent : 1) * a_sal, 3, 0);
        break;
    case P_COCHE:
        an_dibuja(&a_sel, sacude, a_ent * a_sal, 4, 0);
        if (c_coche != COCHE_DEMO && an_hecho(&a_sel)) hudd_dibuja(md, us[U_CAND], 0, sacude, 0, a_ent * a_sal, 5, 0);
        break;
    case P_CAMBIO:
        an_dibuja(&a_sel, 0, 1, 7, 0);
        break;
    case P_MUSICA: {
        an_dibuja(&a_radio, 0, 1, 4, 0);                    /* la radio sale con su propio tramo */
        an_dibuja(&a_ayuda, 0, a_sal, 4.5f, 0);
        an_dibuja(&a_sel, sacude, a_sal, 3, 0);
        an_dibuja(&a_aux, 0, a_sal, 5, 0);
        hudd_dibuja(md, us[U_LISTA], 0, 0, 0, a_sal, 5.5f, 0);
        if (tema >= n_ok && an_hecho(&a_sel)) hudd_dibuja(md, us[U_MCAND], 0, sacude, 0, a_sal, 6, 0);
        break; }
    }
}
