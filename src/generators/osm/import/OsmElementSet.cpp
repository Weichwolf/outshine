#include "OsmElementSet.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {

namespace {

size_t TagCharge(const std::vector<Tag> &tags) noexcept {
  size_t bytes = tags.capacity() * sizeof(Tag);
  for (const auto &tag : tags) { bytes += tag.Key.capacity() + tag.Value.capacity() + 2; }
  return bytes;
}

template <typename Element>
const Element *FindById(const std::vector<Element> &elements, uint64_t id) noexcept {
  const auto found = std::lower_bound(
      elements.begin(), elements.end(), id, [](const Element &element, uint64_t wanted) {
        return element.Id < wanted;
      });
  return found != elements.end() && found->Id == id ? &*found : nullptr;
}

template <typename Element>
std::expected<void, MergeError> SortUnique(std::vector<Element> &elements, ElementKind kind) {
  std::ranges::sort(elements, {}, &Element::Id);
  size_t unique = 0;
  for (size_t read = 0; read < elements.size(); ++read) {
    if (unique > 0 && elements[read].Id == elements[unique - 1].Id) {
      if (elements[read] != elements[unique - 1]) {
        return std::unexpected(MergeError{
            .Code = MergeErrorCode::ConflictingElement, .Kind = kind, .Id = elements[read].Id});
      }
      continue;
    }
    if (read != unique) { elements[unique] = std::move(elements[read]); }
    ++unique;
  }
  elements.resize(unique);
  return {};
}

struct ReferenceClosure {
  const ElementSet &Source;
  std::vector<bool> Nodes;
  std::vector<bool> Ways;
  std::vector<bool> Relations;
  std::vector<ElementId> Pending;

  std::optional<MissingReference> Admit(ElementId owner, ElementId target) {
    switch (target.Kind) {
      case ElementKind::Node:
        if (const Node *node = Source.FindNode(target.Id)) {
          Nodes[static_cast<size_t>(node - Source.Nodes().data())] = true;
          return std::nullopt;
        }
        break;
      case ElementKind::Way:
        if (const Way *way = Source.FindWay(target.Id)) {
          const auto index = static_cast<size_t>(way - Source.Ways().data());
          if (!Ways[index]) {
            Ways[index] = true;
            Pending.push_back(target);
          }
          return std::nullopt;
        }
        break;
      case ElementKind::Relation:
        if (const Relation *relation = Source.FindRelation(target.Id)) {
          const auto index = static_cast<size_t>(relation - Source.Relations().data());
          if (!Relations[index]) {
            Relations[index] = true;
            Pending.push_back(target);
          }
          return std::nullopt;
        }
        break;
    }
    return MissingReference{.OwnerKind = owner.Kind,
                            .OwnerId = owner.Id,
                            .MissingKind = target.Kind,
                            .MissingId = target.Id};
  }

  std::optional<MissingReference> Complete(std::span<const ElementId> roots) {
    for (const ElementId root : roots) {
      if (const auto missing = Admit(root, root)) { return missing; }
    }
    size_t next = 0;
    while (next < Pending.size()) {
      const ElementId owner = Pending[next++];
      if (owner.Kind == ElementKind::Way) {
        for (const uint64_t node : Source.FindWay(owner.Id)->NodeIds) {
          if (const auto missing = Admit(owner, {.Kind = ElementKind::Node, .Id = node})) {
            return missing;
          }
        }
      } else {
        for (const RelationMember &member : Source.FindRelation(owner.Id)->Members) {
          if (const auto missing = Admit(owner, {.Kind = member.Kind, .Id = member.Id})) {
            return missing;
          }
        }
      }
    }
    return std::nullopt;
  }
};

template <typename Element>
std::vector<Element> CopySelected(std::span<const Element> elements,
                                  const std::vector<bool> &selected) {
  std::vector<Element> result;
  result.reserve(static_cast<size_t>(std::ranges::count(selected, true)));
  for (size_t index = 0; index < elements.size(); ++index) {
    if (selected[index]) { result.push_back(elements[index]); }
  }
  return result;
}

}

size_t ElementSet::StorageChargeBytes() const noexcept {
  size_t bytes = sizeof(ElementSet) + SourceIdentity_.DatasetId.capacity() +
                 SourceIdentity_.Revision.capacity() + 2 + Nodes_.capacity() * sizeof(Node) +
                 Ways_.capacity() * sizeof(Way) + Relations_.capacity() * sizeof(Relation);
  for (const auto &node : Nodes_) { bytes += TagCharge(node.Tags); }
  for (const auto &way : Ways_) {
    bytes += way.NodeIds.capacity() * sizeof(uint64_t) + TagCharge(way.Tags);
  }
  for (const auto &relation : Relations_) {
    bytes += relation.Members.capacity() * sizeof(RelationMember) + TagCharge(relation.Tags);
    for (const auto &member : relation.Members) { bytes += member.Role.capacity() + 1; }
  }
  return bytes;
}

std::expected<ElementSet, MergeError> ElementSet::Merge(std::span<const ElementSet> chunks,
                                                        size_t maxInputElements) {
  if (chunks.empty()) { return std::unexpected(MergeError{.Code = MergeErrorCode::EmptyInput}); }
  for (const ElementSet &chunk : chunks.subspan(1)) {
    if (chunk.SourceIdentity_ != chunks.front().SourceIdentity_) {
      return std::unexpected(MergeError{.Code = MergeErrorCode::IdentityMismatch});
    }
  }
  size_t inputElements = 0;
  for (const ElementSet &chunk : chunks) {
    for (const auto [count, kind] : {std::pair{chunk.Nodes_.size(), ElementKind::Node},
                                     std::pair{chunk.Ways_.size(), ElementKind::Way},
                                     std::pair{chunk.Relations_.size(), ElementKind::Relation}}) {
      if (count > maxInputElements - inputElements) {
        return std::unexpected(
            MergeError{.Code = MergeErrorCode::BudgetExceeded, .Kind = kind, .Id = 0});
      }
      inputElements += count;
    }
  }
  ElementSet merged;
  merged.SourceIdentity_ = chunks.front().SourceIdentity_;
  for (const ElementSet &chunk : chunks) {
    merged.Nodes_.insert(merged.Nodes_.end(), chunk.Nodes_.begin(), chunk.Nodes_.end());
    merged.Ways_.insert(merged.Ways_.end(), chunk.Ways_.begin(), chunk.Ways_.end());
    merged.Relations_.insert(
        merged.Relations_.end(), chunk.Relations_.begin(), chunk.Relations_.end());
  }
  if (auto checked = SortUnique(merged.Nodes_, ElementKind::Node); !checked) {
    return std::unexpected(checked.error());
  }
  if (auto checked = SortUnique(merged.Ways_, ElementKind::Way); !checked) {
    return std::unexpected(checked.error());
  }
  if (auto checked = SortUnique(merged.Relations_, ElementKind::Relation); !checked) {
    return std::unexpected(checked.error());
  }
  return merged;
}

const Node *ElementSet::FindNode(uint64_t id) const noexcept {
  return FindById(Nodes_, id);
}

const Way *ElementSet::FindWay(uint64_t id) const noexcept {
  return FindById(Ways_, id);
}

const Relation *ElementSet::FindRelation(uint64_t id) const noexcept {
  return FindById(Relations_, id);
}

std::optional<MissingReference> ElementSet::FirstMissingReference() const noexcept {
  for (const Way &way : Ways_) {
    for (const uint64_t nodeId : way.NodeIds) {
      if (FindNode(nodeId) == nullptr) {
        return MissingReference{.OwnerKind = ElementKind::Way,
                                .OwnerId = way.Id,
                                .MissingKind = ElementKind::Node,
                                .MissingId = nodeId};
      }
    }
  }
  for (const Relation &relation : Relations_) {
    for (const RelationMember &member : relation.Members) {
      const bool found = [&] {
        switch (member.Kind) {
          case ElementKind::Node: return FindNode(member.Id) != nullptr;
          case ElementKind::Way: return FindWay(member.Id) != nullptr;
          case ElementKind::Relation: return FindRelation(member.Id) != nullptr;
        }
        return false;
      }();
      if (!found) {
        return MissingReference{.OwnerKind = ElementKind::Relation,
                                .OwnerId = relation.Id,
                                .MissingKind = member.Kind,
                                .MissingId = member.Id};
      }
    }
  }
  return std::nullopt;
}

std::optional<MissingReference>
ElementSet::FirstMissingReference(std::span<const ElementId> roots) const {
  ReferenceClosure closure{.Source = *this,
                           .Nodes = std::vector<bool>(Nodes_.size()),
                           .Ways = std::vector<bool>(Ways_.size()),
                           .Relations = std::vector<bool>(Relations_.size()),
                           .Pending = {}};
  return closure.Complete(roots);
}

std::expected<ElementSet, MissingReference>
ElementSet::SelectReferenced(std::span<const ElementId> roots) const {
  ReferenceClosure closure{.Source = *this,
                           .Nodes = std::vector<bool>(Nodes_.size()),
                           .Ways = std::vector<bool>(Ways_.size()),
                           .Relations = std::vector<bool>(Relations_.size()),
                           .Pending = {}};
  if (const auto missing = closure.Complete(roots)) { return std::unexpected(*missing); }
  ElementSet result;
  result.SourceIdentity_ = SourceIdentity_;
  result.Nodes_ = CopySelected(Nodes(), closure.Nodes);
  result.Ways_ = CopySelected(Ways(), closure.Ways);
  result.Relations_ = CopySelected(Relations(), closure.Relations);
  return result;
}

}
