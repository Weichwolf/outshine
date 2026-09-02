#include "OsmVector.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <utility>

namespace outshine::Ground {

constexpr int kVarintShiftMost = 63;

constexpr uint64_t kVarintPayload = 0x7fu;

namespace {

struct Reader {
  const uint8_t *P, *End;
  bool Ok = true;

  uint64_t Varint() {
    uint64_t r = 0;
    int s = 0;
    while (P < End) {
      const uint8_t b = *P++;
      r |= static_cast<uint64_t>(b & kVarintPayload) << static_cast<uint32_t>(s);
      if ((b & 0x80u) == 0) { return r; }
      s += 7;
      if (s > kVarintShiftMost) { break; }
    }
    Ok = false;
    return 0;
  }

  [[nodiscard]] bool Field(uint32_t &num, uint32_t &wire) {
    if (P >= End) { return false; }
    const uint64_t k = Varint();
    if (!Ok) { return false; }
    num = static_cast<uint32_t>(k >> 3u);
    wire = static_cast<uint32_t>(k & 7u);
    return true;
  }

  Reader Bytes() {
    const uint64_t n = Varint();
    Reader r{.P = P, .End = P, .Ok = false};
    if (!Ok || static_cast<uint64_t>(End - P) < n) {
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
        if (!Ok || static_cast<uint64_t>(End - P) < n) {
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

} // namespace

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
  uint32_t num = 0;
  uint32_t wire = 0;
  while (top.Field(num, wire)) {
    if (num != 3 || wire != 2) {
      if (!top.Skip(wire)) { return false; }
      continue;
    }
    Reader L = top.Bytes();
    if (!top.Ok) { return false; }

    Reader probe = L;
    std::string name;
    uint32_t n2 = 0;
    uint32_t w2 = 0;
    while (probe.Field(n2, w2)) {
      if (n2 == 1 && w2 == 2) {
        const Reader s = probe.Bytes();
        if (!probe.Ok) { break; }
        name.assign(reinterpret_cast<const char *>(s.P), static_cast<size_t>(s.End - s.P));
      } else if (!probe.Skip(w2)) {
        break;
      }
    }
    if (name != layer) { continue; }
    if (present != nullptr) { *present = true; }

    std::vector<Reader> featureBodies;
    while (L.Field(num, wire)) {
      if (num == 3 && wire == 2) {
        const Reader s = L.Bytes();
        if (!L.Ok) { return false; }
        Keys_.emplace_back(reinterpret_cast<const char *>(s.P), static_cast<size_t>(s.End - s.P));
      } else if (num == 4 && wire == 2) {
        Reader v = L.Bytes();
        if (!L.Ok) { return false; }
        double val = 0.0;
        std::string str;
        bool isNum = false;
        uint32_t vn = 0;
        uint32_t vw = 0;
        while (v.Field(vn, vw)) {
          if (vn == 1 && vw == 2) {
            const Reader s = v.Bytes();
            if (!v.Ok) { break; }
            str.assign(reinterpret_cast<const char *>(s.P), static_cast<size_t>(s.End - s.P));
          } else if (vn == 2 && vw == 5) {
            float f;
            std::memcpy(&f, v.P, 4);
            v.P += 4;
            val = f;
            isNum = true;
          } else if (vn == 3 && vw == 1) {
            double d;
            std::memcpy(&d, v.P, 8);
            v.P += 8;
            val = d;
            isNum = true;
          } else if (vn == 4 && vw == 0) {
            val = static_cast<double>(v.Varint());
            isNum = true;
          } else if (vn == 5 && vw == 0) {
            val = static_cast<double>(v.Varint());
            isNum = true;
          } else if (vn == 6 && vw == 0) {
            val = static_cast<double>(ZigZag(v.Varint()));
            isNum = true;
          } else if (vn == 7 && vw == 0) {
            val = v.Varint() != 0 ? 1.0 : 0.0;
            isNum = true;
          } else if (!v.Skip(vw)) {
            break;
          }
        }
        Values_.push_back(val);
        ValueStrs_.push_back(std::move(str));
        ValueIsNum_.push_back(isNum);
      } else if (num == 5 && wire == 0) {
        Extent_ = static_cast<int>(L.Varint());
      } else if (num == 2 && wire == 2) {
        featureBodies.push_back(L.Bytes());
        if (!L.Ok) { return false; }
      } else if (!L.Skip(wire)) {
        return false;
      }
    }

    for (Reader F : featureBodies) {
      Feature f{};
      f.FirstTag = static_cast<uint32_t>(Tags_.size());
      f.FirstRing = static_cast<uint32_t>(Rings_.size());
      uint32_t fn = 0;
      uint32_t fw = 0;
      std::vector<uint32_t> geom;
      while (F.Field(fn, fw)) {
        if (fn == 2 && fw == 2) {
          Reader t = F.Bytes();
          if (!F.Ok) { break; }
          while (t.P < t.End) {
            const uint64_t v = t.Varint();
            if (!t.Ok) { break; }
            Tags_.push_back(static_cast<uint32_t>(v));
          }
        } else if (fn == 3 && fw == 0) {
          f.Type = static_cast<int>(F.Varint());
        } else if (fn == 4 && fw == 2) {
          Reader gr = F.Bytes();
          if (!F.Ok) { break; }
          while (gr.P < gr.End) {
            const uint64_t v = gr.Varint();
            if (!gr.Ok) { break; }
            geom.push_back(static_cast<uint32_t>(v));
          }
        } else if (!F.Skip(fw)) {
          break;
        }
      }
      f.TagCount = static_cast<uint32_t>(Tags_.size()) - f.FirstTag;

      int32_t cx = 0;
      int32_t cy = 0;
      size_t gi = 0;
      uint32_t ringFirst = 0;
      int ringCount = 0;

      const auto flushLine = [&]() {
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

} // namespace outshine::Ground
