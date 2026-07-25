#!/usr/bin/env python3
"""FlightBox — bakes sim/src/render/FBHudFontRom.h (the GENERATED 16x16 8-bit-coverage glyph ROM) from
B612 Mono (SIL OFL 1.1, sim/tools/fonts/). Run this only when the font source or charset changes; the
checked-in header is the artifact FBHudFont.h consumes -- there is NO build-time dependency on this
script or on Pillow.

Method: each glyph is rasterised at SUPERSAMPLE x the final cell resolution (128x128 for a 16x16 cell,
i.e. 8x), then box-filtered down to 16x16 -- every output texel is the true fractional area its 8x8
supersample block covers, not a smoothing blur. Same charset/index order as the old MAX7456-style ROM
(kFontCharset), same left-baseline anchor for every glyph so they all sit on one shared baseline.

The horizontal content window a glyph's ink may use is derived from FONT_ADVANCE/FONT_QUAD_SIZE below,
which MUST match FBHudFont.h's kFontAdvance/kFontQuadSize (currently 4.0/6.0 -- the same ratio the old
bitmap font used) -- ink past that column would visibly collide with the next glyph at the HUD's tight
character advance. Kept as a manual constant, not parsed out of the C++ header.

Weight: Bold -- B612 Mono Regular's strokes vanished at the HUD's smallest live scale (the "SP"/"GS"/
"ASL" labels, ~1.4x); Bold reads at least as well as the old 1bpp MAX7456-style ROM there while staying
just as clean on the larger tape digits (see the task's crop proof). Only Bold is checked in.

Usage: python3 sim/tools/bake_hud_font.py [--font NAME] [--preview PATH.png]
Requires: Pillow (pip install pillow -- a scratch venv is fine, this never runs from the C++ build).
"""
import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

SIM_DIR = Path(__file__).resolve().parent.parent
FONTS_DIR = SIM_DIR / "tools" / "fonts"
OUT_PATH = SIM_DIR / "src" / "render" / "FBHudFontRom.h"

# Must match FBHudFont.h's kFontAdvance/kFontQuadSize (screen-space HUD text units, unchanged by this
# bake -- only the raster resolution behind them changed).
FONT_ADVANCE = 4.0
FONT_QUAD_SIZE = 6.0

CHARSET = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.:/+\u00b0"

CELL = 16                      # final baked glyph resolution (content only; FBHudFont.h adds the gutter)
SUPERSAMPLE = 8
SS = CELL * SUPERSAMPLE         # 128px working canvas per glyph

# Ink must stay within the fraction of the cell the renderer guarantees won't overlap the next glyph
# (see the module banner), shrunk a further 15% for comfortable inter-character spacing.
SAFE_X_FRACTION = (FONT_ADVANCE / FONT_QUAD_SIZE) * 0.85
CONTENT_X0_FRAC = 0.03          # small left margin, echoes the old ROM's blank leftmost column
CONTENT_Y0_FRAC = 0.06          # top margin
CONTENT_Y1_FRAC = 0.92          # cap-height + descender band


def fit_font_size(font_path: Path) -> tuple[ImageFont.FreeTypeFont, int, int]:
    """Pick a pixel size so the font's own line metrics fill the vertical content band AND its
    (monospace) advance width fills no more than the horizontal safe band -- whichever is tighter."""
    probe = ImageFont.truetype(str(font_path), 200)
    ascent, descent = probe.getmetrics()
    advance = probe.getlength("W")   # monospace: every glyph shares this advance

    size_y = 200 * (SS * (CONTENT_Y1_FRAC - CONTENT_Y0_FRAC)) / (ascent + descent)
    size_x = 200 * (SS * SAFE_X_FRACTION) / advance
    size = max(8, round(min(size_x, size_y)))

    font = ImageFont.truetype(str(font_path), size)
    ascent, descent = font.getmetrics()
    return font, ascent, descent


def bake_glyph(font: ImageFont.FreeTypeFont, ascent: int, ch: str) -> Image.Image:
    img = Image.new("L", (SS, SS), 0)
    if ch != " ":
        draw = ImageDraw.Draw(img)
        baseline_y = int(SS * CONTENT_Y0_FRAC) + ascent
        x0 = int(SS * CONTENT_X0_FRAC)
        draw.text((x0, baseline_y), ch, font=font, fill=255, anchor="ls")
    resampling = getattr(Image, "Resampling", Image).BOX
    return img.resize((CELL, CELL), resampling)


def emit_header(glyphs: list[Image.Image], font_name: str) -> str:
    lines = []
    lines.append("/* GENERATED FILE -- do not hand-edit. Produced by sim/tools/bake_hud_font.py from")
    lines.append(f" * {font_name} (SIL Open Font License 1.1) -- https://github.com/polarsys/b612,")
    lines.append(" * licensed under sim/tools/fonts/OFL.txt. Regenerate after any font/charset change:")
    lines.append(" *   python3 sim/tools/bake_hud_font.py")
    lines.append(" *")
    lines.append(" * Each glyph is an 8-bit COVERAGE bitmap (0..255, true box-filtered area coverage,")
    lines.append(" * see the script), kFontTile x kFontTile texels, row-major, row 0 = top. FBHudFont.h")
    lines.append(" * turns this into the gutter-padded atlas FBHudStage uploads. */")
    lines.append("#ifndef FBHUDFONTROM_H")
    lines.append("#define FBHUDFONTROM_H")
    lines.append("")
    lines.append("namespace FlightBox {")
    lines.append("")
    lines.append(f"inline constexpr int kFontGlyphs = {len(glyphs)};")
    lines.append(f"inline constexpr int kFontTile = {CELL};")
    lines.append("")
    charset_escaped = CHARSET.replace("\\", "\\\\").replace('"', '\\"').replace("\u00b0", "\\xb0")
    lines.append(f'inline const char kFontCharset[] = "{charset_escaped}";')
    lines.append("")
    lines.append("inline const unsigned char kFontGlyphRom[kFontGlyphs][kFontTile][kFontTile] = {")
    for ch, img in zip(CHARSET, glyphs):
        label = ch if ch not in ('"', "\\") else ("\\" + ch)
        if ch == " ":
            label = "SPACE"
        lines.append(f"    /* '{label}' */")
        lines.append("    {")
        px = list(img.getdata())
        for row in range(CELL):
            rowvals = px[row * CELL:(row + 1) * CELL]
            lines.append("        {" + ", ".join(str(v) for v in rowvals) + "},")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("} // namespace FlightBox")
    lines.append("#endif")
    lines.append("")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--font", default="B612Mono-Bold", help="font stem under sim/tools/fonts/ (.ttf)")
    ap.add_argument("--preview", type=Path, default=None, help="also write a PNG atlas preview")
    args = ap.parse_args()

    font_path = FONTS_DIR / f"{args.font}.ttf"
    if not font_path.exists():
        sys.exit(f"font not found: {font_path}")

    font, ascent, descent = fit_font_size(font_path)
    glyphs = [bake_glyph(font, ascent, ch) for ch in CHARSET]

    OUT_PATH.write_text(emit_header(glyphs, font_path.stem))
    print(f"wrote {OUT_PATH} ({len(glyphs)} glyphs, {CELL}x{CELL}, font size {font.size}px)")

    if args.preview:
        atlas = Image.new("L", (CELL * len(glyphs), CELL), 0)
        for i, g in enumerate(glyphs):
            atlas.paste(g, (i * CELL, 0))
        scale = 8
        atlas.resize((atlas.width * scale, atlas.height * scale), Image.NEAREST).save(args.preview)
        print(f"wrote preview {args.preview}")


if __name__ == "__main__":
    main()
