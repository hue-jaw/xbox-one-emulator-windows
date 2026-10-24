#!/usr/bin/env python3
"""make_fixtures.py - genere un faux jeu Xbox 360 et ses images de test.

Arborescence generee (typique d'un jeu 360) :
  default.xex
  media/game.bin
  media/textures/deep/tex.dat
  data/config.ini
  data/version.txt
  ... + un nom de fichier "sale" pour verifier l'assainissement FATX

Sorties dans build/fixtures/ :
  game-trim.iso   partition a 0x0 (image trimee, comme extract-xiso)
  game-xgd2.iso   partition a 0xFD90000 avec du bruit avant
"""
import os
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "build", "fixtures")
SRC = os.path.join(OUT, "game-src")

XEX_STUB = b"XEX2\x00\x00\x00\x00" + b"fake-xex-payload" * 64
CONFIG = b"[game]\nname=xiboot test\nlevel=3\n"


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def main():
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(SRC)

    write(os.path.join(SRC, "default.xex"), XEX_STUB)
    write(os.path.join(SRC, "media", "game.bin"), bytes(range(256)) * 700)
    write(os.path.join(SRC, "media", "textures", "deep", "tex.dat"),
          b"TEX" + bytes(5000))
    write(os.path.join(SRC, "data", "config.ini"), CONFIG)
    write(os.path.join(SRC, "data", "version.txt"), b"1.0.0.0\n")
    # nom "sale" : deux-points, etoile et nom long > 42 caracteres
    write(os.path.join(SRC, "data", 'weird:name*with?bad|chars.bin'), b"X" * 300)
    write(os.path.join(SRC, "data", "a" * 60 + ".dat"), b"L" * 100)

    modes = [
        ("game-trim.iso", 0x0),
        ("game-xgd2.iso", 0xFD90000),
    ]
    for name, base in modes:
        cmd = [sys.executable, os.path.join(HERE, "mkxiso.py"), SRC,
               os.path.join(OUT, name), "--base", hex(base), "--pad-mb", "1"]
        if base:
            cmd.append("--junk")
        subprocess.check_call(cmd)

    print("fixtures dans", OUT)


if __name__ == "__main__":
    sys.exit(main())
