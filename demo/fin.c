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
   - OUTRUN2SP (1b4878 = 0): ABANDONAR de la pausa va directo al modo 27 (fin_game_over) con la misma escena 2C000B.
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
static void a_game_over(void)                           /* 0x1bafc8 */
{
    baja(u_millas); baja(u_cifra[0]); t_panel = -1;
    fase = 27; t_fase = 0; t_temp = M27_CUADROS; voz_go = 1;
    dbglog(DBG_INFO, "FIN: modo 27 (GAME OVER)\n");
    abre("GOVER.DAT");
    u_go = fd ? hudd_uso(fd, "GAME_OVER") : -1;
    n_go = fd && u_go >= 0 ? hudd_cuadros(fd, u_go) : 1;
    if (fd) sube(u_go);
    t_go = 0;                                           /* 0x1a04f0: el GAD crea la escena y dice la voz una vez */
    if (voz_go) { voz_go = 0; sfx_voz(0x954); }
}

/* OUTRUN2SP (1b4878 = 0): ABANDONAR de la pausa pide el modo 27 directamente (0x24a150, 450230(7)); con 450240 = 7 el GAD
   crea la misma escena 2C000B que la consola (0x1a051c; 34001F es la de los demas finales de este juego) */
void fin_game_over(void)
{
    int i;
    hud_baja_tex(); hudd_vacia();
    u_millas = u_preg = u_elige = -1;
    for (i = 0; i < 8; i++) u_cifra[i] = -1;
    como = FIN_PAUSA_ABANDONA;
    a_game_over();
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

void fin_dibuja(void)
{
    int i;
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

void fin_sale(void)
{
    if (fd) { hudd_cierra(fd); fd = NULL; }
    fase = -1; t_panel = t_preg = t_elige = t_go = -1;
}

int fin_activo(void) { return fase >= 0; }
