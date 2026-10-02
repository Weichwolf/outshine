#ifndef OUTSHINE_TEST_HARNESS_SHARED_ORIGINALBUILDINGINPUT_H
#define OUTSHINE_TEST_HARNESS_SHARED_ORIGINALBUILDINGINPUT_H

#include "OsmStructureDescription.h"
#include "StructureInput.h"
#include <expected>
#include <utility>

namespace outshine::Test {

[[nodiscard]] inline std::expected<Generators::RawTile, Generators::Osm::StructureDescriptionError>
OriginalBuildingInput(const Generators::Osm::BuildingFootprints &footprints,
                      Generators::OriginalStructureSource source,
                      Generators::Osm::StructurePolicy policy) {
  auto described = Generators::Osm::DescribeStructures(
      footprints, source.Snapshot, std::move(source.Origin), policy);
  if (!described) { return std::unexpected(described.error()); }
  auto input = Generators::StructureInput(std::move(described->Footprints));
  if (!input) { return std::unexpected(Generators::Osm::StructureDescriptionError::InvalidCell); }
  input->Original.Snapshot = std::move(described->Source);
  input->Original.Archive = std::move(described->Archive);
  return std::move(*input);
}

}

#endif
