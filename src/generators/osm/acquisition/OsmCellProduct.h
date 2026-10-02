#ifndef OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMCELLPRODUCT_H
#define OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMCELLPRODUCT_H

#include "OsmSourceSnapshot.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>

namespace outshine::Generators::Osm {

class CellProduct {
public:
  virtual ~CellProduct() = default;
  [[nodiscard]] virtual size_t StorageChargeBytes() const noexcept = 0;
};

class CellCompiler {
public:
  virtual ~CellCompiler() = default;
  [[nodiscard]] virtual std::expected<std::shared_ptr<const CellProduct>, std::string>
  Compile(std::shared_ptr<const Data::OsmSourceSnapshot> source,
          const std::stop_token &stop) const = 0;
};

}
#endif
