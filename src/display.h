#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#include "mmio.h"

DISPLAY *display_open(int scale);
void display_close(DISPLAY *display);
int display_present(DISPLAY *display, const uint8_t *fb, uint8_t *keys);

#endif

