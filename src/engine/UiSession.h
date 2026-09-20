#ifndef OUTSHINE_ENGINE_UISESSION_H
#define OUTSHINE_ENGINE_UISESSION_H

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "Layout.h"
#include "Markup.h"
#include "Paint.h"
#include "Pointer.h"
#include "SceneRenderer.h"
#include "Style.h"

namespace outshine::Core {

struct UiSurface {
  std::string Markup;

  std::string Style;
  std::string Programme;
  double LeftFrac = 0.0, TopFrac = 0.0, WidthFrac = 1.0, HeightFrac = 1.0;
};

class UiSession {
public:
  struct SurfacePx {
    double WidthPx = 0.0;
    double HeightPx = 0.0;
  };

  void Configure(Render::SceneRenderer &renderer,
                 const Ui::Font *font,
                 std::vector<UiSurface> surfaces,
                 SurfacePx over);

  [[nodiscard]] bool Compose(std::string &error);
  [[nodiscard]] bool Redeclare(std::vector<UiSurface> surfaces, std::string &error);
  [[nodiscard]] std::expected<bool, std::string> Wheel(double xPx, double yPx, double byPx);
  [[nodiscard]] bool RestoreScroll(std::vector<std::vector<Ui::Layout::Scrolled>> kept,
                                   std::string &error);

  [[nodiscard]] Ui::Touched Under(double xPx, double yPx, size_t &surface) const;
  [[nodiscard]] const std::string &ProgrammeOf(size_t surface) const;

  [[nodiscard]] const std::vector<std::vector<Ui::Layout::Scrolled>> &ScrollState() const {
    return Scrolled_;
  }

  [[nodiscard]] const std::vector<UiSurface> &Surfaces() const { return Surfaces_; }

private:
  [[nodiscard]] bool Compose(std::span<const UiSurface> surfaces, std::string &error);
  void ApplyWheel(double xPx, double yPx, double byPx, bool &changed);

  struct Laid {
    Ui::Markup Tree;
    Ui::Stylesheet Sheet;
    Ui::Layout Placed;
    Ui::Painting Painted;
    double LeftPx = 0.0, TopPx = 0.0;
  };

  Render::SceneRenderer *Renderer_ = nullptr;
  const Ui::Font *Font_ = nullptr;
  SurfacePx Surface_;
  std::vector<UiSurface> Surfaces_;
  uint64_t Cut_ = 0;
  std::vector<Laid> Laid_;
  std::vector<std::vector<Ui::Layout::Scrolled>> Scrolled_;
  std::vector<Render::OverlayQuad> Quads_;
};

}
#endif
