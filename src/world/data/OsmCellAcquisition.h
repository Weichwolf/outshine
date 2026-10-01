#ifndef OUTSHINE_WORLD_DATA_OSMCELLACQUISITION_H
#define OUTSHINE_WORLD_DATA_OSMCELLACQUISITION_H

#include "OsmApiReader.h"
#include <memory>

namespace outshine::Data {
class OsmCellAcquisition {
public:
  static constexpr size_t MaximumPendingCells = 8;
  OsmCellAcquisition(SourceProvider catalogue,
                     ContentStore &store,
                     Transport &wire,
                     const ProviderRegistry *registry = nullptr,
                     std::string shippedRoot = {});
  ~OsmCellAcquisition();
  OsmCellAcquisition(const OsmCellAcquisition &) = delete;
  OsmCellAcquisition &operator=(const OsmCellAcquisition &) = delete;
  [[nodiscard]] std::expected<void, std::string> Start(GeoCellId cell);
  [[nodiscard]] std::expected<std::optional<OsmSourceRead>, std::string> TakeReady();
  [[nodiscard]] size_t PendingCount() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> Impl_;
};
}
#endif
