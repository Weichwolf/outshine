#include "CanonicalGroundFieldFixture.h"
#include "CanonicalGroundBodyExport.h"
#include "CppAllocationProbe.h"
#include "CanonicalGroundTerrainPartitionFixture.h"
#include "Check.h"
#include "GroundPortBodies.h"
#include "GroundPortCore.h"
#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <print>
#include <ratio>
#include <variant>
#include <set>
#include <stop_token>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr TransportCoreLimits kLimits{.SourceNodesMost = 16384,
                                      .SourceEdgesMost = 32768,
                                      .SegmentsMost = 4096,
                                      .ContainerCapacityMost = 1048576};

auto Key(const World::TransportEdgeId &edge) {
  return std::tuple{edge.PathId, edge.SegmentOrdinal, edge.Direction};
}

std::set<std::pair<uint32_t, uint32_t>> Cells(const GroundPortField &field) {
  constexpr double count = uint32_t{1} << 19U;
  const auto &graph = field.Source().Source->Topology();
  std::set<std::pair<uint32_t, uint32_t>> cells;
  for (const auto edge : field.Source().SourceEdges) {
    const auto &source = graph.Edges()[edge];
    const auto *a = graph.FindNode(source.FromNodeId);
    const auto *b = graph.FindNode(source.ToNodeId);
    if (a == nullptr || b == nullptr) { return {}; }
    const auto lowX =
        static_cast<uint32_t>((std::min(a->LongitudeDeg, b->LongitudeDeg) + 180) / 360 * count);
    const auto highX =
        static_cast<uint32_t>((std::max(a->LongitudeDeg, b->LongitudeDeg) + 180) / 360 * count);
    const auto lowY =
        static_cast<uint32_t>((std::min(a->LatitudeDeg, b->LatitudeDeg) + 90) / 180 * count);
    const auto highY =
        static_cast<uint32_t>((std::max(a->LatitudeDeg, b->LatitudeDeg) + 90) / 180 * count);
    CHECK(uint64_t{highX - lowX + 1} * (highY - lowY + 1) < 4096,
          "the actual source fixture has a finite independently enumerated geographic extent");
    if (uint64_t{highX - lowX + 1} * (highY - lowY + 1) >= 4096) { return {}; }
    for (uint32_t x = lowX; x <= highX; ++x) {
      for (uint32_t y = lowY; y <= highY; ++y) { cells.emplace(x, y); }
    }
  }
  return cells;
}

void CheckClosed(const HeightSolid &body) {
  std::map<std::array<uint32_t, 2>, std::pair<unsigned, int>> edges;
  for (const auto face : body.Faces) {
    for (size_t at = 0; at < 3; ++at) {
      const auto a = face[at];
      const auto b = face[(at + 1) % 3];
      auto &value = edges[{std::min(a, b), std::max(a, b)}];
      ++value.first;
      value.second += a < b ? 1 : -1;
    }
  }
  for (const auto &[edge, count] : edges) {
    CHECK(count.first == 2 && count.second == 0,
          "every functional body edge has exactly two oppositely wound faces including interfaces");
  }
  for (size_t face = 0; face < body.Faces.size(); ++face) {
    const bool internal =
        face >= 2 * body.SurfaceFaces &&
        body.WallKinds[face - 2 * body.SurfaceFaces] == HeightSolidWallKind::Interface;
    CHECK(HeightSolidExposesFace(body, face) != internal,
          "closed functional interfaces are suppressed while all actual outer surfaces remain "
          "visible");
  }
}

std::set<std::array<double, 3>>
Seam(const GroundPortField &field, const GroundPortBody &body, LongitudeLatitude anchor) {
  const std::array requests{GroundPortFieldRequestPoint{.Edge = body.Source, .Anchor = anchor}};
  const auto sampled = field.Sample(requests);
  CHECK(sampled.has_value(), "seam inspection uses the actual canonical source-bound port");
  if (!sampled) { return {}; }
  const auto &gate = sampled->Ports[0].Port.Gate;
  const auto source = TangentFrame::At(field.Domain().Anchor);
  const auto target = TangentFrame::At(anchor);
  const auto normal = source.ToLocalDirection(
      target.ToEcefDirection({.EastM = gate.OutE, .NorthM = gate.OutN, .UpM = 0}));
  const auto centre =
      source.ToLocalPosition(target.ToEcefPosition({.EastM = 0, .NorthM = 0, .UpM = gate.GradeM}));
  const auto distance = [&](const auto &point) {
    return normal.EastM * (point[0] - centre.EastM) + normal.NorthM * (point[1] - centre.NorthM) +
           normal.UpM * (point[2] - centre.UpM);
  };
  std::set<std::array<double, 3>> seam;
  const auto &geometry = body.Geometry;
  for (size_t wall = 0; wall < geometry.WallKinds.size(); ++wall) {
    if (geometry.WallKinds[wall] != HeightSolidWallKind::Interface) { continue; }
    const auto edge = HeightSolidWallSurfaceEdge(geometry, wall);
    if (!edge) { continue; }
    if (std::abs(distance(geometry.Points[(*edge)[0]])) > 1e-7 ||
        std::abs(distance(geometry.Points[(*edge)[1]])) > 1e-7) {
      continue;
    }
    for (const auto vertex : geometry.Faces[2 * geometry.SurfaceFaces + wall]) {
      seam.insert(geometry.Points[vertex]);
    }
  }
  return seam;
}

void Refusals(const GroundPortField &field,
              const GroundPortBodies &base,
              const TransportCoreSelection &selection) {
  auto changed = selection;
  changed.Segments[0].Ends[0].Fraction =
      std::midpoint(changed.Segments[0].Ends[0].Fraction, changed.Segments[0].Ends[1].Fraction);
  CHECK(!SliceGroundPortBodies(field, base, changed),
        "caller-modified interval ends cannot bypass canonical ownership");
  changed = selection;
  changed.Segments[0].Ends[0].Fraction = std::numeric_limits<double>::quiet_NaN();
  CHECK(!SliceGroundPortBodies(field, base, changed),
        "nonfinite caller intervals are refused before sorting or geometric work");
  changed = selection;
  changed.Segments.pop_back();
  CHECK(!SliceGroundPortBodies(field, base, changed),
        "a missing owned interval cannot publish an apparently full core");
  const auto capped = SliceGroundPortBodies(field, base, selection, {.ContainerBytes = 0});
  CHECK(
      !capped,
      "container admission refuses before output construction instead of exposing partial bodies");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = SliceGroundPortBodies(field, base, selection, {}, stop.get_token());
  CHECK(!canceled, "a canceled geographic request returns no partially constructed product");
}

struct Piece {
  GroundPortBody Body;
  std::array<TransportCoreEnd, 2> Ends;
};

size_t CheckSharedCuts(const GroundPortField &field, std::vector<Piece> &pieces) {
  std::ranges::sort(pieces, {}, [](const auto &piece) {
    return std::tuple{Key(piece.Body.Source), piece.Body.SourceBegin};
  });
  size_t seams = 0;
  for (size_t at = 1; at < pieces.size(); ++at) {
    const auto &a = pieces[at - 1];
    const auto &b = pieces[at];
    if (a.Body.Source != b.Body.Source) { continue; }
    CHECK(a.Body.SourceEnd == b.Body.SourceBegin && a.Ends[1].Geographic == b.Ends[0].Geographic,
          "successive owned source intervals neither overlap nor omit a part of the physical axis");
    const auto seam = Seam(field, a.Body, a.Ends[1].Geographic);
    CHECK(seam.size() >= 4 && seam == Seam(field, b.Body, b.Ends[0].Geographic),
          "independently clipped neighbors share every top/underside seam vertex bit-for-bit");
    ++seams;
  }
  return seams;
}

void CheckCoreFrame(const GroundPortBodies &original) {
  CHECK(original.Core.has_value(),
        "a geographic body retains its canonical owner during reframing");
  if (!original.Core) { return; }
  const auto bounds = original.Core->Bounds();
  if (!bounds) { return; }
  const LongitudeLatitude anchor{.LongitudeDeg = std::midpoint(bounds->WestDeg, bounds->EastDeg),
                                 .LatitudeDeg = std::midpoint(bounds->SouthDeg, bounds->NorthDeg)};
  auto input = original;
  BeginCppAllocationProbe();
  const auto reframed = ReframeGroundPortBodies(std::move(input), anchor);
  const auto allocations = EndCppAllocationProbe();
  CHECK(reframed && allocations.Calls == 0,
        "actual owned original contacts and sliced geometry enter their core frame in-place "
        "without allocation");
  if (!reframed) { return; }
  CHECK(reframed->Core == original.Core && reframed->ContentBinding == original.ContentBinding,
        "the coordinate frame does not change original ownership or bound content identity");
  const auto from = TangentFrame::At(original.Anchor);
  const auto to = TangentFrame::At(anchor);
  for (size_t at = 0; at < original.Bodies.size(); ++at) {
    const auto &oldBody = original.Bodies[at];
    const auto &body = reframed->Bodies[at];
    CHECK(body.SourceBegin == oldBody.SourceBegin && body.SourceEnd == oldBody.SourceEnd &&
              body.TerrainContacts.size() == oldBody.TerrainContacts.size(),
          "original contact ownership remains separate from the new count of body triangles");
    for (size_t contact = 0; contact < body.TerrainContacts.size(); ++contact) {
      const auto &old = oldBody.TerrainContacts[contact];
      const auto &current = body.TerrainContacts[contact];
      for (size_t corner = 0; corner < 3; ++corner) {
        const EastNorth before{.EastM = old.RingEastNorthM[2 * corner],
                               .NorthM = old.RingEastNorthM[2 * corner + 1]};
        const EastNorth after{.EastM = current.RingEastNorthM[2 * corner],
                              .NorthM = current.RingEastNorthM[2 * corner + 1]};
        const auto a = from.ToEcefPosition(
            {.EastM = before.EastM, .NorthM = before.NorthM, .UpM = old.WantsAt(before)});
        const auto b = to.ToEcefPosition(
            {.EastM = after.EastM, .NorthM = after.NorthM, .UpM = current.WantsAt(after)});
        CHECK(std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]) < 1e-7,
              "the transformed contact preserves its actual original ECEF triangle instead of a "
              "clipped surrogate");
      }
    }
  }
}

void CheckField(const GroundPortField &field, size_t which, const char *prefix) {
  const auto base = BuildGroundPortBodies(field, {.ThicknessM = .3, .ApronM = 5, .ReliefM = .4});
  CHECK(base.has_value(), "the actual whole finite body is built once and borrowed by every core");
  if (!base) { return; }
  CanonicalGroundFixture::WriteBodies(
      base->Bodies, [](const auto &body) -> const auto & { return body; }, prefix, which, "before");
  std::vector<Piece> pieces;
  double volume = 0;
  size_t cutCores = 0;
  size_t visits = 0;
  double buildMs = 0;
  size_t allocations = 0;
  size_t requestedBytes = 0;
  size_t containerPeak = 0;
  size_t knownScratchPeak = 0;
  const auto begin = std::chrono::steady_clock::now();
  for (const auto &[x, y] : Cells(field)) {
    auto selection = SelectGroundPortCore(field, {.Level = 19, .X = x, .Y = y}, kLimits);
    CHECK(selection.has_value(),
          "actual source axes are selected through the existing half-open owner contract");
    if (!selection || selection->Segments.empty()) { continue; }
    const auto buildBegin = std::chrono::steady_clock::now();
    BeginCppAllocationProbe();
    const auto first = SliceGroundPortBodies(field, *base, *selection);
    const auto allocated = EndCppAllocationProbe();
    buildMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - buildBegin)
            .count();
    allocations += allocated.Calls;
    requestedBytes += allocated.Bytes;
    std::ranges::reverse(selection->Segments);
    auto second = SliceGroundPortBodies(field, *base, *selection, {.ContainerBytes = 67108864});
    CHECK(first && second,
          "actual affine/C1/scoped bodies clip under normal and enlarged work caps");
    if (!first || !second) {
      if (!first) {
        std::println("GROUND_SLICE_ERROR field={} core={}/{} kind={} value={}",
                     which,
                     x,
                     y,
                     first.error().index(),
                     std::visit([](auto error) { return unsigned(error); }, first.error()));
      }
      continue;
    }
    CHECK(first->ContentBinding == second->ContentBinding && first->Core == second->Core &&
              first->ContentBinding != base->ContentBinding,
          "geographic body identity binds ownership and content independently of request order and "
          "work caps");
    CHECK(first->Work.ContainerCapacityBytes <= first->Work.ContainerBoundBytes,
          "the measured retained containers fit the admitted geographic allocation bound");
    visits += first->Work.FaceVisits;
    containerPeak = std::max(containerPeak, first->Work.ContainerCapacityBytes);
    knownScratchPeak = std::max(knownScratchPeak, first->Work.ScratchCapacityBytes);
    if (cutCores++ == 0) {
      Refusals(field, *base, *selection);
      CheckCoreFrame(*first);
    }
    for (size_t at = 0; at < first->Bodies.size(); ++at) {
      const auto &body = first->Bodies[at];
      const auto &other = second->Bodies[at];
      CHECK(body.Geometry.Points == other.Geometry.Points &&
                body.Geometry.Faces == other.Geometry.Faces &&
                body.Geometry.WallKinds == other.Geometry.WallKinds &&
                body.FaceParents == other.FaceParents,
            "actual entire geometry and interface classification are unchanged by request order or "
            "larger caps");
      CheckClosed(body.Geometry);
      volume += body.Geometry.VolumeM3;
      const auto segment = std::ranges::find_if(selection->Segments, [&](const auto &value) {
        return field.Source().Source->Topology().Edges()[value.SourceEdge].Id == body.Source &&
               value.Ends[0].Fraction == body.SourceBegin &&
               value.Ends[1].Fraction == body.SourceEnd;
      });
      CHECK(segment != selection->Segments.end(),
            "each produced body retains its actual canonical source interval");
      if (segment != selection->Segments.end()) {
        pieces.push_back({.Body = std::move(second->Bodies[at]), .Ends = segment->Ends});
      }
    }
  }
  const auto seams = CheckSharedCuts(field, pieces);
  std::vector<EarthworkStamp> contacts;
  for (const auto &piece : pieces) {
    contacts.insert(
        contacts.end(), piece.Body.TerrainContacts.begin(), piece.Body.TerrainContacts.end());
  }
  CanonicalGroundFixture::CheckTerrainPartition(field, *base, contacts, which);
  CanonicalGroundFixture::WriteBodies(
      pieces, [](const auto &piece) -> const auto & { return piece.Body; }, prefix, which, "after");
  double original = 0;
  for (const auto &body : base->Bodies) { original += body.Geometry.VolumeM3; }
  CHECK(cutCores > 0 && seams > 0 && std::abs(volume - original) < 1e-6,
        "all geographic owners together preserve the whole original solid volume, full width and "
        "underside");
  std::println(
      "GROUND_SLICES field={} cores={} bodies={} seams={} visits={} volume={} original={} "
      "validation_ms={} build_ms={} allocations={} requested_bytes={} container_peak={} "
      "known_vector_scratch_peak={} base_capacity={}",
      which,
      cutCores,
      pieces.size(),
      seams,
      visits,
      volume,
      original,
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count(),
      buildMs,
      allocations,
      requestedBytes,
      containerPeak,
      knownScratchPeak,
      base->Work.ContainerCapacityBytes);
}
}

int main(int argc, char **argv) {
  const auto fields = CanonicalGroundFixture::LoadFields();
  CHECK(fields.size() == 4,
        "both original affine/C1 fields and both actual native scoped publications participate");
  for (size_t at = 0; at < fields.size(); ++at) {
    CheckField(fields[at], at, argc > 1 ? argv[1] : nullptr);
  }
  return Report();
}
