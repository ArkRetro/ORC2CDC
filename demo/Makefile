# Prueba 0: KOS_BASE y el entorno de KallistiOS (source environ.sh)
TARGET = demo.elf
OBJS = main.o ../comun/gd.o ../comun/musica.o ../comun/or2_fisica.o
all: rm-elf $(TARGET)
include $(KOS_BASE)/Makefile.rules
clean: rm-elf
	-rm -f $(OBJS)
rm-elf:
	-rm -f $(TARGET)
$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)
