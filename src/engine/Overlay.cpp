#include "Overlay.h"

#include <algorithm>
#include <string>

namespace outshine::Core {

namespace {

void AsOverlay(const std::vector<Ui::Quad> &from,
               double offsetX,
               double offsetY,
               std::vector<Render::OverlayQuad> &out) {
  out.reserve(out.size() + from.size());
  for (const Ui::Quad &quad : from) {
    Render::OverlayQuad to;
    to.LeftPx = static_cast<float>(quad.X + offsetX);
    to.TopPx = static_cast<float>(quad.Y + offsetY);
    to.WidthPx = static_cast<float>(quad.Width);
    to.HeightPx = static_cast<float>(quad.Height);
    to.U0 = static_cast<float>(quad.U0);
    to.V0 = static_cast<float>(quad.V0);
    to.U1 = static_cast<float>(quad.U1);
    to.V1 = static_cast<float>(quad.V1);
    to.Red = static_cast<float>((quad.Colour >> 24) & 0xFFu) / 255.0f;
    to.Green = static_cast<float>((quad.Colour >> 16) & 0xFFu) / 255.0f;
    to.Blue = static_cast<float>((quad.Colour >> 8) & 0xFFu) / 255.0f;
    to.Alpha = static_cast<float>(quad.Colour & 0xFFu) / 255.0f;
    to.ClipLeftPx = static_cast<float>(quad.ClipX + offsetX);
    to.ClipTopPx = static_cast<float>(quad.ClipY + offsetY);
    to.ClipWidthPx = static_cast<float>(quad.ClipWidth);
    to.ClipHeightPx = static_cast<float>(quad.ClipHeight);
    to.RadiusPx = static_cast<float>(quad.Radius);
    to.Opacity = static_cast<float>(quad.Opacity);
    out.push_back(to);
  }
}

} // namespace

bool Overlay::Compose(Render::SceneRenderer &renderer,
                      std::span<const Shows> surfaces,
                      double surfaceWidthPx,
                      double surfaceHeightPx,
                      std::string &error) {
  Laid_.clear();
  Quads_.clear();
  Laid_.resize(surfaces.size());
  Scrolled_.resize(surfaces.size());
  for (size_t at = 0; at < surfaces.size(); ++at) {
    const Shows &declared = surfaces[at];
    Laid &laid = Laid_[at];
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
    laid.Sheet.Read(Ui::UserAgentSheet());
    if (!declared.Style.empty()) { laid.Sheet.Read(declared.Style); }
    laid.Sheet.Read(laid.Tree.StyleText());
    if (!laid.Placed.Build(laid.Tree,
                           laid.Sheet,
                           widthPx,
                           heightPx,
                           *Font_,
                           std::span<const Ui::Layout::Scrolled>(Scrolled_[at]),
                           error)) {
      return false;
    }
    if (!laid.Painted.Build(laid.Placed, *Font_, error)) { return false; }
    AsOverlay(laid.Painted.Quads(), laid.LeftPx, laid.TopPx, Quads_);
  }
  if (Font_ != nullptr && Font_->Cut() != Cut_) {
    Cut_ = Font_->Cut();
    if (!renderer.SetOverlayAtlas(
            Font_->Sheet(), Font_->SheetWidthPx(), Font_->SheetHeightPx(), error)) {
      return false;
    }
  }
  return renderer.SetOverlay(Quads_.data(), Quads_.size(), error);
}

void Overlay::Wheeled(double xPx, double yPx, double byPx, bool &again) {
  Scrolled_.resize(Laid_.size());
  for (size_t at = Laid_.size(); at > 0; --at) {
    Laid &laid = Laid_[at - 1];
    const double x = xPx - laid.LeftPx, y = yPx - laid.TopPx;
    const int scroller = laid.Placed.Scroller(x, y);
    if (scroller < 0) { continue; }
    const double most = laid.Placed.ScrollableBy(scroller);
    std::vector<Ui::Layout::Scrolled> &kept = Scrolled_[at - 1];
    double *held = nullptr;
    for (Ui::Layout::Scrolled &one : kept) {
      if (one.Node == scroller) { held = &one.Px; }
    }
    if (held == nullptr) {
      kept.push_back(Ui::Layout::Scrolled{.Node = scroller, .Px = 0.0});
      held = &kept.back().Px;
    }
    const double was = *held;
    *held = std::clamp(*held + byPx, 0.0, most);
    again = *held != was;
    return;
  }
}

Ui::Touched Overlay::Under(double xPx, double yPx, size_t &surface) const {
  for (size_t at = Laid_.size(); at > 0; --at) {
    const Laid &laid = Laid_[at - 1];
    Ui::Touched found = Ui::Under(laid.Placed, laid.Tree, xPx - laid.LeftPx, yPx - laid.TopPx);
    if (found.Node >= 0) {
      surface = at - 1;
      return found;
    }
  }
  return Ui::Touched{};
}

} // namespace outshine::Core
