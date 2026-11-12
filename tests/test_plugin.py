"""Binary self-check for the VSH plugin loader contract. Offline; no console.

The bug this guards against: Cobra's loadprx runs module_start on the loader
thread and waits for that thread to TERMINATE. A module_start that returns
normally never completes the syscall, so the caller's load hangs forever.
That is exactly what froze the switcher app inside "Cobra load begin slot=2"
(v2 returned normally; the sibling plugin ps3buzz_vsh ends module_start with
lv2 syscall 41 and its loads always return).

The contract is therefore checked on the COMPILED artifact, not on the source:

  1. module_start must not return to the loader: its body contains no `blr`,
     and its last branch relocation resolves to exit_thread;
  2. exit_thread must issue lv2 syscall 41 (`li r11,41` then `sc`);
  3. module_stop must likewise end by exiting its thread;
  4. the packed PRX must export module_start (nid bc9a0086) and module_stop
     (nid ab779874), and their addresses must match the ELF descriptors.

Run:  python tests/test_plugin.py [path/to/vsh_240p.elf]
"""
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
ELF = ROOT / "plugin" / "vsh_240p.elf"
PRX = ROOT / "plugin" / "vsh_240p.prx"

NID_MODULE_START = 0xBC9A0086
NID_MODULE_STOP = 0xAB779874

# PPC64 instruction encodings used by the check.
INSN_BLR = 0x4E800020     # blr
INSN_SC = 0x44000002      # sc
INSN_LI_R11_41 = 0x39600029  # li r11,41 (addi r11,0,41)

# R_PPC64_REL24: the PPC64 branch relocation the loader/packer resolves.
REL_PPC64_REL24 = 10

# Maximum instructions inspected at the head of a function body.
PROLOGUE_LIMIT = 24

# A terminating branch must sit within this many instructions of the end.
TAIL_WINDOW = 4

# Module-info version byte. The only VSH plugin proven to load on this
# console/toolchain (the sibling helper ps3buzz_vsh) uses 3; the packer's
# generic fallback is 4, which was never exercised.
MODULE_INFO_MINOR = 3


class Elf64BE:
    """Minimal big-endian ELF64 reader: sections, symbols, relocations."""

    def __init__(self, data):
        self.data = data
        assert data[:7] == b"\x7fELF\x02\x02\x01", "expected a big-endian ELF64"
        (shoff,) = struct.unpack_from(">Q", data, 40)
        (shentsize, shnum, shstrndx) = struct.unpack_from(">HHH", data, 58)
        raw = []
        for i in range(shnum):
            at = shoff + i * shentsize
            fields = struct.unpack_from(">IIQQQQIIQQ", data, at)
            raw.append(dict(name=fields[0], type=fields[1], flags=fields[2], addr=fields[3],
                            offset=fields[4], size=fields[5], link=fields[6], info=fields[7],
                            entsize=fields[9]))
        strtab = raw[shstrndx]
        for section in raw:
            base = strtab["offset"] + section["name"]
            section["sname"] = data[base:data.index(0, base)].decode()
        self.sections = raw
        self.by_name = {s["sname"]: s for s in raw}

    def symbols(self):
        symtab = self.by_name.get(".symtab")
        if symtab is None:
            return []
        strtab = self.sections[symtab["link"]]
        out = []
        for at in range(symtab["offset"], symtab["offset"] + symtab["size"], 24):
            st_name, st_info, st_other, st_shndx, st_value, st_size = struct.unpack_from(">IBBHQQ", self.data, at)
            base = strtab["offset"] + st_name
            name = self.data[base:self.data.index(0, base)].decode()
            out.append(dict(name=name, info=st_info, other=st_other, shndx=st_shndx, value=st_value))
        return out

    def relocations(self, section_name=".rela.text"):
        section = self.by_name.get(section_name)
        if section is None:
            return []
        symtab = self.sections[section["link"]]
        out = []
        for at in range(section["offset"], section["offset"] + section["size"], 24):
            r_offset, r_info, r_addend = struct.unpack_from(">QQq", self.data, at)
            index = r_info >> 32
            rtype = r_info & 0xFFFFFFFF
            sym_at = symtab["offset"] + index * 24
            st_name = struct.unpack_from(">I", self.data, sym_at)[0]
            st_value = struct.unpack_from(">Q", self.data, sym_at + 8)[0]
            out.append(dict(site=r_offset, type=rtype, index=index, value=st_value, addend=r_addend))
        return out

    def file_offset(self, section, vma):
        return section["offset"] + (vma - section["addr"])

    def u32(self, section, vma):
        return struct.unpack_from(">I", self.data, self.file_offset(section, vma))[0]


def code_entries(elf):
    """Sorted code entries of every function symbol.

    PS3 ELF functions live in .opd as a descriptor whose first word is the
    real code entry in .text. The symbol's own section index points at .opd,
    so boundaries must be derived from the code entries, not the symbols.
    """
    text = elf.by_name[".text"]
    opd = elf.by_name[".opd"]
    entries = set()
    for symbol in elf.symbols():
        descriptor = symbol["value"]
        if not (opd["addr"] <= descriptor < opd["addr"] + opd["size"]):
            continue
        entry = elf.u32(opd, descriptor)
        if text["addr"] <= entry < text["addr"] + text["size"]:
            entries.add(entry)
    return sorted(entries)


def function_body(elf, symbol, text, opd, entries):
    """Return (code_entry, list_of_instructions) for a function symbol."""
    descriptor = symbol["value"]
    code_entry = elf.u32(opd, descriptor)
    following = [e for e in entries if e > code_entry]
    end = following[0] if following else code_entry + 64
    body = []
    for at in range(code_entry, end, 4):
        body.append(struct.unpack_from(">I", elf.data, elf.file_offset(text, at))[0])
    return code_entry, body


def check_elf(elf):
    text = elf.by_name[".text"]
    opd = elf.by_name[".opd"]
    symbols = {s["name"]: s for s in elf.symbols()}
    for required in ("module_start", "module_stop"):
        assert required in symbols, f"{required} is missing from the binary"
    if "exit_thread" not in symbols:
        raise AssertionError(
            "exit_thread was eliminated from the binary: module_start does not call it. "
            "A module_start that returns normally hangs the Cobra loader forever.")

    entries = code_entries(elf)
    exit_entry, exit_body = function_body(elf, symbols["exit_thread"], text, opd, entries)
    start_entry, start_body = function_body(elf, symbols["module_start"], text, opd, entries)
    stop_entry, stop_body = function_body(elf, symbols["module_stop"], text, opd, entries)

    # 2. exit_thread issues lv2 syscall 41.
    head = exit_body[:PROLOGUE_LIMIT]
    assert INSN_LI_R11_41 in head, "exit_thread does not load syscall number 41"
    assert INSN_SC in head, "exit_thread does not execute sc"

    relocations = [r for r in elf.relocations() if r["type"] == REL_PPC64_REL24]
    exit_descriptor = symbols["exit_thread"]["value"]

    def terminal_branch(name, entry, body):
        """The last branch relocation of a function must exit the thread."""
        end = entry + 4 * len(body)
        inside = [r for r in relocations if entry <= r["site"] < end]
        assert inside, f"{name} has no branch relocation"
        last = inside[-1]
        assert last["value"] == exit_descriptor, (
            f"{name}'s final branch does not target exit_thread")
        assert end - last["site"] <= 4 * TAIL_WINDOW, (
            f"{name} does not end on the thread exit")
        return last["site"]

    # 1. module_start must not return to the loader.
    assert INSN_BLR not in start_body, "module_start contains blr: it can return to the loader"
    terminal_branch("module_start", start_entry, start_body)

    # 3. module_stop must likewise end by exiting its thread.
    assert INSN_BLR not in stop_body, "module_stop contains blr: it can return to the loader"
    terminal_branch("module_stop", stop_entry, stop_body)

    return dict(exit_entry=exit_entry, start_entry=start_entry, stop_entry=stop_entry,
                start_descriptor=symbols["module_start"]["value"],
                stop_descriptor=symbols["module_stop"]["value"])


def check_prx(addresses):
    """The packed PRX must export module_start and module_stop correctly."""
    data = PRX.read_bytes()
    marker = b"vsh_240p"
    at = data.find(marker)
    assert at >= 0, "packed PRX does not contain the module name"
    info = at - 4
    toc, es, ee, ss, se = struct.unpack_from(">IIIII", data, info + 32)

    # In the packed PRX the ELF header is reused: e_phoff sits at offset 32
    # and points at the three native program headers.
    phoff = struct.unpack_from(">Q", data, 32)[0]
    phentsize, phnum = struct.unpack_from(">HH", data, 54)
    seg0 = None
    for i in range(phnum):
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from(
            ">IIQQQQQQ", data, phoff + i * phentsize)
        if p_type == 1 and p_flags & 1:  # PT_LOAD, executable
            seg0 = (p_offset, p_vaddr)
            break
    assert seg0, "packed PRX has no executable load segment"

    def va2off(va):
        return seg0[0] + (va - seg0[1])

    ef = va2off(es)
    assert struct.unpack_from(">HHH", data, ef + 4)[1] == 2, "expected two exported functions"
    nt, st = struct.unpack_from(">II", data, ef + 20)
    exports = {}
    for i in range(2):
        nid = struct.unpack_from(">I", data, va2off(nt) + 4 * i)[0]
        addr = struct.unpack_from(">I", data, va2off(st) + 4 * i)[0]
        exports[nid] = addr
    assert NID_MODULE_START in exports, "module_start is not exported"
    assert NID_MODULE_STOP in exports, "module_stop is not exported"
    assert exports[NID_MODULE_START] == addresses["start_descriptor"], (
        "packed module_start address does not match the ELF descriptor")
    assert exports[NID_MODULE_STOP] == addresses["stop_descriptor"], (
        "packed module_stop address does not match the ELF descriptor")
    return exports


def check_module_info():
    """The module-info version byte must match the proven VSH plugin."""
    data = PRX.read_bytes()
    marker = b"vsh_240p"
    at = data.find(marker)
    assert at >= 0, "packed PRX does not contain the module name"
    info = at - 4
    attr, version, minor = struct.unpack_from(">HBB", data, info)
    assert attr == 0 and version == 0, "unexpected module-info attributes"
    assert minor == MODULE_INFO_MINOR, (
        f"module-info version byte is {minor}, expected {MODULE_INFO_MINOR}")
    return minor


def main():
    elf_path = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ELF
    if not elf_path.exists():
        sys.exit(f"missing {elf_path}; run tools/build_plugin.py first")
    if not PRX.exists():
        sys.exit(f"missing {PRX}; run tools/build_plugin.py first")

    elf = Elf64BE(elf_path.read_bytes())
    addresses = check_elf(elf)
    exports = check_prx(addresses)
    minor = check_module_info()

    print("VSH plugin loader contract:")
    print(f"  exit_thread  code @ 0x{addresses['exit_entry']:08x}  (lv2 syscall 41, no blr)")
    print(f"  module_start code @ 0x{addresses['start_entry']:08x}  (ends in a call to exit_thread, no blr)")
    print(f"  module_stop  code @ 0x{addresses['stop_entry']:08x}  (ends in a call to exit_thread, no blr)")
    print(f"  module info  version byte {minor} (matches the proven VSH helper)")
    print(f"  PRX exports  module_start nid={NID_MODULE_START:08x} -> 0x{exports[NID_MODULE_START]:08x}")
    print(f"               module_stop  nid={NID_MODULE_STOP:08x} -> 0x{exports[NID_MODULE_STOP]:08x}")
    print("PASS: module_start cannot return to the Cobra loader; the load call terminates")
    return 0


if __name__ == "__main__":
    sys.exit(main())
