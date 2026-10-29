#ifndef OUTSHINE_TEST_AHEMFONT_H
#define OUTSHINE_TEST_AHEMFONT_H

#include "Font.h"

namespace outshine::Test {

struct AhemFont final : Ui::Font {
  [[nodiscard]] Ui::FontMetrics At(Ui::FontFace face) const override {
    const double sizePx = face.SizePx;
    return {.Advance = sizePx,
            .Ascent = sizePx * Ui::kAscentShare,
            .Descent = sizePx * Ui::kDescentShare};
  }

  [[nodiscard]] Ui::Glyph Shape(char32_t code, Ui::FontFace face) const override {
    const double sizePx = face.SizePx;
    if (code == U' ') {
      return {.LeftPx = 0,
              .TopPx = 0,
              .WidthPx = 0,
              .HeightPx = 0,
              .U0 = 0,
              .V0 = 0,
              .U1 = 0,
              .V1 = 0,
              .AdvancePx = sizePx,
              .Drawn = false};
    }
    return {.LeftPx = 0.0,
            .TopPx = 0.0,
            .WidthPx = sizePx,
            .HeightPx = sizePx,
            .U0 = 0,
            .V0 = 0,
            .U1 = 0,
            .V1 = 0,
            .AdvancePx = sizePx,
            .Drawn = true};
  }
};

}
#endif
