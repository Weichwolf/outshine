#ifndef VERSATILESVECTOR_H
#define VERSATILESVECTOR_H

#include "WebTileSource.h"

namespace outshine::Data {

class VersatilesVector : public WebTileSource {
public:
  VersatilesVector();

protected:
  [[nodiscard]] std::string Url(const Address &at) const override;
  [[nodiscard]] Meaning Classify(int status, size_t bytes) const noexcept override;
};

}
#endif
