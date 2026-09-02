#ifndef OUTSHINE_UI_TYPEFACE_H
#define OUTSHINE_UI_TYPEFACE_H

#include <array>
#include <cstdint>
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
  Typeface() = default;
  ~Typeface() override;
  Typeface(const Typeface &) = delete;
  Typeface &operator=(const Typeface &) = delete;

  [[nodiscard]] bool Opens(std::string_view fonts, std::string &error);

  [[nodiscard]] FontMetrics At(FontFace face) const override;
  [[nodiscard]] Glyph Shape(char32_t code, FontFace face) const override;

  [[nodiscard]] const uint8_t *Sheet() const override { return Rgba_.data(); }

  [[nodiscard]] int SheetWidthPx() const override { return SheetW_; }

  [[nodiscard]] int SheetHeightPx() const override { return SheetH_; }

  [[nodiscard]] uint64_t Cut() const override { return Cut_; }

  [[nodiscard]] uint64_t Opened() const { return Opened_; }

  [[nodiscard]] uint64_t Missed() const { return Missed_; }

  [[nodiscard]] size_t Cells() const { return Held_; }

private:
  struct Cell {
    uint64_t Key = 0;
    float U0 = 0, V0 = 0, U1 = 0, V1 = 0;
    float LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
    float AdvancePx = 0;
    bool Drawn = false;
    bool Held = false;
  };

  [[nodiscard]] TTF_Font *Set(Family family, int sizePx) const;
  [[nodiscard]] const Cell &Cell0f(Family family, int sizePx, char32_t code) const;
  [[nodiscard]] bool Packs(int widthPx, int heightPx, int &leftPx, int &topPx) const;

  struct Sized {
    uint64_t Key = 0;
    TTF_Font *Set = nullptr;
  };

  std::string Under_;
  std::array<std::vector<uint8_t>, static_cast<size_t>(Family::kCount)> Faces_;
  mutable std::vector<Sized> Sets_;

  mutable std::vector<Cell> Cells_;
  mutable size_t Held_ = 0;

  mutable std::vector<uint8_t> Rgba_;
  int SheetW_ = 0, SheetH_ = 0;
  mutable int ShelfX_ = 0, ShelfY_ = 0, ShelfTall_ = 0;
  mutable uint64_t Cut_ = 0, Opened_ = 0, Missed_ = 0;

  bool Started_ = false;
};

} // namespace outshine::Ui
#endif
