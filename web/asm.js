// Assembler for minnie16, a port of tools/m16asm.py that runs in the browser.
// It has to produce the same bytes as the Python one for the same source, so
// keep the two in step. web/test/asm.test.js checks that they agree.

(function (root) {
  "use strict";

  // name: [opcode, format, funct]
  const OPS = {
    ADD: [0x0, "R", 0],
    SUB: [0x1, "R", 0],
    ADDI: [0x2, "I", 0],
    AND: [0x3, "R", 0],
    OR: [0x4, "R", 0],
    SLL: [0x5, "R", 0],
    SLT: [0x6, "R", 0],
    LW: [0x7, "M", 0],
    SW: [0x8, "M", 0],
    LIL: [0x9, "L", 0],
    LIH: [0xa, "L", 0],
    BEQZ: [0xb, "B", 0],
    BNEZ: [0xc, "B", 0],
    JALR: [0xd, "JR", 0],
    JAL: [0xe, "J", 0],
    HALT: [0xf, "X", 0x0],
    XOR: [0xf, "X", 0x1],
    SRL: [0xf, "X", 0x2],
    SRA: [0xf, "X", 0x3],
    SLTS: [0xf, "X", 0x4],
    LB: [0xf, "X", 0x5],
    SB: [0xf, "X", 0x6],
  };

  const MEMORY_SIZE = 0x10000;

  class AsmError extends Error {
    constructor(message, line) {
      super(message);
      this.name = "AsmError";
      this.line = line; // 1-based, filled in by assemble()
    }
  }

  const has = (obj, key) => Object.prototype.hasOwnProperty.call(obj, key);

  // Decimal, 0x hex, 0b binary or 0o octal, with an optional sign
  function number(text) {
    const s = String(text).trim();
    const m =
      /^([+-]?)(?:0[xX]([0-9a-fA-F_]+)|0[bB]([01_]+)|0[oO]([0-7_]+)|([1-9][0-9_]*|0+))$/.exec(
        s
      );
    if (!m) throw new AsmError("not a number or a known label: " + s);
    const digits = (m[2] || m[3] || m[4] || m[5]).replace(/_/g, "");
    const base = m[2] ? 16 : m[3] ? 2 : m[4] ? 8 : 10;
    const v = parseInt(digits, base);
    return m[1] === "-" ? -v : v;
  }

  function operand(args, i) {
    if (i >= args.length) throw new AsmError("missing operand");
    return args[i];
  }

  // "r5" or "(r5)" -> 5
  function reg(text) {
    const s = text.replace(/^[()]+|[()]+$/g, "").toLowerCase();
    const m = /^r(\d+)$/.exec(s);
    if (!m) throw new AsmError("expected register, got " + s);
    const n = parseInt(m[1], 10);
    if (n > 15) throw new AsmError("no register r" + n);
    return n;
  }

  // Labels turn into an instruction offset from the next instruction, plain
  // numbers are used as the offset directly
  function offset(arg, labels, pc) {
    if (has(labels, arg)) return Math.floor((labels[arg] - (pc + 2)) / 2);
    return number(arg);
  }

  function value(arg, labels) {
    if (has(labels, arg)) return labels[arg];
    return number(arg);
  }

  function stripComment(line) {
    let quoted = false;
    let escaped = false;
    for (let i = 0; i < line.length; i++) {
      const c = line[i];
      if (escaped) escaped = false;
      else if (c === "\\" && quoted) escaped = true;
      else if (c === '"') quoted = !quoted;
      else if (c === ";" && !quoted) return line.slice(0, i);
    }
    return line;
  }

  function splitLabel(line) {
    const head = line.split('"', 1)[0];
    const colon = head.indexOf(":");
    if (colon < 0) return [null, line];
    return [line.slice(0, colon).trim(), line.slice(colon + 1).trim()];
  }

  const SIMPLE_ESCAPES = {
    "\\": 0x5c,
    "'": 0x27,
    '"': 0x22,
    a: 0x07,
    b: 0x08,
    f: 0x0c,
    n: 0x0a,
    r: 0x0d,
    t: 0x09,
    v: 0x0b,
  };

  function stringBytes(text) {
    text = text.trim();
    if (text.length < 2 || text[0] !== '"' || text[text.length - 1] !== '"')
      throw new AsmError("expected a quoted string");
    const body = text.slice(1, -1);
    const utf8 = new TextEncoder();
    const out = [];

    for (let i = 0; i < body.length; ) {
      const c = body[i];
      if (c !== "\\") {
        // Take the whole code point so surrogate pairs encode properly
        const ch = String.fromCodePoint(body.codePointAt(i));
        for (const b of utf8.encode(ch)) out.push(b);
        i += ch.length;
        continue;
      }

      const e = body[i + 1];
      if (e === undefined) throw new AsmError("string ends with a lone \\");
      if (has(SIMPLE_ESCAPES, e)) {
        out.push(SIMPLE_ESCAPES[e]);
        i += 2;
      } else if (e === "x") {
        const hex = body.slice(i + 2, i + 4);
        if (!/^[0-9a-fA-F]{2}$/.test(hex))
          throw new AsmError("\\x needs two hex digits");
        out.push(parseInt(hex, 16));
        i += 4;
      } else if (/[0-7]/.test(e)) {
        const oct = /^[0-7]{1,3}/.exec(body.slice(i + 1))[0];
        const v = parseInt(oct, 8);
        if (v > 0xff) throw new AsmError("\\" + oct + " doesn't fit in a byte");
        out.push(v);
        i += 1 + oct.length;
      } else {
        // Unknown escapes are kept as written
        out.push(0x5c);
        i += 1;
      }
    }
    return out;
  }

  function directive(name, rest) {
    if (name === ".ASCIZ") return ["bytes", stringBytes(rest).concat([0])];
    if (name === ".ASCII") return ["bytes", stringBytes(rest)];
    const args = rest.replace(/,/g, " ").split(/\s+/).filter(Boolean);
    if (name === ".BYTE") return ["bytes", args];
    if (name === ".WORD") return ["words", args];
    if (name === ".SPACE") {
      const n = number(operand(args, 0));
      if (n < 0 || n > MEMORY_SIZE) throw new AsmError("bad .space size " + n);
      return ["bytes", new Array(n).fill(0)];
    }
    if (name === ".ALIGN") return ["align", []];
    throw new AsmError("unknown directive " + name);
  }

  // Turns a pseudo-instruction into real ones, real ones pass through
  function expand(name, args) {
    switch (name) {
      case "NOP": // NOP
        return [["ADD", ["r0", "r0", "r0"]]];
      case "MOV": // MOV rd, rs
        return [["ADD", [operand(args, 0), operand(args, 1), "r0"]]];
      case "JMP": // JMP label
        return [["BEQZ", ["r0", operand(args, 0)]]];
      case "CALL": // CALL label
        return [["JAL", [operand(args, 0)]]];
      case "RET": // RET
        return [["JALR", ["r0", "r15"]]];
      case "LI": // LI rd, imm16
        return [
          ["LI_LO", args],
          ["LI_HI", args],
        ];
    }
    return [[name, args]];
  }

  // Strips comments, pulls out labels and gives every instruction an address
  function parse(lines) {
    const labels = {};
    const instrs = [];
    const data = [];
    let pc = 0;

    const pad = (lineno) => {
      if (pc & 1) {
        data.push({ lineno, pc, kind: "bytes", items: [0] });
        pc += 1;
      }
    };

    lines.forEach((raw, index) => {
      const lineno = index + 1;
      try {
        const split = splitLabel(stripComment(raw).trim());
        const label = split[0];
        const line = split[1];

        if (label !== null && !line.startsWith(".")) pad(lineno);
        if (label !== null) labels[label] = pc;
        if (!line) return;

        const space = line.search(/\s/);
        const name = (space < 0 ? line : line.slice(0, space)).toUpperCase();

        if (name.startsWith(".")) {
          const rest = space < 0 ? "" : line.slice(space + 1);
          const [kind, items] = directive(name, rest);
          if (kind === "align" || kind === "words") {
            pad(lineno);
            if (kind === "align") return;
            if (label !== null) labels[label] = pc;
          }
          data.push({ lineno, pc, kind, items });
          pc += items.length * (kind === "words" ? 2 : 1);
          return;
        }

        const args = line.replace(/,/g, " ").split(/\s+/).filter(Boolean);
        const real = expand(name, args.slice(1));
        pad(lineno);
        for (const [op, opArgs] of real) {
          instrs.push({ lineno, pc, name: op, args: opArgs });
          pc += 2;
        }
      } catch (e) {
        if (e instanceof AsmError && e.line === undefined) e.line = lineno;
        throw e;
      }
    });

    return { labels, instrs, data };
  }

  function encode(name, args, labels, pc) {
    if (name === "LI_LO")
      return (
        (OPS.LIL[0] << 12) |
        (reg(operand(args, 0)) << 8) |
        (value(operand(args, 1), labels) & 0xff)
      );
    if (name === "LI_HI")
      return (
        (OPS.LIH[0] << 12) |
        (reg(operand(args, 0)) << 8) |
        ((value(operand(args, 1), labels) >> 8) & 0xff)
      );
    if (!has(OPS, name)) throw new AsmError("unknown instruction " + name);

    const [op, fmt, funct] = OPS[name];
    let word = op << 12;

    if (fmt === "R") {
      // ADD rd, rs1, rs2
      word |=
        (reg(operand(args, 0)) << 8) |
        (reg(operand(args, 1)) << 4) |
        reg(operand(args, 2));
    } else if (fmt === "I") {
      // ADDI rd, rb, imm4
      const imm = number(operand(args, 2));
      if (imm < -8 || imm > 7)
        throw new AsmError("ADDI immediate must be -8..7");
      word |=
        (reg(operand(args, 0)) << 8) | (reg(operand(args, 1)) << 4) | (imm & 0xf);
    } else if (fmt === "M") {
      // LW rd, off(rb), off is in bytes
      const parts = operand(args, 1).split("(");
      if (parts.length !== 2)
        throw new AsmError("expected off(rb), got " + args[1]);
      const off = parts[0] ? number(parts[0]) : 0;
      word |=
        (reg(operand(args, 0)) << 8) | (reg(parts[1]) << 4) | Math.floor(off / 2);
    } else if (fmt === "L") {
      // LIL rd, imm8
      word |= (reg(operand(args, 0)) << 8) | (number(operand(args, 1)) & 0xff);
    } else if (fmt === "B") {
      // BEQZ rd, label
      word |=
        (reg(operand(args, 0)) << 8) |
        (offset(operand(args, 1), labels, pc) & 0xff);
    } else if (fmt === "JR") {
      // JALR rd, rb
      word |= (reg(operand(args, 0)) << 8) | (reg(operand(args, 1)) << 4);
    } else if (fmt === "J") {
      // JAL label
      word |= offset(operand(args, 0), labels, pc) & 0xfff;
    } else if (fmt === "X") {
      // XOR rd, rs / LB rd, (rs) / HALT
      if (args.length)
        word |= (reg(operand(args, 0)) << 8) | (reg(operand(args, 1)) << 4);
      word |= funct;
    }

    return word & 0xffff;
  }

  // Assembles source text. Returns the program image along with the label
  // table and a map from instruction address to source line, or throws an
  // AsmError carrying the line number.
  function assemble(source) {
    const { labels, instrs, data } = parse(String(source).split(/\r?\n/));
    const image = new Uint8Array(MEMORY_SIZE);
    const lines = new Map();
    let size = 0;

    const put = (addr, b) => {
      if (addr >= MEMORY_SIZE)
        throw new AsmError("program is larger than 64 KiB");
      image[addr] = b & 0xff;
      if (addr + 1 > size) size = addr + 1;
    };

    const located = (lineno, fn) => {
      try {
        fn();
      } catch (e) {
        if (e instanceof AsmError && e.line === undefined) e.line = lineno;
        throw e;
      }
    };

    for (const { lineno, pc, name, args } of instrs) {
      located(lineno, () => {
        const w = encode(name, args, labels, pc);
        put(pc, w);
        put(pc + 1, w >> 8);
        lines.set(pc, lineno);
      });
    }

    for (const { lineno, pc, kind, items } of data) {
      located(lineno, () => {
        items.forEach((item, i) => {
          const v = typeof item === "number" ? item : value(item, labels);
          if (kind === "words") {
            put(pc + i * 2, v);
            put(pc + i * 2 + 1, v >> 8);
          } else {
            put(pc + i, v);
          }
        });
      });
    }

    return {
      image: image.slice(0, size),
      labels,
      lines,
      instructions: instrs.length,
    };
  }

  const api = { assemble, AsmError };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.M16Asm = api;
})(typeof self !== "undefined" ? self : this);
