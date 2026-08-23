#ifndef OUTSHINE_CORE_XML_H
#define OUTSHINE_CORE_XML_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outshine {

inline constexpr size_t kXmlMaxDepth = 64;
inline constexpr size_t kXmlMaxNodes = 65536;
inline constexpr size_t kXmlMaxAttributes = 262144;

class Xml {
public:
  struct Node {
    uint32_t NameOff = 0, NameLen = 0;
    uint32_t TextOff = 0, TextLen = 0;
    uint32_t FirstChild = 0, NextSibling = 0;
    uint32_t FirstAttribute = 0, Attributes = 0;
  };
  struct Attribute {
    uint32_t NameOff = 0, NameLen = 0;
    uint32_t ValueOff = 0, ValueLen = 0;
  };

  class Ref {
  public:
    Ref() = default;
    Ref(const Xml *from, uint32_t at) : From_(from), At_(at) {}

    [[nodiscard]] bool Valid() const { return From_ != nullptr && At_ != 0; }
    [[nodiscard]] std::string Name() const;
    [[nodiscard]] std::string Text() const;

    [[nodiscard]] bool Has(const char *attribute) const;
    [[nodiscard]] std::string Attr(const char *attribute, const char *whenAbsent = "") const;
    [[nodiscard]] double Num(const char *attribute, double whenAbsent) const;
    [[nodiscard]] long long Int(const char *attribute, long long whenAbsent) const;
    [[nodiscard]] bool Flag(const char *attribute, bool whenAbsent) const;

    [[nodiscard]] size_t AttributeCount() const;
    [[nodiscard]] std::string AttributeAt(size_t which) const;

    [[nodiscard]] Ref Child(const char *name) const;
    [[nodiscard]] Ref First() const;
    [[nodiscard]] Ref Next() const;
    [[nodiscard]] size_t Count(const char *name) const;
    [[nodiscard]] Ref At(const char *name, size_t which) const;

  private:
    const Xml *From_ = nullptr;
    uint32_t At_ = 0;
  };

  [[nodiscard]] bool Parse(const char *text, size_t length);
  [[nodiscard]] Ref Root() const { return Ref(this, Root_); }
  [[nodiscard]] const std::string &Error() const { return Error_; }

  [[nodiscard]] size_t NodeCount() const { return Nodes_.size() ? Nodes_.size() - 1u : 0u; }

private:
  friend class Ref;
  [[nodiscard]] std::string Span(uint32_t off, uint32_t len) const {
    return std::string(Text_.data() + off, len);
  }
  [[nodiscard]] bool Refuse(const std::string &why, size_t at);

  std::string Text_;
  std::vector<Node> Nodes_;
  std::vector<Attribute> Attributes_;
  uint32_t Root_ = 0;
  std::string Error_;
};

} // namespace outshine

#endif
