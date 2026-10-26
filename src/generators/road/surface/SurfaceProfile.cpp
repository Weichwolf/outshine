#include "SurfaceProfile.h"
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
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Point = std::array<double, 3>;
using Identity = std::array<Point, 2>;
using Error = SurfaceProfileError;

struct Crossing {
  Identity Key;
  Point Value;
  double Offset = 0;
};

struct Segment {
  std::array<Crossing, 2> Ends;
  uint32_t Face = 0;
};

double Along(Point point, SurfaceProfilePlane plane) {
  return (point[0] - plane.Center[0]) * plane.Tangent[0] +
         (point[1] - plane.Center[1]) * plane.Tangent[1];
}

double Across(Point point, SurfaceProfilePlane plane) {
  return -(point[0] - plane.Center[0]) * plane.Tangent[1] +
         (point[1] - plane.Center[1]) * plane.Tangent[0];
}

bool Finite(Point point) {
  const double bound = std::sqrt(std::numeric_limits<double>::max()) / 8;
  return std::ranges::all_of(
      point, [bound](double value) { return std::isfinite(value) && std::abs(value) <= bound; });
}

std::optional<Crossing> CrossEdge(Point a, Point b, SurfaceProfilePlane plane) {
  if (b < a) { std::swap(a, b); }
  const double first = Along(a, plane);
  const double last = Along(b, plane);
  if (first == 0) { return Crossing{.Key = {a, a}, .Value = a, .Offset = Across(a, plane)}; }
  if (last == 0) { return Crossing{.Key = {b, b}, .Value = b, .Offset = Across(b, plane)}; }
  if (std::signbit(first) == std::signbit(last)) { return std::nullopt; }
  const double fraction = first / (first - last);
  Point point{};
  for (size_t at = 0; at < point.size(); ++at) { point[at] = a[at] + fraction * (b[at] - a[at]); }
  return Crossing{.Key = {a, b}, .Value = point, .Offset = Across(point, plane)};
}

std::expected<std::optional<Segment>, Error> CrossTriangle(const SurfaceProfileTriangle &triangle,
                                                           uint32_t face,
                                                           SurfaceProfilePlane plane,
                                                           SurfaceProfileWork &work) {
  ++work.TriangleVisits;
  if (!std::ranges::all_of(triangle, Finite)) { return std::unexpected(Error::InvalidInput); }
  const double area = (triangle[1][0] - triangle[0][0]) * (triangle[2][1] - triangle[0][1]) -
                      (triangle[1][1] - triangle[0][1]) * (triangle[2][0] - triangle[0][0]);
  if (!std::isfinite(area) || area == 0) { return std::unexpected(Error::InvalidInput); }
  std::array<Crossing, 3> hits{};
  size_t count = 0;
  for (size_t edge = 0; edge < triangle.size(); ++edge) {
    ++work.EdgeTests;
    const auto hit = CrossEdge(triangle[edge], triangle[(edge + 1) % triangle.size()], plane);
    if (!hit) { continue; }
    const auto previous = std::span(hits).first(count);
    if (std::ranges::find(previous, hit->Key, &Crossing::Key) == previous.end()) {
      hits[count++] = *hit;
    }
  }
  if (count < 2) { return std::nullopt; }
  if (count != 2) { return std::unexpected(Error::InvalidInput); }
  if (hits[1].Offset < hits[0].Offset) { std::swap(hits[0], hits[1]); }
  if (!std::isfinite(hits[0].Offset) || !std::isfinite(hits[1].Offset) ||
      hits[0].Offset >= hits[1].Offset) {
    return std::unexpected(Error::InvalidInput);
  }
  if (hits[1].Offset <= -plane.HalfSpanM || hits[0].Offset >= plane.HalfSpanM) {
    return std::nullopt;
  }
  if (hits[0].Offset <= -plane.HalfSpanM || hits[1].Offset >= plane.HalfSpanM) {
    return std::unexpected(Error::ClippedProfile);
  }
  return Segment{.Ends = {hits[0], hits[1]}, .Face = face};
}

std::expected<void, Error> Assemble(std::span<const Segment> segments,
                                    SurfaceProfilePlane plane,
                                    SurfaceProfileBudget budget,
                                    SurfaceProfile &output,
                                    const std::stop_token &stop) {
  if (segments.empty()) { return std::unexpected(Error::MissingSupport); }
  if (segments.size() + 1 > budget.Points) { return std::unexpected(Error::CapacityExceeded); }
  if (segments.front().Ends[0].Offset >= 0 || segments.back().Ends[1].Offset <= 0) {
    return std::unexpected(Error::MissingSupport);
  }
  output.Points.reserve(std::min(segments.size() + 2, budget.Points));
  output.Faces.reserve(segments.size() + 1);
  output.Points.push_back(segments.front().Ends[0].Value);
  for (size_t at = 0; at < segments.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (at != 0 && segments[at - 1].Ends[1].Key != segments[at].Ends[0].Key) {
      return std::unexpected(segments[at - 1].Ends[1].Offset > segments[at].Ends[0].Offset
                                 ? Error::OverlappingProfile
                                 : Error::DisconnectedProfile);
    }
    output.Points.push_back(segments[at].Ends[1].Value);
    output.Faces.push_back(segments[at].Face);
  }
  const auto found = std::ranges::lower_bound(
      output.Points, 0.0, {}, [plane](Point point) { return Across(point, plane); });
  output.AxisVertex = static_cast<size_t>(found - output.Points.begin());
  if (found == output.Points.end() || output.AxisVertex == 0) {
    return std::unexpected(Error::MissingSupport);
  }
  const double after = Across(*found, plane);
  if (after == 0) { return {}; }
  if (output.Points.size() >= budget.Points) { return std::unexpected(Error::CapacityExceeded); }
  const auto previous = *(found - 1);
  const double before = Across(previous, plane);
  const double fraction = -before / (after - before);
  Point axis{};
  for (size_t at = 0; at < axis.size(); ++at) {
    axis[at] = previous[at] + fraction * ((*found)[at] - previous[at]);
  }
  const auto face = output.Faces[output.AxisVertex - 1];
  output.Points.insert(found, axis);
  output.Faces.insert(output.Faces.begin() + static_cast<ptrdiff_t>(output.AxisVertex), face);
  return {};
}
}

std::expected<SurfaceProfile, SurfaceProfileError>
CutSurfaceProfile(std::span<const SurfaceProfileTriangle> triangles,
                  SurfaceProfilePlane plane,
                  SurfaceProfileBudget budget,
                  const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (triangles.size() > budget.Triangles ||
      triangles.size() >= std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (!Finite({plane.Center[0], plane.Center[1], plane.HalfSpanM}) || plane.HalfSpanM <= 0 ||
      !std::isfinite(plane.Tangent[0]) || !std::isfinite(plane.Tangent[1])) {
    return std::unexpected(Error::InvalidInput);
  }
  const double length = std::hypot(plane.Tangent[0], plane.Tangent[1]);
  if (!std::isfinite(length) || length <= 0) { return std::unexpected(Error::InvalidInput); }
  for (auto &value : plane.Tangent) { value /= length; }
  SurfaceProfile output;
  std::vector<Segment> segments;
  segments.reserve(triangles.size());
  output.Work.ScratchHeapBytes = segments.capacity() * sizeof(Segment);
  for (size_t at = 0; at < triangles.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto crossing =
        CrossTriangle(triangles[at], static_cast<uint32_t>(at), plane, output.Work);
    if (!crossing) { return std::unexpected(crossing.error()); }
    if (*crossing) { segments.push_back(**crossing); }
  }
  std::ranges::sort(segments, [](const Segment &a, const Segment &b) {
    if (a.Ends[0].Offset != b.Ends[0].Offset) { return a.Ends[0].Offset < b.Ends[0].Offset; }
    if (a.Ends[1].Offset != b.Ends[1].Offset) { return a.Ends[1].Offset < b.Ends[1].Offset; }
    return a.Face < b.Face;
  });
  const auto assembled = Assemble(segments, plane, budget, output, stop);
  if (!assembled) { return std::unexpected(assembled.error()); }
  output.Work.OutputHeapBytes =
      output.Points.capacity() * sizeof(Point) + output.Faces.capacity() * sizeof(uint32_t);
  return output;
}
}
