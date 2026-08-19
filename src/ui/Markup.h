/* MARKUP AS A TREE, AND THE READER REFUSES RATHER THAN GUESSES.
 *
 * **This is a reader and not a browser's parser** (board:1442). HTML's own algorithm is a recovery
 * machine: it invents `<tbody>`, reopens formatting elements across block boundaries and has a named
 * rule for a `<table>` inside a `<p>`. None of that is a mechanism a game interface needs, and all of it
 * is what makes a browser's parser the size of a browser. What is here is the shape a declaration
 * actually has -- elements, attributes, text -- with the recovery rules the corpus demands and no others,
 * each one written down.
 *
 * **WHAT IT RECOVERS FROM, AND THE LIST IS THE WHOLE LIST**: a void element closes itself; `<style>`,
 * `<script>` and `<title>` hold raw text to their own end tag; an unmatched end tag pops to the nearest
 * ancestor that matches and is otherwise dropped; a `<p>` is closed by the next block-level start tag,
 * which is the one implied end tag real prose relies on.
 *
 * **WHAT IT REFUSES** is an end tag for a void element and an unterminated raw-text element, because
 * both mean the document says something the writer did not mean and a guess would put content in the
 * wrong box silently. */
#ifndef UI_MARKUP_H
#define UI_MARKUP_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Ui {

enum class NodeKind : uint8_t { Element, Text };

struct Attribute {
  std::string Name;   /* lowercased, because a declaration's spelling is not its meaning */
  std::string Value;
};

struct Node {
  NodeKind Kind = NodeKind::Element;
  std::string Name;   /* lowercased tag name; empty on a text node */
  std::string Text;   /* the run's characters, entities already resolved */
  std::vector<Attribute> Attributes;
  std::vector<int> Children;
  int Parent = -1;
};

class Markup {
public:
  [[nodiscard]] bool Read(std::string_view markup, std::string &error);

  [[nodiscard]] const std::vector<Node> &Nodes(void) const { return Nodes_; }
  [[nodiscard]] int Root(void) const { return Root_; }
  /* EVERY `<style>` OF THE DOCUMENT, IN ORDER AND JOINED. The cascade is over declarations rather than
   * over which element carried them, so keeping them apart would be a distinction nothing reads. */
  [[nodiscard]] const std::string &StyleText(void) const { return Style_; }
  /* The value of an attribute, or null where the element does not carry it. */
  [[nodiscard]] const std::string *AttributeOf(int node, std::string_view name) const;

private:
  std::vector<Node> Nodes_;
  std::string Style_;
  int Root_ = -1;
};

/* THE VOID SET AND THE RAW-TEXT SET, PUBLISHED because a reader's recovery rules are part of what it
 * claims: a consumer can see exactly which tags close themselves and which hold text rather than
 * markup, without reading the implementation. */
[[nodiscard]] bool ClosesItself(std::string_view tag);
[[nodiscard]] bool HoldsRawText(std::string_view tag);
[[nodiscard]] bool IsBlockLevel(std::string_view tag);

} // namespace outshine::Ui
#endif
