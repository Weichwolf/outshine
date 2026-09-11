#include "TreeSpecies.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace {
std::string Definition(std::string_view field, std::string_view value) {
  return "{\"name\":\"replacement\",\"" + std::string(field) + "\":" + std::string(value) + "}";
}
}

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  TreeSpecies species;
  const std::string initial = R"({"name":"retained","height_m":10,"seed":17,"trunk_steps":20})";
  CHECK(species.Parse(initial.data(), initial.size()), "initial species loads");
  const auto rejects = [&](std::string_view field, std::string_view value) {
    const auto source = Definition(field, value);
    CHECK(!species.Parse(source.data(), source.size()), "invalid numeric declaration is rejected");
    CHECK(species.Error().find(field) != std::string::npos, "error identifies the offending field");
    CHECK(species.Definition() == initial && species.Name() == "retained" &&
              species.GrowthParams().Seed == 17 && species.GrowthParams().TrunkSteps == 20 &&
              species.HeightM() == 10,
          "failed parse preserves the previous definition and parameters");
  };
  for (const auto *field : {"leaders",
                            "trunk_steps",
                            "trunk_sides",
                            "max_order",
                            "whorl_count",
                            "whorl_spacing",
                            "leaf_cards",
                            "leaf_card_budget",
                            "bark_style",
                            "seed"}) {
    for (const auto *bad : {"0.5", "1e100", "\"2\"", "null", "true"}) { rejects(field, bad); }
  }
  for (const auto *field : {"height_m",
                            "spread_m",
                            "height_sigma",
                            "dbh_cm",
                            "lai",
                            "step_len",
                            "base_radius",
                            "taper",
                            "twig_radius",
                            "branch_chance",
                            "order_len",
                            "foliage_factor",
                            "leaf_card_w",
                            "bark_r",
                            "wind_freq"}) {
    for (const auto *bad : {"1e100", "-1e100", "\"0.5\"", "null", "false"}) { rejects(field, bad); }
  }
  for (const auto *field : {"leaders", "trunk_sides", "whorl_count"}) { rejects(field, "65"); }
  for (const auto *field : {"trunk_steps", "whorl_spacing"}) {
    rejects(field, "0");
    rejects(field, "4097");
  }
  rejects("leaders", "0");
  rejects("trunk_sides", "2");
  rejects("whorl_count", "-1");
  rejects("max_order", "9");
  rejects("seed", "-1");
  rejects("seed", "4294967296");
  for (const auto *field : {"bole_frac", "break_frac", "order_len"}) {
    rejects(field, "-0.1");
    rejects(field, "1.1");
  }
  for (uint32_t seed : std::array<uint32_t, 4>{0, 2147483647u, 2147483648u, UINT32_MAX}) {
    const auto source = Definition("seed", std::to_string(seed));
    CHECK(species.Parse(source.data(), source.size()), "the full unsigned seed range is accepted");
    CHECK(species.GrowthParams().Seed == seed, "seed bits survive without signed conversion");
  }
  const std::string edge =
      R"({"name":"limits","leaders":64,"trunk_sides":64,"trunk_steps":4096,"whorl_count":64,"whorl_spacing":4096,"max_order":8,"bole_frac":1,"break_frac":1,"order_len":1})";
  CHECK(species.Parse(edge.data(), edge.size()), "explicit growth input limits are inclusive");
  size_t profiles = 0;
  for (const auto &entry : std::filesystem::directory_iterator("src/assets/world/species")) {
    if (entry.path().extension() != ".json") { continue; }
    std::ifstream file(entry.path());
    const std::string source{std::istreambuf_iterator<char>(file),
                             std::istreambuf_iterator<char>()};
    CHECK(species.Parse(source.data(), source.size()), entry.path().filename().string().c_str());
    ++profiles;
  }
  CHECK(profiles > 0, "the shipped species corpus was exercised");
  return Report();
}
