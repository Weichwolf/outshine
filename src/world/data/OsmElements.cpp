#include "OsmElements.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
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

template <typename Element>
std::expected<void, OsmMergeError> SortUnique(std::vector<Element> &elements, OsmElementKind kind) {
  std::ranges::sort(elements, {}, &Element::Id);
  size_t unique = 0;
  for (size_t read = 0; read < elements.size(); ++read) {
    if (unique > 0 && elements[read].Id == elements[unique - 1].Id) {
      if (elements[read] != elements[unique - 1]) {
        return std::unexpected(OsmMergeError{
            .Code = OsmMergeErrorCode::ConflictingElement, .Kind = kind, .Id = elements[read].Id});
      }
      continue;
    }
    if (read != unique) { elements[unique] = std::move(elements[read]); }
    ++unique;
  }
  elements.resize(unique);
  return {};
}

}

std::expected<OsmElements, OsmMergeError> OsmElements::Merge(std::span<const OsmElements> chunks,
                                                             size_t maxInputElements) {
  if (chunks.empty()) {
    return std::unexpected(OsmMergeError{.Code = OsmMergeErrorCode::EmptyInput});
  }
  for (const OsmElements &chunk : chunks.subspan(1)) {
    if (chunk.SourceIdentity_ != chunks.front().SourceIdentity_) {
      return std::unexpected(OsmMergeError{.Code = OsmMergeErrorCode::IdentityMismatch});
    }
  }
  size_t inputElements = 0;
  for (const OsmElements &chunk : chunks) {
    for (const auto [count, kind] :
         {std::pair{chunk.Nodes_.size(), OsmElementKind::Node},
          std::pair{chunk.Ways_.size(), OsmElementKind::Way},
          std::pair{chunk.Relations_.size(), OsmElementKind::Relation}}) {
      if (count > maxInputElements - inputElements) {
        return std::unexpected(
            OsmMergeError{.Code = OsmMergeErrorCode::BudgetExceeded, .Kind = kind, .Id = 0});
      }
      inputElements += count;
    }
  }
  OsmElements merged;
  merged.SourceIdentity_ = chunks.front().SourceIdentity_;
  for (const OsmElements &chunk : chunks) {
    merged.Nodes_.insert(merged.Nodes_.end(), chunk.Nodes_.begin(), chunk.Nodes_.end());
    merged.Ways_.insert(merged.Ways_.end(), chunk.Ways_.begin(), chunk.Ways_.end());
    merged.Relations_.insert(
        merged.Relations_.end(), chunk.Relations_.begin(), chunk.Relations_.end());
  }
  if (auto checked = SortUnique(merged.Nodes_, OsmElementKind::Node); !checked) {
    return std::unexpected(checked.error());
  }
  if (auto checked = SortUnique(merged.Ways_, OsmElementKind::Way); !checked) {
    return std::unexpected(checked.error());
  }
  if (auto checked = SortUnique(merged.Relations_, OsmElementKind::Relation); !checked) {
    return std::unexpected(checked.error());
  }
  return merged;
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

std::optional<MissingOsmReference>
OsmElements::FirstMissingReference(std::span<const OsmElementId> roots) const {
  std::vector<bool> visitedWays(Ways_.size());
  std::vector<bool> visitedRelations(Relations_.size());
  std::vector<OsmElementId> pending;
  const auto admit = [&](OsmElementId owner,
                         OsmElementId target) -> std::optional<MissingOsmReference> {
    switch (target.Kind) {
      case OsmElementKind::Node:
        if (FindNode(target.Id)) { return std::nullopt; }
        break;
      case OsmElementKind::Way:
        if (const OsmWay *way = FindWay(target.Id)) {
          const auto index = static_cast<size_t>(way - Ways_.data());
          if (!visitedWays[index]) {
            visitedWays[index] = true;
            pending.push_back(target);
          }
          return std::nullopt;
        }
        break;
      case OsmElementKind::Relation:
        if (const OsmRelation *relation = FindRelation(target.Id)) {
          const auto index = static_cast<size_t>(relation - Relations_.data());
          if (!visitedRelations[index]) {
            visitedRelations[index] = true;
            pending.push_back(target);
          }
          return std::nullopt;
        }
        break;
    }
    return MissingOsmReference{.OwnerKind = owner.Kind,
                               .OwnerId = owner.Id,
                               .MissingKind = target.Kind,
                               .MissingId = target.Id};
  };
  for (const OsmElementId root : roots) {
    if (const auto missing = admit(root, root)) { return missing; }
  }
  for (size_t next = 0; next < pending.size(); ++next) {
    const OsmElementId owner = pending[next];
    if (owner.Kind == OsmElementKind::Way) {
      for (const uint64_t node : FindWay(owner.Id)->NodeIds) {
        if (const auto missing = admit(owner, {.Kind = OsmElementKind::Node, .Id = node})) {
          return missing;
        }
      }
    } else {
      for (const OsmRelationMember &member : FindRelation(owner.Id)->Members) {
        if (const auto missing = admit(owner, {.Kind = member.Kind, .Id = member.Id})) {
          return missing;
        }
      }
    }
  }
  return std::nullopt;
}

}
