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

  [[nodiscard]] Box
  BoxOf(int node, const Computed &style, int parentBox, Area container, double emPx) const;

  struct Framing {
    int Node = 0;
    const Computed *Inherited = nullptr;
    const Computed *Style = nullptr;
    const Box *Held = nullptr;
    Area Container;
    double EmPx = 0;
    double FrameX = 0;
    double FrameY = 0;
    double UsedWidth = -1;
    bool BorderBox = false;
  };

  [[nodiscard]] double ContentWidthOf(const Framing &sized, bool &widthAbsent);
  [[nodiscard]] double DeclaredHeightOf(const Framing &sized, bool &heightAbsent) const;
  void SettleBaseline(int self, double marginBottom) const;
  double Place(int node,
               const Computed *inherited,
               Area container,
               int parentBox,
               Measured used = {.Width = -1, .Height = -1});
  double Children(int node, const Computed &style, int self, Area content, double emPx);
  double Blocks(int node, const Computed &style, int self, Area content, double emPx);

  struct FlexItem {
    int Node = 0;
    Computed Style;
    double Base = 0, Main = 0, Cross = 0;
    double MainMarginStart = 0, MainMarginEnd = 0, CrossMarginStart = 0, CrossMarginEnd = 0;
    double Grow = 0, Shrink = 1;
    double Em = 0, Floor = 0, Hypothetical = 0, Frame = 0;
    Property Least = Property::MinWidth, Most = Property::MaxWidth;
    bool CrossDeclared = false;
  };

  struct FlexLine {
    size_t From = 0, Count = 0;
    double Cross = 0, CrossAt = 0;
  };

  struct Flexing {
    Area Content;
    double MainRoom = 0, CrossRoom = 0, Gap = 0, EmPx = 0;
    uint32_t Justify = 0, Align = 0, Wrapping = 0;
    bool Column = false, MainReversed = false, Reversed = false, Wraps = false;
    bool DefiniteMain = false;
  };

  [[nodiscard]] bool
  CollectFlexItem(int child, const Computed &style, const Flexing &over, FlexItem &item);
  void FlexBaseSize(int child, const Computed &style, const Flexing &over, FlexItem &item);
  [[nodiscard]] double
  ContentSuggestionOf(int child, const Computed &style, const Flexing &over, const FlexItem &item);
  void FlexItemFloor(int child, const Computed &style, const Flexing &over, FlexItem &item);
  [[nodiscard]] bool CollectFlexItems(int node,
                                      const Computed &style,
                                      const Flexing &over,
                                      std::vector<FlexItem> &items);
  static void CollectFlexLines(const std::vector<FlexItem> &items,
                               const Flexing &over,
                               std::vector<FlexLine> &lines);
  [[nodiscard]] static bool FreezeInflexible(const Flexing &over,
                                             const FlexLine &line,
                                             std::vector<FlexItem> &items,
                                             std::vector<bool> &frozen);

  struct FlexPass {
    double Free = 0;
    double Factors = 0;
    size_t Loose = 0;
  };

  [[nodiscard]] static double FlexShareOf(const FlexItem &one, bool growing);
  [[nodiscard]] static double
  FlexWantedOf(const FlexItem &one, bool growing, double free, double factors);
  [[nodiscard]] static FlexPass MeasureFlexPass(const Flexing &over,
                                                const FlexLine &line,
                                                bool growing,
                                                const std::vector<FlexItem> &items,
                                                const std::vector<bool> &frozen);
  [[nodiscard]] double ApplyFlexShares(const Flexing &over,
                                       const FlexLine &line,
                                       bool growing,
                                       FlexPass pass,
                                       std::vector<FlexItem> &items,
                                       const std::vector<bool> &frozen);
  void DistributeFreeSpace(const Flexing &over,
                           const FlexLine &line,
                           bool growing,
                           std::vector<FlexItem> &items,
                           std::vector<bool> &frozen);
  void LineCrossSize(const Computed &style,
                     const Flexing &over,
                     FlexLine &line,
                     std::vector<FlexItem> &items);
  void ResolveFlexibleLengths(const Computed &style,
                              const Flexing &over,
                              std::vector<FlexItem> &items,
                              std::vector<FlexLine> &lines);
  static void
  AlignFlexLines(const Computed &style, const Flexing &over, std::vector<FlexLine> &lines);

  struct CrossPlaced {
    uint32_t Align = 0;
    double Cross = 0;
    double InLine = 0;
  };

  [[nodiscard]] CrossPlaced AlignInCrossAxis(const Computed &style,
                                             const Flexing &over,
                                             const FlexLine &flexLine,
                                             const FlexItem &one,
                                             double lineBaseline);

  struct Placing {
    double Cursor = 0;
    double LineUsed = 0;
    double LineBaseline = 0;
  };

  double PlaceFlexItem(const Computed &style,
                       const Flexing &over,
                       const FlexLine &flexLine,
                       const FlexItem &one,
                       Placing placing,
                       int self);
  double PlaceFlexLine(const Computed &style,
                       const Flexing &over,
                       const FlexLine &flexLine,
                       std::vector<FlexItem> &items,
                       int self,
                       double lineBaseline);
  double Flex(int node, const Computed &style, int self, Area content, double emPx);
  [[nodiscard]] double
  Runs(int node, const Computed &style, int self, Area content, double emPx) const;

  struct Typesetting {
    FontFace Face;
    FontMetrics Metrics;
  };

  [[nodiscard]] size_t
  BreakRun(std::string_view text, size_t at, const Typesetting &set, double roomPx) const;

  struct LineSetting {
    int Child = 0;
    int Self = 0;
    std::string Text;
    Typesetting Set;
    const Computed *Style = nullptr;
    Area At;
    double RoomPx = 0;
    double LineHeight = 0;
    double EmPx = 0;
    uint32_t Align = 0;
  };

  [[nodiscard]] Box LineBoxOf(const LineSetting &set) const;

  [[nodiscard]] Measured Measure(Sizing what, const Computed *inherited);

  double MaxContent(int node, const Computed *inherited);
  double MaxContentUncached(int node, const Computed *inherited);
  double MinContentUncached(int node, const Computed *inherited, bool ownSize);

  double MinContent(int node, const Computed *inherited, bool ownSize = true);

  double BaselineOf(int node, const Computed *inherited, double widthRoom);
  [[nodiscard]] static double
  Clamped(double used, const Computed &style, Limits within, LengthContext in, double frame = 0.0);
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

double
Placer::Clamped(double used, const Computed &style, Limits within, LengthContext in, double frame) {
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

size_t
Placer::BreakRun(std::string_view text, size_t at, const Typesetting &set, double roomPx) const {
  size_t take = text.size() - at;
  if (roomPx > 0) {
    double width = 0;
    size_t lastSpace = std::string::npos;
    size_t cursor = at;
    while (cursor < text.size()) {
      char32_t code = 0;
      const size_t step = ReadUtf8(text, cursor, code);
      const Glyph glyph = Face->Shape(code, set.Face);
      const double advance = glyph.AdvancePx > 0 ? glyph.AdvancePx : set.Metrics.Advance;
      if (width + advance > roomPx && cursor > at) { break; }
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
  return take;
}

Box Placer::LineBoxOf(const LineSetting &set) const {
  Box line;
  line.Node = set.Child;
  line.Text = set.Text;
  line.FontSize = set.EmPx;
  line.Face = set.Set.Face.Name;
  line.Colour =
      set.Style->Has(Property::Colour) ? set.Style->Of(Property::Colour).Word : kOpaqueAlpha;
  line.Width = Width(line.Text, set.Set.Face);
  line.Height = set.LineHeight;
  line.Y = set.At.Y;
  line.X = set.At.X;
  if (set.Align == kCentre) { line.X = set.At.X + (set.RoomPx - line.Width) / 2.0; }
  if (set.Align == kRight) { line.X = set.At.X + set.RoomPx - line.Width; }
  line.Parent = set.Self;
  line.Baseline = (set.LineHeight - (set.Set.Metrics.Ascent + set.Set.Metrics.Descent)) / 2.0 +
                  set.Set.Metrics.Ascent;

  return line;
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
      const size_t take = BreakRun(text, at, {.Face = face, .Metrics = metrics}, contentWidth);
      const Box line = LineBoxOf({.Child = child,
                                  .Self = self,
                                  .Text = text.substr(at, take),
                                  .Set = {.Face = face, .Metrics = metrics},
                                  .Style = &style,
                                  .At = {.X = contentX, .Y = y},
                                  .RoomPx = contentWidth,
                                  .LineHeight = lineHeight,
                                  .EmPx = emPx,
                                  .Align = align});
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

void Placer::FlexBaseSize(int child, const Computed &style, const Flexing &over, FlexItem &item) {
  const auto len = [&](Property what, double against) -> std::optional<double> {
    if (!item.Style.Has(what)) { return std::nullopt; }
    return Resolve(item.Style.Of(what),
                   {.AgainstPx = against, .EmPx = item.Em, .RootEmPx = RootEm});
  };
  bool haveBase = false;
  const Property mainSize = over.Column ? Property::Height : Property::Width;
  if (item.Style.Has(Property::FlexBasis) && item.Style.Of(Property::FlexBasis).How != Unit::Auto) {
    const std::optional<double> basis =
        Resolve(item.Style.Of(Property::FlexBasis),
                {.AgainstPx = over.MainRoom, .EmPx = item.Em, .RootEmPx = RootEm});
    haveBase = basis.has_value();
    item.Base = basis.value_or(0.0);
  }
  if (!haveBase && item.Style.Has(mainSize) && item.Style.Of(mainSize).How != Unit::Auto) {
    const std::optional<double> declared = Resolve(
        item.Style.Of(mainSize), {.AgainstPx = over.MainRoom, .EmPx = item.Em, .RootEmPx = RootEm});
    haveBase = declared.has_value();
    item.Base = declared.value_or(0.0);
  }

  if (haveBase && item.Style.Word(Property::BoxSizing, 0) != kBorderBox) {
    const double frame =
        over.Column ? len(Property::BorderTopWidth, over.Content.Width).value_or(0) +
                          len(Property::BorderBottomWidth, over.Content.Width).value_or(0) +
                          len(Property::PaddingTop, over.Content.Width).value_or(0) +
                          len(Property::PaddingBottom, over.Content.Width).value_or(0)
                    : len(Property::BorderLeftWidth, over.Content.Width).value_or(0) +
                          len(Property::BorderRightWidth, over.Content.Width).value_or(0) +
                          len(Property::PaddingLeft, over.Content.Width).value_or(0) +
                          len(Property::PaddingRight, over.Content.Width).value_or(0);
    item.Base += frame;
  }
  item.Frame = over.Column ? len(Property::BorderTopWidth, over.Content.Width).value_or(0) +
                                 len(Property::BorderBottomWidth, over.Content.Width).value_or(0) +
                                 len(Property::PaddingTop, over.Content.Width).value_or(0) +
                                 len(Property::PaddingBottom, over.Content.Width).value_or(0)
                           : len(Property::BorderLeftWidth, over.Content.Width).value_or(0) +
                                 len(Property::BorderRightWidth, over.Content.Width).value_or(0) +
                                 len(Property::PaddingLeft, over.Content.Width).value_or(0) +
                                 len(Property::PaddingRight, over.Content.Width).value_or(0);
  if (!haveBase) {
    if (over.Column) {
      item.Base = Measure({.Node = child, .AvailableWidth = over.Content.Width}, &style).Height;
    } else {
      item.Base = MaxContent(child, &style) - item.MainMarginStart - item.MainMarginEnd;
    }
  }
}

double Placer::ContentSuggestionOf(int child,
                                   const Computed &style,
                                   const Flexing &over,
                                   const FlexItem &item) {
  const Property mainSize = over.Column ? Property::Height : Property::Width;
  const double narrowest =
      MinContent(child, &style, false) - item.MainMarginStart - item.MainMarginEnd;
  if (!item.Style.Has(mainSize) || item.Style.Of(mainSize).How == Unit::Auto) { return narrowest; }
  const std::optional<double> specified = Resolve(
      item.Style.Of(mainSize), {.AgainstPx = over.MainRoom, .EmPx = item.Em, .RootEmPx = RootEm});
  if (!specified) { return narrowest; }
  return std::fmin(narrowest,
                   *specified +
                       (item.Style.Word(Property::BoxSizing, 0) == kBorderBox ? 0.0 : item.Frame));
}

void Placer::FlexItemFloor(int child, const Computed &style, const Flexing &over, FlexItem &item) {
  if (item.Style.Has(item.Least) && item.Style.Of(item.Least).How != Unit::Auto) { return; }
  const Value spilling =
      item.Style.Has(Property::Overflow) ? item.Style.Of(Property::Overflow) : Value{};
  if (spilling.How == Unit::Auto || spilling.Word == kHidden || spilling.Word == kScroll) {
    return;
  }
  if (over.Column) {
    item.Floor = Measure({.Node = child, .AvailableWidth = over.Content.Width}, &style).Height;
    return;
  }
  item.Floor = Clamped(ContentSuggestionOf(child, style, over, item),
                       item.Style,
                       {.Least = Property::kCount, .Most = item.Most},
                       {.AgainstPx = over.Content.Width, .EmPx = item.Em, .RootEmPx = RootEm},
                       item.Frame);
}

bool Placer::CollectFlexItem(int child,
                             const Computed &style,
                             const Flexing &over,
                             FlexItem &item) {
  item.Node = child;
  item.Style = StyleOf(child, &style);
  if (item.Style.Word(Property::Display, kDisplayBlock) == kDisplayNone) { return false; }
  const double itemEm =
      item.Style.Has(Property::FontSize)
          ? Resolve(item.Style.Of(Property::FontSize),
                    {.AgainstPx = over.EmPx, .EmPx = over.EmPx, .RootEmPx = RootEm})
                .value_or(over.EmPx)
          : over.EmPx;
  const auto len = [&](Property what, double against) -> std::optional<double> {
    if (!item.Style.Has(what)) { return std::nullopt; }
    return Resolve(item.Style.Of(what), {.AgainstPx = against, .EmPx = itemEm, .RootEmPx = RootEm});
  };
  item.MainMarginStart = over.Column ? len(Property::MarginTop, over.Content.Width).value_or(0)
                                     : len(Property::MarginLeft, over.Content.Width).value_or(0);
  item.MainMarginEnd = over.Column ? len(Property::MarginBottom, over.Content.Width).value_or(0)
                                   : len(Property::MarginRight, over.Content.Width).value_or(0);
  item.CrossMarginStart = over.Column ? len(Property::MarginLeft, over.Content.Width).value_or(0)
                                      : len(Property::MarginTop, over.Content.Width).value_or(0);
  item.CrossMarginEnd = over.Column ? len(Property::MarginRight, over.Content.Width).value_or(0)
                                    : len(Property::MarginBottom, over.Content.Width).value_or(0);
  item.Grow = item.Style.Number(Property::FlexGrow, 0);
  item.Shrink = item.Style.Number(Property::FlexShrink, 1);

  FlexBaseSize(child, style, over, item);
  const Property crossSize = over.Column ? Property::Width : Property::Height;
  item.CrossDeclared = item.Style.Has(crossSize) && item.Style.Of(crossSize).How != Unit::Auto;
  if (item.CrossDeclared) {
    const std::optional<double> across =
        Resolve(item.Style.Of(crossSize),
                {.AgainstPx = over.CrossRoom, .EmPx = itemEm, .RootEmPx = RootEm});
    item.CrossDeclared = across.has_value();
    item.Cross = across.value_or(0.0);
  }

  item.Least = over.Column ? Property::MinHeight : Property::MinWidth;
  item.Most = over.Column ? Property::MaxHeight : Property::MaxWidth;
  item.Em = itemEm;

  FlexItemFloor(child, style, over, item);
  item.Main = item.Base;

  item.Hypothetical =
      std::fmax(Clamped(item.Base,
                        item.Style,
                        {.Least = item.Least, .Most = item.Most},
                        {.AgainstPx = over.MainRoom, .EmPx = itemEm, .RootEmPx = RootEm},
                        item.Frame),
                item.Floor);
  return true;
}

bool Placer::CollectFlexItems(int node,
                              const Computed &style,
                              const Flexing &over,
                              std::vector<FlexItem> &items) {
  for (const int child : Tree->Nodes()[static_cast<size_t>(node)].Children) {
    if (Tree->Nodes()[static_cast<size_t>(child)].Kind != NodeKind::Element) { continue; }
    FlexItem item;
    if (!CollectFlexItem(child, style, over, item)) { continue; }
    items.push_back(item);
  }
  return !items.empty();
}

void Placer::CollectFlexLines(const std::vector<FlexItem> &items,
                              const Flexing &over,
                              std::vector<FlexLine> &lines) {
  {
    FlexLine line;
    line.From = 0;
    double taken = 0;
    for (size_t i = 0; i < items.size(); ++i) {
      const double outer =
          items[i].Hypothetical + items[i].MainMarginStart + items[i].MainMarginEnd;
      const double withGap = line.Count == 0 ? outer : taken + over.Gap + outer;
      if (over.Wraps && line.Count > 0 && over.DefiniteMain && over.MainRoom > 0 &&
          withGap > over.MainRoom) {
        lines.push_back(line);
        line = FlexLine{.From = i, .Count = 0, .Cross = 0, .CrossAt = 0};
        taken = outer;
      } else {
        taken = withGap;
      }
      ++line.Count;
    }
    lines.push_back(line);
  }
}

bool Placer::FreezeInflexible(const Flexing &over,
                              const FlexLine &line,
                              std::vector<FlexItem> &items,
                              std::vector<bool> &frozen) {
  double taken = over.Gap * static_cast<double>(line.Count - 1);
  for (size_t i = line.From; i < line.From + line.Count; ++i) {
    taken += items[i].Base + items[i].MainMarginStart + items[i].MainMarginEnd;
  }

  const double outerBases = taken;
  const bool growing = over.DefiniteMain && over.MainRoom > outerBases;
  for (size_t i = 0; i < line.Count; ++i) {
    FlexItem &one = items[line.From + i];
    one.Main = one.Base;
    if (!over.DefiniteMain) {
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
  return growing;
}

double Placer::FlexShareOf(const FlexItem &one, bool growing) {
  return growing ? one.Grow : one.Shrink * one.Base;
}

double Placer::FlexWantedOf(const FlexItem &one, bool growing, double free, double factors) {
  return std::fmax(0.0, one.Base + free * (FlexShareOf(one, growing) / factors));
}

Placer::FlexPass Placer::MeasureFlexPass(const Flexing &over,
                                         const FlexLine &line,
                                         bool growing,
                                         const std::vector<FlexItem> &items,
                                         const std::vector<bool> &frozen) {
  FlexPass pass;
  double held = over.Gap * static_cast<double>(line.Count - 1);
  for (size_t i = 0; i < line.Count; ++i) {
    const FlexItem &one = items[line.From + i];
    held += one.MainMarginStart + one.MainMarginEnd + (frozen[i] ? one.Main : one.Base);
    if (frozen[i]) { continue; }
    ++pass.Loose;
    pass.Factors += FlexShareOf(one, growing);
  }
  pass.Free = over.MainRoom - held;
  return pass;
}

double Placer::ApplyFlexShares(const Flexing &over,
                               const FlexLine &line,
                               bool growing,
                               FlexPass pass,
                               std::vector<FlexItem> &items,
                               const std::vector<bool> &frozen) {
  double violation = 0;
  for (size_t i = 0; i < line.Count; ++i) {
    if (frozen[i]) { continue; }
    FlexItem &one = items[line.From + i];
    const double wanted = FlexWantedOf(one, growing, pass.Free, pass.Factors);
    const double clamped = Clamped(std::fmax(wanted, one.Floor),
                                   one.Style,
                                   {.Least = one.Least, .Most = one.Most},
                                   {.AgainstPx = over.MainRoom, .EmPx = one.Em, .RootEmPx = RootEm},
                                   one.Frame);
    one.Main = clamped;
    violation += clamped - wanted;
  }
  return violation;
}

void Placer::DistributeFreeSpace(const Flexing &over,
                                 const FlexLine &line,
                                 bool growing,
                                 std::vector<FlexItem> &items,
                                 std::vector<bool> &frozen) {
  for (size_t pass = 0; pass <= line.Count; ++pass) {
    const FlexPass measured = MeasureFlexPass(over, line, growing, items, frozen);
    if (measured.Loose == 0 || measured.Factors <= 0.0) { break; }
    const double violation = ApplyFlexShares(over, line, growing, measured, items, frozen);
    if (violation == 0.0) {
      frozen.assign(line.Count, true);
      break;
    }
    for (size_t i = 0; i < line.Count; ++i) {
      if (frozen[i]) { continue; }
      const FlexItem &one = items[line.From + i];
      const double mine = one.Main - FlexWantedOf(one, growing, measured.Free, measured.Factors);
      if ((violation > 0 && mine > 0) || (violation < 0 && mine < 0)) { frozen[i] = true; }
    }
  }
}

void Placer::LineCrossSize(const Computed &style,
                           const Flexing &over,
                           FlexLine &line,
                           std::vector<FlexItem> &items) {
  for (size_t i = line.From; i < line.From + line.Count; ++i) {
    FlexItem &one = items[i];
    if (!one.CrossDeclared) {
      if (over.Column) {
        one.Cross = MaxContent(one.Node, &style) - one.CrossMarginStart - one.CrossMarginEnd;
      } else {
        one.Cross = Measure({.Node = one.Node, .AvailableWidth = one.Main}, &style).Height;
      }
    }
    line.Cross = std::fmax(line.Cross, one.Cross + one.CrossMarginStart + one.CrossMarginEnd);
  }
}

void Placer::ResolveFlexibleLengths(const Computed &style,
                                    const Flexing &over,
                                    std::vector<FlexItem> &items,
                                    std::vector<FlexLine> &lines) {
  for (FlexLine &line : lines) {
    std::vector<bool> frozen(line.Count, false);
    const bool growing = FreezeInflexible(over, line, items, frozen);
    DistributeFreeSpace(over, line, growing, items, frozen);
    LineCrossSize(style, over, line, items);
  }
}

void Placer::AlignFlexLines(const Computed &style,
                            const Flexing &over,
                            std::vector<FlexLine> &lines) {
  double linesDeep = 0;
  for (const FlexLine &line : lines) { linesDeep += line.Cross; }
  linesDeep += over.Gap * static_cast<double>(lines.size() - 1);
  if (!over.Wraps && over.CrossRoom > 0) {
    lines[0].Cross = over.CrossRoom;
    linesDeep = over.CrossRoom;
  }

  const uint32_t alignLines = Aligned(style.Word(Property::AlignContent, kStretch), over.Reversed);
  double lineAt = 0;
  double betweenLines = over.Gap;
  const double crossSlack = over.CrossRoom - linesDeep;
  if (over.CrossRoom > 0 && crossSlack > 0 && !lines.empty()) {
    if (alignLines == kStretch) {
      const double share = crossSlack / static_cast<double>(lines.size());
      for (FlexLine &line : lines) { line.Cross += share; }
    } else if (alignLines == kFlexEnd) {
      lineAt = crossSlack;
    } else if (alignLines == kCentre) {
      lineAt = crossSlack / 2.0;
    } else if (alignLines == kSpaceBetween && lines.size() > 1) {
      betweenLines = over.Gap + crossSlack / static_cast<double>(lines.size() - 1);
    } else if (alignLines == kSpaceAround) {
      lineAt = crossSlack / static_cast<double>(lines.size() * 2);
      betweenLines = over.Gap + crossSlack / static_cast<double>(lines.size());
    } else if (alignLines == kSpaceEvenly) {
      lineAt = crossSlack / static_cast<double>(lines.size() + 1);
      betweenLines = over.Gap + lineAt;
    }
  }
  for (FlexLine &line : lines) {
    line.CrossAt = lineAt;
    lineAt += line.Cross + betweenLines;
  }
  if (over.Wrapping == kWrapReverse && over.CrossRoom > 0) {
    for (FlexLine &line : lines) { line.CrossAt = over.CrossRoom - line.CrossAt - line.Cross; }
  }
}

double Placer::Flex(int node, const Computed &style, int self, Area content, double emPx) {
  const uint32_t direction = style.Word(Property::FlexDirection, 0);
  const bool column = direction == kColumn || direction == kColumnReverse;
  const uint32_t wrapping = style.Word(Property::FlexWrap, 0);
  const bool reversed = wrapping == kWrapReverse;
  const Flexing over{
      .Content = content,
      .MainRoom = column ? content.Height : content.Width,
      .CrossRoom = column ? content.Width : std::fmax(0.0, content.Height),
      .Gap = style.Has(Property::Gap) ? style.Of(Property::Gap).Number : 0.0,
      .EmPx = emPx,
      .Justify = Aligned(style.Word(Property::JustifyContent, 0),
                         direction == kRowReverse || direction == kColumnReverse),
      .Align = Aligned(style.Word(Property::AlignItems, kStretch), reversed),
      .Wrapping = wrapping,
      .Column = column,
      .MainReversed = direction == kRowReverse || direction == kColumnReverse,
      .Reversed = reversed,
      .Wraps = wrapping == kWrap || wrapping == kWrapReverse,
      .DefiniteMain = (column ? content.Height : content.Width) >= 0.0,
  };

  std::vector<FlexItem> items;
  if (!CollectFlexItems(node, style, over, items)) { return 0; }
  std::vector<FlexLine> lines;
  CollectFlexLines(items, over, lines);
  ResolveFlexibleLengths(style, over, items, lines);
  AlignFlexLines(style, over, lines);

  std::vector<double> lineBaseline(lines.size(), 0.0);
  for (size_t at = 0; at < lines.size(); ++at) {
    for (size_t i = lines[at].From; i < lines[at].From + lines[at].Count; ++i) {
      const FlexItem &one = items[i];
      const uint32_t how =
          one.Style.Has(Property::AlignSelf)
              ? Aligned(one.Style.Word(Property::AlignSelf, over.Align), over.Reversed)
              : over.Align;
      if (how != kBaseline || over.Column) { continue; }

      lineBaseline[at] = std::fmax(lineBaseline[at],
                                   BaselineOf(one.Node, &style, one.Main) + one.CrossMarginStart);
    }
  }

  double deepest = 0;
  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    deepest = std::fmax(
        deepest,
        PlaceFlexLine(style, over, lines[lineIndex], items, self, lineBaseline[lineIndex]));
  }

  double crossExtent = 0;
  for (const FlexLine &line : lines) {
    crossExtent = std::fmax(crossExtent, line.CrossAt + line.Cross);
  }
  return over.Column ? deepest : std::fmax(deepest, crossExtent);
}

Placer::CrossPlaced Placer::AlignInCrossAxis(const Computed &style,
                                             const Flexing &over,
                                             const FlexLine &flexLine,
                                             const FlexItem &one,
                                             double lineBaseline) {
  const uint32_t self_align =
      one.Style.Has(Property::AlignSelf)
          ? Aligned(one.Style.Word(Property::AlignSelf, over.Align), over.Reversed)
          : over.Align;
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

  if (self_align == kBaseline && !over.Column) {
    inLine = one.CrossMarginStart +
             (lineBaseline - BaselineOf(one.Node, &style, one.Main) - one.CrossMarginStart);
    inLine = std::fmax(inLine, 0.0);
  }
  if (over.Wrapping == kWrapReverse) { inLine = flexLine.Cross - inLine - cross; }

  return {.Align = self_align, .Cross = cross, .InLine = inLine};
}

double Placer::PlaceFlexItem(const Computed &style,
                             const Flexing &over,
                             const FlexLine &flexLine,
                             const FlexItem &one,
                             Placing placing,
                             int self) {
  double deepest = 0;
  const CrossPlaced placed = AlignInCrossAxis(style, over, flexLine, one, placing.LineBaseline);
  const uint32_t self_align = placed.Align;
  const double cross = placed.Cross;
  const double crossAt = flexLine.CrossAt + placed.InLine;

  const double mirrorAgainst = over.DefiniteMain ? over.MainRoom : placing.LineUsed;
  const double mainAt =
      over.MainReversed ? mirrorAgainst - placing.Cursor - one.Main : placing.Cursor;
  const double x = over.Column ? over.Content.X + crossAt : over.Content.X + mainAt;
  const double y = over.Column ? over.Content.Y + mainAt : over.Content.Y + crossAt;

  const bool stretched = !one.CrossDeclared && self_align == kStretch;
  const double across = stretched || one.CrossDeclared ? cross : -1.0;
  const double usedW = over.Column ? across : one.Main;
  const double usedH = over.Column ? one.Main : across;
  const int before = static_cast<int>(Out->size());
  Place(one.Node,
        &style,
        {.X = x - (over.Column ? one.CrossMarginStart : one.MainMarginStart),
         .Y = y - (over.Column ? one.MainMarginStart : one.CrossMarginStart),
         .Width = over.Content.Width,
         .Height = over.Content.Height},
        self,
        {.Width = usedW, .Height = usedH});
  if (std::cmp_less(before, Out->size())) {
    Box &box = (*Out)[static_cast<size_t>(before)];
    if (over.Column) {
      box.Height = one.Main;
      if (!one.CrossDeclared && self_align == kStretch) { box.Width = cross; }
    } else {
      box.Width = one.Main;
      if (!one.CrossDeclared && self_align == kStretch) { box.Height = cross; }
    }
    deepest = std::fmax(deepest,
                        over.Column ? mainAt + one.Main + one.MainMarginEnd
                                    : crossAt + box.Height + one.CrossMarginEnd);
  }
  return deepest;
}

double Placer::PlaceFlexLine(const Computed &style,
                             const Flexing &over,
                             const FlexLine &flexLine,
                             std::vector<FlexItem> &items,
                             int self,
                             double lineBaseline) {
  double deepest = 0;
  double lineUsed = over.Gap * static_cast<double>(flexLine.Count - 1);
  for (size_t i = flexLine.From; i < flexLine.From + flexLine.Count; ++i) {
    lineUsed += items[i].Main + items[i].MainMarginStart + items[i].MainMarginEnd;
  }
  double cursor = 0;
  double between = over.Gap;
  const double slack = over.DefiniteMain ? over.MainRoom - lineUsed : 0.0;
  if (slack > 0) {
    if (over.Justify == kFlexEnd) {
      cursor = slack;
    } else if (over.Justify == kCentre) {
      cursor = slack / 2.0;
    } else if (over.Justify == kSpaceBetween && flexLine.Count > 1) {
      between = over.Gap + slack / static_cast<double>(flexLine.Count - 1);
    } else if (over.Justify == kSpaceAround) {
      cursor = slack / static_cast<double>(flexLine.Count * 2);
      between = over.Gap + slack / static_cast<double>(flexLine.Count);
    } else if (over.Justify == kSpaceEvenly) {
      cursor = slack / static_cast<double>(flexLine.Count + 1);
      between = over.Gap + cursor;
    }
  } else if (over.Justify == kCentre) {
    cursor = slack / 2.0;
  }

  for (size_t i = flexLine.From; i < flexLine.From + flexLine.Count; ++i) {
    const FlexItem &one = items[i];
    cursor += one.MainMarginStart;
    deepest = std::fmax(
        deepest,
        PlaceFlexItem(style,
                      over,
                      flexLine,
                      one,
                      {.Cursor = cursor, .LineUsed = lineUsed, .LineBaseline = lineBaseline},
                      self));
    cursor += one.Main + one.MainMarginEnd + between;
  }
  return deepest;
}

double Placer::Children(int node, const Computed &style, int self, Area content, double emPx) {
  const uint32_t display = style.Word(Property::Display, kDisplayInline);
  return display == kDisplayFlex || display == kDisplayInlineFlex
             ? Flex(node, style, self, content, emPx)
             : Blocks(node, style, self, content, emPx);
}

Box Placer::BoxOf(
    int node, const Computed &style, int parentBox, Area container, double emPx) const {
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

  return box;
}

double Placer::ContentWidthOf(const Framing &sized, bool &widthAbsent) {
  double contentWidth = 0;
  if (sized.Style->Has(Property::Width) && sized.Style->Of(Property::Width).How != Unit::Auto) {
    const std::optional<double> declared =
        Resolve(sized.Style->Of(Property::Width),
                {.AgainstPx = sized.Container.Width, .EmPx = sized.EmPx, .RootEmPx = RootEm});
    widthAbsent = !declared.has_value();
    contentWidth = declared.value_or(0.0);
    if (!widthAbsent && sized.BorderBox) {
      contentWidth = std::fmax(0.0, contentWidth - sized.FrameX);
    }
  }
  if (widthAbsent) {
    contentWidth = std::fmax(0.0,
                             sized.Container.Width - sized.Held->Margin.Left -
                                 sized.Held->Margin.Right - sized.FrameX);

    if (sized.Style->Word(Property::Display, kDisplayInline) == kDisplayInlineFlex) {
      const double wants = MaxContent(sized.Node, sized.Inherited) - sized.Held->Margin.Left -
                           sized.Held->Margin.Right - sized.FrameX;
      contentWidth = std::fmax(0.0, std::fmin(contentWidth, wants));
    }
  }
  contentWidth =
      Clamped(contentWidth + (sized.BorderBox ? sized.FrameX : 0.0),
              *sized.Style,
              {.Least = Property::MinWidth, .Most = Property::MaxWidth},
              {.AgainstPx = sized.Container.Width, .EmPx = sized.EmPx, .RootEmPx = RootEm}) -
      (sized.BorderBox ? sized.FrameX : 0.0);

  if (sized.UsedWidth >= 0) {
    widthAbsent = false;
    contentWidth = std::fmax(0.0, sized.UsedWidth - sized.FrameX);
  }

  return contentWidth;
}

double Placer::DeclaredHeightOf(const Framing &sized, bool &heightAbsent) const {
  const Computed &style = *sized.Style;
  double contentHeight = 0;
  if (style.Has(Property::Height) && style.Of(Property::Height).How != Unit::Auto) {
    const std::optional<double> declared =
        Resolve(style.Of(Property::Height),
                {.AgainstPx = sized.Container.Height, .EmPx = sized.EmPx, .RootEmPx = RootEm});
    heightAbsent = !declared.has_value();
    contentHeight = declared.value_or(0.0);
    if (!heightAbsent && sized.BorderBox) {
      contentHeight = std::fmax(0.0, contentHeight - sized.FrameY);
    }
  }

  return contentHeight;
}

void Placer::SettleBaseline(int self, double marginBottom) const {
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
        (*Out)[static_cast<size_t>(self)].Height + marginBottom;
  }
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
  Box box = BoxOf(node, style, parentBox, container, emPx);
  const bool borderBox = style.Word(Property::BoxSizing, 0) == kBorderBox;
  const double frameX = box.Border.Left + box.Border.Right + box.Padding.Left + box.Padding.Right;
  const double frameY = box.Border.Top + box.Border.Bottom + box.Padding.Top + box.Padding.Bottom;

  const Framing sized{.Node = node,
                      .Inherited = inherited,
                      .Style = &style,
                      .Held = &box,
                      .Container = container,
                      .EmPx = emPx,
                      .FrameX = frameX,
                      .FrameY = frameY,
                      .UsedWidth = used.Width,
                      .BorderBox = borderBox};
  bool widthAbsent = true;
  const double contentWidth = ContentWidthOf(sized, widthAbsent);
  bool heightAbsent = true;
  double contentHeight = DeclaredHeightOf(sized, heightAbsent);

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

  SettleBaseline(self, box.Margin.Bottom);

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
  return std::ranges::none_of(kNotABox,
                              [tag](std::string_view different) { return different == tag; });
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

bool Layout::DescendsFrom(size_t at, Under root) const {
  for (int up = Boxes_[at].Parent; up >= 0; up = Boxes_[static_cast<size_t>(up)].Parent) {
    if (std::cmp_equal(up, root.Box)) { return true; }
  }
  return false;
}

double Layout::ReachesUnder(Under root, double from) const {
  double reaches = from;
  for (size_t at = 0; at < Boxes_.size(); ++at) {
    if (DescendsFrom(at, root)) { reaches = std::fmax(reaches, Boxes_[at].Y + Boxes_[at].Height); }
  }
  return reaches;
}

void Layout::ShiftUnder(Under root, double byPx) {
  for (size_t at = 0; at < Boxes_.size(); ++at) {
    if (DescendsFrom(at, root)) { Boxes_[at].Y -= byPx; }
  }
}

void Layout::SettleScrollers(std::span<const Scrolled> scrolled) {
  for (size_t at = 0; at < Boxes_.size(); ++at) {
    Box &over = Boxes_[at];
    if (!over.Scrolls) { continue; }
    const double reaches = ReachesUnder({.Box = at}, over.Y);
    const double room = over.Height - over.Border.Top - over.Border.Bottom;
    over.ContentPx = reaches - over.Y;
    const double most = over.ContentPx > room ? over.ContentPx - room : 0.0;
    double by = 0.0;
    for (const Scrolled &one : scrolled) {
      if (one.Node == over.Node) { by = one.Px; }
    }
    over.ScrolledPx = std::clamp(by, 0.0, most);
    if (over.ScrolledPx <= 0.0) { continue; }
    ShiftUnder({.Box = at}, over.ScrolledPx);
  }
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

  SettleScrollers(scrolled);
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
