#include <bus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BUS *bus_init() {
  BUS *bus = (BUS *)malloc(sizeof(BUS));
  if (bus == NULL) {
    return NULL;
  }
  memset(bus->ram, 0, sizeof(bus->ram));
  return bus;
}

// Byte addressable. The high byte of a word at 0xFFFF wraps around to 0x0000.
uint16_t bus_read(BUS *bus, uint16_t addr) {
  uint16_t low = bus->ram[addr];
  uint16_t high = bus->ram[(uint16_t)(addr + 1)];
  return (uint16_t)(low | (high << 8));
}

void bus_write(BUS *bus, uint16_t addr, uint16_t data) {
  bus->ram[addr] = (uint8_t)(data & 0xFF);            // LSB at lower address
  bus->ram[(uint16_t)(addr + 1)] = (uint8_t)((data >> 8) & 0xFF); // MSB at upper address
}

uint8_t bus_read8(BUS *bus, uint16_t addr) { return bus->ram[addr]; }

void bus_write8(BUS *bus, uint16_t addr, uint8_t data) {
  bus->ram[addr] = data;
}

// Clears memory and loads a raw program image at 0x0000. Returns the number of
// bytes loaded, or -1 if the file can't be read or doesn't fit in memory.
int bus_load_program(BUS *bus, const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    return -1;
  }

  memset(bus->ram, 0, sizeof(bus->ram));
  size_t bytesRead = fread(bus->ram, sizeof(uint8_t), sizeof(bus->ram), file);
  int too_big = fgetc(file) != EOF;
  fclose(file);

  if (too_big) {
    return -1;
  }
  return (int)bytesRead;
}
