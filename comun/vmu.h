#ifndef VMU_H
#define VMU_H
#include <dc/vmu_pkg.h>
/* Graficos de la VMU (VMU.DAT, tools/conv_vmu.py): icono, paleta y eyecatch del fichero de guardado (lo que ensena el
   gestor de memoria de la BIOS) y la pantalla LCD de 48x32 de las VMU de los mandos (logo en el titulo y los menus,
   velocidad y marcha en carrera). Sin VMU.DAT todo sigue funcionando: icono liso y LCD sin tocar. */
int vmu_inicia(void);                       /* lee VMU.DAT del GD; 0 bien */
/* rellena la cabecera VMS del fichero de guardado: descripciones ("OUTRUN 2006 C2C" / "Licencia OutRun"), app id,
   icono animado y eyecatch (pkg->data y data_len los pone quien guarda) */
void vmu_cabecera(vmu_pkg_t *pkg);
/* que debe ensenar la LCD (se puede llamar en cada cuadro: solo guarda el estado) */
void vmu_lcd_logo(void);                    /* logo del juego (dos imagenes que se alternan) */
void vmu_lcd_carrera(int kmh, int marcha, int marchas);   /* velocidad "%03d" y caja de cambios de 5 o 6 marchas */
/* una vez por cuadro: compone la pantalla y la envia (sin esperar respuesta) a cada VMU con LCD, como mucho unas
   veces por segundo y solo a las que no la tienen ya */
void vmu_lcd_paso(void);
#endif
