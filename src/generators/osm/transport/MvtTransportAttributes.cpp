#include "MvtTransportSnapshotInternal.h"
#include "OsmField.h"
#include "StreetField.h"
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace outshine::Generators::Osm::MvtTransport {
namespace {
struct Tags {
  static constexpr size_t kCapacity = 16;
  std::array<WayTagView, kCapacity> Values{};
  std::array<std::array<char, 64>, kCapacity> Numbers{};
  size_t Count = 0;

  bool Read(const OsmField &field, const OsmField::Feature &feature, std::string_view key) {
    const auto value = field.ReadAttribute(feature, key);
    if (!value) { return true; }
    std::string_view text = value->String;
    if (value->IsNumber) {
      if (!std::isfinite(value->Number)) { return false; }
      auto &buffer = Numbers[Count];
      const auto written =
          std::to_chars(buffer.data(), buffer.data() + buffer.size(), value->Number);
      if (written.ec != std::errc{}) { return false; }
      text = {buffer.data(), written.ptr};
    }
    Values[Count++] = {.Key = key, .Value = text};
    return true;
  }
};

uint16_t Bit(World::TransportAttribute field) {
  return static_cast<uint16_t>(field);
}

void Mark(Way &way, World::TransportAttribute attribute, bool supplied, bool changed = false) {
  if (supplied) { way.Supplied |= Bit(attribute); }
  if (!supplied || changed) { way.Derived |= Bit(attribute); }
}

void MarkAttributes(Way &out,
                    const OsmField &field,
                    const OsmField::Feature &feature,
                    const StreetField::Way &street,
                    bool classified) {
  using A = World::TransportAttribute;
  Mark(out, A::Modes, classified);
  Mark(out, A::Facility, classified);
  Mark(out, A::Access, field.Has(feature, "access"));
  Mark(out, A::Surface, field.Has(feature, "surface"));
  Mark(out, A::Layer, field.Has(feature, "layer"));
  Mark(out, A::Bridge, field.Has(feature, "bridge"));
  Mark(out, A::Tunnel, field.Has(feature, "tunnel"));
  Mark(out, A::Direction, field.Has(feature, "oneway"));
  const double width = static_cast<double>(street.HalfWidthM) * 2.0;
  Mark(out, A::Width, field.Has(feature, "width"), width != out.Attributes.WidthM);
  out.Attributes.WidthM = width;
  const auto lanes = static_cast<uint8_t>(street.Lanes);
  Mark(out, A::Lanes, field.Has(feature, "lanes"), lanes != out.Attributes.LaneCount);
  out.Attributes.LaneCount = lanes;
}

std::string_view FallbackKind(StreetField::Traffic traffic) {
  if (traffic == StreetField::Traffic::Rail) { return "rail"; }
  if (traffic == StreetField::Traffic::Path) { return "path"; }
  return "road";
}

struct Description {
  OsmWaySemantics Attributes;
  bool Classified = false;
};

std::expected<Description, std::string> ReadAttributes(const OsmField &field,
                                                       const OsmField::Feature &feature,
                                                       const StreetField::Way &street) {
  Tags tags;
  constexpr std::array<std::string_view, 13> kKeys{"highway",
                                                   "railway",
                                                   "waterway",
                                                   "route",
                                                   "oneway",
                                                   "junction",
                                                   "layer",
                                                   "bridge",
                                                   "tunnel",
                                                   "access",
                                                   "width",
                                                   "lanes",
                                                   "surface"};
  for (const auto key : kKeys) {
    if (!tags.Read(field, feature, key)) { return std::unexpected("Invalid MVT transport tag"); }
  }
  const auto kind = field.Str(feature, "kind");
  const bool classified = field.Has(feature, "highway") || field.Has(feature, "railway") ||
                          field.Has(feature, "waterway") || field.Has(feature, "route") ||
                          !kind.empty();
  if (!field.Has(feature, "highway") && !field.Has(feature, "railway")) {
    tags.Values[tags.Count++] = {
        .Key = street.TrafficKind == StreetField::Traffic::Rail ? "railway" : "highway",
        .Value = kind.empty() ? "road" : kind};
  }
  auto attributes = DescribeOsmWay(std::span(tags.Values).first(tags.Count));
  if (!attributes) { return std::unexpected("Invalid MVT transport attributes"); }
  if (attributes->Modes != 0) {
    return Description{.Attributes = *attributes, .Classified = classified};
  }
  tags.Count = 0;
  for (const auto key : kKeys) {
    if (key == "highway" || key == "railway" || key == "waterway" || key == "route") { continue; }
    if (!tags.Read(field, feature, key)) { return std::unexpected("Invalid MVT transport tag"); }
  }
  tags.Values[tags.Count++] = {.Key = street.TrafficKind == StreetField::Traffic::Rail ? "railway"
                                                                                       : "highway",
                               .Value = FallbackKind(street.TrafficKind)};
  attributes = DescribeOsmWay(std::span(tags.Values).first(tags.Count));
  if (!attributes) { return std::unexpected("Invalid MVT transport recipe attributes"); }
  return Description{.Attributes = *attributes, .Classified = false};
}
}

std::expected<Way, std::string>
DescribeWay(const OsmField &field, const StreetField &streets, size_t index) {
  const auto reference = streets.SourceOf(field, index);
  if (!reference || reference->Feature >= field.Features().size()) {
    return std::unexpected("MVT transport street lease does not match the source field");
  }
  const auto &feature = field.Features()[reference->Feature];
  const auto &street = streets.Ways()[index];
  if (feature.Tile >= field.Tiles().size() || !feature.HasContentDigest ||
      reference->Ring < feature.FirstRing ||
      reference->Ring - feature.FirstRing >= feature.RingCount) {
    return std::unexpected("MVT transport street lacks its actual feature fragment");
  }
  Way out;
  out.PointCount = street.PointCount;
  out.Feature = feature.ContentDigest;
  out.Source.ProviderElementId = feature.ProviderFeatureId;
  out.Source.Fragment = feature.ContentDigest;
  const auto described = ReadAttributes(field, feature, street);
  if (!described) { return out; }
  const auto &attributes = described->Attributes;
  if (street.Lanes < 0 || street.Lanes > 32 || !std::isfinite(street.HalfWidthM) ||
      street.HalfWidthM <= 0.0f) {
    return out;
  }
  out.Attributes = attributes;
  if (!field.Has(feature, "oneway") && street.Oneway) {
    out.Attributes.Travel = OsmWayTravel::Forward;
  }
  MarkAttributes(out, field, feature, street, described->Classified);
  out.Usable = !attributes.Tunnel && street.Form == StreetField::Shape::Ribbon;
  return out;
}
}
