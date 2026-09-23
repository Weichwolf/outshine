#ifndef OUTSHINE_WORLD_DATA_OSMELEMENTS_H
#define OUTSHINE_WORLD_DATA_OSMELEMENTS_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine::Data {

enum class OsmElementKind : uint8_t { Node, Way, Relation };

struct OsmSourceIdentity {
  std::string DatasetId;
  std::string Revision;

  [[nodiscard]] bool operator==(const OsmSourceIdentity &) const = default;
};

struct OsmTag {
  std::string Key;
  std::string Value;

  [[nodiscard]] bool operator==(const OsmTag &) const = default;
};

struct OsmNode {
  uint64_t Id = 0;
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
  std::vector<OsmTag> Tags;

  [[nodiscard]] bool operator==(const OsmNode &) const = default;
};

struct OsmWay {
  uint64_t Id = 0;
  std::vector<uint64_t> NodeIds;
  std::vector<OsmTag> Tags;

  [[nodiscard]] bool operator==(const OsmWay &) const = default;
};

struct OsmRelationMember {
  OsmElementKind Kind = OsmElementKind::Node;
  uint64_t Id = 0;
  std::string Role;

  [[nodiscard]] bool operator==(const OsmRelationMember &) const = default;
};

struct OsmRelation {
  uint64_t Id = 0;
  std::vector<OsmRelationMember> Members;
  std::vector<OsmTag> Tags;

  [[nodiscard]] bool operator==(const OsmRelation &) const = default;
};

enum class OsmMergeErrorCode : uint8_t {
  EmptyInput,
  IdentityMismatch,
  BudgetExceeded,
  ConflictingElement
};

struct OsmMergeError {
  OsmMergeErrorCode Code = OsmMergeErrorCode::BudgetExceeded;
  OsmElementKind Kind = OsmElementKind::Node;
  uint64_t Id = 0;
};

struct MissingOsmReference {
  OsmElementKind OwnerKind = OsmElementKind::Node;
  uint64_t OwnerId = 0;
  OsmElementKind MissingKind = OsmElementKind::Node;
  uint64_t MissingId = 0;
};

class OsmElements {
public:
  OsmElements(const OsmElements &) = default;
  OsmElements(OsmElements &&) noexcept = default;
  OsmElements &operator=(const OsmElements &) = default;
  OsmElements &operator=(OsmElements &&) noexcept = default;

  [[nodiscard]] static std::expected<OsmElements, OsmMergeError>
  Merge(std::span<const OsmElements> chunks, size_t maxInputElements);

  [[nodiscard]] const OsmSourceIdentity &SourceIdentity() const noexcept { return SourceIdentity_; }

  [[nodiscard]] std::span<const OsmNode> Nodes() const noexcept { return Nodes_; }

  [[nodiscard]] std::span<const OsmWay> Ways() const noexcept { return Ways_; }

  [[nodiscard]] std::span<const OsmRelation> Relations() const noexcept { return Relations_; }

  [[nodiscard]] const OsmNode *FindNode(uint64_t id) const noexcept;
  [[nodiscard]] const OsmWay *FindWay(uint64_t id) const noexcept;
  [[nodiscard]] const OsmRelation *FindRelation(uint64_t id) const noexcept;
  [[nodiscard]] std::optional<MissingOsmReference> FirstMissingReference() const noexcept;

private:
  friend class OsmXmlReader;

  OsmElements() = default;

  OsmSourceIdentity SourceIdentity_;
  std::vector<OsmNode> Nodes_;
  std::vector<OsmWay> Ways_;
  std::vector<OsmRelation> Relations_;
};

}

#endif
