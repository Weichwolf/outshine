#include "GroundDiagnostics.h"

#include "BuildingField.h"
#include "GroundMesher.h"
#include "Ledger.h"
#include "OsmField.h"
#include "RuntimeScene.h"

#include "math/Quantile.h"
#include "TangentFrame.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace outshine::Core {

void ReportBuildingFootprints(Ledger &report,
                              const Ground::BuildingField &footprints,
                              const Ground::OsmField *vectors,
                              const TangentFrame &frame) {
  constexpr double kGroundCellM = 25.0;
  const Vec3 &anchor = footprints.Anchor();
  double away = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double step = anchor[axis] - frame.OriginEcef()[axis];
    away += step * step;
  }
  report.Places("buildings: their anchor lies from the frame's origin", std::sqrt(away), "m");
  {
    std::vector<double> fill = footprints.SeatSpreadM();
    std::vector<double> across = footprints.FootprintAcrossM();
    const auto publishQuantile =
        [&report](const char *name, std::span<const double> sample, double share) {
          if (const auto value = QuantileOf(sample, share)) { report.Places(name, *value, "m"); }
        };
    if (!fill.empty()) {
      std::ranges::sort(fill);
      size_t wouldStamp = 0;
      for (const double filled : fill) {
        if (filled > kStampWorthM) { ++wouldStamp; }
      }
      publishQuantile("buildings: a stamp would fill, p50", fill, kMiddleQuantile);
      publishQuantile("buildings: a stamp would fill, p95", fill, kBroadQuantile);
      report.Places("buildings: a stamp would fill, worst", fill.back(), "m");
      report.Places(
          "buildings: footprints worth a stamp", static_cast<double>(wouldStamp), "footprints");
    }
    if (!across.empty()) {
      std::ranges::sort(across);
      size_t underOneCell = 0;
      for (const double wide : across) {
        if (wide < kGroundCellM) { ++underOneCell; }
      }
      publishQuantile("buildings: footprint across, p50", across, kMiddleQuantile);
      publishQuantile("buildings: footprint across, p05", across, kNarrowQuantile);
      report.Places("buildings: and the narrowest of them", across.front(), "m");
      report.Places("buildings: footprints narrower than a ground cell",
                    static_cast<double>(underOneCell),
                    "footprints");
    }
  }
  report.Places("buildings: footprints the field holds",
                static_cast<double>(footprints.Footprints().size()),
                "footprints");
  if (vectors != nullptr) {
    report.Places("buildings: vector tiles the field settled",
                  static_cast<double>(vectors->Tiles().size()),
                  "tiles");
    report.Places("buildings: OSM features it holds",
                  static_cast<double>(vectors->Features().size()),
                  "features");
  }
}

void ReportGroundRelief(Ledger &report, GroundRelief relief) {
  report.Places("relief: the ring's tallest vertex ABOVE THE ELLIPSOID", relief.TallestM, "m");
  report.Places("relief: and how far out it lies", relief.TallestDistanceM, "m");
  report.Places("relief: the ring's lowest vertex above the ellipsoid", relief.LowestM, "m");
  report.Places("relief: so the true relief, with the sphere taken out",
                relief.TallestM - relief.LowestM,
                "m");
}

void ReportSubjectPlacements(Ledger &report, const RuntimeScene &scene) {
  for (size_t part = 0; part < scene.Shown().Parts.size(); ++part) {
    const Render::ShapePart &one = scene.Shown().Parts[part];
    report.Places("restand: subject part " + std::to_string(part) + " first vertex",
                  static_cast<double>(one.FirstVertex),
                  "");
    report.Places("restand: subject part " + std::to_string(part) + " vertex count",
                  static_cast<double>(one.VertexCount),
                  "");
    report.Places("restand: subject part " + std::to_string(part) + " first index",
                  static_cast<double>(one.FirstIndex),
                  "");
    report.Places("restand: subject part " + std::to_string(part) + " index count",
                  static_cast<double>(one.IndexCount),
                  "");
  }
  for (size_t part = 0; part < scene.PartsStanding(); ++part) {
    const double *const m = scene.PlacementStanding(part);
    if (m == nullptr) { continue; }
    double most = 0.0;
    for (int at = 0; at < 16; ++at) { most += std::fabs(m[at]); }
    report.Places("restand: part " + std::to_string(part) + " placement, sum of the absolute terms",
                  most,
                  "");
    report.Places(
        "restand: part " + std::to_string(part) + " diagonal", m[0] + m[5] + m[10] + m[15], "");
  }
}

}
