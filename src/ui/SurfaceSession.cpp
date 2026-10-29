#include "SurfaceSession.h"
#include "PaintList.h"
#include "Style.h"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <span>
#include <cstddef>
#include <string>
#include <vector>
#include <utility>

namespace outshine::Ui {

constexpr unsigned kRedShift = 24u;
constexpr unsigned kGreenShift = 16u;
constexpr unsigned kBlueShift = 8u;
constexpr float kByteSteps = 255.0f;

constexpr uint32_t kByteMask = 0xFFu;

namespace {

void AppendPaintQuads(const std::vector<Quad> &from,
                      double offsetX,
                      double offsetY,
                      std::vector<PaintQuad> &out) {
  out.reserve(out.size() + from.size());
  for (const Quad &quad : from) {
    PaintQuad to;
    to.LeftPx = static_cast<float>(quad.X + offsetX);
    to.TopPx = static_cast<float>(quad.Y + offsetY);
    to.WidthPx = static_cast<float>(quad.Width);
    to.HeightPx = static_cast<float>(quad.Height);
    to.U0 = static_cast<float>(quad.U0);
    to.V0 = static_cast<float>(quad.V0);
    to.U1 = static_cast<float>(quad.U1);
    to.V1 = static_cast<float>(quad.V1);
    to.Red = static_cast<float>((quad.Colour >> kRedShift) & kByteMask) / kByteSteps;
    to.Green = static_cast<float>((quad.Colour >> kGreenShift) & kByteMask) / kByteSteps;
    to.Blue = static_cast<float>((quad.Colour >> kBlueShift) & kByteMask) / kByteSteps;
    to.Alpha = static_cast<float>(quad.Colour & kByteMask) / kByteSteps;
    to.ClipLeftPx = static_cast<float>(quad.ClipX + offsetX);
    to.ClipTopPx = static_cast<float>(quad.ClipY + offsetY);
    to.ClipWidthPx = static_cast<float>(quad.ClipWidth);
    to.ClipHeightPx = static_cast<float>(quad.ClipHeight);
    to.RadiusPx = static_cast<float>(quad.Radius);
    to.Opacity = static_cast<float>(quad.Opacity);
    out.push_back(to);
  }
}

}

void SurfaceSession::Configure(PaintPublisher publisher,
                               const Font *font,
                               std::vector<SurfaceDeclaration> surfaces,
                               SurfacePx over) {
  Publisher_ = std::move(publisher);
  Font_ = font;
  Surface_ = over;
  Surfaces_ = std::move(surfaces);
}

bool SurfaceSession::Compose(std::string &error) {
  return Compose(Surfaces_, error);
}

bool SurfaceSession::Compose(std::span<const SurfaceDeclaration> surfaces, std::string &error) {
  const double surfaceWidthPx = Surface_.WidthPx;
  const double surfaceHeightPx = Surface_.HeightPx;
  std::vector<Laid> candidate(surfaces.size());
  std::vector<PaintQuad> quads;
  auto scrolled = Scrolled_;
  scrolled.resize(surfaces.size());
  for (size_t at = 0; at < surfaces.size(); ++at) {
    const SurfaceDeclaration &declared = surfaces[at];
    Laid &laid = candidate[at];
    const double widthPx = declared.WidthFrac * surfaceWidthPx;
    const double heightPx = declared.HeightFrac * surfaceHeightPx;
    if (widthPx <= 0.0 || heightPx <= 0.0) { continue; }
    if (Font_ == nullptr) {
      error =
          "surface " + std::to_string(at) + " is declared and no face was handed over to set it in";
      return false;
    }
    laid.LeftPx = declared.LeftFrac * surfaceWidthPx;
    laid.TopPx = declared.TopFrac * surfaceHeightPx;
    if (!laid.Tree.Read(declared.Markup, error)) { return false; }
    Stylesheet sheet;
    sheet.Read(UserAgentSheet());
    if (!declared.Style.empty()) { sheet.Read(declared.Style); }
    sheet.Read(laid.Tree.StyleText());
    if (!laid.Placed.Build(laid.Tree,
                           sheet,
                           widthPx,
                           heightPx,
                           *Font_,
                           std::span<const Layout::Scrolled>(scrolled[at]),
                           error)) {
      return false;
    }
    PaintList painted;
    if (!painted.Build(laid.Placed, *Font_, error)) { return false; }
    AppendPaintQuads(painted.Quads(), laid.LeftPx, laid.TopPx, quads);
  }
  const bool changed = Font_ != nullptr && Font_->Cut() != Cut_;
  PaintAtlasView atlas;
  if (changed) {
    atlas = {
        .Rgba = Font_->Sheet(), .Width = Font_->SheetWidthPx(), .Height = Font_->SheetHeightPx()};
  }
  if (!Publisher_) {
    error = "UI paint publisher is not configured";
    return false;
  }
  if (!Publisher_(quads, changed ? &atlas : nullptr, error)) { return false; }
  Laid_ = std::move(candidate);
  Scrolled_ = std::move(scrolled);
  if (changed) { Cut_ = Font_->Cut(); }
  return true;
}

bool SurfaceSession::Redeclare(std::vector<SurfaceDeclaration> surfaces, std::string &error) {
  if (!Compose(surfaces, error)) { return false; }
  Surfaces_ = std::move(surfaces);
  return true;
}

std::expected<bool, std::string> SurfaceSession::Wheel(double xPx, double yPx, double byPx) {
  auto previous = Scrolled_;
  bool changed = false;
  ApplyWheel(xPx, yPx, byPx, changed);
  if (!changed) { return false; }
  std::string error;
  if (Compose(error)) { return true; }
  Scrolled_ = std::move(previous);
  return std::unexpected(std::move(error));
}

bool SurfaceSession::RestoreScroll(std::vector<std::vector<Layout::Scrolled>> kept,
                                   std::string &error) {
  auto previous = std::move(Scrolled_);
  Scrolled_ = std::move(kept);
  if (Compose(error)) { return true; }
  Scrolled_ = std::move(previous);
  return false;
}

void SurfaceSession::ApplyWheel(double xPx, double yPx, double byPx, bool &changed) {
  Scrolled_.resize(Laid_.size());
  for (size_t at = Laid_.size(); at > 0; --at) {
    const Laid &laid = Laid_[at - 1];
    const double x = xPx - laid.LeftPx;
    const double y = yPx - laid.TopPx;
    const int scroller = laid.Placed.Scroller(x, y);
    if (scroller < 0) { continue; }
    const double most = laid.Placed.ScrollableBy(scroller);
    std::vector<Layout::Scrolled> &kept = Scrolled_[at - 1];
    double *held = nullptr;
    for (Layout::Scrolled &one : kept) {
      if (one.Node == scroller) { held = &one.Px; }
    }
    if (held == nullptr) {
      kept.push_back(Layout::Scrolled{.Node = scroller, .Px = 0.0});
      held = &kept.back().Px;
    }
    const double was = *held;
    *held = std::clamp(*held + byPx, 0.0, most);
    changed = *held != was;
    return;
  }
}

Touched SurfaceSession::Under(double xPx, double yPx, size_t &surface) const {
  for (size_t at = Laid_.size(); at > 0; --at) {
    const Laid &laid = Laid_[at - 1];
    Touched found = Ui::Under(laid.Placed, laid.Tree, xPx - laid.LeftPx, yPx - laid.TopPx);
    if (found.Node >= 0) {
      surface = at - 1;
      return found;
    }
  }
  return Touched{};
}

const std::string &SurfaceSession::ProgrammeOf(size_t surface) const {
  static const std::string none;
  return surface < Surfaces_.size() ? Surfaces_[surface].Programme : none;
}

}
