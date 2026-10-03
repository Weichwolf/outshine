#ifndef OUTSHINE_GENERATORS_OSM_PROVIDERS_PREFERREDVECTORSOURCE_H
#define OUTSHINE_GENERATORS_OSM_PROVIDERS_PREFERREDVECTORSOURCE_H

#include <world/SourceProvider.h>

namespace outshine::Generators::Osm {

[[nodiscard]] inline Data::SourceProvider PreferredVectorSource() {
  return {.Kind = "vector",
          .Revision = "20260927_080001_pt",
          .Missing = Data::MissingDataPolicy::Fail,
          .Dataset = "openfreemap.openmaptiles",
          .Location = "",
          .Endpoint = "https://tiles.openfreemap.org/planet/20260927_080001_pt/{z}/{x}/{y}.pbf",
          .Coverage = std::nullopt,
          .PayloadSha256 = "",
          .Schema = "openmaptiles"};
}

}
#endif
