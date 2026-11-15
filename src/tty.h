#ifndef TTY_H
#define TTY_H

#include <stdint.h>

#define TTY_NONE (-1) // Nothing waiting right now
#define TTY_END (-2)  // Input reached end of file

// Host side of the terminal device. tty_posix.c talks to stdin and stdout,
// web.c talks to the page.
void tty_putc(uint8_t c);

// Never blocks. Returns the next input byte, TTY_NONE or TTY_END.
int tty_getc(void);

#endif
