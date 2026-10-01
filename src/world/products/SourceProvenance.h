#ifndef OUTSHINE_WORLD_PRODUCTS_SOURCEPROVENANCE_H
#define OUTSHINE_WORLD_PRODUCTS_SOURCEPROVENANCE_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <world/SourceProvider.h>
#include <world/data/GeoCellId.h>

namespace outshine::Data {

struct SourceObjectId {
  uint64_t Id = 0;
  uint8_t Kind = 0;

  [[nodiscard]] bool operator==(const SourceObjectId &) const = default;
};

struct SourceProvenance {
  std::string DatasetId;
  std::string Revision;
  std::vector<std::string> PayloadSha256;
  std::optional<GeoCellId> Cell;

  [[nodiscard]] bool operator==(const SourceProvenance &) const = default;
};

struct ProductOrigin {
  std::shared_ptr<const SourceProvenance> Provenance;
  SourceCoverage Bounds;
  uint64_t Selection = 0;
};

}

#endif
