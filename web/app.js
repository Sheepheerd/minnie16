// Page logic for the minnie16 web simulator. The CPU is the C emulator
// compiled to WebAssembly (src/web.c), the assembler is asm.js. This file
// wires them to the editor, the display, the console and the debugger views.

(function () {
  "use strict";

  const PROGRAM_PATH = "/program.bin";
  const STORAGE_KEY = "minnie16.source";
  const DEFAULT_EXAMPLE = "hello.asm";

  // Return values of web_run and web_step, see src/web.c
  const RUN_HALTED = 1;
  const RUN_BREAK = 2;

  const STEPS_PER_FRAME = 400000;
  const FRAME_MS = 1000 / 60;

  const FB_BASE = 0xe000;
  const FB_WIDTH = 64;
  const FB_HEIGHT = 48;
  // Same colors as src/display_sdl.c
  const PALETTE = [
    0x000000, 0x0000aa, 0x00aa00, 0x00aaaa, 0xaa0000, 0xaa00aa, 0xaa5500,
    0xaaaaaa, 0x555555, 0x5555ff, 0x55ff55, 0x55ffff, 0xff5555, 0xff55ff,
    0xffff55, 0xffffff,
  ];
  // Bits of the KEYS register
  const KEYS = {
    KeyW: 0x01,
    KeyS: 0x02,
    ArrowUp: 0x04,
    ArrowDown: 0x08,
    Space: 0x10,
    Escape: 0x20,
  };

  const DISASM_ROWS = 16;
  const MEM_ROWS = 8;
  const TTY_LIMIT = 200000;

  const $ = (id) => document.getElementById(id);
  const el = {
    examples: $("examples"),
    assemble: $("assemble"),
    run: $("run"),
    step: $("step"),
    reset: $("reset"),
    download: $("download"),
    status: $("status"),
    dirty: $("dirty"),
    gutter: $("gutter"),
    source: $("source"),
    screen: $("screen"),
    clear: $("clear"),
    tty: $("tty"),
    ttyin: $("ttyin"),
    counters: $("counters"),
    regs: $("regs"),
    disasm: $("disasm"),
    memaddr: $("memaddr"),
    mem: $("mem"),
  };

  let m = null; // The Emscripten module
  let program = null; // Result of the last successful assemble
  let labelAt = new Map(); // address -> label name
  let running = false;
  let nextFrame = 0;
  let held = 0; // Keys held down on the display
  let disasmTop = 0;
  let memTop = 0;
  let lastRegs = new Array(16).fill(0);
  let errorLine = 0;

  const hex = (v, width) => v.toString(16).toUpperCase().padStart(width, "0");
  const hex4 = (v) => "0x" + hex(v & 0xffff, 4);

  // ---- status line ----

  function setStatus(text, isError) {
    el.status.textContent = text;
    el.status.classList.toggle("error", Boolean(isError));
  }

  function where() {
    const pc = m._web_pc();
    const line = isDirty() ? undefined : program.lines.get(pc);
    return hex4(pc) + (line ? " (line " + line + ")" : "");
  }

  function haltedText() {
    return (
      "Halted at " + where() + " after " + m._web_cycles() + " instructions"
    );
  }

  // ---- editor ----

  function isDirty() {
    return !program || program.source !== el.source.value;
  }

  function lineCount() {
    return el.source.value.split("\n").length;
  }

  function renderGutter() {
    const want = lineCount();
    while (el.gutter.childElementCount < want) {
      const row = document.createElement("div");
      row.textContent = String(el.gutter.childElementCount + 1);
      el.gutter.appendChild(row);
    }
    while (el.gutter.childElementCount > want)
      el.gutter.removeChild(el.gutter.lastChild);

    // Only point at a source line while the source matches what's loaded
    const pcLine =
      m && program && !running && !isDirty()
        ? program.lines.get(m._web_pc())
        : 0;
    for (let i = 0; i < want; i++) {
      const row = el.gutter.children[i];
      row.className = i + 1 === errorLine ? "bad" : i + 1 === pcLine ? "pc" : "";
    }
    el.gutter.scrollTop = el.source.scrollTop;
  }

  // Scrolls the editor so the line is on screen, without moving it otherwise
  function revealLine(line) {
    if (!line) return;
    const height = parseFloat(getComputedStyle(el.source).lineHeight);
    const top = (line - 1) * height;
    const view = el.source.clientHeight;
    if (top < el.source.scrollTop || top + 2 * height > el.source.scrollTop + view)
      el.source.scrollTop = Math.max(0, top - view / 3);
    el.gutter.scrollTop = el.source.scrollTop;
  }

  function sourceChanged() {
    errorLine = 0;
    el.dirty.hidden = !program || !isDirty();
    renderGutter();
    try {
      localStorage.setItem(STORAGE_KEY, el.source.value);
    } catch (e) {
      // Storage can be blocked, the editor still works without it
    }
  }

  function setSource(text) {
    el.source.value = text;
    el.source.scrollTop = 0;
    sourceChanged();
  }

  // Tab moves to the next multiple of 8 columns, like the example programs
  function insertTab(event) {
    if (event.key !== "Tab" || event.shiftKey || event.ctrlKey || event.altKey)
      return;
    event.preventDefault();
    const start = el.source.selectionStart;
    const lineStart = el.source.value.lastIndexOf("\n", start - 1) + 1;
    const spaces = " ".repeat(8 - ((start - lineStart) % 8));
    el.source.setRangeText(spaces, start, el.source.selectionEnd, "end");
    sourceChanged();
  }

  // ---- console ----

  const decoder = new TextDecoder();
  let pending = [];

  function ttyAppend(text, className) {
    if (!text) return;
    const stick =
      el.tty.scrollTop + el.tty.clientHeight >= el.tty.scrollHeight - 4;
    let node = el.tty.lastChild;
    if (!node || (node.className || "") !== (className || "")) {
      node = document.createElement("span");
      if (className) node.className = className;
      el.tty.appendChild(node);
    }
    node.textContent += text;

    // Drop the oldest output once a chatty program fills the console
    while (el.tty.textContent.length > TTY_LIMIT && el.tty.childNodes.length > 1)
      el.tty.removeChild(el.tty.firstChild);
    if (el.tty.textContent.length > TTY_LIMIT)
      node.textContent = node.textContent.slice(-TTY_LIMIT / 2);
    if (stick) el.tty.scrollTop = el.tty.scrollHeight;
  }

  function ttyFlush() {
    if (!pending.length) return;
    ttyAppend(decoder.decode(new Uint8Array(pending), { stream: true }));
    pending = [];
  }

  function ttyClear() {
    pending = [];
    el.tty.textContent = "";
  }

  function ttySend(event) {
    if (!m || !program) return;
    if (event.key === "d" && event.ctrlKey) {
      // Like a terminal, ctrl+d ends the input
      event.preventDefault();
      m._web_tty_eof();
      ttyAppend("^D\n", "echo");
      return;
    }
    if (event.key !== "Enter") return;
    const line = el.ttyin.value + "\n";
    el.ttyin.value = "";
    for (const b of new TextEncoder().encode(line)) m._web_tty_push(b);
    ttyFlush();
    ttyAppend(line, "echo");
  }

  // ---- display ----

  const ctx = el.screen.getContext("2d");
  const frame = ctx.createImageData(FB_WIDTH, FB_HEIGHT);

  function drawScreen() {
    const ram = m._web_ram();
    const fb = m.HEAPU8.subarray(ram + FB_BASE, ram + FB_BASE + FB_WIDTH * FB_HEIGHT);
    const px = frame.data;
    for (let i = 0; i < fb.length; i++) {
      const color = PALETTE[fb[i] & 0x0f];
      px[i * 4] = color >> 16;
      px[i * 4 + 1] = (color >> 8) & 0xff;
      px[i * 4 + 2] = color & 0xff;
      px[i * 4 + 3] = 0xff;
    }
    ctx.putImageData(frame, 0, 0);
  }

  function keyChange(event, down) {
    const bit = KEYS[event.code];
    if (!bit) return;
    event.preventDefault();
    held = down ? held | bit : held & ~bit;
    if (m) m._web_set_keys(held);
  }

  // ---- debugger views ----

  function readRegs() {
    const base = m._web_regs() >> 1;
    return Array.from(m.HEAPU16.subarray(base, base + 16));
  }

  function renderRegs(markChanges) {
    const regs = readRegs();
    if (!el.regs.childElementCount) {
      for (let i = 0; i < 16; i++) {
        const row = document.createElement("div");
        row.innerHTML =
          '<span class="name">r' + i + '</span><span class="hex"></span><span class="dec"></span>';
        el.regs.appendChild(row);
      }
    }
    // Fill column by column so r0..r7 read down the left side
    for (let i = 0; i < 16; i++) {
      const slot = i < 8 ? i * 2 : (i - 8) * 2 + 1;
      const row = el.regs.children[slot];
      row.children[0].textContent = "r" + i;
      row.children[1].textContent = hex4(regs[i]);
      row.children[2].textContent = String((regs[i] << 16) >> 16);
      row.classList.toggle("changed", markChanges && regs[i] !== lastRegs[i]);
    }
    lastRegs = regs;
    el.counters.textContent =
      "pc " + hex4(m._web_pc()) + " · " + m._web_cycles() + " instructions";
  }

  function word(addr) {
    const ram = m._web_ram();
    return m.HEAPU8[ram + (addr & 0xffff)] | (m.HEAPU8[ram + ((addr + 1) & 0xffff)] << 8);
  }

  function renderDisasm(follow) {
    const pc = m._web_pc();
    const span = DISASM_ROWS * 2;
    if (follow && (pc < disasmTop || pc >= disasmTop + span))
      disasmTop = Math.max(0, pc - 8);
    disasmTop = Math.min(Math.max(0, disasmTop & 0xfffe), 0x10000 - span);

    const rows = document.createDocumentFragment();
    for (let i = 0; i < DISASM_ROWS; i++) {
      const addr = disasmTop + i * 2;
      if (labelAt.has(addr)) {
        const label = document.createElement("div");
        label.className = "label";
        label.textContent = labelAt.get(addr) + ":";
        rows.appendChild(label);
      }

      const row = document.createElement("div");
      // The pc moves too fast to follow while the program runs
      row.className = addr === pc && !running ? "row pc" : "row";
      row.dataset.addr = String(addr);

      const cell = (className, text) => {
        const span = document.createElement("span");
        span.className = className;
        span.textContent = text;
        row.appendChild(span);
      };
      cell("bp", m._web_is_break(addr) ? "●" : "");
      cell("addr", hex4(addr));
      cell("word", hex(word(addr), 4));
      cell("text", m.UTF8ToString(m._web_disasm(addr)));
      rows.appendChild(row);
    }
    el.disasm.replaceChildren(rows);
    el.disasm.classList.toggle("stale", running);
  }

  function renderMem() {
    const ram = m._web_ram();
    let text = "";
    for (let r = 0; r < MEM_ROWS; r++) {
      const base = (memTop + r * 16) & 0xffff;
      let bytes = "";
      let chars = "";
      for (let i = 0; i < 16; i++) {
        const b = m.HEAPU8[ram + ((base + i) & 0xffff)];
        bytes += " " + hex(b, 2);
        chars += b >= 0x20 && b < 0x7f ? String.fromCharCode(b) : ".";
      }
      text += hex4(base) + ":" + bytes + "  " + chars + "\n";
    }
    el.mem.textContent = text;
  }

  // Redraws everything after the CPU stopped or took a single step
  function refresh(markChanges) {
    ttyFlush();
    drawScreen();
    renderRegs(markChanges);
    renderDisasm(true);
    renderMem();
    renderGutter();
    if (!isDirty()) revealLine(program.lines.get(m._web_pc()));
  }

  function updateButtons() {
    const loaded = Boolean(m && program);
    el.assemble.disabled = !m;
    el.run.disabled = !loaded;
    el.step.disabled = !loaded || running;
    el.reset.disabled = !loaded;
    el.download.disabled = !loaded;
    el.run.textContent = running ? "Pause" : "Run";
  }

  // ---- running ----

  function stop(text) {
    running = false;
    updateButtons();
    refresh(true);
    if (text) setStatus(text);
  }

  function tick(now) {
    if (!running) return;
    requestAnimationFrame(tick);
    // Programs expect VID_FLIP about 60 times a second, whatever the monitor
    if (now < nextFrame - 2) return;
    nextFrame = Math.max(nextFrame + FRAME_MS, now);

    const status = m._web_run(STEPS_PER_FRAME);
    if (status === RUN_HALTED) return stop(haltedText());
    if (status === RUN_BREAK) return stop("Breakpoint at " + where());

    ttyFlush();
    drawScreen();
    renderRegs(false);
    renderMem();
  }

  function run() {
    if (running) return stop("Paused at " + where());
    if (m._web_halted()) reset();
    running = true;
    nextFrame = 0;
    updateButtons();
    renderDisasm(false);
    renderGutter();
    setStatus("Running");
    requestAnimationFrame(tick);
  }

  function step() {
    if (running) return;
    if (m._web_halted()) return setStatus(haltedText() + ". Reset to start over");
    const status = m._web_step();
    refresh(true);
    if (status === RUN_HALTED) setStatus(haltedText());
    else if (status === RUN_BREAK) setStatus("Breakpoint at " + where());
    else setStatus("Stepped to " + where());
  }

  function reset() {
    running = false;
    m._web_reset();
    ttyClear();
    disasmTop = 0;
    updateButtons();
    refresh(false);
    setStatus("Reset");
  }

  function assemble() {
    if (!m) return;
    let result;
    try {
      result = M16Asm.assemble(el.source.value);
      if (!result.image.length) throw new M16Asm.AsmError("the program is empty", 1);
    } catch (e) {
      if (!(e instanceof M16Asm.AsmError)) throw e;
      errorLine = e.line || 0;
      renderGutter();
      revealLine(errorLine);
      setStatus("line " + errorLine + ": " + e.message, true);
      return;
    }

    running = false;
    m.FS.writeFile(PROGRAM_PATH, result.image);
    if (m._web_load() !== 0) {
      setStatus("The emulator couldn't load the program", true);
      return;
    }

    program = result;
    program.source = el.source.value;
    labelAt = new Map(Object.entries(result.labels).map(([name, addr]) => [addr, name]));
    errorLine = 0;
    el.dirty.hidden = true;
    disasmTop = 0;
    ttyClear();
    updateButtons();
    refresh(false);
    setStatus(
      "Assembled " + result.instructions + " instructions, " +
        result.image.length + " bytes"
    );
  }

  function download() {
    const url = URL.createObjectURL(new Blob([program.image]));
    const link = document.createElement("a");
    link.href = url;
    link.download = "program.bin";
    link.click();
    URL.revokeObjectURL(url);
  }

  // ---- examples ----

  async function loadExample(name) {
    const response = await fetch("programs/" + name);
    if (!response.ok) throw new Error("couldn't fetch " + name);
    setSource(await response.text());
  }

  async function fillExamples() {
    const response = await fetch("programs/index.json");
    const names = response.ok ? await response.json() : [];
    el.examples.replaceChildren(new Option("Pick one", ""));
    for (const name of names) el.examples.add(new Option(name, name));
    el.examples.disabled = !names.length;
    return names;
  }

  async function pickExample() {
    const name = el.examples.value;
    el.examples.value = "";
    if (!name) return;
    try {
      await loadExample(name);
      assemble();
    } catch (e) {
      setStatus(e.message, true);
    }
  }

  // ---- startup ----

  function bind() {
    el.assemble.addEventListener("click", assemble);
    el.run.addEventListener("click", run);
    el.step.addEventListener("click", step);
    el.reset.addEventListener("click", reset);
    el.download.addEventListener("click", download);
    el.clear.addEventListener("click", ttyClear);
    el.examples.addEventListener("change", pickExample);

    el.source.addEventListener("input", sourceChanged);
    el.source.addEventListener("keydown", insertTab);
    el.source.addEventListener("scroll", () => {
      el.gutter.scrollTop = el.source.scrollTop;
    });
    document.addEventListener("keydown", (event) => {
      if (event.key === "Enter" && (event.ctrlKey || event.metaKey)) {
        event.preventDefault();
        assemble();
      }
    });

    el.ttyin.addEventListener("keydown", ttySend);

    el.screen.addEventListener("keydown", (event) => keyChange(event, true));
    el.screen.addEventListener("keyup", (event) => keyChange(event, false));
    el.screen.addEventListener("blur", () => {
      held = 0;
      if (m) m._web_set_keys(0);
    });

    el.disasm.addEventListener("click", (event) => {
      const row = event.target.closest(".row");
      if (!row || !program) return;
      if (m._web_toggle_break(Number(row.dataset.addr)) < 0)
        setStatus("Too many breakpoints", true);
      renderDisasm(false);
    });
    el.disasm.addEventListener(
      "wheel",
      (event) => {
        if (!program) return;
        event.preventDefault();
        disasmTop += event.deltaY > 0 ? 6 : -6;
        renderDisasm(false);
      },
      { passive: false }
    );

    el.memaddr.addEventListener("change", () => {
      const addr = Number(el.memaddr.value.trim());
      if (Number.isInteger(addr) && addr >= 0 && addr <= 0xffff) memTop = addr;
      el.memaddr.value = hex4(memTop);
      if (program) renderMem();
    });
  }

  async function start() {
    bind();

    let saved = null;
    try {
      saved = localStorage.getItem(STORAGE_KEY);
    } catch (e) {
      // No storage, start from the example
    }
    if (saved) setSource(saved);

    const [module, names] = await Promise.all([
      createMinnie({
        onTtyOut: (c) => pending.push(c),
        printErr: (text) => {
          ttyFlush();
          ttyAppend(text + "\n", "err");
        },
      }),
      fillExamples().catch(() => []),
    ]);
    m = module;

    if (!saved && names.includes(DEFAULT_EXAMPLE)) await loadExample(DEFAULT_EXAMPLE);
    updateButtons();
    if (el.source.value.trim()) assemble();
    else setStatus("Write a program, then assemble it");
  }

  start().catch((e) => setStatus("Couldn't start: " + e.message, true));
})();
