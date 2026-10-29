#ifndef OUTSHINE_UI_FONT_H
#define OUTSHINE_UI_FONT_H

#include <cstdint>

namespace outshine::Ui {

constexpr double kAscentShare = 0.8;
constexpr double kDescentShare = 0.2;

struct FontMetrics {
  double Advance = 0;
  double Ascent = 0;
  double Descent = 0;
};

struct Glyph {
  double LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
  double U0 = 0, V0 = 0, U1 = 0, V1 = 0;

  double AdvancePx = 0;
  bool Drawn = false;
};

enum class Family : uint8_t { Sans, Serif, Mono, kCount };

[[nodiscard]] Family FamilyOf(uint32_t declared);

struct FontFace {
  Family Name = Family{};
  double SizePx = 0;
};

struct Font {
  virtual ~Font() = default;
  [[nodiscard]] virtual FontMetrics At(FontFace face) const = 0;

  [[nodiscard]] virtual Glyph Shape(char32_t code, FontFace face) const {
    (void)code;
    (void)face;
    return {};
  }

  [[nodiscard]] virtual const uint8_t *Sheet() const { return nullptr; }

  [[nodiscard]] virtual int SheetWidthPx() const { return 0; }

  [[nodiscard]] virtual int SheetHeightPx() const { return 0; }

  [[nodiscard]] virtual uint64_t Cut() const { return 0; }
};

}
#endif
