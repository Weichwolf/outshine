#include "StreetField.h"
#include <cstdint>
#include <span>
#include <optional>
#include <cstddef>
#include <utility>
#include <bit>
#include "Digest.h"

namespace outshine::Generators::Osm {

using namespace outshine::Ground;

namespace {

constexpr uint32_t kMaxRingPoints = 512;

uint64_t DigestOfWays(const OsmField &field, std::span<const StreetField::Way> ways) {
  uint64_t digest = kDigestBasis;
  const auto fold = [&digest](uint64_t word) {
    for (unsigned shift = 0; shift < 64u; shift += 8u) {
      digest = DigestFolded(digest, static_cast<uint8_t>(word >> shift));
    }
  };
  const std::span<const double> points = field.Points();
  for (const StreetField::Way &way : ways) {
    const size_t first = way.FirstPoint;
    const size_t count = way.PointCount;
    if (count < 2 || first + count > points.size() / 2u) { continue; }
    fold(count);
    fold(std::bit_cast<uint32_t>(way.HalfWidthM));
    for (size_t at = 0; at < 2u * count; ++at) {
      fold(std::bit_cast<uint64_t>(points[2u * first + at]));
    }
  }
  return digest;
}

}

uint32_t StreetField::Ingest(const OsmField &field, const VegetationTemplates &veg) {
  if (SourceOrigin_.get() != field.OriginToken() || SourceGeneration_ != field.Generation()) {
    *this = StreetField{};
    SourceOrigin_ = field.ShareOriginToken();
    SourceGeneration_ = field.Generation();
  }
  const std::span<const OsmField::Feature> feats = field.Features();
  if (Mark_.Done(feats)) { return static_cast<uint32_t>(Ways_.size()); }

  const TileWatermark::Next next =
      Mark_.Ask(feats,
                field.Tiles(),
                {.CentreX = field.CentreX(), .CentreY = field.CentreY(), .Rings = kEveryRing},
                [](size_t, size_t) { return true; });
  if (!next.Found) { return static_cast<uint32_t>(Ways_.size()); }
  Mark_.Take(next.Tile);
  Mark_.Advance(feats);

  const int lines = field.Layer(OsmLayer::Streets);
  const int areas = field.Layer(OsmLayer::StreetPolygons);
  const auto firstWay = static_cast<uint32_t>(Ways_.size());

  Looked_ += static_cast<long>(next.To - next.From);
  for (size_t c = next.From; c < next.To; c++) {
    const OsmField::Feature &f = feats[c];
    const bool ribbon = f.Type == 2 && std::cmp_equal(f.Layer, lines);
    const bool area = f.Type == 3 && std::cmp_equal(f.Layer, areas);
    if (!ribbon && !area) { continue; }

    if (field.Num(f, "tunnel", 0.0) > 0.5) {
      Tunnels_++;
      continue;
    }

    const VegetationTemplates::Rule *rule =
        veg.Find(field.LayerName(static_cast<int>(f.Layer)), field.Str(f, "kind"));
    if (rule == nullptr) {
      Unruled_++;
      continue;
    }
    if (ribbon && rule->WidthM <= 0.0f) {
      Unwidthed_++;
      continue;
    }

    AppendFeature(field, f, *rule, ribbon ? Shape::Ribbon : Shape::Area);
  }

  ByTile_.Set(next.Tile, firstWay, static_cast<uint32_t>(Ways_.size()));
  if (next.Tile >= SourceDigests_.size()) {
    SourceDigests_.resize(static_cast<size_t>(next.Tile) + 1, kDigestBasis);
  }
  SourceDigests_[next.Tile] = DigestOfWays(field, OfTile(static_cast<int>(next.Tile)));
  return static_cast<uint32_t>(Ways_.size());
}

std::optional<uint64_t> StreetField::SourceDigest(const OsmField &field,
                                                  uint32_t tile) const noexcept {
  if (SourceOrigin_ &&
      (SourceOrigin_.get() != field.OriginToken() || SourceGeneration_ != field.Generation())) {
    return std::nullopt;
  }
  return tile < SourceDigests_.size() ? SourceDigests_[tile] : kDigestBasis;
}

void StreetField::AppendFeature(const OsmField &field,
                                const OsmField::Feature &feature,
                                const VegetationTemplates::Rule &rule,
                                Shape shape) {
  const auto layer = field.Integer(feature, "layer");
  if (!layer) {
    ++InvalidLayers_;
    return;
  }
  const bool ribbon = shape == Shape::Ribbon;
  const bool area = shape == Shape::Area;
  for (uint32_t r = 0; r < feature.RingCount; r++) {
    const OsmField::Ring &ring = field.Rings()[feature.FirstRing + r];
    if (ring.Count > kMaxRingPoints) { continue; }
    if (ribbon && ring.Count < 2) { continue; }
    if (area && (!ring.Exterior || ring.Count < 3)) { continue; }

    Way w{};
    w.FirstPoint = ring.First;
    w.PointCount = ring.Count;
    w.HalfWidthM = ribbon ? rule.WidthM * 0.5f : 0.0f;
    w.CoverRow = static_cast<int32_t>(rule.Tpl);
    w.Form = ribbon ? Shape::Ribbon : Shape::Area;
    w.Lanes = rule.Lanes;
    w.Bridge = field.Num(feature, "bridge", 0.0) > 0.5;
    w.Layer = layer->value_or(0);
    w.ClearanceM = rule.ClearanceM;
    w.MaxGradient = rule.MaxGradient;
    w.SpeedMps = rule.SpeedMps;
    w.Priority = rule.Priority;
    w.Sealed = rule.Sealed;
    w.Oneway = rule.Oneway;
    Bridges_ += w.Bridge ? 1 : 0;
    Layered_ += w.Layer != 0 ? 1 : 0;
    LayerSaid_ += layer->has_value() ? 1 : 0;
    Ways_.push_back(w);
  }
}

}
