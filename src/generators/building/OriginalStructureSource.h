#ifndef OUTSHINE_GENERATORS_BUILDING_ORIGINALSTRUCTURESOURCE_H
#define OUTSHINE_GENERATORS_BUILDING_ORIGINALSTRUCTURESOURCE_H

#include "OsmSourceSnapshot.h"
#include "SourceProvenance.h"
#include <memory>

namespace outshine::Generators {

struct OriginalStructureSource {
  std::shared_ptr<const Data::OsmSourceSnapshot> Snapshot;
  std::weak_ptr<const Data::OsmSourceSnapshot> Archive;
  Data::ProductOrigin Origin;
};

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
