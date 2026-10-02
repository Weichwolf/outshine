#ifndef OUTSHINE_WORLD_DATA_OSMXMLREADER_H
#define OUTSHINE_WORLD_DATA_OSMXMLREADER_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>

#include "OsmElements.h"

namespace outshine::Data {

inline constexpr size_t kMaxOsmXmlBytes = size_t{4} * 1024u * 1024u;

enum class OsmXmlError : uint8_t {
  InvalidSourceIdentity,
  InvalidDocument,
  UnsupportedRoot,
  InvalidId,
  InvalidCoordinate,
  InvalidTag,
  InvalidMember,
  DuplicateElement,
  BudgetExceeded
};

class OsmXmlReader {
public:
  [[nodiscard]] static std::expected<OsmElements, OsmXmlError> Read(std::string_view xml,
                                                                    SourceIdentity identity);
};

}

#endif
