#ifndef UI_STYLE_H
#define UI_STYLE_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Markup.h"

namespace outshine::Ui {

enum class Property : uint8_t {
  Display, Position, BoxSizing, Overflow,
  FlexDirection, FlexWrap, JustifyContent, AlignItems, AlignSelf, AlignContent,
  FlexGrow, FlexShrink, FlexBasis, Gap, RowGap, ColumnGap,
  Width, Height, MinWidth, MaxWidth, MinHeight, MaxHeight,
  MarginTop, MarginRight, MarginBottom, MarginLeft,
  PaddingTop, PaddingRight, PaddingBottom, PaddingLeft,
  BorderTopWidth, BorderRightWidth, BorderBottomWidth, BorderLeftWidth,
  Top, Right, Bottom, Left,
  BackgroundColour, BorderColour, BorderRadius, Opacity, Colour,
  FontSize, LineHeight, TextAlign, WhiteSpace, FontFamily,
  kCount
};

enum class Unit : uint8_t { None, Pixels, Percent, Em, Rem, Auto, Keyword, Colour };

struct Value {
  Unit How = Unit::None;

  bool Prefixed = false;
  double Number = 0;
  uint32_t Word = 0;
};

[[nodiscard]] constexpr uint32_t Keyword(std::string_view word) {
  uint32_t hash = 2166136261u;
  for (const char c : word) {
    hash ^= (uint32_t)(unsigned char)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    hash *= 16777619u;
  }
  return hash;
}

struct Declaration {
  Property What = Property::kCount;
  Value How;
};

struct Compound {

  int NthChild = 0;

  bool Universal = false;
  std::string Tag;
  std::vector<std::string> Classes;
  std::string Id;
};

enum class Reach : uint8_t { Descendant, Child };

struct Rule {
  std::vector<Compound> Chain;

  std::vector<Reach> Links;
  std::vector<Declaration> Declares;
  int Specificity = 0;
  int Order = 0;
};

class Stylesheet {
public:

  void Read(std::string_view text);

  [[nodiscard]] std::vector<Declaration> Inline(std::string_view text);

  [[nodiscard]] const std::vector<Rule> &Rules() const { return Rules_; }
  [[nodiscard]] size_t PropertiesOutsideTheSubset() const { return Unheld_; }
  [[nodiscard]] size_t SelectorsOutsideTheSubset() const { return Unselectable_; }
  [[nodiscard]] const std::vector<std::string> &NamesOutsideTheSubset() const { return Names_; }

private:
  std::vector<Rule> Rules_;
  std::vector<std::string> Names_;
  size_t Unheld_ = 0;
  size_t Unselectable_ = 0;
  int Order_ = 0;
};

[[nodiscard]] Property PropertyNamed(std::string_view name);

[[nodiscard]] bool Selects(const Rule &rule, const Markup &markup, int node);

[[nodiscard]] const char *WhyOutside(std::string_view name);

}
#endif
