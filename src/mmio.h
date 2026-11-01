#ifndef MMIO_H
#define MMIO_H

#include <stdint.h>

#define MMIO_BASE 0xFF00
#define MMIO_TTY_OUT 0xFF00
#define MMIO_TTY_IN 0xFF02
#define MMIO_TTY_STAT 0xFF04
#define MMIO_KEYS 0xFF10
#define MMIO_VID_FLIP 0xFF12
#define MMIO_TICKS 0xFF14

#define TTY_READY 0x01
#define TTY_EOF 0x02

#define KEY_W 0x01
#define KEY_S 0x02
#define KEY_UP 0x04
#define KEY_DOWN 0x08
#define KEY_SPACE 0x10
#define KEY_ESCAPE 0x20

#define FB_BASE 0xE000
#define FB_WIDTH 64
#define FB_HEIGHT 48
#define FB_SIZE (FB_WIDTH * FB_HEIGHT)

typedef struct DISPLAY DISPLAY;

typedef struct {
  int pending;
  int eof;
  uint8_t buffered;
  uint8_t keys;
  uint16_t ticks;
  int headless;
  int display_failed;
  int quit;
  DISPLAY *display;
} MMIO;

void mmio_init(MMIO *mmio);
void mmio_reset(MMIO *mmio);
void mmio_free(MMIO *mmio);

uint8_t mmio_read8(MMIO *mmio, const uint8_t *ram, uint16_t addr);
void mmio_write8(MMIO *mmio, const uint8_t *ram, uint16_t addr, uint8_t data);

#endif

