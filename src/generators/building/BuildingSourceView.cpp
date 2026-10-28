#include "BuildingSourceView.h"
#include "BuildingMeshConversion.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <ratio>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <limits>
#include <optional>
#include <unordered_set>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
Vec3 BuildingCaptureFrame::Local(const Vec3 &point) const noexcept {
  const Vec3 relative = point - Origin;
  return {{Dot(relative, Axes.East), Dot(relative, Axes.Up), -Dot(relative, Axes.North)}};
}

Vec3 BuildingCaptureFrame::SourceDirection(const Vec3 &direction) const noexcept {
  return Axes.East * direction[0] + Axes.Up * direction[1] - Axes.North * direction[2];
}

bool BuildingCaptureFrame::Valid() const noexcept {
  const auto finite = [](const Vec3 &v) {
    return std::ranges::all_of(v, [](double x) { return std::isfinite(x); });
  };
  const auto &axes = Axes;
  constexpr double tolerance = 1e-10;
  return finite(Origin) && finite(axes.East) && finite(axes.North) && finite(axes.Up) &&
         std::abs(Dot(axes.East, axes.East) - 1) <= tolerance &&
         std::abs(Dot(axes.North, axes.North) - 1) <= tolerance &&
         std::abs(Dot(axes.Up, axes.Up) - 1) <= tolerance &&
         std::abs(Dot(axes.East, axes.North)) <= tolerance &&
         std::abs(Dot(axes.East, axes.Up)) <= tolerance &&
         std::abs(Dot(axes.North, axes.Up)) <= tolerance &&
         Dot(Cross(axes.East, axes.North), axes.Up) >= 1 - tolerance;
}

namespace {
std::expected<Geometry, std::string> GeometryOf(const Raised &raised,
                                                const BuildingCaptureFrame &frame,
                                                const std::array<Material, 3> &materials) {
  Geometry geometry;
  for (const auto &material : materials) {
    if (!geometry.addSurface("original-source", material)) {
      return std::unexpected("invalid building source material");
    }
  }
  const std::array parts{BuildingMeshPart{.Vertices = raised.WallCorners,
                                          .Indices = raised.WallRun,
                                          .Colours = raised.WallColours,
                                          .MaterialM = raised.WallMaterialM},
                         BuildingMeshPart{.Vertices = raised.RoofCorners,
                                          .Indices = raised.RoofRun,
                                          .Colours = {},
                                          .MaterialM = {}}};
  for (size_t at = 0; at < parts.size(); ++at) {
    if (parts[at].Vertices.empty()) { continue; }
    const int material = at == 0 && !parts[at].MaterialM.empty() ? 2 : static_cast<int>(at);
    auto mesh = ConvertBuildingMesh(parts[at], frame.Axes, frame.Origin);
    if (!mesh) { return std::unexpected(std::string(mesh.error())); }
    const auto part = geometry.addPart("source-plane", MaterialInstance(material));
    if (!part || !geometry.adoptMesh(*part, *mesh)) {
      return std::unexpected("invalid building source plane geometry");
    }
  }
  return geometry;
}
}

BuildingSourceView::BuildingSourceView(std::shared_ptr<const PreparedStructureTile> base,
                                       BuildingCaptureFrame frame,
                                       BuildingPlaneCapture producer,
                                       Box bounds,
                                       std::vector<uint32_t> sources,
                                       std::vector<uint32_t> unsupported)
    : Base_(std::move(base)),
      Frame_(frame),
      Producer_(std::move(producer)),
      Bounds_(bounds),
      Sources_(std::move(sources)),
      Unsupported_(std::move(unsupported)) {}

std::expected<BuildingSourceView, BuildingSourceView::Error>
BuildingSourceView::Open(std::shared_ptr<const PreparedStructureTile> base,
                         std::span<const uint32_t> cells,
                         BuildingCaptureFrame frame) {
  if (!base || base->Structures.size() != base->Surfaces.size() || cells.empty() ||
      base->Surfaces.size() > std::numeric_limits<uint32_t>::max() || !frame.Valid()) {
    return std::unexpected(Error{.Message = "invalid building source view inputs"});
  }
  const std::unordered_set<uint32_t> selected(cells.begin(), cells.end());
  std::vector<uint32_t> ordinals;
  std::vector<uint32_t> unsupported;
  std::vector<const BuildingSurface *> sources;
  Box bounds;
  for (size_t at = 0; at < base->Surfaces.size(); ++at) {
    if (!selected.contains(base->Structures[at].Layout.Cell.Index)) { continue; }
    const auto &source = base->Surfaces[at];
    if (!source.SupportsProjection() || source.FaceCount() == 0 || source.Bounds().Empty()) {
      unsupported.push_back(static_cast<uint32_t>(at));
      continue;
    }
    sources.push_back(&source);
    ordinals.push_back(static_cast<uint32_t>(at));
    for (unsigned corner = 0; corner < 8; ++corner) {
      bounds.Cover(frame.Local(source.Bounds().Corner(corner)));
    }
  }
  auto producer = BuildingPlaneCapture::Prepare(sources);
  if (!producer) {
    return std::unexpected(
        Error{.Kind = producer.error().Kind, .Message = "building source hierarchy unavailable"});
  }
  return BuildingSourceView(std::move(base),
                            frame,
                            std::move(*producer),
                            bounds,
                            std::move(ordinals),
                            std::move(unsupported));
}

std::expected<BuildingSourceView::Product, BuildingSourceView::Error>
BuildingSourceView::Capture(BuildingPlaneCapture::View localView,
                            BuildingPlaneCapture::Budget budget,
                            const std::array<Material, 3> &wallRoofMetreMaterials,
                            const std::stop_token &stop) const {
  const auto began = std::chrono::steady_clock::now();
  auto screen = localView.Frame ? localView.Frame : ImpostorViewFrame::From(localView.TowardEye);
  if (!screen) { return std::unexpected(Error{.Message = "invalid building source direction"}); }
  localView.Centre = Frame_.Origin + Frame_.SourceDirection(localView.Centre);
  localView.TowardEye = Frame_.SourceDirection(localView.TowardEye);
  localView.Frame = ImpostorViewFrame{.Right = Frame_.SourceDirection(screen->Right),
                                      .Up = Frame_.SourceDirection(screen->Up)};
  auto sampled = Producer_.Capture(localView, budget, stop);
  if (!sampled) {
    const auto error = sampled.error();
    const auto source = error.Source && *error.Source < Sources_.size()
                            ? std::optional(Sources_[*error.Source])
                            : std::nullopt;
    return std::unexpected(
        Error{.Kind = error.Kind, .Source = source, .Message = "building source capture failed"});
  }
  if (stop.stop_requested()) {
    return std::unexpected(Error{.Kind = BuildingPlaneCapture::ErrorKind::Cancelled,
                                 .Message = "building source capture cancelled"});
  }
  const auto conversion = std::chrono::steady_clock::now();
  auto mesh = GeometryOf(sampled->Mesh, Frame_, wallRoofMetreMaterials);
  if (!mesh) { return std::unexpected(Error{.Message = std::move(mesh.error())}); }
  return Product{
      .Mesh = std::move(*mesh),
      .Rays = sampled->Rays,
      .Covered = sampled->Covered,
      .SourceTests = sampled->SourceTests,
      .SamplingMs = std::chrono::duration<double, std::milli>(conversion - began).count(),
      .ConversionMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - conversion)
              .count()};
}
}
