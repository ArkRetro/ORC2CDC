# Registro de la VRAM (`-DDIAG_VRAMLOG`)

Instrumentacion de la memoria de video para medir reservas reales en carrera (Rev 2.6). Sigue el contrato de eventos del
material de investigacion del usuario: CSV `seq,frame,event,id,bytes,asset` con `event` = `alloc`, `free`, `fail` o
`mark`.

## Que se registra

- **pvr_mem** (envolviendo `pvr_mem_malloc` / `pvr_mem_free`, debiles en KallistiOS): `alloc` con el bloque real
  (`request2size` de `pvr_mem_core.c`: tamano + 4 redondeado a 32, minimo 32), `free`, `fail` con lo pedido.
- **Zona del curso** (etapas y uniones): su bloque grande no se registra como reserva; se registra cada textura que se
  coloca dentro (`vram_pide`), con su direccion como `id`, y su `free` cuando se vacia su lado (`tex_suelta`,
  `ar_suelta`, el traslado de la union). La busqueda del mayor bloque libre (reservas de prueba) no se registra.
- **Texturas compartidas** entre union y etapa (`tex_igual`): no son reservas y no se registran.
- **mark**: zona del curso (tamano), fichero leido, etapa nueva, checkpoint.
- **asset**: `FICHERO.DAT:tN` en las subidas de modelos (`sube_modelo`, `sube_tramo`, `vram_ordena`, la union); en lo
  demas `pvr@<direccion de vuelta>`. Para saber que funcion es:
  `sh-elf-addr2line -f -e demo/demo.elf 0x<direccion>` con el mismo `demo.elf`.

No se registra: lo que KallistiOS reserva antes de `main` (bufer de vertices del TA, `pvr_init`), la RAM, ni el
framebuffer. Cada evento se escribe en la consola al momento (sin bufer: no se pierden eventos); la traza hace el juego
algo mas lento, asi que los tiempos se comparan con una ejecucion sin ella.

## Reproducir

```
make -C demo KOS_CFLAGS="$KOS_CFLAGS -DPRUEBA_AUTO -DPRUEBA_SIN_TIEMPO -DDIAG_VRAMLOG"
```

Con el `1ST_READ.BIN` resultante y una carpeta de datos se hace el GD (`tools/gdi.py`) y se ejecuta en Flycast con la
consola a un fichero. Las lineas `VRAMEV,seq,frame,event,id,bytes,asset` del log, con la cabecera
`seq,frame,event,id,bytes,asset`, son la entrada de `AnalizarRegistroVram.ps1` del material del usuario (PowerShell 7
tambien funciona en Linux).

## Revertir

Es un `#ifdef`: compilar sin `-DDIAG_VRAMLOG` deja el juego como antes (comprobado: misma compilacion sin avisos). No
se combina con `-DDIAG_PVRMEM` (los dos envuelven `pvr_mem`; da error de compilacion a proposito).

## Primera captura (Rev 2.6, Flycast, PALM -> ALPI, 150 s)

1020 eventos (704 alloc, 291 free, 25 mark, 0 fail), 0 avisos del analizador. Pico de reservas registradas 1643 KB en
el cuadro 2583, al leer SEG05 con SEG00 aun en la zona. La zona del curso (1280 KB) llego a 599 KB con SEG00 y a 621 KB
con SEG05. Las reservas vivas al final no son fugas: son el HUD, el coche, el trafico y la etapa en uso.
