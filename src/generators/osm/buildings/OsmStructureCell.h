#ifndef OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMSTRUCTURECELL_H
#define OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMSTRUCTURECELL_H

#include "OsmCellProduct.h"
#include "OsmStructureDescription.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine::Generators::Osm {

class StructureCell final : public CellProduct {
public:
  struct ElementProof {
    uint64_t Id = 0;
    std::array<uint8_t, 32> Digest{};
  };

  StructureDescription Description;
  std::array<std::vector<ElementProof>, 3> Elements;
  [[nodiscard]] size_t StorageChargeBytes() const noexcept override;
};

[[nodiscard]] std::expected<StructureDescription, std::string>
PrepareStructureCell(const std::shared_ptr<const Data::OsmSourceSnapshot> &source,
                     StructurePolicy policy,
                     const std::stop_token &stop);
[[nodiscard]] std::shared_ptr<const CellCompiler> MakeStructureCellCompiler(StructurePolicy policy);
[[nodiscard]] std::expected<void, std::string>
VerifyStructureCells(std::span<const std::shared_ptr<const StructureCell>> cells,
                     const std::stop_token &stop);

}
#endif
