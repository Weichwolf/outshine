#ifndef OUTSHINE_GENERATORS_OSM_PROVIDERS_PREFERREDVECTORSOURCE_H
#define OUTSHINE_GENERATORS_OSM_PROVIDERS_PREFERREDVECTORSOURCE_H

#include <SourceProvider.h>
#include <string>

namespace outshine::Generators::Osm {

[[nodiscard]] inline Data::SourceProvider PreferredVectorSource() {
  const std::string revision = "20261025_080012_pt";
  return {.Kind = "vector",
          .Revision = revision,
          .Missing = Data::MissingDataPolicy::Fail,
          .Dataset = "openfreemap.openmaptiles",
          .Location = "",
          .Endpoint = "https://tiles.openfreemap.org/planet/" + revision + "/{z}/{x}/{y}.pbf",
          .Coverage = std::nullopt,
          .PayloadSha256 = "",
          .Schema = "openmaptiles"};
}

}
#endif
