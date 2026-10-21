#include "RoofContourInternal.h"
#include <expected>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::RoofContour {
namespace {
std::optional<std::vector<Front>> ReadFront(std::span<const Loop> loops,
                                            std::span<const Point> normals,
                                            std::span<const double> intercepts) {
  std::vector<Front> result;
  for (const auto &loop : loops) {
    const auto &points = loop.Points;
    const auto &owners = loop.Owners;
    const size_t count = points.size();
    const size_t offset = result.size();
    if (count < 3 || owners.size() != count || count > kMaximumFrontVertices - offset) {
      return std::nullopt;
    }
    for (size_t at = 0; at < count; ++at) {
      const auto previous = owners[(at + count - 1) % count];
      const auto owner = owners[at];
      if (previous >= normals.size() || owner >= normals.size() || owner >= intercepts.size()) {
        return std::nullopt;
      }
      const auto left = normals[previous];
      const auto right = normals[owner];
      const double determinant = left[0] * right[1] - left[1] * right[0];
      if (std::abs(determinant) < kParallelNormalTolerance && Dot(left, right) < 0) {
        return std::nullopt;
      }
      const auto bisector = Add(left, right);
      const double denominator = Dot(bisector, bisector);
      if (!(denominator > 0)) { return std::nullopt; }
      result.push_back({.At = points[at],
                        .Speed = {2 * bisector[0] / denominator, 2 * bisector[1] / denominator},
                        .Normal = right,
                        .Intercept = intercepts[owner],
                        .Following = offset + (at + 1) % count});
    }
  }
  return result;
}

std::optional<Contact>
VertexContact(const Front &vertex, const Front &edge, const Front &end, Step step) {
  const double gap = Dot(vertex.At, edge.Normal) + edge.Intercept - step.NowM;
  const double relative = Dot(vertex.Speed, edge.Normal) - 1;
  if (gap <= step.ToleranceM || relative >= -kMinimumClosingRate) { return std::nullopt; }
  const double time = -gap / relative;
  if (!(time > 0) || !std::isfinite(time)) { return std::nullopt; }
  const auto target = Add(vertex.At, Scale(vertex.Speed, time));
  const auto start = Add(edge.At, Scale(edge.Speed, time));
  const auto delta = Subtract(Add(end.At, Scale(end.Speed, time)), start);
  const double length2 = Dot(delta, delta);
  const auto distance = Subtract(target, start);
  const double along = Dot(distance, delta);
  const double guard = step.ToleranceM * std::sqrt(length2);
  if (along < -guard || along > length2 + guard ||
      (length2 == 0 && std::hypot(distance[0], distance[1]) > step.ToleranceM)) {
    return std::nullopt;
  }
  return Contact{.Time = time, .At = target};
}

double EdgeContact(const Front &edge, const Front &end) {
  const Point direction{edge.Normal[1], -edge.Normal[0]};
  const double length = Dot(Subtract(end.At, edge.At), direction);
  const double rate = Dot(Subtract(end.Speed, edge.Speed), direction);
  return length > 0 && rate < -kMinimumClosingRate ? -length / rate
                                                   : std::numeric_limits<double>::infinity();
}

}

Checked<CollisionResult> Collision(std::span<const Loop> loops,
                                   std::span<const Point> normals,
                                   std::span<const double> intercepts,
                                   Step step) {
  const auto front = ReadFront(loops, normals, intercepts);
  if (!front || front->empty()) {
    return std::unexpected(Failure{"invalid front or missing collision"});
  }
  double minimum = std::numeric_limits<double>::infinity();
  std::vector<double> collapses;
  collapses.reserve(front->size());
  for (const auto &edge : *front) {
    const double time = EdgeContact(edge, (*front)[edge.Following]);
    collapses.push_back(time);
    minimum = std::min(minimum, time);
  }
  for (const auto &vertex : *front) {
    for (const auto &edge : *front) {
      const auto contact = VertexContact(vertex, edge, (*front)[edge.Following], step);
      if (contact) { minimum = std::min(minimum, contact->Time); }
    }
  }
  if (!std::isfinite(minimum)) {
    return std::unexpected(Failure{"invalid front or missing collision"});
  }
  const double simultaneous =
      64 * std::numeric_limits<double>::epsilon() * std::max(1.0, step.NowM + minimum);
  std::vector<Event> events;
  Ring motions;
  for (size_t at = 0; at < front->size(); ++at) {
    const auto &vertex = (*front)[at];
    motions.push_back(vertex.Speed);
    for (const auto &edge : *front) {
      const auto contact = VertexContact(vertex, edge, (*front)[edge.Following], step);
      if (contact && std::abs(contact->Time - minimum) <= simultaneous) {
        events.push_back({.Participants = {at}, .At = contact->At});
      }
    }
  }
  for (size_t at = 0; at < front->size(); ++at) {
    if (std::abs(collapses[at] - minimum) > simultaneous) { continue; }
    const size_t next = (*front)[at].Following;
    const auto a = (*front)[at].Speed;
    const auto b = (*front)[next].Speed;
    const size_t best = std::hypot(a[0], a[1]) < std::hypot(b[0], b[1]) ? at : next;
    const auto &vertex = (*front)[best];
    events.push_back(
        {.Participants = {at, next}, .At = Add(vertex.At, Scale(vertex.Speed, minimum))});
  }
  return CollisionResult{.AdvanceM = minimum,
                         .PairCount = front->size() * front->size(),
                         .Events = std::move(events),
                         .Motions = std::move(motions)};
}
}
