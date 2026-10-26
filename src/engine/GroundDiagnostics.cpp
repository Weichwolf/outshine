#include "GroundDiagnostics.h"
#include "BuildingFootprintMeasurements.h"

#include "BuildingField.h"
#include "DiagnosticLedger.h"
#include "OsmField.h"

#include "TangentFrame.h"

#include <cmath>

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

}
