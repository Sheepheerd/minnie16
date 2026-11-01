#include "display.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

#define FRAME_MS 16

struct DISPLAY {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  Uint64 next_frame;
  uint32_t pixels[FB_SIZE];
};

static const uint32_t PALETTE[16] = {
    0xFF000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA, 0xFFAA0000, 0xFFAA00AA,
    0xFFAA5500, 0xFFAAAAAA, 0xFF555555, 0xFF5555FF, 0xFF55FF55, 0xFF55FFFF,
    0xFFFF5555, 0xFFFF55FF, 0xFFFFFF55, 0xFFFFFFFF,
};

DISPLAY *display_open(int scale) {
  SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "Couldn't start SDL: %s\n", SDL_GetError());
    return NULL;
  }

  DISPLAY *d = (DISPLAY *)calloc(1, sizeof(DISPLAY));
  if (d == NULL) {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return NULL;
  }

  d->window = SDL_CreateWindow("minnie16", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, FB_WIDTH * scale,
                               FB_HEIGHT * scale, 0);
  if (d->window != NULL)
    d->renderer = SDL_CreateRenderer(d->window, -1, 0);
  if (d->renderer != NULL)
    d->texture =
        SDL_CreateTexture(d->renderer, SDL_PIXELFORMAT_ARGB8888,
                          SDL_TEXTUREACCESS_STREAMING, FB_WIDTH, FB_HEIGHT);
  if (d->texture == NULL) {
    fprintf(stderr, "Couldn't open a window: %s\n", SDL_GetError());
    display_close(d);
    return NULL;
  }

  d->next_frame = SDL_GetTicks64() + FRAME_MS;
  return d;
}

void display_close(DISPLAY *d) {
  if (d == NULL)
    return;
  if (d->texture)
    SDL_DestroyTexture(d->texture);
  if (d->renderer)
    SDL_DestroyRenderer(d->renderer);
  if (d->window)
    SDL_DestroyWindow(d->window);
  free(d);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static uint8_t read_keys(void) {
  const Uint8 *state = SDL_GetKeyboardState(NULL);
  uint8_t keys = 0;
  if (state[SDL_SCANCODE_W])
    keys |= KEY_W;
  if (state[SDL_SCANCODE_S])
    keys |= KEY_S;
  if (state[SDL_SCANCODE_UP])
    keys |= KEY_UP;
  if (state[SDL_SCANCODE_DOWN])
    keys |= KEY_DOWN;
  if (state[SDL_SCANCODE_SPACE])
    keys |= KEY_SPACE;
  if (state[SDL_SCANCODE_ESCAPE])
    keys |= KEY_ESCAPE;
  return keys;
}

int display_present(DISPLAY *d, const uint8_t *fb, uint8_t *keys) {
  for (int i = 0; i < FB_SIZE; i++)
    d->pixels[i] = PALETTE[fb[i] & 0x0F];

  SDL_UpdateTexture(d->texture, NULL, d->pixels, FB_WIDTH * sizeof(uint32_t));
  SDL_RenderClear(d->renderer);
  SDL_RenderCopy(d->renderer, d->texture, NULL, NULL);
  SDL_RenderPresent(d->renderer);

  Uint64 now = SDL_GetTicks64();
  if (now < d->next_frame)
    SDL_Delay((Uint32)(d->next_frame - now));
  d->next_frame = (now > d->next_frame ? now : d->next_frame) + FRAME_MS;

  int quit = 0;
  SDL_Event event;
  while (SDL_PollEvent(&event))
    if (event.type == SDL_QUIT)
      quit = 1;

  *keys = read_keys();
  return quit ? -1 : 0;
}
