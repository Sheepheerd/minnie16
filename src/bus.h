#ifndef BUS_H
#define BUS_H

#include <stdint.h>

typedef struct {
  uint8_t ram[65536];
} BUS;


BUS* bus_init();

uint16_t bus_read(BUS *bus, uint16_t addr);
void bus_write(BUS *bus, uint16_t addr, uint16_t data);

uint8_t bus_read8(BUS *bus, uint16_t addr);
void bus_write8(BUS *bus, uint16_t addr, uint8_t data);


int bus_load_program(BUS *bus, const char *filename);

#endif
