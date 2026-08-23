#ifndef FIELDS_H
#define FIELDS_H

#include <string>
#include <string_view>
#include <vector>

#include "Json.h"

namespace outshine::SceneLegacy {

class Fields {
public:
  Fields(const Json::Ref &node, std::string path, std::string &err)
      : Node_(node), Path_(std::move(path)), Err_(err) {
    if (Err_.empty() && Node_.GetKind() != Json::Kind::Object) (void)Refuse("is not an object");
  }
  Fields(const Fields &) = delete;
  Fields &operator=(const Fields &) = delete;

  const std::string &Path() const { return Path_; }

  void Rename(std::string path) { Path_ = std::move(path); }

  [[nodiscard]] bool Need(const char *key, double lo, double hi, double &out) {
    const Json::Ref v = Take(key);
    if (!Err_.empty()) return false;
    if (v.GetKind() != Json::Kind::Number) return Refuse(key, "is missing or is not a number");
    out = v.Num();
    if (out < lo || out > hi)
      return Refuse(key, "is outside [" + Trim(lo) + "," + Trim(hi) + "]: " + Trim(out));
    return true;
  }
  [[nodiscard]] bool Optional(const char *key, double lo, double hi, double &out) {
    return Present(key) ? Need(key, lo, hi, out) : Mark(key);
  }
  [[nodiscard]] bool NeedInt(const char *key, double lo, double hi, int &out) {
    double v = (double)out;
    if (!Need(key, lo, hi, v)) return false;
    out = (int)v;
    return true;
  }
  [[nodiscard]] bool OptionalInt(const char *key, double lo, double hi, int &out) {
    return Present(key) ? NeedInt(key, lo, hi, out) : Mark(key);
  }
  [[nodiscard]] bool NeedString(const char *key, std::string &out) {
    const Json::Ref v = Take(key);
    if (!Err_.empty()) return false;
    if (v.GetKind() != Json::Kind::String) return Refuse(key, "is missing or is not a string");
    out = v.Str();
    return true;
  }
  [[nodiscard]] bool OptionalString(const char *key, std::string &out) {
    return Present(key) ? NeedString(key, out) : Mark(key);
  }

  Json::Ref Child(const char *key) { return Take(key); }

  void Seen(const char *key) { (void)Mark(key); }
  [[nodiscard]] bool Present(const char *key) const {
    return Node_[key].GetKind() != Json::Kind::Invalid;
  }
  [[nodiscard]] std::string Under(const char *key) const { return Path_ + "." + key; }

  [[nodiscard]] bool Refuse(const char *key, std::string_view why) {
    if (Err_.empty()) Err_ = Path_ + "." + key + " " + std::string(why);
    return false;
  }
  [[nodiscard]] bool Refuse(std::string_view why) {
    if (Err_.empty()) Err_ = Path_ + " " + std::string(why);
    return false;
  }

  [[nodiscard]] bool Closed() {
    if (!Err_.empty()) return false;
    for (size_t i = 0; i < Node_.Size(); i++) {
      const std::string key = Node_.Key(i);
      bool seen = false;
      for (const std::string &k : Read_) seen = seen || k == key;
      if (!seen) return Refuse(key.c_str(), "is not a property this engine reads");
    }
    return true;
  }

private:
  [[nodiscard]] bool Mark(const char *key) {
    Read_.emplace_back(key);
    return Err_.empty();
  }
  Json::Ref Take(const char *key) {
    (void)Mark(key);
    return Err_.empty() ? Node_[key] : Json::Ref();
  }
  static std::string Trim(double v) {
    std::string s = std::to_string(v);
    while (s.size() > 1 && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
  }

  Json::Ref Node_;
  std::string Path_;
  std::string &Err_;
  std::vector<std::string> Read_;
};

}
#endif
