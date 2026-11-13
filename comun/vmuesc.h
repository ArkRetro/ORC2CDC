#ifndef VMUESC_H
#define VMUESC_H
/* Escritura propia de ficheros en la VMU (sobre tramas maple, sin vmufs_write) y pantalla de diagnostico de la VMU
   (revision 44; docs/DREAMCAST.md, "Guardado en la VMU"). */
#include <dc/maple.h>

/* codigos de resultado (vmuesc_escribe; 0 bien) */
#define VE_SIN_VMU    -1    /* no hay ninguna VMU (unidad con memoria) */
#define VE_MEMORIA    -2    /* sin memoria en la consola */
#define VE_FORMATO    -3    /* la raiz no es de una VMU formateada (0x55, FAT de 1 bloque, tamanos) o FAT rota */
#define VE_SITIO      -7    /* no hay bloques libres suficientes */
#define VE_DIR_LLENO  -8    /* no queda ninguna entrada libre en el directorio */
#define VE_RAIZ      -10    /* no se puede leer el bloque raiz (255) */
#define VE_FAT       -11    /* no se puede leer la FAT */
#define VE_DIR       -12    /* no se puede leer el directorio */
#define VE_BLOQUE    -13    /* no se puede escribir un bloque de datos (tras los reintentos) */
#define VE_GRABA_FAT -14    /* no se puede escribir la FAT */
#define VE_GRABA_DIR -15    /* no se puede escribir el directorio */
#define VE_DISTINTO  -20    /* al releer el fichero entero no coincide */
#define VE_RELEE     -21    /* no se puede releer el fichero */
#define VE_IRQ       -30    /* llamado desde una interrupcion (no puede esperar a las tramas) */

/* respuestas maple que no son de la VMU (campo maple): las demas son MAPLE_RESPONSE_* (-1 sin respuesta, -2 funcion
   mala, -3 orden mala, -4 otra vez, -5 error de fichero, 7 bien, 8 datos) */
#define VE_M_TIEMPO  99     /* la trama no volvio en 1 s */
#define VE_M_DISTINTO 98    /* el bloque se leyo bien pero no es lo escrito */
#define VE_M_RARO    97     /* respuesta de datos que no es la del bloque pedido */

typedef struct {
    int cod;                /* VE_* (0 bien) */
    char fase;              /* donde fallo: R raiz, T FAT, D directorio, S sitio, B bloques, F grabar FAT,
                               E grabar directorio, V relectura final, '-' nada */
    int intento;            /* intento del guardado entero (lo pone quien llama) */
    int libres;             /* bloques libres de la FAT antes de guardar (-1: sin leer) */
    int viejo;              /* bloques del fichero que ya habia (0: no habia) */
    int necesarios;         /* bloques del fichero */
    int n_vmu;              /* VMU (unidades con memoria) enchufadas (lo pone quien llama) */
    int puerto, unidad;     /* unidad usada (-1 ninguna) */
    int maple;              /* ultima respuesta maple mala (o VE_M_*) */
    int paso;               /* trama que fallo: 0..3 fase de escritura, 4 confirmacion, 5 lectura, 6 comparacion */
    int bloque;             /* bloque en el que fallo (-1) */
    int reintentos;         /* repeticiones de bloques (tambien si al final fue bien) */
    int ms;                 /* lo que tardo */
} vmuesc_diag_t;

/* escribe el fichero nombre (nb bytes, multiplo de 512) en dev: bloques de datos en bloques libres (si no bastan, los
   del fichero viejo), cada bloque con 4 fases + confirmacion, releido y comparado (6 intentos con pausas crecientes),
   despues la FAT y la entrada del directorio, y al final se relee el fichero entero. Bloquea (unos segundos): llamar
   desde un hilo, nunca desde una interrupcion. Rellena d (salvo intento y n_vmu). */
int vmuesc_escribe(maple_device_t *dev, const char *nombre, const uint8_t *buf, int nb, vmuesc_diag_t *d);
/* codigo corto para la pantalla con la letra del menu (mayusculas, cifras, '-' y ':'):
   "E-13 FB I1 L180 N20 U1 PA1 M-1 W0 K199 R12" */
void vmuesc_codigo(const vmuesc_diag_t *d, char *s, int n);
/* pantalla de diagnostico (letra de la BIOS sobre la imagen que se ve): unidades, raiz, FAT, directorio, prueba de
   escritura en un bloque libre (restaurado) con la ruta propia y con vmu_block_write de KallistiOS, tiempos y
   respuestas maple de cada trama. Bloquea hasta que se pulsa A, B o START. dev: la VMU que usaria el guardado */
void vmuesc_pantalla(maple_device_t *dev, const vmuesc_diag_t *ultimo);
void vmuesc_letra(void);    /* copia la letra de la BIOS (al arrancar, con el GD libre) */
#endif
