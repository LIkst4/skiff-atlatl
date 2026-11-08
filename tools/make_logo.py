#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(custom) skiff-atlatl's logo: the stepped hook (an atlatl's hook and shaft, a stepped fret) in LED squares, and
"skiff ATLATL" in Silkscreen (SIL OFL 1.1, by Jason Kottke; its letters drawn here as paths, so the SVGs need no font).
    tools/make_logo.py Silkscreen-Regular.ttf
writes docs/logo/atlatl-*.svg, docs/logo/atlatl-*.png and docs/card.png (the hub's card, 240 x 240)."""
import sys
from pathlib import Path

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "logo"

HOOK = ["...........",
        "......SSSSS",
        "......S...S",
        "......S.S.S",
        "......S.SSS",
        ".....11....",
        "....22.....",
        "...33......",
        "..44.......",
        ".55........",
        "..........."]
P, R = 22, 18                    # the LED squares' pitch and size

NEON = {"bg": "#07070d", "off": "#17172a", "hook": "#ffffff", "glow": "#00f5d4", "sub": "#39ff14", "word": "#ffffff",
        "steps": ["#39ff14", "#ff6a00", "#00f5d4", "#ffe600", "#b026ff"]}      # neon green, orange, turquoise, yellow, purple
LIGHT = {"bg": "#f4f1ea", "off": "#e2ddd1", "hook": "#141414", "glow": None, "sub": "#1f9e0a", "word": "#141414",
         "steps": ["#1f9e0a", "#d95300", "#00a08a", "#c9a200", "#7a1fd1"]}      # the same, deep enough for a light ground


def cells(t):
    for r, line in enumerate(HOOK):
        for c, ch in enumerate(line):
            lit = ch != "."
            yield c, r, lit, (t["off"] if not lit else t["hook"] if ch == "S" else t["steps"][int(ch) - 1])


# ---- SVG
def text_path(font, s, size, x, y, spacing):
    """s as one path: the baseline at y, the letters size px tall (the em), spacing px apart"""
    gs, cmap, upm = font.getGlyphSet(), font.getBestCmap(), font["head"].unitsPerEm
    k, out = size / upm, []
    for ch in s:
        g = cmap[ord(ch)]
        pen = SVGPathPen(gs)
        gs[g].draw(TransformPen(pen, (k, 0, 0, -k, x, y)))
        out.append(pen.getCommands())
        x += gs[g].width * k + spacing
    return " ".join(out), x - spacing


def svg_mark(t, ox, oy, s=1.0, glow_id="glow"):
    off = "".join(f'<rect x="{ox + c * P * s:.2f}" y="{oy + r * P * s:.2f}" width="{R * s:.2f}" height="{R * s:.2f}" '
                  f'rx="{3 * s:.2f}" fill="{col}"/>' for c, r, lit, col in cells(t) if not lit)
    lit = "".join(f'<rect x="{ox + c * P * s:.2f}" y="{oy + r * P * s:.2f}" width="{R * s:.2f}" height="{R * s:.2f}" '
                  f'rx="{3 * s:.2f}" fill="{col}"/>' for c, r, lit, col in cells(t) if lit)
    g = f' filter="url(#{glow_id})"' if t["glow"] else ""
    return f'{off}<g{g}>{lit}</g>'


def glow_def(t, std, glow_id="glow"):
    if not t["glow"]:
        return ""
    return (f'<defs><filter id="{glow_id}" x="-30%" y="-30%" width="160%" height="160%" color-interpolation-filters="sRGB">'
            f'<feGaussianBlur stdDeviation="{std}" result="b"/><feMerge><feMergeNode in="b"/>'
            f'<feMergeNode in="SourceGraphic"/></feMerge></filter></defs>')


def svg_logo(font, t, bg):
    pad, m = 24, 11 * P - (P - R)                       # the mark: 238 px
    sub, subw = text_path(font, "skiff", 26, 0, 0, 4)
    word, wordw = text_path(font, "ATLATL", 96, 0, 0, 2)
    tx, h = pad + m + 44, m + 2 * pad
    w = tx + max(subw, wordw) + pad
    gy = pad + m / 2 - (26 + 8 + 86) / 2               # the two lines' block centred on the mark
    body = (glow_def(t, 4) + (f'<rect width="{w:.0f}" height="{h:.0f}" fill="{t["bg"]}"/>' if bg else "")
            + svg_mark(t, pad, pad)
            + f'<g transform="translate({tx:.1f},{gy + 22:.1f})"><path d="{sub}" fill="{t["sub"]}"/></g>'
            + f'<g transform="translate({tx:.1f},{gy + 26 + 8 + 84:.1f})"'
            + (' filter="url(#glow)"' if t["glow"] else "") + f'><path d="{word}" fill="{t["word"]}"/></g>')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w:.0f} {h:.0f}" width="{w:.0f}" height="{h:.0f}" '
            f'role="img" aria-label="skiff-atlatl">{body}</svg>\n')


def svg_icon(t, size=512, radius=112):
    m = 11 * P - (P - R)
    s = size * 0.7 / m
    o = (size - m * s) / 2
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size} {size}" width="{size}" height="{size}" '
            f'role="img" aria-label="skiff-atlatl">{glow_def(t, round(4 * s, 2))}'
            f'<rect width="{size}" height="{size}" rx="{radius}" fill="{t["bg"]}"/>{svg_mark(t, o, o, s)}</svg>\n')


# ---- PNG (drawn directly; the glow a blur of the lit squares)
def png_mark(img, t, ox, oy, s, glow=True):
    base, lit = ImageDraw.Draw(img), Image.new("RGBA", img.size, (0, 0, 0, 0))
    dl = ImageDraw.Draw(lit)
    for c, r, on, col in cells(t):
        box = (ox + c * P * s, oy + r * P * s, ox + c * P * s + R * s, oy + r * P * s + R * s)
        (dl if on else base).rounded_rectangle(box, radius=3 * s, fill=col)
    if glow and t["glow"]:
        img.alpha_composite(lit.filter(ImageFilter.GaussianBlur(4 * s)))
    img.alpha_composite(lit)


def png_text(img, t, ttf, text, size, xy, color, spacing, glow=False, glow_color=None):
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d, f, x = ImageDraw.Draw(layer), ImageFont.truetype(ttf, size), xy[0]
    for ch in text:
        d.text((x, xy[1]), ch, font=f, fill=color)
        x += f.getlength(ch) + spacing
    if glow and t["glow"]:
        rgb = Image.new("RGB", (1, 1), glow_color or t["glow"]).getpixel((0, 0))
        g = Image.merge("RGBA", (*Image.new("RGB", layer.size, rgb).split(), layer.getchannel("A")))
        img.alpha_composite(g.filter(ImageFilter.GaussianBlur(size / 10)))
    img.alpha_composite(layer)
    return x - spacing


def png_card(ttf, t, size=240):
    k = 4                                              # drawn 4 x, then scaled down
    img = Image.new("RGBA", (size * k, size * k), t["bg"])
    m = 11 * P - (P - R)
    s = 0.56 * k
    png_mark(img, t, (size * k - m * s) / 2, 34 * k, s)
    f = ImageFont.truetype(ttf, 26 * k)
    w = sum(f.getlength(ch) for ch in "ATLATL") + 2 * k * 5
    png_text(img, t, ttf, "ATLATL", 26 * k, ((size * k - w) / 2, 184 * k), t["word"], 2 * k, glow=True)
    return img.resize((size, size), Image.LANCZOS)


def png_icon(t, size):
    k = 2
    img = Image.new("RGBA", (size * k, size * k), (0, 0, 0, 0))
    ImageDraw.Draw(img).rounded_rectangle((0, 0, size * k - 1, size * k - 1), radius=size * k * 0.22, fill=t["bg"])
    m = 11 * P - (P - R)
    s = size * k * 0.7 / m
    png_mark(img, t, (size * k - m * s) / 2, (size * k - m * s) / 2, s, glow=size >= 64)
    return img.resize((size, size), Image.LANCZOS)


def png_social(ttf, t, w=1280, h=640):
    k = 2
    img = Image.new("RGBA", (w * k, h * k), t["bg"])
    m = 11 * P - (P - R)
    s = 1.3 * k
    mx = 150 * k
    png_mark(img, t, mx, (h * k - m * s) / 2 - 30 * k, s)
    tx = mx + m * s + 60 * k
    png_text(img, t, ttf, "skiff", 40 * k, (tx, 200 * k), t["sub"], 6 * k, glow=True, glow_color=t["sub"])
    png_text(img, t, ttf, "ATLATL", 140 * k, (tx, 250 * k), t["word"], 4 * k, glow=True)
    f = ImageFont.truetype(ttf, 22 * k)
    ImageDraw.Draw(img).text((mx, 520 * k), "groovebox firmware for the M-VAVE FM-1  ·  forked from Skiff", font=f,
                             fill="#9a9ab0")
    return img.resize((w, h), Image.LANCZOS)


def main(ttf):
    font = TTFont(ttf)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "atlatl-logo.svg").write_text(svg_logo(font, NEON, bg=False))
    (OUT / "atlatl-logo-light.svg").write_text(svg_logo(font, LIGHT, bg=False))
    (OUT / "atlatl-icon.svg").write_text(svg_icon(NEON))
    png_icon(NEON, 512).save(OUT / "atlatl-icon-512.png")
    png_icon(NEON, 32).save(OUT / "atlatl-favicon-32.png")
    png_social(ttf, NEON).convert("RGB").save(OUT / "atlatl-social.png")
    png_card(ttf, NEON).convert("RGB").save(ROOT / "docs" / "card.png")
    print("docs/logo: atlatl-logo.svg, atlatl-logo-light.svg, atlatl-icon.svg, atlatl-icon-512.png, "
          "atlatl-favicon-32.png, atlatl-social.png; docs/card.png")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    main(sys.argv[1])
