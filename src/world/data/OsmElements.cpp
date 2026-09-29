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

struct ReferenceClosure {
  const OsmElements &Source;
  std::vector<bool> Ways;
  std::vector<bool> Relations;
  std::vector<OsmElementId> Pending;

  std::optional<MissingOsmReference> Admit(OsmElementId owner, OsmElementId target) {
    switch (target.Kind) {
      case OsmElementKind::Node:
        if (Source.FindNode(target.Id)) { return std::nullopt; }
        break;
      case OsmElementKind::Way:
        if (const OsmWay *way = Source.FindWay(target.Id)) {
          const auto index = static_cast<size_t>(way - Source.Ways().data());
          if (!Ways[index]) {
            Ways[index] = true;
            Pending.push_back(target);
          }
          return std::nullopt;
        }
        break;
      case OsmElementKind::Relation:
        if (const OsmRelation *relation = Source.FindRelation(target.Id)) {
          const auto index = static_cast<size_t>(relation - Source.Relations().data());
          if (!Relations[index]) {
            Relations[index] = true;
            Pending.push_back(target);
          }
          return std::nullopt;
        }
        break;
    }
    return MissingOsmReference{.OwnerKind = owner.Kind,
                               .OwnerId = owner.Id,
                               .MissingKind = target.Kind,
                               .MissingId = target.Id};
  }
};

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
  ReferenceClosure closure{.Source = *this,
                           .Ways = std::vector<bool>(Ways_.size()),
                           .Relations = std::vector<bool>(Relations_.size()),
                           .Pending = {}};
  for (const OsmElementId root : roots) {
    if (const auto missing = closure.Admit(root, root)) { return missing; }
  }
  size_t next = 0;
  while (next < closure.Pending.size()) {
    const OsmElementId owner = closure.Pending[next++];
    if (owner.Kind == OsmElementKind::Way) {
      for (const uint64_t node : FindWay(owner.Id)->NodeIds) {
        if (const auto missing = closure.Admit(owner, {.Kind = OsmElementKind::Node, .Id = node})) {
          return missing;
        }
      }
    } else {
      for (const OsmRelationMember &member : FindRelation(owner.Id)->Members) {
        if (const auto missing = closure.Admit(owner, {.Kind = member.Kind, .Id = member.Id})) {
          return missing;
        }
      }
    }
  }
  return std::nullopt;
}

}
