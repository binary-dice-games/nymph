#!/usr/bin/env python3
# MIT License © 2026 Binary Dice Games
"""Writes resources/icon.ico, the icon compiled into nymph.exe
(app/nymph.rc.in), from resources/icon.png.

Run it again after changing resources/icon.png and commit both files:

    python3 packaging/windows/make_ico.py

Needs Pillow. The .ico holds every size Explorer and the taskbar ask for,
each scaled from the 512 px PNG with a Lanczos filter.
"""
import pathlib

from PIL import Image

SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]

root = pathlib.Path(__file__).resolve().parents[2]
source = Image.open(root / "resources" / "icon.png").convert("RGBA")
images = [source.resize((s, s), Image.Resampling.LANCZOS) for s in SIZES]
images[-1].save(root / "resources" / "icon.ico", format="ICO", sizes=[(s, s) for s in SIZES],
                append_images=images[:-1])
print("wrote resources/icon.ico:", ", ".join(f"{s}px" for s in SIZES))
