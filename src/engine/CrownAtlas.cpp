#include "CrownAtlas.h"
#include <Outshine.h>
#include <scenario/Scenario.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace outshine {
namespace {
constexpr size_t kMostAtlasTexels = 1u << 24u;

namespace Says {
constexpr auto Shape =
    "crown atlas requires 3..4096 pixels, 1..64 views and at most 16777216 texels";
constexpr auto Geometry = "crown atlas requires native tree geometry";
constexpr auto Bounds = "crown atlas requires finite nonempty bounds";
constexpr auto Readback = "crown atlas readback has incomplete attachments";
constexpr auto Surface = "crown atlas material identity is outside its source table";
} // namespace Says
} // namespace

std::optional<CrownAtlas>
CrownAtlas::Bake(const Generators::TreePrototype &tree, Shape shape, std::string &error) {
  if (shape.Pixels < 3 || shape.Pixels > 4096 || shape.Views == 0 || shape.Views > 64 ||
      static_cast<size_t>(shape.Pixels) * static_cast<size_t>(shape.Pixels) * shape.Views >
          kMostAtlasTexels) {
    error = Says::Shape;
    return std::nullopt;
  }
  auto geometry = tree.GeometryAt(0);
  if (!geometry) {
    error = Says::Geometry;
    return std::nullopt;
  }
  CrownAtlas atlas;
  atlas.Pixels_ = shape.Pixels;
  Vec3 least{{std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::infinity()}};
  Vec3 most = least * -1.0;
  for (int part = 0; part < geometry->parts(); ++part) {
    const auto positions = geometry->positionsOf(part);
    for (size_t at = 0; at < positions.size(); at += 3) {
      for (size_t axis = 0; axis < 3; ++axis) {
        least[axis] = std::min(least[axis], static_cast<double>(positions[at + axis]));
        most[axis] = std::max(most[axis], static_cast<double>(positions[at + axis]));
      }
    }
  }
  atlas.CentreM_ = (least + most) * 0.5;
  const Vec3 half = (most - least) * 0.5;
  atlas.HalfExtentM_ = std::max(std::hypot(half[0], half[2]), half[1]) *
                       static_cast<double>(shape.Pixels) / static_cast<double>(shape.Pixels - 2);
  if (!(atlas.HalfExtentM_ > 0.0) || !std::isfinite(atlas.HalfExtentM_)) {
    error = Says::Bounds;
    return std::nullopt;
  }
  for (int at = 0; at < geometry->surfaces(); ++at) {
    atlas.Surfaces_.push_back(geometry->surfaceAt(MaterialInstance(at)));
  }
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {shape.Pixels, shape.Pixels};
  scenario.Render.Outputs = {"sceneDepth", "sceneShadingNormal", "sceneSurfaceIdentity"};
  scenario.Lit.Declared = true;
  scenario.Lit.Key.Lux = 20000;
  scenario.Lit.Key.BearingDeg = 135;
  scenario.Lit.Key.ElevationDeg = 40;
  for (unsigned at = 0; at < shape.Views; ++at) {
    const double angle = 2.0 * std::numbers::pi * at / shape.Views;
    const Vec3 direction{{std::sin(angle), 0, std::cos(angle)}};
    atlas.Views_.push_back({.TowardEye = direction});
    Scenario::View view;
    view.Id = std::to_string(at);
    view.Person = "first";
    view.Sees.Placed = true;
    view.Sees.Stands.AtM = atlas.CentreM_ + direction * (3.0 * atlas.HalfExtentM_);
    view.Sees.LooksAt = true;
    view.Sees.LookAtM = atlas.CentreM_;
    view.Sees.setProjection(Scenario::Camera::Ortho{.XMagM = atlas.HalfExtentM_,
                                                    .YMagM = atlas.HalfExtentM_,
                                                    .NearM = atlas.HalfExtentM_,
                                                    .FarM = 5.0 * atlas.HalfExtentM_});
    scenario.Views.push_back(view);
  }
  std::vector<float> depth, normal, identity;
  const size_t count = static_cast<size_t>(shape.Pixels) * static_cast<size_t>(shape.Pixels);
  for (unsigned at = 0; at < shape.Views; ++at) {
    Engine engine;
    if (!engine.drawsInto({shape.Pixels, shape.Pixels}) || !engine.declare(scenario) ||
        !engine.setView(std::to_string(at)) || !engine.setGeometry(*geometry) ||
        !engine.assemble() || !engine.advance()) {
      error = engine.error();
      return std::nullopt;
    }
    if (!engine.renderer().render({}) || !engine.renderer().readPixels(Buffer::Depth, depth) ||
        !engine.renderer().readPixels(Buffer::ShadingNormal, normal) ||
        !engine.renderer().readPixels(Buffer::SurfaceIdentity, identity)) {
      error = engine.error();
      return std::nullopt;
    }
    if (depth.size() != count || normal.size() != count * 4 || identity.size() != count * 4) {
      error = Says::Readback;
      return std::nullopt;
    }
    auto &texels = atlas.Views_[at].Texels;
    texels.resize(count);
    for (size_t pixel = 0; pixel < count; ++pixel) {
      const float surface = identity[pixel * 4];
      if (!std::isfinite(surface) || surface < 0 || surface > atlas.Surfaces_.size() ||
          surface != std::floor(surface)) {
        error = Says::Surface;
        return std::nullopt;
      }
      texels[pixel] = {
          .Normal = {{normal[pixel * 4], normal[pixel * 4 + 1], normal[pixel * 4 + 2]}},
          .Depth = depth[pixel],
          .Surface = static_cast<uint32_t>(surface)};
    }
  }
  return atlas;
}

} // namespace outshine
