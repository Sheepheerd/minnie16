// Checks that web/asm.js and tools/m16asm.py produce the same image for every
// example program, plus a few lines that exercise the corners.
// usage: node web/test/asm.test.js

const { execFileSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");
const { assemble, AsmError } = require("../asm.js");

const root = path.join(__dirname, "..", "..");
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "m16asm-"));
let failed = 0;

function python(source) {
  const src = path.join(tmp, "in.asm");
  const out = path.join(tmp, "out.bin");
  fs.writeFileSync(src, source);
  execFileSync("python3", [path.join(root, "tools", "m16asm.py"), src, out]);
  return fs.readFileSync(out);
}

function same(name, source) {
  const want = python(source);
  const got = Buffer.from(assemble(source).image);
  if (want.equals(got)) {
    console.log("ok    " + name);
  } else {
    failed++;
    console.log("FAIL  " + name + ": images differ");
  }
}

function rejects(name, source, line) {
  try {
    assemble(source);
  } catch (e) {
    if (e instanceof AsmError && e.line === line) {
      console.log("ok    " + name);
      return;
    }
  }
  failed++;
  console.log("FAIL  " + name + ": expected an error on line " + line);
}

const programs = path.join(root, "programs");
for (const file of fs.readdirSync(programs).sort()) {
  if (file.endsWith(".asm"))
    same(file, fs.readFileSync(path.join(programs, file), "utf8"));
}

same(
  "directives and padding",
  [
    "a: .byte 1, 2, 0x41",
    "b: .word 0x1234, b, end",
    '   .ascii "x;y\\n\\t\\"\\\\\\x7f"',
    "c: .asciz \"odd\"",
    "   .align",
    "d: .byte 0b101",
    "e: NOP",
    "   .space 3",
    "end: HALT",
  ].join("\n")
);
same(
  "offsets and immediates",
  [
    "top: BEQZ r0, -1",
    "     BNEZ r1, top",
    "     JAL  2",
    "     CALL top",
    "     LW   r1, (r2)",
    "     SW   r1, 30(r2)",
    "     ADDI r3, r3, -8",
    "     LI   r4, 0xBEEF",
    "     LI   r5, top",
    "     LIL  r6, -1",
    "     mov  r7 r8",
    "     JALR r1, r2",
    "     RET",
  ].join("\n")
);

rejects("unknown instruction", "NOP\nNOP\nFOO r1", 3);
rejects("ADDI range", "ADDI r1, r1, 8", 1);
rejects("bad register", "NOP\nADD r16, r0, r0", 2);
rejects("undefined label", "JMP nowhere", 1);

fs.rmSync(tmp, { recursive: true });
process.exit(failed ? 1 : 0);
