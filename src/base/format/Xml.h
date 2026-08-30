#ifndef OUTSHINE_BASE_FORMAT_XML_H
#define OUTSHINE_BASE_FORMAT_XML_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace outshine {

inline constexpr size_t kXmlMaxDepth = 64;
inline constexpr size_t kXmlDeepestChain = kXmlMaxDepth + 1;
inline constexpr size_t kXmlMaxNodes = 65536;
inline constexpr size_t kXmlMaxAttributes = 262144;
inline constexpr uint32_t kNoAttribute = 0xFFFFFFFFu;

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
    [[nodiscard]] bool Spelt(const char *attribute) const;
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

    class Siblings {
    public:
      class Iterator {
      public:
        using iterator_concept = std::forward_iterator_tag;
        using iterator_category = std::forward_iterator_tag;
        using value_type = Ref;
        using difference_type = std::ptrdiff_t;

        Iterator() = default;

        Iterator(const Xml *from, uint32_t at, const char *name)
            : From_(from), At_(at), Name_(name), Want_(name == nullptr ? 0 : std::strlen(name)) {
          Settle();
        }

        [[nodiscard]] Ref operator*() const { return Ref(From_, At_); }

        Iterator &operator++();

        Iterator operator++(int) {
          Iterator was = *this;
          ++*this;
          return was;
        }

        [[nodiscard]] bool operator==(const Iterator &other) const { return At_ == other.At_; }

      private:
        void Settle();
        [[nodiscard]] bool Named() const;

        const Xml *From_ = nullptr;
        uint32_t At_ = 0;
        const char *Name_ = nullptr;
        size_t Want_ = 0;
      };

      Siblings() = default;

      Siblings(const Xml *from, uint32_t first, const char *name)
          : From_(from), First_(first), Name_(name) {}

      [[nodiscard]] Iterator begin() const { return Iterator(From_, First_, Name_); }

      [[nodiscard]] Iterator end() const { return Iterator(); }

    private:
      const Xml *From_ = nullptr;
      uint32_t First_ = 0;
      const char *Name_ = nullptr;
    };

    [[nodiscard]] Siblings Children(const char *name = nullptr) const;

  private:
    [[nodiscard]] uint32_t Asking(const char *attribute) const;

    const Xml *From_ = nullptr;
    uint32_t At_ = 0;
  };

  struct Unread {
    std::string Path;
    std::string Attribute;
  };

  [[nodiscard]] Unread FirstUnread() const;

  [[nodiscard]] bool Parse(const char *text, size_t length);

  [[nodiscard]] Ref Root() const { return Ref(this, Root_); }

  [[nodiscard]] const std::string &Error() const { return Error_; }

  [[nodiscard]] size_t NodeCount() const { return Nodes_.size() ? Nodes_.size() - 1u : 0u; }

  [[nodiscard]] size_t SiblingSteps() const { return SiblingSteps_; }

private:
  friend class Ref;

  [[nodiscard]] std::string Span(uint32_t off, uint32_t len) const {
    return std::string(Text_.data() + off, len);
  }

  [[nodiscard]] bool Refuse(const std::string &why, size_t at);

  std::string Text_;
  std::vector<Node> Nodes_;
  std::vector<Attribute> Attributes_;
  mutable std::vector<uint8_t> Asked_;
  uint32_t Root_ = 0;
  std::string Error_;
  mutable size_t SiblingSteps_ = 0;
};

static_assert(std::ranges::forward_range<Xml::Ref::Siblings>,
              "a scenario is read by walking children once, and the walk is a range");

}

#endif
