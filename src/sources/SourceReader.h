#ifndef OUTSHINE_SOURCES_SOURCEREADER_H
#define OUTSHINE_SOURCES_SOURCEREADER_H

#include "SourceDelivery.h"
#include "SourceRequest.h"

namespace outshine::Data {
class SourceReader {
public:
  virtual ~SourceReader() = default;
  [[nodiscard]] virtual SourceDelivery Poll(const SourceRequest &request) = 0;
};
}
#endif
