#ifndef MENU_H
#define MENU_H
/* Menus del modo principal (MENU.DAT, tools/conv_menu.py): titulo -> menu principal -> un jugador -> coche ->
   cambio -> musica -> carrera, con las animaciones del original. Se dibuja en la lista TR a 640x480. */
enum { MENU_SIGUE, MENU_CARRERA, MENU_DEMO };
int menu_inicia(void);                      /* abre MENU.DAT (solo tablas en la RAM); 0 bien */
/* entra en el titulo: texturas a la VRAM bajo demanda (antes hay que dejar el hueco: hud_baja_tex) */
void menu_entra(int manual, int pista, int npistas);
int menu_paso(unsigned botones_nuevos);     /* una vez por paso de logica (60 Hz); MENU_CARRERA / MENU_DEMO al acabar */
void menu_dibuja(void);                     /* dentro de la lista TR */
void menu_sale(void);                       /* libera toda la VRAM del menu */
int menu_manual(void);                      /* cambio elegido: 0 AT, 1 MT */
int menu_pista(void);                       /* musica elegida */
int menu_coche3d(void);                     /* 1 si hay que dibujar el coche 3D (seleccion de coche, ficha con coche y
                                               quieta) */
int menu_coche(void);                       /* ficha del coche elegido (0..14, orden del menu) */
int menu_color(void);                       /* color elegido de esa ficha: 1..8 (car+0x12 del original; boton Y) */
void menu_coches(unsigned mascara);         /* fichas que tienen coche (bit k = COCHEkk.DAT); antes de menu_entra */
unsigned menu_vram(void);
void menu_libera_vram(void);                /* echa las pantallas guardadas que no hacen falta ahora */                   /* bytes de VRAM del menu en uso */
#endif
