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

class Ledger;
class RuntimeScene;

struct GroundRelief {
  double TallestM = 0.0;
  double LowestM = 0.0;
  double TallestDistanceM = 0.0;
};

void ReportBuildingFootprints(Ledger &report,
                              const Ground::BuildingField &footprints,
                              const Ground::OsmField *vectors,
                              const TangentFrame &frame);

void ReportGroundRelief(Ledger &report, GroundRelief relief);

void ReportSubjectPlacements(Ledger &report, const RuntimeScene &scene);

}
#endif
