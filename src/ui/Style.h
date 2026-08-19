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
  /* THE WORD CARRIED A VENDOR PREFIX. It is dropped like a prefixed property name and for the same
   * reason: it is written beside the standard word by an author covering an older engine, and it says
   * nothing about what this one can do. */
  bool Prefixed = false;
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
  /* `:nth-child(N)` WITH AN INTEGER, AND ONLY THAT. The `an+b` formula is a second grammar and is
   * named outside the subset; the corpus writes the integer form and this holds it. Zero means the
   * selector said nothing about position, which is what every selector without it says. */
  int NthChild = 0;
  /* `*` -- matches every element and adds nothing to specificity, which is CSS's own arithmetic. */
  bool Universal = false;
  std::string Tag;                    /* empty where the selector names no element type */
  std::vector<std::string> Classes;
  std::string Id;
};

/* ONE RULE: a chain of compounds joined by descent -- the last is the subject -- and what it declares. */
/* HOW A COMPOUND REACHES THE ONE AFTER IT. `Descendant` is any ancestor and `Child` is the immediate
 * parent -- the two CSS spells with a space and with `>`. The sibling combinators are outside the
 * subset and stay outside: they need the tree walked sideways, which no consumer declaration in this
 * engine has asked for and which the corpus's own count can say when one does. */
enum class Reach : uint8_t { Descendant, Child };

struct Rule {
  std::vector<Compound> Chain;
  /* One per link BETWEEN compounds, so it is one shorter than the chain -- the subject has nothing to
   * its right to reach. */
  std::vector<Reach> Links;
  std::vector<Declaration> Declares;
  int Specificity = 0;   /* ids * 10000 + classes * 100 + tags, which is CSS's own ordering */
  int Order = 0;         /* where it appeared, so equal specificity falls to the later rule */
};

class Stylesheet {
public:
  /* Reads a sheet. A rule this engine cannot express is counted and skipped; a property it does not
   * hold is counted too, and both counts are what the corpus selection reads. */
  void Read(std::string_view text);
  /* An element's own `style` attribute, which outranks every rule. IT IS NOT `const`, AND THAT IS THE
   * POINT: what it cannot hold is counted into the same two tallies a rule's would be. [MEASURED] the
   * count was discarded into a local, so `writing-mode` written inline read as a declaration fully
   * inside the subset -- an error in OUR FAVOUR, which is the direction a coverage number must never
   * be wrong in (board:1445). */
  [[nodiscard]] std::vector<Declaration> Inline(std::string_view text);

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

/* WHY A CAPABILITY IS DELIBERATELY OUTSIDE, or `nullptr` where it is not deliberate at all
 * (board:1442).
 *
 * **THIS IS THE DIFFERENCE BETWEEN A BOUNDARY AND A GAP, and a suite that cannot tell them apart is a
 * suite whose second number means nothing.** A case this engine declines because it will never do
 * floats is a case that is FINISHED; a case it declines because nobody has written `flex-basis` yet is
 * a case that is waiting. Both read as *outside the subset* to a counter, and only this table
 * separates them.
 *
 * **THE REASON IS REQUIRED AND IT IS THE POINT.** An entry with no argument beside it is a
 * disqualification wearing a softer word -- the same rule the picture corpus's reductions carry, and
 * for the same reason.
 *
 * The name is what the reader published: a property, a `property:value`, a selector, an `<element>`,
 * or a sentence the harness wrote. A row matches a name it equals or is a prefix of. */
[[nodiscard]] const char *WhyOutside(std::string_view name);

} // namespace outshine::Ui
#endif
