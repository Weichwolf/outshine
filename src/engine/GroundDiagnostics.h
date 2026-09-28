#ifndef OUTSHINE_ENGINE_GROUNDDIAGNOSTICS_H
#define OUTSHINE_ENGINE_GROUNDDIAGNOSTICS_H

namespace outshine {
class TangentFrame;

namespace Ground {
class BuildingField;
class OsmField;
}
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
                              const Ground::BuildingField &footprints,
                              const Ground::OsmField *vectors,
                              const TangentFrame &frame);

void ReportGroundRelief(DiagnosticLedger &report, GroundRelief relief);

void ReportSubjectPlacements(DiagnosticLedger &report, const RuntimeScene &scene);

}
#endif
