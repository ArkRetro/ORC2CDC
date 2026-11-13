/* Menus del modo principal con MENU.DAT (tools/conv_menu.py): mismo formato que HUD.DAT mas el bloque MNU1 con las
   pantallas (rango de texturas propio) y los tramos de animacion (uso, cuadro inicial, final, modo), y el MNU2 con los
   textos de TEXT.PS2 que se escriben en la consola, las metricas de la fuente 14 y las tablas de la seleccion de coche.
   Flujo del original (port de PC, 4D7300 / 4C9290 / 4E2970 / 4DE2B0, con los tokens del PS2):
     titulo (START/A) -> sin licencia: elegir licencia (crear) -> menu principal (SINGLE PLAYER, OUTRUN2SP, RANKINGS,
     OPTIONS; MULTIPLAYER fuera, DESVIACION pedida: menu_sin_multi) -> un jugador (COAST 2 COAST, OUTRUN, HEART ATTACK, TIME ATTACK, SHOWROOM) -> coche -> cambio
     AT/MT -> musica -> carrera; B vuelve atras. LICENSE (X en el menu principal y en un jugador) abre la eleccion de
     licencia. HEART ATTACK (INFARTO) y TIME ATTACK (CONTRARRELOJ) llevan a la carrera con su variante (menu_modo);
     lo que necesita modos que la demo no tiene (multijugador, rankings) tiene su pantalla y la accion
     bloqueada (sacudida y sonido 3, como un coche sin desbloquear). OUTRUN2SP lleva a la carrera del juego de OUTRUN2SP
     (menu_sp) por su frontal de recreativa (comun/fsp.c).
   SHOWROOM (SALON) abre el salon del original (P_SALON, SALON.DAT: la tienda de las millas OutRun; ver mas abajo).
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
   4471A0 y el nombre 0x219; en la VMU (la que tenga el fichero OR2C2C o la primera; vmu_pkg con el icono y el eyecatch de VMU.DAT).
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
#include "vmuesc.h"
#include "fsp.h"
#include "sprani.h"

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
enum { P_TITULO, P_MENU, P_MODO, P_COCHE, P_CAMBIO, P_MUSICA, P_LICENCIA, P_EDITOR, P_OPCIONES, P_RANKING, P_MULTI,
       P_C2C, P_SALON, P_TABLERO, NP };                     /* P_C2C: categorias y mapa de COAST 2 COAST (C2C.DAT); P_SALON:
                                                               el salon (SALON.DAT); P_TABLERO: los tableros de cursos
                                                               de INFARTO / CONTRARRELOJ (TABLERO.DAT) */
#define VB(v) (1 << (v))
static const int MASCARA[NP] = {
    VB(V_TITULO), VB(V_COMUN) | VB(V_MENU), VB(V_COMUN) | VB(V_MODO), VB(V_COMUN) | VB(V_COCHE),
    VB(V_COMUN) | VB(V_COCHE) | VB(V_CAMBIO), VB(V_COMUN) | VB(V_MUSICA), VB(V_COMUN) | VB(V_LICENCIA),
    VB(V_COMUN) | VB(V_LICENCIA), VB(V_COMUN) | VB(V_OPCIONES), VB(V_COMUN) | VB(V_RANKING), VB(V_COMUN) | VB(V_MULTI),
    VB(V_COMUN) | VB(V_MODO), VB(V_COMUN) | VB(V_MODO), VB(V_COMUN) | VB(V_MODO) };

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

#define N_OPC 5                                             /* opciones de un jugador (y del carrusel 0x440000) */
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
#ifdef PRUEBA_NOTAS_C2C
    {   /* prueba de las pantallas de COAST 2 COAST: notas en las categorias 0..5 (Flagman 1 entero y el principio de
           Flagman 2): la 0 con 6 en todas, la 1 con 4 / 5, la 2 con 3 en la 1.a (la 2.a cerrada), las demas sin jugar */
        int c, i;
        for (c = 0; c < 6; c++) for (i = 0; i < 5; i++) p[0x1dd + 5 * c + i] = c == 0 ? 6 : c == 2 ? (i ? 7 : 3) : c == 5 ? 7 : 4 + (i & 1);
    }
#endif
}
/* 447360: todo desbloqueado (nombre ENTIRETY en el editor) */
static void lic_desbloquea(uint8_t *p)
{
    memset(p + 0x28, 0xff, 0x96); memset(p + 0x125, 6, 0x52); memset(p + 0x177, 6, 0x52);
    memset(p + 0x1dd, 6, 0x190); memset(p + 0x1c9, 0, 20); memset(p + 0x36e, 0x66, 0x84);
}

/* VMU: fichero OR2C2C. Contenido: 'OR2L', u32 version, i32 licencia activa, 4 registros de 0x40C (version 2: y
   detras 'RKSP' y las tablas de los rankings de OUTRUN2SP, comun/rksp.c: los datos del sistema que la consola guarda con
   las licencias; los ficheros de la version 1 se siguen leyendo y los rankings quedan por defecto), detras de la
   cabecera VMS (vmu_pkg: descripciones, icono animado y eyecatch de VMU.DAT, comun/vmu.c). Los ficheros del formato
   anterior (icono liso, sin eyecatch; algunos con una cabecera que vmu_pkg_parse no acepta) se siguen leyendo: si la
   cabecera no vale, los datos se buscan por la marca 'OR2L'.
   Unidad (adaptacion de plataforma, revision 42; el PS2 tiene sus ranuras de tarjeta): la primera VMU que tenga el
   fichero (maple_enum_type(n, MAPLE_FUNC_MEMCARD): puertos A..D, ranuras 1 y 2); si ninguna lo tiene, la primera
   VMU enchufada. Antes se fijaba /vmu/a1.
   Escritura (revision 42; docs/DREAMCAST.md, VMU): con vmufs_write directamente y no con fs_open / fs_write /
   fs_close de fs_vmu: fs_vmu escribe de verdad en fs_close y antes no se miraba su resultado (el registro decia
   "bien" aunque fallara). Ahora se mira el resultado de vmufs_write, se relee el fichero (vmufs_read) y se compara
   byte a byte; si algo falla se reintenta (VMU_INTENTOS, con una pausa) y el registro lo dice. Mientras se escribe la
   LCD de las VMU no se toca (vmu_lcd_pausa) y la lectura de la musica del GD esta parada.
   Revision 44 (con la 43, en una Dreamcast PAL real seguia saliendo "Error al guardar!"): ya no se usa vmufs_write sino la
   escritura propia de comun/vmuesc.c (tramas maple con sus respuestas miradas, cada bloque releido, reintentos con
   pausas crecientes), en un hilo para el guardado con Y, con el codigo de diagnostico en pantalla si falla y la
   pantalla de diagnostico con L + R + Y. */
#define VMU_NOMBRE "OR2C2C"
#define VMU_INTENTOS 2
static int vmu_puerto = -1, vmu_unidad = -1;                /* unidad de la que se leyo (o en la que se guardo) */
static maple_device_t *vmu_unidad_guardar(void)
{
    maple_device_t *d;
    if (vmu_puerto >= 0 && (d = maple_enum_dev(vmu_puerto, vmu_unidad)) && (d->info.functions & MAPLE_FUNC_MEMCARD)) return d;
    return maple_enum_type(0, MAPLE_FUNC_MEMCARD);
}
static void vmu_lee(void)
{
    maple_device_t *dev = NULL; void *v = NULL; uint8_t *b; int sz = 0, i, r = -1;
    for (i = 0; (dev = maple_enum_type(i, MAPLE_FUNC_MEMCARD)); i++)
        if (!(r = vmufs_read(dev, VMU_NOMBRE, &v, &sz))) break;
    if (!dev || r) {
        dev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
        dbglog(DBG_INFO, "menu: sin %s en ninguna VMU (%d VMU; se guardara en %c%d)\n", VMU_NOMBRE, i,
               dev ? 'a' + dev->port : '-', dev ? dev->unit : 0);
        return;
    }
    vmu_puerto = dev->port; vmu_unidad = dev->unit; b = v;
    {
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
    dbglog(DBG_INFO, "menu: %s en %c%d, %d bytes, licencia %d%s\n", VMU_NOMBRE, 'a' + dev->port, dev->unit, sz, lic_sel,
           vmu_ok ? "" : " (no vale)");
    free(v);
}
/* escribe (vmuesc_escribe, comun/vmuesc.c: ruta propia sobre las tramas maple, cada bloque releido y comparado, al final
   el fichero entero releido); 0 bien. Si falla se repite el guardado entero (VMU_INTENTOS, 500 ms entre medias) salvo
   sin sitio / directorio lleno / sin VMU. vmu_diag: lo del ultimo guardado (el codigo que se ensena con el error y la
   pantalla de diagnostico, L + R + Y). Codigos: vmuesc.h y docs/DREAMCAST.md */
static vmuesc_diag_t vmu_diag;
static int vmu_escribe(const uint8_t *buf, int n)
{
    maple_device_t *dev, *v; int r = VE_SIN_VMU, k, nv;
    uint8_t *b; int nb = (n + 511) & ~511;
    for (nv = 0; (v = maple_enum_type(nv, MAPLE_FUNC_MEMCARD)); nv++) ;
    memset(&vmu_diag, 0, sizeof vmu_diag);
    vmu_diag.cod = VE_SIN_VMU; vmu_diag.fase = '-'; vmu_diag.libres = -1; vmu_diag.necesarios = nb / 512;
    vmu_diag.puerto = vmu_diag.unidad = vmu_diag.bloque = vmu_diag.paso = -1; vmu_diag.n_vmu = nv; vmu_diag.intento = 1;
    if (!(b = calloc(1, nb))) { vmu_diag.cod = VE_MEMORIA; return VE_MEMORIA; }   /* bloques enteros: relleno con ceros */
    memcpy(b, buf, n);
    for (k = 0; k < VMU_INTENTOS; k++) {
        if (k) thd_sleep(500);
        if (!(dev = vmu_unidad_guardar())) { r = VE_SIN_VMU; vmu_diag.cod = r; vmu_diag.intento = k + 1;
                                             dbglog(DBG_ERROR, "menu: no hay ninguna VMU\n"); break; }
        r = vmuesc_escribe(dev, VMU_NOMBRE, b, nb, &vmu_diag);
        vmu_diag.intento = k + 1; vmu_diag.n_vmu = nv;
        {
            char c[80]; vmuesc_codigo(&vmu_diag, c, sizeof c);
            dbglog(r ? DBG_ERROR : DBG_INFO, "menu: guardado %s en %c%d (%d bytes, %d bloques), intento %d: %s (%d) [%s]\n",
                   VMU_NOMBRE, 'a' + dev->port, dev->unit, n, nb / 512, k + 1, !r ? "bien, releido igual" :
                   r == VE_SITIO ? "no hay sitio" : r == VE_DISTINTO ? "releido distinto" : r == VE_RELEE ?
                   "no se puede releer" : "fallo", r, c);
        }
        if (!r) { vmu_puerto = dev->port; vmu_unidad = dev->unit; break; }
        if (r == VE_SITIO || r == VE_DIR_LLENO || r == VE_FORMATO) break;   /* reintentar no sirve */
    }
    free(b);
    return r;
}
/* el fichero empaquetado (cabecera VMS + datos), malloc; NULL sin memoria */
static uint8_t *vmu_prepara(int *n)
{
    vmu_pkg_t pkg; uint8_t *d, *out = NULL; int v = rksp_hay() ? 2 : 1;
    uint32_t tam = 12 + sizeof lic + (v == 2 ? 4 + RKSP_TAM : 0);
    d = malloc(tam); if (!d) return NULL;
    memcpy(d, "OR2L", 4); memcpy(d + 4, &v, 4); memcpy(d + 8, &lic_sel, 4); memcpy(d + 12, lic, sizeof lic);
    if (v == 2) { memcpy(d + 12 + sizeof lic, "RKSP", 4); memcpy(d + 16 + sizeof lic, rksp_tablas(), RKSP_TAM); }
    memset(&pkg, 0, sizeof pkg);
    vmu_cabecera(&pkg);                                     /* descripciones, icono y eyecatch (comun/vmu.c) */
    pkg.data_len = tam; pkg.data = d;
    if (vmu_pkg_build(&pkg, &out, n)) out = NULL;
    free(d);
    return out;
}
/* escritura con la LCD parada (y unos cuadros sin LCD antes: vmu_lcd_pausa espera a las tramas de LCD en vuelo, y
   despues 100 ms mas) y la lectura de la musica del GD parada; libera out */
static int vmu_escribe_pausado(uint8_t *out, int n)
{
    int r;
    musica_pausa_lectura(1); vmu_lcd_pausa(1); thd_sleep(100);
    r = vmu_escribe(out, n);
    vmu_lcd_pausa(0); musica_pausa_lectura(0);
    free(out);
    return r;
}
static void vmu_acaba(int r)
{
    int i;
    if (!r) { vmu_ok = 1; for (i = 0; i < N_LIC; i++) lic[i][0x3f4] &= ~2; }   /* +3F4 bit 1: cambios sin guardar */
}
static int vmu_guarda(void)
{
    int n, r; uint8_t *out = vmu_prepara(&n);
    if (!out) { memset(&vmu_diag, 0, sizeof vmu_diag); vmu_diag.cod = VE_MEMORIA; return VE_MEMORIA; }
    r = vmu_escribe_pausado(out, n);
    vmu_acaba(r);
    return r;
}

/* la entrada de nombre de OUTRUN2SP (demo/nombre.c): la licencia activa (el nombre +0, el coche +0x1C) y el guardado
   (estados 7..0xB: los rankings 0x106288 / 0x1063d8 y la licencia 0x1a67e0 / 0x1a6840, aqui un solo fichero) */
const uint8_t *menu_licencia(void) { return activa(); }
int menu_guarda_vmu(void) { return vmu_guarda(); }

/* ---------------------------------------------------------------- tramos */
typedef struct { int u; float c; int ini, fin, modo; } anim_t;
enum { F_ENTRA, F_QUIETO, F_SALE };
static anim_t a_cab, a_sel, a_aux, a_radio, a_ayuda, a_tarj, a_fl_d, a_fl_i, a_ed, a_ed_t, a_barra;
static int pant = -1, fase, t, sig, inact, t_bloq, entra_atras, barra_ant = -1, cargando, t_carga;
enum { G_NADA, G_PIDE, G_BIEN, G_MAL };
static int g_est, g_t;                                      /* guardar con Y (g_paso) */
static int modo_sel = 1;                                    /* opcion de un jugador elegida */
/* OUTRUN2SP del menu principal. En el original (SLES-53998 0x23aba0 = PC 4E2970, opcion 2) pone [0x59acac] = 0 (el
   juego de OUTRUN2SP en vez del de la consola), +21C = 7 y el modo 32 pasa al 7 -> 9 -> 10: el frontal de recreativa
   de OUTRUN2SP (PC 4BEE60..4C4880, banco SPRANI 0x2E = SELECTOR_CVT: tipo de juego y curso OutRun 2 / OutRun 2 SP,
   coche, cambio, musica...). Ese frontal esta SIN PORTAR: aqui se pasa por las pantallas de coche, cambio y musica
   del menu de la consola (PROVISIONAL, en su lugar) y se corre la eleccion OUTRUN con los cursos de OutRun 2 SP de la
   lista del frontal (4C3DC0: fila 0 -> variante 1, lado 1 -> preset 1). B en el coche vuelve al menu principal. */
static int sp_sel;
/* MULTIPLAYER fuera del menu principal (DESVIACION pedida por el usuario, demo 41: sin multijugador; docs/PARIDAD.md,
   Menu principal sin MULTIPLAYER). El carrusel del original (escena 0x440000, tabla 0x418CA8: 5 opciones, opcion i
   quieta en el cuadro 20 + 20 i) es una fila de rotulos que pasa de derecha a izquierda (y = 360, el elegido en x = 62
   y los de su derecha cada vez mas transparentes, con un punto delante de cada uno), el titulo y la descripcion de la
   elegida (y = 166 / 211) y una fila de 5 puntos con el de la elegida resaltado (y = 391,5). Se dibuja como un carrusel
   de 4: los sitios 0..3 de la escena con las opciones 0, 2, 3, 4 (el rotulo de la opcion en el sitio del rotulo de ese
   sitio; titulo y descripcion en su propio sitio, son quietos) y sin lo del sitio 4 (su rotulo, su punto separador, su
   punto de la fila). Las reglas (sprites de cada opcion, del separador y de los puntos) se sacan de las piezas de
   MENU.DAT en los cuadros SELi (menu_sin_multi); si no salen, las 5 opciones como el original. c_menu es el sitio */
#define OPC_QUITADA 1                                       /* MULTIPLAYER */
static int n_menu = N_OPC;
static hudd_regla_t r_menu[20]; static int n_r_menu;
static int opc_menu(int c) { return n_menu < N_OPC && c >= OPC_QUITADA ? c + 1 : c; }
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

/* el carrusel del menu principal sin MULTIPLAYER (ver sp_sel): en el cuadro SELi de la escena, el rotulo de la opcion
   i es el mas opaco de la fila y = 360 (el elegido), su descripcion la de y = 211 y su titulo la mas opaca por encima;
   el separador, la pieza de y = 371; en SEL0 los puntos de y = 391,5: el sprite que sale 5 veces (el resaltado, 1) y
   el del sitio 4 el de mas a la derecha */
static void menu_sin_multi(void)
{
    enum { MAXP = 128 };
    hudd_pz_t pz[MAXP]; int lab[N_OPC], des[N_OPC], tit[N_OPC], sep = -1, pto = -1, i, j, k, n, ini, fin, modo, u = us[U_PRINC];
    float xp = -1e9f;
    n_menu = N_OPC; n_r_menu = 0;
    for (i = 0; i < N_OPC; i++) {
        char b[8]; float al[3] = { -1, -1, -1 };
        lab[i] = des[i] = tit[i] = -1;
        sprintf(b, "SEL%d", i);
        if (u < 0 || !tramo(u, b, &ini, &fin, &modo)) goto mal;
        n = hudd_piezas(md, u, ini, pz, MAXP);
        for (j = 0; j < n; j++) {
            float y = pz[j].y;
            if (y > 385) {                                  /* fila de puntos */
                int c = 0;
                if (i) continue;
                for (k = 0; k < n; k++) if (pz[k].spr == pz[j].spr && pz[k].y > 385) c++;
                if (c >= N_OPC) { pto = pz[j].spr; if (pz[j].x > xp) xp = pz[j].x; }
                continue;
            }
            if (y > 365) { sep = pz[j].spr; continue; }
            k = y > 355 ? 0 : y > 205 && y < 215 ? 1 : 2;
            if (pz[j].alfa > al[k]) { al[k] = pz[j].alfa; (k == 0 ? lab : k == 1 ? des : tit)[i] = pz[j].spr; }
        }
        if (lab[i] < 0 || des[i] < 0 || tit[i] < 0) goto mal;
    }
    if (sep < 0 || pto < 0) goto mal;
#define R_(s_, n_, m_, si_, an_, a_, b_) do { hudd_regla_t *r_ = r_menu + n_r_menu++; r_->spr = s_; r_->nuevo = n_; \
        r_->modo = m_; r_->sitio = si_; r_->ancla = an_; r_->x0 = a_; r_->x1 = b_; } while (0)
    for (i = OPC_QUITADA; i < N_OPC - 1; i++) {             /* sitio i: la opcion i + 1 */
        R_(lab[i], lab[i + 1], 0, 0, -1, 0, 0); R_(des[i], des[i + 1], 0, 1, -1, 0, 0); R_(tit[i], tit[i + 1], 0, 1, -1, 0, 0);
    }
    R_(lab[N_OPC - 1], -1, 0, 0, -1, 0, 0); R_(des[N_OPC - 1], -1, 0, 0, -1, 0, 0); R_(tit[N_OPC - 1], -1, 0, 0, -1, 0, 0);
    R_(pto, -1, 1, 0, -1, xp - 2, xp + 2);                  /* el punto del sitio 4 */
    R_(sep, -1, 2, 0, lab[N_OPC - 1], 0, 20);               /* el separador delante del rotulo del sitio 4 (7..11 px) */
#undef R_
    n_menu = N_OPC - 1;
    dbglog(DBG_INFO, "menu: menu principal sin MULTIPLAYER (rotulos %d..%d, separador %d, puntos %d)\n", lab[0], lab[N_OPC - 1], sep, pto);
    return;
mal:
    n_r_menu = 0; n_menu = N_OPC;
    dbglog(DBG_ERROR, "menu: el carrusel del menu principal no es el esperado: con MULTIPLAYER\n");
}

/* COAST 2 COAST (mas abajo) */
static int c2_abre(void);
static void c2_cierra(void);
static void c2_entra(void);
static void c2_control(unsigned n);
static void c2_dibuja(void);
static int c2_activo(void);
static void bloqueado(void);
/* SALON (mas abajo) */
static int sa_abre(void);
static void sa_cierra(void);
static void sa_entra(void);
static int sa_control(unsigned n, int atiende);
static void sa_dibuja(void);
static int sa_activo(void);
static int sa_car_on, sa_car_reg, sa_car_col;
/* TABLEROS de cursos (mas abajo) */
static int tb_abre(void);
static void tb_cierra(void);
static void tb_entra(void);
static int tb_control(unsigned n, int atiende);
static void tb_dibuja(void);
static int tb_activo(void);
static int tb_sig;
static uint32_t tb_bits;
typedef struct { int desbl, modelo, bit, ficha; int8_t color, col[8]; } sa_reg_t;
static sa_reg_t sa_reg[30];

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
void menu_sale(void) { int p; c2_cierra(); sa_cierra(); tb_cierra(); for (p = 0; p < NV; p++) baja_p(p); fsp_sale(); hudd_vacia(); }
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
static int mc_on, mc_grupos;                                /* la pantalla de carga (menu_carga_entra) */
static uint32_t grupos_carrera(void)
{
    int pa = pidx[V_PAUSA], ti = pidx[V_TITULO];
    return hudd_grupos_suelta2(md, pa >= 0 ? pan[pa].t0 : 0, pa >= 0 ? pan[pa].nt : 0,
                               mc_on && ti >= 0 ? pan[ti].t0 : 0, mc_on && ti >= 0 ? pan[ti].nt : 0);
}
int menu_tablas(int en)
{
    if (!md) return 0;
    if (en) return hudd_grupos_carga(md);
    mc_grupos = mc_on;                                      /* (con la pantalla de carga, tambien las del titulo) */
    return (int)grupos_carrera();
}

/* Pantalla de carga propia al empezar un modo de un jugador (DESVIACION pedida por el usuario, demo 41: "una pantalla
   de carga con estilo de outrun ... al principio del modo, no entre etapas"; demo/pcarga.c la lleva). Con lo del
   titulo de MENU.DAT (TITULO_FONDO: el blanco, TITULO_LOGO: el logo "OutRun2006 Coast 2 Coast" de la pelicula del
   titulo, en su sitio) y, en el sitio del START del titulo, el medidor del volumen de la pausa (0x2C00EF, cuadro 0..10)
   como barra de lo cargado: nada que no este ya convertido, sin textos (TEXT.PS2 no tiene un "Cargando" suelto). La
   VRAM, 64 KB (pantallas TITULO y PAUSA); en la RAM nada mas que sus piezas (72 + 528 bytes), que menu_tablas(0) deja
   hasta que se quita la pantalla */
int menu_carga_entra(void)
{
    if (!md || pidx[V_TITULO] < 0 || pidx[V_PAUSA] < 0 || us[U_TFONDO] < 0 || us[U_TLOGO] < 0 || us[U_P_VOL] < 0) return -1;
    if (sube_p(V_TITULO) || sube_p(V_PAUSA)) { baja_p(V_TITULO); baja_p(V_PAUSA); return -2; }
    mc_on = 1;
    return 0;
}
void menu_carga_dibuja(float avance)
{
    int k = (int)(avance * 10.0f + 0.5f);
    if (!mc_on) return;
    k = k < 0 ? 0 : k > 10 ? 10 : k;
    hudd_dibuja(md, us[U_TFONDO], 0, 0, 0, 1, 1, 0);
    hudd_dibuja(md, us[U_TLOGO], 0, 0, 0, 1, 2, 0);
    /* el medidor de la fila 0 del audio de la pausa va de x 321 a 321 + 0x97 en y 161 (4463A0): centrado en el sitio
       del START del titulo (320, 400) */
    hudd_dibuja(md, us[U_P_VOL], k, 320 - (321 + 0x97 / 2), 400 - 161 - 8, 1, 3, 0);
}
void menu_carga_baja(void) { if (mc_on) { baja_p(V_TITULO); baja_p(V_PAUSA); } }
int menu_carga_sube(void) { return !mc_on ? -1 : sube_p(V_TITULO) || sube_p(V_PAUSA) ? -2 : 0; }
void menu_carga_sale(void)
{
    if (!mc_on) return;
    pvr_wait_ready();                                       /* la PowerVR ya no lee sus texturas */
    mc_on = 0;
    baja_p(V_TITULO); baja_p(V_PAUSA);                     /* (la pausa aun no esta abierta) */
    hudd_vacia();
    if (mc_grupos) { mc_grupos = 0; grupos_carrera(); }     /* las piezas del titulo, fuera (las de la pausa no) */
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
        /* u8 modelo[30] y detras, sin relleno (conv_menu.mnu2), i16 desbloqueo[30]: en +30 (antes se leia en +32 y
           cada ficha tomaba el bit, los colores y el color inicial de la siguiente; revision 42, el SALON) */
        t_modelo = mnu2 + h[4]; t_desbloq = (const int16_t *)(t_modelo + 30);
        t_colbit = t_desbloq + 30; t_colini = (const int8_t *)(t_colbit + 240);
        if (h[5]) col_tarjeta = h[5];
        if (h[6] && !memcmp(mnu2 + h[6], "PRG1", 4)) prg = mnu2 + h[6] + 4;
    }
    menu_sin_multi();
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
    case P_MODO: case P_C2C: case P_SALON: case P_TABLERO: return U_CABMODO;
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
    case P_CAMBIO: case P_C2C: case P_TABLERO: return U_BARRA_CAMBIO;   /* (C2C y tableros: SELECT y BACK) */
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
    /* al entrar en las pantallas de COAST 2 COAST y al salir de ellas, la VRAM del menu (y el coche 3D) fuera y lo
       pendiente liberado ya, como antes del frontal de recreativa: sus escenas (hasta 492 KB) van en una VRAM sin
       huecos y al salir no dejan huecos entre las pantallas que se vuelven a subir (antes: "out of PVR memory" al
       subir el cambio y la musica tras el coche) */
    if (p != P_C2C) c2_cierra();
    if (p != P_SALON) sa_cierra();
    if (p != P_TABLERO) tb_cierra();
    if ((p == P_C2C) != (pant == P_C2C) || (p == P_SALON) != (pant == P_SALON) || (p == P_TABLERO) != (pant == P_TABLERO))
        menu_libera_todo();                                 /* (igual el salon y los tableros) */
    cargando = !asegura(mascara(p)); t_carga = 0;
    barra_ant = pant >= 0 ? barra(pant) : -1;
    pant = p; fase = F_ENTRA; t = 0; entra_atras = atras; t_bloq = 0;
    if (cab < 0) a_cab.u = -1;
    else if (us[cab] != a_cab.u) { a_cab.u = us[cab]; an_tramo(&a_cab, "ENTRA"); }   /* cabecera nueva */
    GANCHO(p, 0, 0);
    switch (p) {
    case P_TITULO: fase = F_QUIETO; pon_pista(-1); break;
    case P_C2C: {
        if (c2_abre()) { entra(P_MODO, 1); bloqueado(); return; }   /* (sin C2C.DAT o sin el hueco)  */
        c2_entra();
        break; }
    case P_SALON:
        if (sa_abre()) { entra(P_MODO, 1); bloqueado(); return; }   /* (sin SALON.DAT o sin el hueco) */
        sa_entra();
        break;
    case P_TABLERO:
        if (tb_abre()) { entra(P_MODO, 1); bloqueado(); return; }   /* (sin TABLERO.DAT o sin el hueco) */
        tb_entra();
        break;
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
   Attack, 7 Time Attack, 6 las misiones de COAST 2 COAST) */
static int mis_sel = -1;                                    /* COAST 2 COAST: la mision elegida (ver abajo) */
int menu_modo(void) { return sp_sel ? fsp_variante() : modo_sel == 2 ? 2 : modo_sel == 3 ? 7 : modo_sel == 0 && mis_sel >= 0 ? 6 : 1; }

/* COAST 2 COAST: las misiones de Races.bin (variante 6 de 4EEAD0, selector 0). Pantallas del original (P_C2C, mas
   abajo): categorias (suceso 0x3a), mapa de misiones y detalle de la mision (suceso 0x3b); de ahi a la seleccion de
   coche (suceso 0xa) como los otros modos. main da la lista de Races.bin con menu_pon_misiones: indice, clave
   (categoria + 1), subclave (carrera + 1), tipo (+0x20) y si la carrera se puede correr (tipo 0 y su curso cabe en los
   huecos de la RAM) */
#define MIS_MAX 94
static int mis_n;
static int16_t mis_idx[MIS_MAX]; static uint8_t mis_cl[MIS_MAX], mis_sb[MIS_MAX], mis_tipo[MIS_MAX], mis_jug[MIS_MAX];
void menu_pon_misiones(int n, const int16_t *idx, const uint8_t *clave, const uint8_t *sub, const uint8_t *tipo,
                       const uint8_t *jugable)
{
    int i;
    mis_n = n < 0 ? 0 : n > MIS_MAX ? MIS_MAX : n;
    for (i = 0; i < mis_n; i++) {
        mis_idx[i] = idx[i]; mis_cl[i] = clave[i]; mis_sb[i] = sub[i]; mis_tipo[i] = tipo[i]; mis_jug[i] = jugable[i];
    }
}
/* la mision (clave, subclave) en la lista (0x1f7f20 = 4958C0: el registro de Races.bin con esa clave y subclave) */
static int mis_busca(int clave, int sub)
{
    int i;
    for (i = 0; i < mis_n; i++) if (mis_cl[i] == clave && mis_sb[i] == sub) return i;
    return -1;
}
int menu_nota_de(int clave, int sub)
{
    const uint8_t *p = activa(); int off = 0x1d7 + 5 * clave + sub;
    return p && off >= 0x1d7 && off < 0x1dd + 0xc8 ? p[off] : 7;
}
void menu_elige_mision(int idx) { mis_sel = idx; }
int menu_mision(void) { return modo_sel == 0 && !sp_sel ? mis_sel : -1; }
/* 495A20 (PS2 0x1f81cc): nota de la mision en la licencia +0x1D7 + 5 clave + subclave si mejora o la guardada es 7
   (sin jugar); +3F4 |= 2. Se guarda en la VMU al volver al menu (menu_entra) */
void menu_nota_mision(int clave, int sub, int nota)
{
    uint8_t *p = activa(); int off = 0x1d7 + 5 * clave + sub, have;
    if (!p || off < 0x1d7 || off >= 0x1dd + 0xc8) return;
    have = p[off];
    if (nota > have || have == 7) { p[0x3f4] |= 2; p[off] = (uint8_t)nota; }
    dbglog(DBG_INFO, "LICENCIA: mision %d-%d nota %d (guardada %d)\n", clave, sub, nota, p[off]);
}
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
    if (pant == P_SALON) return md && sa_car_on && fase != F_SALE && !cargando && ficha_de(sa_reg[sa_car_reg].modelo) >= 0;
    return md && pant == P_COCHE && ficha_de(c_modelo(c_indice())) >= 0 && fase != F_SALE && !cargando && an_hecho(&a_sel);
}
int menu_modelo(void) { return sp_sel ? fsp_modelo() : pant == P_SALON && sa_car_on ? sa_reg[sa_car_reg].modelo : c_modelo(c_indice()); }
int menu_coche(void) { return ficha_de(menu_modelo()); }
int menu_color(void) { return sp_sel ? fsp_color() : pant == P_SALON && sa_car_on ? sa_car_col : colores[c_indice()] + 1; }
float menu_giro(void) { return giro; }
int menu_vista(void) { const uint8_t *p = activa(); return p ? l32(p, 0xd8) : 2; }
int menu_mph(void) { const uint8_t *p = activa(); return p ? l32(p, 0xdc) : 0; }
int menu_dificultad(void) { const uint8_t *p = activa(); return p ? l32(p, 0xe0) : 0; }
int menu_foto(void) { const uint8_t *p = activa(); return p ? l32(p, 0x20) : 0; }

static void sale(int destino, int ok) { fase = F_SALE; t = 0; sig = destino; sale_ok = ok; sfx_menu(ok ? SFX_ACEPTA : SFX_CANCELA); }
static void bloqueado(void) { t_bloq = 14; sfx_menu(SFX_BLOQUEADO); }
/* GUARDAR con Y en el menu principal y en el de un jugador (DESVIACION pedida por el usuario, revision 42: en el
   original Y es FRIENDS, juego en linea). Primero se dibuja "Guardando datos." (TEXT.PS2 0x206, su principio) y en el
   paso siguiente se guarda (vmu_guarda bloquea: en una VMU real unos segundos); despues "Exito al guardar." (0x202) con
   el sonido de aceptar o "Error al guardar!" (0x31D) con el de bloqueado, G_T_MSG pasos */
#define G_T_MSG 150
/* revision 44: la escritura va en un hilo (g_hilo) y el menu sigue dibujandose con "Guardando datos." mientras tanto
   (en una VMU real son unos segundos: cada bloque releido y comparado); la entrada del menu esta parada (G_PIDE).
   Con el error se ensena tambien el codigo de vmu_diag (vmuesc_codigo) en una segunda linea, y sin sitio el principio
   de TEXT.PS2 0x30D ("No hay espacio suficiente para guardar.") en vez de 0x31D */
static kthread_t *g_hilo;
static volatile int g_hilo_fin, g_hilo_r;
static uint8_t *g_out; static int g_n;
static void *g_hilo_fn(void *a) { (void)a; g_hilo_r = vmu_escribe_pausado(g_out, g_n); g_out = NULL; g_hilo_fin = 1; return NULL; }
static void g_paso(void)
{
    if (!g_est) return;
    g_t++;
    if (g_est == G_PIDE && g_t >= 3 && !g_hilo) {
        int r;
        g_out = vmu_prepara(&g_n); g_hilo_fin = 0;
        if (g_out && (g_hilo = thd_create(0, g_hilo_fn, NULL))) return;
        r = g_out ? vmu_escribe_pausado(g_out, g_n) : VE_MEMORIA;   /* (sin hilo: como antes, bloqueando) */
        if (!g_out) { memset(&vmu_diag, 0, sizeof vmu_diag); vmu_diag.cod = VE_MEMORIA; }
        g_out = NULL; vmu_acaba(r);
        g_est = r ? G_MAL : G_BIEN; g_t = 0;
        if (r) bloqueado(); else sfx_menu(SFX_ACEPTA);
    } else if (g_est == G_PIDE && g_hilo && g_hilo_fin) {
        int r = g_hilo_r;
        thd_join(g_hilo, NULL); g_hilo = NULL;
        vmu_acaba(r);
        g_est = r ? G_MAL : G_BIEN; g_t = 0;
        if (r) bloqueado(); else sfx_menu(SFX_ACEPTA);
    } else if (g_est != G_PIDE && g_t > G_T_MSG + (g_est == G_MAL ? 300 : 0)) g_est = 0;   /* el error, 6 s mas: para leer el codigo */
}
/* L + R + Y: pantalla de diagnostico de la VMU (vmuesc_pantalla, bloquea hasta A / B / START) */
static int lr_pulsados(void)
{
    maple_device_t *d = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = d ? (cont_state_t *)maple_dev_status(d) : NULL;
    return st && st->ltrig >= 128 && st->rtrig >= 128;
}
static void vmu_diagnostico(void)
{
    musica_pausa_lectura(1); vmu_lcd_pausa(1); thd_sleep(100);
    vmuesc_pantalla(vmu_unidad_guardar(), &vmu_diag);
    vmu_lcd_pausa(0); musica_pausa_lectura(0);
}
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
        carrusel(n, pant == P_MENU ? &c_menu : &c_modo, pant == P_MENU ? n_menu : N_OPC, izq, der);
        if (izq || der) break;
        if (ok) {
            int c = pant == P_MENU ? opc_menu(c_menu) : c_modo;   /* (la opcion del original) */
            GANCHO(pant, 1, c);
            if (pant == P_MENU) {                           /* SINGLE PLAYER, (MULTIPLAYER), OUTRUN2SP, RANKINGS, OPTIONS */
                if (c == 0) { sp_sel = 0; sale(P_MODO, 1); } else if (c == 1) sale(P_MULTI, 1); else if (c == 3) sale(P_RANKING, 1);
                else if (c == 4) sale(P_OPCIONES, 1);
                else {                                      /* OUTRUN2SP (0x23aba0): el frontal de recreativa (fsp.c) */
                    sp_sel = 1; sfx_menu(SFX_ACEPTA); menu_libera_todo();
                    if (fsp_entra()) { sp_sel = 0; entra(P_MENU, 1); }
                }
            } else if (c == 0) {                            /* COAST 2 COAST: categorias (P_C2C) */
                if (mis_n && !c2_abre()) { modo_sel = 0; sale(P_C2C, 1); } else bloqueado();
            } else if (c == 1) { modo_sel = c; mis_sel = -1; sale(P_COCHE, 1); }   /* OUTRUN */
            else if (c <= 3) {                              /* HEART ATTACK, TIME ATTACK: el tablero 0x24 (0x25dd58: */
                modo_sel = c; mis_sel = -1;                 /* raiz +0x208 bits 5..7 = 0x40 / 0x80, tabla 0x375f58) */
                tb_bits = (tb_bits & ~0xe0u) | (c == 2 ? 0x40u : 0x80u); tb_sig = 0x24;
                if (!tb_abre()) sale(P_TABLERO, 1); else bloqueado();
            }
            else if (!sa_abre()) sale(P_SALON, 1);         /* SALON (showroom, 4D4230 = PS2 0x25b418) */
            else bloqueado();
        } else if (n & CONT_X) a_licencia(pant);            /* LICENSE */
        else if (n & CONT_Y) {                              /* GUARDAR (DESVIACION, rev. 42; el original: FRIENDS, en linea) */
            if (lr_pulsados()) { if (!g_est) vmu_diagnostico(); }   /* L + R + Y: diagnostico de la VMU (rev. 44) */
            else if (!g_est) { g_est = G_PIDE; g_t = 0; }
        }
        else if (atras) sale(pant == P_MENU ? P_TITULO : P_MENU, 0);
        break;
    case P_C2C: c2_control(n); break;
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
        } else if (atras) {                                 /* (C2C: al detalle; INFARTO / CONTRARRELOJ: al tablero) */
            if (!sp_sel && (modo_sel == 2 || modo_sel == 3) && !tb_abre()) sale(P_TABLERO, 0);
            else sale(sp_sel ? P_MENU : modo_sel == 0 && mis_sel >= 0 ? P_C2C : P_MODO, 0);
        }
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
        /* 4BF510 para la musica (401030) y pide el modo 32; su init (el estado 0xC del cargador del menu, PC 4456EA:
           con [7B17EC], la vuelta a un menu ya puesto, 445746..44574C) la vuelve a poner: 401000(0, 0x1E, 1), la de los
           menus desde el principio. pista_sonando seguia en la 30 (la del menu antes del frontal) y pon_pista no hacia
           nada: el menu se quedaba sin musica */
        if (r == FSP_MENU) { fsp_sale(); sp_sel = 0; entra(P_MENU, 1); pista_sonando = -2; musica_menu(); }
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
    if (pant == P_C2C && c2_activo()) spr_paso();         /* (los cursores animados de C2C.DAT) */
    if (pant == P_SALON && sa_activo() && fase != F_SALE) {
        int r = sa_control(n, fase == F_QUIETO && !cargando);
        /* sin musica: el salon la para al salir (0x25bf28) y la pantalla de UN JUGADOR no la vuelve a poner (en el
           frontal solo la ponen el arranque de la raiz, 0x23b5b8, y la vuelta de la seleccion de musica, 0x24efec) */
        if (r == 1) sale(P_MODO, 0);
        else if (r == 2) a_licencia(P_SALON);
    }
    if (pant == P_TABLERO && tb_activo() && fase != F_SALE) {
        int r = tb_control(n, fase == F_QUIETO && !cargando);
        if (r == 1) sale(P_COCHE, 1);
        else if (r == 2) sale(P_MODO, 0);
        else if (r == 3) sale(P_TABLERO, 1);
        else if (r == 4) sale(P_TABLERO, 0);
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
    g_paso();
    if (fase == F_QUIETO && g_est != G_PIDE) control(n);
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
static void pz_rect(float x, float y, float w, float h, uint32_t argb, float z);
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
/* ---------------------------------------------------------------- COAST 2 COAST: categorias, mapa y detalle */
/* Pantallas del original (SLES-53998; PC: outrun2-decomp platform/frontend_categories.cpp 4E8130.., frontend_missions
   .cpp 4E9160..), con los graficos del banco SPRANI 0x44 (C2C.DAT, tools/conv_c2c.py: las 22 escenas que usan, las
   tablas del ELF en el bloque C2C1 y los glifos de la fuente 14 de sus textos) en el reproductor comun/sprani.c.
   Los recursos de pantalla del original (465250 / 465860 / 465970: soltar, poner {token, primero, ultimo, capa, modo 0,
   x - 320, y - 240}, aplicar) que se vuelven a poner en cada cuadro con un cuadro fijo van como inmediatas
   (spr_inmediata); los que se animan (los cursores) como instancias que se crean al cambiar la seleccion.
   Categorias (suceso 0x3a; PS2 0x242340 seleccion, 0x242680 control, 0x242ac0 dibujo; 4E8580 / 4E8780 / 4E8C60):
     7 botones (0..3 Flagman 1..3 y OutRun: grupos de misiones; 4..6 Clarissa, Jennifer y Holly: campeonatos) en
     disponible (capa 2) o bloqueado (capa 1) segun 0x242290 (los grupos 0 y 4 siempre; 1..3 si la nota del grupo
     anterior, 0x2420a0, es 4..6; 5 y 6 la del 4 y el 5); cursor animado 0x4400d0 (30 cuadros por boton, capa 4);
     la foto 0x4400cf (cuadro 0x241f60: 4 x el ultimo grupo de misiones abierto + el boton) o la del campeonato; el
     texto del grupo (fuente 14 en (65, 210), 0xFF3F474A; el de "perfeccionado" con la nota 6), su rango (E..AAA,
     centrado en (520, 363), 0xFF4D5559) y el pie 0x4400ce, o el texto de bloqueado. Flechas: vecinos 0x417a88
     (con sonido 1 si cambia; el cursor se vuelve a poner aunque no cambie). A: abierto -> sonido 0x40 y al mapa del
     grupo (0..3; el 4..6 va a los campeonatos, suceso 0x3c); cerrado -> sonido 3. B: atras.
   Mapa (suceso 0x3b; PS2 0x242ff0 seleccion, 0x2431f0 nodo abierto, 0x243748 control, 0x243b88 dibujo; 4E8F80 /
     4E90E0 / 4E9360 / 4E95A0): fondo y encima del grupo, sus 5 / 9 / 11 / 15 categorias como nodos (abiertos: el 0 y
     los que tienen un vecino con nota, 0x241ed0 != 7 = todas sus carreras jugadas) con su nota, el cursor 0x4400d8
     (0..0x34) en el nodo; abajo las carreras de la categoria (0x1f7fe0: hasta 5) con su tipo (+0x20 de Races.bin) y
     su nota (la 1.a siempre abierta; la i + 1 si la i tiene nota 4..6), el nombre 0x4400d6 (categoria + 0x1e), el
     panel, el pie y el rotulo del tipo de la 1.a carrera en el cuadro de texto (300, 330, 570, 370; lineas de 14).
     A: al detalle (cursor en la carrera 0; 0x4400d4 sobre ella); en el detalle, la carrera (suceso 0xa). B: del
     detalle al mapa; del mapa a las categorias. Flechas: en el mapa, al vecino (0x375950) si esta abierto (sonido 1);
     en el detalle, izquierda / derecha entre las carreras abiertas.
   DESVIACION (docs/PARIDAD.md, COAST 2 COAST): solo se corren las misiones de carrera (tipo 0) cuyo curso cabe en la
   RAM; las demas se ven como en el original pero A en su carrera da el sonido de bloqueado. Los campeonatos (botones
   4..6) tampoco estan: A en ellos, bloqueado. */
void *(*menu_hueco)(uint32_t tam);
#define C2C_RAM (160u << 10)                                /* tablas de C2C.DAT (animacion 43 KB, sprites, C2C1...) */
typedef struct { uint32_t tok, pri, ult; } c2_d_t;
typedef struct {                                            /* bloque C2C1 (tools/conv_c2c.py) */
    char m[4]; uint32_t o_txt;
    c2_d_t sel[7], disp[7], bloq[7], foto_d[7], foto_b[7], pie_d[7], pie_b[7];
    float pos[7][2]; int32_t vec[7][4]; uint32_t tok_foto;
    uint32_t t_normal[7], t_completo[7], t_bloq[7], t_rango[8];
    uint32_t cuenta[4], base[4];
    c2_d_t fondo[4], encima[4], tipo[7], boton[4], pie[4], nodo[4], panel[4], elegida[5], nota[7];
    uint32_t rotulo[7];
    float carrera[5][2], nota_xy[5][2], resalta[4][15][2], nodo_xy[4][15][2];
    int32_t mvec[4][15][4];
    uint32_t tok_cursor, tok_nombre, nombre_base; float nombre_xy[2];
} c2_t;
static spr_banco_t *c2_bk; static const c2_t *C2; static hud_dat_t *c2_h; static int c2_uf = -1;
static int c2_mapa;                                         /* 0 categorias (0x3a), 1 mapa (0x3b) */
static int c2_cat;                                          /* boton de las categorias (PS2 0x375330 = 84B7F0) */
static int c2_nodo;                                         /* nodo del mapa (PS2 0x3753d8 = 84B7F4) */
static int c2_det, c2_cur;                                  /* detalle (+0x4c) y su carrera (+0x50) */
static int c2_h_sel = -1, c2_h_cur = -1, c2_h_eleg = -1;    /* recursos +0x5ec (categorias), +0x194 y +0x234 (mapa) */
static uint8_t c2_vacia[LIC_BYTES]; static int c2_vacia_ok; /* sin licencia: todas sin jugar */

static int c2_activo(void) { return C2 != NULL; }
static int c2_abre(void)
{
    void *ram; uint32_t tam;
    if (C2) return 0;
    if (!(ram = menu_hueco ? menu_hueco(C2C_RAM) : NULL)) { dbglog(DBG_ERROR, "C2C: sin el hueco para C2C.DAT\n"); return -1; }
    spr_suelta_todas();
    if (!(c2_bk = spr_abre_en("C2C.DAT", ram, C2C_RAM))) {
        dbglog(DBG_ERROR, "C2C: no se pudo abrir C2C.DAT\n"); menu_hueco(0); return -1;
    }
    C2 = (const c2_t *)spr_extra(c2_bk, &tam);
    if (!C2 || ((uintptr_t)C2 & 3) || tam < sizeof(c2_t) || memcmp(C2->m, "C2C1", 4) || C2->o_txt >= tam) {
        dbglog(DBG_ERROR, "C2C: C2C.DAT sin tablas\n"); spr_cierra(c2_bk); c2_bk = NULL; C2 = NULL; menu_hueco(0); return -1;
    }
    c2_h = spr_hud(c2_bk); c2_uf = hudd_uso(c2_h, "F14");
    if (c2_uf >= 0 && hudd_sube_uso(c2_h, c2_uf)) dbglog(DBG_ERROR, "C2C: sin VRAM para los glifos\n");
    c2_h_sel = c2_h_cur = c2_h_eleg = -1;
    dbglog(DBG_INFO, "C2C: C2C.DAT (%u bytes de tablas en el hueco), VRAM libre %uK\n", (unsigned)hudd_tablas_ocupa(),
           (unsigned)(pvr_mem_available() >> 10));
    return 0;
}
static void c2_cierra(void)
{
    if (!c2_bk) return;
    spr_suelta_todas(); spr_cierra(c2_bk); c2_bk = NULL; C2 = NULL; c2_h = NULL; c2_uf = -1;
    c2_h_sel = c2_h_cur = c2_h_eleg = -1;
    if (menu_hueco) menu_hueco(0);
}
static const uint8_t *c2_lic(void)
{
    const uint8_t *p = activa();
    if (p) return p;
    if (!c2_vacia_ok) { lic_reinicia(c2_vacia); c2_vacia_ok = 1; }
    return c2_vacia;
}
static const char *c2_txt(uint32_t o) { return o ? (const char *)C2 + o : NULL; }
static void c2_inm(const c2_d_t *d, int capa, float x, float y) { if (d->tok >> 16) spr_inmediata(d->tok, x - 320, y - 240, capa, (int)d->pri); }
static int c2_crea(const c2_d_t *d, int capa, float x, float y)
{
    int h = spr_crea(d->tok, capa, 0, (int)d->pri, (int)d->ult);
    spr_traslada(h, x - 320, y - 240);
    return h;
}
/* 0x242290 (4E8410): boton g abierto */
static int c2_abierto(int g)
{
    int n;
    if (g == 0 || g == 4) return 1;
    if (g < 0 || g > 6 || !prg) return 0;
    n = nota_grupo(c2_lic(), g - 1);                       /* (1..3: el grupo anterior; 5 y 6: el 4 y el 5) */
    return n != 7 && n >= 4;
}
/* 0x241f60 (4E8B20): la foto de las categorias */
static int c2_foto(void)
{
    int r = c2_abierto(3) ? 12 : c2_abierto(2) ? 8 : c2_abierto(1) ? 4 : 0;
    return c2_cat > 3 ? r : r + c2_cat;
}
static int c2_grupo(void) { return c2_cat < 4 ? c2_cat : 0; }
static int c2_categoria(void) { return (int)C2->base[c2_grupo()] + c2_nodo; }
static int c2_carreras(int c) { return prg && c >= 0 && c < 40 ? prg[c] : 0; }   /* 0x1f7fe0 (495930) */
/* 0x2431f0 (4E90E0): nodo abierto: el 0 y los que tienen un vecino con nota (0x241ed0 != 7). El PC (4E90E0) pide
   ademas nota 4..6 en el vecino; manda el de PS2 */
static int c2_nodo_abierto(int k)
{
    int g = c2_grupo(), q, v;
    if (k == 0) return 1;
    if (!prg) return 0;
    for (q = 0; q < 4; q++)
        if ((v = C2->mvec[g][k][q]) >= 0 && nota_cat(c2_lic(), (int)C2->base[g] + v) != 7) return 1;
    return 0;
}
/* 0x242340 (4E8580): el cursor de las categorias */
static void c2_sel_cat(void)
{
    spr_suelta(c2_h_sel); c2_h_sel = c2_crea(&C2->sel[c2_cat], 4, 320, 240);
}
/* 0x242ff0 (4E8F80): el cursor del mapa o del detalle y la carrera elegida */
static void c2_sel_mapa(void)
{
    const c2_d_t cur = { C2->tok_cursor, 0, 0x34 };
    spr_suelta(c2_h_eleg); c2_h_eleg = -1;
    spr_suelta(c2_h_cur);
    if (!c2_det) {
        const float *p = C2->resalta[c2_grupo()][c2_nodo];
        c2_h_cur = c2_crea(&cur, 4, p[0], p[1]);
    } else {
        c2_h_cur = spr_crea(cur.tok, 5, 0, 0, 0x34);
        spr_traslada(c2_h_cur, C2->carrera[c2_cur][0] - 320, C2->carrera[c2_cur][1] - 240);
        c2_h_eleg = c2_crea(&C2->elegida[c2_cur], 4, 320, 240);
    }
}
static void c2_suelta_cursores(void)
{
    spr_suelta(c2_h_sel); spr_suelta(c2_h_cur); spr_suelta(c2_h_eleg); c2_h_sel = c2_h_cur = c2_h_eleg = -1;
}
static void c2_a_categorias(void)                           /* 0x242648: init de las categorias (nodo 0) */
{
    c2_suelta_cursores(); c2_mapa = 0; c2_nodo = 0; c2_det = 0; c2_cur = 0; c2_sel_cat();
}
static void c2_a_mapa(void)                                 /* 0x243698: init del mapa (sin detalle) */
{
    c2_suelta_cursores(); c2_mapa = 1; c2_det = 0; c2_cur = 0; c2_sel_mapa();
}
/* al entrar en P_C2C: desde ELEGIR MODO, las categorias; de vuelta de la seleccion de coche, el detalle donde estaba */
static void c2_entra(void)
{
    if (entra_atras && c2_mapa && mis_sel >= 0) { c2_suelta_cursores(); c2_det = 1; c2_sel_mapa(); }
    else c2_a_categorias();
}

/* texto con la fuente 14 (glifos de C2C.DAT; metricas e interletraje de MNU2): 42CDD0 con las banderas (1 sin
   alinear, 4 centrado); '\n' vuelve a x y baja alto + espaciado y (0 en la fuente 14) como 42C610 */
static void c2_texto(const char *s, int n, float x, float y, int flags, float z, uint32_t col)
{
    int i; float x0;
    if (!s || !f_met || c2_uf < 0) return;
    if (!(flags & 1)) x -= (flags & 4) ? f_ancho((const uint8_t *)s, n) / 2 : f_ancho((const uint8_t *)s, n);
    x0 = x;
    hudd_color(col);
    for (i = 0; i < n; i++) {
        int c = (uint8_t)s[i], sig = i + 1 < n ? (uint8_t)s[i + 1] : 0;
        if (c == 10) { x = x0; y += f_h; continue; }
        if (c == 32) { x += (int)(f_w * 0.3f); continue; }
        if (c - f_pri < 0 || c - f_pri >= f_n) continue;
        hudd_dibuja(c2_h, c2_uf, c - f_pri, x, y, 1, z, 0); z = hudd_z();
        x = f_par(c, sig == 32 ? 0 : sig) + x + f_esp;
    }
    hudd_color(0xFFFFFF);
}
/* cuadro de texto con ajuste de linea (48EB60 del PC, el de PS2 con +0x1c en sus campos): palabras hasta un espacio,
   '-' o '\n'; la que no cabe en el ancho pasa a la linea siguiente (sola y demasiado larga: se corta); lineas de
   `alto` hacia abajo desde (x, y), sin alinear */
static void c2_parrafo(const char *s, float x, float y, int ancho, int alto, float z, uint32_t col)
{
    char lin[16][128], pal[128]; int nl = 0, hecho = 0, usado = 0, np = 0, i, n = strlen(s), k;
    memset(lin, 0, sizeof lin);
#define ESCRIBE(o, d, m) do { if (nl < 16 && (o) >= 0 && (o) + (m) <= 128) memcpy(lin[nl] + (o), d, m); } while (0)
#define FIN() do { ESCRIBE(hecho, "", 1); nl++; hecho = usado = 0; } while (0)
    for (i = 0; i <= n && nl < 16; i++) {
        char c = i < n ? s[i] : 0;
        if (c != ' ' && c != '-' && c != '\n' && c && hecho + np != 127) {
            if (np < 127) pal[np++] = c;
            if (usado + f_ancho((const uint8_t *)pal, np) <= ancho) continue;
            if (hecho == 0) { np--; ESCRIBE(0, pal, np); hecho = np; FIN(); np = 0; i--; }
            else FIN();
        } else {
            ESCRIBE(hecho, pal, np); hecho += np;
            if (c == '\n' || !c || hecho == 127) { FIN(); np = 0; }
            else { pal[np++] = c; usado += f_ancho((const uint8_t *)pal, np); ESCRIBE(hecho, &c, 1); hecho++; np = 0; }
        }
    }
#undef FIN
#undef ESCRIBE
    for (k = 0; k < nl && k < 16; k++) {
        int m = strnlen(lin[k], 128);
        if (m) { c2_texto(lin[k], m, x, y + k * alto, 1, z, col); z = hudd_z(); }
    }
}

static void c2_control(unsigned n)
{
    int ok = n & (CONT_A | CONT_START), atras = n & CONT_B, act = -1;
    if (!C2) return;
    if (n & CONT_DPAD_UP) act = 2; else if (n & CONT_DPAD_LEFT) act = 3;      /* 48F5F0: 2 arriba, 3 izquierda, */
    else if (n & CONT_DPAD_DOWN) act = 4; else if (n & CONT_DPAD_RIGHT) act = 5;   /* 4 abajo, 5 derecha */
    if (!c2_mapa) {                                         /* 0x242680 */
        if (ok) {
            if (!c2_abierto(c2_cat)) { sfx_menu(SFX_BLOQUEADO); return; }
            if (c2_cat >= 4) { bloqueado(); return; }       /* campeonatos (suceso 0x3c): no estan */
            sfx_menu(SFX_ACEPTA); c2_a_mapa();
        } else if (atras) { c2_suelta_cursores(); sale(P_MODO, 0); }
        else if (act >= 2) {
            static const int COL[4] = { 0, 3, 1, 2 };       /* tabla de saltos 0x417720 */
            int v = C2->vec[c2_cat][COL[act - 2]];
            if (v >= 0) { c2_cat = v; sfx_menu(SFX_CURSOR); }
            c2_sel_cat();
        }
        return;
    }
    {   /* 0x243748 */
        int g = c2_grupo(), c = c2_categoria();
        const uint8_t *p = c2_lic();
        if (ok) {
            if (!c2_det) { c2_det = 1; c2_cur = 0; c2_sel_mapa(); return; }
            {   int m = mis_busca(c + 1, c2_cur + 1);       /* root+0x208: categoria << 12, carrera << 18; suceso 0xa */
                if (m < 0 || !mis_jug[m]) { bloqueado(); return; }   /* (DESVIACION: solo las de carrera que caben) */
                modo_sel = 0; mis_sel = mis_idx[m];
                dbglog(DBG_INFO, "C2C: mision %d-%d (Races.bin %d)\n", c + 1, c2_cur + 1, mis_sel);
                GANCHO(P_C2C, 1, mis_sel); sale(P_COCHE, 1);
            }
            return;
        }
        if (atras) {
            if (c2_det) { c2_det = 0; c2_cur = 0; c2_sel_mapa(); }
            else { c2_a_categorias(); sfx_menu(SFX_CANCELA); }
            return;
        }
        if (act < 2) return;
        if (c2_det) {
            if (act == 3) { if (!c2_cur) return; c2_cur--; }
            else if (act == 5) {
                int nc = c2_carreras(c), gr = p[0x1dd + 5 * c + c2_cur];
                if (c2_cur >= nc - 1 || gr == 7 || gr < 4) return;
                c2_cur++;
            } else return;
        } else {
            static const int COL[4] = { 0, 3, 1, 2 };       /* tabla de saltos 0x417b70 */
            int v = C2->mvec[g][c2_nodo][COL[act - 2]];
            if (v < 0 || !c2_nodo_abierto(v)) return;
            c2_nodo = v;
        }
        c2_sel_mapa(); sfx_menu(SFX_CURSOR);
    }
}

static void c2_dibuja(void)
{
    const uint8_t *p = c2_lic(); float z;
    int i;
    if (!C2) return;
    if (!c2_mapa) {                                         /* 0x242ac0 */
        int ab = c2_abierto(c2_cat);
        for (i = 0; i < 7; i++) {
            if (c2_abierto(i)) c2_inm(&C2->disp[i], 2, C2->pos[i][0], C2->pos[i][1]);
            else c2_inm(&C2->bloq[i], 1, C2->pos[i][0], C2->pos[i][1]);
        }
        if (c2_cat < 4) { c2_d_t d = { C2->tok_foto, (uint32_t)c2_foto(), 0 }; c2_inm(&d, 1, 320, 240); }
        else c2_inm(ab ? &C2->foto_d[c2_cat] : &C2->foto_b[c2_cat], 1, 320, 240);
        c2_inm(ab ? &C2->pie_d[c2_cat] : &C2->pie_b[c2_cat], 1, 320, 240);
        spr_dibuja(0, SPR_CAPAS, 3, 0); z = hudd_z();
        if (ab) {
            int gr = prg ? nota_grupo(p, c2_cat) : 7; const char *s = c2_txt(gr == 6 ? C2->t_completo[c2_cat] : C2->t_normal[c2_cat]);
            if (s) { c2_texto(s, strlen(s), 65, 210, 1, z, 0x3F474A); z = hudd_z(); }
            if ((s = c2_txt(C2->t_rango[gr & 7]))) c2_texto(s, strlen(s), 520, 363, 4, z, 0x4D5559);
        } else {
            const char *s = c2_txt(C2->t_bloq[c2_cat]);
            if (s) c2_texto(s, strlen(s), 65, 210, 1, z, 0x3F474A);
        }
        return;
    }
    {   /* 0x243b88 */
        int g = c2_grupo(), c = c2_categoria(), nc = c2_carreras(c), t1 = 0, m;
        c2_inm(&C2->fondo[g], 1, 320, 240); c2_inm(&C2->encima[g], 3, 320, 240);
        for (i = 0; i < (int)C2->cuenta[g] && i < 15; i++) {
            const float *q = C2->nodo_xy[g][i]; int gr;
            if (!c2_nodo_abierto(i)) continue;
            c2_inm(&C2->nodo[g], 2, q[0], q[1]);
            if ((gr = nota_cat(p, (int)C2->base[g] + i)) != 7) c2_inm(&C2->nota[gr], 3, q[0], q[1]);
        }
        for (i = 0; i < nc && i < 5; i++) {
            int gr = p[0x1dd + 5 * c + i], ant = i ? p[0x1dc + 5 * c + i] : 7, t;
            const float *q = C2->carrera[i];
            if (i && (ant == 7 || ant < 4)) { c2_d_t d = { C2->boton[g].tok, 5, 5 }; c2_inm(&d, 2, q[0], q[1]); continue; }
            c2_inm(&C2->boton[g], 2, q[0], q[1]);
            m = mis_busca(c + 1, i + 1); t = m >= 0 && mis_tipo[m] < 7 ? mis_tipo[m] : 0;
            c2_inm(&C2->tipo[t], 3, q[0], q[1]);
            if (gr < 7) c2_inm(&C2->nota[gr], 4, C2->nota_xy[i][0], C2->nota_xy[i][1]);
        }
        { c2_d_t d = { C2->tok_nombre, C2->nombre_base + c, 0 }; c2_inm(&d, 2, C2->nombre_xy[0] + 320, C2->nombre_xy[1] + 240); }
        c2_inm(&C2->panel[g], 2, 320, 240); c2_inm(&C2->pie[g], 1, 320, 240);
        spr_dibuja(0, SPR_CAPAS, 3, 0); z = hudd_z();
        m = mis_busca(c + 1, 1); if (m >= 0 && mis_tipo[m] < 7) t1 = mis_tipo[m];
        {   const char *s = c2_txt(C2->rotulo[t1]);      /* 0x26cd48 / 0x26bfc8: cuadro de texto, fuente 14, capa 5 */
            if (s) c2_parrafo(s, 300, 330, 570 - 300, 14, z, 0x3F474A); }
    }
}

/* ---------------------------------------------------------------- SALON (showroom: la tienda de las millas) */
/* El SALON del original (SLES-53998 0x25b338..0x25db20, vtable 0x41adec = 4D4230.. del PC, outrun2-decomp
   platform/frontend_showroom.cpp) con los graficos del banco SPRANI 0x44 (SALON.DAT, tools/conv_salon.py: sus 18
   escenas, las tablas del ELF en el bloque SAL1, los glifos de la fuente 14 y su barra de ayuda) en el reproductor
   comun/sprani.c, por tramos (spr_por_tramos: de las tiras de imagenes solo las texturas del objeto que se ve).
   Se gastan las millas OutRun de la licencia (+0x24, float; las suma 4EF510 al acabar una carrera) en desbloquear
   objetos: el bit i de la licencia +0x28 (4474E0 = PS2 0x1a1cc8), los mismos que leen la seleccion de coche (0x3759E0)
   y la de musica (0x4191E8: 172 + tema - 7).
   Estados (+0x40c del objeto; control 0x25b5d0, tabla de saltos 0x41aca0) como el PC (4D5880):
     0 / 1 / 2 pagina principal (0x25bce8 / 0x25bde8 / 0x25bf28): lista de 5 filas (0x28..0x2c: COCHES, ESCENARIOS,
       EXTRAS, MUSICA, COLORES; tabla 0x41ad10 -> estados 3, 6, 15, 9, 12) y la tira 0x44007d; B sale del salon.
     3..5 coches (0x25c038 / 0x25c2d0 / 0x25c490): 28 registros de 0x41bda0 {modelo, bit}, la ficha (0x25bf68: indice
       en 0x375968) y su color (0x18a110: 0x3d4b80 por modelo); el coche 3D del registro (28e348 / 28e788).
     6..8 escenarios (objetos 0x8a + fila), 9..11 musica (0xac + fila; suena el avance de la pista 0x375dcc[i] + 0x21),
     12..14 colores (0x1c + fila; el coche del registro 0x41ba30[fila] con su color), 15..17 extras (0xc1 + fila).
     21 / 22 / 23 ventana de compra (0x25d560 / 0x25d878 / 0x25da80): "COMPRAR OBJETO" (0x3a4) en (330, 180, 239,
       130); icono 0x44005c (filas 252..256: SI, NO, sin millas, no disponible) en la capa 0xd. Con las millas: A con SI
       compra (0x1a1ba0: bit puesto y millas - precio; +3F4 |= 2) y al cerrar se guarda (0x105f50: aqui la VMU); los
       objetos de 0x41ad38 no se venden en esta version (icono 6, sonido 3).
   Pagina (lista 0x24d430: x -180, y -50, 19 por fila, capas 1 / 2, 7 visibles; tira 0x238798 = 446710 del PC:
   objeto i en el cuadro paso x i + base, entra de paso x i + 1 y sale hasta paso x (i + 1)); precio "%dM" del
   objeto (0x1f4198: 0x629ee0, 10 x i y PRICES de OutrunMiles.bin) en (480, 368) si no es suyo; millas "%dM" en
   (480, 87); icono de comprado 0x440051 (0x25d1a8 / 0x25d2f8). Sin el coche: menu_coche3d.
   Ventana de 0x271ca8 con las imagenes 0x3004x de spr_etc_xst; X abre la licencia (0x23c150) y se vuelve al salon;
   la musica se para al salir (0x25bf28) y no vuelve; palanca analogica con repeticion (0x284e20).
   BLOQUEADO (docs/PARIDAD.md, SALON): la camara del preset 1 de 482E70 (sin la colocacion del coche del frontal: la de
   la seleccion de coche), el aspecto del coche bloqueado (bit 0x40 de 19cfd8), los tableros de cursos que leen los
   bits de ETAPAS y METAS. */
#define SALON_RAM (384u << 10)                              /* tablas de SALON.DAT (animacion 257 KB, sprites, SAL1) */
typedef struct { uint32_t tok, pri, ult; } sa_d_t;
typedef struct {                                            /* bloque SAL1 (tools/conv_salon.py) */
    char m[4]; uint32_t o_txt;
    sa_d_t fila[257];
    uint32_t precio[0xcf];
    int32_t fuera[26];
    int32_t c_modelo[30], c_bit[30], c_ficha[30]; int8_t c_color[30]; uint8_t _r[2];
    int32_t k_modelo[30], k_ficha[30];
    struct { uint8_t reg; int8_t color; } k_fila[110];
    uint32_t pista[21];
    uint32_t t_ventana, t_formato;
    uint16_t vent_wh[16][2];                                /* imagenes 0x30040..0x3004f de la ventana */
} sa_t;
/* paginas (estados 3k .. 3k + 2): base de los objetos (-1: el bit del registro), filas de la lista, tira (escena,
   ultimo indice, paso, base), titulo (entra, quieto, sale), vuelta tras la ventana */
typedef struct { int base, f0, f1; uint32_t tira; int ult, paso, b0, tit, quieto, sale, vuelta; } sa_pag_t;
static const sa_pag_t SA_PAG[6] = {
    { 0xcf, 0x28, 0x2d, 0x44007d, 4, 0x14, 0xa, -1, -1, -1, -1 },                /* principal (0x25bce8) */
    { -1, 0x2d, 0x49, 0x44007e, 0x1b, 0xa, 9, 0x13, 0x14, 0x15, 4 },             /* coches (0x25c038) */
    { 0x8a, 0xb7, 0xd9, 0x44007f, 0x21, 0xa, 9, 0x16, 0x17, 0x18, 7 },           /* escenarios (0x25c4e0) */
    { 0xac, 0xd9, 0xee, 0x4400a9, 0x14, 0xa, 9, 0x1c, 0x1d, 0x1e, 10 },          /* musica (0x25c750) */
    { 0x1c, 0x49, 0xb7, 0x440080, 0x6d, 0xa, 9, 0x1f, 0x20, 0x21, 13 },          /* colores (0x25ca80) */
    { 0xc1, 0xee, 0xfc, 0x4400c5, 0xd, 0xa, 9, 0x19, 0x1a, 0x1b, 16 } };         /* extras (0x25cf00) */
static spr_banco_t *sa_bk; static const sa_t *SA; static hud_dat_t *sa_h; static int sa_uf = -1, sa_ub = -1, sa_uv = -1;
/* recurso de pantalla (465250 / 465860 / 465970 / 4659F0 / 4652E0 del PC; 0x229708 / 0x228e60 / 0x229080 /
   0x229120 / 0x2297d0 del PS2): una instancia del reproductor; modo 3 = 1 y se suelta al acabar */
typedef struct { int h, solo; uint32_t tok; int pri, ult, capa, modo; } sa_r_t;
static sa_r_t sa_fondo, sa_tit, sa_tit2, sa_icono, sa_vent, sa_tira;   /* +0x2084, +0x938, +0x9d8, +0xbe0, +0x1fe0, +0xa78 */
static int sa_est;                                          /* +0x40c */
static int sa_base = 0xcf;                                  /* +0xc84 */
static int sa_comprado, sa_refresca;                        /* +0xbdc (0 no, 1 si), +0xc80 */
static int sa_obj, sa_vuelta, sa_puede, sa_eleccion;        /* +0xc88, +0xc8c, +0x2a74, +0x2080 */
static int sa_mus;                                          /* +0xb45 */
/* lista (0x24d430 = 48D970): filas de la tabla, la elegida, la primera que se ve y cuantas */
static int sa_lf0, sa_ln, sa_lsel, sa_lini, sa_llim, sa_lscroll;
/* tira (0x238798 = 446710): activa, escena, ultimo indice, paso, base, el objeto */
static int sa_ton, sa_tult, sa_tpaso, sa_tb0, sa_ti; static uint32_t sa_ttok;
/* registros de las paginas de coches y colores (+0x4c: 30 x 0x20) */
/* (sa_reg: los registros de las paginas de coches y colores, +0x4c: 30 x 0x20; sa_car_*: el coche 3D, 28e348 /
   28e788: registro y color) */

static int sa_activo(void) { return SA != NULL; }
static void sr_suelta(sa_r_t *r) { if (r->h >= 0) spr_suelta(r->h); r->h = -1; r->solo = 0; }
static void sr_pon(sa_r_t *r, uint32_t tok, int pri, int ult, int capa, int modo, float vel)
{
    /* el mismo cuadro quieto que ya esta (el original lo suelta y lo vuelve a poner: lo mismo en pantalla) */
    if (r->h >= 0 && modo == 0 && pri == ult && r->modo == 0 && r->tok == tok && r->pri == pri && r->ult == ult && r->capa == capa) return;
    sr_suelta(r);
    r->h = spr_crea(tok, capa, modo == 3 ? 1 : modo, pri, ult); r->solo = modo == 3;
    r->tok = tok; r->pri = pri; r->ult = ult; r->capa = capa; r->modo = modo;
    if (r->h >= 0 && vel != 1.0f) spr_velocidad(r->h, vel);
}
static void sr_fila(sa_r_t *r, int f, int capa, int modo, float vel)
{
    const sa_d_t *d = &SA->fila[f]; sr_pon(r, d->tok, (int)d->pri, (int)d->ult, capa, modo, vel);
}
static int sr_listo(const sa_r_t *r) { return r->h < 0 || spr_estado(r->h) != 1; }   /* 4652E0 */
static void sr_tick(sa_r_t *r) { if (r->h >= 0 && r->solo && sr_listo(r)) sr_suelta(r); }   /* 4659F0 */

static int sa_abre(void)
{
    void *ram; uint32_t tam;
    if (SA) return 0;
    if (!(ram = menu_hueco ? menu_hueco(SALON_RAM) : NULL)) { dbglog(DBG_ERROR, "SALON: sin el hueco para SALON.DAT\n"); return -1; }
    spr_suelta_todas();
    if (!(sa_bk = spr_abre_en("SALON.DAT", ram, SALON_RAM))) {
        dbglog(DBG_ERROR, "SALON: no se pudo abrir SALON.DAT\n"); menu_hueco(0); return -1;
    }
    SA = (const sa_t *)spr_extra(sa_bk, &tam);
    if (!SA || ((uintptr_t)SA & 3) || tam < sizeof(sa_t) || memcmp(SA->m, "SAL1", 4) || SA->o_txt >= tam) {
        dbglog(DBG_ERROR, "SALON: SALON.DAT sin tablas\n"); spr_cierra(sa_bk); sa_bk = NULL; SA = NULL; menu_hueco(0); return -1;
    }
    spr_por_tramos(sa_bk, 1);
    sa_h = spr_hud(sa_bk); sa_uf = hudd_uso(sa_h, "F14"); sa_ub = hudd_uso(sa_h, "BARRA"); sa_uv = hudd_uso(sa_h, "VENT");
    if (sa_uv >= 0 && hudd_sube_uso(sa_h, sa_uv)) dbglog(DBG_ERROR, "SALON: sin VRAM para la ventana\n");
    if (sa_uf >= 0 && hudd_sube_uso(sa_h, sa_uf)) dbglog(DBG_ERROR, "SALON: sin VRAM para los glifos\n");
    if (sa_ub >= 0 && hudd_sube_uso(sa_h, sa_ub)) dbglog(DBG_ERROR, "SALON: sin VRAM para la barra\n");
    sa_fondo.h = sa_tit.h = sa_tit2.h = sa_icono.h = sa_vent.h = sa_tira.h = -1;
    dbglog(DBG_INFO, "SALON: SALON.DAT (%u bytes de tablas en el hueco), VRAM libre %uK\n", (unsigned)hudd_tablas_ocupa(),
           (unsigned)(pvr_mem_available() >> 10));
    return 0;
}
static void sa_cierra(void)
{
    if (!sa_bk) return;
    spr_suelta_todas(); spr_cierra(sa_bk); sa_bk = NULL; SA = NULL; sa_h = NULL; sa_uf = sa_ub = sa_uv = -1;
    sa_fondo.h = sa_tit.h = sa_tit2.h = sa_icono.h = sa_vent.h = sa_tira.h = -1;
    sa_car_on = 0; sa_ln = 0; sa_ton = 0;
    if (menu_hueco) menu_hueco(0);
}
static float sa_millas(void) { const uint8_t *p = c2_lic(); float m; memcpy(&m, p + 0x24, 4); return m; }
static int sa_ftoi(float f) { return f > -2147483648.0f && f < 2147483648.0f ? (int)f : (int)0x80000000; }   /* cvt.w.s */
static int sa_tiene(int b) { const uint8_t *p = c2_lic(); return b >= 0 && b < 0x96 * 8 && ((p[0x28 + b / 8] >> (b % 8)) & 1); }   /* 0x1a1cc8 */
static uint32_t sa_precio(int i) { return i >= 0 && i < 0xcf ? SA->precio[i] : 0; }   /* 0x1f4198 */
static int sa_vende(int i) { int k; for (k = 0; k < 26; k++) if (SA->fuera[k] == i) return 0; return 1; }   /* 0x25d790 */
/* 0x1a1ba0 (447460): sin el bit y con las millas (truncadas sin signo, 0x131ca8) >= precio: pone el bit y resta */
static int sa_compra(int b, uint32_t precio)
{
    uint8_t *p = activa(); float m;
    if (!p || sa_tiene(b)) return 0;
    memcpy(&m, p + 0x24, 4);
    if ((m >= 1.0f ? (uint32_t)m : 0u) < precio) return 0;
    p[0x28 + b / 8] |= (uint8_t)(1 << (b % 8));
    m -= (int32_t)precio >= 0 ? (float)(int32_t)precio : (float)((precio >> 1) | (precio & 1)) * 2.0f;
    memcpy(p + 0x24, &m, 4);
    return 1;
}
/* 0x25d170 (4D4200): el objeto de la fila elegida */
static int sa_objeto(void)
{
    if (sa_base == 0xcf) return 0;
    if (sa_base >= 0) return sa_lsel + sa_base;
    return sa_lsel >= 0 && sa_lsel < 30 ? sa_reg[sa_lsel].bit : 0;
}

/* ---- lista (0x24d430 init, 0x24da30 dibujo, 0x24d820 / 0x24d898 anterior / siguiente, 0x24df28 vaciar) */
static void sa_lista(int f0, int f1, int lim, int scroll) { sa_lf0 = f0; sa_ln = f1 - f0; sa_lsel = 0; sa_lini = 0; sa_llim = lim; sa_lscroll = scroll; }
static void sa_lista_mueve(int adelante)                    /* (con la vuelta; sin sonido: argumento 0) */
{
    if (sa_ln <= 0) return;
    sa_lsel += adelante ? 1 : -1;
    if (sa_lsel < 0) sa_lsel = sa_ln - 1; else if (sa_lsel >= sa_ln) sa_lsel = 0;
}
static void sa_lista_paso(void)                             /* 48DDA0: la primera que se ve */
{
    if (sa_lscroll) {
        if (sa_lini + sa_llim <= sa_lsel) { if (sa_lsel < sa_ln) sa_lini++; }
        else if (sa_lsel < sa_lini) sa_lini--;
        else if (sa_lini < 0) sa_lini = 0;
    } else { sa_llim = 96; sa_lini = 0; }
}
static void sa_lista_dibuja(void)                           /* filas en (-180, -50 + 19 k): la fila (capa 2) y su barra (1) */
{
    int i, k = 0;
    for (i = 0; i < sa_ln; i++) {
        const sa_d_t *d, *b;
        if (i < sa_lini || i >= sa_lini + sa_llim) continue;
        d = &SA->fila[sa_lf0 + i]; b = &SA->fila[i == sa_lsel ? 2 : 1];
        spr_inmediata(d->tok, -180, -50 + 19.0f * k, 2, (int)d->pri);
        spr_inmediata(b->tok, -180, -50 + 19.0f * k, 1, (int)b->pri);
        k++;
    }
}
/* ---- tira de imagenes (446710 / 4467A0 / 4468A0 / 446930 / 446830) */
static int sa_tcuadro(int i) { return sa_tpaso * i + sa_tb0; }
static void sa_tira_pon(int pri, int ult, int modo) { sr_pon(&sa_tira, sa_ttok, pri, ult, 4, modo, 1); }
static void sa_tira_inicia(uint32_t tok, int ult, int paso, int b0)
{
    sa_ttok = tok; sa_tult = ult; sa_tpaso = paso; sa_tb0 = b0; sa_ti = 0; sa_ton = 1;
    sa_tira_pon(1, b0, 1);
}
static void sa_tira_deja(void) { sa_tira_pon(sa_tcuadro(sa_ti), sa_tpaso * (sa_ti + 1), 3); }
static void sa_tira_paso(void)
{
    if (!sa_ton) return;
    sr_tick(&sa_tira);
    if (!sr_listo(&sa_tira)) return;
    if (sa_tira.h >= 0) { int f = sa_tcuadro(sa_ti); sa_tira_pon(f, f, 0); }
    else { int k = sa_tpaso * sa_ti; sa_tira_pon(k + 1, sa_tb0 + k, 1); }
}
static void sa_tira_suelta(void) { if (!sa_ton) return; sa_tira_deja(); sr_suelta(&sa_tira); sa_ton = 0; }
static void sa_tira_sig(void)
{
    sa_tira_deja();
    if (sa_ti < sa_tult) { sa_ti++; sfx_menu(SFX_CURSOR); } else { sa_ti = 0; sfx_menu(SFX_CURSOR); }
}
static void sa_tira_ant(void)
{
    sa_tira_deja();
    if (sa_ti > 0) { sa_ti--; sfx_menu(SFX_CURSOR); } else { sa_ti = sa_tult; sfx_menu(SFX_CURSOR); }
}
/* ---- titulos (0x25ba48 / 0x25bb50 / 0x25bbe0) */
static void sa_tit_entra(int f) { sr_fila(&sa_tit, 18, 0, 1, -1); sr_fila(&sa_tit2, f, 0, 1, 1); }
static void sa_tit_quieto(int f) { sr_fila(&sa_tit2, f, 0, 0, 1); }
static void sa_tit_sale(int f) { sr_fila(&sa_tit2, f, 0, 1, -1); sr_fila(&sa_tit, 16, 0, 1, 1); }
/* ---- icono de comprado (0x25d1a8 / 0x25d2f8) */
static void sa_icono_pon(int f) { if (sa_base != 0xcf) sr_fila(&sa_icono, f, 0, 0, 1); else sr_suelta(&sa_icono); }
static void sa_icono_muestra(void)
{
    sr_suelta(&sa_icono);
    sa_comprado = sa_tiene(sa_objeto());
    sa_icono_pon(sa_comprado ? 10 : 7);
}
static void sa_icono_paso(void)
{
    int f;
    sr_tick(&sa_icono);
    if (!sr_listo(&sa_icono) && !sa_refresca) return;
    if (sa_refresca) { sa_comprado = sa_tiene(sa_objeto()); f = sa_comprado ? 0xb : 8; }
    else f = sa_comprado ? 0xb : 8;
    sa_icono_pon(f);
    sa_refresca = 0;
}
/* ---- registros y coche 3D */
static void sa_registros(int colores)
{
    int i, k;
    for (i = 0; i < 30; i++) {
        sa_reg_t *r = sa_reg + i;
        r->modelo = colores ? SA->k_modelo[i] : SA->c_modelo[i];
        r->ficha = colores ? SA->k_ficha[i] : SA->c_ficha[i];
        r->bit = colores ? 0 : SA->c_bit[i];
        r->desbl = c_desbloqueado(r->ficha);                /* 0x244fe0 = 4C8F90 */
        for (k = 0; k < 8; k++) r->col[k] = c_color_ok(r->ficha, k) ? k + 1 : -1;   /* 0x245038 = 4C8FC0 */
        if (colores) { r->color = 1; for (k = 0; k < 8; k++) if (r->col[k] != -1) { r->color = r->col[k]; break; } }
        else r->color = SA->c_color[i];
    }
}
static void sa_coche(int reg, int color) { sa_car_on = 1; sa_car_reg = reg; sa_car_col = color; }
/* ---- ventana de compra (0x25d560) */
static void sa_ventana(int obj, int vuelta)
{
    sa_obj = obj; sa_vuelta = vuelta;
    if (sa_tiene(obj)) { sa_est = vuelta; return; }
    sa_car_on = 0;                                          /* 28e750: el coche fuera */
    if (!sa_vende(obj)) { sr_fila(&sa_vent, 256, 0xd, 0, 1); sa_puede = 0; sfx_menu(SFX_BLOQUEADO); }
    else if (sa_ftoi(sa_millas()) < (int)sa_precio(obj)) { sr_fila(&sa_vent, 255, 0xd, 0, 1); sa_puede = 0; sfx_menu(SFX_BLOQUEADO); }
    else { sr_fila(&sa_vent, 252, 0xd, 0, 1); sa_puede = 1; sfx_menu(SFX_ACEPTA); }
    sa_eleccion = 3; sa_est = 0x16;
}
/* ---- paginas */
static void sa_pagina(int p)                                /* init (0x25bce8, 0x25c038, ...) */
{
    const sa_pag_t *g = SA_PAG + p;
    sa_base = g->base;
    if (p == 1 || p == 4) {                                 /* 482E70(1): la camara del salon; registros y coche */
        sa_registros(p == 4);
        if (p == 1) sa_coche(0, sa_reg[0].color);
        else sa_coche(SA->k_fila[0].reg, SA->k_fila[0].color);
        if (p == 4) sa_refresca = 1;
    }
    if (p == 3) { sa_mus = 0; pon_pista(PISTA_AVANCE + (int)SA->pista[0]); }   /* 0x25c9f0: 401000(0, pista + 0x21, 1) */
    if (p == 0) { sr_suelta(&sa_icono); sa_icono_muestra(); }
    sa_lista(g->f0, g->f1, p ? 7 : 0x60, p != 0);
    sa_tira_inicia(g->tira, g->ult, g->paso, g->b0);
    if (g->tit >= 0) sa_tit_entra(g->tit);
    sa_est++;
    if (p) sa_icono_muestra();
}
static void sa_pag_sale(int p)                              /* 0x25c490 / 0x25c710 / ...: a la principal */
{
    const sa_pag_t *g = SA_PAG + p;
    if (p == 1 || p == 4) sa_car_on = 0;                    /* 482E70(0), 48c450 */
    if (p == 3) pon_pista(-1);                              /* 401030 */
    sa_ln = 0; sa_tira_suelta(); sa_tit_sale(g->sale); sa_est = 0;
}
/* 0x284e20 (48F4F0 del PC): la palanca analogica izquierda. Por eje, al pasar de 64 marca su sentido y cuenta 20; si
   sigue pasada, la cuenta baja uno por paso y a -1 vuelve a marcar (repite cada 21 pasos mientras se mantiene); al
   soltarla (|eje| <= 64), -1. La cruceta solo da la pulsacion (0x101bd4: lo nuevo del cuadro), sin repeticion */
static int sa_eje_n[2] = { -1, -1 }; static int sa_eje_m[2][2];
static void sa_ejes(void)
{
    maple_device_t *d = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = d ? (cont_state_t *)maple_dev_status(d) : NULL;
    int v[2], i;
    v[0] = st ? st->joyx : 0; v[1] = st ? st->joyy : 0;
    for (i = 0; i < 2; i++) {
        int a = v[i] < 0 ? -v[i] : v[i];
        sa_eje_m[i][0] = sa_eje_m[i][1] = 0;
        if (sa_eje_n[i] >= 0) sa_eje_n[i] = a <= 64 ? -1 : sa_eje_n[i] - 1;
        else if (a > 64) { sa_eje_m[i][v[i] < 0] = 1; sa_eje_n[i] = 20; }
    }
}
static int sa_accion(unsigned n)                            /* 0x284990 (48F5F0): 0 A, 1 B, 2 arriba, 3 izquierda, */
{                                                           /* 4 abajo, 5 derecha, 9 X */
    if (n & (CONT_A | CONT_START)) return 0;
    if (n & CONT_B) return 1;
    if ((n & CONT_DPAD_UP) || sa_eje_m[1][1]) return 2;
    if ((n & CONT_DPAD_LEFT) || sa_eje_m[0][1]) return 3;
    if ((n & CONT_DPAD_DOWN) || sa_eje_m[1][0]) return 4;
    if ((n & CONT_DPAD_RIGHT) || sa_eje_m[0][0]) return 5;
    if (n & CONT_X) return 9;
    return 12;
}
static void sa_pag_control(int p, int a)                    /* 0x25c2d0 / 0x25c5e8 / 0x25c858 / 0x25cd38 / 0x25d008 */
{
    const sa_pag_t *g = SA_PAG + p;
    sa_lista_paso(); sa_tira_paso();
    if (sr_listo(&sa_tit2)) sa_tit_quieto(g->quieto);
    if (a == 0) { sa_ventana(sa_objeto(), g->vuelta); return; }
    if (a == 1) { sa_est++; sfx_menu(SFX_CANCELA); return; }
    if (a == 2 || a == 4) {
        int ad = a == 4;
        if (p == 3) {
            if (ad) sa_mus = sa_mus < 0x14 ? sa_mus + 1 : 0; else sa_mus = sa_mus ? sa_mus - 1 : 0x14;
            pon_pista(PISTA_AVANCE + (int)SA->pista[sa_mus]);   /* 0x25ca40 */
        }
        sa_lista_mueve(ad); if (ad) sa_tira_sig(); else sa_tira_ant();
        if (p == 1) sa_coche(sa_lsel, sa_reg[sa_lsel].color);
        if (p == 4) sa_coche(SA->k_fila[sa_lsel].reg, SA->k_fila[sa_lsel].color);
        sa_refresca = 1;
    }
    if (p == 1) {                                           /* el registro elegido se desbloqueo (la compra) */
        sa_reg_t *r = sa_reg + sa_lsel;
        if (c_desbloqueado(r->ficha) != r->desbl) { r->desbl = 1; sa_coche(sa_lsel, r->color); }
    }
}
/* control y logica de un paso (0x25b5d0); n: botones nuevos (0 si no se atiende la entrada). 1: salir del salon */
static int sa_control(unsigned n, int atiende)
{
    int a;
    if (!SA) return 1;
    sa_ejes(); a = atiende ? sa_accion(n) : 12;
    /* X: la raiz (0x23c150) abre la licencia (pantalla 0x15) si la de encima lo permite (+0x2a4: el 1 que pone el
       salon con 0x23bb20(raiz, 0, 1) en cada paso); al volver, el salon empieza otra vez (0x25b418). Al irse, su
       suspender (0x25b998) para la musica (0x25bf28) */
    if (a == 9) { pon_pista(-1); return 2; }
    spr_paso();
    sa_icono_paso();
    sr_tick(&sa_fondo);
    if (sr_listo(&sa_fondo)) sr_fila(&sa_fondo, 5, 0, 0, 1);
    if (sr_listo(&sa_tit) && sa_est == 1) sr_fila(&sa_tit, 17, 0, 0, 1);
    switch (sa_est) {
    case 0: sa_pagina(0); break;
    case 1:                                                 /* 0x25bde8 */
        sa_lista_paso(); sa_tira_paso();
        if (a == 0) {
            static const int A_[5] = { 3, 6, 0xf, 9, 0xc };  /* 0x41ad10 */
            if (sa_lsel >= 0 && sa_lsel < 5) sa_est = A_[sa_lsel];
            sfx_menu(SFX_ACEPTA);
            pon_pista(-1); sr_suelta(&sa_icono); sa_ln = 0; sa_tira_suelta();   /* 0x25bf28 */
        } else if (a == 1) { sa_est = 2; sfx_menu(SFX_CANCELA); }
        else if (a == 2 || a == 4) { sa_lista_mueve(a == 4); if (a == 4) sa_tira_sig(); else sa_tira_ant(); }
        break;
    case 2: pon_pista(-1); sr_suelta(&sa_icono); sa_ln = 0; sa_tira_suelta(); return 1;
    case 3: sa_pagina(1); break;   case 4: sa_pag_control(1, a); break;   case 5: sa_pag_sale(1); break;
    case 6: sa_pagina(2); break;   case 7: sa_pag_control(2, a); break;   case 8: sa_pag_sale(2); break;
    case 9: sa_pagina(3); break;   case 10: sa_pag_control(3, a); break;  case 11: sa_pag_sale(3); break;
    case 12: sa_pagina(4); break;  case 13: sa_pag_control(4, a); break;  case 14: sa_pag_sale(4); break;
    case 15: sa_pagina(5); break;  case 16: sa_pag_control(5, a); break;  case 17: sa_pag_sale(5); break;
    case 21: case 22:                                       /* 0x25d878 */
        if (sa_puede) {
            if (a == 0) {
                if (sa_eleccion == 3) {
                    if (sa_compra(sa_obj, sa_precio(sa_obj))) {
                        uint8_t *p = activa(); p[0x3f4] |= 2;
                        dbglog(DBG_INFO, "SALON: comprado el objeto %d por %u millas (quedan %.0f)\n", sa_obj,
                               (unsigned)sa_precio(sa_obj), (double)sa_millas());
                    }
                    sa_refresca = 1; sa_est++;
                } else if (sa_eleccion == 5) sa_est++;
            } else if (a == 1) sa_est++;
            else if (a == 3 && sa_eleccion != 3) { sr_fila(&sa_vent, 252, 0xd, 0, 1); sa_eleccion = 3; }
            else if (a == 5 && sa_eleccion != 5) { sr_fila(&sa_vent, 253, 0xd, 0, 1); sa_eleccion = 5; }
        } else if (a == 0 || a == 1) sa_est++;
        break;
    case 23:                                                /* 0x25da80 */
        sr_suelta(&sa_vent);
        if (sa_eleccion == 3 && activa() && (activa()[0x3f4] & 2)) {
            int r = vmu_guarda();                           /* 0x105f50 */
            dbglog(DBG_INFO, "SALON: guardado en la VMU %s (%d)\n", r ? "mal" : "bien", r);
        }
        if (sa_vuelta >= 4 && sa_vuelta <= 0x13 && (sa_vuelta - 4) % 3 == 0) sa_est = sa_vuelta;
        if (sa_est == 4) sa_coche(sa_lsel, sa_reg[sa_lsel].color);
        if (sa_est == 13) sa_coche(SA->k_fila[sa_lsel].reg, SA->k_fila[sa_lsel].color);
        break;
    }
    return 0;
}
static void sa_entra(void)
{
    sr_suelta(&sa_fondo); sr_suelta(&sa_tit); sr_suelta(&sa_tit2); sr_suelta(&sa_icono); sr_suelta(&sa_vent);
    sa_ton = 0; sa_tira.h = -1; sa_ln = 0; sa_car_on = 0; sa_refresca = 0;
    sa_base = 0xcf; sa_est = 0;
    sr_fila(&sa_tit, 16, 0, 1, 1);                          /* 0x25b418: +0x938 la fila 16 (+0x2084 sin aplicar) */
#if defined(PRUEBA_SALON_MILLAS)
    {   /* prueba (-DPRUEBA_SALON_MILLAS=n): la licencia activa, si no tiene millas, con n al entrar en el salon por
           primera vez (con la VMU de un arranque anterior, las que le quedaron) */
        static int dado; uint8_t *p = activa(); float m;
        if (p && !dado) {
            memcpy(&m, p + 0x24, 4);
            if (m < 1.0f) { m = (float)PRUEBA_SALON_MILLAS; memcpy(p + 0x24, &m, 4); p[0x3f4] |= 2; }
            dado = 1; dbglog(DBG_INFO, "SALON: prueba, la licencia con %.0f millas\n", (double)m);
        }
    }
#endif
    sa_icono_muestra();
}
/* texto con los glifos de la fuente 14 de SALON.DAT (como c2_texto) */
static void sa_texto(const char *s, float x, float y, float z, uint32_t col)
{
    int i, n = strlen(s);
    if (!f_met || sa_uf < 0) return;
    hudd_color(col);
    for (i = 0; i < n; i++) {
        int c = (uint8_t)s[i], sig = i + 1 < n ? (uint8_t)s[i + 1] : 0;
        if (c == 32) { x += (int)(f_w * 0.3f); continue; }
        if (c - f_pri < 0 || c - f_pri >= f_n) continue;
        hudd_dibuja(sa_h, sa_uf, c - f_pri, x, y, 1, z, 0); z = hudd_z();
        x = f_par(c, sig == 32 ? 0 : sig) + x + f_esp;
    }
    hudd_color(0xFFFFFF);
}
/* ventana (0x271ca8 = 48C5F0 del PC; flags 0, sin franja de botones): las imagenes 0x3004x de spr_etc_xst con
   0x117b88 (42D300): modo 0 a su tamano, 1 estirada en x, 2 en y, 3 en las dos (aqui, estiradas: PROBABLE) */
static void sa_img(int k, float x, float y, float w, float h, float z)
{
    hudd_pz_t pz; float m[6], q[4]; int iw, ih;
    if (sa_uv < 0 || hudd_piezas(sa_h, sa_uv, k, &pz, 1) < 1) return;
    iw = SA->vent_wh[k][0]; ih = SA->vent_wh[k][1];
    if (iw <= 0 || ih <= 0) return;
    (void)z;
    q[0] = q[1] = 0; q[2] = iw; q[3] = ih;
    m[0] = w / iw; m[1] = 0; m[2] = 0; m[3] = h / ih; m[4] = x; m[5] = y;
    hudd_sprite(pz.spr, m, q, 1, 0);
}
static void sa_ventana_dibuja(float x, float y, float w, float h, float z)
{
    hudd_capa(sa_h, z, 0);
    sa_img(0xa, x + 3, y + 26, w - 9, h - 42, z);           /* 0x3004a (3) cuerpo */
    sa_img(0x9, x + 3, y + 32, w - 9, 6, z);                /* 0x30049 (1) */
    sa_img(0xf, x + 9, y, w - 115, 26, z);                  /* 0x3004f (1) cabecera */
    sa_img(0x3, x + 9, y + h - 16, w - 20, 16, z);          /* 0x30043 (1) pie */
    sa_img(0xe, x, y, 9, 26, z);                            /* 0x3004e (0) */
    sa_img(0xd, x + w - 106, y, 106, 26, z);                /* 0x3004d (0) */
    sa_img(0x2, x, y + h - 16, 9, 16, z);                   /* 0x30042 (0) */
    sa_img(0x1, x + w - 11, y + h - 16, 11, 16, z);         /* 0x30041 (0) */
    sa_img(0xc, x, y + 26, 3, h - 42, z);                   /* 0x3004c (2) izquierda */
    sa_img(0xb, x + w - 6, y + 26, 6, h - 42, z);           /* 0x3004b (2) derecha */
}
static void sa_dibuja(void)
{
    char b[24]; float z;
    if (!SA) return;
    if (sa_ln > 0) sa_lista_dibuja();
    spr_dibuja(0, 0xc, 3, 0); z = hudd_z();
    snprintf(b, sizeof b, (const char *)SA + SA->t_formato, sa_ftoi(sa_millas()));   /* 0x25b8c0: millas */
    sa_texto(b, 480, 87, z, 0xFFFFFF); z = hudd_z();
    if (sa_comprado != 1 && sa_base != 0xcf) {             /* el precio */
        snprintf(b, sizeof b, (const char *)SA + SA->t_formato, (int)sa_precio(sa_objeto()));
        sa_texto(b, 480, 368, z, 0xFFFFFF);
    }
    if (sa_ub >= 0) hudd_dibuja(sa_h, sa_ub, 0, 0, 0, 1, 7.5f, 0);   /* barra de ayuda */
    if (sa_est == 0x16 || sa_est == 0x15) {                 /* ventana (48C5F0, capa 0xc) y su icono (0xd) */
        sa_ventana_dibuja(330, 180, 239, 130, 8); 
        sa_texto((const char *)SA + SA->t_ventana, 330 + 6, 180 + 5, 8.01f, 0xFFFFFF);
        spr_dibuja(0xc, SPR_CAPAS, hudd_z() + 0.01f, 0);
    } else spr_dibuja(0xc, SPR_CAPAS, 8, 0);
}

/* ---------------------------------------------------------------- TABLEROS DE CURSOS (INFARTO / CONTRARRELOJ)
   Los del original (TABLERO.DAT, tools/conv_tablero.py; SLES-53998, fabricas de la raiz por clave 0x239eb8): UN
   JUGADOR (0x25dd58, tabla 0x375f58) lleva INFARTO / CONTRARRELOJ a la clave 0x24 con la palabra del frontal (raiz
   +0x208: bits 0..1 juego, 5..7 modo 0x40 / 0x80, 11 curso / etapa, 12..17 codigo; +0x20c bit 0 la opcion B):
     0x24 (0x2528e8, control 0x2530a0): el juego de cursos (OutRun 2 / OutRun 2 SP), carrusel 0x419f70 (0x44003b)
          -> INFARTO: el coche (clave 0xa); CONTRARRELOJ: 0x25 con 0x800 y los bits 0..1 = indice (0x2539c4).
     0x25 (0x2558e8, control 0x255e58): CURSO / ETAPA (0x41a280 / 0x41a2c0) -> 0x26 (+0x800) o 0x27 (0x256520).
     0x26 (0x24f340, control 0x24f818): las metas A..E y el curso seguido; R (0x251da0) las del reves. Candados
          0xcb..0xce (curso seguido) y 0xc1 + 2 i / 0xc2 + 2 i (metas del reves): los METAS que vende el SALON.
     0x27 (0x257e58, control 0x258610): las 15 etapas; R (0x25a880) las del reves con los candados 0x8a + 2 i /
          0x8b + 2 i; abajo en la etapa 0 la opcion B (0xa8..0xab): ETAPAS del SALON.
   Sin la lista de records de cada curso (0x267210 / 0x2674d8 / 0x269880: no hay records de Contrarreloj en la DC):
   el foco no baja a la lista (la barra de ayuda es la de sin tarjeta de memoria, 0x25303c). El curso elegido llega a
   la carrera por menu_ta_codigo (0x2a6758 = 4EEC80 del PC) y menu_ha_arbol. */
#define TABLERO_RAM (256u << 10)                            /* tablas de TABLERO.DAT (animacion, sprites, TAB1) */
typedef struct { uint32_t tok, pri, ult; } tb_d_t;
typedef struct {
    char m[4]; uint32_t _0;
    tb_d_t b4[8], b2[4], c0[4], c1[4], a[4][13], d[4][30];  /* carruseles (0x419fb0.. ver conv_tablero.py) */
    tb_d_t cursor, candado, r_si, r_no, aviso, bani[4][3];  /* 0x41a078, 0x41a088, 0x419d58, 0x419d64, 0x41abd0, 0x41a6c8.. */
    float cx, cy;                                           /* el candado en (-363, -206) */
} tb_t;
static spr_banco_t *tb_bk; static const tb_t *TB;
static int tb_k = 0x24; static int tb_sig = 0x24;                      /* clave del tablero y la del siguiente */
static uint32_t tb_bits;                                    /* raiz +0x208 (la declaracion de arriba) */
static int tb_20c;                                          /* raiz +0x20c bit 0 (opcion B de la etapa 0) */
static int tb_ra, tb_rd;                                    /* R de las metas (0x375e30) y de las etapas (0x375f50) */
static int tb_opb;                                          /* +0x27e68: opcion B puesta */
static struct { const tb_d_t *t; int max, idx; sa_r_t r; } tb_car;   /* carrusel (0x26df20 = 51B7B0 del PC) */
static sa_r_t tb_cur, tb_cand, tb_rind, tb_bani, tb_aviso;
static int tb_activo(void) { return TB != NULL; }
static int tb_abre(void)
{
    void *ram; uint32_t tam;
    if (TB) return 0;
    if (!(ram = menu_hueco ? menu_hueco(TABLERO_RAM) : NULL)) { dbglog(DBG_ERROR, "TABLERO: sin el hueco para TABLERO.DAT\n"); return -1; }
    spr_suelta_todas();
    if (!(tb_bk = spr_abre_en("TABLERO.DAT", ram, TABLERO_RAM))) {
        dbglog(DBG_ERROR, "TABLERO: no se pudo abrir TABLERO.DAT\n"); menu_hueco(0); return -1;
    }
    TB = (const tb_t *)spr_extra(tb_bk, &tam);
    if (!TB || ((uintptr_t)TB & 3) || tam < sizeof(tb_t) || memcmp(TB->m, "TAB1", 4)) {
        dbglog(DBG_ERROR, "TABLERO: TABLERO.DAT sin tablas\n"); spr_cierra(tb_bk); tb_bk = NULL; TB = NULL; menu_hueco(0); return -1;
    }
    spr_por_tramos(tb_bk, 1);
    tb_car.r.h = tb_cur.h = tb_cand.h = tb_rind.h = tb_bani.h = tb_aviso.h = -1;
    dbglog(DBG_INFO, "TABLERO: TABLERO.DAT (%u bytes de tablas en el hueco), VRAM libre %uK\n", (unsigned)hudd_tablas_ocupa(),
           (unsigned)(pvr_mem_available() >> 10));
    return 0;
}
static void tb_cierra(void)
{
    if (!tb_bk) return;
    spr_suelta_todas(); spr_cierra(tb_bk); tb_bk = NULL; TB = NULL;
    tb_car.r.h = tb_cur.h = tb_cand.h = tb_rind.h = tb_bani.h = tb_aviso.h = -1;
    if (menu_hueco) menu_hueco(0);
}
static int tb_modo(void) { return (int)(tb_bits >> 5) & 7; }   /* 2 INFARTO, 4 CONTRARRELOJ */
static int tb_juego(void) { return (int)(tb_bits & 3); }
static int tb_codigo(void) { return (int)(tb_bits >> 12) & 0x3f; }
static void tb_pon_codigo(int c) { tb_bits = (tb_bits & 0xfffc0fffu) | ((uint32_t)(c & 0x3f) << 12); }
/* carrusel: 51B7B0 / 51BDB0 / 51BE30 / 51C0B0 del PC (0x26df20 / 0x26e6e8 / 0x26e000 / 0x26e438 y 0x26e4f0 del PS2):
   la entrada 2 i + 1 es la quieta en i y la 2 i + 2 el paso de i a i + 1 (al reves hacia atras); n entradas */
static void tb_car_conf(int e, int rev, int quieta)
{
    const tb_d_t *d = tb_car.t + e;
    sr_pon(&tb_car.r, d->tok, (int)(rev ? d->ult : d->pri), (int)(rev ? d->pri : d->ult), 0, quieta ? 0 : 3, rev ? -1.0f : 1.0f);
}
static void tb_car_init(const tb_d_t *t, int n) { sr_suelta(&tb_car.r); tb_car.t = t; tb_car.max = (n - 1) / 2; tb_car.idx = 0; }
static void tb_car_elige(int i) { if (i >= 0 && i <= tb_car.max) { tb_car.idx = i; sr_suelta(&tb_car.r); } }
static void tb_car_paso(void) { sr_tick(&tb_car.r); if (sr_listo(&tb_car.r)) tb_car_conf(tb_car.idx * 2 + 1, 0, 1); }
static void tb_car_mueve(int der)
{
    int v = tb_car.idx;
    if (der ? v >= tb_car.max : v <= 0) return;
    tb_car.idx = v + (der ? 1 : -1);
    tb_car_conf(v * 2 + (der ? 2 : 0), !der, 0);
    sfx_menu(SFX_CURSOR);                                   /* 4249F0(1) */
}
/* el carrusel de las metas (0x251da0 = 4CFC40 del PC) y el de las etapas (0x25a880): R cambia de tabla y se queda en
   la misma posicion; al entrar, la posicion sale del codigo de la palabra del frontal */
static void tb_car_metas(int r)
{
    int i = tb_car.idx;
    if (r) { tb_ra = !tb_ra; }
    tb_car_init(TB->a[tb_juego() ? 2 + tb_ra : tb_ra], 0x18);
    if (r) tb_car_elige(i);
    else if (tb_modo() == 4) tb_car_elige(tb_ra ? tb_codigo() - 6 : tb_codigo());
    else tb_car_elige(tb_codigo());
}
static void tb_car_etapas(int r)
{
    int i = tb_car.idx;
    if (r) { tb_rd = !tb_rd; }
    tb_car_init(TB->d[tb_juego() ? 2 + tb_rd : tb_rd], 0x1e);
    if (r) tb_car_elige(i);
    else {
        if (tb_rd) tb_pon_codigo(tb_codigo() - 0xf);       /* 0x258234 */
        if (tb_codigo() < 0xf) tb_car_elige(tb_codigo());
    }
}
/* candado del indice actual (0x253418 / 0x24fd28.. / 0x258b84); -1 sin candado */
static int tb_candado(void)
{
    int i = tb_car.idx, j = tb_juego();
    switch (tb_k) {
    case 0x24: return tb_modo() == 1 && i >= 2 ? (i == 3 ? 0xcb : 0xcc) : -1;   /* (solo con OUTRUN: 4 posiciones) */
    case 0x26:
        if (tb_modo() == 4) {
            if (i == 5) return j ? (tb_ra ? 0xcd : 0xcb) : (tb_ra ? 0xce : 0xcc);
            if (i < 5 && tb_ra) return (j ? 0xc1 : 0xc2) + 2 * i;
            return -1;
        }
        return tb_modo() == 5 && i < 5 && tb_ra ? (j ? 0xc1 : 0xc2) + 2 * i : -1;
    case 0x27:
        if ((tb_modo() != 4 && tb_modo() != 5) || !tb_rd) return -1;
        if (tb_opb) return j ? 0xaa : 0xab;
        return (j ? 0x8a : 0x8b) + 2 * i;
    }
    return -1;
}
static int tb_bit_b(void) { return tb_juego() == 1 ? (tb_rd ? 0xaa : 0xa8) : (tb_rd ? 0xab : 0xa9); }   /* 0x259ef8 */
static void tb_entra(void)
{
    tb_k = tb_sig;
    sr_suelta(&tb_cur); sr_suelta(&tb_cand); sr_suelta(&tb_rind); sr_suelta(&tb_bani); sr_suelta(&tb_aviso);
    tb_opb = 0;
#if defined(PRUEBA_TABLERO_TODO)
    {   /* prueba (-DPRUEBA_TABLERO_TODO): la licencia activa con todas las ETAPAS y METAS del SALON (bits 0x8a..0xce)
           al entrar en un tablero por primera vez */
        static int dado; uint8_t *p = activa(); int b;
        if (p && !dado) {
            for (b = 0x8a; b <= 0xce; b++) p[0x28 + b / 8] |= (uint8_t)(1 << (b % 8));
            p[0x3f4] |= 2; dado = 1; dbglog(DBG_INFO, "TABLERO: prueba, la licencia con las ETAPAS y METAS\n");
        }
    }
#endif
    switch (tb_k) {
    case 0x24:                                              /* 0x252b50 */
        if (tb_modo() == 1) tb_car_init(TB->b4, 8), tb_car_elige(tb_juego());
        else { tb_car_init(TB->b2, 4); if (tb_juego() < 2) tb_car_elige(tb_juego()); }
        break;
    case 0x25:                                              /* 0x2575d0 */
        tb_car_init(tb_juego() ? TB->c1 : TB->c0, 4); tb_car_elige(tb_bits & 0x800 ? 0 : 1);
        break;
    case 0x26: tb_car_metas(0); break;                      /* 0x24f490: +0x3f4 = 0x375e30 */
    case 0x27: tb_20c = 0; tb_car_etapas(0); break;         /* 0x257fd0: +0x538 = 0x375f50 */
    }
    dbglog(DBG_INFO, "TABLERO: clave 0x%x, palabra %08x, posicion %d\n", tb_k, (unsigned)tb_bits, tb_car.idx);
}
static void tb_opcion_b(int pon)                            /* 0x259ea0 / 0x25a040: la animacion +0x348 */
{
    const tb_d_t *t = TB->bani[(tb_juego() == 1 ? 2 : 0) + tb_rd];
    if (pon) { sr_pon(&tb_bani, t[1].tok, (int)t[0].pri, (int)t[1].ult, 1, 5, 1); tb_opb = 1; }
    else { sr_pon(&tb_bani, t[2].tok, (int)t[2].pri, (int)t[2].ult, 1, 3, 1); tb_opb = 0; }
}
/* un paso del tablero: 0 sigue, 1 al coche (clave 0xa), 2 atras a UN JUGADOR, 3 a otro tablero (tb_sig) */
static int tb_control(unsigned n, int atiende)
{
    int a, i, b;
    if (!TB) return 2;
    sa_ejes(); a = atiende ? sa_accion(n) : 12;
    if (atiende && a == 12 && r1_nuevo) a = 11;               /* el gatillo R (bit 0x2000 del frontal) */
    spr_paso();
    tb_car_paso();
    if (tb_cur.h < 0) sr_pon(&tb_cur, TB->cursor.tok, (int)TB->cursor.pri, (int)TB->cursor.ult, 4, 0, 1);   /* +0x2a8 */
    b = tb_candado();
    if (b >= 0 && !sa_tiene(b)) {
        if (tb_cand.h < 0) { sr_pon(&tb_cand, TB->candado.tok, (int)TB->candado.pri, (int)TB->candado.ult, 4, 0, 1); spr_traslada(tb_cand.h, TB->cx, TB->cy); }
    } else sr_suelta(&tb_cand);
    if (tb_k == 0x26 || tb_k == 0x27) {                     /* el indicador de R (+0x488) */
        const tb_d_t *d = (tb_k == 0x26 ? tb_ra : tb_rd) ? &TB->r_si : &TB->r_no;
        sr_pon(&tb_rind, d->tok, (int)d->pri, (int)d->ult, 4, 0, 1);
    }
    if (tb_k == 0x27) {                                     /* el aviso de la opcion B en la etapa 0 (0x258d20) */
        if (tb_car.idx == 0 && sa_tiene(tb_bit_b())) { if (tb_aviso.h < 0) sr_pon(&tb_aviso, TB->aviso.tok, (int)TB->aviso.pri, (int)TB->aviso.ult, 4, 0, 1); }
        else sr_suelta(&tb_aviso);
        sr_tick(&tb_bani);
    }
    i = tb_car.idx;
    switch (a) {
    case 0:                                                 /* A */
        switch (tb_k) {
        case 0x24:                                          /* 0x253890 / 0x2539c4 */
            if (tb_modo() <= 3) {
                if (b >= 0 && !sa_tiene(b)) { bloqueado(); return 0; }
                tb_bits = (tb_bits & ~3u) | (uint32_t)(i & 3); return 1;
            }
            tb_bits |= 0x800; tb_bits = (tb_bits & ~3u) | (i ? 1u : 0u); tb_sig = 0x25; return 3;
        case 0x25:                                          /* 0x256520: 0x375e48[i] */
            if (i == 0) tb_bits |= 0x800; else tb_bits &= ~0x800u;
            tb_sig = i == 0 ? 0x26 : 0x27; return 3;
        case 0x26:                                          /* 0x2502cc */
            if (b >= 0 && !sa_tiene(b)) { bloqueado(); return 0; }
            tb_bits |= 0x800;
            tb_pon_codigo(i + (tb_ra ? (tb_modo() == 4 ? 6 : 5) : 0));
            return 1;
        case 0x27:                                          /* 0x259198 */
            tb_pon_codigo(i + (tb_rd ? 0xf : 0)); tb_20c = 0;
            if (tb_opb) { tb_20c = 1; tb_pon_codigo(tb_rd ? 0xf : 0); return 1; }
            if (b >= 0 && !sa_tiene(b)) { bloqueado(); return 0; }
            return 1;
        }
        break;
    case 1:                                                 /* B: el tablero anterior (la pila de la raiz) */
        if (tb_k == 0x24) tb_bits &= ~0x803u;               /* 0x253e80 */
        if (tb_k == 0x25) tb_bits |= 0x800;                 /* 0x2566d8 */
        if (tb_k == 0x26) { tb_ra = 0; tb_bits &= 0xfffc0fffu; }   /* 0x250be0 */
        if (tb_k == 0x27) { tb_opb = 0; tb_rd = 0; tb_bits &= 0xfffc0fffu; }   /* 0x259834 */
        if (tb_k == 0x24) return 2;
        tb_sig = tb_k == 0x25 ? 0x24 : 0x25; return 4;
    case 3: case 5:                                         /* izquierda / derecha */
        if (a == 5 && tb_k == 0x26 && (i == 5 || (tb_modo() != 4 && i == 4))) break;   /* 0x250d44 */
        if (tb_k == 0x27 && tb_opb) { sr_suelta(&tb_bani); tb_opb = 0; }   /* 0x2599b8 */
        tb_car_mueve(a == 5);
        break;
    case 4:                                                 /* abajo: la opcion B en la etapa 0 (0x259ea0) */
        if (tb_k != 0x27 || i != 0) break;
        if (tb_opb) tb_opcion_b(0);
        else if (sa_tiene(tb_bit_b())) tb_opcion_b(1);
        break;
    case 11:                                                /* R: las del reves (0x250288 / 0x25914c) */
        if (tb_k == 0x26) tb_car_metas(1);
        else if (tb_k == 0x27) { sr_suelta(&tb_bani); tb_opb = 0; tb_car_etapas(1); }
        break;
    }
    return 0;
}
static void tb_dibuja(void)
{
    if (!TB) return;
    spr_dibuja(0, SPR_CAPAS, 3, 0);
}
/* CONTRARRELOJ: el codigo del curso de la carrera ([656234]; 0x2a6758 = 4EEC80 del PC) o -1. preset: 0x2a6978 (juego
   0 -> 1, 1 -> 0) y los cursos seguidos (5 / 11: preset 2 / 3) */
int menu_ta_codigo(int *bandera)
{
    int pre = tb_juego() == 0 ? 1 : tb_juego() == 1 ? 0 : tb_juego() == 2 ? 3 : 2, c = tb_codigo();
    if (bandera) *bandera = 0;
    if (sp_sel || modo_sel != 3 || tb_modo() != 4) return -1;
    if (!(tb_bits & 0x800)) {                               /* 0x2a6800: una etapa */
        if (c >= 0xf) c += 0xf;
        if (pre == 0) c += 0xf;
        if (bandera) *bandera = tb_20c;
        return c;
    }
    if (pre == 0) return c == 5 ? 0x52 : c == 11 ? 0x53 : c + (c > 5 ? 0x45 : 0x41);
    return c == 5 ? 0x50 : c == 11 ? 0x51 : c + (c > 5 ? 0x40 : 0x3c);
}
/* INFARTO: el juego de cursos elegido (raiz +0x208 bits 0..1): 0 OutRun 2 (csc_data_2: el arbol 0), 1 OutRun 2 SP
   (csc_data_cvt: el arbol 1); -1 fuera del INFARTO */
int menu_ha_juego(void) { return !sp_sel && modo_sel == 2 && tb_modo() == 2 ? tb_juego() : -1; }
/* el juego de cursos de los tableros (raiz +0x208 bits 0..1; se queda de una carrera a otra, como en el original): lo
   lee 0x2a6758 al salir del menu en cualquier modo de la consola (0x2a6978: el preset) */
int menu_juego_cursos(void) { return tb_juego(); }

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
        if (pant == P_MENU) hudd_reglas(r_menu, n_r_menu);  /* (sin MULTIPLAYER: menu_sin_multi) */
        an_dibuja(&a_sel, sacude, (entra_atras ? a_ent : 1) * a_sal * (cargando ? 0 : 1), 3, 0);
        hudd_reglas(NULL, 0);
        break;
    case P_C2C: c2_dibuja(); break;
    case P_SALON: sa_dibuja(); break;
    case P_TABLERO: tb_dibuja(); break;
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
    if (g_est) {                                            /* guardar con Y: aviso en una franja en medio de la pantalla */
        int id = g_est == G_PIDE ? 0x206 : g_est == G_BIEN ? 0x202 : 0x31d, n_sin;
        float a = g_est == G_PIDE ? 1.0f : fminf(1.0f, (G_T_MSG + (g_est == G_MAL ? 300 : 0) - g_t) / 15.0f);
        if (g_est == G_MAL && vmu_diag.cod == VE_SITIO && (txt(0x30d, &n_sin), n_sin)) id = 0x30d;   /* sin sitio */
        if (g_est == G_MAL) {                               /* error: el texto y debajo el codigo (docs/DREAMCAST.md) */
            char c[80]; vmuesc_codigo(&vmu_diag, c, sizeof c);
            pz_rect(60, 270, 520, 58, ((uint32_t)(0xD0 * a) << 24) | 0x202830, 8);
            texto_id(id, 320, 286, 12, a, 8.5f, 0xF04030);
            texto_c(c, 320, 314, 12, a, 8.5f, 0xF0F0F0);
        } else {
            pz_rect(160, 282, 320, 32, ((uint32_t)(0xD0 * a) << 24) | 0x202830, 8);
            texto_id(id, 320, 298, 12, a, 8.5f, 0xF0F0F0);
        }
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

static void pz_cab_pon(void)                                /* cabecera de color sin textura (lista TR) */
{
    if (!pz_cab_ok) {
        pvr_poly_cxt_t cx; pvr_poly_cxt_col(&cx, PVR_LIST_TR_POLY);
        cx.depth.comparison = PVR_DEPTHCMP_ALWAYS; cx.depth.write = PVR_DEPTHWRITE_DISABLE; cx.gen.culling = PVR_CULLING_NONE;
        pvr_poly_compile(&pz_cab, &cx); pz_cab_ok = 1;
    }
    pvr_prim(&pz_cab, sizeof pz_cab);
}
static void pz_rect(float x, float y, float w, float h, uint32_t argb, float z)
{
    pvr_vertex_t v; int k;
    pz_cab_pon();
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

/* disco (elipse rx x ry) en una tira de PZ_FILAS filas, color de arriba ca a abajo cb (ARGB) */
#define PZ_FILAS 10
static void pz_disco(float cx, float cy, float rx, float ry, uint32_t ca, uint32_t cb, float z)
{
    pvr_vertex_t v; int i, k, j;
    pz_cab_pon();
    v.oargb = 0; v.u = v.v = 0; v.z = z;
    for (i = 0; i <= PZ_FILAS; i++) {
        float t = (float)i / PZ_FILAS, e = 2 * t - 1, w = rx * sqrtf(fmaxf(0.0f, 1 - e * e)); uint32_t c = 0;
        for (j = 0; j < 32; j += 8) c |= (uint32_t)((((ca >> j) & 255) * (1 - t) + ((cb >> j) & 255) * t) + 0.5f) << j;
        v.argb = c; v.y = cy - ry + 2 * ry * t;
        for (k = 0; k < 2; k++) {
            v.x = cx + (k ? w : -w);
            v.flags = i == PZ_FILAS && k ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            pvr_prim(&v, sizeof v);
        }
    }
}
static uint32_t pz_mezcla(uint32_t c, int a, float f)       /* c (RGB) hacia el blanco (f > 0) o el negro (f < 0) */
{
    uint32_t r = (uint32_t)a << 24; int j;
    for (j = 0; j < 24; j += 8) {
        float x = (c >> j) & 255; x = f > 0 ? x + (255 - x) * f : x * (1 + f);
        r |= (uint32_t)(x + 0.5f) << j;
    }
    return r;
}
/* boton de la Dreamcast (DESVIACION, revision 42, como los de la barra de ayuda de MENU.DAT, conv_menu.py boton_dc):
   circulo del color del mando con sombra, borde oscuro, degradado y brillo, y la letra blanca con contorno oscuro */
static void pz_boton(const char *letra, float cx, float cy, uint32_t col, float z)
{
    float r = 9.5f; int k;
    pz_disco(cx + 1, cy + 1, r, r, 0x70000000, 0x70000000, z);
    pz_disco(cx, cy, r, r, pz_mezcla(col, 255, -0.5f), pz_mezcla(col, 255, -0.5f), z + 0.001f);
    pz_disco(cx, cy, r - 1.5f, r - 1.5f, pz_mezcla(col, 255, 0.38f), pz_mezcla(col, 255, -0.22f), z + 0.002f);
    pz_disco(cx, cy - r * 0.42f, r * 0.62f, r * 0.36f, 0x60FFFFFF, 0x00FFFFFF, z + 0.003f);
    for (k = 0; k < 4; k++)
        texto_c(letra, cx + (k & 1 ? 1 : -1), cy + 1 + (k & 2 ? 1 : -1), 12, 0.8f, z + 0.004f, pz_mezcla(col, 0, -0.65f) & 0xFFFFFF);
    texto_c(letra, cx, cy + 1, 12, 1, z + 0.005f, 0xFFFFFF);
}
/* botones de la ventana (48C5F0: ELEGIR 0x294 en (x + 45, y + h - 38), ATRAS 0x295 acabando en x + w - 45) con el
   boton de la Dreamcast delante (los iconos del original son imagenes del XST sin localizar) */
static void pz_botones(float x, float y, float w, float h, float z, int elegir)
{
    int n; const uint8_t *t = txt(0x295, &n); float xa = x + w - 45 - f_ancho(t, n), cy = y + h - 38 + f_h * 0.5f;
    if (elegir) {
        pz_boton("A", x + 45 - 13, cy, 0xE22E28, z);
        texto_id(0x294, x + 45, y + h - 38, 1, 1, z, 0xF0F0F0);
    }
    pz_boton("B", xa - 13, cy, 0x2662DC, z);
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
