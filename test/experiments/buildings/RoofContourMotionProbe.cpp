#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
using Json = nlohmann::json;
using Point = std::array<double, 2>;
constexpr size_t kMaximumFrontVertices = 1024;
constexpr size_t kMaximumProbeStates = 10000;
constexpr double kParallelNormalTolerance = 1e-12;
constexpr double kMinimumClosingRate = 1e-12;

struct Step {
  double NowM, ToleranceM;
};

Point Add(Point a, Point b) {
  return {a[0] + b[0], a[1] + b[1]};
}

Point Subtract(Point a, Point b) {
  return {a[0] - b[0], a[1] - b[1]};
}

Point Scale(Point p, double factor) {
  return {p[0] * factor, p[1] * factor};
}

double Dot(Point a, Point b) {
  return a[0] * b[0] + a[1] * b[1];
}

struct Front {
  Point At, Speed, Normal;
  double Intercept;
  size_t Following;
};

std::optional<std::vector<Front>> ReadFront(const Json &input) {
  const auto normals = input.at("normals").get<std::vector<Point>>();
  const auto intercepts = input.at("intercepts").get<std::vector<double>>();
  std::vector<Front> result;
  for (const auto &loop : input.at("loops")) {
    const auto points = loop.at("points").get<std::vector<Point>>();
    const auto owners = loop.at("ids").get<std::vector<size_t>>();
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

struct Contact {
  double Time;
  Point At;
};

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

Json Collision(const Json &input) {
  const auto front = ReadFront(input);
  if (!front || front->empty()) { return {{"valid", false}}; }
  const Step step{.NowM = input.at("now").get<double>(),
                  .ToleranceM = input.at("tolerance").get<double>()};
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
  if (!std::isfinite(minimum)) { return {{"valid", false}}; }
  const double simultaneous =
      64 * std::numeric_limits<double>::epsilon() * std::max(1.0, step.NowM + minimum);
  Json events = Json::array();
  Json motions = Json::array();
  for (size_t at = 0; at < front->size(); ++at) {
    const auto &vertex = (*front)[at];
    motions.push_back(vertex.Speed);
    for (const auto &edge : *front) {
      const auto contact = VertexContact(vertex, edge, (*front)[edge.Following], step);
      if (contact && std::abs(contact->Time - minimum) <= simultaneous) {
        events.push_back({{"participants", {at}}, {"point", contact->At}});
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
        {{"participants", {at, next}}, {"point", Add(vertex.At, Scale(vertex.Speed, minimum))}});
  }
  return {{"valid", true},
          {"advance_m", minimum},
          {"pair_count", front->size() * front->size()},
          {"events", events},
          {"motions", motions}};
}
}

int main(int argc, char *const *argv) {
  if (argc != 3) { return 2; }
  std::ifstream source(argv[1]);
  const auto input = Json::parse(source, nullptr, false);
  if (input.is_discarded() || !input.is_array() || input.size() > kMaximumProbeStates) { return 3; }
  Json results = Json::array();
  const auto began = std::chrono::steady_clock::now();
  for (const auto &row : input) {
    results.push_back({{"id", row.at("id")}, {"collision", Collision(row)}});
  }
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  std::ofstream(argv[2])
      << Json({{"results", results}, {"seconds_including_json", seconds}}).dump();
}

extern "C" const char *OutshineRoofCollision(const char *text) {
  thread_local std::string output;
  const auto input = Json::parse(text, nullptr, false);
  if (input.is_discarded()) { return nullptr; }
  output = Collision(input).dump();
  return output.c_str();
}
