#ifndef OUTSHINE_WORLD_PRODUCTS_SOURCEOBJECTS_H
#define OUTSHINE_WORLD_PRODUCTS_SOURCEOBJECTS_H

#include "SourceProvenance.h"

namespace outshine::Data {

class SourceObjects {
public:
  virtual ~SourceObjects() = default;
  SourceObjects(const SourceObjects &) = delete;
  SourceObjects &operator=(const SourceObjects &) = delete;

  [[nodiscard]] virtual bool Contains(SourceObjectId id) const noexcept = 0;

protected:
  SourceObjects() = default;
};

}

#endif
