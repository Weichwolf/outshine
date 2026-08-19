/* WHAT A DECLARATION SAYS ABOUT A BOX, AND WHERE THE CASCADE ENDS.
 *
 * **A property is an enumeration and not a string** (board:1442). A stylesheet is read once into
 * values a layout can use, so nothing downstream re-parses `20px` and nothing carries a name where a
 * number belongs -- which is the same rule the frame path already lives under.
 *
 * **THE SELECTOR SUBSET IS THE ONE THE CORPUS WRITES**: a comma-separated list of compound selectors --
 * a tag name, `.class` and `#id` in any combination -- with the descendant combinator, ranked by CSS
 * specificity and then by document order. [MEASURED] `div.flexbox` and `#circles, #circles div` both
 * appear in the first two files of the corpus, so this is what a reader must hold to rank the corpus's
 * own rules; `>`, `+`, `~`, attribute selectors and pseudo-classes are outside it and named so in
 * `board:1442`.
 *
 * **A PROPERTY THIS ENGINE DOES NOT HOLD IS COUNTED, NEVER DROPPED SILENTLY.** The count is what the
 * corpus selection reads: a pair is inside the subset when neither of its files declares a property
 * this reader had to skip, which makes the selection a FUNCTION of this header rather than a list
 * somebody keeps. */
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

/* ONE VALUE, AND ITS UNIT TRAVELS WITH IT. A length that lost its unit is the defect this engine
 * refuses everywhere else, and a percentage resolved too early is the same mistake in a shorter
 * sentence -- so nothing here is resolved against a container until the layout has one. */
struct Value {
  Unit How = Unit::None;
  double Number = 0;      /* pixels, percent, em or a plain number */
  uint32_t Word = 0;      /* a keyword's own hash, or a packed rgba where `How` is `Colour` */
};

/* WHICH KEYWORD, AS A VALUE RATHER THAN A STRING. The hash is of the lowered spelling, so a keyword
 * comparison is an integer and the spelling stays readable at the call site. */
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

/* A COMPOUND SELECTOR: a tag, any number of classes and at most one id, all of which must hold. */
struct Compound {
  std::string Tag;                    /* empty where the selector names no element type */
  std::vector<std::string> Classes;
  std::string Id;
};

/* ONE RULE: a chain of compounds joined by descent -- the last is the subject -- and what it declares. */
struct Rule {
  std::vector<Compound> Chain;
  std::vector<Declaration> Declares;
  int Specificity = 0;   /* ids * 10000 + classes * 100 + tags, which is CSS's own ordering */
  int Order = 0;         /* where it appeared, so equal specificity falls to the later rule */
};

class Stylesheet {
public:
  /* Reads a sheet. A rule this engine cannot express is counted and skipped; a property it does not
   * hold is counted too, and both counts are what the corpus selection reads. */
  void Read(std::string_view css);
  /* An element's own `style` attribute, which outranks every rule. */
  [[nodiscard]] std::vector<Declaration> Inline(std::string_view text) const;

  [[nodiscard]] const std::vector<Rule> &Rules(void) const { return Rules_; }
  [[nodiscard]] size_t PropertiesOutsideTheSubset(void) const { return Unheld_; }
  [[nodiscard]] size_t SelectorsOutsideTheSubset(void) const { return Unselectable_; }
  [[nodiscard]] const std::vector<std::string> &NamesOutsideTheSubset(void) const { return Names_; }

private:
  std::vector<Rule> Rules_;
  std::vector<std::string> Names_;
  size_t Unheld_ = 0;
  size_t Unselectable_ = 0;
  int Order_ = 0;
};

/* WHICH PROPERTY A NAME IS, or `kCount` where this engine does not hold it. */
[[nodiscard]] Property PropertyNamed(std::string_view name);
/* Whether a rule's chain selects this node of this tree. */
[[nodiscard]] bool Selects(const Rule &rule, const Markup &markup, int node);

} // namespace outshine::Ui
#endif
