#ifndef OUTSHINE_WORLD_PRODUCTS_SOURCEIDENTITY_H
#define OUTSHINE_WORLD_PRODUCTS_SOURCEIDENTITY_H

#include <string>

namespace outshine::Data {

struct SourceIdentity {
  std::string DatasetId;
  std::string Revision;

  [[nodiscard]] bool operator==(const SourceIdentity &) const = default;
};

}

#endif
