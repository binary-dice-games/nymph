#!/usr/bin/env python3
# MIT License © 2026 Binary Dice Games
"""Writes web/app/logo.png and web/app/logo-dark.png, the logo on the web
app's start page, from resources/Nymph.png.

Run it again after changing resources/Nymph.png and commit all three files:

    python3 web/tools/make_logo.py

Needs Pillow and NumPy. The white paper becomes transparent (colours stay
opaque; only anti-aliased edges blend), the result is cropped to the
artwork and scaled to 840 px wide (2x for a 420 px slot). The dark variant
lightens the grey tagline so it reads on the dark theme.
"""
import pathlib

import numpy as np
from PIL import Image

WIDTH = 840

root = pathlib.Path(__file__).resolve().parents[2]
rgb = np.asarray(Image.open(root / "resources" / "Nymph.png").convert("RGB")).astype(np.float64) / 255
# Alpha: how far a pixel is from white, on a steep ramp.
alpha = np.clip(((1 - rgb).max(axis=2) - 0.04) / 0.22, 0, 1)
safe = np.where(alpha > 0, alpha, 1)[..., None]
colour = np.clip((rgb - (1 - alpha[..., None])) / safe, 0, 1)


def save(pixels, name):
  rgba = np.dstack([pixels, alpha]) * 255
  img = Image.fromarray(rgba.round().astype(np.uint8), "RGBA")
  img = img.crop(img.getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox())
  img = img.resize((WIDTH, round(img.height * WIDTH / img.width)), Image.Resampling.LANCZOS)
  img.quantize(256, method=Image.Quantize.FASTOCTREE).save(root / "web" / "app" / name, optimize=True)
  print(f"wrote web/app/{name}: {img.width}x{img.height}")


save(colour, "logo.png")
# The tagline is the only grey (low-saturation) ink: map it to a light grey.
grey = (colour.max(axis=2) - colour.min(axis=2) < 0.12)[..., None]
light = np.repeat(1 - colour.mean(axis=2, keepdims=True) * 0.25, 3, axis=2) * [0.84, 0.88, 0.92]
save(np.where(grey, light, colour), "logo-dark.png")
