#ifndef OUTSHINE_WORLD_DATA_OSMELEMENTS_H
#define OUTSHINE_WORLD_DATA_OSMELEMENTS_H

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine::Data {

enum class OsmElementKind : uint8_t { Node, Way, Relation };

struct OsmTag {
  std::string Key;
  std::string Value;
};

struct OsmNode {
  uint64_t Id = 0;
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
  std::vector<OsmTag> Tags;
};

struct OsmWay {
  uint64_t Id = 0;
  std::vector<uint64_t> NodeIds;
  std::vector<OsmTag> Tags;
};

struct OsmRelationMember {
  OsmElementKind Kind = OsmElementKind::Node;
  uint64_t Id = 0;
  std::string Role;
};

struct OsmRelation {
  uint64_t Id = 0;
  std::vector<OsmRelationMember> Members;
  std::vector<OsmTag> Tags;
};

struct MissingOsmReference {
  OsmElementKind OwnerKind = OsmElementKind::Node;
  uint64_t OwnerId = 0;
  OsmElementKind MissingKind = OsmElementKind::Node;
  uint64_t MissingId = 0;
};

class OsmElements {
public:
  [[nodiscard]] std::span<const OsmNode> Nodes() const noexcept { return Nodes_; }

  [[nodiscard]] std::span<const OsmWay> Ways() const noexcept { return Ways_; }

  [[nodiscard]] std::span<const OsmRelation> Relations() const noexcept { return Relations_; }

  [[nodiscard]] const OsmNode *FindNode(uint64_t id) const noexcept;
  [[nodiscard]] const OsmWay *FindWay(uint64_t id) const noexcept;
  [[nodiscard]] const OsmRelation *FindRelation(uint64_t id) const noexcept;
  [[nodiscard]] std::optional<MissingOsmReference> FirstMissingReference() const noexcept;

private:
  friend class OsmXmlReader;

  std::vector<OsmNode> Nodes_;
  std::vector<OsmWay> Ways_;
  std::vector<OsmRelation> Relations_;
};

}

#endif
