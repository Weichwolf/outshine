#include "OsmElements.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace outshine::Data {

namespace {

template <typename Element>
const Element *FindById(const std::vector<Element> &elements, uint64_t id) noexcept {
  const auto found = std::lower_bound(
      elements.begin(), elements.end(), id, [](const Element &element, uint64_t wanted) {
        return element.Id < wanted;
      });
  return found != elements.end() && found->Id == id ? &*found : nullptr;
}

}

const OsmNode *OsmElements::FindNode(uint64_t id) const noexcept {
  return FindById(Nodes_, id);
}

const OsmWay *OsmElements::FindWay(uint64_t id) const noexcept {
  return FindById(Ways_, id);
}

const OsmRelation *OsmElements::FindRelation(uint64_t id) const noexcept {
  return FindById(Relations_, id);
}

std::optional<MissingOsmReference> OsmElements::FirstMissingReference() const noexcept {
  for (const OsmWay &way : Ways_) {
    for (const uint64_t nodeId : way.NodeIds) {
      if (FindNode(nodeId) == nullptr) {
        return MissingOsmReference{.OwnerKind = OsmElementKind::Way,
                                   .OwnerId = way.Id,
                                   .MissingKind = OsmElementKind::Node,
                                   .MissingId = nodeId};
      }
    }
  }
  for (const OsmRelation &relation : Relations_) {
    for (const OsmRelationMember &member : relation.Members) {
      const bool found = [&] {
        switch (member.Kind) {
          case OsmElementKind::Node: return FindNode(member.Id) != nullptr;
          case OsmElementKind::Way: return FindWay(member.Id) != nullptr;
          case OsmElementKind::Relation: return FindRelation(member.Id) != nullptr;
        }
        return false;
      }();
      if (!found) {
        return MissingOsmReference{.OwnerKind = OsmElementKind::Relation,
                                   .OwnerId = relation.Id,
                                   .MissingKind = member.Kind,
                                   .MissingId = member.Id};
      }
    }
  }
  return std::nullopt;
}

}
