#ifndef OUTSHINE_WORLD_DATA_TERRARIUMDEM_H
#define OUTSHINE_WORLD_DATA_TERRARIUMDEM_H

#include "WebTileSource.h"

namespace outshine::Data {

class TerrariumDem : public WebTileSource {
public:
  TerrariumDem();

protected:
  [[nodiscard]] std::string Url(const Address &at) const override;
  [[nodiscard]] bool CountsAbsent(int status) const noexcept override;
};

} // namespace outshine::Data
#endif
