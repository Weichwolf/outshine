#include "Layout.h"

#include <algorithm>
#include <cmath>

namespace outshine::Ui {

namespace {

constexpr uint32_t kDisplayBlock = Keyword("block");
constexpr uint32_t kDisplayFlex = Keyword("flex");
constexpr uint32_t kDisplayNone = Keyword("none");
constexpr uint32_t kDisplayInline = Keyword("inline");
constexpr uint32_t kColumn = Keyword("column");
constexpr uint32_t kFlexEnd = Keyword("flex-end");
constexpr uint32_t kCentre = Keyword("center");
constexpr uint32_t kSpaceBetween = Keyword("space-between");
constexpr uint32_t kSpaceAround = Keyword("space-around");
constexpr uint32_t kStretch = Keyword("stretch");
constexpr uint32_t kBorderBox = Keyword("border-box");
constexpr uint32_t kHidden = Keyword("hidden");
constexpr uint32_t kPre = Keyword("pre");
constexpr uint32_t kRight = Keyword("right");

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
  const Stylesheet *Author = nullptr;
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

  const FontMetrics metrics = Face->At(emPx);
  double own = 0;
  const bool row = display == kDisplayFlex && style.Word(Property::FlexDirection, 0) != kColumn;
  double along = 0;
  int items = 0;
  for (const int child : element.Children) {
    const Node &node2 = Tree->Nodes()[(size_t)child];
    if (node2.Kind == NodeKind::Text) {
      own = std::fmax(own, metrics.Advance * (double)Collapsed(node2.Text).size());
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
    if (text.empty()) { continue; }
    /* WRAPPED AT SPACES AND NOWHERE ELSE, which is what this subset declares: no hyphenation, no
     * breaking inside a word, and a word wider than the line is placed and overflows rather than
     * being cut in half. */
    size_t at = 0;
    while (at < text.size()) {
      size_t take = text.size() - at;
      if (contentWidth > 0) {
        const size_t fits = metrics.Advance > 0 ? (size_t)(contentWidth / metrics.Advance) : take;
        if (fits < take) {
          size_t space = text.rfind(' ', at + fits);
          take = space != std::string::npos && space > at ? space - at : std::max<size_t>(fits, 1);
        }
      }
      Box line;
      line.Node = child;
      line.Text = text.substr(at, take);
      line.FontSize = emPx;
      line.Colour = style.Has(Property::Colour) ? style.Of(Property::Colour).Word : 0x000000FFu;
      line.Width = (double)line.Text.size() * metrics.Advance;
      line.Height = lineHeight;
      line.Y = y;
      line.X = contentX;
      if (align == kCentre) { line.X = contentX + (contentWidth - line.Width) / 2.0; }
      if (align == kRight) { line.X = contentX + contentWidth - line.Width; }
      line.Parent = self;
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
  const bool column = style.Word(Property::FlexDirection, 0) == kColumn;
  const double mainRoom = column ? contentHeight : contentWidth;
  const double crossRoom = column ? contentWidth : contentHeight;
  const double gap = style.Has(Property::Gap) ? style.Of(Property::Gap).Number : 0.0;
  const uint32_t justify = style.Word(Property::JustifyContent, 0);
  const uint32_t align = style.Word(Property::AlignItems, kStretch);

  struct Item {
    int Node = 0;
    Computed Style;
    double Base = 0, Main = 0, Cross = 0;
    double MainMarginStart = 0, MainMarginEnd = 0, CrossMarginStart = 0, CrossMarginEnd = 0;
    double Grow = 0, Shrink = 1;
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
    item.Main = item.Base;
    items.push_back(std::move(item));
  }
  if (items.empty()) { return 0; }

  double taken = gap * (double)(items.size() - 1);
  double grow = 0, shrinkWeight = 0;
  for (const Item &one : items) {
    taken += one.Base + one.MainMarginStart + one.MainMarginEnd;
    grow += one.Grow;
    shrinkWeight += one.Shrink * one.Base;
  }
  const double free = mainRoom - taken;
  if (free > 0 && grow > 0) {
    for (Item &one : items) { one.Main = one.Base + free * (one.Grow / grow); }
  } else if (free < 0 && shrinkWeight > 0) {
    for (Item &one : items) {
      one.Main = std::fmax(0.0, one.Base + free * (one.Shrink * one.Base / shrinkWeight));
    }
  }

  double used = 0;
  for (const Item &one : items) { used += one.Main + one.MainMarginStart + one.MainMarginEnd; }
  used += gap * (double)(items.size() - 1);
  double cursor = 0, between = gap;
  const double slack = mainRoom - used;
  if (slack > 0) {
    if (justify == kFlexEnd) {
      cursor = slack;
    } else if (justify == kCentre) {
      cursor = slack / 2.0;
    } else if (justify == kSpaceBetween && items.size() > 1) {
      between = gap + slack / (double)(items.size() - 1);
    } else if (justify == kSpaceAround) {
      cursor = slack / (double)(items.size() * 2);
      between = gap + slack / (double)items.size();
    }
  }

  double deepest = 0;
  for (Item &one : items) {
    cursor += one.MainMarginStart;
    double cross = one.Cross;
    if (!one.CrossDeclared) {
      if (align == kStretch && crossRoom > 0) {
        cross = crossRoom - one.CrossMarginStart - one.CrossMarginEnd;
      } else {
        double w = 0, h = 0;
        Measure(one.Node, &style, column ? crossRoom : one.Main, w, h);
        cross = column ? w : h;
      }
    }
    double crossAt = one.CrossMarginStart;
    if (align == kCentre && crossRoom > 0) {
      crossAt = (crossRoom - cross - one.CrossMarginStart - one.CrossMarginEnd) / 2.0 +
                one.CrossMarginStart;
    } else if (align == kFlexEnd && crossRoom > 0) {
      crossAt = crossRoom - cross - one.CrossMarginEnd;
    }
    const double x = column ? contentX + crossAt : contentX + cursor;
    const double y = column ? contentY + cursor : contentY + crossAt;
    /* THE ITEM IS PLACED AT THE SIZE FLEX GAVE IT, which is why it is laid out against that size
     * rather than against the container's: a flexed item's children see the item, not the flexbox. */
    /* THE ITEM IS GIVEN THE ROOM FLEX DECIDED, AND THE ROOM IS ITS OWN AXIS'S. A row's horizontal
     * margins are the MAIN ones and a column's are the CROSS ones, so the two rooms are named by axis
     * rather than by side -- writing them by side is how a column ends up sized by a row's arithmetic. */
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
        if (!one.CrossDeclared || align == kStretch) { placed.Width = cross; }
      } else {
        placed.Width = one.Main;
        if (!one.CrossDeclared && align == kStretch) { placed.Height = cross; }
      }
      deepest = std::fmax(deepest, (column ? cursor + one.Main + one.MainMarginEnd
                                           : crossAt + placed.Height + one.CrossMarginEnd));
    }
    cursor += one.Main + one.MainMarginEnd + between;
  }
  return column ? std::fmax(deepest, used) : deepest;
}

double Placer::Children(int node, const Computed &style, int self, double contentX, double contentY,
                        double contentWidth, double contentHeight, double emPx) {
  return style.Word(Property::Display, kDisplayInline) == kDisplayFlex
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
  const double used = Children(node, style, self, contentX, contentY, contentWidth,
                               heightAbsent ? 0 : contentHeight, emPx);
  if (heightAbsent) { contentHeight = used; }

  (*Out)[(size_t)self].Width = contentWidth + frameX;
  (*Out)[(size_t)self].Height = contentHeight + frameY;
  return (*Out)[(size_t)self].Height + box.Margin.Top + box.Margin.Bottom;
}

}  // namespace

const char *UserAgentSheet(void) {
  /* WHAT A BROWSER BRINGS BEFORE ANY AUTHOR SPEAKS, cut to what this subset can mean. The margins are
   * what the corpus's own numbers are stated against -- `data-offset-x="8"` IS the line below. */
  return "html, body, div, p, h1, h2, h3, h4, h5, h6, section, article, header, footer, nav, main,"
         " ul, ol, li, blockquote, figure, form, fieldset, pre { display: block }\n"
         "span, a, b, i, em, strong, small, code, label { display: inline }\n"
         "body { margin: 8px }\n"
         "p, blockquote, figure, h1, h2, h3, h4, h5, h6, ul, ol, pre, form { margin: 1em 0 }\n"
         "html { color: black; font-size: 16px; line-height: 1.2; text-align: left }\n";
}

bool Layout::Build(const Markup &markup, const Stylesheet &sheet, double viewportWidth,
                   double viewportHeight, const Font &font, std::string &error) {
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
    if (x >= box.X && x < box.X + box.Width && y >= box.Y && y < box.Y + box.Height) {
      return box.Node;
    }
  }
  return -1;
}

}  // namespace outshine::Ui
