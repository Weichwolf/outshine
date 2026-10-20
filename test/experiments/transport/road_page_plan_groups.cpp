#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <fstream>
#include <limits>
#include <map>
#include <new>
#include <numeric>
#include <span>
#include <ratio>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

namespace {
using Json = nlohmann::json;
using Point = std::array<double, 2>;
using Part = std::vector<Point>;
using Groups = std::vector<std::vector<size_t>>;
using Clock = std::chrono::steady_clock;

struct Allocations {
  size_t Calls = 0;
  size_t RequestedBytes = 0;
};

bool Observing = false;
Allocations Allocated;

std::expected<std::vector<Part>, std::string> ReadParts(const Json &input) {
  if (!input.is_object() || !input.contains("parts") || !input["parts"].is_array()) {
    return std::unexpected("input needs a parts array containing only captured plan points");
  }
  std::vector<Part> parts;
  for (const auto &part : input["parts"]) {
    if (!part.is_object() || !part.contains("points") || !part["points"].is_array() ||
        part["points"].empty()) {
      return std::unexpected("every part needs nonempty plan stations or junction gates");
    }
    Part points;
    for (const auto &point : part["points"]) {
      if (!point.is_array() || point.size() != 2 || !point[0].is_number() ||
          !point[1].is_number()) {
        return std::unexpected("a plan point needs two finite coordinates");
      }
      const Point value{point[0].get<double>(), point[1].get<double>()};
      if (!std::isfinite(value[0]) || !std::isfinite(value[1])) {
        return std::unexpected("a plan point is not finite");
      }
      points.push_back(value);
    }
    parts.push_back(std::move(points));
  }
  return parts;
}

std::expected<std::pair<int64_t, int64_t>, std::string> Cell(std::span<const Point> points,
                                                             double width) {
  if (points.empty()) { return std::unexpected("an empty part has no packing position"); }
  Point low = points.front();
  Point high = points.front();
  for (const auto &point : points) {
    for (size_t axis = 0; axis != 2; ++axis) {
      if (!std::isfinite(point[axis])) { return std::unexpected("nonfinite plan point"); }
      low[axis] = std::min(low[axis], point[axis]);
      high[axis] = std::max(high[axis], point[axis]);
    }
  }
  std::array<int64_t, 2> cell{};
  for (size_t axis = 0; axis != 2; ++axis) {
    const double value = std::floor(std::midpoint(low[axis], high[axis]) / width);
    constexpr auto lower = static_cast<double>(std::numeric_limits<int64_t>::min());
    if (!std::isfinite(value) || value < lower || value >= -lower) {
      return std::unexpected("plan cell exceeds its signed coordinate range");
    }
    cell[axis] = static_cast<int64_t>(value);
  }
  return std::pair{cell[0], cell[1]};
}

std::expected<Groups, std::string>
Partition(std::span<const Part> parts, double width, size_t most) {
  if (!std::isfinite(width) || width <= 0 || most == 0) {
    return std::unexpected("packing needs a positive finite cell width and nonzero part limit");
  }
  std::map<std::pair<int64_t, int64_t>, std::vector<size_t>> cells;
  for (size_t index = 0; index != parts.size(); ++index) {
    const auto cell = Cell(parts[index], width);
    if (!cell) { return std::unexpected(cell.error()); }
    cells[*cell].push_back(index);
  }
  Groups result;
  for (const auto &[cell, indices] : cells) {
    for (size_t first = 0; first != indices.size();) {
      const auto count = std::min(most, indices.size() - first);
      const auto selected = std::span(indices).subspan(first, count);
      result.emplace_back(selected.begin(), selected.end());
      first += count;
    }
  }
  std::ranges::sort(result, [](const auto &a, const auto &b) { return a.front() < b.front(); });
  return result;
}

Groups Sequential(size_t count, size_t most) {
  Groups result;
  for (size_t first = 0; first != count;) {
    const auto size = std::min(most, count - first);
    result.emplace_back(size);
    std::iota(result.back().begin(), result.back().end(), first);
    first += size;
  }
  return result;
}

bool Controls() {
  const std::vector<Part> parts{{{{-1, -1}}}, {{{0, 0}}}, {{{256, 0}}}, {{{255, 0}}}};
  const Groups expected{{0}, {1, 3}, {2}};
  const auto grouped = Partition(parts, 256, 64);
  auto invalid = parts;
  invalid.back().clear();
  auto nonfinite = parts;
  nonfinite[0][0][0] = std::numeric_limits<double>::infinity();
  const std::vector<Part> huge{{{{std::numeric_limits<double>::max(), 0}}}};
  const std::vector<Part> same(65, Part{{0, 0}});
  const auto split = Partition(same, 256, 64);
  return grouped && *grouped == expected && !Partition(parts, 0, 64) && !Partition(parts, 256, 0) &&
         !Partition(invalid, 256, 64) && !Partition(nonfinite, 256, 64) &&
         !Partition(huge, 1, 64) && split && split->size() == 2 && split->front().size() == 64 &&
         split->back().size() == 1;
}

struct Case {
  std::string_view Name;
  bool Spatial = false;
  size_t Most = 64;
};

std::expected<Json, std::string> Measure(std::span<const Part> parts, Case test) {
  Json rounds = Json::array();
  Groups previous;
  for (size_t round = 0; round != 5; ++round) {
    Allocated = {};
    Observing = true;
    const auto began = Clock::now();
    auto groups = test.Spatial
                      ? Partition(parts, 256, test.Most)
                      : std::expected<Groups, std::string>(Sequential(parts.size(), test.Most));
    const auto milliseconds =
        std::chrono::duration<double, std::milli>(Clock::now() - began).count();
    Observing = false;
    const auto allocations = Allocated;
    if (!groups) { return std::unexpected(groups.error()); }
    if (round != 0 && *groups != previous) { return std::unexpected("unstable partition"); }
    previous = std::move(*groups);
    rounds.push_back({{"ms", milliseconds},
                      {"new_calls", allocations.Calls},
                      {"new_requested_bytes", allocations.RequestedBytes}});
  }
  std::vector<size_t> ownership(parts.size(), 0);
  for (const auto &group : previous) {
    for (const auto part : group) { ++ownership.at(part); }
  }
  if (!std::ranges::all_of(ownership, [](size_t count) { return count == 1; })) {
    return std::unexpected("every original part must have exactly one owner");
  }
  return Json{{"name", test.Name}, {"groups", std::move(previous)}, {"rounds", std::move(rounds)}};
}

std::expected<Json, std::string> Compare(std::span<const Part> parts) {
  Json results = Json::array();
  for (const Case test : {Case{.Name = "sequential64", .Spatial = false, .Most = 64},
                          Case{.Name = "plan256", .Spatial = true, .Most = parts.size()},
                          Case{.Name = "plan256x64", .Spatial = true, .Most = 64}}) {
    auto one = Measure(parts, test);
    if (!one) { return std::unexpected(one.error()); }
    results.push_back(std::move(*one));
  }
  return results;
}
}

extern "C" void *RealNew(size_t bytes) asm("__real__Znwm");
extern "C" void *ObserveNew(size_t bytes) asm("__wrap__Znwm");

extern "C" void *ObserveNew(size_t bytes) {
  if (Observing) {
    ++Allocated.Calls;
    Allocated.RequestedBytes += bytes;
  }
  return RealNew(bytes);
}

int main(int argc, char **argv) {
  if (argc != 3 || !Controls()) { return 2; }
  constexpr size_t kCalibrationBytes = 37;
  Allocated = {};
  Observing = true;
  void *calibration = ::operator new(kCalibrationBytes);
  Observing = false;
  ::operator delete(calibration);
  if (Allocated.Calls != 1 || Allocated.RequestedBytes != kCalibrationBytes) { return 8; }
  std::ifstream input(argv[1]);
  if (!input) { return 3; }
  const auto json = Json::parse(input, nullptr, false);
  if (json.is_discarded()) { return 4; }
  const auto began = Clock::now();
  const auto parts = ReadParts(json);
  const auto inputMs = std::chrono::duration<double, std::milli>(Clock::now() - began).count();
  if (!parts || parts->empty()) { return 5; }
  const auto compared = Compare(*parts);
  if (!compared) { return 6; }
  size_t points = 0;
  for (const auto &part : *parts) { points += part.size(); }
  std::ofstream output(argv[2]);
  output << Json({{"parts", parts->size()},
                  {"points", points},
                  {"input_conversion_ms", inputMs},
                  {"results", *compared},
                  {"scope",
                   "Plan stations/gates only; every spatial round pays bounds, cell lookup, "
                   "grouping and ordering. JSON parsing/conversion are outside grouping. "
                   "Ordinary C++ new requests exclude allocator/C/OS/GPU peaks. "
                   "No fine mesh, visibility, native world integration or FPS approval."}})
                .dump(2)
         << '\n';
  return output ? 0 : 7;
}
