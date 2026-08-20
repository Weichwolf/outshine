/* THE BROWSER'S OWN FACE, AND IT IS THE CLIENT'S ASSET (board:1447, board:1442).
 *
 * **WHO MAKES AN ASSET IS NOT THE ENGINE'S BUSINESS, AND A FONT IS AN ASSET.** The library declares an
 * interface a face fits through -- an advance per glyph and a patch of an atlas -- and this is one
 * face going through it. Nothing here is engine code and nothing here is general: it is 5x7 cells for
 * the characters a case id and a count are written in, which is what this browser needs and no more.
 *
 * **AHEM COULD NOT HAVE BEEN THE ANSWER.** The measurement face draws every glyph as a filled square,
 * which is exactly right for saying where a box landed and useless for reading a name. A browser whose
 * labels cannot be read is a browser that has not been built.
 *
 * **THE LABELS ARE UPPERCASED, WHICH IS A CLIENT'S CHOICE AND SAYS SO.** Forty-four glyphs is a table
 * somebody can check by eye; ninety-five is one somebody hopes about. A case id is
 * `align-content-vert-001a`, and it is legible in capitals.
 *
 * **THE CELL IS 5x7 IN A 6x8 BOX**, so the spacing is in the cell and the advance is the cell -- one
 * number, and no kerning table to keep true. */
#ifndef VIEWER_GLYPHSHEET_H
#define VIEWER_GLYPHSHEET_H

#include <cstdint>
#include <string>
#include <vector>

#include "Layout.h"

namespace outshine::Viewer {

inline constexpr int kCellW = 6;
inline constexpr int kCellH = 8;
inline constexpr int kInkW = 5;
inline constexpr int kInkH = 7;
inline constexpr int kColumns = 16;

/* THE ALPHABET, IN THE ORDER ITS ROWS APPEAR BELOW. A character outside it draws nothing, which is
 * how an unexpected byte costs a blank rather than a wrong glyph. */
inline constexpr const char *kAlphabet = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._/:()%+*<>=#";

/* SEVEN ROWS OF FIVE BITS, HIGH BIT LEFT. Written out rather than generated because a glyph is a
 * picture and a picture is checked by looking at it. */
inline constexpr uint8_t kInk[][kInkH] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  /*   */
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},  /* A */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},  /* B */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},  /* C */
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},  /* D */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},  /* E */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},  /* F */
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E},  /* G */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},  /* H */
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},  /* I */
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C},  /* J */
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},  /* K */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},  /* L */
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},  /* M */
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},  /* N */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},  /* O */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},  /* P */
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},  /* Q */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},  /* R */
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},  /* S */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},  /* T */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},  /* U */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},  /* V */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11},  /* W */
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},  /* X */
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},  /* Y */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},  /* Z */
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},  /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},  /* 1 */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},  /* 2 */
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},  /* 3 */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},  /* 4 */
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},  /* 5 */
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},  /* 6 */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},  /* 7 */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},  /* 8 */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},  /* 9 */
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},  /* - */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C},  /* . */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F},  /* _ */
    {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10},  /* / */
    {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00},  /* : */
    {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02},  /* ( */
    {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08},  /* ) */
    {0x19, 0x1A, 0x02, 0x04, 0x08, 0x0B, 0x13},  /* % */
    {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00},  /* + */
    {0x00, 0x0A, 0x04, 0x1F, 0x04, 0x0A, 0x00},  /* * */
    {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02},  /* < */
    {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08},  /* > */
    {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00},  /* = */
    {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A},  /* # */
};

/* THE TWO HALVES OF ONE FACT CANNOT DISAGREE. The alphabet says which characters exist and the table
 * says what they look like; a row added to one and not the other would shift every glyph after it, and
 * a shifted alphabet is unreadable in a way no test asserts and every reader sees. */
constexpr size_t kInkRows = sizeof(kInk) / sizeof(kInk[0]);
constexpr size_t kAlphabetLength(void) {
  size_t at = 0;
  while (kAlphabet[at] != '\0') { ++at; }
  return at;
}
static_assert(kInkRows == kAlphabetLength(), "every character of the alphabet owns exactly one row");

/* WHERE A CHARACTER SITS IN THE TABLE, or 0 -- the blank -- for anything the table does not carry. */
[[nodiscard]] inline int SlotOf(char32_t code) {
  char32_t upper = code;
  if (upper >= U'a' && upper <= U'z') { upper = upper - U'a' + U'A'; }
  for (int at = 0; kAlphabet[at] != '\0'; ++at) {
    if ((char32_t)kAlphabet[at] == upper) { return at; }
  }
  return 0;
}

[[nodiscard]] inline int SlotCount(void) {
  int count = 0;
  while (kAlphabet[count] != '\0') { ++count; }
  return count;
}

[[nodiscard]] inline int AtlasWidth(void) { return kColumns * kCellW; }
[[nodiscard]] inline int AtlasRows(void) { return (SlotCount() + kColumns - 1) / kColumns; }
[[nodiscard]] inline int AtlasHeight(void) { return AtlasRows() * kCellH; }

/* THE SHEET AS TEXELS: white everywhere, and the ink is the ALPHA. A glyph then takes the colour the
 * declaration gave its text, which is what makes one atlas serve every colour in the interface. */
[[nodiscard]] inline std::vector<uint8_t> Sheet(void) {
  std::vector<uint8_t> rgba((size_t)AtlasWidth() * (size_t)AtlasHeight() * 4u, 0u);
  for (int slot = 0; slot < SlotCount(); ++slot) {
    const int col = slot % kColumns, row = slot / kColumns;
    for (int y = 0; y < kInkH; ++y) {
      for (int x = 0; x < kInkW; ++x) {
        if ((kInk[slot][y] & (1u << (kInkW - 1 - x))) == 0u) { continue; }
        const size_t at =
            (((size_t)(row * kCellH + y) * (size_t)AtlasWidth()) + (size_t)(col * kCellW + x)) * 4u;
        rgba[at + 0] = 255;
        rgba[at + 1] = 255;
        rgba[at + 2] = 255;
        rgba[at + 3] = 255;
      }
    }
  }
  return rgba;
}

/* THE FACE, THROUGH THE LIBRARY'S OWN INTERFACE. Its natural size is the cell's height, and every
 * other size is that scaled -- so a declaration asking for 16 px gets exactly two texels per texel and
 * the sheet stays crisp. */
struct SheetFont final : Ui::Font {
  [[nodiscard]] Ui::FontMetrics At(double sizePx) const override {
    const double scale = sizePx / (double)kCellH;
    return {(double)kCellW * scale, (double)kInkH * scale, ((double)kCellH - kInkH) * scale};
  }
  [[nodiscard]] Ui::Glyph Shape(char32_t code, double sizePx) const override {
    const int slot = SlotOf(code);
    const double scale = sizePx / (double)kCellH;
    Ui::Glyph glyph;
    glyph.AdvancePx = (double)kCellW * scale;
    if (slot == 0) { return glyph; }
    const int col = slot % kColumns, row = slot / kColumns;
    glyph.WidthPx = (double)kInkW * scale;
    glyph.HeightPx = (double)kInkH * scale;
    glyph.U0 = (double)(col * kCellW) / (double)AtlasWidth();
    glyph.V0 = (double)(row * kCellH) / (double)AtlasHeight();
    glyph.U1 = (double)(col * kCellW + kInkW) / (double)AtlasWidth();
    glyph.V1 = (double)(row * kCellH + kInkH) / (double)AtlasHeight();
    glyph.Drawn = true;
    return glyph;
  }
};

} // namespace outshine::Viewer
#endif
