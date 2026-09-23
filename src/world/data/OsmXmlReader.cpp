#include "OsmXmlReader.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <expected>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Number.h"
#include "Xml.h"

namespace outshine::Data {

namespace {

std::expected<uint64_t, OsmXmlError> ReadId(const Xml::Ref &element, const char *attribute) {
  const std::optional<std::string> text = element.Said(attribute);
  if (!text || text->empty()) { return std::unexpected(OsmXmlError::InvalidId); }
  uint64_t id = 0;
  const auto parsed = std::from_chars(text->data(), text->data() + text->size(), id);
  if (parsed.ec != std::errc{} || parsed.ptr != text->data() + text->size() || id == 0) {
    return std::unexpected(OsmXmlError::InvalidId);
  }
  return id;
}

std::expected<double, OsmXmlError>
ReadCoordinate(const Xml::Ref &element, const char *attribute, double limit) {
  const std::optional<std::string> text = element.Said(attribute);
  if (!text) { return std::unexpected(OsmXmlError::InvalidCoordinate); }
  const auto value = ParseFiniteNumber(*text);
  if (!value || std::abs(*value) > limit) {
    return std::unexpected(OsmXmlError::InvalidCoordinate);
  }
  return *value;
}

std::expected<void, OsmXmlError> AppendTag(const Xml::Ref &element, std::vector<OsmTag> &tags) {
  const std::optional<std::string> key = element.Said("k");
  const std::optional<std::string> value = element.Said("v");
  if (!key || key->empty() || !value) { return std::unexpected(OsmXmlError::InvalidTag); }
  tags.push_back(OsmTag{.Key = *key, .Value = *value});
  return {};
}

void SortTags(std::vector<OsmTag> &tags) {
  std::ranges::sort(tags, [](const OsmTag &left, const OsmTag &right) {
    if (left.Key != right.Key) { return left.Key < right.Key; }
    return left.Value < right.Value;
  });
}

std::expected<OsmNode, OsmXmlError> ReadNode(const Xml::Ref &element) {
  const auto id = ReadId(element, "id");
  if (!id) { return std::unexpected(id.error()); }
  const auto latitude = ReadCoordinate(element, "lat", 90.0);
  const auto longitude = ReadCoordinate(element, "lon", 180.0);
  if (!latitude || !longitude) { return std::unexpected(OsmXmlError::InvalidCoordinate); }
  OsmNode node{.Id = *id, .LatitudeDeg = *latitude, .LongitudeDeg = *longitude, .Tags = {}};
  for (const Xml::Ref child : element.Children()) {
    if (child.Name() != "tag") { return std::unexpected(OsmXmlError::InvalidDocument); }
    const auto tag = AppendTag(child, node.Tags);
    if (!tag) { return std::unexpected(tag.error()); }
  }
  SortTags(node.Tags);
  return node;
}

std::expected<OsmWay, OsmXmlError> ReadWay(const Xml::Ref &element) {
  const auto id = ReadId(element, "id");
  if (!id) { return std::unexpected(id.error()); }
  OsmWay way{.Id = *id, .NodeIds = {}, .Tags = {}};
  for (const Xml::Ref child : element.Children()) {
    const std::string name = child.Name();
    if (name == "nd") {
      const auto nodeId = ReadId(child, "ref");
      if (!nodeId) { return std::unexpected(nodeId.error()); }
      way.NodeIds.push_back(*nodeId);
    } else if (name == "tag") {
      const auto tag = AppendTag(child, way.Tags);
      if (!tag) { return std::unexpected(tag.error()); }
    } else {
      return std::unexpected(OsmXmlError::InvalidDocument);
    }
  }
  SortTags(way.Tags);
  return way;
}

std::expected<OsmRelationMember, OsmXmlError> ReadMember(const Xml::Ref &element) {
  const auto id = ReadId(element, "ref");
  if (!id) { return std::unexpected(id.error()); }
  const std::string type = element.Attr("type");
  OsmElementKind kind;
  if (type == "node") {
    kind = OsmElementKind::Node;
  } else if (type == "way") {
    kind = OsmElementKind::Way;
  } else if (type == "relation") {
    kind = OsmElementKind::Relation;
  } else {
    return std::unexpected(OsmXmlError::InvalidMember);
  }
  return OsmRelationMember{.Kind = kind, .Id = *id, .Role = element.Attr("role")};
}

std::expected<OsmRelation, OsmXmlError> ReadRelation(const Xml::Ref &element) {
  const auto id = ReadId(element, "id");
  if (!id) { return std::unexpected(id.error()); }
  OsmRelation relation{.Id = *id, .Members = {}, .Tags = {}};
  for (const Xml::Ref child : element.Children()) {
    const std::string name = child.Name();
    if (name == "member") {
      auto member = ReadMember(child);
      if (!member) { return std::unexpected(member.error()); }
      relation.Members.push_back(std::move(*member));
    } else if (name == "tag") {
      const auto tag = AppendTag(child, relation.Tags);
      if (!tag) { return std::unexpected(tag.error()); }
    } else {
      return std::unexpected(OsmXmlError::InvalidDocument);
    }
  }
  SortTags(relation.Tags);
  return relation;
}

template <typename Element> bool SortUnique(std::vector<Element> &elements) {
  std::ranges::sort(elements, {}, &Element::Id);
  return std::adjacent_find(
             elements.begin(), elements.end(), [](const Element &left, const Element &right) {
               return left.Id == right.Id;
             }) == elements.end();
}

}

std::expected<OsmElements, OsmXmlError> OsmXmlReader::Read(std::string_view xml,
                                                           OsmSourceIdentity identity) {
  if (identity.DatasetId.empty() || identity.Revision.empty()) {
    return std::unexpected(OsmXmlError::InvalidSourceIdentity);
  }
  Xml document;
  if (!document.Parse(xml.data(), xml.size())) {
    return std::unexpected(OsmXmlError::InvalidDocument);
  }
  const Xml::Ref root = document.Root();
  if (root.Name() != "osm" || root.Attr("version") != "0.6") {
    return std::unexpected(OsmXmlError::UnsupportedRoot);
  }
  OsmElements elements;
  elements.SourceIdentity_ = std::move(identity);
  for (const Xml::Ref child : root.Children()) {
    const std::string name = child.Name();
    if (name == "node") {
      auto node = ReadNode(child);
      if (!node) { return std::unexpected(node.error()); }
      elements.Nodes_.push_back(std::move(*node));
    } else if (name == "way") {
      auto way = ReadWay(child);
      if (!way) { return std::unexpected(way.error()); }
      elements.Ways_.push_back(std::move(*way));
    } else if (name == "relation") {
      auto relation = ReadRelation(child);
      if (!relation) { return std::unexpected(relation.error()); }
      elements.Relations_.push_back(std::move(*relation));
    } else if (name != "bounds" && name != "note" && name != "meta") {
      return std::unexpected(OsmXmlError::InvalidDocument);
    }
  }
  if (!SortUnique(elements.Nodes_) || !SortUnique(elements.Ways_) ||
      !SortUnique(elements.Relations_)) {
    return std::unexpected(OsmXmlError::DuplicateElement);
  }
  return elements;
}

}
