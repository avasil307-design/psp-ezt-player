#!/usr/bin/env python3
"""Fetch Roboto + Material Symbols Rounded from npm and convert woff2 -> ttf.

Result: build/fonts/{Roboto-Regular.ttf,Roboto-Medium.ttf,MaterialSymbolsRounded.ttf}
Requires: node/npm, fonttools, brotli.
"""
import os
import shutil
import subprocess
import sys
import tarfile

from fontTools.ttLib import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build")
NPM = os.path.join(BUILD, "npm")
FONTS = os.path.join(BUILD, "fonts")

PACKAGES = ["roboto-fontface@0.10.0", "material-symbols@0.49.0"]
CONVERT = [
    ("package/fonts/roboto/Roboto-Regular.woff2", "Roboto-Regular.ttf"),
    ("package/fonts/roboto/Roboto-Medium.woff2", "Roboto-Medium.ttf"),
    ("package/material-symbols-rounded.woff2", "MaterialSymbolsRounded.ttf"),
]


def main():
    os.makedirs(NPM, exist_ok=True)
    os.makedirs(FONTS, exist_ok=True)

    tarballs = {}
    for spec in PACKAGES:
        name = spec.split("@")[0]
        out = subprocess.run(
            ["npm", "pack", spec, "--silent", "--pack-destination", NPM],
            check=True, capture_output=True, text=True,
        ).stdout.strip().splitlines()[-1]
        tarballs[name] = os.path.join(NPM, out)
        print("packed", name, "->", out)

    for name, path in tarballs.items():
        dest = os.path.join(NPM, name)
        if os.path.isdir(dest):
            shutil.rmtree(dest)
        with tarfile.open(path) as tf:
            tf.extractall(dest)

    for src_rel, out_name in CONVERT:
        src = None
        for name in tarballs:
            cand = os.path.join(NPM, name, src_rel)
            if os.path.isfile(cand):
                src = cand
                break
        if not src:
            print("ERROR: missing", src_rel)
            return 1
        font = TTFont(src)
        font.flavor = None
        dst = os.path.join(FONTS, out_name)
        font.save(dst)
        print("font", out_name, os.path.getsize(dst), "bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
