/* A read-only JSON DOM over a borrowed byte range — everything a GLB's JSON chunk and the model
 * sidecar need and nothing else. Stdlib only: a glTF file is two chunks and a component table, not a
 * reason to take a dependency. Nodes live in ONE flat pool, so parsing a 80 kB chunk is a handful of
 * vector growths and no per-value allocation; a Ref is an index, copied freely.
 * A missing key yields an INVALID Ref rather than an error, and every reader supplies its default —
 * that is what lets the sidecar gain fields without the renderer noticing. */
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

  /* A cursor into the owning Json. Never outlives it and never owns anything. */
  class Ref {
  public:
    Ref() = default;
    Ref(const Json *doc, int32_t node) : Doc(doc), Node(node) {}

    [[nodiscard]] bool Valid() const { return Doc && Node >= 0; }
    Kind GetKind() const { return Valid() ? Doc->Nodes_[(size_t)Node].K : Kind::Invalid; }
    size_t Size() const { return Valid() ? Doc->Nodes_[(size_t)Node].Count : 0; }

    Ref operator[](size_t i) const;
    Ref operator[](const char *key) const;

    double Num(double def = 0.0) const;
    int Int(int def = 0) const { return (int)Num((double)def); }
    [[nodiscard]] bool Bool(bool def = false) const;
    std::string Str(const char *def = "") const;
    [[nodiscard]] bool StrEquals(const char *s) const;

  private:
    const Json *Doc = nullptr;
    int32_t Node = -1;
  };

  /* `text` is copied; the DOM's strings point into the copy, so the caller's buffer may go. */
  [[nodiscard]] bool Parse(const char *text, size_t len);
  [[nodiscard]] bool Ok() const { return Ok_; }
  Ref Root() const { return Ref(this, Nodes_.empty() ? -1 : 0); }

private:
  friend class Ref;

  struct Node {
    Kind K = Kind::Invalid;
    double Num = 0.0;
    uint32_t Str = 0, StrLen = 0;   /* into Text_ (unescaped strings are re-emitted into Pool_) */
    uint32_t Key = 0, KeyLen = 0;
    uint32_t First = 0, Count = 0;  /* into Kids_ */
    bool Escaped = false, KeyEscaped = false;
  };

  int32_t ParseValue();
  [[nodiscard]] bool ParseString(uint32_t &off, uint32_t &len, bool &escaped);
  void Skip();
  std::string Decode(uint32_t off, uint32_t len, bool escaped) const;

  std::string Text_;
  std::vector<Node> Nodes_;
  std::vector<int32_t> Kids_;
  size_t P_ = 0;
  bool Ok_ = false;
};

} // namespace outshine
#endif
