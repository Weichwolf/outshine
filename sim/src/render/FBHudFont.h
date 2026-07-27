/* The generic, AIRFRAME-AGNOSTIC bitmap-font system every module's HUD text goes through: pure data
 * plus a free function appending glyph quads to a caller-owned vector. The hand-maintained half —
 * atlas layout and quad builder; the baked coverage ROM is the generated FBHudFontRom.h.
 *
 * A module wanting chip-specific quirks (interlace jitter, brightness curve) hooks in from ITS OWN
 * class (modules/f16/FBF16Max7456), never by forking this file.
 * Masse, Herkunft und die "sharp bilinear"-Deckung: doc/flightbox/rendering.md, Abschnitt 7.3. */
#ifndef FBHUDFONT_H
#define FBHUDFONT_H

#include <cstring>
#include <vector>

#include "FBHudFontRom.h"

namespace FlightBox {

/* The 1-texel transparent gutter keeps a glyph's edge texels reachable under LINEAR filtering
 * without ever sampling a neighbour's ink. */
inline constexpr int kFontTilePad = kFontTile + 2;
inline constexpr int kFontAtlasW = kFontGlyphs * kFontTilePad;
inline constexpr int kFontAtlasH = kFontTilePad;
/* The drawn tile is LARGER than the advance, but the ink is narrower (bake_hud_font.py's
 * SAFE_X_FRACTION respects the same ratio), so content < advance leaves a clear gap. */
inline constexpr float kFontAdvance = 4.0f;
inline constexpr float kFontQuadSize = 6.0f;

/* (x,y)-(x+qs,y+qs) is the CONTENT box; the quad is expanded one texel beyond it on every side and
 * the UV widened over the gutter, so the coverage reconstruction sees real ZEROES past the bitmap's
 * edge instead of a clamped repeat of the edge texel. */
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

/* x,y top-left. Unknown and lowercase-folded characters fall to the blank tile. */
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
