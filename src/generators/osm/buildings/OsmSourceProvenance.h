#ifndef OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMSOURCEPROVENANCE_H
#define OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMSOURCEPROVENANCE_H

#include "OsmSourceSnapshot.h"
#include "SourceProvenance.h"
#include <memory>

namespace outshine::Generators::Osm {

[[nodiscard]] inline std::shared_ptr<const Data::SourceProvenance>
DescribeOsmSource(const Data::OsmSourceSnapshot &source) {
  auto provenance = std::make_shared<Data::SourceProvenance>();
  const auto &identity = source.Elements.SourceIdentity();
  provenance->DatasetId = identity.DatasetId;
  provenance->Revision = identity.Revision;
  provenance->Cell = source.Cell;
  provenance->PayloadSha256.reserve(source.Chunks.size());
  for (const auto &chunk : source.Chunks) {
    provenance->PayloadSha256.push_back(chunk.PayloadSha256);
  }
  return provenance;
}

}

#endif
