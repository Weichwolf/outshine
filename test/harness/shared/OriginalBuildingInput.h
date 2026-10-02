#ifndef OUTSHINE_TEST_HARNESS_SHARED_ORIGINALBUILDINGINPUT_H
#define OUTSHINE_TEST_HARNESS_SHARED_ORIGINALBUILDINGINPUT_H

#include "OsmStructureDescription.h"
#include "OsmSourceCapture.h"
#include "StructureInput.h"
#include <expected>
#include <utility>

namespace outshine::Test {

struct OriginalBuildingSource {
  std::shared_ptr<const Data::OsmSourceSnapshot> Snapshot;
  Data::ProductOrigin Origin;
};

[[nodiscard]] inline const Data::OsmSourceSnapshot &
CapturedOsmSource(const Generators::RawTile &input) {
  return static_cast<const Generators::Osm::SourceCapture &>(*input.SourceInputs.Objects)
      .Snapshot();
}

[[nodiscard]] inline std::expected<Generators::RawTile, Generators::Osm::StructureDescriptionError>
OriginalBuildingInput(const Generators::Osm::BuildingFootprints &footprints,
                      OriginalBuildingSource source,
                      Generators::Osm::StructurePolicy policy) {
  auto described = Generators::Osm::DescribeStructures(
      footprints, source.Snapshot, std::move(source.Origin), policy);
  if (!described) { return std::unexpected(described.error()); }
  auto input = Generators::StructureInput(std::move(described->Footprints));
  if (!input) { return std::unexpected(Generators::Osm::StructureDescriptionError::InvalidCell); }
  input->SourceInputs.Objects = std::move(described->Source);
  input->SourceInputs.Archive = std::move(described->Archive);
  return std::move(*input);
}

}

#endif
