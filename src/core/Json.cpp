#include "Json.h"

#include <charconv>
#include <cstdlib>
#include <cstring>

namespace outshine {

bool Json::Parse(const char *text, size_t len) {
  Text_.assign(text, len);
  Nodes_.clear();
  Kids_.clear();
  P_ = 0;
  Depth_ = 0;
  Ok_ = ParseValue() == 0;
  if (Ok_) {
    // one document, wholly consumed: "{...} garbage" is not json with a suffix, it is
    // not json
    Skip();
    Ok_ = P_ == Text_.size();
  }
  return Ok_;
}

void Json::Skip() {
  while (P_ < Text_.size()) {
    const char c = Text_[P_];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') P_++;
    else break;
  }
}

bool Json::ParseString(uint32_t &off, uint32_t &len, bool &escaped) {
  if (P_ >= Text_.size() || Text_[P_] != '"') return false;
  P_++;
  const size_t start = P_;
  escaped = false;
  while (P_ < Text_.size()) {
    const char c = Text_[P_];
    if (c == '\\') { escaped = true; P_ += 2; continue; }
    if (c == '"') {
      off = (uint32_t)start;
      len = (uint32_t)(P_ - start);
      P_++;
      return true;
    }
    P_++;
  }
  return false;
}

int32_t Json::ParseValue() {
  Skip();
  if (P_ >= Text_.size()) return -1;
  // a counter bounds the depth, not the C stack: a 200k-bracket bomb is a refusal at its
  // byte, never a segfault behind every careful Refuse the gltf door holds
  if (Depth_ >= kMostDepth) return -1;
  ++Depth_;
  const int32_t id = ParseValueInside();
  --Depth_;
  return id;
}

int32_t Json::ParseValueInside() {
  const int32_t id = (int32_t)Nodes_.size();
  Nodes_.emplace_back();
  const char c = Text_[P_];

  if (c == '{' || c == '[') {
    const bool obj = c == '{';
    const char close = obj ? '}' : ']';
    P_++;
    std::vector<int32_t> kids;
    bool afterComma = false;
    for (;;) {
      Skip();
      if (P_ >= Text_.size()) return -1;
      if (Text_[P_] == close) {
        // "[1,]" ends on a comma's promise -- the grammar has no trailing comma
        if (afterComma) return -1;
        P_++;
        break;
      }
      uint32_t koff = 0, klen = 0;
      bool kesc = false;
      if (obj) {
        if (!ParseString(koff, klen, kesc)) return -1;
        Skip();
        if (P_ >= Text_.size() || Text_[P_] != ':') return -1;
        P_++;
      }
      const int32_t kid = ParseValue();
      if (kid < 0) return -1;
      if (obj) {
        Nodes_[(size_t)kid].Key = koff;
        Nodes_[(size_t)kid].KeyLen = klen;
        Nodes_[(size_t)kid].KeyEscaped = kesc;
      }
      kids.push_back(kid);
      Skip();
      if (P_ >= Text_.size()) return -1;
      if (Text_[P_] == ',') {
        P_++;
        afterComma = true;
        continue;
      }
      // "[1 2]" is two values and no grammar -- a member ends on a comma or the close
      if (Text_[P_] != close) return -1;
      afterComma = false;
    }
    Node &n = Nodes_[(size_t)id];
    n.K = obj ? Kind::Object : Kind::Array;
    n.First = (uint32_t)Kids_.size();
    n.Count = (uint32_t)kids.size();
    Kids_.insert(Kids_.end(), kids.begin(), kids.end());
    return id;
  }

  if (c == '"') {
    uint32_t off = 0, len = 0;
    bool esc = false;
    if (!ParseString(off, len, esc)) return -1;
    Node &n = Nodes_[(size_t)id];
    n.K = Kind::String;
    n.Str = off;
    n.StrLen = len;
    n.Escaped = esc;
    return id;
  }

  const auto literal = [&](const char *word, size_t bytes) {
    if (Text_.size() - P_ < bytes || std::memcmp(Text_.c_str() + P_, word, bytes) != 0) {
      return false;
    }
    // "truex" is not true with a suffix -- the literal ends where the grammar ends
    const size_t after = P_ + bytes;
    if (after < Text_.size()) {
      const char next = Text_[after];
      if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') ||
          (next >= '0' && next <= '9') || next == '_') {
        return false;
      }
    }
    P_ += bytes;
    return true;
  };
  if (literal("true", 4)) { Nodes_[(size_t)id].K = Kind::Bool; Nodes_[(size_t)id].Num = 1.0; return id; }
  if (literal("false", 5)) { Nodes_[(size_t)id].K = Kind::Bool; Nodes_[(size_t)id].Num = 0.0; return id; }
  if (literal("null", 4)) { Nodes_[(size_t)id].K = Kind::Null; return id; }

  // rfc 8259's number grammar, scanned EXACTLY: -?(0|[1-9][0-9]*)(.[0-9]+)?([eE][+-]?[0-9]+)?
  // -- from_chars alone is laxer (01, 1., -.5 all read) and strtod was laxer still
  {
    size_t at = P_;
    if (at < Text_.size() && Text_[at] == '-') ++at;
    const size_t whole = at;
    if (at < Text_.size() && Text_[at] == '0') {
      ++at;
    } else {
      while (at < Text_.size() && Text_[at] >= '0' && Text_[at] <= '9') ++at;
    }
    if (at == whole) return -1;
    if (at < Text_.size() && Text_[at] == '.') {
      ++at;
      const size_t fraction = at;
      while (at < Text_.size() && Text_[at] >= '0' && Text_[at] <= '9') ++at;
      if (at == fraction) return -1;
    }
    if (at < Text_.size() && (Text_[at] == 'e' || Text_[at] == 'E')) {
      ++at;
      if (at < Text_.size() && (Text_[at] == '+' || Text_[at] == '-')) ++at;
      const size_t exponent = at;
      while (at < Text_.size() && Text_[at] >= '0' && Text_[at] <= '9') ++at;
      if (at == exponent) return -1;
    }
    double v = 0.0;
    const auto scanned = std::from_chars(Text_.c_str() + P_, Text_.c_str() + at, v);
    if (scanned.ec == std::errc::result_out_of_range) {
      // 1e999 overflows the double; json's grammar allows it and the value is the edge
      v = Text_[P_] == '-' ? -1.7976931348623157e308 : 1.7976931348623157e308;
    } else if (scanned.ec != std::errc() || scanned.ptr != Text_.c_str() + at) {
      return -1;
    }
    P_ = at;
    Nodes_[(size_t)id].K = Kind::Number;
    Nodes_[(size_t)id].Num = v;
    return id;
  }
}

std::string Json::Decode(uint32_t off, uint32_t len, bool escaped) const {
  if (!escaped) return Text_.substr(off, len);
  std::string out;
  out.reserve(len);
  for (uint32_t i = 0; i < len; i++) {
    char c = Text_[off + i];
    if (c != '\\' || i + 1 >= len) { out.push_back(c); continue; }
    const char e = Text_[off + ++i];
    switch (e) {
      case 'n': out.push_back('\n'); break;
      case 't': out.push_back('\t'); break;
      case 'r': out.push_back('\r'); break;
      case 'b': out.push_back('\b'); break;
      case 'f': out.push_back('\f'); break;

      case 'u': {
        if (i + 4 >= len) break;
        unsigned cp = (unsigned)std::strtoul(Text_.substr(off + i + 1, 4).c_str(), nullptr, 16);
        i += 4;
        // a surrogate half is not a character: a high half pairs with the \uDC00..DFFF
        // that follows, and a lone half becomes U+FFFD instead of invalid utf-8
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < len && Text_[off + i + 1] == '\\' &&
            Text_[off + i + 2] == 'u') {
          const unsigned low =
              (unsigned)std::strtoul(Text_.substr(off + i + 3, 4).c_str(), nullptr, 16);
          if (low >= 0xDC00 && low <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            i += 6;
          }
        }
        if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
        if (cp < 0x80) out.push_back((char)cp);
        else if (cp < 0x800) { out.push_back((char)(0xC0 | (cp >> 6))); out.push_back((char)(0x80 | (cp & 0x3F))); }
        else if (cp < 0x10000) { out.push_back((char)(0xE0 | (cp >> 12))); out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
               out.push_back((char)(0x80 | (cp & 0x3F))); }
        else { out.push_back((char)(0xF0 | (cp >> 18))); out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
               out.push_back((char)(0x80 | ((cp >> 6) & 0x3F))); out.push_back((char)(0x80 | (cp & 0x3F))); }
        break;
      }
      default: out.push_back(e); break;
    }
  }
  return out;
}

Json::Ref Json::Ref::operator[](size_t i) const {
  if (!Valid()) return Ref();
  const Json::Node &n = Doc->Nodes_[(size_t)Node];
  if (i >= n.Count) return Ref();
  return Ref(Doc, Doc->Kids_[n.First + i]);
}

std::string Json::Ref::Key(size_t i) const {
  if (!Valid()) return std::string();
  const Json::Node &n = Doc->Nodes_[(size_t)Node];
  if (n.K != Kind::Object || i >= n.Count) return std::string();
  const Json::Node &c = Doc->Nodes_[(size_t)Doc->Kids_[n.First + i]];
  return Doc->Decode(c.Key, c.KeyLen, c.KeyEscaped);
}

Json::Ref Json::Ref::operator[](const char *key) const {
  if (!Valid() || !key) return Ref();
  const Json::Node &n = Doc->Nodes_[(size_t)Node];
  if (n.K != Kind::Object) return Ref();
  const size_t klen = std::strlen(key);
  for (uint32_t i = 0; i < n.Count; i++) {
    const int32_t kid = Doc->Kids_[n.First + i];
    const Json::Node &c = Doc->Nodes_[(size_t)kid];
    if (c.KeyEscaped) {
      if (Doc->Decode(c.Key, c.KeyLen, true) == key) return Ref(Doc, kid);
    } else if (c.KeyLen == klen && !std::memcmp(Doc->Text_.c_str() + c.Key, key, klen)) {
      return Ref(Doc, kid);
    }
  }
  return Ref();
}

double Json::Ref::Num(double def) const {
  if (!Valid()) return def;
  const Json::Node &n = Doc->Nodes_[(size_t)Node];
  // a bool is not a number: "byteLength": true reaching a size door as 1.0 is the
  // interconversion this line refuses
  return n.K == Kind::Number ? n.Num : def;
}

bool Json::Ref::Bool(bool def) const {
  if (!Valid()) return def;
  const Json::Node &n = Doc->Nodes_[(size_t)Node];
  return n.K == Kind::Bool ? n.Num != 0.0 : def;
}

std::string Json::Ref::Str(const char *def) const {
  if (!Valid()) return def;
  const Json::Node &n = Doc->Nodes_[(size_t)Node];
  return n.K == Kind::String ? Doc->Decode(n.Str, n.StrLen, n.Escaped) : std::string(def);
}

bool Json::Ref::StrEquals(const char *s) const {
  if (!Valid() || !s) return false;
  const Json::Node &n = Doc->Nodes_[(size_t)Node];
  if (n.K != Kind::String) return false;
  if (n.Escaped) return Doc->Decode(n.Str, n.StrLen, true) == s;
  const size_t l = std::strlen(s);
  return n.StrLen == l && !std::memcmp(Doc->Text_.c_str() + n.Str, s, l);
}

}
