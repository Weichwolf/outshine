#include "Typeface.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "Style.h"

#include <algorithm>
#include <cmath>

namespace outshine::Ui {
namespace {

constexpr int kSheetEdge = 1024;
constexpr int kSheetTallest = 4096;
constexpr int kPad = 1;

struct Named {
  const char *Spelled;
  Family Is;
};

constexpr Named kFamilies[] = {
    {"serif", Family::Serif},         {"times", Family::Serif},
    {"times new roman", Family::Serif}, {"georgia", Family::Serif},
    {"garamond", Family::Serif},      {"palatino", Family::Serif},
    {"book antiqua", Family::Serif},  {"ui-serif", Family::Serif},

    {"monospace", Family::Mono},      {"courier", Family::Mono},
    {"courier new", Family::Mono},    {"consolas", Family::Mono},
    {"menlo", Family::Mono},          {"monaco", Family::Mono},
    {"sf mono", Family::Mono},        {"dejavu sans mono", Family::Mono},
    {"liberation mono", Family::Mono}, {"ui-monospace", Family::Mono},

    {"sans-serif", Family::Sans},     {"arial", Family::Sans},
    {"helvetica", Family::Sans},      {"helvetica neue", Family::Sans},
    {"verdana", Family::Sans},        {"tahoma", Family::Sans},
    {"segoe ui", Family::Sans},       {"system-ui", Family::Sans},
    {"ui-sans-serif", Family::Sans},  {"roboto", Family::Sans},
    {"inter", Family::Sans},          {"dejavu sans", Family::Sans},
};

constexpr const char *kFiles[] = {"DejaVuSans.ttf", "DejaVuSerif.ttf", "DejaVuSansMono.ttf"};
static_assert(sizeof(kFiles) / sizeof(kFiles[0]) == (size_t)Family::kCount,
              "every family the catalogue offers names the file it is set in");

[[nodiscard]] uint64_t Keyed(Family family, int sizePx, char32_t code) {
  return ((uint64_t)family << 56) | ((uint64_t)(uint32_t)sizePx << 32) | (uint64_t)code;
}

[[nodiscard]] std::string Lowered(std::string_view from) {
  std::string out;
  out.reserve(from.size());
  for (const char letter : from) {
    out.push_back(letter >= 'A' && letter <= 'Z' ? (char)(letter - 'A' + 'a') : letter);
  }
  return out;
}

[[nodiscard]] std::string Trimmed(std::string_view from) {
  size_t first = 0, last = from.size();
  const auto space = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '"' || c == '\''; };
  while (first < last && space(from[first])) { ++first; }
  while (last > first && space(from[last - 1])) { --last; }
  return std::string(from.substr(first, last - first));
}

}

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

const char *FileOf(Family family) { return kFiles[(size_t)family]; }

Family FamilyOf(uint32_t declared) {
  for (const Named &known : kFamilies) {
    if (Keyword(known.Spelled) == declared) { return known.Is; }
  }
  return Family::Sans;
}

Typeface::~Typeface(void) {
  for (const auto &held : Sets_) { TTF_CloseFont(held.second); }
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

  for (size_t at = 0; at < (size_t)Family::kCount; ++at) {
    const std::string path = Under_ + kFiles[at];
    TTF_Font *probe = TTF_OpenFont(path.c_str(), 16.0f);
    if (probe == nullptr) {
      error = "the face '" + path + "' did not open: " + SDL_GetError();
      return false;
    }
    TTF_CloseFont(probe);
  }
  return true;
}

TTF_Font *Typeface::Set(Family family, int sizePx) const {
  const uint64_t key = Keyed(family, sizePx, 0);
  const auto held = Sets_.find(key);
  if (held != Sets_.end()) { return held->second; }

  const std::string path = Under_ + kFiles[(size_t)family];
  TTF_Font *set = TTF_OpenFont(path.c_str(), (float)sizePx);
  Sets_.emplace(key, set);
  return set;
}

bool Typeface::Packs(int widthPx, int heightPx, int &leftPx, int &topPx) const {
  if (Rgba_.empty()) {
    SheetW_ = kSheetEdge;
    SheetH_ = kSheetEdge;
    Rgba_.assign((size_t)SheetW_ * (size_t)SheetH_ * 4u, 0u);
  }
  if (ShelfX_ + widthPx + kPad > SheetW_) {
    ShelfX_ = 0;
    ShelfY_ += ShelfTall_ + kPad;
    ShelfTall_ = 0;
  }
  if (ShelfY_ + heightPx + kPad > SheetH_) {
    if (SheetH_ >= kSheetTallest) { return false; }
    const int taller = std::min(SheetH_ * 2, kSheetTallest);
    Rgba_.resize((size_t)SheetW_ * (size_t)taller * 4u, 0u);
    SheetH_ = taller;
    if (ShelfY_ + heightPx + kPad > SheetH_) { return false; }
  }
  leftPx = ShelfX_;
  topPx = ShelfY_;
  ShelfX_ += widthPx + kPad;
  ShelfTall_ = std::max(ShelfTall_, heightPx);
  return true;
}

const Typeface::Cell &Typeface::Cell0f(Family family, int sizePx, char32_t code) const {
  const uint64_t key = Keyed(family, sizePx, code);
  const auto held = Cells_.find(key);
  if (held != Cells_.end()) { return held->second; }

  Cell cut;
  TTF_Font *set = Set(family, sizePx);
  if (set == nullptr) { return Cells_.emplace(key, cut).first->second; }

  int minx = 0, maxx = 0, miny = 0, maxy = 0, advance = 0;
  if (!TTF_GetGlyphMetrics(set, (Uint32)code, &minx, &maxx, &miny, &maxy, &advance)) {
    return Cells_.emplace(key, cut).first->second;
  }
  cut.AdvancePx = (float)advance;

  TTF_ImageType kind = TTF_IMAGE_INVALID;
  SDL_Surface *ink = TTF_GetGlyphImage(set, (Uint32)code, &kind);
  if (ink == nullptr || ink->w <= 0 || ink->h <= 0) {
    if (ink != nullptr) { SDL_DestroySurface(ink); }
    return Cells_.emplace(key, cut).first->second;
  }

  SDL_Surface *rgba = ink->format == SDL_PIXELFORMAT_RGBA32
                          ? ink
                          : SDL_ConvertSurface(ink, SDL_PIXELFORMAT_RGBA32);
  int leftPx = 0, topPx = 0;
  if (rgba != nullptr && Packs(rgba->w, rgba->h, leftPx, topPx)) {
    const uint8_t *from = (const uint8_t *)rgba->pixels;
    for (int row = 0; row < rgba->h; ++row) {
      uint8_t *into = Rgba_.data() + (((size_t)(topPx + row) * (size_t)SheetW_) + (size_t)leftPx) * 4u;
      SDL_memcpy(into, from + (size_t)row * (size_t)rgba->pitch, (size_t)rgba->w * 4u);
    }
    cut.WidthPx = (float)rgba->w;
    cut.HeightPx = (float)rgba->h;
    cut.LeftPx = (float)minx;
    cut.TopPx = (float)(TTF_GetFontAscent(set) - maxy);
    cut.U0 = (float)leftPx / (float)SheetW_;
    cut.V0 = (float)topPx / (float)SheetH_;
    cut.U1 = (float)(leftPx + rgba->w) / (float)SheetW_;
    cut.V1 = (float)(topPx + rgba->h) / (float)SheetH_;
    cut.Drawn = true;
    ++Cut_;
  }
  if (rgba != nullptr && rgba != ink) { SDL_DestroySurface(rgba); }
  SDL_DestroySurface(ink);
  return Cells_.emplace(key, cut).first->second;
}

FontMetrics Typeface::At(double sizePx, Family family) const {
  const int rounded = std::max(1, (int)std::lround(sizePx));
  TTF_Font *set = Set(family, rounded);
  if (set == nullptr) { return {sizePx * 0.5, sizePx * 0.8, sizePx * 0.2}; }
  const double ascent = (double)TTF_GetFontAscent(set);
  const double descent = -(double)TTF_GetFontDescent(set);
  return {Cell0f(family, rounded, U' ').AdvancePx, ascent, descent};
}

Glyph Typeface::Shape(char32_t code, double sizePx, Family family) const {
  const int rounded = std::max(1, (int)std::lround(sizePx));
  const Cell &cut = Cell0f(family, rounded, code);
  const double scale = sizePx / (double)rounded;

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

}
