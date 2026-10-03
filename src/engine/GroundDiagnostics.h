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
class RuntimeScene;

struct GroundRelief {
  double TallestM = 0.0;
  double LowestM = 0.0;
  double TallestDistanceM = 0.0;
};

void ReportBuildingFootprints(DiagnosticLedger &report,
                              const ::outshine::Generators::Osm::BuildingField &footprints,
                              const ::outshine::Generators::Osm::OsmField *vectors,
                              const TangentFrame &frame);

void ReportGroundRelief(DiagnosticLedger &report, GroundRelief relief);

void ReportSubjectPlacements(DiagnosticLedger &report, const RuntimeScene &scene);

}
#endif
