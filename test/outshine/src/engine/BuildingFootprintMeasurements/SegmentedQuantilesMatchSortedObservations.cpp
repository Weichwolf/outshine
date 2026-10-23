#include "BuildingField.h"
#include "BuildingFootprintMeasurements.h"
#include "DiagnosticLedger.h"
#include "Check.h"
#include "math/Quantile.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using Field = outshine::Generators::Osm::BuildingField;

void Accept(Field &field, uint32_t tile, std::span<const double> sample) {
  const Field::Baked baked{
      .Coordinates = {}, .Prints = {}, .SeatSpreadM = sample, .AcrossM = sample};
  field.Take(tile);
  auto pending = field.PrepareAcceptance(tile, baked);
  field.CommitAcceptance(std::move(pending), baked);
}

const outshine::DiagnosticSample *Find(const outshine::Core::DiagnosticLedger &report,
                                       std::string_view name) {
  const auto found = std::ranges::find(report.Samples(), name, &outshine::DiagnosticSample::Name);
  return found != report.Samples().end() ? &*found : nullptr;
}

void CheckRanks(std::span<const double> values) {
  using namespace outshine;
  using namespace outshine::Test;
  Field field;
  const size_t middle = values.size() / 2;
  Accept(field, 9, values.subspan(middle));
  Accept(field, 5, {});
  Accept(field, 2, values.first(middle));
  std::vector<double> sorted(values.begin(), values.end());
  std::ranges::sort(sorted);
  const size_t heap = field.HeapBytes();
  Core::DiagnosticLedger report;
  Core::ReportBuildingFootprintMeasurements(report, field, true);

  struct Rank {
    const char *Name;
    double Share;
  };

  constexpr std::array ranks{
      Rank{.Name = "buildings: a stamp would fill, p50", .Share = kMiddleQuantile},
      Rank{.Name = "buildings: a stamp would fill, p95", .Share = kBroadQuantile},
      Rank{.Name = "buildings: footprint across, p50", .Share = kMiddleQuantile},
      Rank{.Name = "buildings: footprint across, p05", .Share = kNarrowQuantile}};
  for (const auto &rank : ranks) {
    const auto expected = QuantileOf(sorted, rank.Share);
    const auto *actual = Find(report, rank.Name);
    CHECK(expected && actual && actual->Value == *expected,
          "read-only segmented selection preserves exact nearest ranks across tiles");
  }
  CHECK(field.HeapBytes() == heap && report.Samples().size() == 8,
        "exact diagnostics do not retain a flattened sample array or mutate field storage");
  Core::DiagnosticLedger optional;
  Core::ReportBuildingFootprintMeasurements(optional, field, false);
  CHECK(optional.Samples().size() == 4 && !Find(optional, ranks.front().Name),
        "ordinary diagnostics omit optional rank work while retaining thresholds and extrema");
  const auto *narrowest = Find(optional, "buildings: and the narrowest of them");
  const auto *worst = Find(optional, "buildings: a stamp would fill, worst");
  CHECK(narrowest && worst && narrowest->Value == sorted.front() && worst->Value == sorted.back(),
        "segmented extrema agree with original sorted observations");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double kInfinity = std::numeric_limits<double>::infinity();
  const std::array extremes{-kInfinity,
                            -9.0,
                            -0.0,
                            0.0,
                            std::numeric_limits<double>::denorm_min(),
                            1.0,
                            1.0,
                            25.0,
                            kInfinity};
  CheckRanks(extremes);
  const std::array<double, 20> boundary{20, 19, 18, 17, 16, 15, 14, 13, 12, 11,
                                        10, 9,  8,  7,  6,  5,  4,  3,  2,  1};
  CheckRanks(boundary);
  const std::array<double, 5> duplicates{4, 4, 4, 4, 4};
  CheckRanks(duplicates);
  const std::array<double, 1> single{0};
  CheckRanks(single);
  Field empty;
  Core::DiagnosticLedger report;
  Core::ReportBuildingFootprintMeasurements(report, empty, true);
  Accept(empty, 7, {});
  Core::ReportBuildingFootprintMeasurements(report, empty, true);
  CHECK(report.Samples().empty(), "empty tile samples invent no extrema or quantiles");
  return Report();
}
