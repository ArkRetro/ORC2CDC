/* Menus del modo principal con MENU.DAT (tools/conv_menu.py): mismo formato que HUD.DAT mas el bloque MNU1 con las
   pantallas (rango de texturas propio) y los tramos de animacion (uso, cuadro inicial, final, modo), y el MNU2 con los
   textos de TEXT.PS2 que se escriben en la consola, las metricas de la fuente 14 y las tablas de la seleccion de coche.
   Flujo del original (port de PC, 4D7300 / 4C9290 / 4E2970 / 4DE2B0, con los tokens del PS2):
     titulo (START/A) -> sin licencia: elegir licencia (crear) -> menu principal (SINGLE PLAYER, MULTIPLAYER, OUTRUN2SP,
     RANKINGS, OPTIONS) -> un jugador (COAST 2 COAST, OUTRUN, HEART ATTACK, TIME ATTACK, SHOWROOM) -> coche -> cambio
     AT/MT -> musica -> carrera; B vuelve atras. LICENSE (X en el menu principal y en un jugador) abre la eleccion de
     licencia. HEART ATTACK (INFARTO) y TIME ATTACK (CONTRARRELOJ) llevan a la carrera con su variante (menu_modo);
     lo que necesita modos que la demo no tiene (multijugador, rankings, SHOWROOM) tiene su pantalla y la accion
     bloqueada (sacudida y sonido 3, como un coche sin desbloquear). OUTRUN2SP lleva a la carrera del juego de OUTRUN2SP
     (menu_sp) por su frontal de recreativa (comun/fsp.c).
   Cursores con los tramos MUEVEij (20 cuadros por paso; 40 en la musica), entradas ENTRA, cambio con AT/MT en bucle y
   AT_OK/MT_OK o AT_ATRAS/MT_ATRAS al confirmar o volver, radio de la musica ENTRA/SALE. Cabeceras: ENTRA y luego
   BRILLO (destello del rotulo en bucle). Barra de ayuda de cada pantalla (AYUDA_*: botones y textos del original,
   ya compuestos en MENU.DAT con la fuente del juego), en COMUN.
   Seleccion de coche (4C9010 / 4C9290 del PC = 0x2445D8 del PS2): carrusel 0x4400E5 (15 normales) o 0x4400E6 (15
   "_t"), ficha = cursor + 15 * variante, modelo de la tabla del ELF; desbloqueo por los bits de la licencia (+0x28;
   las 2 primeras siempre), colores con su bit (-1 siempre, -2 nunca) y el siguiente desbloqueado con Y; candado en
   las bloqueadas; el gatillo R cambia de variante (bit 0x2000 del frontal: R1 / R2 del PS2, ver lee_r1). Las "_t" no
   estan convertidas: se ve y se corre con el modelo normal (PROVISIONAL). El coche gira +100/65536 de vuelta por
   paso (4A5B20).
   Licencia: hasta 4 (como el original), registro de 0x40C bytes con la disposicion del original (nombre +0, signo +0x18,
   bandera +0x1C, foto +0x20, desbloqueos +0x28, opciones +0xD8.., en uso +0x3F4 bit 0), nueva con los valores de
   4471A0 y el nombre 0x219; en la VMU A1 (fichero OR2C2C, vmu_pkg con el icono y el eyecatch de VMU.DAT).
   Sonido (como el original): efectos del frontal con sfx_menu (cursor, aceptar, bloqueado, cancelar); el titulo va
   sin musica (en el original suena la pelicula), al salir de el arranca la musica de menu (pista 30 de MUSIC.PS2,
   MUS30.ADP); en la musica cada tema suena con su avance (pista 33 + tema) y al confirmar empieza el tema elegido.
   Musica: las 3 listas del original (9 / 7 / 12 temas, 28 en total; el gatillo R pasa a la siguiente: el bit 0x2000,
   R1 / R2 del PS2), candado y sonido 3 en los temas sin su bit de la licencia, RANDOM (Y) elige uno desbloqueado y sale a correr.
   VRAM: en la RAM solo las tablas de MENU.DAT; las texturas de cada pantalla se leen del GD al entrar y se quedan
   mientras quepan (se echa la pantalla usada hace mas tiempo). hud.c libera la VRAM unos cuadros despues de su ultimo
   uso (la PowerVR aun dibuja el cuadro anterior): una pantalla que no cabe espera a que se libere (sin dibujarse a
   medias). menu_sale() lo libera todo antes de la carrera. */
#include <kos.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "hud.h"
#include "musica.h"
#include "sfx.h"
#include "menu.h"
#include "rksp.h"
#include "vmu.h"
#include "fsp.h"

void (*menu_gancho)(int pantalla, int evento, int valor);
void (*menu_suelta_coche)(void);
#define GANCHO(p, e, v) do { if (menu_gancho) menu_gancho(p, e, v); } while (0)

typedef struct { char nombre[16]; uint16_t g0, ng, u0, nu, t0, nt; uint32_t vram, _r[4]; } m_pant_t;
typedef struct { char nombre[16]; uint16_t uso, ini, fin, modo; uint32_t _r[2]; } m_tramo_t;

/* pantallas de VRAM (las de conv_menu.py) */
enum { V_TITULO, V_COMUN, V_MENU, V_MODO, V_COCHE, V_CAMBIO, V_MUSICA, V_LICENCIA, V_RANKING, V_MULTI, V_OPCIONES, V_PAUSA,
       V_MUS0, V_MUS1, V_MUS2, NV };                        /* V_MUSn: carrusel y titulos de la lista n de la musica */
static const char *const VNOM[NV] = { "TITULO", "COMUN", "MENU", "MODO", "COCHE", "CAMBIO", "MUSICA", "LICENCIA",
                                      "RANKING", "MULTI", "OPCIONES", "PAUSA", "MUSICA0", "MUSICA1",
                                      "MUSICA2" };
/* pantallas del menu (las de menu.h, MP_*) y la VRAM que necesita cada una */
enum { P_TITULO, P_MENU, P_MODO, P_COCHE, P_CAMBIO, P_MUSICA, P_LICENCIA, P_EDITOR, P_OPCIONES, P_RANKING, P_MULTI, NP };
#define VB(v) (1 << (v))
static const int MASCARA[NP] = {
    VB(V_TITULO), VB(V_COMUN) | VB(V_MENU), VB(V_COMUN) | VB(V_MODO), VB(V_COMUN) | VB(V_COCHE),
    VB(V_COMUN) | VB(V_COCHE) | VB(V_CAMBIO), VB(V_COMUN) | VB(V_MUSICA), VB(V_COMUN) | VB(V_LICENCIA),
    VB(V_COMUN) | VB(V_LICENCIA), VB(V_COMUN) | VB(V_OPCIONES), VB(V_COMUN) | VB(V_RANKING), VB(V_COMUN) | VB(V_MULTI) };

enum { U_TFONDO, U_TLOGO, U_TSTART, U_FONDO, U_CABMENU, U_PRINC, U_CABMODO, U_UNJUG, U_CABCOCHE, U_INFO, U_INFO_T,
       U_CAND, U_CAMBIO, U_RADIO, U_AYUDA, U_L0, U_T0, U_LISTA, U_MCAND, U_CABMUSICA,
       U_BARRA_MENU, U_BARRA_MODO, U_BARRA_COCHE, U_BARRA_CAMBIO, U_BARRA_MUSICA, U_BARRA_LIC, U_BARRA_OPC, U_FUENTE,
       U_CABLIC, U_TARJ, U_FL_D, U_FL_I, U_EDITOR, U_TARJ_E, U_REJ_FOTO, U_REJ_BAND, U_REJ_SIGNO, U_HECHO, U_FOTO,
       U_BAND, U_SIGNO, U_LIC_BARRA, U_CABRANK, U_RANK, U_CABMULTI, U_MULTI, U_CABOPC, U_OPC_BARRA,
       U_P_FILA, U_P_BARRA, U_P_OPC, U_P_VOL, U_P_FUENTE, U_L1, U_L2, U_T1, U_T2, NU };
static const char *const UNOM[NU] = { "TITULO_FONDO", "TITULO_LOGO", "TITULO_START", "FONDO", "CABECERA_MENU",
    "MODO_PRINCIPAL", "CABECERA_MODO", "MODO_UN_JUGADOR", "CABECERA_COCHE", "COCHE_INFO", "COCHE_INFO_T",
    "COCHE_CANDADO", "CAMBIO", "MUSICA_RADIO", "MUSICA_AYUDA", "MUSICA_L0", "MUSICA_T0", "MUSICA_LISTA",
    "MUSICA_CANDADO", "CABECERA_MUSICA",
    "AYUDA_MENU", "AYUDA_MODO", "AYUDA_COCHE", "AYUDA_CAMBIO", "AYUDA_MUSICA", "AYUDA_LICENCIA", "AYUDA_OPCIONES",
    "FUENTE", "CABECERA_LICENCI", "LIC_TARJETA", "LIC_FLECHA_D", "LIC_FLECHA_I", "LIC_EDITOR", "LIC_TARJETA_E",
    "LIC_REJ_FOTO", "LIC_REJ_BANDERA", "LIC_REJ_SIGNO", "LIC_HECHO", "LIC_FOTO", "LIC_BANDERA", "LIC_SIGNO",
    "LIC_BARRA", "CABECERA_RANKING", "MODO_RANKING", "CABECERA_MULTI", "MODO_MULTI", "CABECERA_OPCIONE", "OPC_BARRA",
    "PAUSA_FILA", "PAUSA_BARRA", "PAUSA_OPC", "PAUSA_VOL", "FUENTE_PAUSA", "MUSICA_L1", "MUSICA_L2", "MUSICA_T1",
    "MUSICA_T2" };

#define N_OPC 5                                             /* opciones del menu principal y de un jugador */
#define N_COCHES 15
/* musica (4C9790 / 4C9C20 del PC; tablas del PS2 0x375DA8 y 0x4191E8): 3 listas de 9 / 7 / 12 temas (68F36C), el
   tema k (0..27, 4C9750: lista 0, luego la 1 y la 2) es la pista k de MUSIC.PS2 (68F370: la misma) y su avance la
   k + 0x21; los temas 0..6 siempre estan y los demas con su bit de la licencia (5C9AB0) */
#define N_LISTAS 3
#define N_TEMAS 28
static const int8_t TEMAS_LISTA[N_LISTAS] = { 9, 7, 12 };
static const uint8_t DESBLOQ_TEMA[N_TEMAS] = { 174, 174, 174, 174, 174, 174, 174, 172, 173, 174, 175, 176, 177, 178,
                                               179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192 };
#define T_DEMO (8 * 60)                                     /* inactividad en el titulo hasta la demo */
#define T_FUNDIDO 12
#define T_CARGA_MAX 90                                      /* cuadros que se espera a la VRAM de una pantalla */
#define T_SUELTA_COCHE 2                                    /* cuadros sin VRAM hasta soltar el coche 3D */
#define T_REORDENA 20                                       /* ... y hasta bajar todas las pantallas y subirlas otra vez */
#define PRECARGA_MARGEN (128 * 1024)                        /* VRAM que se deja libre al precargar */
#define HOLGURA (256 * 1024)                                /* libre de mas antes de subir sin echar nada */
#define PISTA_MENU 30                                       /* musica de los menus */
#define PISTA_AVANCE 33                                     /* avance del tema k: pista 33 + k */

static hud_dat_t *md;
static const m_pant_t *pan; static const m_tramo_t *tra; static int n_pan, n_tra;
static int pidx[NV], cargada[NV], us[NU]; static uint32_t edad[NV], reloj;

/* ---------------------------------------------------------------- MNU2: textos, fuente y tablas de coches */
static const uint8_t *mnu2; static int n_txt;
static int f_w, f_h, f_pri, f_n, f_esp, f_ker; static const int8_t *f_met, *f_inter;
static const uint8_t *t_modelo; static const int16_t *t_desbloq, *t_colbit; static const int8_t *t_colini;
static uint32_t col_tarjeta = 0x2c3650;
static const uint8_t *prg;                                  /* bloque PRG1 de MNU2 (conv_menu.py progreso) */

static const uint8_t *txt(int id, int *n)
{
    int i;
    for (i = 0; mnu2 && i < n_txt; i++) {
        const uint16_t *e = (const uint16_t *)(mnu2 + 32 + 8 * i);
        if (e[0] == id) { if (n) *n = e[1]; return mnu2 + *(const uint32_t *)(e + 2); }
    }
    if (n) *n = 0;
    return (const uint8_t *)"";
}

/* fuente 14 (42C480 / 42C610 del PC): avance = ancho - interletraje(c, siguiente) + espaciado; espacio 0,3 celdas */
static int f_par(int c, int sig)
{
    int i = c - f_pri;
    if (f_ker && sig && sig - f_pri >= 0 && sig - f_pri < f_n) return f_met[2 * i + 1] - f_inter[i * f_n + sig - f_pri];
    return f_met[2 * i + 1] + f_esp;
}
static int f_ancho(const uint8_t *t, int n)
{
    int a = 0, i;
    for (i = 0; i < n; i++) {
        int c = t[i], sig = i + 1 < n ? t[i + 1] : 0;
        if (c == 32) { a += (int)(f_w * 0.3f); continue; }
        if (c - f_pri < 0 || c - f_pri >= f_n) continue;
        a = f_par(c, sig == 32 ? 0 : sig) + a + f_esp;
    }
    return a;
}
static int u_fuente = U_FUENTE;                             /* FUENTE (COMUN) o FUENTE_PAUSA (en carrera) */
/* texto en (x, y) con las banderas de 42CDD0 (1 sin alinear, 4 centrado, si no a la derecha; 8 centrado en vertical) */
static void texto(const uint8_t *t, int n, float x, float y, int flags, float alfa, float z, uint32_t col)
{
    int i;
    if (!f_met || us[u_fuente] < 0 || alfa <= 0.004f) return;
    if (!(flags & 1)) x -= (flags & 4) ? f_ancho(t, n) / 2 : f_ancho(t, n);
    if (flags & 8) y += (int)(f_h * -0.5f);
    hudd_color(col);
    for (i = 0; i < n; i++) {
        int c = t[i], sig = i + 1 < n ? t[i + 1] : 0;
        if (c == 32) { x += (int)(f_w * 0.3f); continue; }
        if (c - f_pri < 0 || c - f_pri >= f_n) continue;
        hudd_dibuja(md, us[u_fuente], c - f_pri, x, y, alfa, z, 0);
        x = f_par(c, sig == 32 ? 0 : sig) + x + f_esp;
    }
    hudd_color(0xFFFFFF);
}
static void texto_id(int id, float x, float y, int flags, float alfa, float z, uint32_t col)
{
    int n; const uint8_t *t = txt(id, &n); texto(t, n, x, y, flags, alfa, z, col);
}
static void texto_c(const char *s, float x, float y, int flags, float alfa, float z, uint32_t col)
{
    texto((const uint8_t *)s, strlen(s), x, y, flags, alfa, z, col);
}

/* ---------------------------------------------------------------- licencias (registro de 0x40C del original) */
#define LIC_BYTES 0x40C
#define N_LIC 4
static uint8_t lic[N_LIC][LIC_BYTES];
static int lic_sel = -1;                                    /* licencia activa (-1 ninguna) */
static int vmu_ok;
static int l32(const uint8_t *p, int o) { int v; memcpy(&v, p + o, 4); return v; }
static void p32(uint8_t *p, int o, int v) { memcpy(p + o, &v, 4); }
static int lic_usada(int i) { return lic[i][0x3f4] & 1; }
static uint8_t *activa(void) { return lic_sel >= 0 ? lic[lic_sel] : NULL; }

/* 4471A0: valores de una licencia nueva (el +0x110 es el rango de 4EF260, 0x44BA5D17 - 0x4223A2E9 en float) */
static void lic_reinicia(uint8_t *p)
{
    static const int cero[] = { 0x14, 0x1c, 0x20, 0x24, 0xc4, 0xc8, 0xdc, 0xe0, 0xf0, 0xf8, 0x100, 0x104, 0x108, 0x10c,
                                0x114, 0x11c };
    static const int diez[] = { 0x18, 0xcc, 0xd0, 0xd4 };
    unsigned i; float x, y; uint32_t a = 0x44ba5d17, b = 0x4223a2e9;
    for (i = 0; i < sizeof cero / sizeof *cero; i++) p32(p, cero[i], 0);
    for (i = 0; i < sizeof diez / sizeof *diez; i++) p32(p, diez[i], 10);
    p32(p, 0xc0, 0x3f000000); p32(p, 0xd8, 2); p32(p, 0xe4, 1); p32(p, 0xf4, 1);
    p[0xe8] = p[0xe9] = p[0xea] = p[0xed] = p[0xee] = 0; p[0xeb] = 4; p[0xec] = 1; p[0] = 0;
    memcpy(&x, &a, 4); memcpy(&y, &b, 4); x -= y; memcpy(&a, &x, 4); p32(p, 0x110, (int)a); p32(p, 0x118, 9);
    memset(p + 0x28, 0, 0x96);
    memset(p + 0x125, 7, 0x52); memset(p + 0x177, 7, 0x52);
    memset(p + 0x1dd, 7, 0xc8); memset(p + 0x2a5, 0, 0xc8); memset(p + 0x1c9, 0, 20); memset(p + 0x36e, 0x77, 0x84);
    p[0x3f4] = (p[0x3f4] & 0xfc) | 2;
    memcpy(p + 0x3fc, "051013153129", 12); p32(p, 0x408, rand());
}
/* 447360: todo desbloqueado (nombre ENTIRETY en el editor) */
static void lic_desbloquea(uint8_t *p)
{
    memset(p + 0x28, 0xff, 0x96); memset(p + 0x125, 6, 0x52); memset(p + 0x177, 6, 0x52);
    memset(p + 0x1dd, 6, 0x190); memset(p + 0x1c9, 0, 20); memset(p + 0x36e, 0x66, 0x84);
}

/* VMU: fichero OR2C2C en A1. Contenido: 'OR2L', u32 version, i32 licencia activa, 4 registros de 0x40C (version 2: y
   detras 'RKSP' y las tablas de los rankings de OUTRUN2SP, comun/rksp.c: los datos del sistema que la consola guarda con
   las licencias; los ficheros de la version 1 se siguen leyendo y los rankings quedan por defecto), detras de la
   cabecera VMS (vmu_pkg: descripciones, icono animado y eyecatch de VMU.DAT, comun/vmu.c). Se lee y se escribe en
   bruto (O_META): fs_vmu no toca la cabecera y vmu_pkg_parse la comprueba (CRC incluido). Los ficheros del formato
   anterior (icono liso, sin eyecatch; algunos con una cabecera que vmu_pkg_parse no acepta) se siguen leyendo: si la
   cabecera no vale, los datos se buscan por la marca 'OR2L' */
#define VMU_FICH "/vmu/a1/OR2C2C"
static void vmu_lee(void)
{
    file_t f; uint8_t *b; int sz;
    f = fs_open(VMU_FICH, O_RDONLY | O_META);
    if (f < 0) { dbglog(DBG_INFO, "menu: sin %s\n", VMU_FICH); return; }
    sz = fs_total(f); b = malloc(sz > 0 ? sz : 1);
    if (b && fs_read(f, b, sz) == sz) {
        vmu_pkg_t pk; int o = -1;
        if (!vmu_pkg_parse(b, sz, &pk) && pk.data_len >= 12 + (int)sizeof lic && !memcmp(pk.data, "OR2L", 4)) {
            o = pk.data - b;
            dbglog(DBG_INFO, "menu: cabecera VMS bien (%d iconos, eyecatch %d), datos en +%d\n", pk.icon_cnt, pk.eyecatch_type, o);
        } else {                                            /* formato anterior: busca la marca */
            for (o = 0; o + 12 + (int)sizeof lic <= sz && memcmp(b + o, "OR2L", 4); o += 4) ;
            if (o + 12 + (int)sizeof lic > sz) o = -1;
            dbglog(DBG_INFO, "menu: cabecera VMS no valida; datos por la marca en +%d\n", o);
        }
        if (o >= 0) {
            const uint8_t *d = b + o; int ver;
            memcpy(&ver, d + 4, 4);
            memcpy(&lic_sel, d + 8, 4);
            memcpy(lic, d + 12, sizeof lic);
            if (ver >= 2 && o + 12 + (int)sizeof lic + 4 + RKSP_TAM <= sz && !memcmp(d + 12 + sizeof lic, "RKSP", 4))
                rksp_pon(d + 12 + sizeof lic + 4);              /* version 2: los rankings de OUTRUN2SP */
            if (lic_sel < -1 || lic_sel >= N_LIC || (lic_sel >= 0 && !lic_usada(lic_sel))) lic_sel = -1;
            vmu_ok = 1;
        }
    }
    dbglog(DBG_INFO, "menu: %s %d bytes, licencia %d%s\n", VMU_FICH, sz, lic_sel, vmu_ok ? "" : " (no vale)");
    free(b); fs_close(f);
}
static void vmu_guarda(void)
{
    vmu_pkg_t pkg; uint8_t *d, *out = NULL; int n, v = rksp_hay() ? 2 : 1, i; file_t f;
    uint32_t tam = 12 + sizeof lic + (v == 2 ? 4 + RKSP_TAM : 0);
    d = malloc(tam); if (!d) return;
    memcpy(d, "OR2L", 4); memcpy(d + 4, &v, 4); memcpy(d + 8, &lic_sel, 4); memcpy(d + 12, lic, sizeof lic);
    if (v == 2) { memcpy(d + 12 + sizeof lic, "RKSP", 4); memcpy(d + 16 + sizeof lic, rksp_tablas(), RKSP_TAM); }
    for (i = 0; i < N_LIC; i++) lic[i][0x3f4] &= ~2;        /* +3F4 bit 1: cambios sin guardar */
    memset(&pkg, 0, sizeof pkg);
    vmu_cabecera(&pkg);                                     /* descripciones, icono y eyecatch (comun/vmu.c) */
    pkg.data_len = tam; pkg.data = d;
    if (!vmu_pkg_build(&pkg, &out, &n)) {
        musica_pausa_lectura(1);
        fs_unlink(VMU_FICH);
        if ((f = fs_open(VMU_FICH, O_WRONLY | O_TRUNC | O_META)) >= 0) { if (fs_write(f, out, n) == n) vmu_ok = 1; fs_close(f); }
        dbglog(DBG_INFO, "menu: guardado %s (%d bytes, %d bloques): %s\n", VMU_FICH, n, (n + 511) / 512,
               f >= 0 ? "bien" : "no se pudo abrir");
        musica_pausa_lectura(0);
    }
    free(out); free(d);
}

/* la entrada de nombre de OUTRUN2SP (demo/nombre.c): la licencia activa (el nombre +0, el coche +0x1C) y el guardado
   (estados 7..0xB: los rankings 0x106288 / 0x1063d8 y la licencia 0x1a67e0 / 0x1a6840, aqui un solo fichero) */
const uint8_t *menu_licencia(void) { return activa(); }
void menu_guarda_vmu(void) { vmu_guarda(); }

/* ---------------------------------------------------------------- tramos */
typedef struct { int u; float c; int ini, fin, modo; } anim_t;
enum { F_ENTRA, F_QUIETO, F_SALE };
static anim_t a_cab, a_sel, a_aux, a_radio, a_ayuda, a_tarj, a_fl_d, a_fl_i, a_ed, a_ed_t, a_barra;
static int pant = -1, fase, t, sig, inact, t_bloq, entra_atras, barra_ant = -1, cargando, t_carga;
static int modo_sel = 1;                                    /* opcion de un jugador elegida */
/* OUTRUN2SP del menu principal. En el original (SLES-53998 0x23aba0 = PC 4E2970, opcion 2) pone [0x59acac] = 0 (el
   juego de OUTRUN2SP en vez del de la consola), +21C = 7 y el modo 32 pasa al 7 -> 9 -> 10: el frontal de recreativa
   de OUTRUN2SP (PC 4BEE60..4C4880, banco SPRANI 0x2E = SELECTOR_CVT: tipo de juego y curso OutRun 2 / OutRun 2 SP,
   coche, cambio, musica...). Ese frontal esta SIN PORTAR: aqui se pasa por las pantallas de coche, cambio y musica
   del menu de la consola (PROVISIONAL, en su lugar) y se corre la eleccion OUTRUN con los cursos de OutRun 2 SP de la
   lista del frontal (4C3DC0: fila 0 -> variante 1, lado 1 -> preset 1). B en el coche vuelve al menu principal. */
static int sp_sel;
static int c_menu, c_modo, c_rank, c_multi, manual, n_ok, pista_sonando, sale_ok;
static int lista, curs[N_LISTAS], elegido;                  /* 84B0F4, 84B0EC y 830364 del PC */

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
static void an_tramo_o(anim_t *a, const char *n1, const char *n2)
{
    int i, f, m;
    if (tramo(a->u, n1, &i, &f, &m)) an_va(a, i, f, m); else an_tramo(a, n2);
}
/* MUEVEij hacia delante o hacia atras (de j a i) */
static void an_mueve(anim_t *a, int de, int a_)
{
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
/* la de la musica, con la lista que se ve */
static int mascara(int p) { return MASCARA[p] | (p == P_MUSICA ? VB(V_MUS0 + lista) : 0); }
static int sube_err;                                        /* lo ultimo de hudd_sube: -2 VRAM, -3 RAM, -4 GD */
static int sube_p(int p)
{
    const m_pant_t *q = pan + pidx[p];
    if (pidx[p] < 0) return -1;
    if (!cargada[p]) {
        if ((sube_err = hudd_sube(md, q->t0, q->nt))) return -1;
        cargada[p] = 1;
    }
    edad[p] = ++reloj;
    return 0;
}
static void baja_p(int p)
{
    if (cargada[p]) { hudd_baja(md, pan[pidx[p]].t0, pan[pidx[p]].nt); cargada[p] = 0; }
}
/* sube las pantallas de la mascara; si no caben, echa las cargadas hace mas tiempo que no hagan falta. 1 si estan
   todas (si no, la VRAM echada se libera unos cuadros despues: hay que volver a llamar) */
static int orden_v[NV];                                     /* pantallas de mas a menos VRAM: las grandes primero */
static int asegura(int mascara)
{
    int i, p, q, v, ok = 1;
    sube_err = 0;
    for (i = 0; i < NV; i++) {
        p = orden_v[i];
        if (!(mascara & (1 << p)) || pidx[p] < 0) continue;
        for (;;) {
            if (cargada[p]) { edad[p] = ++reloj; break; }
            for (q = 0, v = -1; q < NV; q++)
                if (cargada[q] && !(mascara & (1 << q)) && (v < 0 || edad[q] < edad[v])) v = q;
            /* con holgura (la VRAM libre puede estar partida: un intento fallido avisa "out of PVR memory") */
            if (pvr_mem_available() >= pan[pidx[p]].vram + (v < 0 ? 16384 : HOLGURA) && !sube_p(p)) break;
            if (v < 0) { if (!hudd_pendiente() && !sube_p(p)) break; ok = 0; if (!sube_err) sube_err = -2; break; }
            baja_p(v);
        }
    }
    return ok;
}
unsigned menu_vram(void)
{
    unsigned s = 0; int p;
    for (p = 0; p < NV; p++) if (cargada[p]) s += pan[pidx[p]].vram;
    return s;
}
void menu_sale(void) { int p; for (p = 0; p < NV; p++) baja_p(p); fsp_sale(); hudd_vacia(); }
/* antes del frontal de recreativa: la VRAM del menu fuera (sus tablas siguen en la RAM) */
static void menu_libera_todo(void) { int p; for (p = 0; p < NV; p++) baja_p(p); if (menu_suelta_coche) menu_suelta_coche(); hudd_vacia(); }
void menu_libera_vram(void)
{
    int p;
    for (p = 0; p < NV; p++) if (pant < 0 || !(mascara(pant) & (1 << p))) baja_p(p);
    hudd_vacia();
}

/* ---------------------------------------------------------------- carga */
/* las tablas de MENU.DAT (~1,3 MB seguidos al leerlas) lo primero al arrancar: se quedan en la RAM siempre y, leidas
   despues de los sonidos, el coche, el HUD y la VMU, los huecos del monton no dejaban el bloque seguido (en la consola
   real no salia el menu y START iba a la carrera) */
/* Tablas de MENU.DAT en la carrera: solo los grupos de la pantalla de pausa (lo unico del menu que se dibuja); las
   piezas de los demas (casi todas: ~700 KB) se sueltan al empezar la carrera (en = 0) y se vuelven a leer del GD al
   volver al menu (en = 1; sin leer nada si estan). Devuelve los bytes sueltos, o 0 / < 0 al volver */
int menu_tablas(int en)
{
    if (!md) return 0;
    if (en) return hudd_grupos_carga(md);
    return (int)(pidx[V_PAUSA] >= 0 ? hudd_grupos_suelta(md, pan[pidx[V_PAUSA]].t0, pan[pidx[V_PAUSA]].nt) : hudd_grupos_suelta(md, 0, 0));
}
int menu_precarga(void) { return md || (md = hudd_abre("MENU.DAT", 0, 1)) ? 0 : -1; }

int menu_inicia(void)
{
    const uint8_t *x; int i, p; uint32_t o2;
    vmu_lee();
#ifdef PRUEBA_VMU
    /* prueba: guarda al arrancar con la cabecera nueva y vuelve a leer (dbglog de los dos pasos) */
    vmu_guarda(); vmu_ok = 0; vmu_lee();
#endif
    if (!md && !(md = hudd_abre("MENU.DAT", 0, 1))) return -1;
    x = hudd_extra(md);
    if (memcmp(x, "MNU1", 4)) { md = NULL; return -2; }
    n_pan = ((const uint32_t *)x)[1]; n_tra = ((const uint32_t *)x)[2]; o2 = ((const uint32_t *)x)[3];
    pan = (const m_pant_t *)(x + 16); tra = (const m_tramo_t *)(x + 16 + 48 * n_pan);
    for (p = 0; p < NV; p++)
        for (pidx[p] = -1, i = 0; i < n_pan; i++) if (!strncmp(pan[i].nombre, VNOM[p], 16)) pidx[p] = i;
    for (i = 0; i < NU; i++) us[i] = hudd_uso(md, UNOM[i]);
    for (i = 0; i < NV; i++) orden_v[i] = i;
    for (i = 1; i < NV; i++)                                /* (pantallas que no estan: al final) */
        for (p = i; p > 0; p--) {
            int a = orden_v[p - 1], b = orden_v[p];
            uint32_t va = pidx[a] < 0 ? 0 : pan[pidx[a]].vram, vb = pidx[b] < 0 ? 0 : pan[pidx[b]].vram;
            if (va >= vb) break;
            orden_v[p - 1] = b; orden_v[p] = a;
        }
    if (o2 && !memcmp(x + o2, "MNU2", 4)) {
        const uint32_t *h = (const uint32_t *)(x + o2); const uint8_t *f;
        mnu2 = x + o2; n_txt = h[1]; f = mnu2 + h[3];
        f_w = ((const uint16_t *)f)[0]; f_h = ((const uint16_t *)f)[1]; f_pri = ((const uint16_t *)f)[2];
        f_n = ((const uint16_t *)f)[3]; f_esp = ((const int16_t *)f)[4]; f_ker = ((const uint16_t *)f)[5];
        f_met = (const int8_t *)(f + 12); f_inter = f_met + 2 * f_n;
        t_modelo = mnu2 + h[4]; t_desbloq = (const int16_t *)(t_modelo + 32);
        t_colbit = t_desbloq + 30; t_colini = (const int8_t *)(t_colbit + 240);
        if (h[5]) col_tarjeta = h[5];
        if (h[6] && !memcmp(mnu2 + h[6], "PRG1", 4)) prg = mnu2 + h[6] + 4;
    }
    return 0;
}

/* ---------------------------------------------------------------- coches */
static int c_cursor, c_var;                                 /* 84B0E8 / 84B0E9 */
static int8_t colores[30];                                  /* 68ECA8: color elegido de cada ficha (0..7) */
static int colores_ok;
static uint8_t ficha_de_modelo[32];                         /* COCHEnn.DAT de cada modelo (0xFF si no hay) */
static float giro;
static int c_indice(void) { return c_cursor + 15 * c_var; }
static int c_modelo(int i) { return t_modelo ? t_modelo[i] : i; }
static int lic_bit(int b) { const uint8_t *p = activa(); return p && ((p[0x28 + b / 8] >> (b % 8)) & 1); }
/* 4C8F90: las dos primeras fichas siempre; las demas, su bit de la licencia */
static int c_desbloqueado(int i) { return i < 2 || (t_desbloq && lic_bit(t_desbloq[i])); }
/* 4C8FC0: color c de la ficha i */
static int c_color_ok(int i, int c)
{
    int b; if (!t_colbit) return 1;
    b = t_colbit[8 * i + c];
    return b < 0 ? b == -1 : lic_bit(b);
}
/* COCHEnn.DAT del modelo (los "_t", 15-29, no estan: el normal, modelo - 15) */
static int ficha_de(int m)
{
    if (m >= 0 && m < 32 && ficha_de_modelo[m] != 0xFF) return ficha_de_modelo[m];
    if (m >= 15 && m < 30 && ficha_de_modelo[m - 15] != 0xFF) return ficha_de_modelo[m - 15];
    return -1;
}
void menu_fichas(const uint8_t *fm, int n)
{
    int i; memset(ficha_de_modelo, 0xFF, sizeof ficha_de_modelo);
    for (i = 0; i < n; i++) if (fm[i] < 30) ficha_de_modelo[fm[i]] = i;
}
void menu_coches(unsigned mascara) { (void)mascara; }
/* 4C9010: si la ficha esta desbloqueada y su color no, el primero desbloqueado (o el 0) */
static void c_arregla_color(void)
{
    int i = c_indice(), c;
    if (c_desbloqueado(i) && !c_color_ok(i, colores[i])) {
        for (c = 0; c < 8 && !c_color_ok(i, c); c++) ;
        colores[i] = c == 8 ? 0 : c;
    }
}

/* ---------------------------------------------------------------- pantallas */
static int cabecera(int p)
{
    switch (p) {
    case P_MENU: return U_CABMENU;
    case P_MODO: return U_CABMODO;
    case P_COCHE: case P_CAMBIO: return U_CABCOCHE;
    case P_MUSICA: return U_CABMUSICA;
    case P_LICENCIA: case P_EDITOR: return U_CABLIC;
    case P_OPCIONES: return U_CABOPC;
    case P_RANKING: return U_CABRANK;
    case P_MULTI: return U_CABMULTI;
    }
    return -1;
}
/* barra de ayuda (iconos de los botones y sus textos, conv_menu.AYUDA) de cada pantalla */
static int barra(int p)
{
    switch (p) {
    case P_MENU: return U_BARRA_MENU;
    case P_MODO: return U_BARRA_MODO;
    case P_COCHE: return U_BARRA_COCHE;
    case P_CAMBIO: return U_BARRA_CAMBIO;
    case P_MUSICA: return U_BARRA_MUSICA;
    case P_LICENCIA: case P_EDITOR: case P_RANKING: case P_MULTI: return U_BARRA_LIC;
    case P_OPCIONES: return U_BARRA_OPC;
    }
    return -1;
}

static int pon_pista(int n)
{
    if (n == pista_sonando) return 0;
    if (n < 0) { musica_para(); pista_sonando = -1; return 0; }
    if (musica_pon(n)) { pista_sonando = -1; return -1; }
    pista_sonando = n;
    return 0;
}
static void pon_musica(int k)
{
    if (pista_sonando != PISTA_AVANCE + k && pista_sonando != k && pon_pista(PISTA_AVANCE + k) && k < n_ok) pon_pista(k);
}
static void musica_menu(void)
{
    if (pon_pista(PISTA_MENU) && pista_sonando < 0) pon_pista(menu_pista());
}
/* 4C9750: tema de la lista y el cursor actuales */
static int tema_actual(void) { int k = curs[lista], l; for (l = 0; l < lista; l++) k += TEMAS_LISTA[l]; return k; }
static int lic_bit(int b);
static int tema_libre(int k) { return k <= 6 || lic_bit(DESBLOQ_TEMA[k]); }   /* 4474E0 solo por encima del 6 */
static void musica_lista(void)                              /* carrusel y titulo de la lista actual en su tema */
{
    static const int UL[N_LISTAS] = { U_L0, U_L1, U_L2 }, UT[N_LISTAS] = { U_T0, U_T1, U_T2 };
    a_sel.u = us[UL[lista]]; an_tramo_n(&a_sel, "TEMA%d", curs[lista]);
    a_aux.u = us[UT[lista]]; an_tramo_n(&a_aux, "TEMA%d", curs[lista]);
}

/* ---- licencia: eleccion (4E2970) y editor (4DE2B0) */
static int l_lista[N_LIC + 1], l_n, l_nueva, l_idx, l_vuelta, l_ctx, l_ctx_sel, l_borra, l_desliza, l_dir;
enum { E_MENU, E_NOMBRE, E_REJILLA };
static int e_estado, e_item, e_rej, e_val, e_existente, e_tecla, e_slot;
static uint8_t e_copia[LIC_BYTES];                          /* para deshacer (nombre, bandera, signo) */
static const char TECLAS[] = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ ";   /* 468D40 (mayusculas) */
#define N_TECLAS 38                                         /* 37 caracteres y borrar */
static const int REJ_COL[3] = { 2, 4, 3 }, REJ_N[3] = { 6, 26, 12 }, REJ_OFF[3] = { 0x20, 0x1c, 0x18 };

static void l_lista_hace(void)
{
    int i; l_n = 0; l_nueva = -1;
    for (i = 0; i < N_LIC; i++) if (lic_usada(i)) l_lista[l_n++] = i;
    for (i = 0; i < N_LIC && lic_usada(i); i++) ;
    if (i < N_LIC) { l_nueva = l_n; l_lista[l_n++] = i; }
    for (l_idx = 0; l_idx < l_n && l_lista[l_idx] != lic_sel; l_idx++) ;
    if (l_idx >= l_n) l_idx = 0;
}

static void entra(int p, int atras)
{
    int cab = cabecera(p);
    cargando = !asegura(mascara(p)); t_carga = 0;
    barra_ant = pant >= 0 ? barra(pant) : -1;
    pant = p; fase = F_ENTRA; t = 0; entra_atras = atras; t_bloq = 0;
    if (cab < 0) a_cab.u = -1;
    else if (us[cab] != a_cab.u) { a_cab.u = us[cab]; an_tramo(&a_cab, "ENTRA"); }   /* cabecera nueva */
    GANCHO(p, 0, 0);
    switch (p) {
    case P_TITULO: fase = F_QUIETO; pon_pista(-1); break;
    case P_MENU: case P_MODO: case P_RANKING: case P_MULTI: {
        static const int U_[] = { U_PRINC, U_UNJUG, U_RANK, U_MULTI };
        int k = p == P_MENU ? 0 : p == P_MODO ? 1 : p == P_RANKING ? 2 : 3;
        int c = k == 0 ? c_menu : k == 1 ? c_modo : k == 2 ? c_rank : c_multi;
        a_sel.u = us[U_[k]];
        if (atras || c) an_tramo_n(&a_sel, "SEL%d", c); else an_tramo(&a_sel, "ENTRA");
        break; }
    case P_COCHE:
        if (!colores_ok && t_colini) { memcpy(colores, t_colini, 30); colores_ok = 1; }
        c_arregla_color();
        a_sel.u = us[c_var ? U_INFO_T : U_INFO]; an_tramo_n(&a_sel, "COCHE%d", c_cursor);
        break;
    case P_CAMBIO: a_sel.u = us[U_CAMBIO]; an_tramo(&a_sel, "ENTRA"); break;
    case P_MUSICA:
        a_radio.u = us[U_RADIO]; an_tramo(&a_radio, "ENTRA");
        a_ayuda.u = us[U_AYUDA]; an_tramo(&a_ayuda, "ENTRA");
        musica_lista(); if (!curs[lista]) an_tramo(&a_sel, "ENTRA");
        pon_musica(tema_actual());
        break;
    case P_LICENCIA:
        l_lista_hace(); l_ctx = 0; l_borra = 0; l_desliza = 0;
        a_tarj.u = us[U_TARJ]; an_tramo(&a_tarj, "ENTRA"); a_sel = a_tarj;
        a_fl_d.u = us[U_FL_D]; an_tramo(&a_fl_d, "QUIETA"); a_fl_i.u = us[U_FL_I]; an_tramo(&a_fl_i, "QUIETA");
        a_barra.u = us[U_LIC_BARRA]; an_tramo(&a_barra, "ENTRA");
        break;
    case P_EDITOR:
        e_estado = E_MENU; e_item = 0;
        a_ed.u = us[U_EDITOR]; an_tramo(&a_ed, "ENTRA"); a_ed_t.u = us[U_TARJ_E]; an_tramo(&a_ed_t, "ENTRA");
        a_sel = a_ed;
        break;
    case P_OPCIONES:
        e_item = 0; a_barra.u = us[U_OPC_BARRA]; an_tramo(&a_barra, "ENTRA"); a_sel = a_barra;
        break;
    }
}

void menu_entra(int man, int pista, int npistas)
{
    manual = man; pista_sonando = -2; n_ok = npistas < N_TEMAS ? npistas : N_TEMAS;
    elegido = pista >= 0 && pista < N_TEMAS ? pista : 0;   /* las listas y sus cursores siguen como estaban */
    inact = 0; a_cab.u = -1; pant = -1;
    /* vuelta a los menus: 4457B7 del PC guarda (416420) si la licencia activa tiene cambios (+3F4 bit 1), p. ej. el
       tiempo de juego que suma la carrera (menu_suma_tiempo) */
    if (activa() && (activa()[0x3f4] & 2)) vmu_guarda();
    if (md) entra(P_TITULO, 0);
}

/* tiempo de juego de la licencia activa (+0x10C): al salir de la meta (modo 19) o del fin del tiempo (modo 20) y al
   confirmar REINTENTAR / ABANDONAR en la pausa, el original suma el crono total de la carrera (4505D0 = [7D39DC];
   SLES-53998 0x1cfdc0 = [0x5c5f48], sumado en 0x1b96bc / 0x1b9b58 / 0x249c5c / 0x24a110 a la licencia 0x588668) y
   marca +3F4 |= 2 (cambios sin guardar) */
void menu_suma_tiempo(int cuadros)
{
    uint8_t *p = activa();
    if (!p || cuadros <= 0) return;
    p32(p, 0x10c, l32(p, 0x10c) + cuadros); p[0x3f4] |= 2;
    dbglog(DBG_INFO, "LICENCIA: tiempo de juego +%d cuadros = %d\n", cuadros, l32(p, 0x10c));
}

/* millas OutRun de la licencia activa (+0x24, float): 4EF510 (PS2 0x1f41d8) al salir del modo 34 suma las de la carrera
   (entero sin signo pasado a float, suma en float) y marca +3F4 |= 2 */
void menu_suma_millas(uint32_t millas)
{
    uint8_t *p = activa(); float m, s;
    if (!p) return;
    s = millas >> 31 ? (float)((millas >> 1) | (millas & 1)) * 2.0f : (float)(int32_t)millas;
    memcpy(&m, p + 0x24, 4); m += s; memcpy(p + 0x24, &m, 4); p[0x3f4] |= 2;
    dbglog(DBG_INFO, "LICENCIA: millas +%u = %.0f\n", (unsigned)millas, (double)m);
}

/* variante de [780258] (PS2 0x443508) del modo elegido, como 4EEAD0 del PC (runtime_route_mode: 1 OutRun, 2 Heart
   Attack, 7 Time Attack); COAST 2 COAST (las misiones de Races.bin, variante 5) no esta: corre como OutRun */
int menu_modo(void) { return sp_sel ? fsp_variante() : modo_sel == 2 ? 2 : modo_sel == 3 ? 7 : 1; }
int menu_sp(void) { return sp_sel; }
void menu_entra_sp(void)
{
    sp_sel = 1; menu_libera_todo();
    if (fsp_entra()) { sp_sel = 0; dbglog(DBG_ERROR, "menu: sin el frontal de recreativa, al titulo\n"); }
}
int menu_preset(void) { return sp_sel ? fsp_preset() : 0; }
int menu_manual(void) { return sp_sel ? fsp_manual() : manual; }
int menu_pista(void) { return sp_sel ? fsp_musica() : elegido < n_ok ? elegido : 0; }
int menu_coche3d(void)
{
    if (sp_sel) return fsp_coche3d() && ficha_de(fsp_modelo()) >= 0;
    return md && pant == P_COCHE && ficha_de(c_modelo(c_indice())) >= 0 && fase != F_SALE && !cargando && an_hecho(&a_sel);
}
int menu_modelo(void) { return sp_sel ? fsp_modelo() : c_modelo(c_indice()); }
int menu_coche(void) { return ficha_de(menu_modelo()); }
int menu_color(void) { return sp_sel ? fsp_color() : colores[c_indice()] + 1; }
float menu_giro(void) { return giro; }
int menu_vista(void) { const uint8_t *p = activa(); return p ? l32(p, 0xd8) : 2; }
int menu_mph(void) { const uint8_t *p = activa(); return p ? l32(p, 0xdc) : 0; }
int menu_dificultad(void) { const uint8_t *p = activa(); return p ? l32(p, 0xe0) : 0; }
int menu_foto(void) { const uint8_t *p = activa(); return p ? l32(p, 0x20) : 0; }

static void sale(int destino, int ok) { fase = F_SALE; t = 0; sig = destino; sale_ok = ok; sfx_menu(ok ? SFX_ACEPTA : SFX_CANCELA); }
static void bloqueado(void) { t_bloq = 14; sfx_menu(SFX_BLOQUEADO); }
static void a_licencia(int vuelta) { l_vuelta = vuelta; sale(P_LICENCIA, 1); }

/* carrusel de opciones (menu principal, un jugador, rankings, multijugador) */
static void carrusel(unsigned n, int *c, int nopc, int izq, int der)
{
    (void)n;
    if (izq && *c > 0) { an_mueve(&a_sel, *c, *c - 1); (*c)--; sfx_menu(SFX_CURSOR); }
    else if (der && *c < nopc - 1) { an_mueve(&a_sel, *c, *c + 1); (*c)++; sfx_menu(SFX_CURSOR); }
}

/* editor: 4DE2B0 (menu de 5 opciones), rejillas (stage 3-5) y nombre */
static void ed_resalta(int item) { e_item = item; an_tramo_n(&a_ed, "OPC%d", item); an_tramo_n(&a_ed_t, "OPC%d", item); sfx_menu(SFX_CURSOR); }
static void ed_control(unsigned n)
{
    uint8_t *p = lic[e_slot];
    int ok = n & CONT_A, atras = n & CONT_B, ar = n & CONT_DPAD_UP, ab = n & CONT_DPAD_DOWN,
        iz = n & CONT_DPAD_LEFT, de = n & CONT_DPAD_RIGHT;
    if (e_estado == E_NOMBRE) {                             /* teclado: A escribe, START acepta, B deshace */
        int l = strnlen((char *)p, 16), k = e_tecla;
        if (ar && k >= 6) k -= 6; else if (ab && k + 6 < N_TECLAS) k += 6;
        else if (iz && k % 6) k--; else if (de && k % 6 < 5 && k + 1 < N_TECLAS) k++;
        if (k != e_tecla) { e_tecla = k; sfx_menu(SFX_CURSOR); }
        if (ok) {
            if (e_tecla == N_TECLAS - 1) { if (l) p[l - 1] = 0; }
            else if (l < 15) { p[l] = TECLAS[e_tecla]; p[l + 1] = 0; }   /* 468780(1, 16): 15 como mucho */
            sfx_menu(SFX_CURSOR);
        }
        if (n & CONT_START) { if (p[0]) { p[0x3f4] |= 2; e_estado = E_MENU; sfx_menu(SFX_ACEPTA); } else bloqueado(); }
        else if (atras) { memcpy(p, e_copia, 16); e_estado = E_MENU; sfx_menu(SFX_CANCELA); }
        return;
    }
    if (e_estado == E_REJILLA) {                            /* 4DE160: A guarda, B deja el de antes */
        int col = REJ_COL[e_rej], cnt = REJ_N[e_rej], s = e_val;
        if (ok) { p32(p, REJ_OFF[e_rej], e_val); p[0x3f4] |= 2; e_estado = E_MENU; sfx_menu(SFX_ACEPTA); return; }
        if (atras) { e_estado = E_MENU; sfx_menu(SFX_CANCELA); return; }
        if (ar && s >= col) s -= col; else if (ab && s < cnt - col) s += col;
        else if (iz && s % col) s--; else if (de && s % col != col - 1 && s < cnt - 1) s++;
        if (s != e_val) { e_val = s; sfx_menu(SFX_CURSOR); }
        return;
    }
    if (atras) {                                            /* sale sin guardar: nombre, bandera y signo de antes */
        if (!memcmp(p, "MILESANDMILES", 14)) { float m; memcpy(&m, p + 0x24, 4); m += 1000000.0f; memcpy(p + 0x24, &m, 4); }
        else if (!memcmp(p, "ENTIRETY", 9)) lic_desbloquea(p);
        memcpy(p + 0x1c, e_copia + 0x1c, 4); memcpy(p + 0x18, e_copia + 0x18, 4); memcpy(p, e_copia, 16);
        if (!e_existente) { p[0x3f4] &= ~1; if (lic_sel == e_slot) lic_sel = -1; }
        else vmu_guarda();
        an_tramo(&a_ed, "SALE"); an_tramo(&a_ed_t, "SALE"); a_sel = a_ed;
        sale(P_LICENCIA, 0); return;
    }
    switch (e_item) {                                       /* 4DD9C0: el nombre baja a la 2 (salta la 1) */
    case 0: if (ok) { e_estado = E_NOMBRE; e_tecla = 0; memcpy(e_copia, p, 16); sfx_menu(SFX_ACEPTA); } else if (ab) ed_resalta(2); break;
    case 1: case 2: case 3:
        if (ok) { e_estado = E_REJILLA; e_rej = e_item - 1; e_val = l32(p, REJ_OFF[e_rej]);
                  if (e_val < 0 || e_val >= REJ_N[e_rej]) e_val = 0;
                  sfx_menu(SFX_ACEPTA); }
        else if (e_item == 1) { if (ar) ed_resalta(0); else if (ab) ed_resalta(2); }
        else if (e_item == 2) { if (ar) ed_resalta(0); else if (ab || de) ed_resalta(3); }
        else { if (ar || iz) ed_resalta(2); else if (ab) ed_resalta(4); }
        break;
    case 4:
        if (ar) ed_resalta(3);
        else if (ok) {                                      /* DONE: guarda y sigue */
            lic_sel = e_slot; vmu_guarda();
            an_tramo(&a_ed, "SALE"); an_tramo(&a_ed_t, "SALE"); a_sel = a_ed;
            sale(e_existente ? P_LICENCIA : l_vuelta, 1);
        }
        break;
    }
}

static void lic_control(unsigned n)
{
    int ok = n & CONT_A, atras = n & CONT_B;
    if (l_desliza) return;
    if (l_ctx) {                                            /* menu de una licencia: Select / Edit / Reset (4E1CF0) */
        int s = l_lista[l_idx];
        if (l_borra) {                                      /* Reset: si / no */
            if (n & (CONT_DPAD_LEFT | CONT_DPAD_RIGHT)) { l_borra = 3 - l_borra; sfx_menu(SFX_CURSOR); }
            if (ok && l_borra == 1) {                       /* 416500: la licencia vuelve a la de fabrica, libre */
                lic_reinicia(lic[s]); lic[s][0x3f4] &= ~1; if (lic_sel == s) lic_sel = -1;
                vmu_guarda(); l_lista_hace(); l_ctx = 0; l_borra = 0; sfx_menu(SFX_ACEPTA);
            } else if (atras || ok) { l_borra = 0; sfx_menu(SFX_CANCELA); }
            return;
        }
        if (n & CONT_DPAD_UP && l_ctx_sel > 0) { l_ctx_sel--; sfx_menu(SFX_CURSOR); }
        else if (n & CONT_DPAD_DOWN && l_ctx_sel < 2) { l_ctx_sel++; sfx_menu(SFX_CURSOR); }
        else if (atras) { l_ctx = 0; sfx_menu(SFX_CANCELA); }
        else if (ok) {
            if (l_ctx_sel == 0) { lic_sel = s; vmu_guarda(); l_ctx = 0; sale(l_vuelta, 1); }
            else if (l_ctx_sel == 1) { e_slot = s; e_existente = 1; memcpy(e_copia, lic[s], LIC_BYTES); l_ctx = 0; sale(P_EDITOR, 1); }
            else { l_borra = 2; sfx_menu(SFX_ACEPTA); }
        }
        return;
    }
    if ((n & CONT_DPAD_LEFT) && l_idx > 0) {
        an_tramo(&a_fl_i, "PULSA"); l_desliza = 1; l_dir = -1; sfx_menu(SFX_CURSOR);
    } else if ((n & CONT_DPAD_RIGHT) && l_idx < l_n - 1) {
        an_tramo(&a_fl_d, "PULSA"); l_desliza = 1; l_dir = 1; sfx_menu(SFX_CURSOR);
    } else if (ok) {
        if (l_idx == l_nueva) {                             /* CREATE NEW LICENSE: nueva con el nombre 0x219 */
            int k, s = l_lista[l_idx]; const uint8_t *nom = txt(0x219, &k);
            lic_reinicia(lic[s]); memset(lic[s], 0, 16); memcpy(lic[s], nom, k < 15 ? k : 15);
            lic[s][0x3f4] |= 1; e_slot = s; e_existente = 0; memcpy(e_copia, lic[s], LIC_BYTES);
            sale(P_EDITOR, 1);
        } else { l_ctx = 1; l_ctx_sel = 0; sfx_menu(SFX_ACEPTA); }
    } else if (atras) sale(lic_sel >= 0 && l_vuelta != P_TITULO ? l_vuelta : P_TITULO, 0);   /* sin licencia: titulo */
}

static void opc_control(unsigned n)
{
    static const int OFF[3] = { 0xd8, 0xdc, 0xe0 }, MAXV[3] = { 3, 2, 5 };
    uint8_t *p = activa(); int v;
    if (n & CONT_DPAD_UP && e_item > 0) { e_item--; an_tramo(&a_barra, "ENTRA"); sfx_menu(SFX_CURSOR); }
    else if (n & CONT_DPAD_DOWN && e_item < 2) { e_item++; an_tramo(&a_barra, "ENTRA"); sfx_menu(SFX_CURSOR); }
    else if (n & (CONT_DPAD_LEFT | CONT_DPAD_RIGHT)) {     /* 446010 / 446050: sin dar la vuelta */
        if (!p) { bloqueado(); return; }
        v = l32(p, OFF[e_item]) + (n & CONT_DPAD_RIGHT ? 1 : -1);
        if (v >= 0 && v < MAXV[e_item]) { p32(p, OFF[e_item], v); p[0x3f4] |= 2; sfx_menu(SFX_CURSOR); }
    } else if (n & CONT_B) {                                /* 4D76F0: se aplican y se guardan al salir */
        if (p && (p[0x3f4] & 2)) vmu_guarda();
        sale(P_MENU, 0);
    }
}

/* Bit 0x2000 del mando del frontal (PS2 0x101e90: R1 0x800 y R2 0x200 del libpad dan 0x2000; L1 / L2, 0x1000): lo
   miran la seleccion de musica (lista siguiente, 0x24e748 = 4C9C20 del PC) y la de coche (variante, 0x244c44 =
   4C9290), sobre lo pulsado en este cuadro (+8 del mando, 0x101bd4). En la DC: el gatillo R, que es analogico (0..255);
   cuenta como pulsado desde la mitad del recorrido (128) y vale el cuadro en que pasa a pulsado. */
static int r1_nuevo, r1_ant;
static void lee_r1(void)
{
    maple_device_t *d = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = d ? (cont_state_t *)maple_dev_status(d) : NULL;
    int on = st && st->rtrig >= 128;
    r1_nuevo = on && !r1_ant; r1_ant = on;
}

/* entrada en las pantallas en reposo */
static void control(unsigned n)
{
    int ok = n & (CONT_A | CONT_START), atras = n & CONT_B, izq = n & CONT_DPAD_LEFT, der = n & CONT_DPAD_RIGHT;
    int r1 = r1_nuevo;
    switch (pant) {
    case P_TITULO:
        if (ok) { if (lic_sel < 0) a_licencia(P_MENU); else sale(P_MENU, 1); musica_menu(); }
        break;
    case P_MENU: case P_MODO:
        if (!an_hecho(&a_sel)) break;
        carrusel(n, pant == P_MENU ? &c_menu : &c_modo, N_OPC, izq, der);
        if (izq || der) break;
        if (ok) {
            int c = pant == P_MENU ? c_menu : c_modo;
            GANCHO(pant, 1, c);
            if (pant == P_MENU) {                           /* SINGLE PLAYER, MULTIPLAYER, OUTRUN2SP, RANKINGS, OPTIONS */
                if (c == 0) { sp_sel = 0; sale(P_MODO, 1); } else if (c == 1) sale(P_MULTI, 1); else if (c == 3) sale(P_RANKING, 1);
                else if (c == 4) sale(P_OPCIONES, 1);
                else {                                      /* OUTRUN2SP (0x23aba0): el frontal de recreativa (fsp.c) */
                    sp_sel = 1; sfx_menu(SFX_ACEPTA); menu_libera_todo();
                    if (fsp_entra()) { sp_sel = 0; entra(P_MENU, 1); }
                }
            } else if (c <= 3) { modo_sel = c; sale(P_COCHE, 1); } else bloqueado();   /* C2C, OUTRUN, HEART ATTACK, TIME ATTACK */
        } else if (n & CONT_X) a_licencia(pant);            /* LICENSE */
        else if (n & CONT_Y) bloqueado();                   /* FRIENDS: en linea */
        else if (atras) sale(pant == P_MENU ? P_TITULO : P_MENU, 0);
        break;
    case P_RANKING: case P_MULTI:
        if (!an_hecho(&a_sel)) break;
        carrusel(n, pant == P_RANKING ? &c_rank : &c_multi, pant == P_RANKING ? 3 : 2, izq, der);
        if (izq || der) break;
        if (ok) bloqueado();                                /* sin datos de ranking ni red */
        else if (atras) sale(P_MENU, 0);
        break;
    case P_COCHE: {
        int i = c_indice();
        if (!an_hecho(&a_sel)) break;
        if (izq && c_cursor > 0) { an_mueve(&a_sel, c_cursor, c_cursor - 1); c_cursor--; c_arregla_color(); sfx_menu(SFX_CURSOR); }
        else if (der && c_cursor < N_COCHES - 1) { an_mueve(&a_sel, c_cursor, c_cursor + 1); c_cursor++; c_arregla_color(); sfx_menu(SFX_CURSOR); }
        else if (ok) { if (c_desbloqueado(i)) { GANCHO(P_COCHE, 1, c_modelo(i)); sale(P_CAMBIO, 1); } else sfx_menu(SFX_BLOQUEADO); }
        else if (r1) {                                      /* variante normal / "_t" (bit 0x2000: R1 / R2 del PS2) */
            c_var ^= 1; a_sel.u = us[c_var ? U_INFO_T : U_INFO]; an_tramo_n(&a_sel, "COCHE%d", c_cursor);
            c_arregla_color(); sfx_menu(SFX_CURSOR);
        } else if (n & CONT_Y) {                            /* COLOR: el siguiente desbloqueado de los 8 */
            if (c_desbloqueado(i)) { int o = colores[i], c = o; do c = (c + 1) & 7; while (c != o && !c_color_ok(i, c)); colores[i] = c; }
        } else if (atras) sale(sp_sel ? P_MENU : P_MODO, 0);
        break; }
    case P_CAMBIO:
        if (izq && manual) { manual = 0; an_tramo(&a_sel, "AT"); sfx_menu(SFX_CURSOR); }
        else if (der && !manual) { manual = 1; an_tramo(&a_sel, "MT"); sfx_menu(SFX_CURSOR); }
        else if (ok) { an_tramo(&a_sel, manual ? "MT_OK" : "AT_OK"); sale(P_MUSICA, 1); }
        else if (atras) { an_tramo(&a_sel, manual ? "MT_ATRAS" : "AT_ATRAS"); sale(P_COCHE, 0); }
        break;
    case P_MUSICA:
        if (!an_hecho(&a_sel)) break;
        if (n & CONT_Y) {                                   /* RANDOM (0x10 del PS2): un tema desbloqueado y a correr */
            int k = 0, i;                                   /* (rand & 0x1F) * 27 >> 5: 0..26, 100 intentos o el 0 */
            for (i = 1; i < 100; i++) { k = ((rand() & 0x1f) * 0x1b) >> 5; if (tema_libre(k)) break; }
            if (i == 100) k = 0;
            elegido = k; an_va(&a_radio, 25, 0, 0);
            fase = F_SALE; t = 0; sig = -1; sale_ok = 1;     /* sin el sonido de aceptar (como el original) */
        } else if (r1) {                                    /* lista siguiente (bit 0x2000: R1 / R2 del PS2; gatillo
                                                               R de la DC) con su cursor, titulo y avance */
            lista = (lista + 1) % N_LISTAS;
            cargando = !asegura(mascara(P_MUSICA)); t_carga = 0;
            musica_lista(); pon_musica(tema_actual());
        } else if ((izq && curs[lista] > 0) || (der && curs[lista] < TEMAS_LISTA[lista] - 1)) {
            int k = curs[lista] + (der ? 1 : -1);
            an_mueve(&a_sel, curs[lista], k); curs[lista] = k; an_tramo_n(&a_aux, "TEMA%d", k); sfx_menu(SFX_CURSOR);
        } else if (ok) {                                    /* bloqueado: sonido 3 */
            if (tema_libre(tema_actual())) { elegido = tema_actual(); an_va(&a_radio, 25, 0, 0); sale(-1, 1); }
            else bloqueado();
        }
        else if (atras) { an_va(&a_radio, 25, 0, 0); sale(P_CAMBIO, 0); musica_menu(); }
        break;
    case P_LICENCIA: lic_control(n); break;
    case P_EDITOR: if (an_hecho(&a_ed)) ed_control(n); break;
    case P_OPCIONES: opc_control(n); break;
    }
}

static void aplica_audio(void);
int menu_paso(unsigned n)
{
    if (fsp_activo()) {                                     /* frontal de recreativa de OUTRUN2SP */
        int r;
        giro += 100.0f * 2.0f * (float)M_PI / 65536.0f;     /* 475670: +0x64 por paso */
        if (giro > 2.0f * (float)M_PI) giro -= 2.0f * (float)M_PI;
        r = fsp_paso(n);
        if (r == FSP_CARRERA) {                         /* el tema elegido (48B1D0: [830364], el mismo que la eleccion de
                                                           la consola) suena al empezar la carrera, como al salir del menu */
            pista_sonando = -2; pon_pista(menu_pista()); return MENU_CARRERA;
        }
        if (r == FSP_MENU) { fsp_sale(); sp_sel = 0; entra(P_MENU, 1); musica_menu(); }
        return MENU_SIGUE;
    }
    lee_r1();
    if (n || r1_nuevo) inact = 0; else inact++;
    if (!md) return (n & CONT_START) ? MENU_CARRERA : inact > T_DEMO ? MENU_DEMO : MENU_SIGUE;
    aplica_audio();                                         /* volumen de la licencia activa (cambia al elegirla) */
    giro += 100.0f * 2.0f * (float)M_PI / 65536.0f;         /* 4A5B20: +100 por paso */
    if (giro > 2.0f * (float)M_PI) giro -= 2.0f * (float)M_PI;
    if (cargando) {                                         /* la VRAM de la pantalla aun no esta: se espera */
        if (asegura(mascara(pant))) {
            cargando = 0;
            if (t_carga > 2) dbglog(DBG_INFO, "menu: pantalla %d lista tras %d cuadros\n", pant, t_carga);
        }
        else if (++t_carga == T_SUELTA_COCHE && menu_suelta_coche && sube_err == -2) menu_suelta_coche();   /* el coche 3D
                                                               parte la VRAM (sin RAM o con el GD fallando no sirve) */
        else if (t_carga == T_REORDENA) { int p; for (p = 0; p < NV; p++) baja_p(p); }   /* y si aun no: todo de nuevo */
        else if (t_carga > T_CARGA_MAX) {
            cargando = 0;
            dbglog(DBG_INFO, "menu: pantalla %d sin %s (VRAM libre %uK, menu %uK)\n", pant,
                   sube_err == -3 ? "RAM" : sube_err == -4 ? "lectura del GD" : "VRAM",
                   (unsigned)(pvr_mem_available() >> 10), menu_vram() >> 10);
        }
        else return MENU_SIGUE;
    }
    t++;
    if (t_bloq) t_bloq--;
    if (a_cab.u >= 0) { an_paso(&a_cab); if (an_hecho(&a_cab) && a_cab.modo == 0) an_tramo_o(&a_cab, "BRILLO", "QUIETA"); }
    {   int estaba = an_hecho(&a_sel);
        if (pant == P_LICENCIA) { an_paso(&a_tarj); an_paso(&a_fl_d); an_paso(&a_fl_i); a_sel = a_tarj; }
        else if (pant == P_OPCIONES) { an_paso(&a_barra); a_sel = a_barra; }
        else if (pant == P_EDITOR) { an_paso(&a_ed); an_paso(&a_ed_t); a_sel = a_ed; }
        else { an_paso(&a_sel); an_paso(&a_aux); an_paso(&a_radio); an_paso(&a_ayuda); }
        if (!estaba && an_hecho(&a_sel) && fase != F_SALE) {
            if (pant == P_MENU || pant == P_MODO) an_tramo_n(&a_sel, "SEL%d", pant == P_MENU ? c_menu : c_modo);
            else if (pant == P_RANKING || pant == P_MULTI) an_tramo_n(&a_sel, "SEL%d", pant == P_RANKING ? c_rank : c_multi);
            else if (pant == P_COCHE) an_tramo_n(&a_sel, "COCHE%d", c_cursor);
            else if (pant == P_MUSICA) { an_tramo_n(&a_sel, "TEMA%d", curs[lista]); pon_musica(tema_actual()); }
            else if (pant == P_CAMBIO) an_tramo(&a_sel, manual ? "MT" : "AT");
            else if (pant == P_EDITOR) { an_tramo_n(&a_ed, "OPC%d", e_item); an_tramo_n(&a_ed_t, "OPC%d", e_item); a_sel = a_ed; }
        }
    }
    if (pant == P_LICENCIA && l_desliza) {                  /* 4E2970 estados 6-9: sale 48 px por paso y entra */
        if (++l_desliza == 11) l_idx += l_dir;
        if (l_desliza >= 21) { l_desliza = 0; an_tramo(&a_fl_d, "QUIETA"); an_tramo(&a_fl_i, "QUIETA"); }
    }
    if (fase == F_ENTRA && t >= T_FUNDIDO && (an_hecho(&a_sel) || a_sel.modo == 1)) fase = F_QUIETO;
    if (pant == P_TITULO && fase == F_QUIETO && inact > T_DEMO) { inact = 0; pon_pista(menu_pista()); return MENU_DEMO; }
    /* precarga de la pantalla siguiente cuando sobra VRAM */
    if (fase == F_QUIETO && t == 40 && pant >= 0 && pant <= P_CAMBIO) {
        int p, sigp = pant == P_TITULO ? P_MENU : pant + 1, falta = 0;
        for (p = 0; p < NV; p++) if ((mascara(sigp) & (1 << p)) && pidx[p] >= 0 && !cargada[p]) falta += pan[pidx[p]].vram;
        if (falta && pvr_mem_available() > (size_t)(falta + PRECARGA_MARGEN)) asegura(mascara(sigp) | mascara(pant));
    }
    if (fase == F_QUIETO) control(n);
    else if (fase == F_SALE) {
        int fin = t >= (pant == P_TITULO ? 30 : T_FUNDIDO);
        if (pant == P_CAMBIO || pant == P_MUSICA) fin = fin && an_hecho(pant == P_CAMBIO ? &a_sel : &a_radio);
        if (pant == P_EDITOR) fin = fin && an_hecho(&a_ed);
        if (fin) {
            if (sig < 0) { pista_sonando = -2; pon_pista(menu_pista()); return MENU_CARRERA; }
            entra(sig, !sale_ok);
        }
    }
    return MENU_SIGUE;
}

/* ---------------------------------------------------------------- dibujo */
/* datos de la tarjeta (0x440088 / 0x44009F; medidas en la escena: franja del nombre y = 175, cajas de foto, bandera y
   signo; los iconos pequenos estan colocados para la tarjeta en -8) */
/* ---- progreso de la licencia (PS2 0x1a17e8 = 447400 del PC; tablas del bloque PRG1 de MNU2, del disco) */
#define PRG_TIPOS (prg + 40)                                /* u8 [3][15]: tipo de cada carrera de los campeonatos */
static int prg_i32(int o) { int v; memcpy(&v, prg + o, 4); return v; }
static float prg_f32(int o) { float v; memcpy(&v, prg + o, 4); return v; }
/* 0x241ed0: nota de la categoria c (0..39): media de sus notas (licencia +0x1DD + 5 c), 7 si falta alguna o no tiene */
static int nota_cat(const uint8_t *p, int c)
{
    int n = prg[c], i, s = 0;
    for (i = 0; i < n; i++) { int g = p[0x1dd + 5 * c + i]; if (g == 7) return 7; s += g; }
    return n ? s / n : 7;
}
/* 0x2420a0: nota del grupo g (0..6): 0-3 categorias 0-4, 5-13, 14-24, 25-39 (/5, /9, /11, /15); 4-6 los campeonatos
   (0x241fd8: 15 carreras de su curso, CARRERAS_N notas cada una en los cuartetos de licencia +0x36E + 2 tipo; / 41) */
static int nota_grupo(const uint8_t *p, int g)
{
    static const int LIM[5] = { 0, 5, 14, 25, 40 };
    int s = 0, d, i, k;
    if (g < 4) {
        for (i = LIM[g]; i < LIM[g + 1]; i++) { int v = nota_cat(p, i); if (v != 7) s += v + 1; }
        d = LIM[g + 1] - LIM[g];
    } else {
        for (i = 0; i < 15; i++) {
            int t = PRG_TIPOS[15 * (g - 4) + i], w = p[0x36e + 2 * t] | p[0x36f + 2 * t] << 8;
            for (k = 0; k < prg_i32(88 + 4 * i); k++) { int v = k < 3 ? (w >> 4 * k) & 15 : w >> 12; if (v != 7) s += v + 1; }
        }
        d = 41;
    }
    s = s / d - 1;
    return s > -1 ? s : 7;
}
/* 0x1a17e8: porcentaje = suma de los pesos (10, 20, 30, 40, 60, 80, 100; 0 sin nota) de los 7 grupos / 7 */
static float progreso(const uint8_t *p)
{
    float t = 0; int g;
    if (!prg) return 0;
    for (g = 0; g < 7; g++) t += prg_f32(148 + 4 * nota_grupo(p, g));
    return t / 7.0f;
}
/* 0x1f6d70: tiempo de juego (licencia +0x10C, cuadros) en horas, minutos y segundos: s = cuadros / 60,2 */
static void tiempo_lic(uint32_t cuadros, int *h, int *m, int *sg)
{
    float f = (float)cuadros / 60.2f, r; unsigned s = (uint16_t)(int)f, ms;
    r = (f - (float)s) * 1000.0f;
    if (r < 0) { r += 1000.0f; s = (uint16_t)(s - 1); }
    ms = (uint16_t)(int)r;
    if (ms >= 1000) s = (uint16_t)(s + 1);
    *m = s / 60; *sg = s % 60; *h = *m / 60; *m %= 60;
}
/* datos de la tarjeta de la licencia elegida (PS2 0x275080 / 0x275f80 = 4E2150 / 4E2640 del PC): fuente 14 sin
   alinear (flags 1, la esquina de arriba), color 0xFF3F474A del PS2 (aqui el de los rotulos, col_tarjeta). Nueva
   (p NULL): "0.00%", "?", "?", "0", "0". Porcentaje en (354, 209) (359 en la nueva), rango (359, 232), proporcion
   (359, 255), tiempo (359, 278), millas (210, 311); dx: el deslizamiento de la tarjeta */
static void tarjeta_datos(const uint8_t *p, float dx, float alfa, float z)
{
    char v[5][32]; int i, h, m, sg;
    static const float X[5] = { 354, 359, 359, 359, 210 }, Y[5] = { 209, 232, 255, 278, 311 };
    if (!p) { strcpy(v[0], "0.00%"); strcpy(v[1], "?"); strcpy(v[2], "?"); strcpy(v[3], "0"); strcpy(v[4], "0"); }
    else {
        float f8, f10, f24; uint32_t rango;
        memcpy(&f8, p + 0x108, 4); memcpy(&f10, p + 0x110, 4); memcpy(&f24, p + 0x24, 4); memcpy(&rango, p + 0x118, 4);
        snprintf(v[0], 32, "%5.2f%%", (double)progreso(p));
        if (f8 <= 0 || !prg || rango >= 24) { strcpy(v[1], "?"); strcpy(v[2], "?"); }
        else {
            float f4; memcpy(&f4, p + 0x104, 4);
            snprintf(v[1], 32, "%s (%d)", (const char *)prg + 180 + 8 * rango, (int)f10);
            snprintf(v[2], 32, "%5.2f%%", (double)(f4 / f8 * 100.0f));
        }
        tiempo_lic((uint32_t)l32(p, 0x10c), &h, &m, &sg);
        snprintf(v[3], 32, "%2d''%02d'%02d", h, m, sg);
        snprintf(v[4], 32, "%.0f", (double)f24);
    }
    for (i = 0; i < 5; i++) texto_c(v[i], (i == 0 && !p ? 359 : X[i]) + dx, Y[i], 1, alfa, z, col_tarjeta);
}

/* nombre en (xn, yn) con la fuente 14 sin alinear (la tarjeta de elegir: 202, 167; la del editor: 126, 168) */
static void tarjeta(const uint8_t *p, const uint8_t *nombre, int n, float xn, float yn, float dx, float alfa, float z)
{
    if (nombre) texto(nombre, n, xn, yn, 1, alfa, z, 0xFFFFFF);
    if (p) {
        int f = l32(p, 0x20), b = l32(p, 0x1c), s = l32(p, 0x18);
        hudd_dibuja(md, us[U_FOTO], f < 0 || f > 5 ? 0 : f, dx + 8, 0, alfa, z, 0);
        hudd_dibuja(md, us[U_BAND], b < 0 || b > 25 ? 0 : b, dx + 8, 0, alfa, z, 0);
        hudd_dibuja(md, us[U_SIGNO], s < 0 || s > 11 ? 0 : s, dx + 8, 0, alfa, z, 0);
    }
}

void menu_dibuja(void)
{
    if (fsp_activo()) { fsp_dibuja(menu_coche3d()); return; }
    float a_ent = fase == F_ENTRA ? fminf(t / (float)T_FUNDIDO, 1.0f) : 1.0f;
    float a_sal = fase == F_SALE ? fmaxf(1.0f - t / (float)T_FUNDIDO, 0.0f) : 1.0f;
    float sacude = t_bloq ? sinf(t_bloq * 1.6f) * t_bloq * 0.6f : 0.0f;
    int cab, cab_sale;
    if (!md || pant < 0) return;
    if (cargando) a_ent = 0;
    cab = cabecera(pant); cab_sale = fase == F_SALE && cabecera(sig < 0 ? P_MUSICA : sig) != cab;
    if (pant == P_TITULO) {
        float a = 1;
        if (fase == F_SALE) a = (t / 3) & 1 ? 0.2f : 1.0f;  /* parpadeo rapido al pulsar */
        else if ((t % 64) >= 44) a = 0;
        hudd_dibuja(md, us[U_TFONDO], 0, 0, 0, 1, 1, 0);
        hudd_dibuja(md, us[U_TLOGO], 0, 0, 0, 1, 2, 0);
        hudd_dibuja(md, us[U_TSTART], 0, 0, 0, a, 3, 0);
        return;
    }
    hudd_dibuja(md, us[U_FONDO], 0, 0, 0, 1, 0.0002f, 1);  /* detras del coche 3D */
    if (cab >= 0) an_dibuja(&a_cab, 0, (cab_sale ? a_sal : 1) * (cargando && barra_ant < 0 ? 0 : 1), 6, 0);
    {   /* barra de ayuda encima de la rejilla de la cabecera; se funde solo si cambia de una pantalla a otra */
        int b = barra(pant), bs = fase == F_SALE ? (sig < 0 ? -1 : barra(sig)) : b;
        float a = (bs != b ? a_sal : 1) * (fase == F_ENTRA && barra_ant != b ? a_ent : 1);
        if (b >= 0) hudd_dibuja(md, us[b], 0, 0, 0, a, 7.5f, 0);
    }
    switch (pant) {
    case P_MENU: case P_MODO: case P_RANKING: case P_MULTI:
        an_dibuja(&a_sel, sacude, (entra_atras ? a_ent : 1) * a_sal * (cargando ? 0 : 1), 3, 0);
        break;
    case P_COCHE:
        an_dibuja(&a_sel, 0, a_ent * a_sal, 4, 0);
        if (!c_desbloqueado(c_indice()) && an_hecho(&a_sel)) hudd_dibuja(md, us[U_CAND], 0, 0, 0, a_ent * a_sal, 5, 0);
        break;
    case P_CAMBIO:
        an_dibuja(&a_sel, 0, 1, 7, 0);
        break;
    case P_MUSICA:
        an_dibuja(&a_radio, 0, 1, 4, 0);
        an_dibuja(&a_ayuda, 0, a_sal, 4.5f, 0);
        if (!cargando) {                                    /* (la lista nueva aun sin VRAM: sin carrusel) */
            an_dibuja(&a_sel, sacude, a_sal, 3, 0);
            an_dibuja(&a_aux, 0, a_sal, 5, 0);
            if (!tema_libre(tema_actual()) && an_hecho(&a_sel)) hudd_dibuja(md, us[U_MCAND], 0, sacude, 0, a_sal, 6, 0);
        }
        hudd_dibuja(md, us[U_LISTA], lista, 0, 0, a_sal, 5.5f, 0);   /* nombre de la lista (5C9F18) */
        break;
    case P_LICENCIA: {
        float al = a_ent * a_sal, dx = 0; int s = l_idx < l_n ? l_lista[l_idx] : -1;
        if (l_desliza) { int k = l_desliza <= 10 ? l_desliza : l_desliza - 21; dx = -l_dir * 48.0f * k; }
        an_dibuja(&a_tarj, dx + sacude, al, 3, 0);
        if (an_hecho(&a_tarj) && s >= 0) {
            if (l_idx == l_nueva) { texto_id(0x218, 202 + dx, 167, 1, al, 4, 0xFFFFFF); tarjeta_datos(NULL, dx, al, 4); }
            else {
                tarjeta(lic[s], lic[s], strnlen((char *)lic[s], 16), 202 + dx, 167, 77 + dx, al, 4);
                tarjeta_datos(lic[s], dx, al, 4);
            }
        }
        if (l_idx > 0) an_dibuja(&a_fl_i, 0, al, 5, 0);
        if (l_idx < l_n - 1) an_dibuja(&a_fl_d, 0, al, 5, 0);
        if (l_ctx) {                                        /* Select / Edit / Reset License y si / no */
            static const int ID[3] = { 0x233, 0x237, 0x234 };
            int i;
            hudd_dibuja(md, us[U_LIC_BARRA], 11, 0, 104 + 22 * l_ctx_sel, al, 6, 0);
            for (i = 0; i < 3; i++) texto_id(ID[i], 320, 344 + 22 * i, 12, al, 7, i == l_ctx_sel ? 0x202020 : col_tarjeta);
            if (l_borra) {
                texto_id(0x213, 270, 410, 12, al, 7, l_borra == 1 ? 0xE0A000 : col_tarjeta);
                texto_id(0x214, 370, 410, 12, al, 7, l_borra == 2 ? 0xE0A000 : col_tarjeta);
            }
        }
        break; }
    case P_EDITOR: {
        uint8_t *p = lic[e_slot]; float al = a_sal;
        an_dibuja(&a_ed_t, 0, al, 3, 0);
        an_dibuja(&a_ed, 0, al, 4, 0);
        if (an_hecho(&a_ed_t)) {
            tarjeta(e_estado == E_REJILLA ? NULL : p, p, strnlen((char *)p, 16), 126, 168, -8, al, 5);
            if (e_estado == E_REJILLA) {                    /* el pequeno de la rejilla muestra el que se esta eligiendo */
                static const int U_P[3] = { U_FOTO, U_BAND, U_SIGNO }, U_R[3] = { U_REJ_FOTO, U_REJ_BAND, U_REJ_SIGNO };
                int i;
                for (i = 0; i < 3; i++) hudd_dibuja(md, us[U_P[i]], i == e_rej ? e_val : l32(p, REJ_OFF[i]), 0, 0, al, 5, 0);
                hudd_dibuja(md, us[U_R[e_rej]], e_val, 0, 0, al, 6, 0);
            }
            if (e_estado == E_NOMBRE) {                     /* teclado (468D40): 6 columnas en el sitio de las rejillas */
                int k; char c[2] = { 0, 0 };
                for (k = 0; k < N_TECLAS; k++) {
                    float x = 412 + 30 * (k % 6), y = 150 + 32 * (k / 6);
                    uint32_t col = k == e_tecla ? 0xE0A000 : col_tarjeta;
                    if (k == N_TECLAS - 1) texto_id(0x327, x - 15, y, 9, al, 6, col);
                    else if (TECLAS[k] == ' ') texto_c("-", x, y, 12, al, 6, col);
                    else { c[0] = TECLAS[k]; texto_c(c, x, y, 12, al, 6, col); }
                }
            }
            hudd_dibuja(md, us[U_HECHO], e_item == 4 ? 1 : 0, 0, 0, al, 5, 0);
        }
        break; }
    case P_OPCIONES: {                                      /* 4D60C0: valor de cada opcion con flechas */
        static const int PRI[3] = { 3, 6, 8 }, ROT[3] = { 0x2b, -1, 0x2a };
        const uint8_t *p = activa(); int i; float al = a_ent * a_sal;
        hudd_dibuja(md, us[U_OPC_BARRA], (int)a_barra.c, 0, -80 + 40 * e_item, al, 3, 0);
        for (i = 0; i < 3; i++) {
            int v = p ? l32(p, 0xd8 + 4 * i) : (i == 0 ? 2 : 0); float y = 160 + 40 * i;
            if (ROT[i] >= 0) texto_id(ROT[i], 190, y, 9, al, 4, col_tarjeta);
            texto_c("<", 360, y, 12, al, 4, col_tarjeta); texto_c(">", 520, y, 12, al, 4, col_tarjeta);
            texto_id(PRI[i] + v, 440, y, 12, al, 4, i == e_item ? 0x202020 : col_tarjeta);
        }
        break; }
    }
}

/* ---------------------------------------------------------------- volumen (licencia +D4 efectos, +D0 musica) */
/* Como el original (PS2 0x1a1a8c al leer la licencia; opciones de audio 0x2492a0 / 0x249530 = 4D78C0 / 4D89B0 del PC):
   valores 0..10 (fuera de rango: 0); efectos: 0x11ae38(trunc(v / 10 * 144)) = volumen general del controlador de
   sonido (sfx_master); musica: 0x100210(v / 10) = volumen del flujo de musica, v / 10 * 0x3fff (musica_nivel). Sin
   licencia, los de una licencia nueva (4471A0: 10 y 10). */
static int ap_se = -1, ap_bgm = -1;
static int vol_lic(int off) { const uint8_t *p = activa(); int v = p ? l32(p, off) : 10; return v < 0 || v > 10 ? 0 : v; }
static int master_de(int v) { return (int)((float)v / 10.0f * 144.0f); }
static void aplica_audio(void)
{
    int se = vol_lic(0xd4), bgm = vol_lic(0xd0);
    if (se != ap_se) { sfx_master(master_de(se)); ap_se = se; }
    if (bgm != ap_bgm) { musica_nivel((float)bgm / 10.0f); ap_bgm = bgm; }
}
void menu_aplica_audio(void) { aplica_audio(); }

/* ---------------------------------------------------------------- pausa de la carrera */
/* El menu de pausa del original (PS2 0x247b40 = 4D5E40 del PC con la raiz 16; control 4D7300): ventana con la lista
   VOLVER A LA PARTIDA, REINTENTAR, ABANDONAR, CONFIGURACION, CONTROLES y AUDIO (filas 8, 7, 6, 10, 11, 14 de la tabla
   0x4187F8; MENU.DAT, pantalla PAUSA). Los botones no responden hasta 15 cuadros despues de abrirla (692C9C = 15,05).
   A / START elige, B cierra (guarda la licencia si cambio: 4D66D0 / 4D7DA0), arriba / abajo mueven el cursor (con
   la vuelta, 48DC10 / 48DC60). REINTENTAR y ABANDONAR piden confirmacion ("Estas seguro?", 0x3A6: PS2 0x249d40 sin
   partida en red) con A (ELEGIR) y B (ATRAS). AUDIO (0x2492a0 / 0x249530): VOLUMEN DE SONIDO (+D4) y VOLUMEN DE
   MUSICA (+D0) de 0 a 10 con izquierda / derecha (sonido de cursor), aplicados al momento; la fila de la musica manda
   tambien su valor al volumen general de efectos (asi lo hace el original) hasta que se cambia de fila o se sale; B
   guarda los valores en la licencia y vuelve a la lista.
   CONFIGURACION y CONTROLES (brillo, pantalla, velocimetro, sensibilidad, vibracion...) no estan: la fila suena
   bloqueada (PROVISIONAL). La ventana (en el original, imagenes 0x3004x de 48C5F0 de un XST sin localizar) es un
   rectangulo claro con barra oscura (PROVISIONAL); su sitio y tamano son los del original (135, 110, 350 x filas * 16 +
   104; audio 120, 110, 380; confirmacion 120, 110, 380 x 150). */
enum { PZ_LISTA, PZ_AUDIO, PZ_CONFIRMA };
static int pz_estado, pz_sel, pz_aud, pz_conf, pz_t, pz_vol[2], pz_ok;
static const int PZ_FILA[6] = { 9, 8, 7, 0, 1, 4 };          /* cuadro de 0x2C00EE de cada fila */
#define PZ_ESPERA 15
static pvr_poly_hdr_t pz_cab;
static int pz_cab_ok;

static void pz_rect(float x, float y, float w, float h, uint32_t argb, float z)
{
    pvr_vertex_t v; int k;
    if (!pz_cab_ok) {
        pvr_poly_cxt_t cx; pvr_poly_cxt_col(&cx, PVR_LIST_TR_POLY);
        cx.depth.comparison = PVR_DEPTHCMP_ALWAYS; cx.depth.write = PVR_DEPTHWRITE_DISABLE; cx.gen.culling = PVR_CULLING_NONE;
        pvr_poly_compile(&pz_cab, &cx); pz_cab_ok = 1;
    }
    pvr_prim(&pz_cab, sizeof pz_cab);
    v.argb = argb; v.oargb = 0; v.u = v.v = 0; v.z = z;
    for (k = 0; k < 4; k++) {
        v.flags = k == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = x + ((k & 1) ? w : 0); v.y = y + ((k & 2) ? h : 0);
        pvr_prim(&v, sizeof v);
    }
}
/* ventana (PROVISIONAL, ver arriba) con las medidas de 48C5F0: cuerpo claro, cabecera de 26, pie de 16 y franja de
   los botones de 30 en y + h - 46 */
static void pz_ventana(float x, float y, float w, float h, float z)
{
    pz_rect(x, y, w, h, 0xF0D8DCE0, z);
    pz_rect(x, y, w, 26, 0xF0303840, z + 0.001f);
    pz_rect(x, y + h - 16, w, 16, 0xF0303840, z + 0.001f);
    pz_rect(x + 3, y + h - 46, w - 9, 30, 0xE0505860, z + 0.001f);
}

/* botones de la ventana (48C5F0: ELEGIR 0x294 en (x + 45, y + h - 38), ATRAS 0x295 acabando en x + w - 45) con la letra
   del boton de la Dreamcast delante (los iconos del original son imagenes del XST sin localizar) */
static void pz_botones(float x, float y, float w, float h, float z, int elegir)
{
    int n; const uint8_t *t = txt(0x295, &n); float xa = x + w - 45 - f_ancho(t, n);
    if (elegir) {
        texto_c("A", x + 45 - 14, y + h - 38, 1, 1, z, 0xE83428);
        texto_id(0x294, x + 45, y + h - 38, 1, 1, z, 0xF0F0F0);
    }
    texto_c("B", xa - 14, y + h - 38, 1, 1, z, 0x2860E8);
    texto(t, n, xa, y + h - 38, 1, 1, z, 0xF0F0F0);
}

int menu_pausa_entra(void)
{
    pz_estado = PZ_LISTA; pz_sel = 0; pz_t = 0; pz_ok = 0;
    if (!md || pidx[V_PAUSA] < 0) return -1;
    if (sube_p(V_PAUSA)) { dbglog(DBG_INFO, "menu: pausa sin VRAM (libre %uK)\n", (unsigned)(pvr_mem_available() >> 10)); return -2; }
    pz_ok = 1;
    return 0;
}
unsigned menu_pausa_vram(void) { return md && pidx[V_PAUSA] >= 0 && !cargada[V_PAUSA] ? pan[pidx[V_PAUSA]].vram : 0; }
void menu_pausa_sale(void) { if (md && pidx[V_PAUSA] >= 0) baja_p(V_PAUSA); pz_ok = 0; }

static void pz_guarda(void) { const uint8_t *p = activa(); if (p && (p[0x3f4] & 2)) vmu_guarda(); }

int menu_pausa_paso(unsigned n)
{
    int ok = n & (CONT_A | CONT_START), atras = n & CONT_B, ar = n & CONT_DPAD_UP, ab = n & CONT_DPAD_DOWN;
    int iz = n & CONT_DPAD_LEFT, de = n & CONT_DPAD_RIGHT;
    pz_t++;
    if (pz_t <= PZ_ESPERA) return PAUSA_SIGUE;
    switch (pz_estado) {
    case PZ_LISTA:
        if (ar) { pz_sel = (pz_sel + 5) % 6; sfx_menu(SFX_CURSOR); }
        else if (ab) { pz_sel = (pz_sel + 1) % 6; sfx_menu(SFX_CURSOR); }
        else if (atras) { sfx_menu(SFX_CANCELA); pz_guarda(); return PAUSA_CONTINUA; }
        else if (ok) {
            if (pz_sel == 0) { sfx_menu(SFX_ACEPTA); return PAUSA_CONTINUA; }
            if (pz_sel == 1 || pz_sel == 2) { pz_estado = PZ_CONFIRMA; pz_conf = pz_sel; sfx_menu(SFX_ACEPTA); }
            else if (pz_sel == 5) {
                pz_estado = PZ_AUDIO; pz_aud = 0; pz_vol[0] = vol_lic(0xd4); pz_vol[1] = vol_lic(0xd0);
                sfx_menu(SFX_ACEPTA);
            } else sfx_menu(SFX_BLOQUEADO);                 /* CONFIGURACION, CONTROLES: no estan */
        }
        break;
    case PZ_CONFIRMA:                                       /* 4D7B20 / 4D7C70: A confirma, B vuelve */
        if (ok) { sfx_menu(SFX_ACEPTA); pz_guarda(); return pz_conf == 1 ? PAUSA_REINTENTA : PAUSA_ABANDONA; }
        if (atras) { pz_estado = PZ_LISTA; sfx_menu(SFX_CANCELA); }
        break;
    case PZ_AUDIO:                                          /* 0x249530 */
        if (ar || ab) {                                     /* cambia de fila: vuelve a mandar el de efectos */
            sfx_master(master_de(pz_vol[0])); ap_se = pz_vol[0];
            pz_aud ^= 1; sfx_menu(SFX_CURSOR);
        } else if (iz || de) {
            int v = pz_vol[pz_aud] + (de ? 1 : -1);
            if (v >= 0 && v <= 10) { pz_vol[pz_aud] = v; sfx_menu(SFX_CURSOR); }   /* 4469F0 / 446A20 */
            if (pz_aud == 0) { sfx_master(master_de(pz_vol[0])); ap_se = pz_vol[0]; }
            else { musica_nivel((float)pz_vol[1] / 10.0f); ap_bgm = pz_vol[1]; sfx_master(master_de(pz_vol[1])); ap_se = -1; }
        } else if (atras || ok) {                           /* 4D7AB0: a la licencia; y a la lista */
            uint8_t *p = activa();
            sfx_master(master_de(pz_vol[0])); ap_se = pz_vol[0];
            if (p) { p32(p, 0xd4, pz_vol[0]); p32(p, 0xd0, pz_vol[1]); p[0x3f4] |= 2; }
            pz_estado = PZ_LISTA; sfx_menu(atras ? SFX_CANCELA : SFX_ACEPTA);
        }
        break;
    }
    return PAUSA_SIGUE;
}

void menu_pausa_dibuja(void)
{
    float z = 200.0f; int i;
    if (!md) return;
    u_fuente = U_P_FUENTE;
    if (pz_estado == PZ_LISTA) {                            /* (la confirmacion usa la misma ventana: sin lista) */
        pz_ventana(135, 110, 350, 6 * 16 + 104, z);
        for (i = 0; i < 6; i++) {
            hudd_dibuja(md, us[U_P_BARRA], i == pz_sel && pz_t > PZ_ESPERA ? 1 : 0, 0, 16 * i, 1, z + 1, 0);
            hudd_dibuja(md, us[U_P_FILA], PZ_FILA[i], 0, 16 * i, 1, z + 2, 0);
        }
        pz_botones(135, 110, 350, 6 * 16 + 104, z + 3, 1);
    }
    if (pz_estado == PZ_AUDIO) {
        pz_ventana(120, 110, 380, 2 * 16 + 104, z);
        pz_botones(120, 110, 380, 2 * 16 + 104, z + 3, 0);   /* 4D78C0: ventana de tipo 2 (solo ATRAS) */
        for (i = 0; i < 2; i++) {
            hudd_dibuja(md, us[U_P_BARRA], i == pz_aud ? 1 : 0, 0, 16 * i, 1, z + 1, 0);
            hudd_dibuja(md, us[U_P_OPC], i, 0, 16 * i, 1, z + 2, 0);
            hudd_dibuja(md, us[U_P_VOL], pz_vol[i], 0, 16 * i, 1, z + 2, 0);
            /* flechas de 4463A0 (x - 12 y x + 0x97 - 12 en 321, 161 + 16 i): con la fuente 14 (las del original son
               imagenes del XST sin localizar) */
            if (pz_vol[i] > 0) texto_c("<", 321 - 12, 161 + 16 * i + 3, 1, 1, z + 3, 0x3f474a);
            if (pz_vol[i] < 10) texto_c(">", 321 + 0x97 - 12, 161 + 16 * i + 3, 1, 1, z + 3, 0x3f474a);
        }
    }
    if (pz_estado == PZ_CONFIRMA) {
        pz_ventana(120, 110, 380, 150, z);
        texto_id(0x3a6, 120 + 14, 110 + 38, 1, 1, z + 3, 0x202020);
        pz_botones(120, 110, 380, 150, z + 3, 1);
    }
    u_fuente = U_FUENTE;
}
