/* Prueba 0 del port de OutRun 2006 Coast 2 Coast a Dreamcast: comprueba la cadena entera (compilador, KallistiOS,
   imagen .gdi, GDEMU). Pantalla 640x480 con texto, contador de cuadros y los botones del mando del puerto A. */
#include <kos.h>
#include <dc/biosfont.h>

static void texto(int x, int y, const char *s)
{
    bfont_draw_str(vram_s + y * 640 + x, 640, 1, s);
}

int main(int argc, char **argv)
{
    unsigned n = 0;
    char l[96];
    (void)argc; (void)argv;
    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        maple_device_t *c = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = c ? (cont_state_t *)maple_dev_status(c) : NULL;
        int y;
        vid_waitvbl();
        for (y = 0; y < 480; y++) {
            int x;
            uint16_t col = (uint16_t)(((y * 31 / 480) << 11) | (8 << 5) | 6);   /* degradado de fondo */
            for (x = 0; x < 640; x++) vram_s[y * 640 + x] = col;
        }
        texto(40, 40, "OutRun 2006 C2C - Dreamcast");
        texto(40, 80, "Prueba 0: compilador, KallistiOS, .gdi y GDEMU");
        sprintf(l, "Cuadros: %u", n++);
        texto(40, 140, l);
        if (st) {
            sprintf(l, "Mando A: botones %04x  gatillos L %3d R %3d", (unsigned)st->buttons, st->ltrig, st->rtrig);
            texto(40, 180, l);
            sprintf(l, "Stick: %4d %4d", st->joyx, st->joyy);
            texto(40, 210, l);
        } else texto(40, 180, "Sin mando en el puerto A");
        texto(40, 420, "Uso personal. Sin datos del juego.");
    }
    return 0;
}
