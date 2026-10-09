#include "OsmField.h"
#include "BinaryValueArchive.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
constexpr uint32_t kNativeTileVersion = 0x314d534f;
constexpr int kMaximumTileZoom = std::numeric_limits<int>::digits - 1;

template <class Archive, class T> using Transfer = std::conditional_t<Archive::Reading, T, const T>;

struct NativeWriter : BinaryValueWriter {
  static constexpr bool Reading = false;
  using BinaryValueWriter::BinaryValueWriter;

  bool String(std::string_view text) { return Text(text); }

  template <class T> bool Array(const std::vector<T> &items) {
    return BinaryValueWriter::Array(std::span(items));
  }

  template <class T, class Each> bool Sequence(const std::vector<T> &items, Each each) {
    if (items.size() > std::numeric_limits<uint32_t>::max() ||
        !(*this)(static_cast<uint32_t>(items.size()))) {
      return false;
    }
    return std::ranges::all_of(items, [&](const auto &item) { return each(*this, item); });
  }
};

struct NativeReader : BinaryValueReader {
  static constexpr bool Reading = true;

  NativeReader(std::span<const uint8_t> bytes, size_t most)
      : BinaryValueReader(bytes), Remaining(most) {}

  bool String(std::string &text) {
    uint32_t length = 0;
    if (!(*this)(length) || length > Remaining) { return false; }
    const auto bytes = In.Take(length);
    if (!bytes) { return false; }
    Remaining -= length;
    text.assign(reinterpret_cast<const char *>(bytes->data()), bytes->size());
    return true;
  }

  template <class T> bool Array(std::vector<T> &items) {
    auto header = In;
    uint32_t count = 0;
    if (!header.Number(count) || count > Remaining / sizeof(T)) { return false; }
    Remaining -= static_cast<size_t>(count) * sizeof(T);
    return BinaryValueReader::Array(items);
  }

  template <class T, class Each> bool Sequence(std::vector<T> &items, Each each) {
    uint32_t count = 0;
    if (!(*this)(count) || count > In.Remaining() || count > Remaining / sizeof(T)) {
      return false;
    }
    Remaining -= static_cast<size_t>(count) * sizeof(T);
    items.resize(count);
    return std::ranges::all_of(items, [&](auto &item) { return each(*this, item); });
  }

  size_t Remaining;
};

template <class Archive, class T> bool Optional(Archive &archive, T &value) {
  Transfer<Archive, bool> present = value.has_value();
  if (!archive(present)) { return false; }
  if constexpr (Archive::Reading) {
    if (present) {
      value.emplace();
    } else {
      value.reset();
    }
  }
  return !present || archive(*value);
}

bool Range(uint32_t first, uint32_t count, size_t size) {
  return first <= size && count <= size - first;
}
}

struct OsmField::NativeCodec {
  template <class Archive, class Field> static bool Values(Archive &archive, Field &field) {
    const auto text = [](auto &in, auto &value) { return in.String(value); };
    return archive.Sequence(field.Layers_, text) && archive.Array(field.Points_) &&
           archive.Array(field.Tags_) && archive.Sequence(field.Keys_, text) &&
           archive.Sequence(field.Strings_, text) &&
           archive.Sequence(
               field.Values_,
               [](auto &in, auto &value) { return in(value.Num, value.Str, value.IsNum); }) &&
           archive.Sequence(
               field.Rings_,
               [](auto &in, auto &ring) { return in(ring.First, ring.Count, ring.Exterior); }) &&
           archive.Sequence(field.Features_, [](auto &in, auto &feature) {
             return in(feature.FirstRing,
                       feature.RingCount,
                       feature.FirstTag,
                       feature.TagCount,
                       feature.Tile,
                       feature.Layer,
                       feature.Type) &&
                    Optional(in, feature.ProviderFeatureId) &&
                    in(feature.MinLat, feature.MinLon, feature.MaxLat, feature.MaxLon);
           });
  }

  template <class Archive, class Field> static bool Tile(Archive &archive, Field &field) {
    auto &tile = field.Tiles_.front();
    auto &source = tile.Source;
    Transfer<Archive, bool> native = source.NativeCell.has_value();
    if (!archive(tile.Z,
                 tile.X,
                 tile.Y,
                 tile.FirstFeature,
                 tile.FeatureCount,
                 source.From,
                 source.Kind,
                 source.Tile.Zoom,
                 source.Tile.X,
                 source.Tile.Y,
                 native)) {
      return false;
    }
    if constexpr (Archive::Reading) {
      if (native) { source.NativeCell.emplace(); }
    }
    return (!native || archive(source.NativeCell->SouthDeg, source.NativeCell->WestDeg)) &&
           archive.String(source.SourceId) && archive.String(source.Revision) &&
           archive.String(tile.InputDigest);
  }

  template <class Archive, class Field> static bool Metadata(Archive &archive, Field &field) {
    Transfer<Archive, uint64_t> missing{static_cast<uint64_t>(field.Missing_)};
    if (!archive(field.Zoom_, field.Schema_, field.Extent_, missing)) { return false; }
    if constexpr (Archive::Reading) {
      if (missing > std::numeric_limits<uint16_t>::max() + uint64_t{1}) { return false; }
      field.Missing_ = static_cast<long>(missing);
    }
    return true;
  }

  static bool FeaturesValid(const OsmField &field) {
    for (const auto &feature : field.Features_) {
      if (feature.Tile != 0 || feature.Layer >= field.Layers_.size() || feature.Type < 1 ||
          feature.Type > 3 || feature.TagCount % 2 != 0 || !std::isfinite(feature.MinLat) ||
          !std::isfinite(feature.MinLon) || !std::isfinite(feature.MaxLat) ||
          !std::isfinite(feature.MaxLon) ||
          !Range(feature.FirstRing, feature.RingCount, field.Rings_.size()) ||
          !Range(feature.FirstTag, feature.TagCount, field.Tags_.size())) {
        return false;
      }
      for (uint32_t at = 0; at < feature.RingCount; ++at) {
        const auto &ring = field.Rings_[feature.FirstRing + at];
        for (uint32_t point = 0; point < ring.Count; ++point) {
          const size_t index = (static_cast<size_t>(ring.First) + point) * 2;
          const double lat = field.Points_[index];
          const double lon = field.Points_[index + 1];
          if (lat < feature.MinLat || lat > feature.MaxLat || lon < feature.MinLon ||
              lon > feature.MaxLon) {
            return false;
          }
        }
      }
    }
    return true;
  }

  static bool Valid(const OsmField &field) {
    if (field.Tiles_.size() != 1 || field.Schema_ > MvtSchema::OpenMapTiles || field.Zoom_ < 0 ||
        field.Zoom_ > kMaximumTileZoom || field.Extent_ <= 0 ||
        field.Layers_.size() > std::numeric_limits<uint16_t>::max() + size_t{1} ||
        field.Points_.size() % 2 != 0 || field.Tags_.size() % 2 != 0 || field.Missing_ < 0 ||
        static_cast<size_t>(field.Missing_) > field.Layers_.size()) {
      return false;
    }
    const auto &tile = field.Tiles_.front();
    const auto side = uint64_t{1} << static_cast<unsigned>(field.Zoom_);
    if (tile.Z != field.Zoom_ || tile.X < 0 || tile.Y < 0 || std::cmp_greater_equal(tile.X, side) ||
        std::cmp_greater_equal(tile.Y, side) || tile.FirstFeature != 0 ||
        tile.FeatureCount != field.Features_.size() ||
        tile.Source.Kind != Data::DataKind::VectorMap ||
        tile.Source.From > Data::TileSourceIdentity::Origin::Direct ||
        tile.Source.Tile.Zoom != tile.Z || std::cmp_not_equal(tile.Source.Tile.X, tile.X) ||
        std::cmp_not_equal(tile.Source.Tile.Y, tile.Y) || tile.InputDigest.size() != 64 ||
        !std::ranges::all_of(
            tile.InputDigest,
            [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); })) {
      return false;
    }
    for (const auto &ring : field.Rings_) {
      if (!Range(ring.First, ring.Count, field.Points_.size() / 2)) { return false; }
    }
    for (size_t at = 0; at < field.Tags_.size(); at += 2) {
      if (field.Tags_[at] >= field.Keys_.size() || field.Tags_[at + 1] >= field.Values_.size()) {
        return false;
      }
    }
    return std::ranges::all_of(field.Points_, [](double value) { return std::isfinite(value); }) &&
           std::ranges::all_of(field.Values_,
                               [&](const auto &value) {
                                 return value.IsNum ? std::isfinite(value.Num)
                                                    : value.Str < field.Strings_.size();
                               }) &&
           FeaturesValid(field);
  }
};

std::optional<std::vector<uint8_t>> OsmField::EncodeNativeTile(size_t bytesMost) const {
  if (!NativeCodec::Valid(*this)) { return std::nullopt; }
  NativeWriter out(bytesMost);
  if (!out(kNativeTileVersion) || !NativeCodec::Metadata(out, *this) ||
      !NativeCodec::Tile(out, *this) || !NativeCodec::Values(out, *this)) {
    return std::nullopt;
  }
  return std::move(out.Out).TakeBytes();
}

std::unique_ptr<OsmField> OsmField::DecodeNativeTile(std::span<const uint8_t> bytes,
                                                     size_t bytesMost) {
  if (bytes.size() > bytesMost) { return {}; }
  NativeReader in(bytes, bytesMost);
  uint32_t version = 0;
  auto field = std::make_unique<OsmField>(0, std::span<const std::string>{});
  field->Tiles_.resize(1);
  if (!in(version) || version != kNativeTileVersion || !NativeCodec::Metadata(in, *field) ||
      !NativeCodec::Tile(in, *field) || !NativeCodec::Values(in, *field) ||
      in.In.Remaining() != 0 || !NativeCodec::Valid(*field)) {
    return {};
  }
  const auto &tile = field->Tiles_.front();
  field->CentreX_ = tile.X;
  field->CentreY_ = tile.Y;
  field->Stage_ = SnapshotStage::Complete;
  field->PublishedSettledTiles_ = 1;
  field->RequestedRing_ = 0;
  field->Pending_ = 0;
  field->Generation_ = 1;
  field->Settled_.push_back((static_cast<uint64_t>(static_cast<uint32_t>(tile.X)) << 32u) |
                            static_cast<uint32_t>(tile.Y));
  return field->HeapBytes() <= bytesMost ? std::move(field) : nullptr;
}
}
