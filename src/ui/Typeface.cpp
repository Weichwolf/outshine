#include "Typeface.h"

#include <array>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "Style.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Ui {

constexpr unsigned kFamilyShift = 56u;
constexpr uint64_t kGoldenWord32 = 0x9E3779B97F4A7C15ull;
constexpr double kAdvanceShare = 0.5;

namespace {

constexpr int kSheetEdge = 2048;
constexpr int kPad = 1;
constexpr size_t kCellSlots = 1u << 14u;

struct Named {
  const char *Spelled;
  Family Is;
};

constexpr std::array<Named, 30> kFamilies = {{
    {.Spelled = "serif", .Is = Family::Serif},
    {.Spelled = "times", .Is = Family::Serif},
    {.Spelled = "times new roman", .Is = Family::Serif},
    {.Spelled = "georgia", .Is = Family::Serif},
    {.Spelled = "garamond", .Is = Family::Serif},
    {.Spelled = "palatino", .Is = Family::Serif},
    {.Spelled = "book antiqua", .Is = Family::Serif},
    {.Spelled = "ui-serif", .Is = Family::Serif},

    {.Spelled = "monospace", .Is = Family::Mono},
    {.Spelled = "courier", .Is = Family::Mono},
    {.Spelled = "courier new", .Is = Family::Mono},
    {.Spelled = "consolas", .Is = Family::Mono},
    {.Spelled = "menlo", .Is = Family::Mono},
    {.Spelled = "monaco", .Is = Family::Mono},
    {.Spelled = "sf mono", .Is = Family::Mono},
    {.Spelled = "dejavu sans mono", .Is = Family::Mono},
    {.Spelled = "liberation mono", .Is = Family::Mono},
    {.Spelled = "ui-monospace", .Is = Family::Mono},

    {.Spelled = "sans-serif", .Is = Family::Sans},
    {.Spelled = "arial", .Is = Family::Sans},
    {.Spelled = "helvetica", .Is = Family::Sans},
    {.Spelled = "helvetica neue", .Is = Family::Sans},
    {.Spelled = "verdana", .Is = Family::Sans},
    {.Spelled = "tahoma", .Is = Family::Sans},
    {.Spelled = "segoe ui", .Is = Family::Sans},
    {.Spelled = "system-ui", .Is = Family::Sans},
    {.Spelled = "ui-sans-serif", .Is = Family::Sans},
    {.Spelled = "roboto", .Is = Family::Sans},
    {.Spelled = "inter", .Is = Family::Sans},
    {.Spelled = "dejavu sans", .Is = Family::Sans},
}};

constexpr std::array<const char *, 3> kFiles = {
    {"DejaVuSans.ttf", "DejaVuSerif.ttf", "DejaVuSansMono.ttf"}};
static_assert(sizeof(kFiles) / sizeof(kFiles[0]) == static_cast<size_t>(Family::kCount),
              "every family the catalogue offers names the file it is set in");

[[nodiscard]] uint64_t Keyed(Family family, int sizePx, char32_t code) {
  return (static_cast<uint64_t>(family) << kFamilyShift) |
         (static_cast<uint64_t>(static_cast<uint32_t>(sizePx)) << 32u) |
         static_cast<uint64_t>(code);
}

[[nodiscard]] std::string Lowered(std::string_view from) {
  std::string out;
  out.reserve(from.size());
  for (const char letter : from) {
    out.push_back(letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter);
  }
  return out;
}

[[nodiscard]] std::string Trimmed(std::string_view from) {
  size_t first = 0;
  size_t last = from.size();
  const auto space = [](char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '"' || c == '\'';
  };
  while (first < last && space(from[first])) { ++first; }
  while (last > first && space(from[last - 1])) { --last; }
  return std::string(from.substr(first, last - first));
}

} // namespace

Family FamilyNamed(std::string_view declared) {
  size_t at = 0;
  while (at <= declared.size()) {
    const size_t comma = declared.find(',', at);
    const std::string one = Lowered(Trimmed(declared.substr(
        at, comma == std::string_view::npos ? std::string_view::npos : comma - at)));
    for (const Named &known : kFamilies) {
      if (one == known.Spelled) { return known.Is; }
    }
    if (comma == std::string_view::npos) { break; }
    at = comma + 1;
  }
  return Family::Sans;
}

const char *FileOf(Family family) {
  return kFiles[static_cast<size_t>(family)];
}

Family FamilyOf(uint32_t declared) {
  for (const Named &known : kFamilies) {
    if (Keyword(known.Spelled) == declared) { return known.Is; }
  }
  return Family::Sans;
}

Typeface::~Typeface() {
  for (const Sized &held : Sets_) {
    if (held.Set != nullptr) { TTF_CloseFont(held.Set); }
  }
  Sets_.clear();
  if (Started_) { TTF_Quit(); }
}

bool Typeface::Opens(std::string_view fonts, std::string &error) {
  if (!Started_ && !TTF_Init()) {
    error = std::string("the text engine did not start: ") + SDL_GetError();
    return false;
  }
  Started_ = true;
  Under_ = std::string(fonts);
  if (!Under_.empty() && Under_.back() != '/') { Under_.push_back('/'); }

  for (size_t at = 0; at < static_cast<size_t>(Family::kCount); ++at) {
    if (!Faces_[at].empty()) { continue; }
    const std::string path = Under_ + kFiles[at];
    size_t many = 0;
    void *const held = SDL_LoadFile(path.c_str(), &many);
    if (held == nullptr || many == 0) {
      error = "the face '" + path + "' did not open: " + SDL_GetError();
      if (held != nullptr) { SDL_free(held); }
      return false;
    }
    Faces_[at].assign(static_cast<const uint8_t *>(held),
                      static_cast<const uint8_t *>(held) + many);
    SDL_free(held);
  }
  if (Rgba_.empty()) {
    SheetW_ = kSheetEdge;
    SheetH_ = kSheetEdge;
    Rgba_.assign(static_cast<size_t>(SheetW_) * static_cast<size_t>(SheetH_) * 4u, 0u);
    Cells_.assign(kCellSlots, Cell{});
  }
  return true;
}

TTF_Font *Typeface::Set(Family family, int sizePx) const {
  const uint64_t key = Keyed(family, sizePx, 0);
  for (const Sized &held : Sets_) {
    if (held.Key == key) { return held.Set; }
  }
  const std::vector<uint8_t> &held = Faces_[static_cast<size_t>(family)];
  if (held.empty()) { return nullptr; }
  SDL_IOStream *const from = SDL_IOFromConstMem(held.data(), held.size());
  if (from == nullptr) { return nullptr; }
  TTF_Font *const made = TTF_OpenFontIO(from, true, static_cast<float>(sizePx));
  Sets_.push_back(Sized{.Key = key, .Set = made});
  ++Opened_;
  return made;
}

bool Typeface::Packs(int widthPx, int heightPx, int &leftPx, int &topPx) const {
  if (Rgba_.empty()) { return false; }
  if (ShelfX_ + widthPx + kPad > SheetW_) {
    ShelfX_ = 0;
    ShelfY_ += ShelfTall_ + kPad;
    ShelfTall_ = 0;
  }
  if (ShelfY_ + heightPx + kPad > SheetH_) { return false; }
  leftPx = ShelfX_;
  topPx = ShelfY_;
  ShelfX_ += widthPx + kPad;
  ShelfTall_ = std::max(ShelfTall_, heightPx);
  return true;
}

const Typeface::Cell &Typeface::Cell0f(Family family, int sizePx, char32_t code) const {
  static const Cell kNotdef;
  if (Cells_.empty()) { return kNotdef; }
  const uint64_t key = Keyed(family, sizePx, code);
  size_t slot = static_cast<size_t>(key * kGoldenWord32 >> 50u) & (kCellSlots - 1u);
  for (size_t step = 0; step < kCellSlots; ++step) {
    const Cell &held = Cells_[slot];
    if (held.Held && held.Key == key) { return held; }
    if (!held.Held) { break; }
    slot = (slot + 1u) & (kCellSlots - 1u);
  }
  if (Cells_[slot].Held) {
    ++Missed_;
    return kNotdef;
  }

  Cell cut;
  cut.Key = key;
  cut.Held = true;
  TTF_Font *set = Set(family, sizePx);
  if (set == nullptr) { return Cells_[slot] = cut; }

  int minx = 0;
  int maxx = 0;
  int miny = 0;
  int maxy = 0;
  int advance = 0;
  if (!TTF_GetGlyphMetrics(set, static_cast<Uint32>(code), &minx, &maxx, &miny, &maxy, &advance)) {
    return Cells_[slot] = cut;
  }
  cut.AdvancePx = static_cast<float>(advance);

  TTF_ImageType kind = TTF_IMAGE_INVALID;
  SDL_Surface *ink = TTF_GetGlyphImage(set, static_cast<Uint32>(code), &kind);
  if (ink == nullptr || ink->w <= 0 || ink->h <= 0) {
    if (ink != nullptr) { SDL_DestroySurface(ink); }
    return Cells_[slot] = cut;
  }

  SDL_Surface *rgba =
      ink->format == SDL_PIXELFORMAT_RGBA32 ? ink : SDL_ConvertSurface(ink, SDL_PIXELFORMAT_RGBA32);
  int leftPx = 0;
  int topPx = 0;
  if (rgba != nullptr && Packs(rgba->w, rgba->h, leftPx, topPx)) {
    const auto *from = static_cast<const uint8_t *>(rgba->pixels);
    for (int row = 0; row < rgba->h; ++row) {
      uint8_t *into =
          Rgba_.data() + ((static_cast<size_t>(topPx + row) * static_cast<size_t>(SheetW_)) +
                          static_cast<size_t>(leftPx)) *
                             4u;
      SDL_memcpy(into,
                 from + static_cast<size_t>(row) * static_cast<size_t>(rgba->pitch),
                 static_cast<size_t>(rgba->w) * 4u);
    }
    cut.WidthPx = static_cast<float>(rgba->w);
    cut.HeightPx = static_cast<float>(rgba->h);
    cut.LeftPx = static_cast<float>(minx);
    cut.TopPx = static_cast<float>(TTF_GetFontAscent(set) - maxy);
    cut.U0 = static_cast<float>(leftPx) / static_cast<float>(SheetW_);
    cut.V0 = static_cast<float>(topPx) / static_cast<float>(SheetH_);
    cut.U1 = static_cast<float>(leftPx + rgba->w) / static_cast<float>(SheetW_);
    cut.V1 = static_cast<float>(topPx + rgba->h) / static_cast<float>(SheetH_);
    cut.Drawn = true;
    ++Cut_;
  }
  if (rgba != nullptr && rgba != ink) { SDL_DestroySurface(rgba); }
  SDL_DestroySurface(ink);
  ++Held_;
  return Cells_[slot] = cut;
}

FontMetrics Typeface::At(double sizePx, Family family) const {
  const int rounded = std::max(1, static_cast<int>(std::lround(sizePx)));
  const TTF_Font *set = Set(family, rounded);
  if (set == nullptr) {
    return {.Advance = sizePx * kAdvanceShare, .Ascent = sizePx * 0.8, .Descent = sizePx * 0.2};
  }
  const auto ascent = static_cast<double>(TTF_GetFontAscent(set));
  const double descent = -static_cast<double>(TTF_GetFontDescent(set));
  return {.Advance = Cell0f(family, rounded, U' ').AdvancePx, .Ascent = ascent, .Descent = descent};
}

Glyph Typeface::Shape(char32_t code, double sizePx, Family family) const {
  const int rounded = std::max(1, static_cast<int>(std::lround(sizePx)));
  const Cell &cut = Cell0f(family, rounded, code);
  const double scale = sizePx / static_cast<double>(rounded);

  Glyph glyph;
  glyph.AdvancePx = cut.AdvancePx * scale;
  if (!cut.Drawn) { return glyph; }
  glyph.LeftPx = cut.LeftPx * scale;
  glyph.TopPx = cut.TopPx * scale;
  glyph.WidthPx = cut.WidthPx * scale;
  glyph.HeightPx = cut.HeightPx * scale;
  glyph.U0 = cut.U0;
  glyph.V0 = cut.V0;
  glyph.U1 = cut.U1;
  glyph.V1 = cut.V1;
  glyph.Drawn = true;
  return glyph;
}

} // namespace outshine::Ui
