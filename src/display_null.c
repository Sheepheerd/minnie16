#include "display.h"
#include <stddef.h>

DISPLAY *display_open(int scale) {
  (void)scale;
  return NULL;
}

void display_close(DISPLAY *display) { (void)display; }

int display_present(DISPLAY *display, const uint8_t *fb, uint8_t *keys) {
  (void)display;
  (void)fb;
  *keys = 0;
  return 0;
}
