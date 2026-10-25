#ifndef OUTSHINE_ENGINE_BUILDINGPAGEADAPTER_H
#define OUTSHINE_ENGINE_BUILDINGPAGEADAPTER_H
#include "GeometryPageView.h"
#include "StructureBake.h"
#include <array>
#include <optional>

namespace outshine {
struct BuildingPageMaterials {
  Render::PieceSurface Walls{1};
  Render::PieceSurface Roofs{2};
  std::optional<Render::PieceSurface> MetreWalls = std::nullopt;
};

class BuildingPageView {
public:
  BuildingPageView(const Generators::BakedTile &baked, BuildingPageMaterials materials);
  [[nodiscard]] Render::GeometryPageView View() const noexcept;

private:
  std::array<Render::GeometryPagePartView, 2> Parts_;
  Render::GeometryPageView Page_;
};
}
#endif
