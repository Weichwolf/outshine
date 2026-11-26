#ifndef OUTSHINE_GENERATORS_OSM_PROVIDERS_PREFERREDVECTORSOURCE_H
#define OUTSHINE_GENERATORS_OSM_PROVIDERS_PREFERREDVECTORSOURCE_H

#include <SourceProvider.h>

namespace outshine::Generators::Osm {

inline constexpr auto kOpenFreeMapLiveEndpoint =
    "https://tiles.openfreemap.org/planet/latest/{z}/{x}/{y}.pbf";

[[nodiscard]] inline Data::SourceProvider PreferredVectorSource() {
  return {.Kind = "vector",
          .Revision = "openmaptiles-live-v1",
          .Priority = 1,
          .Missing = Data::MissingDataPolicy::Fail,
          .Dataset = "openfreemap.openmaptiles",
          .Location = "",
          .Endpoint = kOpenFreeMapLiveEndpoint,
          .Coverage = std::nullopt,
          .PayloadSha256 = "",
          .Schema = "openmaptiles"};
}

}
#endif
