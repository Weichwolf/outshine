#ifndef OUTSHINE_WORLD_DATA_TERRARIUMDEM_H
#define OUTSHINE_WORLD_DATA_TERRARIUMDEM_H

#include <string>

#include "WebTileSource.h"

namespace outshine::Data {

class TerrariumDem : public WebTileSource {
public:
  TerrariumDem(std::string revision, Rank order, AbsencePolicy absence);

protected:
  [[nodiscard]] std::string Url(const Address &at) const override;
  [[nodiscard]] bool CountsAbsent(int status) const noexcept override;
};

}
#endif
