#include "RoadGroundMeshInternal.h"
#include "RoadGroundFacetInternal.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numeric>
#include <span>
#include <stop_token>

namespace outshine::Generators::RoadGround {
namespace {
constexpr size_t kSourceVerticesMost = 4096;
constexpr size_t kSourceFacesMost = 8192;
constexpr size_t kPatchesMost = 511;
constexpr size_t kKnotReserve = 513;
constexpr double kCoordinateMostM = 1e6;

size_t Capacity(const SliceState &state) {
  return state.Mesh.Points.capacity() * sizeof(PlanarPoint) +
         state.Mesh.Faces.capacity() * sizeof(PlanarFace) +
         state.Mesh.PointRemap.capacity() * sizeof(uint32_t) +
         state.Mesh.FaceParents.capacity() * sizeof(uint32_t) +
         state.Knots.capacity() * sizeof(double) + state.Edges.capacity() * sizeof(SliceEdge) +
         state.Nodes.capacity() * sizeof(uint32_t);
}

MeshResult<void> SlicePatchFaces(SliceState &state, bool hard, const std::stop_token &stop) {
  for (size_t face = 0; face < state.Source.Faces.size(); ++face) {
    ++state.Work.SourceFaceVisits;
    if (state.Source.Faces[face].Patch != state.Patch) { continue; }
    if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
    if (hard) {
      if (state.Mesh.Faces.size() >= state.Budget.Faces) {
        return std::unexpected(RoadGroundError::CapacityExceeded);
      }
      state.Mesh.Faces.push_back(state.Source.Triangles[face]);
      state.Mesh.FaceParents.push_back(static_cast<uint32_t>(face));
    } else if (const auto sliced = SliceTriangle(state, face); !sliced) {
      return std::unexpected(sliced.error());
    }
  }
  return {};
}

}

MeshResult<void> AdmitMeshSource(const RoadGroundField &source, RoadGroundMeshBudget budget) {
  const size_t vertices = source.Priors.size();
  const size_t faces = source.Triangles.size();
  if (vertices > kSourceVerticesMost || faces > kSourceFacesMost ||
      source.Patches.size() > kPatchesMost || vertices > budget.Vertices || faces > budget.Faces) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  const size_t meshBytes = budget.Vertices * sizeof(PlanarPoint) +
                           budget.Faces * 4 * sizeof(uint32_t) + vertices * sizeof(uint32_t);
  const size_t scratchBytes = (kKnotReserve + 3 * faces) * sizeof(double) +
                              3 * faces * sizeof(SliceEdge) +
                              budget.CacheEntries * sizeof(uint32_t);
  const size_t auditBytes =
      budget.Vertices * (sizeof(double) + sizeof(uint32_t)) + FacetAuditBytes(budget.Faces);
  if (meshBytes + std::max(scratchBytes, auditBytes) +
          source.Patches.size() * (sizeof(size_t) + sizeof(uint8_t)) >
      budget.ContainerBytes) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  if (vertices < 3 || faces == 0 || source.Patches.empty() || source.Faces.size() != faces) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  for (const auto &prior : source.Priors) {
    if (!std::isfinite(prior.At.EastM) || !std::isfinite(prior.At.NorthM) ||
        std::abs(prior.At.EastM) > kCoordinateMostM ||
        std::abs(prior.At.NorthM) > kCoordinateMostM) {
      return std::unexpected(RoadGroundError::InvalidGeometry);
    }
  }
  for (size_t at = 0; at < faces; ++at) {
    if (source.Faces[at].Patch >= source.Patches.size()) {
      return std::unexpected(RoadGroundError::InvalidGeometry);
    }
    for (size_t corner = 0; corner < 3; ++corner) {
      const auto vertex = source.Triangles[at][corner];
      if (vertex >= vertices ||
          source.Faces[at].Triangle[corner].EastM != source.Priors[vertex].At.EastM ||
          source.Faces[at].Triangle[corner].NorthM != source.Priors[vertex].At.NorthM) {
        return std::unexpected(RoadGroundError::InvalidGeometry);
      }
    }
  }
  return {};
}

MeshResult<PlanarMesh> SliceMesh(const RoadGroundField &source,
                                 std::span<const size_t> segments,
                                 RoadGroundMeshBudget budget,
                                 RoadGroundMeshWork &work,
                                 const std::stop_token &stop) {
  PlanarMesh mesh;
  mesh.Points.reserve(budget.Vertices);
  mesh.Faces.reserve(budget.Faces);
  mesh.FaceParents.reserve(budget.Faces);
  mesh.PointRemap.resize(source.Priors.size());
  std::ranges::iota(mesh.PointRemap, uint32_t{0});
  double scale = 1;
  for (const auto &prior : source.Priors) {
    mesh.Points.push_back({prior.At.EastM, prior.At.NorthM});
    scale = std::max({scale, std::abs(prior.At.EastM), std::abs(prior.At.NorthM)});
  }
  work.CopiedSourcePointBytes += source.Priors.size() * sizeof(PlanarPoint);
  SliceState state{.Source = source,
                   .Mesh = mesh,
                   .Work = work,
                   .Budget = budget,
                   .Patch = 0,
                   .ToleranceM = 64 * std::numeric_limits<double>::epsilon() * scale,
                   .Knots = {},
                   .Edges = {},
                   .Nodes = {}};
  state.Knots.reserve(kKnotReserve + 3 * source.Faces.size());
  state.Edges.reserve(3 * source.Faces.size());
  state.Nodes.reserve(budget.CacheEntries);
  for (size_t patch = 0; patch < source.Patches.size(); ++patch) {
    if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
    state.Patch = patch;
    const bool hard = source.Patches[patch].Parents[0] == source.Patches[patch].Parents[1];
    if (!hard) {
      if (const auto prepared = PreparePatch(state, segments[patch]); !prepared) {
        return std::unexpected(prepared.error());
      }
    }
    if (const auto sliced = SlicePatchFaces(state, hard, stop); !sliced) {
      return std::unexpected(sliced.error());
    }
    work.ContainerCapacityBytes = std::max(work.ContainerCapacityBytes, Capacity(state));
    if (work.ContainerCapacityBytes > budget.ContainerBytes) {
      return std::unexpected(RoadGroundError::CapacityExceeded);
    }
  }
  work.NewVertices += mesh.Points.size() - source.Priors.size();
  return mesh;
}
}
