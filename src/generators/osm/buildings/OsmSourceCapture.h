#ifndef OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMSOURCECAPTURE_H
#define OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMSOURCECAPTURE_H

#include "OsmSourceSnapshot.h"
#include "SourceObjects.h"
#include <cassert>
#include <memory>
#include <set>
#include <utility>

namespace outshine::Generators::Osm {

class SourceCapture final : public Data::SourceObjects {
public:
  explicit SourceCapture(std::shared_ptr<const Data::OsmSourceSnapshot> snapshot)
      : Snapshot_(std::move(snapshot)) {
    assert(Snapshot_);
  }

  [[nodiscard]] bool Contains(Data::SourceObjectId id) const noexcept override {
    const auto &elements = Snapshot_->Elements;
    switch (static_cast<Data::OsmElementKind>(id.Kind)) {
      case Data::OsmElementKind::Node: return elements.FindNode(id.Id) != nullptr;
      case Data::OsmElementKind::Way: return elements.FindWay(id.Id) != nullptr;
      case Data::OsmElementKind::Relation: return elements.FindRelation(id.Id) != nullptr;
    }
    return false;
  }

  [[nodiscard]] const Data::OsmSourceSnapshot &Snapshot() const noexcept { return *Snapshot_; }

  void RecordMemberWays(Data::SourceObjectId id, std::set<uint64_t> &ways) const {
    if (id.Kind != static_cast<uint8_t>(Data::OsmElementKind::Relation)) { return; }
    const auto *relation = Snapshot_->Elements.FindRelation(id.Id);
    if (relation == nullptr) { return; }
    for (const auto &member : relation->Members) {
      if (member.Kind == Data::OsmElementKind::Way) { ways.insert(member.Id); }
    }
  }

private:
  std::shared_ptr<const Data::OsmSourceSnapshot> Snapshot_;
};

}

#endif
