#ifndef MENU_H
#define MENU_H
#include <stdint.h>
/* Menus del modo principal (MENU.DAT, tools/conv_menu.py) con el flujo del original:
     titulo -> (licencia: elegir / crear / editar) -> menu principal -> un jugador -> coche -> cambio -> musica ->
     carrera; menu principal -> OUTRUN2SP, rankings, opciones (sin multijugador); LICENSE (X) desde los menus.
   Se dibuja en la lista TR a 640x480. Las licencias y opciones se guardan en la VMU (A1). */
enum { MENU_SIGUE, MENU_CARRERA, MENU_DEMO };
int menu_precarga(void);                                 /* las tablas de MENU.DAT, lo primero al arrancar */
int menu_tablas(int en);   /* 0: al empezar la carrera suelta las piezas que no son de la pausa (bytes); 1: al volver al menu las lee */
/* pantalla de carga propia al empezar un modo (demo/pcarga.c; ver menu.c): sube sus texturas (0 bien), la dibuja
   (dentro de la lista TR; avance 0..1, la barra), las baja / vuelve a subir y la quita (VRAM y las piezas que
   menu_tablas(0) le dejo) */
int menu_carga_entra(void);
void menu_carga_dibuja(float avance);
void menu_carga_baja(void);
int menu_carga_sube(void);
void menu_carga_sale(void);
int menu_inicia(void);                      /* abre MENU.DAT (solo tablas en la RAM) y lee la VMU; 0 bien */
/* entra en el titulo: texturas a la VRAM bajo demanda (antes hay que dejar el hueco: hud_baja_tex) */
void menu_entra(int manual, int pista, int npistas);
void menu_suma_tiempo(int cuadros);           /* licencia +0x10C += crono total de la carrera (4505D0) */
void menu_suma_millas(uint32_t millas);       /* licencia +0x24 += millas OutRun de la carrera (4EF510) */
int menu_paso(unsigned botones_nuevos);     /* una vez por paso de logica (60 Hz); MENU_CARRERA / MENU_DEMO al acabar */
void menu_dibuja(void);                     /* dentro de la lista TR */
void menu_sale(void);                       /* libera toda la VRAM del menu */
int menu_modo(void);                        /* variante del modo: 1 OutRun, 2 Heart Attack (INFARTO), 7 Time Attack */
/* COAST 2 COAST (variante 6): las misiones que se pueden elegir (indice de Races.bin, clave y subclave), la elegida
   (-1: otro modo) y la nota de la mision en la licencia (495A20) */
void menu_pon_misiones(int n, const int16_t *idx, const uint8_t *clave, const uint8_t *sub);
int menu_mision(void);
void menu_nota_mision(int clave, int sub, int nota);
int menu_nota_de(int clave, int sub);         /* licencia +0x1D7 + 5 clave + subclave (7 sin jugar) */
void menu_elige_mision(int idx);             /* 495A60: la mision siguiente para REINTENTAR */
int menu_sp(void);                          /* 1: OUTRUN2SP del menu principal ([780260] = 0, frontal de recreativa) */
void menu_entra_sp(void);                   /* OUTRUN2SP tras la carrera: el frontal de recreativa (49AF90: modo 10) */
int menu_preset(void);                      /* OUTRUN2SP: preset del curso (43F950: 0 OutRun 2, 1 OutRun 2 SP) */
int menu_manual(void);                      /* cambio elegido: 0 AT, 1 MT */
int menu_pista(void);                       /* musica elegida */
int menu_coche3d(void);                     /* 1 si hay que dibujar el coche 3D (seleccion de coche, ficha quieta) */
int menu_coche(void);                       /* COCHEnn.DAT del coche elegido (nn, o -1 si su modelo no esta) */
int menu_modelo(void);                      /* modelo del original (car+0x11, 0..29) del coche elegido */
int menu_color(void);                       /* color elegido de esa ficha: 1..8 (car+0x12 del original; boton Y) */
float menu_giro(void);                      /* giro del coche de la seleccion (radianes, +100/65536 de vuelta por paso) */
void menu_coches(unsigned mascara);         /* (antiguo) fichas con coche; ver menu_fichas */
void menu_fichas(const uint8_t *ficha_modelo, int n);   /* modelo de cada COCHEnn.DAT (0xFF si no hay) */
unsigned menu_vram(void);                   /* bytes de VRAM del menu en uso */
void menu_libera_vram(void);                /* echa las pantallas guardadas que no hacen falta ahora */
/* opciones de la licencia activa (las del original: +D8 vista, +DC unidades, +E0 dificultad) */
int menu_vista(void);                       /* 0 parachoques, 1 interior, 2 exterior */
int menu_mph(void);                         /* 0 km/h, 1 mph */
int menu_dificultad(void);                  /* 0 muy facil .. 4 muy dificil (licencia +E0, TIM_DIF) */
const uint8_t *menu_licencia(void);         /* la licencia activa (registro de 0x40C) o NULL */
void menu_guarda_vmu(void);                 /* licencias y rankings de OUTRUN2SP a la VMU */
int menu_foto(void);                        /* licencia +0x20 (la foto: PC [7C2400], el piloto y la acompanante) */
/* ganchos para otros modulos (voces del locutor, video): se llaman al entrar en cada pantalla (evento 0) y al elegir
   una opcion (evento 1, valor = opcion); NULL = nada */
enum { MP_TITULO, MP_MENU, MP_MODO, MP_COCHE, MP_CAMBIO, MP_MUSICA, MP_LICENCIA, MP_EDITOR, MP_OPCIONES, MP_RANKING,
       MP_MULTI };
extern void (*menu_gancho)(int pantalla, int evento, int valor);
/* lo pone quien dibuja el coche 3D de la seleccion: libera su VRAM (se vuelve a cargar cuando haga falta). El menu lo
   llama cuando una pantalla no cabe: las texturas del coche quedan en medio de la VRAM del menu y la parten (la
   pantalla de musica, 3 texturas de 256 KB, no encontraba hueco seguido) */
extern void (*menu_suelta_coche)(void);
/* volumen de efectos y de musica de la licencia activa (+D4, +D0; 0..10) al controlador de sonido y a la musica */
void menu_aplica_audio(void);
/* menu de pausa de la carrera (ver menu.c): menu_pausa_entra sube sus texturas (0 bien; si no, la pausa sigue sin
   dibujarse); menu_pausa_paso una vez por paso de logica en pausa; menu_pausa_dibuja dentro de la lista TR, encima de
   todo; menu_pausa_sale libera su VRAM */
enum { PAUSA_SIGUE, PAUSA_CONTINUA, PAUSA_REINTENTA, PAUSA_ABANDONA };
int menu_pausa_entra(void);
int menu_pausa_paso(unsigned botones_nuevos);
void menu_pausa_dibuja(void);
void menu_pausa_sale(void);
unsigned menu_pausa_vram(void);                          /* VRAM que pedira la pausa (bytes; 0 si ya esta subida) */
#endif
