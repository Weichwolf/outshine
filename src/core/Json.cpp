#include "Json.h"

#include <cstdlib>
#include <cstring>

namespace outshine {

/* Kids_ is filled depth-first while a container is still open, so a child's slot cannot be reserved
 * before its subtree exists. Each container therefore collects its member ids on a local vector and
 * copies them into Kids_ on close — bounded by the container's own arity, never by the document. */

bool Json::Parse(const char *text, size_t len) {
  Text_.assign(text, len);
  Nodes_.clear();
  Kids_.clear();
  P_ = 0;
  Ok_ = ParseValue() == 0;
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
  const int32_t id = (int32_t)Nodes_.size();
  Nodes_.emplace_back();
  const char c = Text_[P_];

  if (c == '{' || c == '[') {
    const bool obj = c == '{';
    const char close = obj ? '}' : ']';
    P_++;
    std::vector<int32_t> kids;
    for (;;) {
      Skip();
      if (P_ >= Text_.size()) return -1;
      if (Text_[P_] == close) { P_++; break; }
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
      if (P_ < Text_.size() && Text_[P_] == ',') { P_++; continue; }
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

  if (!std::strncmp(Text_.c_str() + P_, "true", 4)) { P_ += 4; Nodes_[(size_t)id].K = Kind::Bool; Nodes_[(size_t)id].Num = 1.0; return id; }
  if (!std::strncmp(Text_.c_str() + P_, "false", 5)) { P_ += 5; Nodes_[(size_t)id].K = Kind::Bool; Nodes_[(size_t)id].Num = 0.0; return id; }
  if (!std::strncmp(Text_.c_str() + P_, "null", 4)) { P_ += 4; Nodes_[(size_t)id].K = Kind::Null; return id; }

  char *end = nullptr;
  const double v = std::strtod(Text_.c_str() + P_, &end);
  if (!end || end == Text_.c_str() + P_) return -1;
  P_ = (size_t)(end - Text_.c_str());
  Nodes_[(size_t)id].K = Kind::Number;
  Nodes_[(size_t)id].Num = v;
  return id;
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
      /* \uXXXX is decoded to UTF-8 only for the BMP: no glTF or sidecar key needs a surrogate pair,
       * and a lone high surrogate would be a corrupt document rather than a case to handle. */
      case 'u': {
        if (i + 4 >= len) break;
        const unsigned cp = (unsigned)std::strtoul(Text_.substr(off + i + 1, 4).c_str(), nullptr, 16);
        i += 4;
        if (cp < 0x80) out.push_back((char)cp);
        else if (cp < 0x800) { out.push_back((char)(0xC0 | (cp >> 6))); out.push_back((char)(0x80 | (cp & 0x3F))); }
        else { out.push_back((char)(0xE0 | (cp >> 12))); out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
               out.push_back((char)(0x80 | (cp & 0x3F))); }
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
  return n.K == Kind::Number || n.K == Kind::Bool ? n.Num : def;
}

bool Json::Ref::Bool(bool def) const {
  if (!Valid()) return def;
  const Json::Node &n = Doc->Nodes_[(size_t)Node];
  return n.K == Kind::Bool || n.K == Kind::Number ? n.Num != 0.0 : def;
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

} // namespace outshine
