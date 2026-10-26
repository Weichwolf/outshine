#include "RoofContourInternal.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::RoofContour {
namespace {
constexpr double kMinimumClearanceM = 1e-6;

struct ContourSeed {
  std::vector<Loop> Loops;
  size_t SourceVertices;
};

ContourSeed SeedContours(std::span<const Ring> rings) {
  std::vector<Loop> loops;
  size_t source = 0;
  for (size_t ringAt = 0; ringAt < rings.size(); ++ringAt) {
    const auto &ring = rings[ringAt];
    Loop loop;
    loop.Points = ring;
    double area = 0;
    for (size_t at = 0; at < ring.size(); ++at) {
      loop.Owners.push_back(source++);
      const auto a = Subtract(ring[at], ring[0]);
      const auto b = Subtract(ring[(at + 1) % ring.size()], ring[0]);
      area += a[0] * b[1] - a[1] * b[0];
    }
    if ((ringAt == 0 && area < 0) || (ringAt != 0 && area > 0)) {
      std::reverse(loop.Points.begin() + 1, loop.Points.end());
      std::ranges::reverse(loop.Owners);
    }
    loops.push_back(std::move(loop));
  }
  return ContourSeed{.Loops = std::move(loops), .SourceVertices = source};
}

Ring FrontPoints(std::span<const Loop> loops) {
  Ring points;
  for (const auto &loop : loops) {
    points.insert(points.end(), loop.Points.begin(), loop.Points.end());
  }
  return points;
}

using SweptPatches = std::vector<std::vector<std::array<Point, 4>>>;

Checked<Ring>
AdvanceFront(std::span<const Point> before, const CollisionResult &collision, double tolerance) {
  Ring after;
  after.reserve(before.size());
  for (size_t at = 0; at < before.size(); ++at) {
    after.push_back(Add(before[at], Scale(collision.Motions[at], collision.AdvanceM)));
  }
  for (const auto &event : collision.Events) {
    for (const auto at : event.Participants) { after[at] = event.At; }
  }
  return WeldVertices(after, tolerance);
}

struct SweepInput {
  std::span<const Loop> Loops;
  std::span<const Point> Normals, After;
  double NowM, AdvanceM;
};

struct SweepResult {
  std::vector<Loop> Loops;
  size_t Pieces = 0;
};

Checked<SweepResult> SweepFront(const Geos &geometry, SweepInput data, SweptPatches &patches) {
  SweepResult result;
  size_t offset = 0;
  for (const auto &loop : data.Loops) {
    Loop next;
    next.Owners = loop.Owners;
    for (size_t at = 0; at < loop.Points.size(); ++at) {
      next.Points.push_back(data.After[offset + at]);
    }
    for (size_t at = 0; at < loop.Points.size(); ++at) {
      const size_t following = (at + 1) % loop.Points.size();
      const size_t owner = loop.Owners[at];
      const Point tangent{data.Normals[owner][1], -data.Normals[owner][0]};
      const std::array<Point, 4> quad{
          {{Dot(loop.Points[at], tangent), data.NowM},
           {Dot(loop.Points[following], tangent), data.NowM},
           {Dot(next.Points[following], tangent), data.NowM + data.AdvanceM},
           {Dot(next.Points[at], tangent), data.NowM + data.AdvanceM}}};
      const auto polygon = Polygon(geometry, quad);
      double area = 0;
      if (!polygon || GEOSArea_r(geometry.Handle, polygon.get(), &area) == 0) {
        return std::unexpected(Failure{"roof swept measure"});
      }
      if (area > 0) {
        if (GEOSisValid_r(geometry.Handle, polygon.get()) != 1) {
          return std::unexpected(Failure{"roof sweep crossed before contact"});
        }
        patches[owner].push_back(quad);
        ++result.Pieces;
      }
    }
    result.Loops.push_back(std::move(next));
    offset += loop.Points.size();
  }
  return result;
}
}

Checked<Partition> Roof(const Input &input) {
  const auto began = std::chrono::steady_clock::now();
  const Geos geometry;
  if (geometry.Handle == nullptr) { return std::unexpected(Failure{"GEOS context"}); }
  const auto rings = input.Rings;
  const auto normals = input.Normals;
  const auto intercepts = input.Intercepts;
  const double tolerance = input.ToleranceM;
  const size_t vertices = input.Limits.Vertices;
  const size_t eventLimit = input.EventLimit;
  const double seconds = input.Seconds;
  auto seed = SeedContours(rings);
  auto loops = std::move(seed.Loops);
  const size_t source = seed.SourceVertices;
  if (source > input.Limits.Facets) {
    return std::unexpected(Failure{HipRoofError::CapacityExceeded, "source facet budget"});
  }
  if (source != normals.size() || source > vertices || normals.size() != intercepts.size()) {
    return std::unexpected(Failure{HipRoofError::InvalidInput, "roof input shape"});
  }
  std::vector<std::vector<std::array<Point, 4>>> patches(normals.size());
  double now = 0;
  size_t steps = 0;
  size_t tests = 0;
  size_t pieces = 0;
  while (!loops.empty()) {
    if (steps >= eventLimit ||
        std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() > seconds) {
      return std::unexpected(Failure{HipRoofError::CapacityExceeded, "roof event/deadline budget"});
    }
    const auto before = FrontPoints(loops);
    const size_t count = before.size();
    if (count * count > kMaximumCandidates - tests || count > kMaximumWeldVertices - pieces) {
      return std::unexpected(Failure{HipRoofError::CapacityExceeded, "roof pair/piece budget"});
    }
    const auto collision =
        Collision(loops, normals, intercepts, {.NowM = now, .ToleranceM = tolerance});
    if (!collision) { return std::unexpected(collision.error()); }
    const double dt = collision->AdvanceM;
    const auto after = AdvanceFront(before, *collision, tolerance);
    if (!after) { return std::unexpected(after.error()); }
    auto updated = SweepFront(
        geometry,
        {.Loops = loops, .Normals = normals, .After = *after, .NowM = now, .AdvanceM = dt},
        patches);
    if (!updated) { return std::unexpected(updated.error()); }
    pieces += updated->Pieces;
    now += dt;
    auto next =
        Contours(updated->Loops, normals, intercepts, {.NowM = now, .ToleranceM = tolerance});
    if (!next) { return std::unexpected(next.error()); }
    loops = std::move(*next);
    size_t frontVertices = 0;
    for (const auto &loop : loops) { frontVertices += loop.Points.size(); }
    if (frontVertices > vertices * 4) {
      return std::unexpected(Failure{HipRoofError::CapacityExceeded, "roof front budget"});
    }
    ++steps;
    tests += count * count;
  }
  auto result = Assemble(input, patches);
  if (!result) { return std::unexpected(result.error()); }
  result->Steps = steps;
  result->CollisionPairs = tests;
  result->SweptPieces = pieces;
  result->BuildSeconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  return result;
}

Checked<Partition> Prepare(const Input &input) {
  const auto began = std::chrono::steady_clock::now();
  const Geos g;
  if (g.Handle == nullptr) { return std::unexpected(Failure{"GEOS context"}); }
  const auto rings = input.Rings;
  const auto footprint = Footprint(g, rings);
  if (!footprint || GEOSisValid_r(g.Handle, footprint.get()) != 1) {
    return std::unexpected(Failure{HipRoofError::InvalidInput, "invalid footprint"});
  }
  double clearance = 0;
  if (GEOSMinimumClearance_r(g.Handle, footprint.get(), &clearance) != 0 ||
      clearance < kMinimumClearanceM) {
    return std::unexpected(Failure{HipRoofError::InsufficientClearance, "insufficient clearance"});
  }
  auto plan = Roof(input);
  if (!plan) { return std::unexpected(plan.error()); }
  auto refined = RefineRidges(*plan, input);
  if (!refined) { return std::unexpected(refined.error()); }
  const double displacement = *refined;
  const auto valid = ValidatePartition(g, footprint.get(), *plan, input);
  if (!valid) { return std::unexpected(valid.error()); }
  if (std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() >
      input.Seconds) {
    return std::unexpected(Failure{HipRoofError::CapacityExceeded, "final construction deadline"});
  }
  plan->MaximumFinalRefinementM = displacement;
  plan->CompleteSeconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  return plan;
}
}
