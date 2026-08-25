#ifndef OUTSHINE_UI_TYPEFACE_H
#define OUTSHINE_UI_TYPEFACE_H

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "Layout.h"

struct TTF_Font;

namespace outshine::Ui {

enum class Family : uint8_t { Sans, Serif, Mono, kCount };

[[nodiscard]] Family FamilyNamed(std::string_view declared);
[[nodiscard]] const char *FileOf(Family family);

class Typeface final : public Font {
public:
  Typeface(void) = default;
  ~Typeface(void) override;
  Typeface(const Typeface &) = delete;
  Typeface &operator=(const Typeface &) = delete;

  [[nodiscard]] bool Opens(std::string_view fonts, std::string &error);

  [[nodiscard]] FontMetrics At(double sizePx, Family family) const override;
  [[nodiscard]] Glyph Shape(char32_t code, double sizePx, Family family) const override;

  [[nodiscard]] const uint8_t *Sheet(void) const override { return Rgba_.data(); }
  [[nodiscard]] int SheetWidthPx(void) const override { return SheetW_; }
  [[nodiscard]] int SheetHeightPx(void) const override { return SheetH_; }
  [[nodiscard]] uint64_t Cut(void) const override { return Cut_; }

private:
  struct Cell {
    float U0 = 0, V0 = 0, U1 = 0, V1 = 0;
    float LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
    float AdvancePx = 0;
    bool Drawn = false;
  };

  [[nodiscard]] TTF_Font *Set(Family family, int sizePx) const;
  [[nodiscard]] const Cell &Cell0f(Family family, int sizePx, char32_t code) const;
  [[nodiscard]] bool Packs(int widthPx, int heightPx, int &leftPx, int &topPx) const;

  std::string Under_;
  mutable std::map<uint64_t, TTF_Font *> Sets_;
  mutable std::map<uint64_t, Cell> Cells_;

  mutable std::vector<uint8_t> Rgba_;
  mutable int SheetW_ = 0, SheetH_ = 0;
  mutable int ShelfX_ = 0, ShelfY_ = 0, ShelfTall_ = 0;
  mutable uint64_t Cut_ = 0;

  bool Started_ = false;
};

}
#endif
