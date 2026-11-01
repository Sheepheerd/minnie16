#include "mmio.h"
#include "display.h"
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

#define DISPLAY_SCALE 10

void mmio_init(MMIO *mmio) {
  mmio->headless = 0;
  mmio->display_failed = 0;
  mmio->display = NULL;
  mmio_reset(mmio);
}

void mmio_reset(MMIO *mmio) {
  mmio->pending = 0;
  mmio->eof = 0;
  mmio->buffered = 0;
  mmio->keys = 0;
  mmio->ticks = 0;
  mmio->quit = 0;
}

void mmio_free(MMIO *mmio) {
  display_close(mmio->display);
  mmio->display = NULL;
}

static uint8_t tty_status(MMIO *mmio) {
  if (!mmio->pending && !mmio->eof) {
    fflush(stdout);
    struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN};
    if (poll(&pfd, 1, 0) > 0) {
      uint8_t c;
      ssize_t n = read(STDIN_FILENO, &c, 1);
      if (n == 1) {
        mmio->buffered = c;
        mmio->pending = 1;
      } else {
        mmio->eof = 1;
      }
    }
  }
  if (mmio->pending)
    return TTY_READY;
  return mmio->eof ? TTY_EOF : 0;
}

static uint8_t tty_read(MMIO *mmio) {
  if (!(tty_status(mmio) & TTY_READY))
    return 0;
  mmio->pending = 0;
  return mmio->buffered;
}

static void vid_flip(MMIO *mmio, const uint8_t *ram) {
  mmio->ticks++;
  if (mmio->headless || mmio->display_failed)
    return;

  if (mmio->display == NULL) {
    mmio->display = display_open(DISPLAY_SCALE);
    if (mmio->display == NULL) {
      mmio->display_failed = 1;
      return;
    }
  }

  fflush(stdout);
  if (display_present(mmio->display, ram + FB_BASE, &mmio->keys) != 0)
    mmio->quit = 1;
}

uint8_t mmio_read8(MMIO *mmio, const uint8_t *ram, uint16_t addr) {
  (void)ram;
  switch (addr) {
  case MMIO_TTY_IN:
    return tty_read(mmio);
  case MMIO_TTY_STAT:
    return tty_status(mmio);
  case MMIO_KEYS:
    return mmio->keys;
  case MMIO_TICKS:
    return mmio->ticks & 0xFF;
  case MMIO_TICKS + 1:
    return mmio->ticks >> 8;
  default:
    return 0;
  }
}

void mmio_write8(MMIO *mmio, const uint8_t *ram, uint16_t addr, uint8_t data) {
  switch (addr) {
  case MMIO_TTY_OUT:
    putchar(data);
    if (data == '\n')
      fflush(stdout);
    break;
  case MMIO_VID_FLIP:
    vid_flip(mmio, ram);
    break;
  default:
    break;
  }
}
