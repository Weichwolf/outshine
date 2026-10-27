#include "GroundPortBodies.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
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
  size_t remaining = budget.ContainerBytes;
  const auto fits = [&](size_t elements, size_t width) {
    if (elements > remaining / width) { return false; }
    remaining -= elements * width;
    return true;
  };
  if (count > budget.Bodies || count >= kAbsent || faces > budget.SurfaceFaces ||
      faces >= kAbsent || !fits(count, sizeof(GroundPortBody) + sizeof(uint32_t)) ||
      !fits(faces, 2 * sizeof(uint32_t) + sizeof(EarthworkStamp) + 12 * sizeof(double)) ||
      !fits(points, sizeof(uint32_t)) || budget.Solid.Points == 0 ||
      budget.Solid.SurfaceFaces == 0 || budget.Solid.Vertices == 0 || budget.Solid.Faces == 0 ||
      budget.Solid.EdgeVisits == 0) {
    return std::unexpected(Error{RoadGroundError::CapacityExceeded});
  }
  GroundPortBodies output{
      .Anchor = field.Domain().Anchor, .Source = field.Source().Source, .Bodies = {}, .Work = {}};
  Scratch scratch;
  output.Bodies.reserve(count);
  output.Work.ContainerCapacityBytes = output.Bodies.capacity() * sizeof(GroundPortBody);
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
    auto closed = BuildHeightSolid({.Points = scratch.Points,
                                    .Faces = scratch.Faces,
                                    .TopHeights = scratch.Top,
                                    .BottomHeights = scratch.Bottom},
                                   budget.Solid,
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
  return output;
}
}
