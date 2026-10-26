#ifndef OUTSHINE_ENGINE_GROUNDDIAGNOSTICS_H
#define OUTSHINE_ENGINE_GROUNDDIAGNOSTICS_H

namespace outshine::Generators::Osm {
class BuildingField;
class OsmField;
}

namespace outshine {
class TangentFrame;

}

namespace outshine::Core {

class DiagnosticLedger;

struct GroundRelief {
  double TallestM = 0.0;
  double LowestM = 0.0;
  double TallestDistanceM = 0.0;
};

void ReportBuildingFootprints(DiagnosticLedger &report,
                              const ::outshine::Generators::Osm::BuildingField &footprints,
                              const ::outshine::Generators::Osm::OsmField *vectors,
                              const TangentFrame &frame,
                              bool quantiles);

void ReportGroundRelief(DiagnosticLedger &report, GroundRelief relief);

}
#endif
