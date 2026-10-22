#include "Projection.h"
#include "Receiver.h"
#include "Outward.h"
#include "receiver_coverage/Coverage.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace B = ReceiverBlocks;
namespace C = ReceiverCoverage;
using Json = nlohmann::json;

namespace {
struct Checks {
  uint64_t Count = 0;
  uint64_t Failures = 0;

  void Test(bool valid) {
    ++Count;
    Failures += valid ? 0u : 1u;
  }
};

void Steps(Checks &checks, std::mt19937_64 &random) {
  const auto check = [&](double value) {
    for (const bool positive : {false, true}) {
      const double target = positive ? std::numeric_limits<double>::infinity()
                                     : -std::numeric_limits<double>::infinity();
      const auto expected = std::bit_cast<uint64_t>(std::nextafter(value, target));
      checks.Test(std::bit_cast<uint64_t>(B::NextFinite(value, positive)) == expected);
    }
  };
  for (const double value : {0.,
                             -0.,
                             1.,
                             -1.,
                             std::numeric_limits<double>::min(),
                             -std::numeric_limits<double>::min(),
                             std::numeric_limits<double>::denorm_min(),
                             -std::numeric_limits<double>::denorm_min(),
                             std::numeric_limits<double>::max(),
                             -std::numeric_limits<double>::max()}) {
    check(value);
  }
  constexpr size_t Trials = 1000000;
  for (size_t i = 0; i < Trials; ++i) {
    const auto value = std::bit_cast<double>(static_cast<uint64_t>(random()));
    if (std::isfinite(value)) { check(value); }
  }
}

void Coverage(Checks &checks, std::mt19937_64 &random) {
  for (const uint32_t side : {1u, 63u, 64u, 65u, 129u, 256u, 1024u}) {
    C::RowMask mask(side);
    std::vector<uint8_t> expected(size_t{side} * side);
    constexpr size_t Updates = 160;
    for (size_t step = 0; step < Updates; ++step) {
      const auto rectangle = [&] {
        const auto x = static_cast<uint32_t>(random() % side);
        const auto y = static_cast<uint32_t>(random() % side);
        return C::Rectangle{.Left = x,
                            .Right = std::min(side - 1, x + static_cast<uint32_t>(random() % 9)),
                            .Top = y,
                            .Bottom = std::min(side - 1, y + static_cast<uint32_t>(random() % 9))};
      };
      const auto r = rectangle();
      mask.Mark(r);
      for (uint32_t y = r.Top; y <= r.Bottom; ++y) {
        for (uint32_t x = r.Left; x <= r.Right; ++x) { expected[size_t{y} * side + x] = 1; }
      }
      checks.Test(mask.Covers(r));
      for (size_t at = 0; at < 8; ++at) {
        const auto q = rectangle();
        bool covered = true;
        for (uint32_t y = q.Top; y <= q.Bottom; ++y) {
          for (uint32_t x = q.Left; x <= q.Right; ++x) {
            covered &= expected[size_t{y} * side + x] != 0;
          }
        }
        checks.Test(mask.Covers(q) == covered);
      }
    }
    checks.Test(!mask.Covers({.Left = 0, .Right = side, .Top = 0, .Bottom = 0}));
  }
}

B::Domain Domain(const Json &source) {
  const auto held = source.get<std::array<std::array<double, 2>, 3>>();
  B::Domain result{};
  for (size_t i = 0; i < result.size(); ++i) { result[i] = {.Lo = held[i][0], .Hi = held[i][1]}; }
  return result;
}

Json Project(const Json &source) {
  const auto inverse = source["inverse"].get<B::Matrix>();
  const auto eye = source["eye"].get<std::array<double, 3>>();
  const auto lights = source["lights"].get<std::vector<B::Matrix>>();
  const auto model = B::Prepared::Make(inverse, eye, lights, Domain(source["frame"]));
  Json queries = Json::array();
  std::vector<B::Domain> output(lights.size());
  for (const auto &query : source["queries"]) {
    const bool known = model && model->Project(Domain(query), output);
    Json bounds = Json::array();
    if (known) {
      for (const auto &light : output) {
        Json axes = Json::array();
        for (const auto &axis : light) { axes.push_back({axis.Lo, axis.Hi}); }
        bounds.push_back(axes);
      }
    }
    queries.push_back({{"known", known}, {"bounds", bounds}});
  }
  return {{"name", source["name"]}, {"prepared", model.has_value()}, {"queries", queries}};
}

Json Frame(const Json &source) {
  const B::Camera camera{.Inverse = source["inverse"].get<B::Matrix>(),
                         .Eye = source["eye"].get<std::array<double, 3>>(),
                         .Width = source["width"].get<uint32_t>(),
                         .Height = source["height"].get<uint32_t>()};
  const auto depth = source["depth"].get<std::vector<float>>();
  std::vector<B::Region> regions;
  for (const auto &r : source["regions"]) {
    regions.push_back({.Clip = r["matrix"].get<B::Matrix>(),
                       .X = r["x"].get<uint32_t>(),
                       .Y = r["y"].get<uint32_t>()});
  }
  const uint32_t atlas = source["atlas"].get<uint32_t>();
  C::RowMask mask(atlas / 16);
  B::Costs first;
  B::Costs repeat;
  const bool accepted = B::Append(camera, depth, regions, atlas, mask, first);
  const bool repeated = B::Append(camera, depth, regions, atlas, mask, repeat);
  std::vector<uint8_t> snapshot(size_t{atlas / 16} * (atlas / 16));
  for (size_t i = 0; i < snapshot.size(); ++i) { snapshot[i] = mask.Occupied(i) ? 1u : 0u; }
  return {{"name", source["name"]},
          {"accepted", accepted},
          {"repeated", repeated},
          {"mask", snapshot},
          {"modeled", first.Modeled},
          {"unknown", first.Unknown},
          {"repeat_covered", repeat.Covered},
          {"repeat_word_updates", repeat.WordUpdates}};
}

}

int main(int argc, const char *const *argv) {
  if (argc != 3) { return 2; }
  std::ifstream input(argv[1]);
  const auto description = Json::parse(input, nullptr, false);
  if (description.is_discarded()) { return 2; }
  std::mt19937_64 random(description["seed"].get<uint64_t>());
  Checks steps;
  Checks coverage;
  Steps(steps, random);
  Coverage(coverage, random);
  Json cases = Json::array();
  for (const auto &source : description["cases"]) { cases.push_back(Project(source)); }
  Json frames = Json::array();
  for (const auto &source : description["frames"]) { frames.push_back(Frame(source)); }
  const int original = std::fegetround();
  if (std::fesetround(FE_DOWNWARD) != 0) { return 2; }
  const auto rejected =
      !B::Prepared::Make(description["cases"][0]["inverse"].get<B::Matrix>(),
                         description["cases"][0]["eye"].get<std::array<double, 3>>(),
                         description["cases"][0]["lights"].get<std::vector<B::Matrix>>(),
                         Domain(description["cases"][0]["frame"]));
  if (std::fesetround(original) != 0) { return 2; }
  const Json result{{"qualified_for_page_suppression", false},
                    {"steps", {{"checks", steps.Count}, {"failures", steps.Failures}}},
                    {"coverage", {{"checks", coverage.Count}, {"failures", coverage.Failures}}},
                    {"rounding_mode_rejected", rejected},
                    {"frames", frames},
                    {"cases", cases}};
  std::ofstream output(argv[2]);
  output << result.dump(2) << '\n';
  return output && steps.Failures == 0 && coverage.Failures == 0 && rejected ? 0 : 1;
}
