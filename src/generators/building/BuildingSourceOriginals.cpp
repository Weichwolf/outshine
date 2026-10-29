#include "BuildingSourceView.h"
#include "BuildingMeshConversion.h"
#include "BuildingScratch.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
struct VertexAppend {
  size_t Previous = 0;
  size_t Added = 0;
};

void AppendAttribute(std::vector<float> &target,
                     std::span<const float> source,
                     VertexAppend vertices,
                     size_t width,
                     float fallback) {
  if (target.empty() && source.empty()) { return; }
  if (target.empty()) { target.resize(vertices.Previous * width, fallback); }
  if (source.empty()) {
    target.insert(target.end(), vertices.Added * width, fallback);
  } else {
    target.insert(target.end(), source.begin(), source.end());
  }
}

bool AppendMesh(Geometry::Mesh &target, Geometry::Mesh source) {
  const size_t previous = target.PositionsM.size() / 3;
  const size_t added = source.PositionsM.size() / 3;
  if (added > std::numeric_limits<uint32_t>::max() ||
      previous > std::numeric_limits<uint32_t>::max() - added) {
    return false;
  }
  const VertexAppend vertices{.Previous = previous, .Added = added};
  AppendAttribute(target.Colours, source.Colours, vertices, 4, 1);
  AppendAttribute(target.Uv1, source.Uv1, vertices, 2, 0);
  target.PositionsM.insert(
      target.PositionsM.end(), source.PositionsM.begin(), source.PositionsM.end());
  target.Normals.insert(target.Normals.end(), source.Normals.begin(), source.Normals.end());
  target.Uv.insert(target.Uv.end(), source.Uv.begin(), source.Uv.end());
  for (const uint32_t index : source.Indices) {
    target.Indices.push_back(static_cast<uint32_t>(previous) + index);
  }
  return true;
}

bool AppendOriginal(std::array<Geometry::Mesh, 3> &meshes,
                    const Raised &source,
                    const BuildingCaptureFrame &frame) {
  const std::array parts{BuildingMeshPart{.Vertices = source.WallCorners,
                                          .Indices = source.WallRun,
                                          .Colours = source.WallColours,
                                          .MaterialM = source.WallMaterialM},
                         BuildingMeshPart{.Vertices = source.RoofCorners,
                                          .Indices = source.RoofRun,
                                          .Colours = {},
                                          .MaterialM = {}}};
  for (size_t at = 0; at < parts.size(); ++at) {
    const auto &part = parts[at];
    if (part.Vertices.empty()) { continue; }
    const size_t material = at == 0 && !part.MaterialM.empty() ? 2 : at;
    auto mesh = ConvertBuildingMesh(part, frame.Axes, frame.Origin);
    if (!mesh || !AppendMesh(meshes[material], std::move(*mesh))) { return false; }
  }
  return true;
}

std::expected<void, BuildingSourceView::Error>
EmitOriginal(const BuildingSurface &source,
             BuildingScratch &scratch,
             const BuildingScratch::TriangleBudget &triangles,
             Raised &emitted,
             const std::stop_token &stop) {
  if (!source.RequireShapes()) {
    return std::unexpected(
        BuildingSourceView::Error{.Kind = BuildingPlaneCapture::ErrorKind::SourceUnavailable,
                                  .Message = "original building forms unavailable"});
  }
  emitted.Clear();
  for (size_t face = 0; face < source.FaceCount(); ++face) {
    if (stop.stop_requested()) {
      return std::unexpected(
          BuildingSourceView::Error{.Kind = BuildingPlaneCapture::ErrorKind::Cancelled,
                                    .Message = "original building geometry cancelled"});
    }
    if (triangles.Remaining == 0) {
      return std::unexpected(
          BuildingSourceView::Error{.Kind = BuildingPlaneCapture::ErrorKind::WorkBudgetExceeded,
                                    .Message = "original building triangle budget exhausted"});
    }
    const std::array selected{source.FaceAt(face)};
    if (!source.MeshVisible(selected, scratch, emitted)) {
      return std::unexpected(BuildingSourceView::Error{
          .Kind = triangles.Exhausted ? BuildingPlaneCapture::ErrorKind::WorkBudgetExceeded
                                      : BuildingPlaneCapture::ErrorKind::UnsupportedSource,
          .Message = "original building mesh emission failed"});
    }
  }
  return {};
}
}

std::expected<Geometry, BuildingSourceView::Error>
BuildingSourceView::Originals(BuildingSourceBudget budget,
                              const std::array<Material, 3> &materials,
                              const std::stop_token &stop) const {
  if (stop.stop_requested()) {
    return std::unexpected(Error{.Kind = BuildingPlaneCapture::ErrorKind::Cancelled,
                                 .Message = "original building geometry cancelled"});
  }
  if (!Unsupported_.empty()) {
    return std::unexpected(Error{.Kind = BuildingPlaneCapture::ErrorKind::UnsupportedSource,
                                 .Source = Unsupported_.front(),
                                 .Message = "whole original group has unsupported sources"});
  }
  if (Sources_.size() > budget.Sources || budget.TriangleAttempts == 0 ||
      budget.TriangleAttempts > std::numeric_limits<uint32_t>::max() / 3) {
    return std::unexpected(
        Error{.Kind = BuildingPlaneCapture::ErrorKind::WorkBudgetExceeded,
              .Message = "original building source or triangle budget exceeded"});
  }
  uint64_t faces = 0;
  for (const auto source : Sources_) {
    const size_t count = Base_->Surfaces[source].FaceCount();
    if (faces > budget.Faces || count > budget.Faces - faces) {
      return std::unexpected(Error{.Kind = BuildingPlaneCapture::ErrorKind::WorkBudgetExceeded,
                                   .Message = "original building face budget exceeded"});
    }
    faces += count;
  }
  BuildingScratch scratch;
  const auto &triangles = scratch.Triangles.emplace(
      BuildingScratch::TriangleBudget{.Remaining = budget.TriangleAttempts});
  std::array<Geometry::Mesh, 3> meshes;
  Raised emitted;
  for (const auto ordinal : Sources_) {
    if (stop.stop_requested()) {
      return std::unexpected(Error{.Kind = BuildingPlaneCapture::ErrorKind::Cancelled,
                                   .Message = "original building geometry cancelled"});
    }
    const auto emission = EmitOriginal(Base_->Surfaces[ordinal], scratch, triangles, emitted, stop);
    if (!emission) {
      auto error = emission.error();
      error.Source = ordinal;
      return std::unexpected(std::move(error));
    }
    if (!AppendOriginal(meshes, emitted, Frame_)) {
      return std::unexpected(
          Error{.Source = ordinal, .Message = "original building geometry conversion failed"});
    }
  }
  Geometry result;
  for (const auto &material : materials) {
    if (!result.addSurface("original-source", material)) {
      return std::unexpected(Error{.Message = "invalid original building material"});
    }
  }
  for (size_t at = 0; at < meshes.size(); ++at) {
    if (meshes[at].PositionsM.empty()) { continue; }
    const auto part = result.addPart("original-source", MaterialInstance(static_cast<int>(at)));
    if (!part || !result.adoptMesh(*part, meshes[at])) {
      return std::unexpected(Error{.Message = "original building mesh rejected"});
    }
  }
  return result;
}
}
