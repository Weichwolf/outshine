#ifndef TERRARIUMDEM_H
#define TERRARIUMDEM_H

#include "WebTileSource.h"

namespace outshine::Data {

class TerrariumDem : public WebTileSource {
public:
  TerrariumDem();

protected:
  [[nodiscard]] std::string Url(const Address &at) const override;
  [[nodiscard]] Meaning Classify(int status, size_t bytes) const noexcept override;
};

}
#endif
