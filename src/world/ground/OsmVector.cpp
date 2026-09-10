#include "OsmVector.h"

#include <cstdint>
#include <cstddef>
#include <bit>
#include <expected>
#include <limits>
#include <string_view>
#include <type_traits>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Ground {

constexpr int kVarintShiftMost = 63;

constexpr uint64_t kVarintPayload = 0x7fu;

namespace Says {
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

int32_t ZigZag(uint64_t v) {
  return static_cast<int32_t>((v >> 1u) ^ (~(v & 1u) + 1));
}

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

}

bool OsmVector::Parse(const uint8_t *bytes, size_t len, const char *layer, bool *present) {
  if (present != nullptr) { *present = false; }
  Features_.clear();
  Rings_.clear();
  Points_.clear();
  Tags_.clear();
  Keys_.clear();
  Values_.clear();
  ValueStrs_.clear();
  ValueIsNum_.clear();
  Extent_ = 4096;
  if ((bytes == nullptr) || len == 0) { return false; }

  Reader top{.P = bytes, .End = bytes + len, .Ok = true};
  FieldHeader field;
  while (top.ReadField(field)) {
    if (field.Number != 3 || field.Wire != 2) {
      if (!top.Skip(field.Wire)) { return false; }
      continue;
    }
    Reader L = top.Bytes();
    if (!top.Ok) { return false; }

    Reader probe = L;
    std::string name;
    FieldHeader probeField;
    while (probe.ReadField(probeField)) {
      if (probeField.Number == 1 && probeField.Wire == 2) {
        const Reader s = probe.Bytes();
        if (!probe.Ok) { break; }
        name.assign(reinterpret_cast<const char *>(s.P), static_cast<size_t>(s.End - s.P));
      } else if (!probe.Skip(probeField.Wire)) {
        break;
      }
    }
    if (name != layer) { continue; }
    if (present != nullptr) { *present = true; }

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
      } else if (field.Number == 5 && field.Wire == 0) {
        Extent_ = static_cast<int>(L.Varint());
      } else if (field.Number == 2 && field.Wire == 2) {
        featureBodies.push_back(L.Bytes());
        if (!L.Ok) { return false; }
      } else if (!L.Skip(field.Wire)) {
        return false;
      }
    }

    for (Reader F : featureBodies) {
      Feature f{};
      f.FirstTag = static_cast<uint32_t>(Tags_.size());
      f.FirstRing = static_cast<uint32_t>(Rings_.size());
      FieldHeader featureField;
      std::vector<uint32_t> geom;
      while (F.ReadField(featureField)) {
        if (featureField.Number == 2 && featureField.Wire == 2) {
          Reader t = F.Bytes();
          if (!F.Ok) { break; }
          while (t.P < t.End) {
            const uint64_t v = t.Varint();
            if (!t.Ok) { break; }
            Tags_.push_back(static_cast<uint32_t>(v));
          }
        } else if (featureField.Number == 3 && featureField.Wire == 0) {
          f.Type = static_cast<int>(F.Varint());
        } else if (featureField.Number == 4 && featureField.Wire == 2) {
          Reader gr = F.Bytes();
          if (!F.Ok) { break; }
          while (gr.P < gr.End) {
            const uint64_t v = gr.Varint();
            if (!gr.Ok) { break; }
            geom.push_back(static_cast<uint32_t>(v));
          }
        } else if (!F.Skip(featureField.Wire)) {
          break;
        }
      }
      f.TagCount = static_cast<uint32_t>(Tags_.size()) - f.FirstTag;

      int32_t cx = 0;
      int32_t cy = 0;
      size_t gi = 0;
      uint32_t ringFirst = 0;
      int ringCount = 0;

      const auto flushLine = [&] {
        if (f.Type != 2 || ringCount < 2) { return; }
        Ring r{};
        r.First = ringFirst;
        r.Count = static_cast<uint32_t>(ringCount);
        r.Exterior = true;
        Rings_.push_back(r);
      };
      while (gi < geom.size()) {
        const uint32_t cmd = geom[gi] & 7u;
        const uint32_t cnt = geom[gi] >> 3u;
        gi++;
        if (cmd == 1 || cmd == 2) {
          for (uint32_t k = 0; k < cnt; k++) {
            if (gi + 1 >= geom.size()) {
              gi = geom.size();
              break;
            }
            cx += ZigZag(geom[gi]);
            cy += ZigZag(geom[gi + 1]);
            gi += 2;
            if (cmd == 1) {
              flushLine();
              ringFirst = static_cast<uint32_t>(Points_.size()) / 2;
              ringCount = 0;
            }
            Points_.push_back(cx);
            Points_.push_back(cy);
            ringCount++;
          }
        } else if (cmd == 7) {
          if (ringCount >= 3) {
            double a = 0.0;
            for (int k = 0; k < ringCount; k++) {
              const size_t i0 = (static_cast<size_t>(ringFirst) + static_cast<size_t>(k)) * 2;
              const size_t i1 =
                  (static_cast<size_t>(ringFirst) + static_cast<size_t>((k + 1) % ringCount)) * 2;
              a += static_cast<double>(Points_[i0]) * Points_[i1 + 1] -
                   static_cast<double>(Points_[i1]) * Points_[i0 + 1];
            }
            Ring r{};
            r.First = ringFirst;
            r.Count = static_cast<uint32_t>(ringCount);

            r.Exterior = a > 0.0;
            Rings_.push_back(r);
          }
          ringCount = 0;
        } else {
          break;
        }
      }
      flushLine();
      f.RingCount = static_cast<uint32_t>(Rings_.size()) - f.FirstRing;
      Features_.push_back(f);
    }
    return true;
  }
  return false;
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
