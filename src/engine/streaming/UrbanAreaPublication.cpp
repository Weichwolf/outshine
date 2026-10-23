#include "UrbanAreaStreaming.h"
#include "PrismGridViews.h"
#include "PrismInstance.h"
#include "SceneRenderer.h"
#include "BuildingMaterials.h"
#include "RenderFrame.h"
#include <scene/Geometry.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <vector>
#include <span>
#include <utility>

namespace outshine {
namespace {
double MercatorY(double latitudeDeg) {
  return kWgs84A * std::asinh(std::tan(latitudeDeg * kDeg2Rad));
}

std::vector<PrismCellRect> Exclusions(const Generators::Osm::UrbanArea &area,
                                      std::span<const Generators::Osm::UrbanPatchRange> patches,
                                      std::span<const Ground::GeoBounds> children) {
  std::vector<PrismCellRect> cuts;
  const double pitch = area.Recipe.ProjectedPitchM;
  for (const auto &bounds : children) {
    const double west = bounds.MinLonDeg * kDeg2Rad * kWgs84A / pitch;
    const double east = bounds.MaxLonDeg * kDeg2Rad * kWgs84A / pitch;
    const double north = MercatorY(bounds.MaxLatDeg) / pitch;
    const double south = MercatorY(bounds.MinLatDeg) / pitch;
    for (size_t index = 0; index < patches.size(); ++index) {
      const auto &patch = patches[index];
      const double x = static_cast<double>(area.FirstCell[0]) + patch.Column + .5;
      const double y = static_cast<double>(area.FirstCell[1]) - patch.Row + .5;
      const auto column = [&](double at) {
        return static_cast<uint32_t>(
            std::clamp(std::ceil(at - x), 0., static_cast<double>(patch.Columns)));
      };
      const auto row = [&](double at) {
        return static_cast<uint32_t>(
            std::clamp(std::floor(y - at) + 1, 0., static_cast<double>(patch.Rows)));
      };
      const uint32_t left = column(west);
      const uint32_t right = column(east);
      const uint32_t top = row(north);
      const uint32_t bottom = row(south);
      if (left < right && top < bottom) {
        cuts.push_back({.Patch = static_cast<uint32_t>(index),
                        .Left = left,
                        .Top = top,
                        .Right = right,
                        .Bottom = bottom});
      }
    }
  }
  return cuts;
}

Mat4f RelativePlacement(const Mat4 &ecef,
                        const TangentFrame &frame,
                        const PrismPatch &patch,
                        PrismViewPlacement view) {
  Mat4f matrix;
  Vec3 origin{{ecef[12], ecef[13], ecef[14]}};
  for (size_t axis = 0; axis < 3; ++axis) {
    const Vec3 direction{{ecef[axis * 4], ecef[axis * 4 + 1], ecef[axis * 4 + 2]}};
    const auto local = frame.ToLocalDirection(direction);
    matrix[axis * 4] = static_cast<float>(local.EastM);
    matrix[axis * 4 + 1] = static_cast<float>(local.UpM);
    matrix[axis * 4 + 2] = static_cast<float>(RenderFrame::ZOfNorth(local.NorthM));
    matrix[axis * 4 + 3] = 0;
    if (axis == 0) { origin = origin + direction * (view.Column * double{patch.PitchX}); }
    if (axis == 2) { origin = origin + direction * (view.Row * double{patch.PitchZ}); }
  }
  const auto local = frame.ToLocalPosition(origin);
  matrix[12] = static_cast<float>(local.EastM);
  matrix[13] = static_cast<float>(local.UpM);
  matrix[14] = static_cast<float>(RenderFrame::ZOfNorth(local.NorthM));
  matrix[15] = 1;
  return matrix;
}
}

std::expected<bool, std::string>
UrbanAreaStreaming::Publish(Group &group,
                            const View &view,
                            std::span<const Ground::GeoBounds> children,
                            uint64_t childrenRevision) {
  const auto &key = group.Slot->ReadyKey();
  if (group.PublishedKey == key && group.ChildrenRevision == childrenRevision &&
      (!group.Handle || Renderer_->HasPrismGrid(group.Handle))) {
    return false;
  }
  const auto &product = *group.Slot->Ready();
  if (product.Geometry.Patches.size() != group.Patches.size()) {
    return std::unexpected("urban asset patch layout differs from its intrinsic request");
  }
  auto cuts = Exclusions(*group.Area, group.Patches, children);
  if (group.PublishedKey == key && group.Exclusions == cuts &&
      (!group.Handle || Renderer_->HasPrismGrid(group.Handle))) {
    group.ChildrenRevision = childrenRevision;
    return false;
  }
  auto views = ExcludePrismCells(
      product.Geometry,
      cuts,
      {.InputRecordsMost = 65536, .ViewsMost = 4096, .IntersectionTestsMost = 1048576});
  if (!views) { return std::unexpected("urban child exclusions exceed valid view budget"); }
  const auto geometry = std::make_shared<PrismGrid>(std::move(views->Geometry));
  const auto info = InspectPrismGrid(*geometry);
  if (!info) { return std::unexpected("urban view references an invalid cached prism grid"); }
  if (info->Occupied == 0) {
    if (group.Handle) { Renderer_->ReleasePrismGrid(group.Handle); }
    group.Handle = {};
  } else {
    std::vector<Render::PrismInstance> instances;
    instances.reserve(views->Placements.size());
    for (size_t index = 0; index < views->Placements.size(); ++index) {
      const auto placement = views->Placements[index];
      const auto matrix = RelativePlacement(product.Placements[placement.Patch].EcefFromLocal,
                                            view.Frame,
                                            product.Geometry.Patches[placement.Patch],
                                            placement);
      instances.push_back(
          {.Patch = static_cast<uint32_t>(index), .Current = matrix, .Previous = matrix});
    }
    const auto uploaded = Upload(group, geometry, instances);
    if (!uploaded || !*uploaded) { return uploaded; }
  }

  group.PublishedKey = key;
  group.Exclusions = std::move(cuts);
  group.ChildrenRevision = childrenRevision;
  ++Publications_;
  return true;
}

std::expected<bool, std::string>
UrbanAreaStreaming::Upload(Group &group,
                           const std::shared_ptr<const PrismGrid> &geometry,
                           std::span<const Render::PrismInstance> instances) {
  if (!Material_) {
    Geometry materials;
    Material surface;
    for (size_t axis = 0; axis < 3; ++axis) {
      surface.BaseColour[axis] = Generators::kBuildingWallColour[axis];
    }
    constexpr float roughness = 0.8f;
    surface.Roughness = roughness;
    if (!materials.addSurface("urban-shell", surface)) {
      return std::unexpected("could not declare the urban shell material");
    }
    auto material = Renderer_->RegisterPieceMaterials(std::move(materials));
    if (!material) { return std::unexpected(material.error()); }
    Material_ = *material;
  }
  if (Renderer_->HasPrismGrid(group.Handle)) {
    auto replaced = Renderer_->ReplacePrismGrid(group.Handle, geometry, instances, *Material_);
    if (!replaced && replaced.error().Kind == Render::Native::ErrorKind::Busy) { return false; }
    if (!replaced) { return std::unexpected(replaced.error().Message); }
  } else {
    auto placed = Renderer_->PlacePrismGrid(geometry, instances, *Material_);
    if (!placed && placed.error().Kind == Render::Native::ErrorKind::Busy) { return false; }
    if (!placed) { return std::unexpected(placed.error().Message); }
    group.Handle = *placed;
  }

  return true;
}

}
