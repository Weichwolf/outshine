#include "RoadGroundStrip.h"
#include "RoadGroundStripInternal.h"
#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>

namespace outshine::Generators {
namespace {
constexpr double kModelHeightMostM = 1e6;
constexpr size_t kControlsMost = 256;
constexpr size_t kSamplesMost = 4096;

bool ValidTriangleSegments(const RoadGroundStrip &strip) {
  if (strip.Triangles.size() % 2 != 0 || strip.TriangleAxisSegments.empty() ||
      strip.TriangleAxisSegments.size() != strip.Triangles.size() ||
      strip.TriangleAxisSegments.front() != 0) {
    return false;
  }
  const size_t intervals = strip.Triangles.size() / 2;
  for (size_t at = 0; at < strip.TriangleAxisSegments.size(); ++at) {
    const uint32_t segment = strip.TriangleAxisSegments[at];
    if (segment >= intervals || segment != strip.TriangleAxisSegments[at % intervals]) {
      return false;
    }
    if (at != 0 && at < intervals &&
        (segment < strip.TriangleAxisSegments[at - 1] ||
         segment - strip.TriangleAxisSegments[at - 1] > 1)) {
      return false;
    }
  }
  return true;
}

bool ValidStrip(const RoadGroundStrip &strip) {
  const size_t points = strip.TerrainQueries.size();
  if (!RoadGround::Valid(strip.Anchor) || points < 4 || points % 2 != 0 ||
      strip.Boundaries.size() < 4 || strip.Boundaries.size() % 2 != 0 ||
      strip.Boundaries.front() != 0 || strip.Boundaries.back() != points / 2 - 1 ||
      strip.Triangles.size() != points - 2 || strip.FacePatches.size() != strip.Triangles.size() ||
      strip.GradeFrames.size() != strip.Triangles.size() || !ValidTriangleSegments(strip)) {
    return false;
  }
  for (size_t at = 1; at < strip.Boundaries.size(); ++at) {
    if (strip.Boundaries[at] <= strip.Boundaries[at - 1]) { return false; }
  }
  for (size_t at = 0; at < strip.Triangles.size(); ++at) {
    const size_t intervals = points / 2 - 1;
    const auto first = static_cast<uint32_t>(2 * (at % intervals));
    const auto wanted = at < intervals ? std::array{first, first + 2, first + 1}
                                       : std::array{first + 2, first + 3, first + 1};
    const auto owner = static_cast<uint32_t>(
        std::ranges::upper_bound(strip.Boundaries, static_cast<uint32_t>(at % intervals)) -
        strip.Boundaries.begin() - 1);
    if (strip.Triangles[at] != wanted || strip.FacePatches[at] != owner) { return false; }
    for (const double coordinate : strip.GradeFrames[at]) {
      if (!std::isfinite(coordinate)) { return false; }
    }
  }
  return true;
}

size_t Capacity(const RoadGroundField &field) {
  return field.Vertices.capacity() * sizeof(EastNorthUp) +
         field.Triangles.capacity() * sizeof(std::array<uint32_t, 3>) +
         field.TriangleAxisSegments.capacity() * sizeof(uint32_t) +
         field.Planes.capacity() * sizeof(RoadPortField::Plane) +
         field.Patches.capacity() * sizeof(RoadPortField::Patch) +
         field.Priors.capacity() * sizeof(RoadPortPrior) +
         field.Faces.capacity() * sizeof(RoadPortFace);
}

std::expected<void, RoadGroundError> Faces(const RoadGroundStrip &strip, RoadGroundField &field) {
  field.Priors.resize(field.Vertices.size());
  for (size_t at = 0; at < field.Vertices.size(); ++at) {
    const auto point = field.Vertices[at];
    field.Priors[at] = {.Patch = std::numeric_limits<uint32_t>::max(),
                        .At = {.EastM = point.EastM, .NorthM = point.NorthM},
                        .HeightM = point.UpM};
  }
  field.Faces.reserve(strip.Triangles.size());
  for (size_t at = 0; at < strip.Triangles.size(); ++at) {
    RoadPortFace face{
        .Patch = strip.FacePatches[at], .Triangle = {}, .GradeFrame = strip.GradeFrames[at]};
    for (size_t corner = 0; corner < 3; ++corner) {
      const auto index = strip.Triangles[at][corner];
      face.Triangle[corner] = field.Priors[index].At;
      field.Priors[index].Patch = std::min(field.Priors[index].Patch, face.Patch);
    }
    const auto &t = face.Triangle;
    const double area = (t[1].EastM - t[0].EastM) * (t[2].NorthM - t[0].NorthM) -
                        (t[1].NorthM - t[0].NorthM) * (t[2].EastM - t[0].EastM);
    if (!(area > 0) || !std::isfinite(area)) {
      return std::unexpected(RoadGroundError::InvalidGeometry);
    }
    field.Faces.push_back(face);
  }
  if (std::ranges::any_of(field.Priors, [](const auto &prior) {
        return prior.Patch == std::numeric_limits<uint32_t>::max();
      })) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  return {};
}
}

RoadPortFieldFitInput RoadGroundField::FitInput(double maximumPriorChangeM) const noexcept {
  return {.Planes = Planes,
          .Patches = Patches,
          .Priors = Priors,
          .Faces = Faces,
          .MaximumPriorChangeM = maximumPriorChangeM};
}

std::expected<RoadGroundField, RoadGroundError>
BindRoadGroundField(const RoadGroundStrip &strip,
                    std::span<const double> modelHeightsM,
                    RoadGroundBudget budget,
                    const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
  if (strip.TerrainQueries.size() != modelHeightsM.size()) {
    return std::unexpected(RoadGroundError::MissingTerrain);
  }
  const size_t controls = strip.Boundaries.size() / 2;
  if (controls > std::min(budget.Controls, kControlsMost) ||
      modelHeightsM.size() > std::min(budget.TerrainSamples, kSamplesMost) ||
      modelHeightsM.size() >
          budget.ContainerBytes / (sizeof(EastNorthUp) + sizeof(RoadPortPrior))) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  if (!ValidStrip(strip)) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  const size_t capacity =
      modelHeightsM.size() * (sizeof(EastNorthUp) + sizeof(RoadPortPrior)) +
      strip.Triangles.size() *
          (sizeof(std::array<uint32_t, 3>) + sizeof(uint32_t) + sizeof(RoadPortFace)) +
      (2 * controls - 1) * sizeof(RoadPortField::Patch) +
      controls * (sizeof(RoadPortField::Plane) + 2 * sizeof(uint32_t) + sizeof(size_t));
  if (capacity > budget.ContainerBytes) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  RoadGroundField field{.Anchor = strip.Anchor,
                        .Vertices = {},
                        .Triangles = {},
                        .TriangleAxisSegments = {},
                        .Planes = {},
                        .Patches = {},
                        .Priors = {},
                        .Faces = {},
                        .Work = strip.Work};
  const auto frame = TangentFrame::At(strip.Anchor);
  field.Vertices.reserve(modelHeightsM.size());
  for (size_t at = 0; at < modelHeightsM.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
    const double height = modelHeightsM[at];
    if (!std::isfinite(height) || std::abs(height) > kModelHeightMostM ||
        !RoadGround::Valid(strip.TerrainQueries[at])) {
      return std::unexpected(RoadGroundError::MissingTerrain);
    }
    const auto geographic = strip.TerrainQueries[at];
    field.Vertices.push_back(frame.ToLocalPosition({.LongitudeDeg = geographic.LongitudeDeg,
                                                    .LatitudeDeg = geographic.LatitudeDeg,
                                                    .HeightM = height}));
  }
  field.Triangles = strip.Triangles;
  field.TriangleAxisSegments = strip.TriangleAxisSegments;
  const auto faces = Faces(strip, field);
  if (!faces) { return std::unexpected(faces.error()); }
  const auto layout = RoadGround::Layout(strip, field);
  if (!layout) { return std::unexpected(layout.error()); }
  field.Work.ContainerCapacityBytes = Capacity(field);
  if (field.Work.ContainerCapacityBytes > budget.ContainerBytes) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  return field;
}
}
