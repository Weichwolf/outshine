#include "HipRoofCodec.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <expected>
#include <fstream>
#include <memory>
#include <ratio>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using Json = nlohmann::json;
using Ring = std::vector<EastNorth>;
using Clock = std::chrono::steady_clock;

Ring ReadRing(const Json &value) {
  Ring result;
  for (const auto &point : value) {
    result.push_back({.EastM = point[0].get<double>(), .NorthM = point[1].get<double>()});
  }
  return result;
}

std::vector<Ring> Rectangles() {
  std::vector<Ring> result;
  for (int at = 0; at < 32; ++at) {
    const double a = 1 + at % 7;
    const double b = 1 + at % 5;
    const double x = 100000 + 13 * at;
    const double y = -200000 - 7 * at;
    Ring ring{{.EastM = x, .NorthM = y},
              {.EastM = x + 3 * a, .NorthM = y + 4 * a},
              {.EastM = x + 3 * a - 4 * b, .NorthM = y + 4 * a + 3 * b},
              {.EastM = x - 4 * b, .NorthM = y + 3 * b}};
    result.push_back(ring);
    std::ranges::reverse(ring);
    result.push_back(std::move(ring));
  }
  return result;
}

Json EncodedPlan(const Ring &outer, const std::vector<Ring> &holes, const std::string &id) {
  auto plan = HipRoofPlan::Prepare(outer, holes, {});
  const auto product =
      std::make_shared<const std::expected<HipRoofPlan, HipRoofError>>(std::move(plan));
  StructureBinary::Writer writer;
  if (!HipRoofCodec::Fields(writer, product)) { return {{"id", id}, {"codec_valid", false}}; }
  constexpr std::string_view digits = "0123456789abcdef";
  std::string hex;
  hex.reserve(2 * writer.Bytes.size());
  for (const auto byte : writer.Bytes) {
    hex.push_back(digits[byte >> 4]);
    hex.push_back(digits[byte & 15]);
  }
  return {{"id", id}, {"valid", product->has_value()}, {"codec_valid", true}, {"bytes", hex}};
}

double Milliseconds(Clock::time_point began) {
  return std::chrono::duration<double, std::milli>(Clock::now() - began).count();
}

Json Measure(const std::vector<Ring> &rectangles, size_t repetitions) {
  const auto coldAt = Clock::now();
  const auto cold = HipRoofPlan::Prepare(rectangles.front(), {}, {});
  const double coldMs = Milliseconds(coldAt);
  double checksum = 0;
  size_t refused = 0;
  const auto warmAt = Clock::now();
  for (size_t at = 0; at < repetitions; ++at) {
    const auto plan = HipRoofPlan::Prepare(rectangles[at % rectangles.size()], {}, {});
    if (plan) {
      checksum += plan->MaximumDistanceM();
    } else {
      ++refused;
    }
  }
  return {{"repetitions", repetitions},
          {"cold_valid", cold.has_value()},
          {"cold_ms", coldMs},
          {"warm_total_ms", Milliseconds(warmAt)},
          {"refused", refused},
          {"checksum", checksum}};
}
}

int main(int argc, char *const *argv) {
  if (argc != 4) { return 2; }
  constexpr size_t kMostRepetitions = 100000;
  size_t repetitions = 0;
  const std::string_view argument(argv[3]);
  const auto [end, error] =
      std::from_chars(argument.data(), argument.data() + argument.size(), repetitions);
  if (error != std::errc{} || end != argument.data() + argument.size() || repetitions == 0 ||
      repetitions > kMostRepetitions) {
    return 2;
  }
  std::ifstream input(argv[1]);
  const auto fixture = Json::parse(input, nullptr, false);
  if (fixture.is_discarded()) { return 3; }
  const auto rectangles = Rectangles();
  Json output{{"rectangle_cost", Measure(rectangles, repetitions)}, {"plans", Json::array()}};
  for (const auto &record : fixture.at("records")) {
    std::vector<Ring> holes;
    for (const auto &hole : record.at("holes")) { holes.push_back(ReadRing(hole)); }
    output["plans"].push_back(
        EncodedPlan(ReadRing(record.at("outer")), holes, record.at("id").get<std::string>()));
  }
  for (size_t at = 0; at < rectangles.size(); ++at) {
    output["plans"].push_back(EncodedPlan(rectangles[at], {}, "rectangle:" + std::to_string(at)));
  }
  std::ofstream file(argv[2]);
  file << output.dump(2) << '\n';
  if (!file) { return 4; }
  return output["rectangle_cost"]["refused"] == 0 ? 0 : 5;
}
