#ifndef OUTSHINE_GENERATORS_OSM_IMPORT_OSMELEMENTSET_H
#define OUTSHINE_GENERATORS_OSM_IMPORT_OSMELEMENTSET_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "SourceIdentity.h"

namespace outshine::Generators::Osm {

enum class ElementKind : uint8_t { Node, Way, Relation };

struct ElementId {
  ElementKind Kind = ElementKind::Node;
  uint64_t Id = 0;

  [[nodiscard]] bool operator==(const ElementId &) const = default;
};

struct Tag {
  std::string Key;
  std::string Value;

  [[nodiscard]] bool operator==(const Tag &) const = default;
};

struct Node {
  uint64_t Id = 0;
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
  std::vector<Tag> Tags;

  [[nodiscard]] bool operator==(const Node &) const = default;
};

struct Way {
  uint64_t Id = 0;
  std::vector<uint64_t> NodeIds;
  std::vector<Tag> Tags;

  [[nodiscard]] bool operator==(const Way &) const = default;
};

struct RelationMember {
  ElementKind Kind = ElementKind::Node;
  uint64_t Id = 0;
  std::string Role;

  [[nodiscard]] bool operator==(const RelationMember &) const = default;
};

struct Relation {
  uint64_t Id = 0;
  std::vector<RelationMember> Members;
  std::vector<Tag> Tags;

  [[nodiscard]] bool operator==(const Relation &) const = default;
};

enum class MergeErrorCode : uint8_t {
  EmptyInput,
  IdentityMismatch,
  BudgetExceeded,
  ConflictingElement
};

struct MergeError {
  MergeErrorCode Code = MergeErrorCode::BudgetExceeded;
  ElementKind Kind = ElementKind::Node;
  uint64_t Id = 0;
};

struct MissingReference {
  ElementKind OwnerKind = ElementKind::Node;
  uint64_t OwnerId = 0;
  ElementKind MissingKind = ElementKind::Node;
  uint64_t MissingId = 0;
};

class ElementSet {
public:
  ElementSet(const ElementSet &) = default;
  ElementSet(ElementSet &&) noexcept = default;
  ElementSet &operator=(const ElementSet &) = default;
  ElementSet &operator=(ElementSet &&) noexcept = default;

  [[nodiscard]] static std::expected<ElementSet, MergeError>
  Merge(std::span<const ElementSet> chunks, size_t maxInputElements);

  [[nodiscard]] const Data::SourceIdentity &SourceIdentity() const noexcept {
    return SourceIdentity_;
  }

  [[nodiscard]] std::span<const Node> Nodes() const noexcept { return Nodes_; }

  [[nodiscard]] std::span<const Way> Ways() const noexcept { return Ways_; }

  [[nodiscard]] std::span<const Relation> Relations() const noexcept { return Relations_; }

  [[nodiscard]] size_t StorageChargeBytes() const noexcept;

  [[nodiscard]] const Node *FindNode(uint64_t id) const noexcept;
  [[nodiscard]] const Way *FindWay(uint64_t id) const noexcept;
  [[nodiscard]] const Relation *FindRelation(uint64_t id) const noexcept;
  [[nodiscard]] std::optional<MissingReference> FirstMissingReference() const noexcept;
  [[nodiscard]] std::optional<MissingReference>
  FirstMissingReference(std::span<const ElementId> roots) const;
  [[nodiscard]] std::expected<ElementSet, MissingReference>
  SelectReferenced(std::span<const ElementId> roots) const;

private:
  friend class XmlReader;

  ElementSet() = default;

  Data::SourceIdentity SourceIdentity_;
  std::vector<Node> Nodes_;
  std::vector<Way> Ways_;
  std::vector<Relation> Relations_;
};

}

#endif
