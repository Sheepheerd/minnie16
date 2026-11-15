#include "tty.h"
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

void tty_putc(uint8_t c) {
  putchar(c);
  if (c == '\n')
    fflush(stdout);
}

int tty_getc(void) {
  fflush(stdout);
  struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN};
  if (poll(&pfd, 1, 0) <= 0)
    return TTY_NONE;

  uint8_t c;
  if (read(STDIN_FILENO, &c, 1) != 1)
    return TTY_END;
  return c;
}
