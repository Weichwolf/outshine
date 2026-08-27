#ifndef OUTSHINE_ENGINE_OVERLAY_H
#define OUTSHINE_ENGINE_OVERLAY_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Layout.h"
#include "Markup.h"
#include "Paint.h"
#include "Pointer.h"
#include "Renderer.h"
#include "Style.h"

namespace outshine::Core {

struct Shows {
  std::string Markup;

  std::string Style;
  std::string Programme;
  double LeftFrac = 0.0, TopFrac = 0.0, WidthFrac = 1.0, HeightFrac = 1.0;
};

class Overlay {
public:
  void Faces(const Ui::Font *font) { Font_ = font; }
  [[nodiscard]] const Ui::Font *Face() const { return Font_; }

  [[nodiscard]] bool Compose(Render::Renderer &renderer, std::span<const Shows> surfaces,
                             double surfaceWidthPx, double surfaceHeightPx, std::string &error);

  void Wheeled(double xPx, double yPx, double byPx, bool &again);
  [[nodiscard]] Ui::Touched Under(double xPx, double yPx, size_t &surface) const;

  [[nodiscard]] const std::vector<std::vector<Ui::Layout::Scrolled>> &Scrolled() const {
    return Scrolled_;
  }
  void Scrolled(std::vector<std::vector<Ui::Layout::Scrolled>> kept) { Scrolled_ = std::move(kept); }

private:
  struct Laid {
    Ui::Markup Tree;
    Ui::Stylesheet Sheet;
    Ui::Layout Placed;
    Ui::Painting Painted;
    double LeftPx = 0.0, TopPx = 0.0;
  };

  const Ui::Font *Font_ = nullptr;
  uint64_t Cut_ = 0;
  std::vector<Laid> Laid_;
  std::vector<std::vector<Ui::Layout::Scrolled>> Scrolled_;
  std::vector<Render::OverlayQuad> Quads_;
};

}
#endif
