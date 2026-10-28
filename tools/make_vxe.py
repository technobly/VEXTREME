"""make_vxe.py - pack, inspect and unpack VXE bundles (bundle format version 2).

A .vxe is ONE file per co-op game holding its halves, so the 6809 side and the
STM32 side it must match can never be separated:

    <NAME>6809.bin       the 6809 cartridge image (code/app6809/<game>/<game>.bin)
    <NAME>STM_<layout>.bin  the STM32 payload (code/stm32/payload_<game>.bin) linked
                         at the slot address of one flash layout, with a
                         VxePayloadHeader first. A bundle carries one per
                         layout it supports: uf2, dfu, or both.

The STM32 payload is position-dependent, so a payload only runs in the layout
it was linked for. A game that fits both slots ships both images in the same
file and runs on either kernel; a game that only fits the 192 KB dfu slot
carries just the dfu image, and a uf2 kernel refuses it with a message.
The 6809 image is the same for both.

The VEXTREME kernel (code/stm32/vxe/vxe_install.c) installs the image for its own
layout into the flash slot when the game is picked in the menu, unless that
exact payload is already installed, then serves the 6809 image like any cartridge.

FILE LAYOUT (all integers little-endian; must match vxe/vxe_api.h VxeBundleHeader)
    0    char[4]  magic "VXE1"
    4    u16      format version (2)
    6    u16      header size (128)
    8    u16      payload ABI version (from the payloads' own headers; both agree)
    10   u16      layouts: bit0 = uf2 image present, bit1 = dfu image present
    12   u32      6809 part offset        16 u32 6809 part size     20 u32 6809 part CRC32
    24   image[0] uf2 layout:  u32 load address (0 if absent), u32 offset, u32 size, u32 CRC32
    40   image[1] dfu layout:  same
    56   char[24] 6809 part name
    80   char[24] game title (the 6809 header's title string, informational)
    104  u32[5]   reserved (0)
    124  u32      CRC32 of bytes 0..123
    128  ...      the parts, each 4-byte aligned

The packer also stamps each payload's IDENTITY - CRC32 of the payload image after
its header - into the payload header's crc32 field (offset 12). The kernel
compares that with the installed slot's to skip reinstalling the same payload.
CRC32 is zlib's (polynomial 0xEDB88320).

    tools/make_vxe.py pack GAME.vxe --m6809 GAME.bin --stm-uf2 payload_uf2.bin --stm-dfu payload_dfu.bin --name GAME
    tools/make_vxe.py info GAME.vxe
    tools/make_vxe.py unpack GAME.vxe outdir/
"""
import argparse
import os
import struct
import sys
import zlib

MAGIC = b"VXE1"
FORMAT_VERSION = 2
HEADER_SIZE = 128
# The two flash layouts (code/stm32/vxe/vxe_api.h): where the payload is linked to run, and how big the slot is.
#   uf2: the UF2 bootloader owns sector 0, the payload owns sector 5 only.
#   dfu: no bootloader, the payload owns sectors 4-5.
# The order is the order of the header's image[] table and of the `layouts` bits.
LAYOUTS = {"uf2": (0x08020000, 128 * 1024), "dfu": (0x08010000, 192 * 1024)}
LAYOUT_NAMES = list(LAYOUTS)
PAYLOAD_MAGIC = 0x50455856          # "VXEP"
PAYLOAD_HDR_FMT = "<IHHIIIII"       # VxePayloadHeader (28 bytes)
PAYLOAD_HDR_SIZE = struct.calcsize(PAYLOAD_HDR_FMT)
PAYLOAD_CRC_OFFSET = 12
M6809_MAX = 64 * 1024
HDR_FMT = "<4sHHHH III IIII IIII 24s24s IIIII"
assert struct.calcsize(HDR_FMT) == 124


def crc32(data, prev=0):
    return zlib.crc32(data, prev) & 0xFFFFFFFF


def fail(msg):
    sys.exit("make_vxe: " + msg)


def check_payload(stm, layout):
    slot_addr, slot_size = LAYOUTS[layout]
    where = "%s payload" % layout
    if len(stm) < PAYLOAD_HDR_SIZE:
        fail("%s is too short to hold a VxePayloadHeader" % where)
    magic, abi, hsize, end, _crc, r0, r1, start = struct.unpack_from(PAYLOAD_HDR_FMT, stm, 0)
    if magic != PAYLOAD_MAGIC:
        fail("%s has no VXEP header (magic %08x) - was it built with `make vxe-<game>`?" % (where, magic))
    if hsize != PAYLOAD_HDR_SIZE:
        fail("%s header size %d, expected %d" % (where, hsize, PAYLOAD_HDR_SIZE))
    if end - slot_addr != len(stm):
        fail("%s header says the image ends at %08x (%d bytes) but the file is %d bytes - "
             "was it linked for the %s layout (slot %08x)?" % (where, end, end - slot_addr, len(stm), layout, slot_addr))
    if len(stm) > slot_size:
        fail("%s is %d bytes, the %s slot holds %d" % (where, len(stm), layout, slot_size))
    if len(stm) % 4:
        fail("%s size %d is not a multiple of 4" % (where, len(stm)))
    if not (start & 1) or not (slot_addr + hsize <= (start & ~1) < end):
        fail("%s entry %08x is not a Thumb address inside the image" % (where, start))
    return abi


def stamp_identity(stm):
    """Return (stm with the identity CRC stamped into its header, identity)."""
    ident = crc32(stm[PAYLOAD_HDR_SIZE:])
    out = bytearray(stm)
    struct.pack_into("<I", out, PAYLOAD_CRC_OFFSET, ident)
    return bytes(out), ident


def title_of(m6809):
    """The 6809 header's title: after the copyright string's $80 and 4 header bytes, up to $80."""
    try:
        i = m6809.index(b"\x80", 0, 32) + 1 + 2 + 4     # $80, music pointer (2), h/w/y/x (4)
        j = m6809.index(b"\x80", i, i + 64)
        return m6809[i:j].decode("ascii", "replace")
    except ValueError:
        return ""


def pack(args):
    m6809 = open(args.m6809, "rb").read()
    if not m6809 or len(m6809) > M6809_MAX:
        fail("6809 part is %d bytes (1..%d)" % (len(m6809), M6809_MAX))
    given = {"uf2": args.stm_uf2, "dfu": args.stm_dfu}
    if not any(given.values()):
        fail("give at least one of --stm-uf2 / --stm-dfu")
    name = args.name or os.path.splitext(os.path.basename(args.out))[0]
    n6809 = (name + "6809.bin").encode()[:23]
    title = title_of(m6809).encode("ascii", "replace")[:23]

    off6809 = HEADER_SIZE
    cursor = (off6809 + len(m6809) + 3) & ~3
    images, parts, abis, idents, layouts = [], [], set(), {}, 0
    for i, lay in enumerate(LAYOUT_NAMES):
        if not given[lay]:
            images += [0, 0, 0, 0]
            continue
        stm = open(given[lay], "rb").read()
        abis.add(check_payload(stm, lay))
        stm, idents[lay] = stamp_identity(stm)
        images += [LAYOUTS[lay][0], cursor, len(stm), crc32(stm)]
        parts.append((cursor, stm))
        cursor = (cursor + len(stm) + 3) & ~3
        layouts |= 1 << i
    if len(abis) != 1:
        fail("the payloads were built against different ABI versions: %s" % sorted(abis))
    head = struct.pack(HDR_FMT, MAGIC, FORMAT_VERSION, HEADER_SIZE, abis.pop(), layouts,
                       off6809, len(m6809), crc32(m6809), *images, n6809, title, 0, 0, 0, 0, 0)
    head += struct.pack("<I", crc32(head))
    assert len(head) == HEADER_SIZE
    body = bytearray(head + m6809)
    for off, stm in parts:
        body += b"\x00" * (off - len(body)) + stm
    open(args.out, "wb").write(bytes(body))
    print("%s: %d bytes  6809 %s %d B  %s  title %r"
          % (args.out, len(body), n6809.decode(), len(m6809),
             "  ".join("%s %d B identity %08x" % (l, len(open(given[l], "rb").read()), idents[l])
                       for l in LAYOUT_NAMES if given[l]), title.decode()))


def parse(path):
    data = open(path, "rb").read()
    if len(data) < HEADER_SIZE or data[:4] != MAGIC:
        fail("%s is not a VXE bundle" % path)
    f = struct.unpack_from(HDR_FMT, data, 0)
    (magic, ver, hsize, abi, layouts, o6, s6, c6) = f[:8]
    imgs = [f[8 + 4 * i: 12 + 4 * i] for i in range(2)]
    n6, title = f[16], f[17]
    hcrc = struct.unpack_from("<I", data, 124)[0]
    problems = []
    if ver != FORMAT_VERSION: problems.append("format version %d (this tool reads %d)" % (ver, FORMAT_VERSION))
    if hsize != HEADER_SIZE: problems.append("header size %d" % hsize)
    if crc32(data[:124]) != hcrc: problems.append("header CRC mismatch")
    p6 = data[o6:o6 + s6]
    if len(p6) != s6 or crc32(p6) != c6: problems.append("6809 part CRC/size mismatch")
    stm = {}
    for i, lay in enumerate(LAYOUT_NAMES):
        load, off, size, crc = imgs[i]
        present = size != 0
        if present != bool(layouts & (1 << i)):
            problems.append("layouts flags disagree with the %s image table entry" % lay)
        if not present:
            continue
        ps = data[off:off + size]
        if len(ps) != size or crc32(ps) != crc: problems.append("%s STM part CRC/size mismatch" % lay)
        if load != LAYOUTS[lay][0]: problems.append("%s load address %08x, expected %08x" % (lay, load, LAYOUTS[lay][0]))
        stm[lay] = dict(load=load, data=ps,
                        ident=struct.unpack_from("<I", ps, PAYLOAD_CRC_OFFSET)[0] if len(ps) >= 16 else 0)
    if layouts == 0 or layouts & ~3: problems.append("layouts flags %#x" % layouts)
    return dict(abi=abi, layouts=layouts, p6=p6, stm=stm, n6=n6.rstrip(b"\0").decode(),
                title=title.rstrip(b"\0").decode(), problems=problems)


def info(args):
    b = parse(args.vxe)
    print("%s\n  title    %r\n  6809     %s  %d bytes  (shared by every layout)"
          % (args.vxe, b["title"], b["n6"], len(b["p6"])))
    for lay in LAYOUT_NAMES:
        if lay in b["stm"]:
            m = b["stm"][lay]
            print("  %-4s     STM %d bytes  slot %08x  ABI %d  identity %08x"
                  % (lay, len(m["data"]), m["load"], b["abi"], m["ident"]))
    print("  runs on  " + (" and ".join(l + " kernel" for l in LAYOUT_NAMES if l in b["stm"]) or "nothing"))
    print("  check    " + ("OK" if not b["problems"] else "PROBLEMS: " + "; ".join(b["problems"])))
    return 1 if b["problems"] else 0


def unpack(args):
    b = parse(args.vxe)
    if b["problems"]:
        fail("refusing to unpack a damaged bundle: " + "; ".join(b["problems"]))
    os.makedirs(args.outdir, exist_ok=True)
    stem = b["n6"][:-len("6809.bin")] if b["n6"].endswith("6809.bin") else b["n6"]
    parts = [(b["n6"], b["p6"])] + [("%sSTM_%s.bin" % (stem, l), b["stm"][l]["data"]) for l in LAYOUT_NAMES if l in b["stm"]]
    for name, data in parts:
        open(os.path.join(args.outdir, name), "wb").write(data)
        print("wrote %s (%d bytes)" % (os.path.join(args.outdir, name), len(data)))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("pack"); p.add_argument("out")
    p.add_argument("--m6809", required=True); p.add_argument("--name")
    p.add_argument("--stm-uf2", help="payload linked for the uf2 layout (128 KB slot at 0x08020000)")
    p.add_argument("--stm-dfu", help="payload linked for the dfu layout (192 KB slot at 0x08010000)")
    p.set_defaults(fn=pack)
    p = sub.add_parser("info"); p.add_argument("vxe"); p.set_defaults(fn=info)
    p = sub.add_parser("unpack"); p.add_argument("vxe"); p.add_argument("outdir"); p.set_defaults(fn=unpack)
    a = ap.parse_args()
    sys.exit(a.fn(a) or 0)


if __name__ == "__main__":
    main()
