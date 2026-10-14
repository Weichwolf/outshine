#include "Check.h"
#include "Json.h"
#include "LeafOpticsTable.h"
#include "LeafOpticsDefaults.h"
#include "ReadTextFile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <print>
#include <vector>

namespace {
void CheckOwnedTable() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto source =
      ReadTextFile("lib/src/outshine_ref/vegetation/data/leaf-optics-table-1.json", 1024u * 1024u);
  CHECK(source.has_value(), "the runtime table has one owned canonical reference input");
  if (!source) { return; }
  Json document;
  CHECK(document.Parse(source->data(), source->size()), "owned raw table parses independently");
  if (!document.Ok()) { return; }
  const auto values = document.Root()["values"];
  const auto &native = DefaultLeafOptics();
  CHECK(native.Side() == values.Size() && native.Side() == 33,
        "native runtime keeps both owned pigment axes");
  CHECK(native.Limits() == LeafOpticsTable::kPigmentLimitsUnsaid,
        "native runtime keeps the owned physical domain");
  for (size_t x = 0; x < native.Side(); ++x) {
    for (size_t y = 0; y < native.Side(); ++y) {
      for (size_t channel = 0; channel < 6; ++channel) {
        CHECK(native.Values()[x * native.Side() + y][channel] == values[x][y][channel].Num(),
              "native preparation preserves every raw gamut channel without preclamping or "
              "transposition");
      }
    }
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto text =
      ReadTextFile("test/experiments/vegetation/reference/leaf-optics-native.json", 128u * 1024u);
  CHECK(text.has_value(), "the unchanged Python reference input is available");
  if (!text) { return Report(); }
  Json document;
  CHECK(document.Parse(text->data(), text->size()), "the reference product parses");
  if (!document.Ok()) { return Report(); }
  const auto root = document.Root();
  const auto recipe = root["recipe"];
  CHECK(recipe["version"].StrEquals("leaf-optics-table-1") &&
            recipe["colour_space"].StrEquals("linear-srgb"),
        "reference values carry the actual unlit R/T schema");
  const auto values = recipe["values"];
  const size_t side = values.Size();
  std::vector<LeafOpticsTable::Spectrum> samples;
  for (size_t x = 0; x < side; ++x) {
    CHECK(values[x].Size() == side, "reference table has matching pigment axes");
    for (size_t y = 0; y < side; ++y) {
      LeafOpticsTable::Spectrum sample;
      CHECK(values[x][y].Size() == sample.size(), "raw table keeps all six direct R/T channels");
      for (size_t channel = 0; channel < sample.size(); ++channel) {
        sample[channel] = values[x][y][channel].Num();
      }
      samples.push_back(sample);
    }
  }
  const auto limits = recipe["limits"];
  const auto table =
      LeafOpticsTable::Create(side, samples, {limits[size_t{0}].Num(), limits[size_t{1}].Num()});
  CHECK(table.has_value(), "Python's immutable prepared table satisfies native boundaries");
  if (!table) { return Report(); }
  const auto queries = root["pigments"];
  const auto expected = root["expected"];
  CHECK(queries.Size() > 0 && expected.Size() == queries.Size(),
        "every unchanged input has a published reference response");
  double worst = 0;
  for (size_t query = 0; query < queries.Size(); ++query) {
    const auto actual =
        table->Sample({queries[query][size_t{0}].Num(), queries[query][size_t{1}].Num()});
    CHECK(actual.has_value(), "prepared native pigment lookup remains within the reference domain");
    if (!actual) { continue; }
    for (size_t channel = 0; channel < actual->size(); ++channel) {
      const double want = expected[query][channel].Num();
      worst = std::max(worst, std::abs((*actual)[channel] - want));
      CHECK_NEAR((*actual)[channel],
                 want,
                 1e-12,
                 "linear R/T",
                 "native lookup and gamut reduction match the unchanged Python reference");
    }
  }
  std::println("LEAF_OPTICS cases={} maximum_channel_error={:.17g}", queries.Size(), worst);
  CheckOwnedTable();
  return Report();
}
