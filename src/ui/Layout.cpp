#include "Layout.h"

#include <algorithm>
#include <cmath>

namespace outshine::Ui {

namespace {

constexpr uint32_t kDisplayBlock = Keyword("block");
constexpr uint32_t kDisplayFlex = Keyword("flex");
constexpr uint32_t kDisplayInlineFlex = Keyword("inline-flex");
constexpr uint32_t kBaseline = Keyword("baseline");
constexpr uint32_t kDisplayNone = Keyword("none");
constexpr uint32_t kDisplayInline = Keyword("inline");
constexpr uint32_t kColumn = Keyword("column");
constexpr uint32_t kColumnReverse = Keyword("column-reverse");
constexpr uint32_t kRowReverse = Keyword("row-reverse");
constexpr uint32_t kFlexEnd = Keyword("flex-end");
constexpr uint32_t kCentre = Keyword("center");
constexpr uint32_t kSpaceBetween = Keyword("space-between");
constexpr uint32_t kSpaceAround = Keyword("space-around");
constexpr uint32_t kStretch = Keyword("stretch");
constexpr uint32_t kBorderBox = Keyword("border-box");
constexpr uint32_t kHidden = Keyword("hidden");
constexpr uint32_t kPre = Keyword("pre");
constexpr uint32_t kRight = Keyword("right");
constexpr uint32_t kSpaceEvenly = Keyword("space-evenly");
/* `start` AND `end` ARE CSS BOX ALIGNMENT'S OWN WORDS, and they are not spare spellings of the flex
 * ones: `flex-start` is the start of the FLEX direction and `start` is the start of the WRITING
 * direction, which differ under `row-reverse` and under a right-to-left flow. This engine holds
 * neither of those -- both are named outside the subset -- so within what it does hold the two
 * coincide, and mapping them is exact rather than approximate. The day a reversed direction arrives,
 * this mapping is where it stops being exact, and that is written here so the next reader finds it. */
constexpr uint32_t kStart = Keyword("start");
constexpr uint32_t kEnd = Keyword("end");
constexpr uint32_t kWrap = Keyword("wrap");
constexpr uint32_t kWrapReverse = Keyword("wrap-reverse");

/* THE ALIGNMENT WORD, IN ONE SPELLING. Every place that compares against `kFlexEnd` would otherwise
 * have to compare against `kEnd` too, and the one that forgot would be a keyword that silently did
 * nothing -- which is exactly how `space-evenly` read as `flex-start` for a whole corpus run.
 *
 * **`start` AND `end` ARE PHYSICAL AND `flex-start` AND `flex-end` ARE NOT**, and under
 * `wrap-reverse` the two disagree: the cross axis is mirrored, so the line the FLEX axis starts with
 * is the one that ends up physically last. [MEASURED] `align-content: start` in a reversed container
 * states x = 8 -- the physical left -- and mapping it to `flex-start` answered 198. Everything below
 * is computed in flex coordinates and mirrored once at the end, so translating the physical word into
 * the flex one HERE is what makes the single mirror correct for both.
 *
 * The symmetric words -- `center` and the three distributions -- are unchanged by a mirror and need no
 * arm, which is why there is none. */
[[nodiscard]] constexpr uint32_t Aligned(uint32_t word, bool reversed) {
  if (word == kStart) { return reversed ? kFlexEnd : 0u; }
  if (word == kEnd) { return reversed ? 0u : kFlexEnd; }
  return word;
}

/* EVERY DECLARATION AN ELEMENT ENDED UP WITH, one slot per property. A slot nobody set stays unset,
 * which is how *the author said nothing* stays distinguishable from *the author said zero*. */
struct Computed {
  Value Held[(size_t)Property::kCount];
  bool Set[(size_t)Property::kCount] = {};

  void Take(const Declaration &one) {
    Held[(size_t)one.What] = one.How;
    Set[(size_t)one.What] = true;
  }
  [[nodiscard]] bool Has(Property what) const { return Set[(size_t)what]; }
  [[nodiscard]] Value Of(Property what) const { return Held[(size_t)what]; }
  [[nodiscard]] uint32_t Word(Property what, uint32_t fallback) const {
    return Set[(size_t)what] && Held[(size_t)what].How == Unit::Keyword ? Held[(size_t)what].Word
                                                                        : fallback;
  }
  [[nodiscard]] double Number(Property what, double fallback) const {
    return Set[(size_t)what] && (Held[(size_t)what].How == Unit::None ||
                                 Held[(size_t)what].How == Unit::Pixels)
               ? Held[(size_t)what].Number
               : fallback;
  }
};

/* A LENGTH AGAINST THE ONE THING THAT CAN RESOLVE IT. `Auto` answers *absent*, because the difference
 * between "auto" and "unset" lives in the caller's own rule and not here. */
double Resolve(const Value &value, double against, double emPx, double rootEmPx, bool &absent) {
  absent = false;
  switch (value.How) {
    case Unit::Pixels: return value.Number;
    case Unit::Percent: return against * value.Number / 100.0;
    case Unit::Em: return emPx * value.Number;
    case Unit::Rem: return rootEmPx * value.Number;
    case Unit::None: return value.Number;
    default: break;
  }
  absent = true;
  return 0;
}

std::string Collapsed(const std::string &raw) {
  std::string out;
  bool space = false;
  for (const char c : raw) {
    const bool blank = c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
    if (blank) {
      space = true;
      continue;
    }
    if (space && !out.empty()) { out.push_back(' '); }
    space = false;
    out.push_back(c);
  }
  return out;
}

struct Placer {
  const Markup *Tree = nullptr;
  const Stylesheet *Agent = nullptr;
  Stylesheet *Author = nullptr;
  const Font *Face = nullptr;
  double RootEm = 16.0;
  std::vector<Box> *Out = nullptr;

  [[nodiscard]] Computed StyleOf(int node, const Computed *inherited) const;
  double Place(int node, const Computed *inherited, double originX, double originY,
               double containerWidth, double containerHeight, int parentBox);
  double Children(int node, const Computed &style, int self, double contentX, double contentY,
                  double contentWidth, double contentHeight, double emPx);
  double Blocks(int node, const Computed &style, int self, double contentX, double contentY,
                double contentWidth, double emPx);
  double Flex(int node, const Computed &style, int self, double contentX, double contentY,
              double contentWidth, double contentHeight, double emPx);
  double Runs(int node, const Computed &style, int self, double contentX, double contentY,
              double contentWidth, double emPx);
  /* WHAT A SUBTREE WOULD TAKE, measured by laying it out and throwing the boxes away. Flex needs an
   * item's content size before it can decide the item's size, and there is no shortcut that stays
   * true for a subtree with its own children. */
  void Measure(int node, const Computed *inherited, double availableWidth, double &width,
               double &height);
  /* THE WIDTH A SUBTREE WANTS WITH NOTHING WRAPPING -- max-content, and it is a different question
   * from *what would it take in this much room*. A flex item with no declared main size takes this
   * and NOT the room its container offers: an empty box wants nothing, and an engine that handed it
   * the container's width would report every such row overfull and shrink every sibling. */
  double MaxContent(int node, const Computed *inherited);
  /* WHERE AN ITEM'S FIRST BASELINE SITS, measured from its own top margin edge, by laying it out in
   * the width it will get. It is a trial layout and its boxes are thrown away, which is what makes a
   * baseline knowable before the item is placed. */
  double BaselineOf(int node, const Computed *inherited, double widthRoom);
  [[nodiscard]] double Clamped(double used, const Computed &style, Property least, Property most,
                               double against, double emPx) const;
  [[nodiscard]] double Width(const std::string &text, size_t from, size_t to, double emPx) const;
};

Computed Placer::StyleOf(int node, const Computed *inherited) const {
  Computed out;
  if (inherited != nullptr) {
    const Property carried[] = {Property::Colour,   Property::FontSize,  Property::LineHeight,
                                Property::TextAlign, Property::FontFamily, Property::WhiteSpace};
    for (const Property what : carried) {
      if (inherited->Has(what)) {
        out.Held[(size_t)what] = inherited->Of(what);
        out.Set[(size_t)what] = true;
      }
    }
  }
  struct Ranked {
    int Specificity;
    int Order;
    const Declaration *One;
  };
  std::vector<Ranked> found;
  const auto gather = [&](const Stylesheet &sheet, int bias) {
    for (const Rule &rule : sheet.Rules()) {
      if (!Selects(rule, *Tree, node)) { continue; }
      for (const Declaration &one : rule.Declares) {
        found.push_back({rule.Specificity + bias, rule.Order, &one});
      }
    }
  };
  /* THE USER-AGENT SHEET SITS BENEATH EVERY AUTHOR RULE, which one bias states once rather than a
   * second ranking rule stating it everywhere. */
  gather(*Agent, -1000000);
  gather(*Author, 0);
  std::stable_sort(found.begin(), found.end(), [](const Ranked &a, const Ranked &b) {
    return a.Specificity == b.Specificity ? a.Order < b.Order : a.Specificity < b.Specificity;
  });
  for (const Ranked &one : found) { out.Take(*one.One); }
  const std::string *inlineStyle = Tree->AttributeOf(node, "style");
  if (inlineStyle != nullptr) {
    for (const Declaration &one : Author->Inline(*inlineStyle)) { out.Take(one); }
  }
  return out;
}

void Placer::Measure(int node, const Computed *inherited, double availableWidth, double &width,
                     double &height) {
  std::vector<Box> scratch;
  std::vector<Box> *held = Out;
  Out = &scratch;
  Place(node, inherited, 0, 0, availableWidth, 0, -1);
  Out = held;
  width = scratch.empty() ? 0 : scratch[0].Width;
  height = scratch.empty() ? 0 : scratch[0].Height;
  /* A BLOCK WITH NO DECLARED WIDTH FILLED ITS CONTAINER, and what a flex item wants is what its
   * CONTENTS take. The widest descendant answers that, which is the shrink-to-fit rule stated for the
   * one case this subset has. */
  double widest = 0;
  for (const Box &one : scratch) {
    if (one.Parent < 0) { continue; }
    widest = std::fmax(widest, one.X - scratch[0].X + one.Width);
  }
  if (widest > 0) { width = std::fmin(width, widest); }
}

double Placer::MaxContent(int node, const Computed *inherited) {
  const Node &element = Tree->Nodes()[(size_t)node];
  if (element.Kind == NodeKind::Text) { return 0; }
  const Computed style = StyleOf(node, inherited);
  const uint32_t display = style.Word(Property::Display, kDisplayInline);
  if (display == kDisplayNone) { return 0; }

  double emPx = 16.0;
  if (style.Has(Property::FontSize)) {
    bool absent = false;
    const double found = Resolve(style.Of(Property::FontSize), 16.0, 16.0, RootEm, absent);
    if (!absent) { emPx = found; }
  }
  const auto len = [&](Property what, double fallback) {
    if (!style.Has(what)) { return fallback; }
    bool absent = false;
    /* A PERCENTAGE HAS NO ANSWER HERE, and that is the honest one: max-content is asked before any
     * container is known, so a percentage contributes nothing rather than a number invented from a
     * width nobody has yet. */
    const double found = Resolve(style.Of(what), 0.0, emPx, RootEm, absent);
    return absent ? fallback : found;
  };
  const double frame = len(Property::BorderLeftWidth, 0) + len(Property::BorderRightWidth, 0) +
                       len(Property::PaddingLeft, 0) + len(Property::PaddingRight, 0);
  const double margins = len(Property::MarginLeft, 0) + len(Property::MarginRight, 0);

  if (style.Has(Property::Width) && style.Of(Property::Width).How == Unit::Pixels) {
    const double declared = style.Of(Property::Width).Number;
    return (style.Word(Property::BoxSizing, 0) == kBorderBox ? declared : declared + frame) + margins;
  }

  double own = 0;
  const uint32_t how = style.Word(Property::FlexDirection, 0);
  const bool row = display == kDisplayFlex && how != kColumn && how != kColumnReverse;
  double along = 0;
  int items = 0;
  for (const int child : element.Children) {
    const Node &node2 = Tree->Nodes()[(size_t)child];
    if (node2.Kind == NodeKind::Text) {
      own = std::fmax(own, Width(Collapsed(node2.Text), 0, std::string::npos, emPx));
      continue;
    }
    const double child2 = MaxContent(child, &style);
    if (row) {
      along += child2;
      ++items;
    } else {
      own = std::fmax(own, child2);
    }
  }
  if (row) {
    const double gap = style.Has(Property::Gap) ? style.Of(Property::Gap).Number : 0.0;
    own = std::fmax(own, along + (items > 1 ? gap * (double)(items - 1) : 0.0));
  }
  return own + frame + margins;
}

namespace {

/* THE CODE POINTS OF A RUN, one at a time -- the same decoding the painter does, so the two halves
 * walk the same columns. */
size_t NextCodePoint(const std::string &text, size_t at, char32_t &code) {
  const unsigned char lead = (unsigned char)text[at];
  size_t length = 1;
  code = lead;
  if ((lead & 0xE0u) == 0xC0u) {
    length = 2;
    code = lead & 0x1Fu;
  } else if ((lead & 0xF0u) == 0xE0u) {
    length = 3;
    code = lead & 0x0Fu;
  } else if ((lead & 0xF8u) == 0xF0u) {
    length = 4;
    code = lead & 0x07u;
  }
  if (at + length > text.size()) { return text.size() - at; }
  for (size_t i = 1; i < length; ++i) {
    code = (code << 6) | ((unsigned char)text[at + i] & 0x3Fu);
  }
  return length;
}

} // namespace

/* HOW WIDE A STRETCH OF A RUN IS, ASKED GLYPH BY GLYPH. A single advance for every character is a
 * MONOSPACE assumption wearing the word `metrics`, and it is the one thing between a label and a page
 * of a book set in a real face. A font with nothing per-glyph to say answers zero and the size's own
 * advance is used, so the measurement face costs exactly what it did. */
double Placer::Width(const std::string &text, size_t from, size_t to, double emPx) const {
  const FontMetrics metrics = Face->At(emPx);
  double width = 0;
  for (size_t at = from; at < to && at < text.size();) {
    char32_t code = 0;
    at += NextCodePoint(text, at, code);
    const Glyph glyph = Face->Shape(code, emPx);
    width += glyph.AdvancePx > 0 ? glyph.AdvancePx : metrics.Advance;
  }
  return width;
}

/* WHAT A DECLARED MINIMUM AND MAXIMUM DO TO A USED SIZE, and the ORDER is CSS's own: the maximum is
 * applied first and the minimum second, so a box whose minimum exceeds its maximum comes out at the
 * MINIMUM. Reversing the two reads as the same rule and answers differently exactly where the two
 * declarations disagree, which is the case a corpus writes a test for.
 *
 * A LIMIT NOBODY DECLARED IS NOT A LIMIT OF ZERO. `min-*` defaults to nothing here rather than to 0,
 * because the difference only shows on a negative used size -- and a used size is never negative,
 * which is itself the floor this applies. */
double Placer::Clamped(double used, const Computed &style, Property least, Property most,
                       double against, double emPx) const {
  double out = used;
  if (style.Has(most) && style.Of(most).How != Unit::Auto) {
    bool absent = false;
    const double ceiling = Resolve(style.Of(most), against, emPx, RootEm, absent);
    if (!absent) { out = std::fmin(out, ceiling); }
  }
  if (style.Has(least) && style.Of(least).How != Unit::Auto) {
    bool absent = false;
    const double floor = Resolve(style.Of(least), against, emPx, RootEm, absent);
    if (!absent) { out = std::fmax(out, floor); }
  }
  /* A USED SIZE IS NEVER NEGATIVE. CSS floors it, and a box of negative width is a rectangle nobody
   * can draw -- it reached the corpus as `width is -80.000000` before this line existed. */
  return std::fmax(0.0, out);
}

double Placer::BaselineOf(int node, const Computed *inherited, double widthRoom) {
  const size_t before = Out->size();
  Place(node, inherited, 0, 0, widthRoom, 0, -1);
  double baseline = 0;
  if (Out->size() > before) { baseline = (*Out)[before].Baseline; }
  Out->resize(before);
  return baseline;
}

double Placer::Runs(int node, const Computed &style, int self, double contentX, double contentY,
                    double contentWidth, double emPx) {
  const double lineFactor = style.Number(Property::LineHeight, 1.2);
  const double lineHeight = lineFactor > 3.0 ? lineFactor : lineFactor * emPx;
  const FontMetrics metrics = Face->At(emPx);
  const bool keepSpace = style.Word(Property::WhiteSpace, 0) == kPre;
  const uint32_t align = style.Word(Property::TextAlign, 0);
  double y = contentY;

  for (const int child : Tree->Nodes()[(size_t)node].Children) {
    const Node &run = Tree->Nodes()[(size_t)child];
    if (run.Kind != NodeKind::Text) { continue; }
    const std::string text = keepSpace ? run.Text : Collapsed(run.Text);
    /* A RUN THAT COLLAPSES TO NOTHING BUT SPACES IS REMOVED, NOT LAID OUT. Collapsible whitespace
     * between block-level boxes carries no content, and CSS deletes it rather than giving it a line
     * box. [MEASURED] the newline and indentation an author writes between two `<div>`s produced ONE
     * line box each, stacked before every block child, and the first box of `align-content-vert-001a`
     * landed at y = 46.4 where the document states 8 -- two lines of 19.2 px that exist only in the
     * source's formatting. */
    if (text.empty() || (!keepSpace && text.find_first_not_of(' ') == std::string::npos)) {
      continue;
    }
    /* WRAPPED AT SPACES AND NOWHERE ELSE, which is what this subset declares: no hyphenation, no
     * breaking inside a word, and a word wider than the line is placed and overflows rather than
     * being cut in half. */
    size_t at = 0;
    while (at < text.size()) {
      /* WALK UNTIL THE LINE IS FULL, REMEMBERING THE LAST SPACE. A width divided by one advance
       * answers *how many characters fit* only when every character is the same width, so the walk is
       * what makes a proportional face wrap where it actually runs out of room. */
      size_t take = text.size() - at;
      if (contentWidth > 0) {
        double width = 0;
        size_t lastSpace = std::string::npos, cursor = at;
        while (cursor < text.size()) {
          char32_t code = 0;
          const size_t step = NextCodePoint(text, cursor, code);
          const Glyph glyph = Face->Shape(code, emPx);
          const double advance = glyph.AdvancePx > 0 ? glyph.AdvancePx : metrics.Advance;
          if (width + advance > contentWidth && cursor > at) { break; }
          if (code == U' ') { lastSpace = cursor; }
          width += advance;
          cursor += step;
        }
        if (cursor < text.size()) {
          if (lastSpace != std::string::npos && lastSpace > at) {
            take = lastSpace - at;
          } else {
            /* A WORD WIDER THAN THE LINE IS PLACED WHOLE AND OVERFLOWS. No hyphenation and no break
             * inside a word is a DECLARED part of this subset, so the line is extended to the word's
             * own end -- stopping where the room ran out would cut it, and taking one character at a
             * time is that same cut spelled once per glyph. */
            const size_t next = text.find(' ', at);
            take = next == std::string::npos ? text.size() - at : next - at;
          }
        }
      }
      Box line;
      line.Node = child;
      line.Text = text.substr(at, take);
      line.FontSize = emPx;
      line.Colour = style.Has(Property::Colour) ? style.Of(Property::Colour).Word : 0x000000FFu;
      line.Width = Width(line.Text, 0, line.Text.size(), emPx);
      line.Height = lineHeight;
      line.Y = y;
      line.X = contentX;
      if (align == kCentre) { line.X = contentX + (contentWidth - line.Width) / 2.0; }
      if (align == kRight) { line.X = contentX + contentWidth - line.Width; }
      line.Parent = self;
      line.Baseline = (lineHeight - (metrics.Ascent + metrics.Descent)) / 2.0 + metrics.Ascent;
      /* THE BOX'S BASELINE IS ITS FIRST LINE'S, and only the first: a paragraph aligns by the line a
       * reader's eye starts on, which is what CSS states and what makes two columns of prose line up. */
      if ((*Out)[(size_t)self].Baseline == 0.0) {
        (*Out)[(size_t)self].Baseline = (y - contentY) + line.Baseline +
                                        ((*Out)[(size_t)self].Border.Top +
                                         (*Out)[(size_t)self].Padding.Top);
      }
      Out->push_back(line);
      (*Out)[(size_t)self].Children.push_back((int)Out->size() - 1);
      y += lineHeight;
      at += take;
      while (at < text.size() && text[at] == ' ') { ++at; }
    }
  }
  return y - contentY;
}

double Placer::Blocks(int node, const Computed &style, int self, double contentX, double contentY,
                      double contentWidth, double emPx) {
  double y = contentY;
  y += Runs(node, style, self, contentX, y, contentWidth, emPx);
  for (const int child : Tree->Nodes()[(size_t)node].Children) {
    if (Tree->Nodes()[(size_t)child].Kind != NodeKind::Element) { continue; }
    y += Place(child, &style, contentX, y, contentWidth, 0, self);
  }
  return y - contentY;
}

double Placer::Flex(int node, const Computed &style, int self, double contentX, double contentY,
                    double contentWidth, double contentHeight, double emPx) {
  const uint32_t direction = style.Word(Property::FlexDirection, 0);
  const bool column = direction == kColumn || direction == kColumnReverse;
  /* A REVERSED DIRECTION IS THE MAIN AXIS MIRRORED, and it is applied ONCE at placement -- everything
   * between here and there is computed in flex coordinates. Reversing the item order instead would
   * work for the sizes and be wrong for `justify-content`, which measures from the flex start. */
  const bool mainReversed = direction == kRowReverse || direction == kColumnReverse;
  const double mainRoom = column ? contentHeight : contentWidth;
  const double crossRoom = column ? contentWidth : contentHeight;
  const double gap = style.Has(Property::Gap) ? style.Of(Property::Gap).Number : 0.0;
  const uint32_t wrapping = style.Word(Property::FlexWrap, 0);
  const bool reversed = wrapping == kWrapReverse;
  /* THE MAIN AXIS IS NOT MIRRORED BY `wrap-reverse` -- only the cross one is -- so `justify-content`
   * takes the unreversed translation and `align-items` takes the reversed one. Handing both the same
   * flag is the kind of symmetry that reads as tidy and is a different layout. */
  const uint32_t justify = Aligned(style.Word(Property::JustifyContent, 0), mainReversed);
  const uint32_t align = Aligned(style.Word(Property::AlignItems, kStretch), reversed);

  struct Item {
    int Node = 0;
    Computed Style;
    double Base = 0, Main = 0, Cross = 0;
    double MainMarginStart = 0, MainMarginEnd = 0, CrossMarginStart = 0, CrossMarginEnd = 0;
    double Grow = 0, Shrink = 1;
    double Em = 0;
    Property Least = Property::MinWidth, Most = Property::MaxWidth;
    bool CrossDeclared = false;
  };
  std::vector<Item> items;
  for (const int child : Tree->Nodes()[(size_t)node].Children) {
    if (Tree->Nodes()[(size_t)child].Kind != NodeKind::Element) { continue; }
    Item item;
    item.Node = child;
    item.Style = StyleOf(child, &style);
    if (item.Style.Word(Property::Display, kDisplayBlock) == kDisplayNone) { continue; }
    const double itemEm = item.Style.Has(Property::FontSize)
                              ? [&] {
                                  bool absent = false;
                                  const double found = Resolve(item.Style.Of(Property::FontSize),
                                                               emPx, emPx, RootEm, absent);
                                  return absent ? emPx : found;
                                }()
                              : emPx;
    const auto len = [&](Property what, double against, double fallback) {
      if (!item.Style.Has(what)) { return fallback; }
      bool absent = false;
      const double found = Resolve(item.Style.Of(what), against, itemEm, RootEm, absent);
      return absent ? fallback : found;
    };
    item.MainMarginStart = column ? len(Property::MarginTop, contentWidth, 0)
                                  : len(Property::MarginLeft, contentWidth, 0);
    item.MainMarginEnd = column ? len(Property::MarginBottom, contentWidth, 0)
                                : len(Property::MarginRight, contentWidth, 0);
    item.CrossMarginStart = column ? len(Property::MarginLeft, contentWidth, 0)
                                   : len(Property::MarginTop, contentWidth, 0);
    item.CrossMarginEnd = column ? len(Property::MarginRight, contentWidth, 0)
                                 : len(Property::MarginBottom, contentWidth, 0);
    item.Grow = item.Style.Number(Property::FlexGrow, 0);
    item.Shrink = item.Style.Number(Property::FlexShrink, 1);

    /* THE BASE IS `flex-basis`, THEN THE MAIN-AXIS SIZE, THEN WHAT THE CONTENTS TAKE -- which is the
     * order the specification states and the only one where an item with neither answers at all. */
    bool haveBase = false;
    const Property mainSize = column ? Property::Height : Property::Width;
    if (item.Style.Has(Property::FlexBasis) && item.Style.Of(Property::FlexBasis).How != Unit::Auto) {
      bool absent = false;
      item.Base = Resolve(item.Style.Of(Property::FlexBasis), mainRoom, itemEm, RootEm, absent);
      haveBase = !absent;
    }
    if (!haveBase && item.Style.Has(mainSize) && item.Style.Of(mainSize).How != Unit::Auto) {
      bool absent = false;
      item.Base = Resolve(item.Style.Of(mainSize), mainRoom, itemEm, RootEm, absent);
      haveBase = !absent;
    }
    /* A DECLARED SIZE IS THE CONTENT'S UNDER CSS'S OWN DEFAULT, and what flex distributes is the
     * BORDER box -- so the frame is added here, once, and every later step speaks one currency. */
    if (haveBase && item.Style.Word(Property::BoxSizing, 0) != kBorderBox) {
      const double frame =
          column ? len(Property::BorderTopWidth, contentWidth, 0) +
                       len(Property::BorderBottomWidth, contentWidth, 0) +
                       len(Property::PaddingTop, contentWidth, 0) +
                       len(Property::PaddingBottom, contentWidth, 0)
                 : len(Property::BorderLeftWidth, contentWidth, 0) +
                       len(Property::BorderRightWidth, contentWidth, 0) +
                       len(Property::PaddingLeft, contentWidth, 0) +
                       len(Property::PaddingRight, contentWidth, 0);
      item.Base += frame;
    }
    if (!haveBase) {
      if (column) {
        /* A COLUMN'S MAIN AXIS IS HEIGHT, and a height is only knowable once a width is -- so this one
         * IS measured in the room it will get, which is the container's own content width. */
        double w = 0, h = 0;
        Measure(child, &style, contentWidth, w, h);
        item.Base = h;
      } else {
        item.Base = MaxContent(child, &style) - item.MainMarginStart - item.MainMarginEnd;
      }
    }
    const Property crossSize = column ? Property::Width : Property::Height;
    item.CrossDeclared = item.Style.Has(crossSize) && item.Style.Of(crossSize).How != Unit::Auto;
    if (item.CrossDeclared) {
      bool absent = false;
      item.Cross = Resolve(item.Style.Of(crossSize), crossRoom, itemEm, RootEm, absent);
      item.CrossDeclared = !absent;
    }
    /* THE ITEM'S OWN LIMITS ARE KEPT AND APPLIED AFTER FLEXING, not before: growing then clamping is
     * what CSS's resolution does, and clamping the base first would hand the line a free space that
     * was never free. */
    item.Least = column ? Property::MinHeight : Property::MinWidth;
    item.Most = column ? Property::MaxHeight : Property::MaxWidth;
    item.Em = itemEm;
    item.Main = item.Base;
    items.push_back(std::move(item));
  }
  if (items.empty()) { return 0; }

  /* THE ITEMS ARE BROKEN INTO LINES BEFORE ANYTHING IS SIZED, because every later step is per line:
   * a line resolves its OWN free space, a line has its OWN cross size, and `align-items` stretches an
   * item to the line rather than to the container. Laying a wrapped container out as one line is the
   * defect that makes `align-content` unspellable -- there is nothing to distribute. */
  const bool wraps = wrapping == kWrap || wrapping == kWrapReverse;
  struct Line {
    size_t From = 0, Count = 0;
    double Cross = 0, CrossAt = 0;
  };
  std::vector<Line> lines;
  {
    Line line;
    line.From = 0;
    double taken = 0;
    for (size_t i = 0; i < items.size(); ++i) {
      const double outer = items[i].Base + items[i].MainMarginStart + items[i].MainMarginEnd;
      const double withGap = line.Count == 0 ? outer : taken + gap + outer;
      if (wraps && line.Count > 0 && mainRoom > 0 && withGap > mainRoom) {
        lines.push_back(line);
        line = Line{i, 0, 0, 0};
        taken = outer;
      } else {
        taken = withGap;
      }
      ++line.Count;
    }
    lines.push_back(line);
  }

  /* EACH LINE RESOLVES ITS OWN FLEXIBLE LENGTHS, and its own cross size falls out of what it holds. */
  for (Line &line : lines) {
    double taken = gap * (double)(line.Count - 1);
    double grow = 0, shrinkWeight = 0;
    for (size_t i = line.From; i < line.From + line.Count; ++i) {
      taken += items[i].Base + items[i].MainMarginStart + items[i].MainMarginEnd;
      grow += items[i].Grow;
      shrinkWeight += items[i].Shrink * items[i].Base;
    }
    const double free = mainRoom - taken;
    for (size_t i = line.From; i < line.From + line.Count; ++i) {
      Item &one = items[i];
      one.Main = one.Base;
      if (free > 0 && grow > 0) {
        one.Main = one.Base + free * (one.Grow / grow);
      } else if (free < 0 && shrinkWeight > 0) {
        one.Main = std::fmax(0.0, one.Base + free * (one.Shrink * one.Base / shrinkWeight));
      }
      one.Main = Clamped(one.Main, one.Style, one.Least, one.Most, mainRoom, one.Em);
    }
    for (size_t i = line.From; i < line.From + line.Count; ++i) {
      Item &one = items[i];
      if (!one.CrossDeclared) {
        if (column) {
          /* A COLUMN'S CROSS AXIS IS WIDTH, AND AN AUTO WIDTH ASKS WHAT THE CONTENT WANTS. Laying the
           * item out in the room it might get answers *the whole room*, because a block with no width
           * fills its container -- so the line would be as wide as the container and one line would
           * eat every other. [MEASURED] `align-content-vert-001b` missed 104 of its assertions on
           * exactly this, reporting 200 where the document states 110. It is the same defect the MAIN
           * axis had, one axis over. */
          one.Cross = MaxContent(one.Node, &style) - one.CrossMarginStart - one.CrossMarginEnd;
        } else {
          /* A ROW'S CROSS AXIS IS HEIGHT, and a height is only knowable once a width is -- so this one
           * IS laid out, in the width flex just gave it. */
          double w = 0, h = 0;
          Measure(one.Node, &style, one.Main, w, h);
          one.Cross = h;
        }
      }
      line.Cross = std::fmax(line.Cross, one.Cross + one.CrossMarginStart + one.CrossMarginEnd);
    }
  }

  /* A SINGLE-LINE CONTAINER'S ONE LINE IS THE WHOLE CROSS ROOM, which is what makes `align-items:
   * stretch` reach the container's edge and is CSS's own rule rather than a shortcut. */
  double linesDeep = 0;
  for (const Line &line : lines) { linesDeep += line.Cross; }
  linesDeep += gap * (double)(lines.size() - 1);
  if (!wraps && crossRoom > 0) {
    lines[0].Cross = crossRoom;
    linesDeep = crossRoom;
  }

  /* `align-content` DISTRIBUTES THE LINES, AND ITS INITIAL VALUE IS `stretch` -- which is why a
   * wrapped container with no declaration at all fills its cross axis rather than hugging its lines. */
  const uint32_t alignLines = Aligned(style.Word(Property::AlignContent, kStretch), reversed);
  double lineAt = 0, betweenLines = gap;
  const double crossSlack = crossRoom - linesDeep;
  if (crossRoom > 0 && crossSlack > 0 && lines.size() > 0) {
    if (alignLines == kStretch) {
      const double share = crossSlack / (double)lines.size();
      for (Line &line : lines) { line.Cross += share; }
    } else if (alignLines == kFlexEnd) {
      lineAt = crossSlack;
    } else if (alignLines == kCentre) {
      lineAt = crossSlack / 2.0;
    } else if (alignLines == kSpaceBetween && lines.size() > 1) {
      betweenLines = gap + crossSlack / (double)(lines.size() - 1);
    } else if (alignLines == kSpaceAround) {
      lineAt = crossSlack / (double)(lines.size() * 2);
      betweenLines = gap + crossSlack / (double)lines.size();
    } else if (alignLines == kSpaceEvenly) {
      /* EVERY GAP THE SAME, INCLUDING THE TWO AT THE ENDS -- which is what separates it from
       * `space-around`, where the end gaps are half. `n` lines make `n + 1` gaps. */
      lineAt = crossSlack / (double)(lines.size() + 1);
      betweenLines = gap + lineAt;
    }
  }
  for (Line &line : lines) {
    line.CrossAt = lineAt;
    lineAt += line.Cross + betweenLines;
  }
  if (wrapping == kWrapReverse && crossRoom > 0) {
    for (Line &line : lines) { line.CrossAt = crossRoom - line.CrossAt - line.Cross; }
  }

  /* THE BASELINE EACH LINE ALIGNS TO, and it is the deepest one any of its baseline items reached.
   * Every such item is then pushed down by the difference, which is what makes their first lines of
   * text share one row -- and it is measured in a first pass because an item cannot know how far to
   * move until the deepest one is known. */
  std::vector<double> lineBaseline(lines.size(), 0.0);
  for (size_t at = 0; at < lines.size(); ++at) {
    for (size_t i = lines[at].From; i < lines[at].From + lines[at].Count; ++i) {
      const Item &one = items[i];
      const uint32_t how = one.Style.Has(Property::AlignSelf)
                               ? Aligned(one.Style.Word(Property::AlignSelf, align), reversed)
                               : align;
      if (how != kBaseline || column) { continue; }
      /* A COLUMN'S CROSS AXIS IS HORIZONTAL AND A BASELINE IS NOT, so `baseline` in a column falls
       * back to `flex-start`, which is what CSS says for a case where the two are perpendicular. */
      lineBaseline[at] = std::fmax(lineBaseline[at], BaselineOf(one.Node, &style, one.Main) +
                                                        one.CrossMarginStart);
    }
  }

  double deepest = 0;
  for (size_t lineAt = 0; lineAt < lines.size(); ++lineAt) {
    const Line &line = lines[lineAt];
    double used = gap * (double)(line.Count - 1);
    for (size_t i = line.From; i < line.From + line.Count; ++i) {
      used += items[i].Main + items[i].MainMarginStart + items[i].MainMarginEnd;
    }
    double cursor = 0, between = gap;
    const double slack = mainRoom - used;
    if (slack > 0) {
      if (justify == kFlexEnd) {
        cursor = slack;
      } else if (justify == kCentre) {
        cursor = slack / 2.0;
      } else if (justify == kSpaceBetween && line.Count > 1) {
        between = gap + slack / (double)(line.Count - 1);
      } else if (justify == kSpaceAround) {
        cursor = slack / (double)(line.Count * 2);
        between = gap + slack / (double)line.Count;
      } else if (justify == kSpaceEvenly) {
        cursor = slack / (double)(line.Count + 1);
        between = gap + cursor;
      }
    } else if (justify == kCentre) {
      /* CENTRING AN OVERFULL LINE PUTS ITS START BEFORE THE CONTAINER, and that is the answer CSS
       * gives rather than a clamp: the overflow is symmetric and the corpus states a NEGATIVE offset
       * for it. Flooring the slack at zero here reads as tidy and is a different layout. */
      cursor = slack / 2.0;
    }

    for (size_t i = line.From; i < line.From + line.Count; ++i) {
      Item &one = items[i];
      cursor += one.MainMarginStart;
      const uint32_t self_align = one.Style.Has(Property::AlignSelf)
                                      ? Aligned(one.Style.Word(Property::AlignSelf, align), reversed)
                                      : align;
      double cross = one.Cross;
      if (!one.CrossDeclared && self_align == kStretch) {
        cross = line.Cross - one.CrossMarginStart - one.CrossMarginEnd;
      }
      double inLine = one.CrossMarginStart;
      if (self_align == kCentre) {
        inLine = (line.Cross - cross - one.CrossMarginStart - one.CrossMarginEnd) / 2.0 +
                 one.CrossMarginStart;
      } else if (self_align == kFlexEnd) {
        inLine = line.Cross - cross - one.CrossMarginEnd;
      }
      /* `wrap-reverse` TURNS THE CROSS AXIS ROUND FOR THE ITEM TOO, and not only for the lines. The
       * cross-start becomes the far edge, so `align-items: flex-start` puts an item AGAINST it --
       * [MEASURED] `align-content-vert-002` states x = 198 for a 10 px item in a 200 px container, and
       * reversing only the line order answered 8. Reversing one of the two is a layout that is
       * mirrored in the large and not in the small. */
      if (self_align == kBaseline && !column) {
        /* PUSHED DOWN BY THE DIFFERENCE between this item's own baseline and the line's, which is
         * zero for whichever item is deepest and positive for every other. */
        inLine = one.CrossMarginStart +
                 (lineBaseline[lineAt] - BaselineOf(one.Node, &style, one.Main) -
                  one.CrossMarginStart);
        inLine = std::fmax(inLine, 0.0);
      }
      if (wrapping == kWrapReverse) { inLine = line.Cross - inLine - cross; }
      const double crossAt = line.CrossAt + inLine;
      /* THE MAIN POSITION IS MIRRORED HERE AND NOWHERE ELSE. `cursor` is where the item sits measured
       * from the flex start; a reversed direction measures the same distance from the other end. */
      const double mainAt = mainReversed ? mainRoom - cursor - one.Main : cursor;
      const double x = column ? contentX + crossAt : contentX + mainAt;
      const double y = column ? contentY + mainAt : contentY + crossAt;
      /* THE ITEM IS GIVEN THE ROOM FLEX DECIDED, AND THE ROOM IS ITS OWN AXIS'S. A row's horizontal
       * margins are the MAIN ones and a column's are the CROSS ones, so the two rooms are named by
       * axis rather than by side -- writing them by side is how a column ends up sized by a row's
       * arithmetic. */
      const double widthRoom = column ? cross + one.CrossMarginStart + one.CrossMarginEnd
                                      : one.Main + one.MainMarginStart + one.MainMarginEnd;
      const double heightRoom = column ? one.Main : cross;
      const int before = (int)Out->size();
      Place(one.Node, &style, x - (column ? one.CrossMarginStart : one.MainMarginStart),
            y - (column ? one.MainMarginStart : one.CrossMarginStart), widthRoom, heightRoom, self);
      if (before < (int)Out->size()) {
        Box &placed = (*Out)[(size_t)before];
        if (column) {
          placed.Height = one.Main;
          if (!one.CrossDeclared && self_align == kStretch) { placed.Width = cross; }
        } else {
          placed.Width = one.Main;
          if (!one.CrossDeclared && self_align == kStretch) { placed.Height = cross; }
        }
        deepest = std::fmax(deepest, column ? mainAt + one.Main + one.MainMarginEnd
                                            : crossAt + placed.Height + one.CrossMarginEnd);
      }
      cursor += one.Main + one.MainMarginEnd + between;
    }
  }
  /* WHAT THE CONTAINER USED ON ITS BLOCK AXIS. A column's is the deepest MAIN extent any line
   * reached; a row's is where its last LINE ends, which is not the same number the moment a container
   * wraps. */
  double crossExtent = 0;
  for (const Line &line : lines) { crossExtent = std::fmax(crossExtent, line.CrossAt + line.Cross); }
  return column ? deepest : std::fmax(deepest, crossExtent);
}

double Placer::Children(int node, const Computed &style, int self, double contentX, double contentY,
                        double contentWidth, double contentHeight, double emPx) {
  const uint32_t display = style.Word(Property::Display, kDisplayInline);
  return display == kDisplayFlex || display == kDisplayInlineFlex
             ? Flex(node, style, self, contentX, contentY, contentWidth, contentHeight, emPx)
             : Blocks(node, style, self, contentX, contentY, contentWidth, emPx);
}

double Placer::Place(int node, const Computed *inherited, double originX, double originY,
                     double containerWidth, double containerHeight, int parentBox) {
  const Node &element = Tree->Nodes()[(size_t)node];
  if (element.Kind == NodeKind::Text) { return 0; }

  const Computed style = StyleOf(node, inherited);
  const uint32_t display = style.Word(Property::Display, kDisplayInline);
  if (display == kDisplayNone) { return 0; }

  double emPx = 16.0;
  if (style.Has(Property::FontSize)) {
    bool absent = false;
    const double found =
        Resolve(style.Of(Property::FontSize), inherited != nullptr ? 16.0 : RootEm, 16.0, RootEm, absent);
    if (!absent) { emPx = found; }
  }
  const auto len = [&](Property what, double against, double fallback) {
    if (!style.Has(what)) { return fallback; }
    bool absent = false;
    const double found = Resolve(style.Of(what), against, emPx, RootEm, absent);
    return absent ? fallback : found;
  };

  Box box;
  box.Node = node;
  box.Margin = {len(Property::MarginTop, containerWidth, 0),
                len(Property::MarginRight, containerWidth, 0),
                len(Property::MarginBottom, containerWidth, 0),
                len(Property::MarginLeft, containerWidth, 0)};
  box.Border = {len(Property::BorderTopWidth, containerWidth, 0),
                len(Property::BorderRightWidth, containerWidth, 0),
                len(Property::BorderBottomWidth, containerWidth, 0),
                len(Property::BorderLeftWidth, containerWidth, 0)};
  box.Padding = {len(Property::PaddingTop, containerWidth, 0),
                 len(Property::PaddingRight, containerWidth, 0),
                 len(Property::PaddingBottom, containerWidth, 0),
                 len(Property::PaddingLeft, containerWidth, 0)};
  box.Background = style.Has(Property::BackgroundColour) ? style.Of(Property::BackgroundColour).Word : 0;
  box.BorderColour = style.Has(Property::BorderColour) ? style.Of(Property::BorderColour).Word : 0;
  box.Radius = len(Property::BorderRadius, containerWidth, 0);
  box.Opacity = style.Has(Property::Opacity) ? style.Of(Property::Opacity).Number : 1.0;
  box.Clips = style.Word(Property::Overflow, 0) == kHidden;
  box.Colour = style.Has(Property::Colour) ? style.Of(Property::Colour).Word : 0x000000FFu;
  box.FontSize = emPx;
  box.Parent = parentBox;

  const bool borderBox = style.Word(Property::BoxSizing, 0) == kBorderBox;
  const double frameX = box.Border.Left + box.Border.Right + box.Padding.Left + box.Padding.Right;
  const double frameY = box.Border.Top + box.Border.Bottom + box.Padding.Top + box.Padding.Bottom;

  bool widthAbsent = true;
  double contentWidth = 0;
  if (style.Has(Property::Width) && style.Of(Property::Width).How != Unit::Auto) {
    contentWidth = Resolve(style.Of(Property::Width), containerWidth, emPx, RootEm, widthAbsent);
    if (!widthAbsent && borderBox) { contentWidth = std::fmax(0.0, contentWidth - frameX); }
  }
  if (widthAbsent) {
    contentWidth =
        std::fmax(0.0, containerWidth - box.Margin.Left - box.Margin.Right - frameX);
  }
  contentWidth = Clamped(contentWidth + (borderBox ? frameX : 0.0), style, Property::MinWidth,
                         Property::MaxWidth, containerWidth, emPx) -
                 (borderBox ? frameX : 0.0);

  bool heightAbsent = true;
  double contentHeight = 0;
  if (style.Has(Property::Height) && style.Of(Property::Height).How != Unit::Auto) {
    contentHeight = Resolve(style.Of(Property::Height), containerHeight, emPx, RootEm, heightAbsent);
    if (!heightAbsent && borderBox) { contentHeight = std::fmax(0.0, contentHeight - frameY); }
  }

  box.X = originX + box.Margin.Left;
  box.Y = originY + box.Margin.Top;
  const int self = (int)Out->size();
  Out->push_back(box);
  if (parentBox >= 0) { (*Out)[(size_t)parentBox].Children.push_back(self); }

  const double contentX = box.X + box.Border.Left + box.Padding.Left;
  const double contentY = box.Y + box.Border.Top + box.Padding.Top;
  /* AN INDEFINITE MAIN SIZE WITH A DEFINITE MAXIMUM IS THE MAXIMUM, for the purpose of the room the
   * contents get. CSS says the available space in an axis is the box's definite max size when its own
   * size is not definite -- and for a wrapping flex container that room is what DECIDES the wrapping.
   * [MEASURED] `align-content-vert-001b` writes `max-height: 10px` where `001a` writes `height: 10px`,
   * and handing zero room to the second made the container one line where the document states three:
   * 104 of its assertions, on a declaration one word apart from a case that already held. */
  double heightRoom = heightAbsent ? 0.0 : contentHeight;
  if (heightAbsent && style.Has(Property::MaxHeight) &&
      style.Of(Property::MaxHeight).How != Unit::Auto) {
    bool absent = false;
    const double ceiling = Resolve(style.Of(Property::MaxHeight), containerHeight, emPx, RootEm, absent);
    if (!absent) { heightRoom = std::fmax(0.0, ceiling - (borderBox ? frameY : 0.0)); }
  }
  const double used = Children(node, style, self, contentX, contentY, contentWidth, heightRoom, emPx);
  if (heightAbsent) { contentHeight = used; }
  /* THE LIMITS ARE APPLIED TO THE BOX THE DECLARATION MEANS. `min-height` speaks of the same box
   * `height` does, so under `border-box` both are the border box and under the default both are the
   * content box -- reading one in each currency is how a padded box comes out a frame too tall. */
  contentHeight = Clamped(contentHeight + (borderBox ? frameY : 0.0), style, Property::MinHeight,
                          Property::MaxHeight, containerHeight, emPx) -
                  (borderBox ? frameY : 0.0);

  (*Out)[(size_t)self].Width = contentWidth + frameX;
  (*Out)[(size_t)self].Height = contentHeight + frameY;
  /* A BOX WITH NO TEXT OF ITS OWN TAKES ITS FIRST CHILD'S BASELINE, and one with no child at all takes
   * its own bottom margin edge -- the synthesised baseline CSS names for a box that has nothing to
   * align by. Without the synthesis a box would align by zero, which is its TOP, and two items would
   * line up by the one edge baseline alignment is not about. */
  if ((*Out)[(size_t)self].Baseline == 0.0) {
    for (const int child : (*Out)[(size_t)self].Children) {
      const Box &inner = (*Out)[(size_t)child];
      if (inner.Baseline > 0.0) {
        (*Out)[(size_t)self].Baseline = (inner.Y - (*Out)[(size_t)self].Y) + inner.Baseline;
        break;
      }
    }
  }
  if ((*Out)[(size_t)self].Baseline == 0.0) {
    (*Out)[(size_t)self].Baseline = (*Out)[(size_t)self].Height + box.Margin.Bottom;
  }
  return (*Out)[(size_t)self].Height + box.Margin.Top + box.Margin.Bottom;
}

}  // namespace

/* THE ELEMENTS THIS ENGINE LAYS OUT (board:1445, board:1442), and the ones it deliberately draws nothing for. Two lists rather
 * than one because they fail differently: a flow element missing from the first would be laid out
 * wrongly, a metadata element missing from the second would be laid out AT ALL. */
namespace {

constexpr std::string_view kFlowElements[] = {
    "html", "body",   "div",   "span",       "p",    "section", "article", "header", "footer",
    "nav",  "main",   "aside", "figure",     "figcaption", "h1", "h2",     "h3",     "h4",
    "h5",   "h6",     "ul",    "ol",         "li",   "dl",      "dt",      "dd",     "blockquote",
    "pre",  "code",   "em",    "strong",     "b",    "i",       "u",       "s",      "small",
    "sub",  "sup",    "br",    "hr",         "a",    "abbr",    "cite",    "q",      "mark",
    "time", "kbd",    "samp",  "var",        "wbr",  "del",     "ins",     "bdi",    "bdo",
};

/* NO BOX AT ALL, AND THAT IS THE CORRECT ANSWER RATHER THAN A GAP. A document that could not name its
 * own title or link its own sheet would put every case outside the subset for a reason that has
 * nothing to do with layout. */
constexpr std::string_view kNoBoxElements[] = {"head",  "title", "link",  "meta",  "style",
                                               "script", "base", "noscript"};

} // namespace

bool ElementIsInTheSubset(std::string_view tag) {
  for (const std::string_view known : kFlowElements) {
    if (known == tag) { return true; }
  }
  for (const std::string_view known : kNoBoxElements) {
    if (known == tag) { return true; }
  }
  return false;
}

std::vector<std::string> ElementsOutsideTheSubset(const Markup &markup) {
  std::vector<std::string> outside;
  for (int index = 0; index < (int)markup.Nodes().size(); ++index) {
    const Node &node = markup.Nodes()[size_t(index)];
    /* The root is the document itself and carries no tag a declaration could have written. */
    if (index == markup.Root()) { continue; }
    if (node.Kind != NodeKind::Element || ElementIsInTheSubset(node.Name)) { continue; }
    bool already = false;
    for (const std::string &seen : outside) { already = already || seen == node.Name; }
    if (!already) { outside.push_back(node.Name); }
  }
  return outside;
}

const char *UserAgentSheet(void) {
  /* WHAT A BROWSER BRINGS BEFORE ANY AUTHOR SPEAKS, cut to what this subset can mean. The margins are
   * what the corpus's own numbers are stated against -- `data-offset-x="8"` IS the line below. */
  return "html, body, div, p, h1, h2, h3, h4, h5, h6, section, article, header, footer, nav, main,"
         " ul, ol, li, blockquote, figure, form, fieldset, pre { display: block }\n"
         "span, a, b, i, em, strong, small, code, label { display: inline }\n"
         "body { margin: 8px }\n"
         "p, blockquote, figure, h1, h2, h3, h4, h5, h6, ul, ol, pre, form { margin: 1em 0 }\n"
         "html { color: black; font-size: 16px; line-height: 1.2; text-align: left }\n"
         /* THE DOCUMENT'S METADATA DRAWS NOTHING, AND SAYING SO IS THE SHEET'S JOB RATHER THAN THE
          * LAYOUT'S. [MEASURED] `<title>` was laid out as ordinary text: two lines of 19.2 px that
          * pushed `<body>` from y = 8 to y = 46.4, and three cases of the corpus read as an
          * `align-content` defect when the cause was a title nobody was supposed to see. */
         "head, title, link, meta, style, script, base, noscript { display: none }\n";
}

bool Layout::Build(const Markup &markup, Stylesheet &sheet, double viewportWidth,
                   double viewportHeight, const Font &font, std::string &error) {
  ViewportWidth_ = viewportWidth;
  ViewportHeight_ = viewportHeight;
  Boxes_.clear();
  if (markup.Root() < 0) {
    error = "the declaration has no root, so there is nothing to place";
    return false;
  }
  Stylesheet agent;
  agent.Read(UserAgentSheet());

  Placer placer;
  placer.Tree = &markup;
  placer.Agent = &agent;
  placer.Author = &sheet;
  placer.Face = &font;
  placer.Out = &Boxes_;

  /* THE DOCUMENT'S OWN ROOT IS THIS READER'S AND CARRIES NO BOX, so its element children are placed
   * straight against the viewport. */
  double y = 0;
  for (const int child : markup.Nodes()[(size_t)markup.Root()].Children) {
    if (markup.Nodes()[(size_t)child].Kind != NodeKind::Element) { continue; }
    y += placer.Place(child, nullptr, 0, y, viewportWidth, viewportHeight, -1);
  }
  return true;
}

int Layout::Hit(double x, double y) const {
  /* DEEPEST FIRST, which is what a pointer means: the box drawn last over that point is the one the
   * client asked about. The library answers WHICH and never what it means. */
  for (size_t at = Boxes_.size(); at-- > 0;) {
    const Box &box = Boxes_[at];
    if (!(x >= box.X && x < box.X + box.Width && y >= box.Y && y < box.Y + box.Height)) { continue; }
    /* EVERY CLIPPING ANCESTOR MUST ALSO CONTAIN THE POINT. The walk is up the parent chain rather than
     * a rectangle carried on the box, because a clip is a property of an ANCESTOR and copying it down
     * would be a second fact that drifts the moment a layout is rebuilt. */
    bool seen = true;
    for (int up = box.Parent; up >= 0 && seen; up = Boxes_[(size_t)up].Parent) {
      const Box &over = Boxes_[(size_t)up];
      if (!over.Clips) { continue; }
      const double left = over.X + over.Border.Left, top = over.Y + over.Border.Top;
      const double right = over.X + over.Width - over.Border.Right;
      const double bottom = over.Y + over.Height - over.Border.Bottom;
      seen = x >= left && x < right && y >= top && y < bottom;
    }
    if (seen) { return box.Node; }
  }
  return -1;
}

}  // namespace outshine::Ui
