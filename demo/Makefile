# Prueba 0: KOS_BASE y el entorno de KallistiOS (source environ.sh)
TARGET = demo.elf
OBJS = main.o camara.o coli.o perfil.o meta.o fin.o sp.o nombre.o ../comun/gd.o ../comun/musica.o ../comun/or2_fisica.o ../comun/hud.o ../comun/sfx.o ../comun/voces.o ../comun/video.o ../comun/menu.o ../comun/vmu.o ../comun/robot.o ../comun/infarto.o ../comun/modo_video.o ../comun/registro.o ../comun/sprani.o ../comun/fsp.o ../comun/rksp.o
all: rm-elf $(TARGET)
# el HUD y los menus dibujan con pvr_prim: pasan por ta_prim (demo/main.c) para contar sus vertices en el presupuesto
# del TA (docs/DREAMCAST.md)
../comun/hud.o ../comun/menu.o: CFLAGS += -Dpvr_prim=ta_prim
include $(KOS_BASE)/Makefile.rules
clean: rm-elf
	-rm -f $(OBJS)
rm-elf:
	-rm -f $(TARGET)
$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)
