#include "MvtLayer.h"
#include "Sha256.h"
#include "Capacity.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <bit>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {

size_t MvtLayer::HeapBytes() const {
  size_t strings = 0;
  for (const std::string &key : Keys_) { strings += key.capacity(); }
  for (const std::string &value : ValueStrs_) { strings += value.capacity(); }
  return CapacityBytes(Features_) + CapacityBytes(Rings_) + CapacityBytes(Points_) +
         CapacityBytes(Tags_) + CapacityBytes(Keys_) + CapacityBytes(Values_) +
         CapacityBytes(ValueStrs_) + (ValueIsNum_.capacity() + 7u) / 8u + strings;
}

constexpr int kVarintShiftMost = 63;

constexpr uint64_t kVarintPayload = 0x7fu;

namespace Says {
constexpr std::string_view kInvalidMvtFeature =
    "vector tile feature contains an invalid field or integer";
constexpr std::string_view kInvalidMvtValue =
    "vector tile value has an invalid wire encoding or value type";
}

namespace {

struct FieldHeader {
  uint32_t Number = 0;
  uint32_t Wire = 0;
};

struct Reader {
  const uint8_t *P, *End;
  bool Ok = true;

  uint64_t Varint() {
    uint64_t r = 0;
    int s = 0;
    while (P < End) {
      const uint8_t b = *P++;
      if (s == kVarintShiftMost && b > 1) {
        Ok = false;
        return 0;
      }
      r |= static_cast<uint64_t>(b & kVarintPayload) << static_cast<uint32_t>(s);
      if ((b & 0x80u) == 0) { return r; }
      s += 7;
      if (s > kVarintShiftMost) { break; }
    }
    Ok = false;
    return 0;
  }

  [[nodiscard]] bool ReadField(FieldHeader &field) {
    if (P >= End) { return false; }
    const uint64_t k = Varint();
    if (!Ok || k > std::numeric_limits<uint32_t>::max() || (k >> 3u) == 0) {
      Ok = false;
      return false;
    }
    field.Number = static_cast<uint32_t>(k >> 3u);
    field.Wire = static_cast<uint32_t>(k & 7u);
    return true;
  }

  template <typename UInt> UInt Fixed() {
    static_assert(std::is_unsigned_v<UInt>);
    if (std::cmp_less(End - P, sizeof(UInt))) {
      Ok = false;
      return 0;
    }
    UInt bits = 0;
    for (size_t i = 0; i < sizeof(UInt); ++i) {
      bits |= static_cast<UInt>(*P++)
              << static_cast<unsigned>(i * std::numeric_limits<uint8_t>::digits);
    }
    return bits;
  }

  Reader Bytes() {
    const uint64_t n = Varint();
    Reader r{.P = P, .End = P, .Ok = false};
    if (!Ok || std::cmp_less(End - P, n)) {
      Ok = false;
      return r;
    }
    r = Reader{.P = P, .End = P + n, .Ok = true};
    P += n;
    return r;
  }

  [[nodiscard]] bool Skip(uint32_t wire) {
    switch (wire) {
      case 0: Varint(); return Ok;
      case 1:
        if (End - P < 8) {
          Ok = false;
          return false;
        }
        P += 8;
        return true;
      case 2: {
        const uint64_t n = Varint();
        if (!Ok || std::cmp_less(End - P, n)) {
          Ok = false;
          return false;
        }
        P += n;
        return true;
      }
      case 5:
        if (End - P < 4) {
          Ok = false;
          return false;
        }
        P += 4;
        return true;
      default: Ok = false; return false;
    }
  }
};

struct LayerHeader {
  std::string_view Name;
  std::optional<uint32_t> Version;
  uint32_t Extent = 0;
  bool HasName = false;
  bool Complete = false;
};

[[nodiscard]] LayerHeader ReadLayerHeader(Reader reader) {
  LayerHeader header;
  FieldHeader field;
  while (reader.ReadField(field)) {
    if (field.Number == 1 && field.Wire == 2) {
      const Reader name = reader.Bytes();
      if (!reader.Ok) { return header; }
      header.Name = {reinterpret_cast<const char *>(name.P),
                     static_cast<size_t>(name.End - name.P)};
      header.HasName = true;
    } else if ((field.Number == 5 || field.Number == 15) && field.Wire == 0) {
      const auto value = reader.Varint();
      if (!reader.Ok || value > std::numeric_limits<uint32_t>::max()) { return header; }
      if (field.Number == 5) {
        header.Extent = static_cast<uint32_t>(value);
      } else {
        header.Version = static_cast<uint32_t>(value);
      }
    } else if (!reader.Skip(field.Wire)) {
      return header;
    }
  }
  header.Complete = reader.Ok && header.HasName;
  return header;
}

[[nodiscard]] bool ValidTags(std::span<const uint32_t> tags, size_t keyCount, size_t valueCount) {
  if (tags.size() % 2 != 0) { return false; }
  for (size_t i = 0; i < tags.size(); i += 2) {
    if (tags[i] >= keyCount || tags[i + 1] >= valueCount) { return false; }
  }
  return true;
}

class GeometryReader {
public:
  GeometryReader(std::span<const uint32_t> words,
                 std::vector<int32_t> &points,
                 std::vector<MvtLayer::Ring> &rings)
      : Words_(words), Points_(points), Rings_(rings) {}

  [[nodiscard]] bool Read(int type) {
    if (type == 0) { return true; }
    if (Words_.empty()) { return false; }
    if (type == 1) {
      const size_t first = Points_.size() / 2;
      return ReadCommand(1, 1, false) && Words_.empty() && AppendPart(first, true);
    }
    const size_t firstRing = Rings_.size();
    while (!Words_.empty()) {
      const size_t first = Points_.size() / 2;
      if (!ReadCommand(1, 1, true) || !ReadCommand(2, type == 3 ? 2u : 1u, false)) { return false; }
      if (type == 3 && !Close(first)) { return false; }
      const int orientation = type == 3 ? AreaSign(first) : 1;
      if (orientation == 0 || (orientation < 0 && Rings_.size() == firstRing)) { return false; }
      if (!AppendPart(first, orientation > 0)) { return false; }
    }
    return true;
  }

private:
  [[nodiscard]] bool AppendPart(size_t first, bool exterior) {
    if (Rings_.size() == std::numeric_limits<uint32_t>::max()) { return false; }
    Rings_.push_back({.First = static_cast<uint32_t>(first),
                      .Count = static_cast<uint32_t>(Points_.size() / 2 - first),
                      .Exterior = exterior});
    return true;
  }

  [[nodiscard]] bool ReadCommand(uint32_t id, uint32_t minimum, bool single) {
    if (Words_.empty()) { return false; }
    const uint32_t command = Words_.front();
    Words_ = Words_.subspan(1);
    const uint32_t count = command >> 3u;
    if ((command & 7u) != id || count < minimum || (single && count != 1) ||
        count > Words_.size() / 2 ||
        count > std::numeric_limits<uint32_t>::max() - Points_.size() / 2) {
      return false;
    }
    for (uint32_t i = 0; i < count; ++i) {
      const uint32_t dx = Words_[0];
      const uint32_t dy = Words_[1];
      Words_ = Words_.subspan(2);
      if ((id == 2 && dx == 0 && dy == 0) || !Advance(X_, dx) || !Advance(Y_, dy)) { return false; }
      Points_.push_back(X_);
      Points_.push_back(Y_);
    }
    return true;
  }

  [[nodiscard]] static bool Advance(int32_t &cursor, uint32_t encoded) {
    if (encoded == std::numeric_limits<uint32_t>::max()) { return false; }
    const int64_t magnitude = encoded >> 1u;
    const int64_t delta = (encoded & 1u) != 0 ? -magnitude - 1 : magnitude;
    const int64_t next = static_cast<int64_t>(cursor) + delta;
    if (next < std::numeric_limits<int32_t>::min() || next > std::numeric_limits<int32_t>::max()) {
      return false;
    }
    cursor = static_cast<int32_t>(next);
    return true;
  }

  [[nodiscard]] bool Close(size_t first) {
    if (Words_.empty() || Words_.front() != 15u ||
        (X_ == Points_[first * 2] && Y_ == Points_[first * 2 + 1])) {
      return false;
    }
    Words_ = Words_.subspan(1);
    return true;
  }

  [[nodiscard]] int AreaSign(size_t first) const {
    int64_t high = 0;
    uint64_t low = 0;
    const size_t end = Points_.size() / 2;
    for (size_t point = first; point < end; ++point) {
      const size_t next = point + 1 == end ? first : point + 1;
      const int64_t term = static_cast<int64_t>(Points_[point * 2]) * Points_[next * 2 + 1] -
                           static_cast<int64_t>(Points_[next * 2]) * Points_[point * 2 + 1];
      const uint64_t previous = low;
      low += static_cast<uint64_t>(term);
      high += (term < 0 ? -1 : 0) + (low < previous ? 1 : 0);
    }
    if (high < 0) { return -1; }
    return high > 0 || low != 0 ? 1 : 0;
  }

  std::span<const uint32_t> Words_;
  std::vector<int32_t> &Points_;
  std::vector<MvtLayer::Ring> &Rings_;
  int32_t X_ = 0;
  int32_t Y_ = 0;
};

struct DecodedValue {
  std::string_view Text;
  double Number = 0.0;
  bool IsNumber = false;
};

enum class ValueTag : uint32_t {
  String = 0x0a,
  Float = 0x15,
  Double = 0x19,
  Int = 0x20,
  Uint = 0x28,
  Sint = 0x30,
  Bool = 0x38
};

std::expected<DecodedValue, std::string_view> ReadValue(Reader reader) {
  static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559);
  DecodedValue value;
  uint32_t type = 0;
  FieldHeader field;
  while (reader.ReadField(field)) {
    switch (static_cast<ValueTag>((field.Number << 3u) | field.Wire)) {
      case ValueTag::String: {
        const auto text = reader.Bytes();
        if (!reader.Ok) { return std::unexpected(Says::kInvalidMvtValue); }
        value.Text = {reinterpret_cast<const char *>(text.P),
                      static_cast<size_t>(text.End - text.P)};
        break;
      }
      case ValueTag::Float: value.Number = std::bit_cast<float>(reader.Fixed<uint32_t>()); break;
      case ValueTag::Double: value.Number = std::bit_cast<double>(reader.Fixed<uint64_t>()); break;
      case ValueTag::Int:
        value.Number = static_cast<double>(std::bit_cast<int64_t>(reader.Varint()));
        break;
      case ValueTag::Uint: value.Number = static_cast<double>(reader.Varint()); break;
      case ValueTag::Sint: {
        const auto bits = reader.Varint();
        value.Number =
            static_cast<double>(std::bit_cast<int64_t>((bits >> 1u) ^ (uint64_t{0} - (bits & 1u))));
        break;
      }
      case ValueTag::Bool: value.Number = reader.Varint() != 0 ? 1.0 : 0.0; break;
      default:
        if (!reader.Skip(field.Wire)) { return std::unexpected(Says::kInvalidMvtValue); }
        continue;
    }
    if (!reader.Ok || (type != 0 && type != field.Number)) {
      return std::unexpected(Says::kInvalidMvtValue);
    }
    type = field.Number;
    value.IsNumber = static_cast<ValueTag>((field.Number << 3u) | field.Wire) != ValueTag::String;
  }
  if (!reader.Ok || type == 0) { return std::unexpected(Says::kInvalidMvtValue); }
  return value;
}

template <class Words> struct FeatureWords {
  Words Tags;
  Words Geometry;
  int Type = 0;
  std::optional<uint64_t> ProviderFeatureId;
};

using EncodedFeature = FeatureWords<std::vector<uint32_t>>;

struct WordCount {
  size_t Count = 0;
  std::array<uint32_t, 2> Maximum{};

  void clear() {
    Count = 0;
    Maximum = {};
  }

  void push_back(uint32_t word) {
    Maximum[Count % 2] = std::max(Maximum[Count % 2], word);
    ++Count;
  }
};

enum class FeatureTag : uint32_t {
  Id = 0x08,
  Tag = 0x10,
  PackedTags = 0x12,
  Type = 0x18,
  GeometryWord = 0x20,
  PackedGeometry = 0x22
};

template <class Words> bool AppendWord(Reader &reader, Words &words) {
  const auto word = reader.Varint();
  if (!reader.Ok || word > std::numeric_limits<uint32_t>::max()) { return false; }
  words.push_back(static_cast<uint32_t>(word));
  return true;
}

template <class Words> bool AppendPackedWords(Reader reader, Words &words) {
  while (reader.P < reader.End) {
    if (!AppendWord(reader, words)) { return false; }
  }
  return reader.Ok;
}

template <class Words> bool ReadProviderFeatureId(Reader &reader, FeatureWords<Words> &feature) {
  if (feature.ProviderFeatureId) { return false; }
  const uint64_t id = reader.Varint();
  if (!reader.Ok) { return false; }
  feature.ProviderFeatureId = id;
  return true;
}

template <class Words> bool ReadFeatureType(Reader &reader, FeatureWords<Words> &feature) {
  const uint64_t type = reader.Varint();
  if (!reader.Ok || type > 3) { return false; }
  feature.Type = static_cast<int>(type);
  return true;
}

struct FeaturePresence {
  bool HasType = false;
  bool HasGeometry = false;
};

template <class Words>
bool ReadFeatureField(Reader &reader,
                      FieldHeader field,
                      FeatureWords<Words> &feature,
                      FeaturePresence &presence) {
  switch (static_cast<FeatureTag>((field.Number << 3u) | field.Wire)) {
    case FeatureTag::Id: return ReadProviderFeatureId(reader, feature);
    case FeatureTag::Tag: return AppendWord(reader, feature.Tags);
    case FeatureTag::PackedTags: return AppendPackedWords(reader.Bytes(), feature.Tags);
    case FeatureTag::GeometryWord:
    case FeatureTag::PackedGeometry:
      presence.HasGeometry = true;
      return field.Wire == 0 ? AppendWord(reader, feature.Geometry)
                             : AppendPackedWords(reader.Bytes(), feature.Geometry);
    case FeatureTag::Type:
      presence.HasType = ReadFeatureType(reader, feature);
      return presence.HasType;
    default: return field.Number != 1 && reader.Skip(field.Wire);
  }
}

template <class Words>
std::expected<void, std::string_view> ReadFeature(Reader reader, FeatureWords<Words> &feature) {
  feature.Tags.clear();
  feature.Geometry.clear();
  feature.Type = 0;
  feature.ProviderFeatureId.reset();
  FeaturePresence presence;
  FieldHeader field;
  while (reader.ReadField(field)) {
    if (!ReadFeatureField(reader, field, feature, presence)) {
      return std::unexpected(Says::kInvalidMvtFeature);
    }
  }
  if (!reader.Ok || !presence.HasType || !presence.HasGeometry) {
    return std::unexpected(Says::kInvalidMvtFeature);
  }
  return {};
}

struct SelectedLayer {
  std::span<const uint8_t> Bytes;
  int Extent = 0;
};

std::expected<SelectedLayer, MvtLayer::ParseError> SelectLayer(std::span<const uint8_t> bytes,
                                                               std::string_view name) {
  using Error = MvtLayer::ParseError;
  if (bytes.empty()) { return std::unexpected(Error::MissingLayer); }
  std::optional<SelectedLayer> selected;
  Reader top{.P = bytes.data(), .End = bytes.data() + bytes.size(), .Ok = true};
  FieldHeader field;
  while (top.ReadField(field)) {
    if (field.Number != 3 || field.Wire != 2) {
      if (!top.Skip(field.Wire)) { return std::unexpected(Error::InvalidTile); }
      continue;
    }
    const Reader body = top.Bytes();
    if (!top.Ok) { return std::unexpected(Error::InvalidTile); }
    const auto header = ReadLayerHeader(body);
    if (!header.Complete) { return std::unexpected(Error::InvalidTile); }
    if (header.Name != name) { continue; }
    if (selected || !header.Version || header.Extent == 0 ||
        std::cmp_greater(header.Extent, std::numeric_limits<int>::max())) {
      return std::unexpected(Error::InvalidTile);
    }
    if (*header.Version != 2) { return std::unexpected(Error::UnsupportedVersion); }
    selected = SelectedLayer{.Bytes = std::span(body.P, body.End),
                             .Extent = static_cast<int>(header.Extent)};
  }
  if (!top.Ok) { return std::unexpected(Error::InvalidTile); }
  if (!selected) { return std::unexpected(Error::MissingLayer); }
  return *selected;
}

bool SizePlan(MvtLayer::AllocationPlan &plan) {
  constexpr size_t indices = std::numeric_limits<uint32_t>::max();
  if (plan.Features > std::numeric_limits<int>::max() || plan.Rings > indices ||
      plan.Points / 2 > indices || plan.Tags > indices || plan.Keys > indices ||
      plan.Values > indices) {
    return false;
  }
  size_t bytes = 0;
  const auto add = [&bytes](size_t count, size_t width) {
    if (count > (std::numeric_limits<size_t>::max() - bytes) / width) { return false; }
    bytes += count * width;
    return true;
  };
  if (!add(plan.Features, sizeof(MvtLayer::Feature)) || !add(plan.Rings, sizeof(MvtLayer::Ring)) ||
      !add(plan.Points, sizeof(int32_t)) || !add(plan.Tags, sizeof(uint32_t)) ||
      !add(plan.Keys, sizeof(std::string)) ||
      !add(plan.Values, sizeof(double) + sizeof(std::string)) ||
      !add(plan.Values / 64 + (plan.Values % 64 != 0 ? 1 : 0), sizeof(uint64_t)) ||
      !add(plan.TextBytes, 2) || !add(plan.Keys, sizeof(std::string) + 2) ||
      !add(plan.Values, sizeof(std::string) + 2)) {
    return false;
  }
  plan.HeapBytesMost = bytes;
  if (!add(plan.ScratchTags, sizeof(uint32_t)) || !add(plan.ScratchGeometry, sizeof(uint32_t))) {
    return false;
  }
  plan.PeakBytesMost = bytes;
  return true;
}

bool PlanFeature(Reader body,
                 MvtLayer::AllocationPlan &plan,
                 std::array<uint32_t, 2> &maximumTag,
                 bool &hasTags) {
  FeatureWords<WordCount> feature;
  if (!body.Ok || !ReadFeature(body, feature) || feature.Tags.Count % 2 != 0) { return false; }
  if (feature.Tags.Count != 0) {
    hasTags = true;
    for (size_t at = 0; at < maximumTag.size(); ++at) {
      maximumTag[at] = std::max(maximumTag[at], feature.Tags.Maximum[at]);
    }
  }
  plan.ScratchTags = std::max(plan.ScratchTags, feature.Tags.Count);
  plan.ScratchGeometry = std::max(plan.ScratchGeometry, feature.Geometry.Count);
  if (feature.Type == 0) { return true; }
  ++plan.Features;
  plan.Tags += feature.Tags.Count;
  plan.Points += (feature.Geometry.Count / 2) * 2;
  plan.Rings += feature.Geometry.Count / 3;
  return true;
}

std::expected<MvtLayer::AllocationPlan, MvtLayer::ParseError>
PlanLayer(std::span<const uint8_t> bytes) {
  using Error = MvtLayer::ParseError;
  MvtLayer::AllocationPlan plan;
  Reader reader{.P = bytes.data(), .End = bytes.data() + bytes.size(), .Ok = true};
  FieldHeader field;
  std::array<uint32_t, 2> maximumTag{};
  bool hasTags = false;
  while (reader.ReadField(field)) {
    if (field.Number == 3 && field.Wire == 2) {
      const auto text = reader.Bytes();
      if (!reader.Ok) { return std::unexpected(Error::InvalidTile); }
      ++plan.Keys;
      plan.TextBytes += static_cast<size_t>(text.End - text.P);
    } else if (field.Number == 4 && field.Wire == 2) {
      const auto body = reader.Bytes();
      const auto value = ReadValue(body);
      if (!reader.Ok || !value) { return std::unexpected(Error::InvalidTile); }
      ++plan.Values;
      plan.TextBytes += value->Text.size();
    } else if (field.Number == 2 && field.Wire == 2) {
      const auto body = reader.Bytes();
      if (!reader.Ok || !PlanFeature(body, plan, maximumTag, hasTags)) {
        return std::unexpected(Error::InvalidTile);
      }
    } else if (!reader.Skip(field.Wire)) {
      return std::unexpected(Error::InvalidTile);
    }
  }
  if (!reader.Ok || (hasTags && (maximumTag[0] >= plan.Keys || maximumTag[1] >= plan.Values))) {
    return std::unexpected(Error::InvalidTile);
  }
  if (!SizePlan(plan)) { return std::unexpected(Error::CapacityExceeded); }
  return plan;
}

}

std::expected<MvtLayer::AllocationPlan, MvtLayer::ParseError>
MvtLayer::Plan(std::span<const uint8_t> bytes, std::string_view layer) {
  const auto selected = SelectLayer(bytes, layer);
  if (!selected) { return std::unexpected(selected.error()); }
  return PlanLayer(selected->Bytes);
}

std::expected<void, MvtLayer::ParseError>
MvtLayer::Parse(std::span<const uint8_t> bytes, std::string_view layer, size_t bytesMost) {
  MvtLayer candidate;
  const auto result = candidate.Decode(bytes, layer, bytesMost);
  if (!result) { return result; }
  *this = std::move(candidate);
  return {};
}

std::expected<void, MvtLayer::ParseError>
MvtLayer::Decode(std::span<const uint8_t> bytes, std::string_view layer, size_t bytesMost) {
  const auto selected = SelectLayer(bytes, layer);
  if (!selected) { return std::unexpected(selected.error()); }
  const auto plan = PlanLayer(selected->Bytes);
  if (!plan) { return std::unexpected(plan.error()); }
  if (plan->PeakBytesMost > bytesMost) { return std::unexpected(ParseError::CapacityExceeded); }
  Features_.reserve(plan->Features);
  Rings_.reserve(plan->Rings);
  Points_.reserve(plan->Points);
  Tags_.reserve(plan->Tags);
  Keys_.reserve(plan->Keys);
  Values_.reserve(plan->Values);
  ValueStrs_.reserve(plan->Values);
  ValueIsNum_.reserve(plan->Values);
  Extent_ = selected->Extent;
  const auto features = ReadLayerTables(selected->Bytes);
  if (!features) { return std::unexpected(features.error()); }
  return DecodeFeatures(*features, *plan);
}

std::expected<std::span<const uint8_t>, MvtLayer::ParseError>
MvtLayer::ReadLayerTables(std::span<const uint8_t> bytes) {
  Reader L{.P = bytes.data(), .End = bytes.data() + bytes.size(), .Ok = true};
  FieldHeader field;
  std::span<const uint8_t> features(bytes.data() + bytes.size(), size_t{0});
  for (;;) {
    const auto *start = L.P;
    if (!L.ReadField(field)) { break; }
    if (field.Number == 3 && field.Wire == 2) {
      const Reader s = L.Bytes();
      if (!L.Ok) { return std::unexpected(ParseError::InvalidTile); }
      Keys_.emplace_back(reinterpret_cast<const char *>(s.P), static_cast<size_t>(s.End - s.P));
    } else if (field.Number == 4 && field.Wire == 2) {
      const auto body = L.Bytes();
      auto value = ReadValue(body);
      if (!value) { return std::unexpected(ParseError::InvalidTile); }
      Values_.push_back(value->Number);
      ValueStrs_.emplace_back(value->Text);
      ValueIsNum_.push_back(value->IsNumber);
    } else if (field.Number == 2 && field.Wire == 2) {
      (void)L.Bytes();
      if (!L.Ok) { return std::unexpected(ParseError::InvalidTile); }
      features = std::span(features.empty() ? start : features.data(), L.P);
    } else if (!L.Skip(field.Wire)) {
      return std::unexpected(ParseError::InvalidTile);
    }
  }

  if (!L.Ok) { return std::unexpected(ParseError::InvalidTile); }
  return features;
}

std::expected<void, MvtLayer::ParseError>
MvtLayer::DecodeFeatures(std::span<const uint8_t> layerBytes, const AllocationPlan &plan) {
  Reader layer{.P = layerBytes.data(), .End = layerBytes.data() + layerBytes.size(), .Ok = true};
  FieldHeader field;
  EncodedFeature encoded;
  encoded.Tags.reserve(plan.ScratchTags);
  encoded.Geometry.reserve(plan.ScratchGeometry);
  while (layer.ReadField(field)) {
    if (field.Number != 2 || field.Wire != 2) {
      if (!layer.Skip(field.Wire)) { return std::unexpected(ParseError::InvalidTile); }
      continue;
    }
    const Reader body = layer.Bytes();
    if (!layer.Ok || !ReadFeature(body, encoded) ||
        !ValidTags(encoded.Tags, Keys_.size(), Values_.size()) ||
        encoded.Tags.size() > std::numeric_limits<uint32_t>::max() - Tags_.size()) {
      return std::unexpected(ParseError::InvalidTile);
    }
    if (encoded.Type == 0) { continue; }
    Feature f{};
    f.Type = encoded.Type;
    f.ProviderFeatureId = encoded.ProviderFeatureId;
    f.HasContentDigest = true;
    f.ContentDigest = Sha256Digest(body.P, static_cast<size_t>(body.End - body.P));
    f.FirstTag = static_cast<uint32_t>(Tags_.size());
    f.FirstRing = static_cast<uint32_t>(Rings_.size());
    Tags_.insert(Tags_.end(), encoded.Tags.begin(), encoded.Tags.end());
    f.TagCount = static_cast<uint32_t>(Tags_.size()) - f.FirstTag;
    GeometryReader geometry(encoded.Geometry, Points_, Rings_);
    if (!geometry.Read(f.Type)) { return std::unexpected(ParseError::InvalidTile); }
    f.RingCount = static_cast<uint32_t>(Rings_.size()) - f.FirstRing;
    Features_.push_back(f);
  }
  if (!layer.Ok) { return std::unexpected(ParseError::InvalidTile); }
  return {};
}

double MvtLayer::NumberTag(const Feature &feature, const char *key, double fallback) const {
  for (uint32_t i = 0; i + 1 < feature.TagCount; i += 2) {
    const uint32_t k = Tags_[feature.FirstTag + i];
    const uint32_t v = Tags_[feature.FirstTag + i + 1];
    if (k >= Keys_.size() || v >= Values_.size()) { continue; }
    if (Keys_[k] == key && ValueIsNum_[v]) { return Values_[v]; }
  }
  return fallback;
}

MvtLayer::Tag MvtLayer::TagAt(const Feature &f, uint32_t i) const {
  Tag t{};
  if (i * 2 + 1 >= f.TagCount) { return t; }
  const uint32_t k = Tags_[f.FirstTag + i * 2];
  const uint32_t v = Tags_[f.FirstTag + i * 2 + 1];
  if (k >= Keys_.size() || v >= Values_.size()) { return t; }
  t.Key = Keys_[k];
  t.IsNumber = ValueIsNum_[v];
  if (t.IsNumber) {
    t.Number = Values_[v];
  } else {
    t.String = ValueStrs_[v];
  }
  return t;
}

std::string_view MvtLayer::StringTag(const Feature &feature, const char *key) const {
  for (uint32_t i = 0; i + 1 < feature.TagCount; i += 2) {
    const uint32_t k = Tags_[feature.FirstTag + i];
    const uint32_t v = Tags_[feature.FirstTag + i + 1];
    if (k >= Keys_.size() || v >= ValueStrs_.size()) { continue; }
    if (Keys_[k] == key && !ValueIsNum_[v]) { return ValueStrs_[v]; }
  }
  return {};
}

}
