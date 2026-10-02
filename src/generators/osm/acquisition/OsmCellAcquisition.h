#ifndef OUTSHINE_GENERATORS_OSM_OSMCELLACQUISITION_H
#define OUTSHINE_GENERATORS_OSM_OSMCELLACQUISITION_H

#include "OsmApiReader.h"
#include <memory>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>

namespace outshine::Generators::Osm {
class CellAcquisition {
public:
  static constexpr size_t MaximumPendingCells = 8;
  CellAcquisition(Data::SourceProvider catalogue,
                  Data::ContentStore &store,
                  Data::Transport &wire,
                  const Data::ProviderRegistry *registry = nullptr,
                  std::string shippedRoot = {});
  ~CellAcquisition();
  CellAcquisition(const CellAcquisition &) = delete;
  CellAcquisition &operator=(const CellAcquisition &) = delete;
  [[nodiscard]] std::expected<void, std::string> Start(Data::GeoCellId cell);
  [[nodiscard]] std::expected<std::optional<SourceRead>, std::string> TakeReady();
  [[nodiscard]] size_t PendingCount() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> Impl_;
};
}
#endif
