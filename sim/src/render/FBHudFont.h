/* FlightBox — FBHudFont: the generic bitmap-font rendering system every module's HUD text goes through.
 * Glyphs are 16x16 8-bit COVERAGE bitmaps (true box-filtered area coverage, not a 1bpp mask) baked from
 * B612 Mono (SIL OFL 1.1 -- Airbus's own cockpit-display typeface) by sim/tools/bake_hud_font.py; the
 * GENERATED ROM data (charset/tile size/coverage array) lives in FBHudFontRom.h, this file is the
 * hand-maintained atlas-layout + glyph-quad-builder logic, baked once into an atlas texture
 * (FBHudStage::Init) and blitted as textured quads. Render-backend resource: the atlas layout (glyph
 * count/tile size/advance) is shared by every consumer that blits HUD glyphs (FBHudGeometry::Text,
 * FBHudStage's loading screen). Airframe-agnostic by design -- a module wanting chip-specific quirks
 * (interlace jitter, brightness curve, ...) hooks in from ITS OWN class (e.g. modules/f16's
 * FBF16Max7456), not by forking this file.
 *
 * kFontAdvance/kFontQuadSize are the PUBLIC screen-pixel-equivalent units every caller already uses --
 * unchanged by the font upgrade, so on-screen text size/baseline/advance stay put; only the raster
 * resolution behind them grew (8x8 -> 16x16). Each atlas tile carries a 1-texel transparent gutter
 * around the kFontTile x kFontTile bitmap (kFontTilePad) so LINEAR sampling never blends across a
 * glyph boundary; FBHudStage's WGSL turns that gutter + LINEAR into real coverage antialiasing (a
 * screen-footprint-aware "sharp bilinear" reconstruction), not just a blocky NEAREST blit.
 *
 * Pure data + a free function appending glyph quads to a caller-owned vector -- no globals, no GL. */
#ifndef FBHUDFONT_H
#define FBHUDFONT_H

#include <cstring>
#include <vector>

#include "FBHudFontRom.h"

namespace FlightBox {

/* Atlas tile = the baked kFontTile x kFontTile coverage bitmap plus a 1-texel transparent gutter on
 * every side, so a glyph's own edge texels stay reachable under LINEAR filtering without ever sampling
 * a neighbour's ink. */
inline constexpr int kFontTilePad = kFontTile + 2;
inline constexpr int kFontAtlasW = kFontGlyphs * kFontTilePad;
inline constexpr int kFontAtlasH = kFontTilePad;
/* Advance kept at 4*s so text positions line up with the old vector font; the drawn tile is a touch
 * larger (6*s) so the glyph reads at the established HUD text height (content < advance -> a clear
 * gap -- see bake_hud_font.py's SAFE_X_FRACTION, which bakes ink to respect this same ratio). */
inline constexpr float kFontAdvance = 4.0f;
inline constexpr float kFontQuadSize = 6.0f;

/* Blit one glyph: 6 verts (2 tris), x,y,u,v,r,g,b. (x,y)-(x+qs,y+qs) is the CONTENT box (unchanged
 * baseline/advance); the quad is expanded by one texel's worth of screen pixels on every side and
 * the UV widened to the padded atlas tile's full extent (gutter included) so the fragment shader's
 * coverage reconstruction sees real (zero) data past the bitmap's edge instead of a clamped repeat of
 * the edge texel -- that's what lets edge-touching ink reach full coverage while the outside falls off
 * cleanly to 0, with no bleed into the neighbouring glyph's tile. */
inline void FBHudFontAppendGlyph(std::vector<float> &out, float x, float y, float qs, int gi, float r,
                                  float g, float b) {
  float texel = qs / (float)kFontTile;   /* screen px per atlas texel at this glyph's scale */
  float u0 = (float)(gi * kFontTilePad) / (float)kFontAtlasW,
        u1 = (float)(gi * kFontTilePad + kFontTilePad) / (float)kFontAtlasW;
  float v0 = 0.0f, v1 = 1.0f;
  float x0 = x - texel, y0 = y - texel, x1 = x + qs + texel, y1 = y + qs + texel;
  auto vert = [&](float px, float py, float pu, float pv) {
    out.insert(out.end(), {px, py, pu, pv, r, g, b});
  };
  vert(x0, y0, u0, v0);
  vert(x1, y0, u1, v0);
  vert(x1, y1, u1, v1);
  vert(x0, y0, u0, v0);
  vert(x1, y1, u1, v1);
  vert(x0, y1, u0, v1);
}

/* Appends a whole string as glyph quads (drop-in for the old mx_text contract: x,y top-left, scale s,
 * colour, string). Unknown/lowercase-folded chars fall to the blank tile (index 0). */
inline void FBHudFontAppendText(std::vector<float> &out, float x, float y, float s, float r, float g,
                                 float b, const char *text) {
  float adv = kFontAdvance * s, qs = kFontQuadSize * s;
  for (; *text; text++) {
    char u = *text;
    if (u >= 'a' && u <= 'z') u -= 32;
    const char *p = strchr(kFontCharset, u);
    int gi = p ? (int)(p - kFontCharset) : 0;
    if (gi > 0) FBHudFontAppendGlyph(out, x, y, qs, gi, r, g, b);
    x += adv;
  }
}

} // namespace FlightBox
#endif /* FBHUDFONT_H */
