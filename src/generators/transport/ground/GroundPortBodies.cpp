#include "GroundPortBodies.h"
#include "TransportNetworkSnapshot.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Error = GroundPortBodyError;
template <typename T> using Checked = std::expected<T, Error>;
constexpr uint32_t kAbsent = std::numeric_limits<uint32_t>::max();

auto Key(const World::TransportEdgeId &edge) {
  return std::tuple{edge.PathId, edge.SegmentOrdinal, edge.Direction};
}

size_t InputBytes(const TransportInputReference &input) {
  return input.DatasetId.capacity() + input.Version.capacity() + input.Revision.capacity() +
         input.ItemId.capacity() + 4;
}

std::array<uint8_t, 32> ContentBinding(const GroundPortField &field,
                                       const GroundPortBodies &bodies) {
  constexpr std::string_view kVersion = "outshine/canonical-ground-bodies/1";
  Sha256Prefix hash(kVersion.data(), kVersion.size());
  const auto word = [&hash](auto value) {
    const auto encoded = LittleEndian(value);
    hash.Append(&encoded, sizeof(encoded));
  };
  const auto &source = bodies.Source->SourceIdentity();
  for (const auto *text : {&source.DatasetId, &source.Revision}) {
    word(static_cast<uint64_t>(text->size()));
    hash.Append(text->data(), text->size());
  }
  hash.Append(bodies.ProcedureAndTerrainBinding.data(), bodies.ProcedureAndTerrainBinding.size());
  word(field.Source().WidthM);
  word(field.Source().MaximumGradient);
  word(bodies.Recipe.ThicknessM);
  word(bodies.Recipe.ApronM);
  word(bodies.Recipe.ReliefM);
  word(static_cast<uint8_t>(bodies.Scope.has_value()));
  if (bodies.Scope) {
    hash.Append(bodies.Scope->ContentBinding.data(), bodies.Scope->ContentBinding.size());
  }
  word(static_cast<uint64_t>(bodies.Bodies.size()));
  for (const auto &body : bodies.Bodies) {
    word(body.Source.PathId);
    word(body.Source.SegmentOrdinal);
    word(static_cast<uint8_t>(body.Source.Direction));
  }
  return hash.Digest(nullptr, 0);
}

std::optional<size_t> ContainerBound(const GroundPortField &field) {
  const size_t count = field.Source().SourceEdges.size();
  const size_t faces = field.Geometry().Surface.Faces.size();
  const size_t points = field.Geometry().Surface.Points.size();
  size_t remaining = std::numeric_limits<size_t>::max();
  const auto fits = [&](size_t elements, size_t width) {
    if (elements > remaining / width) { return false; }
    remaining -= elements * width;
    return true;
  };
  constexpr size_t growth = 2;
  constexpr size_t scratchPoint = sizeof(PlanarPoint) + sizeof(double) + sizeof(uint32_t);
  constexpr size_t scratchFace = sizeof(PlanarFace) + sizeof(std::array<double, 3>);
  constexpr size_t solidFace = 6 * sizeof(std::array<double, 3>) + 8 * sizeof(PlanarFace) +
                               6 * (sizeof(uint32_t) + sizeof(HeightSolidWallKind));
  constexpr size_t contactFace = sizeof(EarthworkStamp) + 12 * sizeof(double);
  if (!fits(count, growth * (sizeof(GroundPortBody) + sizeof(uint32_t))) ||
      !fits(faces, growth * (2 * sizeof(uint32_t) + scratchFace + solidFace + contactFace)) ||
      !fits(points, growth * (sizeof(uint32_t) + scratchPoint)) ||
      !fits(InputBytes(field.Fit().RecipeInput), growth) ||
      !fits(InputBytes(field.Terrain().Input()), growth)) {
    return {};
  }
  if (field.Scope() && (!fits(field.Scope()->Exits.size(), growth * sizeof(GroundPortFieldExit)) ||
                        !fits(InputBytes(field.Scope()->Input), growth))) {
    return {};
  }
  return std::numeric_limits<size_t>::max() - remaining;
}

HeightSolidBudget SolidLimits(HeightSolidBudget budget, size_t points, size_t faces) {
  budget.Points = std::min(budget.Points, points);
  budget.SurfaceFaces = std::min(budget.SurfaceFaces, faces);
  budget.Vertices = std::min(budget.Vertices, points * 2);
  budget.Faces = std::min(budget.Faces, faces * 8);
  return budget;
}

struct Scratch {
  std::vector<uint32_t> Counts, Owners, Remap, OriginalPoints;
  std::vector<PlanarPoint> Points;
  std::vector<PlanarFace> Faces;
  std::vector<std::array<double, 3>> Top;
  std::vector<double> Bottom;

  [[nodiscard]] size_t Bytes() const {
    return (Counts.capacity() + Owners.capacity() + Remap.capacity() + OriginalPoints.capacity()) *
               sizeof(uint32_t) +
           Points.capacity() * sizeof(PlanarPoint) + Faces.capacity() * sizeof(PlanarFace) +
           Top.capacity() * sizeof(std::array<double, 3>) + Bottom.capacity() * sizeof(double);
  }

  void Clear() {
    for (const auto original : OriginalPoints) { Remap[original] = kAbsent; }
    OriginalPoints.clear();
    Points.clear();
    Faces.clear();
    Top.clear();
    Bottom.clear();
  }
};

size_t Bytes(const HeightSolid &body) {
  return body.Points.capacity() * sizeof(std::array<double, 3>) +
         body.Faces.capacity() * sizeof(PlanarFace) +
         body.WallParents.capacity() * sizeof(uint32_t) +
         body.WallKinds.capacity() * sizeof(HeightSolidWallKind);
}

Checked<void> Capacity(GroundPortBodies &output, const Scratch &scratch, size_t bytesMost) {
  const size_t retained = output.Work.ContainerCapacityBytes;
  const size_t temporary = scratch.Bytes();
  output.Work.ScratchCapacityBytes = std::max(output.Work.ScratchCapacityBytes, temporary);
  if (retained > bytesMost || temporary > bytesMost - retained) {
    return std::unexpected(Error{RoadGroundError::CapacityExceeded});
  }
  return {};
}

Checked<void> Partition(const GroundPortField &field,
                        GroundPortBodies &output,
                        Scratch &scratch,
                        const std::stop_token &stop) {
  const auto edges = field.Source().Source->Topology().Edges();
  for (const auto index : field.Source().SourceEdges) {
    output.Bodies.push_back(
        {.Source = edges[index].Id, .Geometry = {}, .FaceParents = {}, .TerrainContacts = {}});
  }
  std::ranges::sort(output.Bodies, {}, [](const auto &body) { return Key(body.Source); });
  scratch.Counts.resize(output.Bodies.size());
  for (size_t face = 0; face < field.Geometry().Surface.Faces.size(); ++face) {
    if (stop.stop_requested()) { return std::unexpected(Error{RoadGroundError::Canceled}); }
    const auto source = field.SourceEdgeOfFace(face);
    if (!source) { return std::unexpected(Error{source.error()}); }
    const auto found = std::ranges::lower_bound(
        output.Bodies, Key(*source), {}, [](const auto &body) { return Key(body.Source); });
    if (found == output.Bodies.end() || found->Source != *source) {
      return std::unexpected(Error{RoadGroundError::InvalidGeometry});
    }
    const auto owner = static_cast<uint32_t>(found - output.Bodies.begin());
    scratch.Owners.push_back(owner);
    ++scratch.Counts[owner];
    ++output.Work.FaceVisits;
  }
  for (size_t at = 0; at < output.Bodies.size(); ++at) {
    if (scratch.Counts[at] == 0) {
      return std::unexpected(Error{RoadGroundError::InvalidGeometry});
    }
    output.Bodies[at].FaceParents.reserve(scratch.Counts[at]);
    output.Work.ContainerCapacityBytes +=
        output.Bodies[at].FaceParents.capacity() * sizeof(uint32_t);
  }
  for (uint32_t face = 0; face < scratch.Owners.size(); ++face) {
    output.Bodies[scratch.Owners[face]].FaceParents.push_back(face);
    ++output.Work.FaceVisits;
  }
  return {};
}

Checked<void> Copy(const RoadGroundMesh &mesh,
                   std::span<const uint32_t> parents,
                   double thicknessM,
                   HeightSolidBudget budget,
                   Scratch &scratch,
                   const std::stop_token &stop) {
  if (parents.size() > budget.SurfaceFaces) {
    return std::unexpected(Error{HeightSolidError::CapacityExceeded});
  }
  for (const auto parent : parents) {
    if (stop.stop_requested()) { return std::unexpected(Error{RoadGroundError::Canceled}); }
    PlanarFace face{};
    std::array<double, 3> heights{};
    const auto original = mesh.Surface.Faces[parent];
    for (size_t corner = 0; corner < original.size(); ++corner) {
      const auto point = original[corner];
      if (scratch.Remap[point] == kAbsent) {
        if (scratch.Points.size() >= budget.Points || scratch.Points.size() >= kAbsent) {
          return std::unexpected(Error{HeightSolidError::CapacityExceeded});
        }
        scratch.Remap[point] = static_cast<uint32_t>(scratch.Points.size());
        scratch.Points.push_back(mesh.Surface.Points[point]);
        scratch.Bottom.push_back(mesh.HeightsM[point] - thicknessM);
        scratch.OriginalPoints.push_back(point);
      }
      face[corner] = scratch.Remap[point];
      heights[corner] = mesh.HeightsM[point];
    }
    scratch.Faces.push_back(face);
    scratch.Top.push_back(heights);
  }
  return {};
}

Checked<void> Contacts(GroundPortBody &body,
                       GroundPortBodyRecipe recipe,
                       GroundPortBodyWork &work,
                       size_t bytesMost,
                       const std::stop_token &stop) {
  body.TerrainContacts.reserve(body.Geometry.SurfaceFaces);
  work.ContainerCapacityBytes += body.TerrainContacts.capacity() * sizeof(EarthworkStamp);
  const SurfaceGroundSource source{.Effect = SurfaceGroundEffect::GroundContact,
                                   .ContactKey = body.Source.PathId,
                                   .ReliefM = recipe.ReliefM};
  for (size_t face = 0; face < body.Geometry.SurfaceFaces; ++face) {
    if (stop.stop_requested()) { return std::unexpected(Error{RoadGroundError::Canceled}); }
    if (work.ContainerCapacityBytes > bytesMost) {
      return std::unexpected(Error{RoadGroundError::CapacityExceeded});
    }
    auto contact = BuildHeightSolidTerrainContact(body.Geometry, face, source, recipe.ApronM);
    if (!contact) { return std::unexpected(Error{contact.error()}); }
    work.ContainerCapacityBytes += contact->HeapBytes();
    body.TerrainContacts.push_back(std::move(*contact));
    ++work.TerrainContactFaces;
  }
  return {};
}
}

size_t GroundPortBodyCapacityBytes(const GroundPortBodies &bodies) noexcept {
  size_t bytes = bodies.Bodies.capacity() * sizeof(GroundPortBody) + InputBytes(bodies.Inputs[0]) +
                 InputBytes(bodies.Inputs[1]);
  if (bodies.Scope) {
    bytes += bodies.Scope->Exits.capacity() * sizeof(GroundPortFieldExit) +
             InputBytes(bodies.Scope->Input);
  }
  for (const auto &body : bodies.Bodies) {
    bytes += Bytes(body.Geometry) + body.FaceParents.capacity() * sizeof(uint32_t) +
             body.TerrainContacts.capacity() * sizeof(EarthworkStamp);
    for (const auto &contact : body.TerrainContacts) { bytes += contact.HeapBytes(); }
  }
  return bytes;
}

std::expected<GroundPortBodies, GroundPortBodyError>
BuildGroundPortBodies(const GroundPortField &field,
                      GroundPortBodyRecipe recipe,
                      GroundPortBodyBudget budget,
                      const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error{RoadGroundError::Canceled}); }
  if (!std::isfinite(recipe.ThicknessM) || recipe.ThicknessM <= 0 ||
      !std::isfinite(recipe.ApronM) || recipe.ApronM < 0 || !std::isfinite(recipe.ReliefM) ||
      recipe.ReliefM < 0) {
    return std::unexpected(Error{HeightSolidError::InvalidInput});
  }
  const auto &mesh = field.Geometry();
  const size_t count = field.Source().SourceEdges.size();
  const size_t faces = mesh.Surface.Faces.size();
  const size_t points = mesh.Surface.Points.size();
  if (count > budget.Bodies || count >= kAbsent || faces > budget.SurfaceFaces ||
      faces >= kAbsent || budget.Solid.Points == 0 || budget.Solid.SurfaceFaces == 0 ||
      budget.Solid.Vertices == 0 || budget.Solid.Faces == 0 || budget.Solid.EdgeVisits == 0) {
    return std::unexpected(Error{RoadGroundError::CapacityExceeded});
  }
  const auto bound = ContainerBound(field);
  if (!bound || *bound > budget.ContainerBytes) {
    return std::unexpected(Error{RoadGroundError::CapacityExceeded});
  }
  GroundPortBodies output{.Anchor = field.Domain().Anchor,
                          .Source = field.Source().Source,
                          .Inputs = {field.Fit().RecipeInput, field.Terrain().Input()},
                          .ProcedureAndTerrainBinding = field.ProcedureAndTerrainBinding(),
                          .Recipe = recipe,
                          .Scope = field.Scope(),
                          .Bodies = {},
                          .Work = {}};
  Scratch scratch;
  output.Bodies.reserve(count);
  output.Work.ContainerCapacityBytes = output.Bodies.capacity() * sizeof(GroundPortBody) +
                                       InputBytes(output.Inputs[0]) + InputBytes(output.Inputs[1]);
  if (output.Scope) {
    output.Work.ContainerCapacityBytes +=
        output.Scope->Exits.capacity() * sizeof(GroundPortFieldExit) +
        InputBytes(output.Scope->Input);
  }
  output.Work.ContainerBoundBytes = *bound;
  scratch.Owners.reserve(faces);
  scratch.Remap.resize(points, kAbsent);
  if (auto partition = Partition(field, output, scratch, stop); !partition) {
    return std::unexpected(partition.error());
  }
  if (auto capacity = Capacity(output, scratch, budget.ContainerBytes); !capacity) {
    return std::unexpected(capacity.error());
  }
  for (auto &body : output.Bodies) {
    scratch.Clear();
    if (auto copied = Copy(mesh, body.FaceParents, recipe.ThicknessM, budget.Solid, scratch, stop);
        !copied) {
      return std::unexpected(copied.error());
    }
    if (auto capacity = Capacity(output, scratch, budget.ContainerBytes); !capacity) {
      return std::unexpected(capacity.error());
    }
    auto closed =
        BuildHeightSolid({.Points = scratch.Points,
                          .Faces = scratch.Faces,
                          .TopHeights = scratch.Top,
                          .BottomHeights = scratch.Bottom},
                         SolidLimits(budget.Solid, scratch.Points.size(), scratch.Faces.size()),
                         stop);
    if (!closed) { return std::unexpected(Error{closed.error()}); }
    output.Work.FaceVisits += scratch.Faces.size();
    output.Work.RemappedVertices += scratch.Points.size();
    output.Work.CopiedSurfaceBytes +=
        scratch.Points.size() * (sizeof(PlanarPoint) + sizeof(double)) +
        scratch.Faces.size() * (sizeof(PlanarFace) + sizeof(std::array<double, 3>));
    output.Work.SolidEdgeVisits += closed->Work.EdgeVisits;
    output.Work.ContainerCapacityBytes += Bytes(*closed);
    body.Geometry = std::move(*closed);
    const auto contacts = Contacts(body, recipe, output.Work, budget.ContainerBytes, stop);
    if (!contacts) { return std::unexpected(contacts.error()); }
    if (auto capacity = Capacity(output, scratch, budget.ContainerBytes); !capacity) {
      return std::unexpected(capacity.error());
    }
  }
  output.ContentBinding = ContentBinding(field, output);
  return output;
}
}
