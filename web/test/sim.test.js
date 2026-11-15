// Runs the example programs on the WebAssembly build of the emulator.
// usage: web/build.sh && node web/test/sim.test.js [build/web]

const fs = require("fs");
const path = require("path");
const { assemble } = require("../asm.js");

const root = path.join(__dirname, "..", "..");
const dist = path.resolve(process.argv[2] || path.join(root, "build", "web"));
const RUN_HALTED = 1;
const RUN_FRAME = 3;
let failed = 0;

function check(name, ok, detail) {
  if (!ok) failed++;
  console.log((ok ? "ok    " : "FAIL  ") + name + (ok ? "" : ": " + detail));
}

async function main() {
  let out = "";
  const m = await require(path.join(dist, "minnie.js"))({
    onTtyOut: (c) => (out += String.fromCharCode(c)),
  });

  const load = (name) => {
    const source = fs.readFileSync(path.join(root, "programs", name), "utf8");
    m.FS.writeFile("/program.bin", assemble(source).image);
    out = "";
    if (m._web_load() !== 0) throw new Error("couldn't load " + name);
  };
  const word = (addr) => m.HEAPU8[m._web_ram() + addr] | (m.HEAPU8[m._web_ram() + addr + 1] << 8);

  load("hello.asm");
  check("hello halts", m._web_run(100000) === RUN_HALTED, "still running");
  check("hello prints", out === "Hello from minnie16!\n", JSON.stringify(out));

  load("example.asm");
  m._web_run(100000);
  check("example sums 1..10", word(0x0100) === 55, word(0x0100));

  load("fib.asm");
  m._web_run(100000);
  const fib = Array.from({ length: 10 }, (_, i) => word(0x0100 + i * 2));
  check("fib", fib.join() === "0,1,1,2,3,5,8,13,21,34", fib.join());

  load("greet.asm");
  m._web_run(100000);
  check("greet waits for input", !m._web_halted(), "halted early");
  for (const c of "Minnie\n") m._web_tty_push(c.charCodeAt(0));
  check("greet halts", m._web_run(100000) === RUN_HALTED, "still running");
  check("greet answers", out === "What's your name? Hello, Minnie!\n", JSON.stringify(out));

  load("pong.asm");
  check("pong shows a frame", m._web_run(5000000) === RUN_FRAME, "no frame");
  const fb = m.HEAPU8.subarray(m._web_ram() + 0xe000, m._web_ram() + 0xec00);
  check("pong draws something", fb.some((p) => p !== 0), "framebuffer is blank");
  m._web_set_keys(0x20); // Escape
  let status = RUN_FRAME;
  for (let i = 0; i < 10 && status === RUN_FRAME; i++) status = m._web_run(5000000);
  check("pong quits on escape", status === RUN_HALTED, "status " + status);

  load("hello.asm");
  m._web_toggle_break(4);
  m._web_run(100000);
  check("breakpoint stops the run", m._web_pc() === 4, "pc " + m._web_pc());
  check("disassembly", m.UTF8ToString(m._web_disasm(4)) === "LIL  r1, 0x14", m.UTF8ToString(m._web_disasm(4)));
  m._web_reset();
  check("reset", m._web_pc() === 0 && m._web_cycles() === 0, "pc " + m._web_pc());

  process.exit(failed ? 1 : 0);
}

main();
