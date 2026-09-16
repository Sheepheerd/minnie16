#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BUS *bus_init() {
  BUS *bus = (BUS *)malloc(sizeof(BUS));
  memset(bus->ram, 0, sizeof(bus->ram));
  return bus;
}

// Byte addressable
uint16_t bus_read(BUS *bus, uint16_t addr) {
  uint16_t low = bus->ram[addr];
  uint16_t high = bus->ram[addr + 1];
  return (uint16_t)(low | (high << 8));
}

void bus_write(BUS *bus, uint16_t addr, uint16_t data) {
  bus->ram[addr] = (uint8_t)(data & 0xFF);            // LSB at lower address
  bus->ram[addr + 1] = (uint8_t)((data >> 8) & 0xFF); // MSB at upper address
}

uint8_t bus_read8(BUS *bus, uint16_t addr) { return bus->ram[addr]; }

void bus_write8(BUS *bus, uint16_t addr, uint8_t data) {
  bus->ram[addr] = data;
}

int bus_load_program(BUS *bus, const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    return -1;
  }

  size_t bytesRead = fread(bus->ram, sizeof(uint8_t), 65536, file);
  fclose(file);

  return bytesRead;
}
