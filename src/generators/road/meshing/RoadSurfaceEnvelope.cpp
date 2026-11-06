#include "RoadSurfaceEnvelope.h"
#include "RibbonEnvelope.h"
#include "RoadCrossSection.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Generators {
namespace {
using Ring = std::array<RibbonVertex, kRibbonAcross>;
using VertexIds = std::array<uint32_t, kRibbonAcross>;

Ring StoredRing(const Placed &on,
                const RibbonSamplingPlan &sampling,
                const RoadSweep &surface,
                std::optional<bool> atEnd) {
  const auto samples = SampleRibbonSection(
      on, RoadSection(surface.HalfWidthM, surface.Profile), sampling.OriginM, RibbonForm::Surface);
  Ring result;
  std::ranges::copy_n(samples.Vertices.begin(), kRibbonAcross, result.begin());
  for (auto &vertex : result) {
    for (size_t axis = 0; axis < 3; ++axis) {
      vertex.PositionM[axis] = static_cast<float>(vertex.PositionM[axis] + sampling.OriginM[axis]);
    }
  }
  if (!atEnd) { return result; }
  const auto &port = surface.EndPorts[*atEnd ? 1 : 0];
  if (!port) { return result; }
  auto positions = RoadPortPositions(*port);
  if (*atEnd) { std::ranges::reverse(positions); }
  const auto normal = RoadPortNormal(port->Plane);
  for (size_t across = 0; across < kRibbonAcross; ++across) {
    for (size_t axis = 0; axis < 3; ++axis) {
      result[across].PositionM[axis] =
          static_cast<float>(positions[across][axis] - 0.0 * normal[axis]);
      result[across].NormalM[axis] = normal[axis];
    }
  }
  return result;
}

std::optional<bool> Endpoint(size_t station, size_t stations) {
  if (station == 0) { return false; }
  if (station + 1 == stations) { return true; }
  return std::nullopt;
}

std::array<Vec3f, kRibbonAcross> Positions(const Ring &ring) {
  std::array<Vec3f, kRibbonAcross> result;
  for (size_t at = 0; at < kRibbonAcross; ++at) { result[at] = ring[at].PositionM; }
  return result;
}

struct SurfacePlan {
  std::vector<Placed> Poses;
  std::vector<std::optional<RibbonEnvelope>> Spans;
};

std::expected<SurfacePlan, std::string_view> PlanSurface(const ReferenceLine &line,
                                                         const RibbonSamplingPlan &sampling,
                                                         const RoadSweep &surface,
                                                         double errorM) {
  SurfacePlan result;
  result.Poses.resize(sampling.StationsM.size());
  result.Spans.resize(sampling.StationsM.size() - 1);
  std::array<Vec3f, kRibbonAcross> previous;
  const bool hasShoulders = RoadSection(surface.HalfWidthM, surface.Profile).ShoulderM > 0;
  for (size_t at = 0; at < sampling.StationsM.size(); ++at) {
    if (!line.At(sampling.StationsM[at], result.Poses[at])) {
      return std::unexpected("surface envelope cannot place a canonical station");
    }
    const auto ring = Positions(
        StoredRing(result.Poses[at], sampling, surface, Endpoint(at, sampling.StationsM.size())));
    if (at != 0 && hasShoulders) {
      auto bound = BoundRibbonSpan(previous, ring);
      if (bound && bound->PositionBoundM <= errorM) { result.Spans[at - 1] = *bound; }
    }
    previous = ring;
  }
  for (size_t at = 0; at < result.Spans.size(); ++at) {
    if (result.Spans[at] && (at == 0 || !result.Spans[at - 1]) &&
        (at + 1 == result.Spans.size() || !result.Spans[at + 1])) {
      result.Spans[at].reset();
    }
  }
  return result;
}

struct SpanTriangles {
  std::array<uint8_t, 6 + 2 * (kRibbonAcross - 2) * 3> Indices{};
  size_t Count = 6;
};

void StitchEdge(SpanTriangles &mesh, uint8_t first, uint8_t last) {
  for (size_t at = 0; at < mesh.Count; at += 3) {
    for (size_t edge = 0; edge < 3; ++edge) {
      const auto a = mesh.Indices[at + edge];
      const auto b = mesh.Indices[at + (edge + 1) % 3];
      if ((a != first || b != last) && (a != last || b != first)) { continue; }
      const auto c = mesh.Indices[at + (edge + 2) % 3];
      const auto step = a < b ? 1 : -1;
      const auto one = static_cast<uint8_t>(static_cast<int>(a) + step);
      const auto two = static_cast<uint8_t>(static_cast<int>(a) + 2 * step);
      const std::array<uint8_t, 9> replacement{a, one, c, one, two, c, two, b, c};
      std::ranges::copy_n(
          replacement.begin(), 3, mesh.Indices.begin() + static_cast<ptrdiff_t>(at));
      std::ranges::copy(replacement.begin() + 3,
                        replacement.end(),
                        mesh.Indices.begin() + static_cast<ptrdiff_t>(mesh.Count));
      mesh.Count += 6;
      return;
    }
  }
}

VertexIds AppendRing(const Ring &ring, bool complete, const Vec3f &colour, RoadMeshBuffers &mesh) {
  VertexIds result{};
  for (size_t at = 0; at < kRibbonAcross; ++at) {
    if (!complete && at != 0 && at + 1 != kRibbonAcross) { continue; }
    result[at] = static_cast<uint32_t>(mesh.PositionM.size() / 3);
    const auto &vertex = ring[at];
    mesh.PositionM.insert(mesh.PositionM.end(), vertex.PositionM.begin(), vertex.PositionM.end());
    mesh.NormalM.insert(mesh.NormalM.end(), vertex.NormalM.begin(), vertex.NormalM.end());
    mesh.ColourRgba.insert(mesh.ColourRgba.end(), {colour[0], colour[1], colour[2], 1.f});
  }
  return result;
}

void AppendOriginal(const VertexIds &first,
                    const VertexIds &second,
                    bool shoulders,
                    RoadMeshBuffers &mesh) {
  const size_t begin = shoulders ? 0 : 1;
  const size_t end = shoulders ? 3 : 2;
  for (size_t at = begin; at < end; ++at) {
    mesh.Index.insert(
        mesh.Index.end(),
        {first[at], second[at], first[at + 1], first[at + 1], second[at], second[at + 1]});
  }
}

void AppendEnvelope(const RibbonEnvelope &envelope,
                    const std::array<VertexIds, 2> &rings,
                    std::array<bool, 2> contacts,
                    RoadMeshBuffers &mesh) {
  SpanTriangles triangles;
  std::ranges::copy(envelope.Triangles, triangles.Indices.begin());
  if (contacts[0]) { StitchEdge(triangles, 0, 3); }
  if (contacts[1]) { StitchEdge(triangles, 4, 7); }
  for (size_t at = 0; at < triangles.Count; ++at) {
    const size_t source = triangles.Indices[at];
    mesh.Index.push_back(rings[source / kRibbonAcross][source % kRibbonAcross]);
  }
}

bool CompleteRing(size_t at, std::span<const std::optional<RibbonEnvelope>> spans) {
  return at == 0 || at == spans.size() || !spans[at - 1] || !spans[at];
}

void ReserveSurface(const SurfacePlan &plan, bool shoulders, RoadMeshBuffers &mesh) {
  size_t vertices = 0;
  size_t indices = 0;
  for (size_t at = 0; at < plan.Poses.size(); ++at) {
    vertices += CompleteRing(at, plan.Spans) ? 4 : 2;
    if (at == 0) { continue; }
    if (plan.Spans[at - 1]) {
      indices +=
          6 + (CompleteRing(at - 1, plan.Spans) ? 6 : 0) + (CompleteRing(at, plan.Spans) ? 6 : 0);
    } else {
      indices += shoulders ? (kRibbonAcross - 1) * 6 : 6;
    }
  }
  mesh.PositionM.reserve(mesh.PositionM.size() + vertices * 3);
  mesh.NormalM.reserve(mesh.NormalM.size() + vertices * 3);
  mesh.ColourRgba.reserve(mesh.ColourRgba.size() + vertices * 4);
  mesh.Index.reserve(mesh.Index.size() + indices);
}

RoadSurfaceEnvelopeStats AppendSurface(const SurfacePlan &plan,
                                       const RibbonSamplingPlan &sampling,
                                       const RoadSweep &surface,
                                       RoadMeshBuffers &mesh) {
  RoadSurfaceEnvelopeStats result;
  VertexIds previous{};
  const bool shoulders = RoadSection(surface.HalfWidthM, surface.Profile).ShoulderM > 0;
  ReserveSurface(plan, shoulders, mesh);
  for (size_t at = 0; at < plan.Poses.size(); ++at) {
    const auto ring =
        StoredRing(plan.Poses[at], sampling, surface, Endpoint(at, plan.Poses.size()));
    const bool complete = CompleteRing(at, plan.Spans);
    const auto here = AppendRing(ring, complete, surface.WearsLinear, mesh);
    if (at != 0) {
      const auto &span = plan.Spans[at - 1];
      if (span) {
        AppendEnvelope(*span, {previous, here}, {CompleteRing(at - 1, plan.Spans), complete}, mesh);
        result.PositionBoundM = std::max(result.PositionBoundM, span->PositionBoundM);
        ++result.EnvelopeSpans;
      } else {
        AppendOriginal(previous, here, shoulders, mesh);
        ++result.OriginalSpans;
      }
    }
    previous = here;
  }
  return result;
}
}

std::expected<RoadSurfaceEnvelopeStats, std::string_view>
AppendRoadSurfaceEnvelope(const ReferenceLine &line,
                          const RibbonSamplingPlan &sampling,
                          const RoadSweep &surface,
                          double errorM,
                          RoadMeshBuffers &mesh) {
  if (surface.Form != RibbonForm::Surface || !std::isfinite(errorM) || errorM <= 0 ||
      sampling.StationsM.size() < 2 ||
      mesh.PositionM.size() / 3 > std::numeric_limits<uint32_t>::max() ||
      sampling.StationsM.size() >
          (std::numeric_limits<uint32_t>::max() - mesh.PositionM.size() / 3) / kRibbonAcross) {
    return std::unexpected(
        "surface envelope needs a finite budget and addressable canonical stations");
  }
  const auto plan = PlanSurface(line, sampling, surface, errorM);
  if (!plan) { return std::unexpected(plan.error()); }
  return AppendSurface(*plan, sampling, surface, mesh);
}
}
