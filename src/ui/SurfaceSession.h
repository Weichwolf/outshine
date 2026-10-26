#ifndef OUTSHINE_UI_SURFACESESSION_H
#define OUTSHINE_UI_SURFACESESSION_H

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "Layout.h"
#include "Markup.h"
#include "Pointer.h"
#include "UiPaint.h"
#include <functional>

namespace outshine::Ui {

struct SurfaceDeclaration {
  std::string Markup;

  std::string Style;
  std::string Programme;
  double LeftFrac = 0.0, TopFrac = 0.0, WidthFrac = 1.0, HeightFrac = 1.0;
};

using PaintPublisher =
    std::function<bool(std::span<const PaintQuad>, const PaintAtlasView *, std::string &)>;

class SurfaceSession {
public:
  struct SurfacePx {
    double WidthPx = 0.0;
    double HeightPx = 0.0;
  };

  void Configure(PaintPublisher publisher,
                 const Font *font,
                 std::vector<SurfaceDeclaration> surfaces,
                 SurfacePx over);

  [[nodiscard]] bool Compose(std::string &error);
  [[nodiscard]] bool Redeclare(std::vector<SurfaceDeclaration> surfaces, std::string &error);
  [[nodiscard]] std::expected<bool, std::string> Wheel(double xPx, double yPx, double byPx);
  [[nodiscard]] bool RestoreScroll(std::vector<std::vector<Layout::Scrolled>> kept,
                                   std::string &error);

  [[nodiscard]] Touched Under(double xPx, double yPx, size_t &surface) const;
  [[nodiscard]] const std::string &ProgrammeOf(size_t surface) const;

  [[nodiscard]] const std::vector<std::vector<Layout::Scrolled>> &ScrollState() const {
    return Scrolled_;
  }

  [[nodiscard]] const std::vector<SurfaceDeclaration> &Surfaces() const { return Surfaces_; }

private:
  [[nodiscard]] bool Compose(std::span<const SurfaceDeclaration> surfaces, std::string &error);
  void ApplyWheel(double xPx, double yPx, double byPx, bool &changed);

  struct Laid {
    Markup Tree;
    Layout Placed;
    double LeftPx = 0.0, TopPx = 0.0;
  };

  PaintPublisher Publisher_;
  const Font *Font_ = nullptr;
  SurfacePx Surface_;
  std::vector<SurfaceDeclaration> Surfaces_;
  uint64_t Cut_ = 0;
  std::vector<Laid> Laid_;
  std::vector<std::vector<Layout::Scrolled>> Scrolled_;
};

}
#endif
