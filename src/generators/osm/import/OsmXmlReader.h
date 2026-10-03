#ifndef OUTSHINE_GENERATORS_OSM_IMPORT_OSMXMLREADER_H
#define OUTSHINE_GENERATORS_OSM_IMPORT_OSMXMLREADER_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>

#include "OsmElementSet.h"

namespace outshine::Generators::Osm {

inline constexpr size_t kMaximumXmlBytes = size_t{4} * 1024u * 1024u;

enum class XmlError : uint8_t {
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

class XmlReader {
public:
  [[nodiscard]] static std::expected<ElementSet, XmlError> Read(std::string_view xml,
                                                                Data::SourceIdentity identity);
};

}

#endif
