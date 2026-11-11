/* Fin de la carrera del original en la consola (SLES-53998; PC: race_end_modes.cpp, SUMO_FE con [780260] = 1):

   - Modo 34 (0x22) OutRun Miles: PS2 0x1bc270 / 0x1bc3d8 / 0x1bc618 (PC 49B470 / 49B570 / 49B7C0). Se llega desde la
     meta (modo 19 -> 33, que sin red pasa directo al 34), desde el fin del tiempo (modo 20, 0x1b9aa0: 43F860 -> 0x22)
     y desde REINTENTAR / ABANDONAR de la pausa (0x249c98 / 0x24a140: 0x22 con 450230(6) / (7)). Al entrar calcula las
     millas de la carrera (4EF550 = PS2 0x1f3e90), crea la escena 2B004C (panel "MILLAS OUTRUN:", modo 5) y pone a 0
     las 8 cifras (0x369250..); con REINTENTAR / ABANDONAR de la pausa suspende ademas la carrera. Cada paso, si el
     panel ha acabado (estado 4), cada cifra cuenta hasta su digito x 6 (un cuadro de la escena 2B004B por paso); al
     pararse la cuenta suena el efecto 0x217 (11c090 = 424940; muestra de \MILES.pak). Sigue cuando acaba su
     temporizador (0xF0 pasos, 1b4a48) o con A / B (1b4548 bits 4 / 8): 450240 = 6 -> REINTENTAR (modo 30), 2 (meta)
     -> continuar (35), 7 -> GAME OVER (27), 1 (fin del tiempo) -> 27 en red (46C500) o en el Time Attack de una etapa
     (48B310 y 48B350), si no continuar. Al salir suma las millas a la licencia (4EF510 = PS2 0x1f41d8).
     Las cifras las dibuja el GAD (0x1a0c98 = 497FA0): 8 veces 2B004B (capa 5) en (540 - 320 - i x 32, 210 - 240).
   - Modo 35 (0x23) continuar: PS2 0x1bc650 / 0x1bc700 / 0x1bc8c8 (PC 49B7E0 / 49D9C0 / 49DAE0). Fuera de las
     misiones de C2C: 2B004D ("VOLVER A INTENTARLO?", modo 5); cuando acaba, 2B004A (REINTENTAR / ABANDONAR, modo 5);
     cuando acaba esta, A -> modo 30 (REINTENTAR: la misma carrera desde la salida, 49B190 -> START 13) y B -> modo 28
     (al frontal), los dos con el efecto 0x40 (11b2b0 = 4249F0).
   - Modo 27 (0x1b) GAME OVER: PS2 0x1bafc8 / 0x1bb120 (PC 49ADC0 / 49AEA0): temporizador 0xF0 y despues el modo 28
     (al frontal); el GAD (0x1a04f0 = 497960) crea 2C000B (capa 0, modo 1) y, una vez, la voz 0x954 (0x1E4 en las
     variantes 4 y 6).
   Las millas (4EF550, variante v):
     m = 0; con la bandera 84BCFC (primer puesto en una tabla de records) m += la de la variante de OutrunMiles.bin y
     v 1 / 9: puntos / 1000, v 2: corazones / 10000 / 2,5; sin ella v 1 / 9: puntos / 3000, v 2: corazones / 10000 /
     10; m += 84BCF8 (por etapa: 4EF460 +25 / +10 / +10 / +25 / +15 / +15 por cada estadistica limpia y 4EF440 40 por
     segundo por debajo de 60, desde 4F2B20); fuera de red m += (car+E78 / 60) x 5 (car+E78: veces que el rebufo
     encontro un coche mejor, CheckSlipStream) y car+E78 = 0.
   - OUTRUN2SP (1b4878 = 0): el modo 27 (fin_game_over) desde ABANDONAR de la pausa con la misma escena 2C000B y tras la
     meta (19 -> 33 -> 24 -> 25 -> 27) o el fin del tiempo (20 -> 25 -> 26 -> 27) con la escena 34001F (GOVSP.DAT); los
     resultados (25, el mapa de la ruta) mas abajo; el final de recreativa (24) y la entrada de nombre (26) estan SIN
     PORTAR (la meta pasa directa al 25 y el 25 al 27).
   SIN PORTAR: la bandera 84BCFC y su suma (las tablas de records no estan en el port: nunca se pone), las variantes 4
   (red) y 6 (misiones de C2C: 4EF550 con 496510 / 495890, otros tokens en el modo 35), la espera de la muestra 0x1a1 de
   los modos 34 (pausa) y el bloqueo de botones del modo 35 (+0x5E0 del objeto 0x375068, PC +0x518 de 4035F0: sin
   localizar quien lo pone).
   Los graficos son FIN.DAT y GOVER.DAT (tools/conv_fin.py): se abren al entrar (solo las tablas en la RAM) y sus
   texturas se suben mientras se ven. */
#include <kos.h>
#include <string.h>
#include "fin.h"
#include "../comun/hud.h"
#include "../comun/sfx.h"
#include "../comun/menu.h"
#include "../comun/musica.h"
#include "../comun/sprani.h"

#define M34_CUADROS 0xf0                                /* 0x1bc288: 1b4a10(0xF0) */
#define M27_CUADROS 0xf0                                /* 0x1bafe0: 1b4a10(0xF0) */
#define BOTON_A CONT_A                                  /* 1b4548(4): la cruz del PS2 (A, como en los menus) */
#define BOTON_B CONT_B                                  /* 1b4548(8): el circulo (B) */

static uint32_t m_etapa;                                /* 84BCF8 (PS2 0x36d2e4) */
static uint32_t rebufo_e78;                             /* car+E78 del coche del jugador */
static uint32_t millas;                                 /* 84BCF4 (PS2 0x36d2e0) */
static int8_t cifra[8];                                 /* 836D1C.. (PS2 0x369250..): contador de cada cifra */
static int fase = -1, como, t_fase, t_temp, rodando, voz_go;
static int t_panel = -1, t_preg = -1, t_elige = -1, t_go = -1;   /* cuadros desde que se creo cada escena (-1: no) */
static hud_dat_t *fd;                                   /* FIN.DAT o GOVER.DAT */
static int u_millas = -1, u_cifra[8], u_preg = -1, u_elige = -1, u_go = -1;
static int n_millas, n_preg, n_elige, n_go;             /* cuadros de cada escena */

void fin_carrera_empieza(void) { rebufo_e78 = 0; }
void fin_rebufo(int n) { rebufo_e78 += (uint32_t)n; }

/* 4EF460 (limpio: +84DF28, +84DF1C, +84DF20, +84DF24, +84DF2C, +84DF30 a cero) y 4EF440(max(60 - segundos, 0)); fuera
   de red (22b880 = 55A930) */
void fin_etapa(const int limpio[6], int segundos)
{
    static const uint32_t S[6] = { 0x19, 0xa, 0xa, 0x19, 0xf, 0xf };
    int k, q = 60 - segundos;
    for (k = 0; k < 6; k++) if (limpio[k]) m_etapa += S[k];
    if (q < 0) q = 0;
    m_etapa += (uint32_t)q * 40;
}

static int sube(int u)
{
    int t0, n = hudd_tex_uso(fd, u, &t0);
    if (n && hudd_sube(fd, t0, n)) { dbglog(DBG_ERROR, "FIN: sin VRAM para el uso %d\n", u); return -1; }
    return 0;
}
static void baja(int u)
{
    int t0, n;
    if (fd && u >= 0 && (n = hudd_tex_uso(fd, u, &t0))) hudd_baja(fd, t0, n);
}
static void abre(const char *f)
{
    if (fd) hudd_cierra(fd);
    musica_pausa_lectura(1);                            /* el GD para el fichero (la musica tira del anillo) */
    fd = hudd_abre(f, 0, 1);
    musica_pausa_lectura(0);
    if (!fd) dbglog(DBG_ERROR, "FIN: no se pudo abrir %s\n", f);
}

void fin_entra(int c, const fin_carrera_t *k)
{
    int i; char n[16];
    /* 4EF550 (PS2 0x1f3e90) con la variante */
    millas = 0;
    if (k->variante == 1 || k->variante == 9) millas += (uint32_t)(k->puntos / 3000);   /* 0x1f4018: div 0xBB8 */
    else if (k->variante == 2) millas += (uint32_t)(int32_t)((float)k->corazones / 10000.0f / 10.0f);   /* 0x1f4034 */
    millas += m_etapa;
    millas += rebufo_e78 / 60 * 5;                      /* 0x1f40b0 (fuera de red) */
    rebufo_e78 = 0;
    como = c; fase = 34; t_fase = 0; t_temp = M34_CUADROS; rodando = 0;
    memset(cifra, 0, sizeof cifra);
    /* 428600 borra los sprites de la carrera y el NAVI ya no dibuja: las texturas del HUD fuera de la VRAM (vuelven al
       empezar otra carrera, hud_sube_tex), que en el INFARTO no queda sitio para las de aqui */
    hud_baja_tex(); hudd_vacia();
    abre("FIN.DAT");
    u_millas = fd ? hudd_uso(fd, "MILLAS") : -1; u_preg = fd ? hudd_uso(fd, "CONT_PREG") : -1;
    u_elige = fd ? hudd_uso(fd, "CONT_ELIGE") : -1;
    for (i = 0; i < 8; i++) { sprintf(n, "MILLAS_C%d", i); u_cifra[i] = fd ? hudd_uso(fd, n) : -1; }
    n_millas = fd && u_millas >= 0 ? hudd_cuadros(fd, u_millas) : 1;
    n_preg = fd && u_preg >= 0 ? hudd_cuadros(fd, u_preg) : 1;
    n_elige = fd && u_elige >= 0 ? hudd_cuadros(fd, u_elige) : 1;
    if (fd) { sube(u_millas); sube(u_cifra[0]); }
    dbglog(DBG_INFO, "FIN: FIN.DAT %s, VRAM libre %uK\n", fd ? "abierto" : "sin abrir", (unsigned)(pvr_mem_available() >> 10));
    t_panel = 0; t_preg = t_elige = t_go = -1;
    dbglog(DBG_INFO, "FIN: modo 34 (450240 = %d), millas %u (puntos %d, etapas %u, variante %d)\n", c, (unsigned)millas,
           k->puntos, (unsigned)m_etapa, k->variante);
}

/* 4EF510 (PS2 0x1f41d8): a la licencia y a cero */
static void suma_licencia(void)
{
    menu_suma_millas(millas);
    millas = 0; m_etapa = 0;
}

static void a_continuar(void)                           /* 0x1bc650 */
{
    baja(u_millas); baja(u_cifra[0]); t_panel = -1;
    fase = 35; t_fase = 0;
    dbglog(DBG_INFO, "FIN: modo 35 (continuar), VRAM libre %uK\n", (unsigned)(pvr_mem_available() >> 10));
    if (fd) sube(u_preg);
    t_preg = 0;
}
static int go_sp;                                       /* 0x1a0508..0x1a052c: 1b4878 = 0 y 450240 != 7 -> 34001F */
static void a_game_over(void)                           /* 0x1bafc8 */
{
    baja(u_millas); baja(u_cifra[0]); t_panel = -1;
    fase = 27; t_fase = 0; t_temp = M27_CUADROS; voz_go = 1;
    dbglog(DBG_INFO, "FIN: modo 27 (GAME OVER)\n");
    abre(go_sp ? "GOVSP.DAT" : "GOVER.DAT");
    u_go = fd ? hudd_uso(fd, go_sp ? "GAME_OVER_SP" : "GAME_OVER") : -1;
    n_go = fd && u_go >= 0 ? hudd_cuadros(fd, u_go) : 1;
    if (fd) sube(u_go);
    t_go = 0;                                           /* 0x1a04f0: el GAD crea la escena y dice la voz una vez */
    if (voz_go) { voz_go = 0; sfx_voz(0x954); }
}

/* OUTRUN2SP (1b4878 = 0): el modo 27 desde ABANDONAR de la pausa (0x24a150, 450230(7): con 450240 = 7 el GAD crea la
   misma escena 2C000B que la consola, 0x1a051c) o tras la meta / el fin del tiempo (450240 = 2 / 1: la escena 34001F
   del banco 0x34, GOVSP.DAT) */
void fin_game_over(int c)
{
    int i;
    hud_baja_tex(); hudd_vacia();
    u_millas = u_preg = u_elige = -1;
    for (i = 0; i < 8; i++) u_cifra[i] = -1;
    como = c; go_sp = c != FIN_PAUSA_ABANDONA;
    a_game_over();
    go_sp = 0;
}

/* ---------------------------------------------------------------- OUTRUN2SP: modo 25 (resultados)
   PS2 0x1ba6c0 / 0x1ba748 / 0x1ba910 (tabla de modos 0x3f8e50 + 25 x 16; PC 49A790 / 49A800 / 49A920): temporizador
   0xB4 (1b4a10), el banco 0x34 ROUTE_CVT (1183d0 / 115d78), la musica 401000(0, 0x1C, 1) (\music\28.ps2 en bucle,
   MUS28.ADP), 0x5a2030 = 1a7180(preset) solo en la variante 0 (el record de la Contrarreloj) y la animacion de carga
   (288d20(1)). Con el banco leido el suceso 0x188 con la funcion 0x15 (el mapa de la ruta; la 0x50, resultado de la
   Contrarreloj de los presets 2 / 3, si 1b4a90: esos presets no salen, SIN PORTAR) y fuera la animacion de carga. Despues
   A o START (210c58: 210988(1) / (4)) o el mapa acabado (209d50 = [0x370dc8]) y entonces el temporizador o A / START
   piden el modo siguiente (1ba7f0..1ba848): 26 (entrada de nombre) con 450240 = 1 (fin del tiempo) o en la variante 0
   con el record; si no, 27. Las variantes 3 / 4 piden antes el 27. En la Dreamcast el banco se lee de un tiron al
   entrar: sin la animacion de carga (banco 0x33, sin convertir). RUTASP.DAT de tools/conv_sp.py (banco 0x34 entero para
   comun/sprani.c y el bloque RSP1 con las tablas del mapa).
   El mapa (PS2 0x209208 / 0x209518 / 0x2099f8 = PC 4AE750 / 4AE960 / 4AED20, bloque de trabajo 0x5df960 = PC 842048):
   el coche del mapa (token por el modelo y su fila de 0x40fc10, 340017 sin color) recorre los puntos de la ruta (0x40fd70)
   tramo a tramo, 0x28 pasos cada uno, por los nodos (0x40fc88) de la rama tomada en cada nivel (451350); deja el tramo
   34001B (capa 2, modo 1) al empezar cada uno y la marca 34001C (capa 3) en los de etapa; con el nivel de 4AEEF0 (fin
   del tiempo) se para en la razon de la posicion del coche en su etapa o union. */
#define M25_CUADROS 0xb4                                /* 0x1ba6c4: 1b4a10(0xB4) */
#define M26_PASA 0                                      /* el modo 26 (entrada de nombre) SIN PORTAR: directo al 27 */
typedef struct {
    char id[4];
    uint32_t fila[30];                                  /* 0x40fc10: fila de cada modelo (1..3) */
    uint32_t tok[3][30];                                /* 0x40fe68: coche del mapa por fila y modelo */
    int32_t nodo[15][4];                                /* 0x40fc88: {de, a, siguiente 0, siguiente 1} */
    float pt[31][2];                                    /* 0x40fd70: puntos del mapa */
} rsp_t;
static spr_banco_t *rb;
static const rsp_t *R;
static struct {
    float p0[2], p[12][2];                              /* +0 y +8.. (12 puntos a -10000) */
    int32_t kind[13];                                   /* +0x68..: 0 etapa, 1 izquierda, 2 derecha, -1 sin llegar */
    int step, pos, count, h, h2;                        /* +0x98, +0x9c, +0xa0 (0x28), +0xa4, +0xac */
    uint32_t token;                                     /* +0xa8 */
} ru;
static int ru_hecho;                                    /* 0x370dc8 */
static int ru_nivel = -1, ru_lado = -1; static float ru_razon = -1;   /* 0x370dd0 / 0x370dcc / 0x370dd4 (4AEEF0) */
static int s25;                                         /* 0x369238 (PC 836D08) */
static fin_sp_t sp_d;
static void ruta_init(void)                             /* 0x209208 */
{
    const fin_sp_t *d = &sp_d; int k, leg, node = 0;
    if (d->modelo >= 0 && d->modelo < 30 && d->color && R->fila[d->modelo] >= 1 && R->fila[d->modelo] <= 3)
        ru.token = R->tok[R->fila[d->modelo] - 1][d->modelo];
    else ru.token = R->tok[0][0];
    ru_hecho = 0;
    for (k = 0; k < 12; k++) { ru.p[k][0] = ru.p[k][1] = -10000.0f; ru.kind[k] = 0; }
    ru.kind[12] = 0;
    ru.step = ru.pos = 0; ru.count = 0x28;
    memcpy(ru.p0, R->pt[1], 8); memcpy(ru.p[0], R->pt[1], 8); memcpy(ru.p[1], R->pt[3], 8);   /* 0x40fd78, 0x40fd88 */
    ru.h2 = -1;
    for (leg = 0; leg < 4; leg++) {
        int r = d->rama[leg];
        if (r == 1) { node = R->nodo[node][3]; ru.kind[2 * leg + 1] = 2; }
        else if (r == 0) { node = R->nodo[node][2]; ru.kind[2 * leg + 1] = 1; }
        else { ru.kind[2 * leg + 1] = -1; break; }
        if (node < 0 || node >= 15) break;
        memcpy(ru.p[2 + 2 * leg], R->pt[R->nodo[node][0]], 8); memcpy(ru.p[3 + 2 * leg], R->pt[R->nodo[node][1]], 8);
    }
    if (d->preset == 0 || d->preset == 2) { spr_crea_entera(0x34000e, 1, 0); spr_crea_entera(0x34001a, 5, 0); }
    else { spr_crea_entera(0x34000d, 1, 0); spr_crea_entera(0x340019, 5, 0); }
    spr_crea_entera(0x34001d, 6, 1);
    ru.h = spr_crea(ru.token, 4, 0, 0x5b, 0x96);
    spr_traslada(ru.h, ru.p0[0], ru.p0[1]);
}
static void ruta_control(void)                          /* 0x209518 */
{
    float t, x, y;
    int st;
    if (ru_hecho) return;
    if (ru.p[ru.step + 1][0] < -9000.0f) {              /* 0x209554: lui 0xc60c, ori 0xa000 */
        memcpy(ru.p[ru.step + 1], ru.p[ru.step], 8);
        ru_hecho = 1; spr_modo(ru.h2, 2);               /* 115020(h, 2) = 428770 */
    }
    if (ru_nivel != -1) {
        int lim = ru_lado == 0 ? ru_nivel * 2 : ru_nivel * 2 + 1;
        if (!(ru.step < lim)) {
            t = (float)ru.pos / (float)ru.count;
            if (ru_razon <= t || lim < ru.step) { ru_hecho = 1; spr_modo(ru.h, 2); spr_modo(ru.h2, 2); return; }
        }
    }
    if (ru.pos == ru.count - 0xe && !(ru.p[ru.step + 2][0] < -9000.0f) && !ru_hecho) {
        float v = 1; int a = -1, b = -1;
        switch (ru.kind[ru.step + 1]) {
        case 0: if (ru.kind[ru.step] == 2) { v = -1; a = 0x97; b = 0xa5; } else { v = 1; a = 0x3d; b = 0x4b; } break;
        case 1: v = -1; a = 0x3d; b = 0x4b; break;
        case 2: v = 1; a = 0x97; b = 0xa5; break;
        default: break;                                 /* (el PS2 usa registros sin poner: la pierna siguiente siempre
                                                           es 0..2 si su punto existe) */
        }
        if (a >= 0) { spr_suelta(ru.h); ru.h = spr_crea(ru.token, 4, 0, a, b); spr_velocidad(ru.h, v); }
    }
    st = ru.step;
    t = (float)ru.pos / (float)ru.count;
    x = (ru.p[st + 1][0] - ru.p[st][0]) * t + ru.p[st][0];
    y = (ru.p[st + 1][1] - ru.p[st][1]) * t + ru.p[st][1];
    spr_traslada(ru.h, x, -y);
    if (ru.pos == 0 && st < 9) {
        int a, b, m;
        switch (ru.kind[st]) { case 1: a = 0x29; b = 0x50; break; case 2: a = 0x51; b = 0x78; break; default: a = 1; b = 0x28; break; }
        ru.h2 = spr_crea(0x34001b, 2, 1, a, b); spr_traslada(ru.h2, x, -y);
        if (st && !(st & 1)) { m = spr_crea_entera(0x34001c, 3, 0); spr_traslada(m, x, -y); }
    }
    if (++ru.pos < ru.count - 1) return;
    ru.step++;
    {
        int a, b;
        switch (ru.kind[ru.step]) { case 0: a = 0x5b; b = 0x95; break; case 1: a = 1; b = 0x3c; break;
                                    case 2: a = 0xb5; b = 0xf0; break; default: return; }
        spr_suelta(ru.h); ru.h = spr_crea(ru.token, 4, 0, a, b); ru.pos = 0;
        spr_traslada(ru.h, ru.p[ru.step][0], -ru.p[ru.step][1]);
    }
}
static void ruta_suelta(void)                           /* 0x2099f8 */
{
    spr_suelta_todas();
    ru_nivel = -1; ru_razon = -1; ru_hecho = 0; ru_lado = -1;
}
static void a_game_over(void);
static void sp_siguiente(void)                          /* 1ba7f0..1ba848 (PC result_next) */
{
    /* 450240 = 1 o (variante 0 y record) -> 26; si no 27 */
    if (como == FIN_TIEMPO && M26_PASA) return;
    ruta_suelta();                                      /* salida del modo 25 (0x1ba910): fin del suceso 0x188 */
    if (rb) { pvr_wait_ready(); spr_cierra(rb); rb = NULL; R = NULL; }
    go_sp = 1; a_game_over(); go_sp = 0;
}
void fin_sp_resultados(int c, const fin_sp_t *d)
{
    uint32_t tam;
    hud_baja_tex(); hudd_vacia();
    u_millas = u_preg = u_elige = -1;
    { int i; for (i = 0; i < 8; i++) u_cifra[i] = -1; }
    como = c; sp_d = *d;
    ru_nivel = d->nivel; ru_lado = d->lado; ru_razon = d->razon;
    fase = 25; t_fase = 0; t_temp = M25_CUADROS; s25 = 0;
    musica_pon(0x1c);                                   /* 100050(0, 0x1C, 1) */
    musica_pausa_lectura(1);
    rb = spr_abre("RUTASP.DAT");
    musica_pausa_lectura(0);
    R = rb ? (const rsp_t *)spr_extra(rb, &tam) : NULL;
    if (R && (tam < sizeof *R || memcmp(R->id, "RSP1", 4))) R = NULL;
    if (!R) { dbglog(DBG_ERROR, "FIN: sin RUTASP.DAT\n"); if (rb) spr_cierra(rb); rb = NULL; go_sp = 1; a_game_over(); go_sp = 0; return; }
    spr_suelta_todas();
    dbglog(DBG_INFO, "FIN: modo 25 (450240 = %d), preset %d, modelo %d color %d, ramas %d %d %d %d, nivel %d lado %d razon %.3f, VRAM libre %uK\n",
           c, d->preset, d->modelo, d->color, d->rama[0], d->rama[1], d->rama[2], d->rama[3], d->nivel, d->lado, (double)d->razon,
           (unsigned)(pvr_mem_available() >> 10));
}

int fin_paso(uint32_t nuevo)
{
    int i, r = FIN_SIGUE;
    if (fase < 0) return FIN_SIGUE;
    if (fase == 34) {                                   /* 0x1bc3d8 */
        uint32_t s = millas; int antes = rodando, acabo;
        if (t_panel > n_millas - 1) {                   /* 115230 == 4: la escena ha llegado al final */
            rodando = 0;
            for (i = 0; i < 8; i++) {
                uint32_t d = s % 10; s /= 10;
                if ((uint32_t)(int32_t)cifra[i] < d * 6) { cifra[i]++; rodando = 1; }
            }
        }
        acabo = antes == 1 && !rodando;
        if (acabo) sfx_menu(0x217);                     /* 11c090(0x217) */
        if (--t_temp < 1 || (nuevo & (BOTON_A | BOTON_B))) {   /* 1b4a48, 1b4548(4), 1b4548(8) */
            suma_licencia();                            /* salida del modo 34 (0x1bc618) */
            if (como == FIN_PAUSA_REINTENTA) r = FIN_REINTENTA;   /* 0x1e */
            else if (como == FIN_META) a_continuar();             /* 0x23 */
            else if (como == FIN_PAUSA_ABANDONA) a_game_over();   /* 0x1b */
            else a_continuar();                         /* fin del tiempo: sin red y sin el Time Attack de una etapa
                                                           (48B350: el port corre el curso entero) */
        }
    } else if (fase == 35) {                            /* 0x1bc700 */
        if (t_elige < 0) {
            if (t_preg > n_preg - 1) { if (fd) sube(u_elige); t_elige = 0; }
        } else if (t_elige > n_elige - 1) {
            if (nuevo & BOTON_A) { sfx_menu(SFX_ACEPTA); r = FIN_REINTENTA; }   /* 1b49b8(0x1e), 11b2b0(0x40) */
            if (nuevo & BOTON_B) { sfx_menu(SFX_ACEPTA); r = FIN_MENU; }        /* 1b49b8(0x1c) */
        }
    } else if (fase == 27) {                            /* 0x1bb120 */
        if (--t_temp < 1) r = FIN_MENU;                 /* 1b49b8(0x1c) */
    } else if (fase == 25) {                            /* 0x1ba748 */
        int in = (nuevo & (CONT_START | BOTON_A)) != 0; /* 210c58: 210988(1) START, (4) la cruz */
        if (t_temp > 0) t_temp--;                       /* el temporizador del modo (1b4a48) */
        if (s25 == 0) {                                 /* 118500 / 115ea8: el banco leido */
            ruta_init(); s25 = 1;                       /* 19c870(0x188, 0x15), 288dd8 */
        } else {
            if (s25 == 1) {
                if (in) { sp_siguiente(); in = 0; }
                else if (ru_hecho) s25 = 2;             /* 209d50 */
            }
            if (fase == 25 && s25 == 2 && (t_temp < 1 || in)) sp_siguiente();
            if (fase == 25) ruta_control();             /* el suceso 0x188 (tras el modo) */
        }
        if (fase == 25) spr_paso();
    }
    /* el reloj de las escenas (427F70: un cuadro por paso; modo 5 / 1: se quedan en el ultimo) */
    if (t_panel >= 0) t_panel++;
    if (t_preg >= 0) t_preg++;
    if (t_elige >= 0) t_elige++;
    if (t_go >= 0) t_go++;
    t_fase++;
    if (r != FIN_SIGUE) {
        dbglog(DBG_INFO, "FIN: %s\n", r == FIN_REINTENTA ? "modo 30 (REINTENTAR)" : "modo 28 (al frontal)");
        fin_sale();
    }
    return r;
}

static void dibuja25(void);
void fin_dibuja(void)
{
    int i;
    dibuja25();
    if (fase < 0 || !fd) return;
    if (fase == 34) {
        if (t_panel >= 0) hudd_dibuja(fd, u_millas, t_panel, 0, 0, 1, 130, 0);          /* 2B004C, capa 1 */
        for (i = 0; i < 8; i++) hudd_dibuja(fd, u_cifra[i], cifra[i], 0, 0, 1, 135, 0);  /* 2B004B, capa 5 */
    } else if (fase == 35) {
        if (t_preg >= 0) hudd_dibuja(fd, u_preg, t_preg, 0, 0, 1, 130, 0);
        if (t_elige >= 0) hudd_dibuja(fd, u_elige, t_elige, 0, 0, 1, 131, 0);
    } else if (fase == 27) {
        hudd_dibuja(fd, u_go, t_go, 0, 0, 1, 125, 0);   /* 2C000B, capa 0 */
    }
}
static void dibuja25(void)
{
    if (fase == 25 && rb) spr_dibuja(0, SPR_CAPAS, 120.0f, 0);
}

void fin_sale(void)
{
    if (fase == 27) musica_para();                      /* salida del modo 27 (49AED0): 401030(0) y (1) */
    if (rb) { ruta_suelta(); pvr_wait_ready(); spr_cierra(rb); rb = NULL; R = NULL; }
    if (fd) { hudd_cierra(fd); fd = NULL; }
    fase = -1; t_panel = t_preg = t_elige = t_go = -1;
}

int fin_activo(void) { return fase >= 0; }
