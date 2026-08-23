#ifndef UI_MARKUP_H
#define UI_MARKUP_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Ui {

enum class NodeKind : uint8_t { Element, Text };

struct Attribute {
  std::string Name;
  std::string Value;
};

struct Node {
  NodeKind Kind = NodeKind::Element;
  std::string Name;
  std::string Text;
  std::vector<Attribute> Attributes;
  std::vector<int> Children;
  int Parent = -1;
};

class Markup {
public:
  [[nodiscard]] bool Read(std::string_view markup, std::string &error);

  [[nodiscard]] const std::vector<Node> &Nodes() const { return Nodes_; }
  [[nodiscard]] int Root() const { return Root_; }

  [[nodiscard]] const std::string &StyleText() const { return Style_; }

  [[nodiscard]] bool CarriesAScript() const { return Scripted_; }

  [[nodiscard]] const std::string *AttributeOf(int node, std::string_view name) const;

private:
  std::vector<Node> Nodes_;
  std::string Style_;
  bool Scripted_ = false;
  int Root_ = -1;
};

[[nodiscard]] bool ClosesItself(std::string_view tag);
[[nodiscard]] bool HoldsRawText(std::string_view tag);
[[nodiscard]] bool IsBlockLevel(std::string_view tag);

}
#endif
