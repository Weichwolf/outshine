#ifndef OUTSHINE_ENGINE_BUILDINGFOOTPRINTMEASUREMENTS_H
#define OUTSHINE_ENGINE_BUILDINGFOOTPRINTMEASUREMENTS_H

namespace outshine::Generators::Osm {
class BuildingField;
}

namespace outshine::Core {
class DiagnosticLedger;

void ReportBuildingFootprintMeasurements(DiagnosticLedger &report,
                                         const ::outshine::Generators::Osm::BuildingField &field,
                                         bool quantiles);
}

#endif
