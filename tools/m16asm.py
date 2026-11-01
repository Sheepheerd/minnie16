# Simple assembler for minnie16
# usage: python3 m16asm.py input.asm [output.bin]

import struct
import sys

# name: (opcode, format, funct)
OPS = {
    "ADD":  (0x0, "R", 0),
    "SUB":  (0x1, "R", 0),
    "ADDI": (0x2, "I", 0),
    "AND":  (0x3, "R", 0),
    "OR":   (0x4, "R", 0),
    "SLL":  (0x5, "R", 0),
    "SLT":  (0x6, "R", 0),
    "LW":   (0x7, "M", 0),
    "SW":   (0x8, "M", 0),
    "LIL":  (0x9, "L", 0),
    "LIH":  (0xA, "L", 0),
    "BEQZ": (0xB, "B", 0),
    "BNEZ": (0xC, "B", 0),
    "JALR": (0xD, "JR", 0),
    "JAL":  (0xE, "J", 0),
    "HALT": (0xF, "X", 0x0),
    "XOR":  (0xF, "X", 0x1),
    "SRL":  (0xF, "X", 0x2),
    "SRA":  (0xF, "X", 0x3),
    "SLTS": (0xF, "X", 0x4),
    "LB":   (0xF, "X", 0x5),
    "SB":   (0xF, "X", 0x6),
}


def reg(s):
    # "r5" or "(r5)" -> 5
    s = s.strip("()").lower()
    if not s.startswith("r"):
        raise ValueError("expected register, got " + s)
    n = int(s[1:])
    if n > 15:
        raise ValueError("no register r%d" % n)
    return n


def offset(arg, labels, pc):
    # labels get turned into an instruction offset from the next instruction,
    # plain numbers are used as the offset directly
    if arg in labels:
        return (labels[arg] - (pc + 2)) // 2
    return int(arg, 0)


def value(arg, labels):
    if arg in labels:
        return labels[arg]
    return int(arg, 0)


def strip_comment(line):
    quoted = False
    escaped = False
    for i, c in enumerate(line):
        if escaped:
            escaped = False
        elif c == "\\" and quoted:
            escaped = True
        elif c == '"':
            quoted = not quoted
        elif c == ";" and not quoted:
            return line[:i]
    return line


def split_label(line):
    head = line.split('"', 1)[0]
    if ":" not in head:
        return None, line
    label, rest = line.split(":", 1)
    return label.strip(), rest.strip()


def string_bytes(text):
    text = text.strip()
    if len(text) < 2 or text[0] != '"' or text[-1] != '"':
        raise ValueError("expected a quoted string")
    return list(text[1:-1].encode().decode("unicode_escape").encode("latin-1"))


def directive(name, rest):
    if name == ".ASCIZ":
        return ("bytes", string_bytes(rest) + [0])
    if name == ".ASCII":
        return ("bytes", string_bytes(rest))
    args = rest.replace(",", " ").split()
    if name == ".BYTE":
        return ("bytes", args)
    if name == ".WORD":
        return ("words", args)
    if name == ".SPACE":
        return ("bytes", [0] * int(args[0], 0))
    if name == ".ALIGN":
        return ("align", [])
    raise ValueError("unknown directive " + name)


def expand(name, args):
    # turn a pseudo-instruction into real ones, real ones pass through
    if name == "NOP":   # NOP
        return [("ADD", ["r0", "r0", "r0"])]
    if name == "MOV":   # MOV rd, rs
        return [("ADD", [args[0], args[1], "r0"])]
    if name == "JMP":   # JMP label
        return [("BEQZ", ["r0", args[0]])]
    if name == "CALL":  # CALL label
        return [("JAL", [args[0]])]
    if name == "RET":   # RET
        return [("JALR", ["r0", "r15"])]
    if name == "LI":    # LI rd, imm16
        return [("LI_LO", args), ("LI_HI", args)]
    return [(name, args)]


def parse(lines):
    # strip comments, pull out labels, give every instruction an address
    labels = {}
    instrs = []
    data = []
    pc = 0
    for lineno, line in enumerate(lines, 1):
        line = strip_comment(line).strip()
        label, line = split_label(line)
        if label is not None and not line.startswith("."):
            if pc & 1:
                data.append((lineno, pc, "bytes", [0]))
                pc += 1
            labels[label] = pc
        elif label is not None:
            labels[label] = pc
        if not line:
            continue
        parts = line.split(None, 1)
        name = parts[0].upper()
        try:
            if name.startswith("."):
                kind, items = directive(name, parts[1] if len(parts) > 1 else "")
                if kind == "align" or kind == "words":
                    if pc & 1:
                        data.append((lineno, pc, "bytes", [0]))
                        pc += 1
                    if kind == "align":
                        continue
                    if label is not None:
                        labels[label] = pc
                data.append((lineno, pc, kind, items))
                pc += len(items) * (2 if kind == "words" else 1)
                continue
            real = expand(name, line.replace(",", " ").split()[1:])
        except (ValueError, IndexError) as e:
            print("line %d: error: %s" % (lineno, e))
            sys.exit(1)
        if pc & 1:
            data.append((lineno, pc, "bytes", [0]))
            pc += 1
        for name, args in real:
            instrs.append((lineno, pc, name, args))
            pc += 2
    return labels, instrs, data


def encode(name, args, labels, pc):
    if name == "LI_LO":
        return OPS["LIL"][0] << 12 | reg(args[0]) << 8 | (value(args[1], labels) & 0xFF)
    if name == "LI_HI":
        return OPS["LIH"][0] << 12 | reg(args[0]) << 8 | (value(args[1], labels) >> 8 & 0xFF)
    if name not in OPS:
        raise ValueError("unknown instruction " + name)
    op, fmt, funct = OPS[name]
    word = op << 12

    if fmt == "R":    # ADD rd, rs1, rs2
        word |= reg(args[0]) << 8 | reg(args[1]) << 4 | reg(args[2])
    elif fmt == "I":  # ADDI rd, rb, imm4
        imm = int(args[2], 0)
        if imm < -8 or imm > 7:
            raise ValueError("ADDI immediate must be -8..7")
        word |= reg(args[0]) << 8 | reg(args[1]) << 4 | (imm & 0xF)
    elif fmt == "M":  # LW rd, off(rb), off is in bytes
        off, rb = args[1].split("(")
        off = int(off, 0) if off else 0
        word |= reg(args[0]) << 8 | reg(rb) << 4 | (off // 2)
    elif fmt == "L":  # LIL rd, imm8
        word |= reg(args[0]) << 8 | (int(args[1], 0) & 0xFF)
    elif fmt == "B":  # BEQZ rd, label
        word |= reg(args[0]) << 8 | (offset(args[1], labels, pc) & 0xFF)
    elif fmt == "JR": # JALR rd, rb
        word |= reg(args[0]) << 8 | reg(args[1]) << 4
    elif fmt == "J":  # JAL label
        word |= offset(args[0], labels, pc) & 0xFFF
    elif fmt == "X":  # XOR rd, rs / LB rd, (rs) / HALT
        if args:
            word |= reg(args[0]) << 8 | reg(args[1]) << 4
        word |= funct

    return word


def main():
    if len(sys.argv) < 2:
        print("usage: m16asm.py input.asm [output.bin]")
        sys.exit(1)
    out = sys.argv[2] if len(sys.argv) > 2 else "program.bin"

    with open(sys.argv[1]) as f:
        labels, instrs, data = parse(f.readlines())

    image = bytearray()

    def put(addr, b):
        if len(image) < addr + 1:
            image.extend(b"\0" * (addr + 1 - len(image)))
        image[addr] = b & 0xFF

    for lineno, pc, name, args in instrs:
        try:
            w = encode(name, args, labels, pc)
        except (KeyError, ValueError, IndexError) as e:
            print("line %d: error: %s" % (lineno, e))
            sys.exit(1)
        put(pc, w)
        put(pc + 1, w >> 8)

    for lineno, pc, kind, items in data:
        try:
            for i, item in enumerate(items):
                v = item if isinstance(item, int) else value(item, labels)
                if kind == "words":
                    put(pc + i * 2, v)
                    put(pc + i * 2 + 1, v >> 8)
                else:
                    put(pc + i, v)
        except ValueError as e:
            print("line %d: error: %s" % (lineno, e))
            sys.exit(1)

    with open(out, "wb") as f:
        f.write(image)
    print("Wrote %d instructions and %d bytes of data to %s"
          % (len(instrs), len(image) - 2 * len(instrs), out))

if __name__ == "__main__":
    main()

