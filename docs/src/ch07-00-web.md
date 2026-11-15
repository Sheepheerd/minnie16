# Web frontend

The simulator also runs in a browser, with nothing to install. The page has an
editor, the 64 × 48 display, a console, and the debugger views (registers,
disassembly, memory). It is published with GitHub Pages at
<https://sheepheerd.github.io/minnie16/>.

## Using it

1. Write a program, or pick one from the **Example** menu.
2. **Assemble** (or Ctrl+Enter) loads it into the emulator. Errors show the
   line number and mark the line in the editor.
3. **Run** starts it and turns into **Pause**. **Step** executes one
   instruction. **Reset** reloads the program and clears the console.

| Panel | What it does |
|-------|--------------|
| Display | Shows the framebuffer. Click it so it has focus, then W, S, Up, Down, Space and Escape reach the `KEYS` register. |
| Console | Shows `TTY_OUT`. Type in the box under it and press Enter to send a line to `TTY_IN`. Ctrl+D ends the input, which sets the end of file bit in `TTY_STAT`. |
| Registers | A register is highlighted when the last step or run changed it. |
| Disassembly | Follows the pc while paused. Click a line to set or clear a breakpoint, scroll to move through memory. |
| Memory | Type an address in the box to jump there. |

While the program is paused, the line the pc is on is highlighted in the
editor. The highlight goes away once the source is edited, until it is
assembled again.

**Download .bin** saves the assembled image, which the native emulator runs
as it is. The editor contents are kept in the browser's local storage.

## How it is built

The page does not have its own CPU. `web/build.sh` compiles the same C sources
as the native build to WebAssembly with Emscripten, so both always agree.

| File | Role |
|------|------|
| `src/web.c` | Browser host. Stands in for `display_sdl.c` and `tty_posix.c`, and exports the functions the page calls. |
| `web/asm.js` | The assembler, ported from `tools/m16asm.py`. |
| `web/app.js` | The page: editor, display, console and debugger views. |
| `web/index.html`, `web/style.css` | Markup and styling. |

The native emulator blocks inside `VID_FLIP` until the next frame. A browser
tab can't block, so `web_run` returns when the program writes `VID_FLIP`, and
the page calls it again on the next animation frame, at most 60 times a
second. A program that never writes `VID_FLIP` runs 400,000 instructions per
frame.

`web/asm.js` has to produce the same bytes as `tools/m16asm.py`. A change to
one needs the same change in the other, and `web/test/asm.test.js` fails if
they disagree on any of the example programs.

## Building and testing

`nix develop` provides `emcc` and `node`.

```text
web/build.sh                       # writes build/web
python3 -m http.server -d build/web 8000
```

Then open <http://localhost:8000>. The page has to be served over HTTP,
opening `index.html` as a file won't load the WebAssembly module.

```text
node web/test/asm.test.js          # asm.js against m16asm.py
node web/test/sim.test.js          # example programs on the wasm build
```

## Publishing

`.github/workflows/pages.yml` builds the page, runs both tests and deploys to
GitHub Pages on every push to `main`. This needs to be switched on once: in the
repository settings, under **Pages**, set **Source** to **GitHub Actions**.
