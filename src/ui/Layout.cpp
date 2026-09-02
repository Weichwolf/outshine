#include "Utf8.h"
#include "Layout.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <span>
#include <unordered_map>
#include <cstring>
#include <utility>
#include <vector>

namespace outshine::Ui {

constexpr uint32_t kOpaqueAlpha = 0x000000FFu;

namespace {

constexpr double kEmPx = 16.0;
constexpr int kFarBeforeAny = -1000000;

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
constexpr uint32_t kScroll = Keyword("scroll");
constexpr uint32_t kStatic = Keyword("static");
constexpr uint32_t kPre = Keyword("pre");
constexpr uint32_t kRight = Keyword("right");
constexpr uint32_t kSpaceEvenly = Keyword("space-evenly");

constexpr uint32_t kStart = Keyword("start");
constexpr uint32_t kEnd = Keyword("end");
constexpr uint32_t kWrap = Keyword("wrap");
constexpr uint32_t kWrapReverse = Keyword("wrap-reverse");

[[nodiscard]] constexpr uint32_t Aligned(uint32_t word, bool reversed) {
  if (word == kStart) { return reversed ? kFlexEnd : 0u; }
  if (word == kEnd) { return reversed ? 0u : kFlexEnd; }
  return word;
}

struct Computed {
  std::array<Value, static_cast<size_t>(Property::kCount)> Held;
  std::array<bool, static_cast<size_t>(Property::kCount)> Set = {};

  void Take(const Declaration &one) {
    Held[static_cast<size_t>(one.What)] = one.How;
    Set[static_cast<size_t>(one.What)] = true;
  }

  [[nodiscard]] bool Has(Property what) const { return Set[static_cast<size_t>(what)]; }

  [[nodiscard]] Value Of(Property what) const { return Held[static_cast<size_t>(what)]; }

  [[nodiscard]] uint32_t Word(Property what, uint32_t fallback) const {
    return Set[static_cast<size_t>(what)] && Held[static_cast<size_t>(what)].How == Unit::Keyword
               ? Held[static_cast<size_t>(what)].Word
               : fallback;
  }

  [[nodiscard]] double Number(Property what, double fallback) const {
    return Set[static_cast<size_t>(what)] && (Held[static_cast<size_t>(what)].How == Unit::None ||
                                              Held[static_cast<size_t>(what)].How == Unit::Pixels)
               ? Held[static_cast<size_t>(what)].Number
               : fallback;
  }
};

struct LengthContext {
  double AgainstPx = 0;
  double EmPx = kEmPx;
  double RootEmPx = kEmPx;
};

struct Limits {
  Property Least = Property::kCount;
  Property Most = Property::kCount;
};

struct Sizing {
  int Node = -1;
  double AvailableWidth = 0;
};

[[nodiscard]] std::optional<double> Resolve(const Value &value, LengthContext in) {
  switch (value.How) {
    case Unit::Pixels: return value.Number;
    case Unit::Percent: return in.AgainstPx * value.Number / 100.0;
    case Unit::Em: return in.EmPx * value.Number;
    case Unit::Rem: return in.RootEmPx * value.Number;
    case Unit::None: return value.Number;
    default: break;
  }
  return std::nullopt;
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

inline constexpr int kDeepestNesting = 128;

inline constexpr size_t kMostPlacesPerBox = 64;

struct DepthGuard;

struct Placer {
  const Markup *Tree = nullptr;
  const Stylesheet *Agent = nullptr;
  Stylesheet *Author = nullptr;
  const Font *Face = nullptr;
  double RootEm = 16.0;
  std::vector<Box> *Out = nullptr;
  int Depth = 0;
  bool TooDeep = false;
  size_t Places = 0;
  size_t Budget = static_cast<size_t>(-1);
  bool TooCostly = false;
  size_t Measures = 0;
  size_t MeasureHits = 0;
  size_t Baselines_ = 0;
  size_t BaselineHits = 0;
  size_t Intrinsics = 0;
  size_t IntrinsicHits = 0;

  struct Measured {
    double Width = 0.0;
    double Height = 0.0;
  };

  std::unordered_map<uint64_t, Measured> Sizes;
  std::unordered_map<uint64_t, double> Baselines;
  std::unordered_map<uint64_t, double> MinContents;
  std::unordered_map<int, double> MaxContents;

  [[nodiscard]] static uint64_t MemoKey(Sizing what) {
    const auto rounded = static_cast<float>(what.AvailableWidth);
    uint32_t bits = 0;
    std::memcpy(&bits, &rounded, sizeof bits);
    return (static_cast<uint64_t>(static_cast<uint32_t>(what.Node)) << 32u) |
           static_cast<uint64_t>(bits);
  }

  struct DepthHeld {
    explicit DepthHeld(Placer &of) : Of(of) { ++Of.Depth; }

    ~DepthHeld() { --Of.Depth; }

    DepthHeld(const DepthHeld &) = delete;
    DepthHeld &operator=(const DepthHeld &) = delete;
    Placer &Of;
  };

  [[nodiscard]] Computed StyleOf(int node, const Computed *inherited) const;

  double Place(int node,
               const Computed *inherited,
               Area container,
               int parentBox,
               Measured used = {.Width = -1, .Height = -1});
  double Children(int node, const Computed &style, int self, Area content, double emPx);
  double Blocks(int node, const Computed &style, int self, Area content, double emPx);
  double Flex(int node, const Computed &style, int self, Area content, double emPx);
  double Runs(int node, const Computed &style, int self, Area content, double emPx) const;

  [[nodiscard]] Measured Measure(Sizing what, const Computed *inherited);

  double MaxContent(int node, const Computed *inherited);
  double MaxContentUncached(int node, const Computed *inherited);
  double MinContentUncached(int node, const Computed *inherited, bool ownSize);

  double MinContent(int node, const Computed *inherited, bool ownSize = true);

  double BaselineOf(int node, const Computed *inherited, double widthRoom);
  [[nodiscard]] double Clamped(double used,
                               const Computed &style,
                               Limits within,
                               LengthContext in,
                               double frame = 0.0) const;
  [[nodiscard]] double Width(std::string_view text, FontFace face) const;
};

[[nodiscard]] Family FaceOf(const Computed &style) {
  return FamilyOf(style.Word(Property::FontFamily, 0));
}

Computed Placer::StyleOf(int node, const Computed *inherited) const {
  Computed out;
  if (inherited != nullptr) {
    const std::array<Property, 6> carried = {{Property::Colour,
                                              Property::FontSize,
                                              Property::LineHeight,
                                              Property::TextAlign,
                                              Property::FontFamily,
                                              Property::WhiteSpace}};
    for (const Property what : carried) {
      if (inherited->Has(what)) {
        out.Held[static_cast<size_t>(what)] = inherited->Of(what);
        out.Set[static_cast<size_t>(what)] = true;
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
        found.push_back({.Specificity = rule.Specificity + bias, .Order = rule.Order, .One = &one});
      }
    }
  };

  gather(*Agent, kFarBeforeAny);
  gather(*Author, 0);
  std::ranges::stable_sort(found, [](const Ranked &a, const Ranked &b) {
    return a.Specificity == b.Specificity ? a.Order < b.Order : a.Specificity < b.Specificity;
  });
  for (const Ranked &one : found) { out.Take(*one.One); }
  const std::string *inlineStyle = Tree->AttributeOf(node, "style");
  if (inlineStyle != nullptr) {
    for (const Declaration &one : Author->Inline(*inlineStyle)) { out.Take(one); }
  }
  return out;
}

Placer::Measured Placer::Measure(Sizing what, const Computed *inherited) {
  ++Measures;
  const uint64_t key = MemoKey(what);
  const auto seen = Sizes.find(key);
  if (seen != Sizes.end()) {
    ++MeasureHits;
    return seen->second;
  }
  std::vector<Box> scratch;
  std::vector<Box> *held = Out;
  Out = &scratch;
  Place(what.Node, inherited, Area{.Width = what.AvailableWidth}, -1);
  Out = held;
  Measured got;
  got.Width = scratch.empty() ? 0 : scratch[0].Width;
  got.Height = scratch.empty() ? 0 : scratch[0].Height;

  double widest = 0;
  for (const Box &one : scratch) {
    if (one.Parent < 0) { continue; }
    widest = std::fmax(widest, one.X - scratch[0].X + one.Width);
  }
  if (widest > 0) { got.Width = std::fmin(got.Width, widest); }
  Sizes.emplace(key, got);
  if (!scratch.empty()) { Baselines.emplace(key, scratch[0].Baseline); }
  return got;
}

double Placer::MinContent(int node, const Computed *inherited, bool ownSize) {
  ++Intrinsics;
  const uint64_t key =
      (static_cast<uint64_t>(static_cast<uint32_t>(node)) << 1u) | (ownSize ? 1u : 0u);
  const auto seen = MinContents.find(key);
  if (seen != MinContents.end()) {
    ++IntrinsicHits;
    return seen->second;
  }
  const double answer = MinContentUncached(node, inherited, ownSize);
  MinContents.emplace(key, answer);
  return answer;
}

double Placer::MinContentUncached(int node, const Computed *inherited, bool ownSize) {
  const Node &element = Tree->Nodes()[static_cast<size_t>(node)];
  if (element.Kind == NodeKind::Text) { return 0; }
  const Computed style = StyleOf(node, inherited);
  const uint32_t display = style.Word(Property::Display, kDisplayInline);
  if (display == kDisplayNone) { return 0; }

  double emPx = kEmPx;
  if (style.Has(Property::FontSize)) {
    emPx = Resolve(style.Of(Property::FontSize),
                   {.AgainstPx = kEmPx, .EmPx = kEmPx, .RootEmPx = RootEm})
               .value_or(emPx);
  }
  const auto len = [&](Property what) -> std::optional<double> {
    if (!style.Has(what)) { return std::nullopt; }
    return Resolve(style.Of(what), {.AgainstPx = 0.0, .EmPx = emPx, .RootEmPx = RootEm});
  };
  const double frame =
      len(Property::BorderLeftWidth).value_or(0) + len(Property::BorderRightWidth).value_or(0) +
      len(Property::PaddingLeft).value_or(0) + len(Property::PaddingRight).value_or(0);
  const double margins =
      len(Property::MarginLeft).value_or(0) + len(Property::MarginRight).value_or(0);

  if (ownSize && style.Has(Property::Width) && style.Of(Property::Width).How == Unit::Pixels) {
    const double declared = style.Of(Property::Width).Number;
    return (style.Word(Property::BoxSizing, 0) == kBorderBox ? declared : declared + frame) +
           margins;
  }

  double own = 0;
  for (const int child : element.Children) {
    const Node &inner = Tree->Nodes()[static_cast<size_t>(child)];
    if (inner.Kind == NodeKind::Text) {
      const std::string text = Collapsed(inner.Text);
      size_t at = 0;
      while (at < text.size()) {
        const size_t end = text.find(' ', at);
        const size_t stop = end == std::string::npos ? text.size() : end;
        own = std::fmax(own,
                        Width(std::string_view(text).substr(at, stop - at),
                              {.Name = FaceOf(style), .SizePx = emPx}));
        at = stop == text.size() ? stop : stop + 1;
      }
      continue;
    }

    own = std::fmax(own, MinContent(child, &style));
  }
  return own + frame + margins;
}

double Placer::MaxContent(int node, const Computed *inherited) {
  ++Intrinsics;
  const auto seen = MaxContents.find(node);
  if (seen != MaxContents.end()) {
    ++IntrinsicHits;
    return seen->second;
  }
  const double answer = MaxContentUncached(node, inherited);
  MaxContents.emplace(node, answer);
  return answer;
}

double Placer::MaxContentUncached(int node, const Computed *inherited) {
  const Node &element = Tree->Nodes()[static_cast<size_t>(node)];
  if (element.Kind == NodeKind::Text) { return 0; }
  const Computed style = StyleOf(node, inherited);
  const uint32_t display = style.Word(Property::Display, kDisplayInline);
  if (display == kDisplayNone) { return 0; }

  double emPx = kEmPx;
  if (style.Has(Property::FontSize)) {
    emPx = Resolve(style.Of(Property::FontSize),
                   {.AgainstPx = kEmPx, .EmPx = kEmPx, .RootEmPx = RootEm})
               .value_or(emPx);
  }
  const auto len = [&](Property what) -> std::optional<double> {
    if (!style.Has(what)) { return std::nullopt; }
    return Resolve(style.Of(what), {.AgainstPx = 0.0, .EmPx = emPx, .RootEmPx = RootEm});
  };
  const double frame =
      len(Property::BorderLeftWidth).value_or(0) + len(Property::BorderRightWidth).value_or(0) +
      len(Property::PaddingLeft).value_or(0) + len(Property::PaddingRight).value_or(0);
  const double margins =
      len(Property::MarginLeft).value_or(0) + len(Property::MarginRight).value_or(0);

  if (style.Has(Property::Width) && style.Of(Property::Width).How == Unit::Pixels) {
    const double declared = style.Of(Property::Width).Number;
    return (style.Word(Property::BoxSizing, 0) == kBorderBox ? declared : declared + frame) +
           margins;
  }

  double own = 0;
  const uint32_t how = style.Word(Property::FlexDirection, 0);
  const bool row = display == kDisplayFlex && how != kColumn && how != kColumnReverse;
  double along = 0;
  int items = 0;
  for (const int child : element.Children) {
    const Node &node2 = Tree->Nodes()[static_cast<size_t>(child)];
    if (node2.Kind == NodeKind::Text) {
      own = std::fmax(own, Width(Collapsed(node2.Text), {.Name = FaceOf(style), .SizePx = emPx}));
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
    own = std::fmax(own, along + (items > 1 ? gap * static_cast<double>(items - 1) : 0.0));
  }
  return own + frame + margins;
}

namespace {}

double Placer::Width(std::string_view text, FontFace face) const {
  const FontMetrics metrics = Face->At(face);
  double width = 0;
  for (size_t at = 0; at < text.size();) {
    char32_t code = 0;
    at += ReadUtf8(text, at, code);
    const Glyph glyph = Face->Shape(code, face);
    width += glyph.AdvancePx > 0 ? glyph.AdvancePx : metrics.Advance;
  }
  return width;
}

double Placer::Clamped(
    double used, const Computed &style, Limits within, LengthContext in, double frame) const {
  const double sides = style.Word(Property::BoxSizing, 0) == kBorderBox ? 0.0 : frame;
  double out = used;

  const Property most = within.Most;
  if (most != Property::kCount && style.Has(most) && style.Of(most).How != Unit::Auto) {
    const std::optional<double> ceiling = Resolve(style.Of(most), in);
    if (ceiling) { out = std::fmin(out, *ceiling + sides); }
  }
  const Property least = within.Least;
  if (least != Property::kCount && style.Has(least) && style.Of(least).How != Unit::Auto) {
    const std::optional<double> floor = Resolve(style.Of(least), in);
    if (floor) { out = std::fmax(out, *floor + sides); }
  }

  return std::fmax(0.0, out);
}

double Placer::BaselineOf(int node, const Computed *inherited, double widthRoom) {
  ++Baselines_;
  const uint64_t key = MemoKey({.Node = node, .AvailableWidth = widthRoom});
  const auto seen = Baselines.find(key);
  if (seen != Baselines.end()) {
    ++BaselineHits;
    return seen->second;
  }
  const size_t before = Out->size();
  Place(node, inherited, Area{.Width = widthRoom}, -1);
  double baseline = 0;
  if (Out->size() > before) { baseline = (*Out)[before].Baseline; }
  Out->resize(before);
  Baselines.emplace(key, baseline);
  return baseline;
}

double Placer::Runs(int node, const Computed &style, int self, Area content, double emPx) const {
  const double contentX = content.X;
  const double contentY = content.Y;
  const double contentWidth = content.Width;
  const double lineFactor = style.Number(Property::LineHeight, 1.2);
  const double lineHeight = lineFactor > 3.0 ? lineFactor : lineFactor * emPx;
  const FontFace face = {.Name = FaceOf(style), .SizePx = emPx};
  const FontMetrics metrics = Face->At(face);
  const bool keepSpace = style.Word(Property::WhiteSpace, 0) == kPre;
  const uint32_t align = style.Word(Property::TextAlign, 0);
  double y = contentY;

  for (const int child : Tree->Nodes()[static_cast<size_t>(node)].Children) {
    const Node &run = Tree->Nodes()[static_cast<size_t>(child)];
    if (run.Kind != NodeKind::Text) { continue; }
    const std::string text = keepSpace ? run.Text : Collapsed(run.Text);

    if (text.empty() || (!keepSpace && text.find_first_not_of(' ') == std::string::npos)) {
      continue;
    }

    size_t at = 0;
    while (at < text.size()) {
      size_t take = text.size() - at;
      if (contentWidth > 0) {
        double width = 0;
        size_t lastSpace = std::string::npos;
        size_t cursor = at;
        while (cursor < text.size()) {
          char32_t code = 0;
          const size_t step = ReadUtf8(text, cursor, code);
          const Glyph glyph = Face->Shape(code, face);
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
            const size_t next = text.find(' ', at);
            take = next == std::string::npos ? text.size() - at : next - at;
          }
        }
      }
      Box line;
      line.Node = child;
      line.Text = text.substr(at, take);
      line.FontSize = emPx;
      line.Face = face.Name;
      line.Colour = style.Has(Property::Colour) ? style.Of(Property::Colour).Word : kOpaqueAlpha;
      line.Width = Width(line.Text, face);
      line.Height = lineHeight;
      line.Y = y;
      line.X = contentX;
      if (align == kCentre) { line.X = contentX + (contentWidth - line.Width) / 2.0; }
      if (align == kRight) { line.X = contentX + contentWidth - line.Width; }
      line.Parent = self;
      line.Baseline = (lineHeight - (metrics.Ascent + metrics.Descent)) / 2.0 + metrics.Ascent;

      if ((*Out)[static_cast<size_t>(self)].Baseline == 0.0) {
        (*Out)[static_cast<size_t>(self)].Baseline =
            (y - contentY) + line.Baseline +
            ((*Out)[static_cast<size_t>(self)].Border.Top +
             (*Out)[static_cast<size_t>(self)].Padding.Top);
      }
      Out->push_back(line);
      (*Out)[static_cast<size_t>(self)].Children.push_back(static_cast<int>(Out->size()) - 1);
      y += lineHeight;
      at += take;
      while (at < text.size() && text[at] == ' ') { ++at; }
    }
  }
  return y - contentY;
}

double Placer::Blocks(int node, const Computed &style, int self, Area content, double emPx) {
  double y = content.Y;
  y += Runs(node, style, self, {.X = content.X, .Y = y, .Width = content.Width}, emPx);
  for (const int child : Tree->Nodes()[static_cast<size_t>(node)].Children) {
    if (Tree->Nodes()[static_cast<size_t>(child)].Kind != NodeKind::Element) { continue; }

    y += Place(child,
               &style,
               {.X = content.X, .Y = y, .Width = content.Width, .Height = content.Height},
               self);
  }
  return y - content.Y;
}

double Placer::Flex(int node, const Computed &style, int self, Area content, double emPx) {
  const double contentX = content.X;
  const double contentY = content.Y;
  const double contentWidth = content.Width;
  const double contentHeight = content.Height;
  const uint32_t direction = style.Word(Property::FlexDirection, 0);
  const bool column = direction == kColumn || direction == kColumnReverse;

  const bool mainReversed = direction == kRowReverse || direction == kColumnReverse;
  const double mainRoom = column ? contentHeight : contentWidth;
  const double crossRoom = column ? contentWidth : std::fmax(0.0, contentHeight);

  const bool definiteMain = mainRoom >= 0.0;
  const double gap = style.Has(Property::Gap) ? style.Of(Property::Gap).Number : 0.0;
  const uint32_t wrapping = style.Word(Property::FlexWrap, 0);
  const bool reversed = wrapping == kWrapReverse;

  const uint32_t justify = Aligned(style.Word(Property::JustifyContent, 0), mainReversed);
  const uint32_t align = Aligned(style.Word(Property::AlignItems, kStretch), reversed);

  struct Item {
    int Node = 0;
    Computed Style;
    double Base = 0, Main = 0, Cross = 0;
    double MainMarginStart = 0, MainMarginEnd = 0, CrossMarginStart = 0, CrossMarginEnd = 0;
    double Grow = 0, Shrink = 1;
    double Em = 0, Floor = 0, Hypothetical = 0, Frame = 0;
    Property Least = Property::MinWidth, Most = Property::MaxWidth;
    bool CrossDeclared = false;
  };

  std::vector<Item> items;
  for (const int child : Tree->Nodes()[static_cast<size_t>(node)].Children) {
    if (Tree->Nodes()[static_cast<size_t>(child)].Kind != NodeKind::Element) { continue; }
    Item item;
    item.Node = child;
    item.Style = StyleOf(child, &style);
    if (item.Style.Word(Property::Display, kDisplayBlock) == kDisplayNone) { continue; }
    const double itemEm = item.Style.Has(Property::FontSize)
                              ? Resolve(item.Style.Of(Property::FontSize),
                                        {.AgainstPx = emPx, .EmPx = emPx, .RootEmPx = RootEm})
                                    .value_or(emPx)
                              : emPx;
    const auto len = [&](Property what, double against) -> std::optional<double> {
      if (!item.Style.Has(what)) { return std::nullopt; }
      return Resolve(item.Style.Of(what),
                     {.AgainstPx = against, .EmPx = itemEm, .RootEmPx = RootEm});
    };
    item.MainMarginStart = column ? len(Property::MarginTop, content.Width).value_or(0)
                                  : len(Property::MarginLeft, content.Width).value_or(0);
    item.MainMarginEnd = column ? len(Property::MarginBottom, content.Width).value_or(0)
                                : len(Property::MarginRight, content.Width).value_or(0);
    item.CrossMarginStart = column ? len(Property::MarginLeft, content.Width).value_or(0)
                                   : len(Property::MarginTop, content.Width).value_or(0);
    item.CrossMarginEnd = column ? len(Property::MarginRight, content.Width).value_or(0)
                                 : len(Property::MarginBottom, content.Width).value_or(0);
    item.Grow = item.Style.Number(Property::FlexGrow, 0);
    item.Shrink = item.Style.Number(Property::FlexShrink, 1);

    bool haveBase = false;
    const Property mainSize = column ? Property::Height : Property::Width;
    if (item.Style.Has(Property::FlexBasis) &&
        item.Style.Of(Property::FlexBasis).How != Unit::Auto) {
      const std::optional<double> basis =
          Resolve(item.Style.Of(Property::FlexBasis),
                  {.AgainstPx = mainRoom, .EmPx = itemEm, .RootEmPx = RootEm});
      haveBase = basis.has_value();
      item.Base = basis.value_or(0.0);
    }
    if (!haveBase && item.Style.Has(mainSize) && item.Style.Of(mainSize).How != Unit::Auto) {
      const std::optional<double> declared = Resolve(
          item.Style.Of(mainSize), {.AgainstPx = mainRoom, .EmPx = itemEm, .RootEmPx = RootEm});
      haveBase = declared.has_value();
      item.Base = declared.value_or(0.0);
    }

    if (haveBase && item.Style.Word(Property::BoxSizing, 0) != kBorderBox) {
      const double frame = column
                               ? len(Property::BorderTopWidth, content.Width).value_or(0) +
                                     len(Property::BorderBottomWidth, content.Width).value_or(0) +
                                     len(Property::PaddingTop, content.Width).value_or(0) +
                                     len(Property::PaddingBottom, content.Width).value_or(0)
                               : len(Property::BorderLeftWidth, content.Width).value_or(0) +
                                     len(Property::BorderRightWidth, content.Width).value_or(0) +
                                     len(Property::PaddingLeft, content.Width).value_or(0) +
                                     len(Property::PaddingRight, content.Width).value_or(0);
      item.Base += frame;
    }
    item.Frame = column ? len(Property::BorderTopWidth, content.Width).value_or(0) +
                              len(Property::BorderBottomWidth, content.Width).value_or(0) +
                              len(Property::PaddingTop, content.Width).value_or(0) +
                              len(Property::PaddingBottom, content.Width).value_or(0)
                        : len(Property::BorderLeftWidth, content.Width).value_or(0) +
                              len(Property::BorderRightWidth, content.Width).value_or(0) +
                              len(Property::PaddingLeft, content.Width).value_or(0) +
                              len(Property::PaddingRight, content.Width).value_or(0);
    if (!haveBase) {
      if (column) {
        item.Base = Measure({.Node = child, .AvailableWidth = contentWidth}, &style).Height;
      } else {
        item.Base = MaxContent(child, &style) - item.MainMarginStart - item.MainMarginEnd;
      }
    }
    const Property crossSize = column ? Property::Width : Property::Height;
    item.CrossDeclared = item.Style.Has(crossSize) && item.Style.Of(crossSize).How != Unit::Auto;
    if (item.CrossDeclared) {
      const std::optional<double> across = Resolve(
          item.Style.Of(crossSize), {.AgainstPx = crossRoom, .EmPx = itemEm, .RootEmPx = RootEm});
      item.CrossDeclared = across.has_value();
      item.Cross = across.value_or(0.0);
    }

    item.Least = column ? Property::MinHeight : Property::MinWidth;
    item.Most = column ? Property::MaxHeight : Property::MaxWidth;
    item.Em = itemEm;

    if (!item.Style.Has(item.Least) || item.Style.Of(item.Least).How == Unit::Auto) {
      const Value spilling =
          item.Style.Has(Property::Overflow) ? item.Style.Of(Property::Overflow) : Value{};
      if (spilling.How != Unit::Auto && spilling.Word != kHidden && spilling.Word != kScroll) {
        if (column) {
          item.Floor = Measure({.Node = child, .AvailableWidth = contentWidth}, &style).Height;
        } else {
          const double narrowest =
              MinContent(child, &style, false) - item.MainMarginStart - item.MainMarginEnd;

          double suggestion = narrowest;
          if (item.Style.Has(mainSize) && item.Style.Of(mainSize).How != Unit::Auto) {
            const std::optional<double> specified =
                Resolve(item.Style.Of(mainSize),
                        {.AgainstPx = mainRoom, .EmPx = itemEm, .RootEmPx = RootEm});
            if (specified) {
              suggestion = std::fmin(
                  narrowest,
                  *specified +
                      (item.Style.Word(Property::BoxSizing, 0) == kBorderBox ? 0.0 : item.Frame));
            }
          }
          item.Floor = Clamped(suggestion,
                               item.Style,
                               {.Least = Property::kCount, .Most = item.Most},
                               {.AgainstPx = contentWidth, .EmPx = itemEm, .RootEmPx = RootEm},
                               item.Frame);
        }
      }
    }
    item.Main = item.Base;

    item.Hypothetical =
        std::fmax(Clamped(item.Base,
                          item.Style,
                          {.Least = item.Least, .Most = item.Most},
                          {.AgainstPx = mainRoom, .EmPx = itemEm, .RootEmPx = RootEm},
                          item.Frame),
                  item.Floor);
    items.push_back(std::move(item));
  }
  if (items.empty()) { return 0; }

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
      const double outer =
          items[i].Hypothetical + items[i].MainMarginStart + items[i].MainMarginEnd;
      const double withGap = line.Count == 0 ? outer : taken + gap + outer;
      if (wraps && line.Count > 0 && definiteMain && mainRoom > 0 && withGap > mainRoom) {
        lines.push_back(line);
        line = Line{.From = i, .Count = 0, .Cross = 0, .CrossAt = 0};
        taken = outer;
      } else {
        taken = withGap;
      }
      ++line.Count;
    }
    lines.push_back(line);
  }

  for (Line &line : lines) {
    double taken = gap * static_cast<double>(line.Count - 1);
    for (size_t i = line.From; i < line.From + line.Count; ++i) {
      taken += items[i].Base + items[i].MainMarginStart + items[i].MainMarginEnd;
    }

    const double outerBases = taken;
    const bool growing = definiteMain && mainRoom > outerBases;
    std::vector<bool> frozen(line.Count, false);
    for (size_t i = 0; i < line.Count; ++i) {
      Item &one = items[line.From + i];
      one.Main = one.Base;
      if (!definiteMain) {
        one.Main = one.Hypothetical;
        frozen[i] = true;
        continue;
      }
      const double factor = growing ? one.Grow : one.Shrink;
      if (factor == 0.0 || (growing && one.Base > one.Hypothetical) ||
          (!growing && one.Base < one.Hypothetical)) {
        one.Main = one.Hypothetical;
        frozen[i] = true;
      }
    }
    for (size_t pass = 0; pass <= line.Count; ++pass) {
      double held = gap * static_cast<double>(line.Count - 1);
      double factors = 0;
      size_t loose = 0;
      for (size_t i = 0; i < line.Count; ++i) {
        const Item &one = items[line.From + i];
        held += one.MainMarginStart + one.MainMarginEnd + (frozen[i] ? one.Main : one.Base);
        if (frozen[i]) { continue; }
        ++loose;
        factors += growing ? one.Grow : one.Shrink * one.Base;
      }
      if (loose == 0 || factors <= 0.0) { break; }
      const double free = mainRoom - held;
      double violation = 0;
      for (size_t i = 0; i < line.Count; ++i) {
        if (frozen[i]) { continue; }
        Item &one = items[line.From + i];
        const double share = growing ? one.Grow : one.Shrink * one.Base;
        const double wanted = std::fmax(0.0, one.Base + free * (share / factors));
        const double clamped = Clamped(std::fmax(wanted, one.Floor),
                                       one.Style,
                                       {.Least = one.Least, .Most = one.Most},
                                       {.AgainstPx = mainRoom, .EmPx = one.Em, .RootEmPx = RootEm},
                                       one.Frame);
        one.Main = clamped;
        violation += clamped - wanted;
      }
      if (violation == 0.0) {
        for (size_t i = 0; i < line.Count; ++i) { frozen[i] = true; }
        break;
      }

      for (size_t i = 0; i < line.Count; ++i) {
        if (frozen[i]) { continue; }
        const Item &one = items[line.From + i];
        const double share = growing ? one.Grow : one.Shrink * one.Base;
        const double wanted = std::fmax(0.0, one.Base + free * (share / factors));
        const double mine = one.Main - wanted;
        if ((violation > 0 && mine > 0) || (violation < 0 && mine < 0)) { frozen[i] = true; }
      }
    }
    for (size_t i = line.From; i < line.From + line.Count; ++i) {
      Item &one = items[i];
      if (!one.CrossDeclared) {
        if (column) {
          one.Cross = MaxContent(one.Node, &style) - one.CrossMarginStart - one.CrossMarginEnd;
        } else {
          one.Cross = Measure({.Node = one.Node, .AvailableWidth = one.Main}, &style).Height;
        }
      }
      line.Cross = std::fmax(line.Cross, one.Cross + one.CrossMarginStart + one.CrossMarginEnd);
    }
  }

  double linesDeep = 0;
  for (const Line &line : lines) { linesDeep += line.Cross; }
  linesDeep += gap * static_cast<double>(lines.size() - 1);
  if (!wraps && crossRoom > 0) {
    lines[0].Cross = crossRoom;
    linesDeep = crossRoom;
  }

  const uint32_t alignLines = Aligned(style.Word(Property::AlignContent, kStretch), reversed);
  double lineAt = 0;
  double betweenLines = gap;
  const double crossSlack = crossRoom - linesDeep;
  if (crossRoom > 0 && crossSlack > 0 && lines.size() > 0) {
    if (alignLines == kStretch) {
      const double share = crossSlack / static_cast<double>(lines.size());
      for (Line &line : lines) { line.Cross += share; }
    } else if (alignLines == kFlexEnd) {
      lineAt = crossSlack;
    } else if (alignLines == kCentre) {
      lineAt = crossSlack / 2.0;
    } else if (alignLines == kSpaceBetween && lines.size() > 1) {
      betweenLines = gap + crossSlack / static_cast<double>(lines.size() - 1);
    } else if (alignLines == kSpaceAround) {
      lineAt = crossSlack / static_cast<double>(lines.size() * 2);
      betweenLines = gap + crossSlack / static_cast<double>(lines.size());
    } else if (alignLines == kSpaceEvenly) {
      lineAt = crossSlack / static_cast<double>(lines.size() + 1);
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

  std::vector<double> lineBaseline(lines.size(), 0.0);
  for (size_t at = 0; at < lines.size(); ++at) {
    for (size_t i = lines[at].From; i < lines[at].From + lines[at].Count; ++i) {
      const Item &one = items[i];
      const uint32_t how = one.Style.Has(Property::AlignSelf)
                               ? Aligned(one.Style.Word(Property::AlignSelf, align), reversed)
                               : align;
      if (how != kBaseline || column) { continue; }

      lineBaseline[at] = std::fmax(lineBaseline[at],
                                   BaselineOf(one.Node, &style, one.Main) + one.CrossMarginStart);
    }
  }

  double deepest = 0;
  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    const Line &flexLine = lines[lineIndex];
    double lineUsed = gap * static_cast<double>(flexLine.Count - 1);
    for (size_t i = flexLine.From; i < flexLine.From + flexLine.Count; ++i) {
      lineUsed += items[i].Main + items[i].MainMarginStart + items[i].MainMarginEnd;
    }
    double cursor = 0;
    double between = gap;
    const double slack = definiteMain ? mainRoom - lineUsed : 0.0;
    if (slack > 0) {
      if (justify == kFlexEnd) {
        cursor = slack;
      } else if (justify == kCentre) {
        cursor = slack / 2.0;
      } else if (justify == kSpaceBetween && flexLine.Count > 1) {
        between = gap + slack / static_cast<double>(flexLine.Count - 1);
      } else if (justify == kSpaceAround) {
        cursor = slack / static_cast<double>(flexLine.Count * 2);
        between = gap + slack / static_cast<double>(flexLine.Count);
      } else if (justify == kSpaceEvenly) {
        cursor = slack / static_cast<double>(flexLine.Count + 1);
        between = gap + cursor;
      }
    } else if (justify == kCentre) {
      cursor = slack / 2.0;
    }

    for (size_t i = flexLine.From; i < flexLine.From + flexLine.Count; ++i) {
      const Item &one = items[i];
      cursor += one.MainMarginStart;
      const uint32_t self_align =
          one.Style.Has(Property::AlignSelf)
              ? Aligned(one.Style.Word(Property::AlignSelf, align), reversed)
              : align;
      double cross = one.Cross;
      if (!one.CrossDeclared && self_align == kStretch) {
        cross = flexLine.Cross - one.CrossMarginStart - one.CrossMarginEnd;
      }
      double inLine = one.CrossMarginStart;
      if (self_align == kCentre) {
        inLine = (flexLine.Cross - cross - one.CrossMarginStart - one.CrossMarginEnd) / 2.0 +
                 one.CrossMarginStart;
      } else if (self_align == kFlexEnd) {
        inLine = flexLine.Cross - cross - one.CrossMarginEnd;
      }

      if (self_align == kBaseline && !column) {
        inLine =
            one.CrossMarginStart + (lineBaseline[lineIndex] -
                                    BaselineOf(one.Node, &style, one.Main) - one.CrossMarginStart);
        inLine = std::fmax(inLine, 0.0);
      }
      if (wrapping == kWrapReverse) { inLine = flexLine.Cross - inLine - cross; }
      const double crossAt = flexLine.CrossAt + inLine;

      const double mirrorAgainst = definiteMain ? mainRoom : lineUsed;
      const double mainAt = mainReversed ? mirrorAgainst - cursor - one.Main : cursor;
      const double x = column ? contentX + crossAt : contentX + mainAt;
      const double y = column ? contentY + mainAt : contentY + crossAt;

      const bool stretched = !one.CrossDeclared && self_align == kStretch;
      const double usedW = column ? (stretched || one.CrossDeclared ? cross : -1.0) : one.Main;
      const double usedH = column ? one.Main : (stretched || one.CrossDeclared ? cross : -1.0);
      const int before = static_cast<int>(Out->size());
      Place(one.Node,
            &style,
            {.X = x - (column ? one.CrossMarginStart : one.MainMarginStart),
             .Y = y - (column ? one.MainMarginStart : one.CrossMarginStart),
             .Width = contentWidth,
             .Height = contentHeight},
            self,
            {.Width = usedW, .Height = usedH});
      if (std::cmp_less(before, Out->size())) {
        Box &placed = (*Out)[static_cast<size_t>(before)];
        if (column) {
          placed.Height = one.Main;
          if (!one.CrossDeclared && self_align == kStretch) { placed.Width = cross; }
        } else {
          placed.Width = one.Main;
          if (!one.CrossDeclared && self_align == kStretch) { placed.Height = cross; }
        }
        deepest = std::fmax(deepest,
                            column ? mainAt + one.Main + one.MainMarginEnd
                                   : crossAt + placed.Height + one.CrossMarginEnd);
      }
      cursor += one.Main + one.MainMarginEnd + between;
    }
  }

  double crossExtent = 0;
  for (const Line &line : lines) {
    crossExtent = std::fmax(crossExtent, line.CrossAt + line.Cross);
  }
  return column ? deepest : std::fmax(deepest, crossExtent);
}

double Placer::Children(int node, const Computed &style, int self, Area content, double emPx) {
  const uint32_t display = style.Word(Property::Display, kDisplayInline);
  return display == kDisplayFlex || display == kDisplayInlineFlex
             ? Flex(node, style, self, content, emPx)
             : Blocks(node, style, self, content, emPx);
}

double
Placer::Place(int node, const Computed *inherited, Area container, int parentBox, Measured used) {
  ++Places;
  if (Places > Budget) { TooCostly = true; }
  if (TooCostly) { return 0; }
  if (TooDeep) { return 0; }
  if (Depth >= kDeepestNesting) {
    TooDeep = true;
    return 0;
  }
  const DepthHeld held(*this);
  const Node &element = Tree->Nodes()[static_cast<size_t>(node)];
  if (element.Kind == NodeKind::Text) { return 0; }

  const Computed style = StyleOf(node, inherited);
  const uint32_t display = style.Word(Property::Display, kDisplayInline);
  if (display == kDisplayNone) { return 0; }

  double emPx = kEmPx;
  if (style.Has(Property::FontSize)) {
    emPx =
        Resolve(
            style.Of(Property::FontSize),
            {.AgainstPx = inherited != nullptr ? kEmPx : RootEm, .EmPx = kEmPx, .RootEmPx = RootEm})
            .value_or(emPx);
  }
  const auto len = [&](Property what, double against) -> std::optional<double> {
    if (!style.Has(what)) { return std::nullopt; }
    return Resolve(style.Of(what), {.AgainstPx = against, .EmPx = emPx, .RootEmPx = RootEm});
  };

  Box box;
  box.Node = node;
  box.Margin = {.Top = len(Property::MarginTop, container.Width).value_or(0),
                .Right = len(Property::MarginRight, container.Width).value_or(0),
                .Bottom = len(Property::MarginBottom, container.Width).value_or(0),
                .Left = len(Property::MarginLeft, container.Width).value_or(0)};
  box.Border = {.Top = len(Property::BorderTopWidth, container.Width).value_or(0),
                .Right = len(Property::BorderRightWidth, container.Width).value_or(0),
                .Bottom = len(Property::BorderBottomWidth, container.Width).value_or(0),
                .Left = len(Property::BorderLeftWidth, container.Width).value_or(0)};
  box.Padding = {.Top = len(Property::PaddingTop, container.Width).value_or(0),
                 .Right = len(Property::PaddingRight, container.Width).value_or(0),
                 .Bottom = len(Property::PaddingBottom, container.Width).value_or(0),
                 .Left = len(Property::PaddingLeft, container.Width).value_or(0)};
  box.Background =
      style.Has(Property::BackgroundColour) ? style.Of(Property::BackgroundColour).Word : 0;
  box.BorderColour = style.Has(Property::BorderColour) ? style.Of(Property::BorderColour).Word : 0;
  box.Radius = len(Property::BorderRadius, container.Width).value_or(0);
  box.Opacity = style.Has(Property::Opacity) ? style.Of(Property::Opacity).Number : 1.0;
  const Value spills = style.Has(Property::Overflow) ? style.Of(Property::Overflow) : Value{};
  box.Scrolls = spills.How == Unit::Auto || spills.Word == kScroll;
  box.Clips = spills.Word == kHidden || box.Scrolls;
  box.Positioned =
      style.Has(Property::Position) && style.Word(Property::Position, kStatic) != kStatic;
  box.Colour = style.Has(Property::Colour) ? style.Of(Property::Colour).Word : kOpaqueAlpha;
  box.FontSize = emPx;
  box.Parent = parentBox;

  const bool borderBox = style.Word(Property::BoxSizing, 0) == kBorderBox;
  const double frameX = box.Border.Left + box.Border.Right + box.Padding.Left + box.Padding.Right;
  const double frameY = box.Border.Top + box.Border.Bottom + box.Padding.Top + box.Padding.Bottom;

  bool widthAbsent = true;
  double contentWidth = 0;
  if (style.Has(Property::Width) && style.Of(Property::Width).How != Unit::Auto) {
    const std::optional<double> declared =
        Resolve(style.Of(Property::Width),
                {.AgainstPx = container.Width, .EmPx = emPx, .RootEmPx = RootEm});
    widthAbsent = !declared.has_value();
    contentWidth = declared.value_or(0.0);
    if (!widthAbsent && borderBox) { contentWidth = std::fmax(0.0, contentWidth - frameX); }
  }
  if (widthAbsent) {
    contentWidth = std::fmax(0.0, container.Width - box.Margin.Left - box.Margin.Right - frameX);

    if (style.Word(Property::Display, kDisplayInline) == kDisplayInlineFlex) {
      const double wants =
          MaxContent(node, inherited) - box.Margin.Left - box.Margin.Right - frameX;
      contentWidth = std::fmax(0.0, std::fmin(contentWidth, wants));
    }
  }
  contentWidth = Clamped(contentWidth + (borderBox ? frameX : 0.0),
                         style,
                         {.Least = Property::MinWidth, .Most = Property::MaxWidth},
                         {.AgainstPx = container.Width, .EmPx = emPx, .RootEmPx = RootEm}) -
                 (borderBox ? frameX : 0.0);

  if (used.Width >= 0) {
    widthAbsent = false;
    contentWidth = std::fmax(0.0, used.Width - frameX);
  }

  bool heightAbsent = true;
  double contentHeight = 0;
  if (style.Has(Property::Height) && style.Of(Property::Height).How != Unit::Auto) {
    const std::optional<double> declared =
        Resolve(style.Of(Property::Height),
                {.AgainstPx = container.Height, .EmPx = emPx, .RootEmPx = RootEm});
    heightAbsent = !declared.has_value();
    contentHeight = declared.value_or(0.0);
    if (!heightAbsent && borderBox) { contentHeight = std::fmax(0.0, contentHeight - frameY); }
  }

  box.X = container.X + box.Margin.Left;
  box.Y = container.Y + box.Margin.Top;
  const int self = static_cast<int>(Out->size());
  Out->push_back(box);
  if (parentBox >= 0) { (*Out)[static_cast<size_t>(parentBox)].Children.push_back(self); }

  const double contentX = box.X + box.Border.Left + box.Padding.Left;
  const double contentY = box.Y + box.Border.Top + box.Padding.Top;

  if (used.Height >= 0) {
    heightAbsent = false;
    contentHeight = std::fmax(0.0, used.Height - frameY);
  }
  double heightRoom = heightAbsent ? -1.0 : contentHeight;
  if (heightAbsent && style.Has(Property::MaxHeight) &&
      style.Of(Property::MaxHeight).How != Unit::Auto) {
    const std::optional<double> ceiling =
        Resolve(style.Of(Property::MaxHeight),
                {.AgainstPx = container.Height, .EmPx = emPx, .RootEmPx = RootEm});
    if (ceiling) { heightRoom = std::fmax(0.0, *ceiling - (borderBox ? frameY : 0.0)); }
  }
  const double deep = Children(node,
                               style,
                               self,
                               {.X = contentX,
                                .Y = contentY,
                                .Width = contentWidth,
                                .Height = std::fmax(heightRoom, heightRoom < 0 ? -1.0 : 0.0)},
                               emPx);
  if (heightAbsent) { contentHeight = deep; }

  contentHeight = Clamped(contentHeight + (borderBox ? frameY : 0.0),
                          style,
                          {.Least = Property::MinHeight, .Most = Property::MaxHeight},
                          {.AgainstPx = container.Height, .EmPx = emPx, .RootEmPx = RootEm}) -
                  (borderBox ? frameY : 0.0);

  (*Out)[static_cast<size_t>(self)].Width = contentWidth + frameX;
  (*Out)[static_cast<size_t>(self)].Height = contentHeight + frameY;

  if ((*Out)[static_cast<size_t>(self)].Baseline == 0.0) {
    for (const int child : (*Out)[static_cast<size_t>(self)].Children) {
      const Box &inner = (*Out)[static_cast<size_t>(child)];
      if (inner.Baseline > 0.0) {
        (*Out)[static_cast<size_t>(self)].Baseline =
            (inner.Y - (*Out)[static_cast<size_t>(self)].Y) + inner.Baseline;
        break;
      }
    }
  }
  if ((*Out)[static_cast<size_t>(self)].Baseline == 0.0) {
    (*Out)[static_cast<size_t>(self)].Baseline =
        (*Out)[static_cast<size_t>(self)].Height + box.Margin.Bottom;
  }
  return (*Out)[static_cast<size_t>(self)].Height + box.Margin.Top + box.Margin.Bottom;
}

} // namespace

namespace {

constexpr std::array<std::string_view, 39> kNotABox = {{
    "img",      "picture", "source",   "video",    "audio",   "canvas",   "iframe", "embed",
    "object",   "svg",     "math",     "input",    "select",  "textarea", "button", "label",
    "fieldset", "legend",  "progress", "meter",    "details", "summary",  "form",   "marquee",
    "table",    "thead",   "tbody",    "tfoot",    "tr",      "td",       "th",     "caption",
    "colgroup", "col",     "frame",    "frameset", "applet",  "template", "slot",
}};

}

bool ElementIsInTheSubset(std::string_view tag) {
  for (const std::string_view different : kNotABox) {
    if (different == tag) { return false; }
  }
  return true;
}

std::vector<std::string> ElementsOutsideTheSubset(const Markup &markup) {
  std::vector<std::string> outside;
  for (int index = 0; std::cmp_less(index, markup.Nodes().size()); ++index) {
    const Node &node = markup.Nodes()[static_cast<size_t>(index)];

    if (index == markup.Root()) { continue; }
    if (node.Kind != NodeKind::Element || ElementIsInTheSubset(node.Name)) { continue; }
    bool already = false;
    for (const std::string &seen : outside) { already = already || seen == node.Name; }
    if (!already) { outside.push_back(node.Name); }
  }
  return outside;
}

const char *UserAgentSheet() {
  return "html, body, div, p, h1, h2, h3, h4, h5, h6, section, article, header, footer, nav, main,"
         " ul, ol, li, blockquote, figure, form, fieldset, pre { display: block }\n"
         "span, a, b, i, em, strong, small, code, label { display: inline }\n"
         "body { margin: 8px }\n"
         "p, blockquote, figure, h1, h2, h3, h4, h5, h6, ul, ol, pre, form { margin: 1em 0 }\n"
         "html { color: black; font-size: 16px; line-height: 1.2; text-align: left }\n"

         "head, title, link, meta, style, script, base, noscript { display: none }\n";
}

bool Layout::Build(const Markup &markup,
                   Stylesheet &sheet,
                   double viewportWidth,
                   double viewportHeight,
                   const Font &font,
                   std::span<const Scrolled> scrolled,
                   std::string &error) {
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

  size_t elements = 0;
  for (const Node &one : markup.Nodes()) {
    if (one.Kind == NodeKind::Element) { ++elements; }
  }
  placer.Budget = (elements + 1) * kMostPlacesPerBox;

  double y = 0;
  for (const int child : markup.Nodes()[static_cast<size_t>(markup.Root())].Children) {
    if (markup.Nodes()[static_cast<size_t>(child)].Kind != NodeKind::Element) { continue; }
    y += placer.Place(
        child, nullptr, {.X = 0, .Y = y, .Width = viewportWidth, .Height = viewportHeight}, -1);
  }
  Spent_ = Work{.Places = placer.Places,
                .Measures = placer.Measures,
                .MeasureHits = placer.MeasureHits,
                .Baselines = placer.Baselines_,
                .BaselineHits = placer.BaselineHits,
                .Intrinsics = placer.Intrinsics,
                .IntrinsicHits = placer.IntrinsicHits};
  if (placer.TooCostly) {
    Boxes_.clear();
    error = "the declaration costs more than the " + std::to_string(kMostPlacesPerBox) +
            " placements per box this layout budgets (" + std::to_string(elements) + " elements, " +
            std::to_string(placer.Places) +
            " placements spent) -- a shape whose cost multiplies with nesting is refused, "
            "not walked for minutes";
    return false;
  }
  if (placer.TooDeep) {
    Boxes_.clear();
    error = "the declaration nests deeper than the " + std::to_string(kDeepestNesting) +
            " levels this layout walks -- the walk spends stack per level and a document "
            "past the bound is a refusal, never a crash";
    return false;
  }

  for (size_t at = 0; at < Boxes_.size(); ++at) {
    Box &over = Boxes_[at];
    if (!over.Scrolls) { continue; }
    double reaches = over.Y;
    for (const Box &under : Boxes_) {
      for (int up = under.Parent; up >= 0; up = Boxes_[static_cast<size_t>(up)].Parent) {
        if (std::cmp_not_equal(up, at)) { continue; }
        reaches = std::fmax(reaches, under.Y + under.Height);
        break;
      }
    }
    const double room = over.Height - over.Border.Top - over.Border.Bottom;
    over.ContentPx = reaches - over.Y;
    const double most = over.ContentPx > room ? over.ContentPx - room : 0.0;
    double by = 0.0;
    for (const Scrolled &one : scrolled) {
      if (one.Node == over.Node) { by = one.Px; }
    }
    over.ScrolledPx = std::clamp(by, 0.0, most);
    if (over.ScrolledPx <= 0.0) { continue; }
    for (auto &Boxe : Boxes_) {
      for (int up = Boxe.Parent; up >= 0; up = Boxes_[static_cast<size_t>(up)].Parent) {
        if (std::cmp_not_equal(up, at)) { continue; }
        Boxe.Y -= over.ScrolledPx;
        break;
      }
    }
  }
  return true;
}

int Layout::Scroller(double x, double y) const {
  for (size_t at = Boxes_.size(); at-- > 0;) {
    const Box &box = Boxes_[at];
    if (!box.Scrolls) { continue; }
    if (x >= box.X && x < box.X + box.Width && y >= box.Y && y < box.Y + box.Height) {
      return box.Node;
    }
  }
  return -1;
}

double Layout::ScrollableBy(int node) const {
  for (const Box &box : Boxes_) {
    if (box.Node != node || !box.Scrolls) { continue; }
    const double room = box.Height - box.Border.Top - box.Border.Bottom;
    return box.ContentPx > room ? box.ContentPx - room : 0.0;
  }
  return 0.0;
}

int Layout::Hit(double x, double y) const {
  for (size_t at = Boxes_.size(); at-- > 0;) {
    const Box &box = Boxes_[at];
    if (x < box.X || x >= box.X + box.Width || y < box.Y || y >= box.Y + box.Height) { continue; }

    bool seen = true;
    for (int up = box.Parent; up >= 0 && seen; up = Boxes_[static_cast<size_t>(up)].Parent) {
      const Box &over = Boxes_[static_cast<size_t>(up)];
      if (!over.Clips) { continue; }
      const double left = over.X + over.Border.Left;
      const double top = over.Y + over.Border.Top;
      const double right = over.X + over.Width - over.Border.Right;
      const double bottom = over.Y + over.Height - over.Border.Bottom;
      seen = x >= left && x < right && y >= top && y < bottom;
    }
    if (seen) { return box.Node; }
  }
  return -1;
}

} // namespace outshine::Ui
