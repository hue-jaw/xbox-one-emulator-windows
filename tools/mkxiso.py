#!/usr/bin/env python3
"""mkxiso.py - fabrique une image XDVDFS (Xbox 360) de test.

Ce n'est pas un outil de production : il sert a generer des images
reproductibles pour les tests de xiboot (parseur, extraction, cache).

Usage:
  python3 tools/mkxiso.py <dossier_source> <sortie.iso> [--base 0xFD90000]
                          [--junk] [--pad-mb 200]

  --base   offset de la partition jeu dans le fichier (0 = image trimee)
  --junk   remplit la zone avant la partition de donnees aleatoires
           (imite un dump XGD2 ou la partition est noyee dans le bruit)
"""
import argparse
import os
import random
import struct
import sys

SECTOR = 2048
VD_OFFSET = 0x10000
VD_MAGIC = b"MICROSOFT*XBOX*MEDIA"
ATTR_DIR = 0x10
ATTR_FILE = 0x20


def align_up(n, a):
    return (n + a - 1) // a * a


class Node:
    def __init__(self, name, path, is_dir):
        self.name = name
        self.path = path
        self.is_dir = is_dir
        self.size = 0 if is_dir else os.path.getsize(path)
        self.children = []
        self.sector = 0
        self.table_size = 0

    @property
    def attr(self):
        return ATTR_DIR if self.is_dir else ATTR_FILE


def scan(path, name=""):
    node = Node(name, path, os.path.isdir(path))
    if node.is_dir:
        for child in sorted(os.listdir(path)):
            node.children.append(scan(os.path.join(path, child), child))
    return node


def rec_len(name):
    return align_up(14 + len(name.encode("latin-1", "replace")[:42]), 4)


def table_size(node):
    total = sum(rec_len(c.name) for c in node.children)
    return align_up(max(total, 1), SECTOR)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("out")
    ap.add_argument("--base", default="0")
    ap.add_argument("--junk", action="store_true")
    ap.add_argument("--pad-mb", type=int, default=0)
    ap.add_argument("--seed", type=int, default=1234)
    args = ap.parse_args()

    base = int(args.base, 0)
    root = scan(args.src)

    # --- allocation (deux passes) --------------------------------------
    cursor = 33  # secteur 32 = descripteur de volume

    def alloc_tables(node):
        nonlocal cursor
        node.table_size = table_size(node)
        node.sector = cursor
        cursor += node.table_size // SECTOR
        for c in node.children:
            if c.is_dir:
                alloc_tables(c)

    def alloc_data(node):
        nonlocal cursor
        for c in node.children:
            if c.is_dir:
                alloc_data(c)
            else:
                c.sector = cursor
                cursor += align_up(max(c.size, 1), SECTOR) // SECTOR

    alloc_tables(root)
    alloc_data(root)
    total_sectors = cursor
    img_size = base + total_sectors * SECTOR

    buf = bytearray(img_size)

    # --- descripteur de volume ----------------------------------------
    vd = bytearray(SECTOR)
    vd[0:20] = VD_MAGIC
    struct.pack_into("<IIQ", vd, 0x14, root.sector, root.table_size, 0)
    vd[0x7EC:0x7EC + 20] = VD_MAGIC
    buf[base + VD_OFFSET:base + VD_OFFSET + SECTOR] = vd

    # --- tables de repertoires ----------------------------------------
    def write_table(node):
        ents = node.children
        body = bytearray(b"\xFF" * node.table_size)
        state = {"cursor": 0}

        def rec(lo, hi):
            if lo > hi:
                return 0
            mid = (lo + hi) // 2
            c = ents[mid]
            name = c.name.encode("latin-1", "replace")[:42]
            reclen = rec_len(c.name)
            # une entree ne traverse jamais une frontiere de secteur
            off = state["cursor"]
            if (off % SECTOR) + reclen > SECTOR:
                off = ((off // SECTOR) + 1) * SECTOR
            state["cursor"] = off + reclen
            left = rec(lo, mid - 1)
            right = rec(mid + 1, hi)
            sector = c.sector
            size = c.table_size if c.is_dir else c.size
            rec_bytes = struct.pack("<HHIIBB", left // 4 if left else 0,
                                    right // 4 if right else 0, sector, size,
                                    c.attr, len(name)) + name
            rec_bytes += b"\x00" * (reclen - len(rec_bytes))
            body[off:off + reclen] = rec_bytes
            return off

        rec(0, len(ents) - 1)
        start = base + node.sector * SECTOR
        buf[start:start + node.table_size] = body
        for c in ents:
            if c.is_dir:
                write_table(c)

    write_table(root)

    # --- contenu des fichiers ------------------------------------------
    def write_data(node):
        for c in node.children:
            if c.is_dir:
                write_data(c)
            else:
                with open(c.path, "rb") as f:
                    data = f.read()
                start = base + c.sector * SECTOR
                buf[start:start + len(data)] = data

    write_data(root)

    # --- zones hors partition ------------------------------------------
    if args.junk and base > 0:
        rnd = random.Random(args.seed)
        chunk = bytes(rnd.getrandbits(8) for _ in range(SECTOR))
        for i in range(0, base, SECTOR):
            buf[i:i + SECTOR] = chunk
        # on reecrit le descripteur apres le bruit
        buf[base + VD_OFFSET:base + VD_OFFSET + len(vd)] = vd
        write_table(root)
        write_data(root)

    if args.pad_mb:
        buf += b"\x00" * (args.pad_mb * 1024 * 1024)

    with open(args.out, "wb") as f:
        f.write(buf)

    nfiles = sum(1 for _ in files(root))
    print(f"OK {args.out}: base=0x{base:X} sectors={total_sectors} "
          f"files={nfiles} size={len(buf)}")


def files(node):
    for c in node.children:
        if c.is_dir:
            yield from files(c)
        else:
            yield c


if __name__ == "__main__":
    sys.exit(main())
