#include "OsmVector.h"

#include <cstdint>
#include <cstddef>
#include <bit>
#include <expected>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Ground {

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
  uint32_t Version = 0;
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
                 std::vector<OsmVector::Ring> &rings)
      : Words_(words), Points_(points), Rings_(rings) {}

  [[nodiscard]] bool Read(int type) {
    if (type == 0) { return true; }
    if (Words_.empty()) { return false; }
    if (type == 1) { return ReadCommand(1, 1, false) && Words_.empty(); }
    while (!Words_.empty()) {
      const size_t first = Points_.size() / 2;
      if (!ReadCommand(1, 1, true) || !ReadCommand(2, type == 3 ? 2u : 1u, false)) { return false; }
      if (type == 3 && !Close(first)) { return false; }
      if (Rings_.size() == std::numeric_limits<uint32_t>::max()) { return false; }
      Rings_.push_back({.First = static_cast<uint32_t>(first),
                        .Count = static_cast<uint32_t>(Points_.size() / 2 - first),
                        .Exterior = type != 3 || PositiveArea(first)});
    }
    return true;
  }

private:
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

  [[nodiscard]] bool PositiveArea(size_t first) const {
    double area = 0.0;
    const size_t end = Points_.size() / 2;
    for (size_t point = first; point < end; ++point) {
      const size_t next = point + 1 == end ? first : point + 1;
      area += static_cast<double>(Points_[point * 2]) * Points_[next * 2 + 1] -
              static_cast<double>(Points_[next * 2]) * Points_[point * 2 + 1];
    }
    return area > 0.0;
  }

  std::span<const uint32_t> Words_;
  std::vector<int32_t> &Points_;
  std::vector<OsmVector::Ring> &Rings_;
  int32_t X_ = 0;
  int32_t Y_ = 0;
};

struct DecodedValue {
  std::string Text;
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
        value.Text.assign(reinterpret_cast<const char *>(text.P),
                          static_cast<size_t>(text.End - text.P));
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

struct EncodedFeature {
  std::vector<uint32_t> Tags;
  std::vector<uint32_t> Geometry;
  int Type = 0;
};

enum class FeatureTag : uint32_t {
  Tag = 0x10,
  PackedTags = 0x12,
  Type = 0x18,
  GeometryWord = 0x20,
  PackedGeometry = 0x22
};

bool AppendWord(Reader &reader, std::vector<uint32_t> &words) {
  const auto word = reader.Varint();
  if (!reader.Ok || word > std::numeric_limits<uint32_t>::max()) { return false; }
  words.push_back(static_cast<uint32_t>(word));
  return true;
}

bool AppendPackedWords(Reader reader, std::vector<uint32_t> &words) {
  while (reader.P < reader.End) {
    if (!AppendWord(reader, words)) { return false; }
  }
  return reader.Ok;
}

std::expected<void, std::string_view> ReadFeature(Reader reader, EncodedFeature &feature) {
  feature.Tags.clear();
  feature.Geometry.clear();
  feature.Type = 0;
  FieldHeader field;
  while (reader.ReadField(field)) {
    switch (static_cast<FeatureTag>((field.Number << 3u) | field.Wire)) {
      case FeatureTag::Tag:
        if (!AppendWord(reader, feature.Tags)) { return std::unexpected(Says::kInvalidMvtFeature); }
        break;
      case FeatureTag::PackedTags:
        if (!AppendPackedWords(reader.Bytes(), feature.Tags)) {
          return std::unexpected(Says::kInvalidMvtFeature);
        }
        break;
      case FeatureTag::GeometryWord:
        if (!AppendWord(reader, feature.Geometry)) {
          return std::unexpected(Says::kInvalidMvtFeature);
        }
        break;
      case FeatureTag::PackedGeometry:
        if (!AppendPackedWords(reader.Bytes(), feature.Geometry)) {
          return std::unexpected(Says::kInvalidMvtFeature);
        }
        break;
      case FeatureTag::Type: {
        const auto type = reader.Varint();
        if (!reader.Ok || type > 3) { return std::unexpected(Says::kInvalidMvtFeature); }
        feature.Type = static_cast<int>(type);
        break;
      }
      default:
        if (!reader.Skip(field.Wire)) { return std::unexpected(Says::kInvalidMvtFeature); }
        break;
    }
  }
  if (!reader.Ok) { return std::unexpected(Says::kInvalidMvtFeature); }
  return {};
}

}

bool OsmVector::Parse(const uint8_t *bytes, size_t len, const char *layer, bool *present) {
  if (present != nullptr) { *present = false; }
  if (bytes == nullptr || len == 0 || layer == nullptr) { return false; }
  OsmVector candidate;
  if (!candidate.Decode(std::span(bytes, len), layer, present)) { return false; }
  *this = std::move(candidate);
  return true;
}

bool OsmVector::Decode(std::span<const uint8_t> bytes, std::string_view layer, bool *present) {
  bool found = false;
  Reader top{.P = bytes.data(), .End = bytes.data() + bytes.size(), .Ok = true};
  FieldHeader field;
  while (top.ReadField(field)) {
    if (field.Number != 3 || field.Wire != 2) {
      if (!top.Skip(field.Wire)) { return false; }
      continue;
    }
    Reader L = top.Bytes();
    if (!top.Ok) { return false; }

    const auto header = ReadLayerHeader(L);
    if (header.Name != layer) {
      if (!header.Complete) { return false; }
      continue;
    }
    if (present != nullptr) { *present = true; }
    if (!header.Complete || found || header.Version != 2 || header.Extent == 0 ||
        std::cmp_greater(header.Extent, std::numeric_limits<int>::max())) {
      return false;
    }
    Extent_ = static_cast<int>(header.Extent);

    std::vector<Reader> featureBodies;
    while (L.ReadField(field)) {
      if (field.Number == 3 && field.Wire == 2) {
        const Reader s = L.Bytes();
        if (!L.Ok) { return false; }
        Keys_.emplace_back(reinterpret_cast<const char *>(s.P), static_cast<size_t>(s.End - s.P));
      } else if (field.Number == 4 && field.Wire == 2) {
        const auto body = L.Bytes();
        if (!L.Ok) { return false; }
        auto value = ReadValue(body);
        if (!value) { return false; }
        Values_.push_back(value->Number);
        ValueStrs_.push_back(std::move(value->Text));
        ValueIsNum_.push_back(value->IsNumber);
      } else if (field.Number == 2 && field.Wire == 2) {
        featureBodies.push_back(L.Bytes());
        if (!L.Ok) { return false; }
      } else if (!L.Skip(field.Wire)) {
        return false;
      }
    }

    if (!L.Ok) { return false; }
    EncodedFeature encoded;
    for (const Reader body : featureBodies) {
      if (!ReadFeature(body, encoded) || !ValidTags(encoded.Tags, Keys_.size(), Values_.size()) ||
          encoded.Tags.size() > std::numeric_limits<uint32_t>::max() - Tags_.size()) {
        return false;
      }
      Feature f{};
      f.Type = encoded.Type;
      f.FirstTag = static_cast<uint32_t>(Tags_.size());
      f.FirstRing = static_cast<uint32_t>(Rings_.size());
      Tags_.insert(Tags_.end(), encoded.Tags.begin(), encoded.Tags.end());
      f.TagCount = static_cast<uint32_t>(Tags_.size()) - f.FirstTag;
      GeometryReader geometry(encoded.Geometry, Points_, Rings_);
      if (!geometry.Read(f.Type)) { return false; }
      f.RingCount = static_cast<uint32_t>(Rings_.size()) - f.FirstRing;
      Features_.push_back(f);
    }
    found = true;
  }
  return top.Ok && found;
}

double OsmVector::Num(const Feature &f, const char *key, double def) const {
  for (uint32_t i = 0; i + 1 < f.TagCount; i += 2) {
    const uint32_t k = Tags_[f.FirstTag + i];
    const uint32_t v = Tags_[f.FirstTag + i + 1];
    if (k >= Keys_.size() || v >= Values_.size()) { continue; }
    if (Keys_[k] == key && ValueIsNum_[v]) { return Values_[v]; }
  }
  return def;
}

OsmVector::Tag OsmVector::TagAt(const Feature &f, uint32_t i) const {
  Tag t{};
  if (i * 2 + 1 >= f.TagCount) { return t; }
  const uint32_t k = Tags_[f.FirstTag + i * 2];
  const uint32_t v = Tags_[f.FirstTag + i * 2 + 1];
  if (k >= Keys_.size() || v >= Values_.size()) { return t; }
  t.Key = Keys_[k];
  t.IsNum = ValueIsNum_[v];
  if (t.IsNum) {
    t.Num = Values_[v];
  } else {
    t.Str = ValueStrs_[v];
  }
  return t;
}

std::string_view OsmVector::Str(const Feature &f, const char *key) const {
  for (uint32_t i = 0; i + 1 < f.TagCount; i += 2) {
    const uint32_t k = Tags_[f.FirstTag + i];
    const uint32_t v = Tags_[f.FirstTag + i + 1];
    if (k >= Keys_.size() || v >= ValueStrs_.size()) { continue; }
    if (Keys_[k] == key && !ValueIsNum_[v]) { return ValueStrs_[v]; }
  }
  return {};
}

}
