#include "BuildingFootprintMeasurements.h"
#include "BuildingPhysicalProduct.h"

#include "BuildingField.h"
#include "DiagnosticLedger.h"
#include "GroundMesher.h"
#include "Quantile.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

namespace outshine::Core {
namespace {
using Field = ::outshine::Generators::Osm::BuildingField;
using Samples = std::vector<double> Ground::BuildingPhysicalProduct::*;
constexpr uint64_t kSignBit = uint64_t{1} << 63u;
constexpr double kGroundCellM = 25.0;
static_assert(std::numeric_limits<double>::is_iec559);

struct Summary {
  size_t Count = 0, Below = 0, Above = 0;
  double Minimum = std::numeric_limits<double>::infinity();
  double Maximum = -std::numeric_limits<double>::infinity();
};

template <typename Visit> void Each(const Field &field, Samples samples, Visit visit) {
  for (size_t at = 0; at < field.AcceptedTiles().size(); ++at) {
    for (const double value : field.ProductAt(at).*samples) { visit(value); }
  }
}

Summary Describe(const Field &field, Samples samples, double threshold) {
  Summary summary;
  Each(field, samples, [&](double value) {
    ++summary.Count;
    summary.Below += value < threshold;
    summary.Above += value > threshold;
    summary.Minimum = std::min(summary.Minimum, value);
    summary.Maximum = std::max(summary.Maximum, value);
  });
  return summary;
}

uint64_t Ordered(double value) noexcept {
  const auto bits = std::bit_cast<uint64_t>(value);
  return (bits & kSignBit) != 0 ? ~bits : bits ^ kSignBit;
}

double Quantile(const Field &field, Samples samples, const Summary &summary, double share) {
  const double at = static_cast<double>(summary.Count) * share;
  const auto rank = static_cast<size_t>(std::ceil(at));
  const size_t target = rank > 0 ? rank - 1 : 0;
  uint64_t low = Ordered(summary.Minimum);
  uint64_t high = Ordered(summary.Maximum);
  while (low < high) {
    const uint64_t middle = std::midpoint(low, high);
    uint64_t below = low;
    uint64_t above = high;
    size_t count = 0;
    Each(field, samples, [&](double value) {
      const uint64_t key = Ordered(value);
      if (key <= middle) {
        ++count;
        below = std::max(below, key);
      } else {
        above = std::min(above, key);
      }
    });
    if (count <= target) {
      low = above;
    } else {
      high = below;
    }
  }
  return std::bit_cast<double>((low & kSignBit) != 0 ? low ^ kSignBit : ~low);
}
}

void ReportBuildingFootprintMeasurements(DiagnosticLedger &report,
                                         const Field &field,
                                         bool quantiles) {
  constexpr Samples kFill = &Ground::BuildingPhysicalProduct::SeatSpreadM;
  constexpr Samples kAcross = &Ground::BuildingPhysicalProduct::AcrossM;
  const Summary fill = Describe(field, kFill, kStampWorthM);
  if (fill.Count != 0) {
    report.RecordMetric("buildings: a stamp would fill, worst", fill.Maximum, "m");
    report.RecordMetric(
        "buildings: footprints worth a stamp", static_cast<double>(fill.Above), "footprints");
    if (quantiles) {
      report.RecordMetric(
          "buildings: a stamp would fill, p50", Quantile(field, kFill, fill, kMiddleQuantile), "m");
      report.RecordMetric(
          "buildings: a stamp would fill, p95", Quantile(field, kFill, fill, kBroadQuantile), "m");
    }
  }
  const Summary across = Describe(field, kAcross, kGroundCellM);
  if (across.Count != 0) {
    report.RecordMetric("buildings: and the narrowest of them", across.Minimum, "m");
    report.RecordMetric("buildings: footprints narrower than a ground cell",
                        static_cast<double>(across.Below),
                        "footprints");
    if (quantiles) {
      report.RecordMetric("buildings: footprint across, p50",
                          Quantile(field, kAcross, across, kMiddleQuantile),
                          "m");
      report.RecordMetric("buildings: footprint across, p05",
                          Quantile(field, kAcross, across, kNarrowQuantile),
                          "m");
    }
  }
}
}
