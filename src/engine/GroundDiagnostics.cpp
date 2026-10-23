#include "GroundDiagnostics.h"
#include "BuildingFootprintMeasurements.h"

#include "BuildingField.h"
#include "GroundMesher.h"
#include "DiagnosticLedger.h"
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

void ReportBuildingFootprints(DiagnosticLedger &report,
                              const ::outshine::Generators::Osm::BuildingField &footprints,
                              const ::outshine::Generators::Osm::OsmField *vectors,
                              const TangentFrame &frame,
                              bool quantiles) {
  const Vec3 &anchor = footprints.Anchor();
  double away = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double step = anchor[axis] - frame.OriginEcef()[axis];
    away += step * step;
  }
  report.RecordMetric("buildings: their anchor lies from the frame's origin", std::sqrt(away), "m");
  ReportBuildingFootprintMeasurements(report, footprints, quantiles);
  report.RecordMetric("buildings: footprints the field holds",
                      static_cast<double>(footprints.FootprintCount()),
                      "footprints");
  if (vectors != nullptr) {
    report.RecordMetric("buildings: vector tiles the field settled",
                        static_cast<double>(vectors->Tiles().size()),
                        "tiles");
    report.RecordMetric("buildings: OSM features it holds",
                        static_cast<double>(vectors->Features().size()),
                        "features");
  }
}

void ReportGroundRelief(DiagnosticLedger &report, GroundRelief relief) {
  report.RecordMetric(
      "relief: the ring's tallest vertex ABOVE THE ELLIPSOID", relief.TallestM, "m");
  report.RecordMetric("relief: and how far out it lies", relief.TallestDistanceM, "m");
  report.RecordMetric("relief: the ring's lowest vertex above the ellipsoid", relief.LowestM, "m");
  report.RecordMetric("relief: so the true relief, with the sphere taken out",
                      relief.TallestM - relief.LowestM,
                      "m");
}

void ReportSubjectPlacements(DiagnosticLedger &report, const RuntimeScene &scene) {
  for (size_t part = 0; part < scene.Shown().Parts.size(); ++part) {
    const Render::ShapePart &one = scene.Shown().Parts[part];
    report.RecordMetric("restand: subject part " + std::to_string(part) + " first vertex",
                        static_cast<double>(one.FirstVertex),
                        "");
    report.RecordMetric("restand: subject part " + std::to_string(part) + " vertex count",
                        static_cast<double>(one.VertexCount),
                        "");
    report.RecordMetric("restand: subject part " + std::to_string(part) + " first index",
                        static_cast<double>(one.FirstIndex),
                        "");
    report.RecordMetric("restand: subject part " + std::to_string(part) + " index count",
                        static_cast<double>(one.IndexCount),
                        "");
  }
  for (size_t part = 0; part < scene.PartsStanding(); ++part) {
    const double *const m = scene.PlacementStanding(part);
    if (m == nullptr) { continue; }
    double most = 0.0;
    for (int at = 0; at < 16; ++at) { most += std::fabs(m[at]); }
    report.RecordMetric("restand: part " + std::to_string(part) +
                            " placement, sum of the absolute terms",
                        most,
                        "");
    report.RecordMetric(
        "restand: part " + std::to_string(part) + " diagonal", m[0] + m[5] + m[10] + m[15], "");
  }
}

}
