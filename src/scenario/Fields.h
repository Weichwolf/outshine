/* READING ONE DECLARED OBJECT, WITH NO WAY TO IGNORE PART OF IT. Every reader below marks the key it
 * read; `Closed()` then refuses any key that is present and was not, naming its full path. That is
 * the whole mechanism behind "an unknown property is refused": a misspelt field cannot fall through
 * to a default, and a field the engine stopped reading cannot survive in the declaration — which is
 * how ten scenes came to carry eighty fields nothing had read for months.
 *
 * The error is written once, at the first refusal, and every later call is a no-op: a reader states
 * its fields in a row and asks at the end, instead of threading a bool through each line. */
#ifndef FIELDS_H
#define FIELDS_H

#include <string>
#include <vector>

#include "Json.h"

namespace outshine::Scenario {

class Fields {
public:
  Fields(const Json::Ref &node, std::string path, std::string &err)
      : Node_(node), Path_(std::move(path)), Err_(err) {
    if (Err_.empty() && Node_.GetKind() != Json::Kind::Object) (void)Refuse("is not an object");
  }
  Fields(const Fields &) = delete;
  Fields &operator=(const Fields &) = delete;

  const std::string &Path() const { return Path_; }
  /* The path this object names itself by, restated once its own id is known: a refusal reads better
   * as `scenes(walk).fovDeg` than as `scenes[0].fovDeg`, and the id is only known after one read. */
  void Rename(std::string path) { Path_ = std::move(path); }

  /* A key that is present, of the right kind and inside its declared range, or a refusal naming the
   * path and the bound. A range is part of the declaration: "eyeM" without one is a metre count that
   * can be a light-year. */
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

  /* A nested object or array, marked read; the caller builds a Fields of its own over it with the
   * path this one hands it, so a refusal three levels down still names all three. */
  Json::Ref Child(const char *key) { return Take(key); }
  /* Accounted for without being read here: a nested reader takes it, or it is absent and its
   * declared default stands. Without this, `Closed()` would refuse a key its own reader handled. */
  void Seen(const char *key) { (void)Mark(key); }
  [[nodiscard]] bool Present(const char *key) const {
    return Node_[key].GetKind() != Json::Kind::Invalid;
  }
  std::string Under(const char *key) const { return Path_ + "." + key; }

  /* Whatever the caller could not say in a Need: a refusal in this object's own words, at its path. */
  [[nodiscard]] bool Refuse(const char *key, const std::string &why) {
    if (Err_.empty()) Err_ = Path_ + "." + key + " " + why;
    return false;
  }
  [[nodiscard]] bool Refuse(const std::string &why) {
    if (Err_.empty()) Err_ = Path_ + " " + why;
    return false;
  }

  /* EVERY KEY ACCOUNTED FOR. Called once, at the end of a reader. */
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

} // namespace outshine::Scenario
#endif
