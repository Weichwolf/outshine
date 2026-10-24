#ifndef OUTSHINE_WORLD_DATA_SOURCEPROVIDER_H
#define OUTSHINE_WORLD_DATA_SOURCEPROVIDER_H

#include <cstdint>
#include <optional>
#include "GeographicBounds.h"
#include <string>

namespace outshine::Data {

enum class MissingDataPolicy : uint8_t { Continue, Fail };

struct SourceProvider {
  std::string Kind;
  std::string Revision;
  int Priority = 0;
  MissingDataPolicy Missing = MissingDataPolicy::Continue;
  std::string Dataset;
  std::string Location;
  std::string Endpoint;
  std::optional<GeographicBounds> Coverage;
  std::string PayloadSha256;

  std::string Schema;

  [[nodiscard]] bool operator==(const SourceProvider &) const = default;
};

}

#endif
