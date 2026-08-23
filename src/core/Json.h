#ifndef JSON_H
#define JSON_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outshine {

class Json {
public:
  enum class Kind : uint8_t { Invalid, Null, Bool, Number, String, Array, Object };

  class Ref {
  public:
    Ref() = default;
    Ref(const Json *doc, int32_t node) : Doc(doc), Node(node) {}

    [[nodiscard]] bool Valid() const { return Doc && Node >= 0; }
    [[nodiscard]] Kind GetKind() const { return Valid() ? Doc->Nodes_[(size_t)Node].K : Kind::Invalid; }
    [[nodiscard]] size_t Size() const { return Valid() ? Doc->Nodes_[(size_t)Node].Count : 0; }

    [[nodiscard]] Ref operator[](size_t i) const;
    [[nodiscard]] Ref operator[](const char *key) const;

    [[nodiscard]] std::string Key(size_t i) const;

    [[nodiscard]] double Num(double def = 0.0) const;
    // a hostile 1e300 is not an index: outside int's range or not whole, the caller's
    // default answers -- the raw cast alone was UB
    [[nodiscard]] int Int(int def = 0) const {
      const double v = Num((double)def);
      if (!(v >= -2147483648.0) || !(v <= 2147483647.0) || v != (double)(long long)v) {
        return def;
      }
      return (int)v;
    }
    [[nodiscard]] bool Bool(bool def = false) const;
    [[nodiscard]] std::string Str(const char *def = "") const;
    [[nodiscard]] bool StrEquals(const char *s) const;

  private:
    const Json *Doc = nullptr;
    int32_t Node = -1;
  };

  [[nodiscard]] bool Parse(const char *text, size_t len);
  [[nodiscard]] bool Ok() const { return Ok_; }

  [[nodiscard]] size_t StoppedAt() const { return P_; }
  [[nodiscard]] Ref Root() const { return Ref(this, Nodes_.empty() ? -1 : 0); }

private:
  friend class Ref;

  struct Node {
    Kind K = Kind::Invalid;
    double Num = 0.0;
    uint32_t Str = 0, StrLen = 0;
    uint32_t Key = 0, KeyLen = 0;
    uint32_t First = 0, Count = 0;
    bool Escaped = false, KeyEscaped = false;
  };

  int32_t ParseValue();
  int32_t ParseValueInside();
  // [SET] the nesting bound: glTF and every provider manifest sit under ten levels, and
  // a counter is what keeps a hostile depth bomb a REFUSAL instead of a blown C stack
  static constexpr size_t kMostDepth = 256;
  size_t Depth_ = 0;
  [[nodiscard]] bool ParseString(uint32_t &off, uint32_t &len, bool &escaped);
  void Skip();
  std::string Decode(uint32_t off, uint32_t len, bool escaped) const;

  std::string Text_;
  std::vector<Node> Nodes_;
  std::vector<int32_t> Kids_;
  size_t P_ = 0;
  bool Ok_ = false;
};

}
#endif
