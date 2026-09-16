#include <bus.h>
#include <cpu.h>
#include <stdio.h>

int main(void) {

  CPU *cpu = cpu_init();
  BUS *bus = bus_init();

  if (!bus_load_program(bus, "program.bin")) {
    return -1;
  }

  while (!cpu->halted) {
    cpu_step(cpu, bus, 1);
  }

  // Debug the memeory
  while (1) {
    // ask the user what memeory address they want to see
    printf("Enter memory address to view (hex): ");
    int address;
    scanf("%x", &address);
    printf("Memory at 0x%04X: 0x%02X\n", address, bus_read(bus, address));
  }
  return 0;
}
