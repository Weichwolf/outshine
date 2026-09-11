#include "Markup.h"
#include "Check.h"
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Ui::Markup tree;
  std::string error;
  CHECK(tree.Read("<!doctype html><DIV ID='kept' data-x=bare disabled>A<!--join--> &amp; B<br/>"
                  "<p>P<div>D</div></DIV><style>.x{color:red}</style>"
                  "<title>T &lt; U</title><script>run()</script>",
                  error),
        "representative tree parses");
  CHECK(tree.Nodes().size() == 12 && tree.Root() == 0, "expected element/text topology");
  if (tree.Nodes().size() == 12) {
    CHECK(tree.Nodes()[0].Children == std::vector<int>({1, 8, 9, 11}), "root child order retained");
    CHECK(tree.Nodes()[1].Name == "div" &&
              tree.Nodes()[1].Children == std::vector<int>({2, 3, 4, 6}),
          "block start closes paragraph before sibling div");
    CHECK(tree.Nodes()[2].Text == "A & B" && tree.Nodes()[10].Text == "T < U",
          "comments join text and character references decode in title");
    CHECK(tree.StyleText() == ".x{color:red}\n" && tree.CarriesAScript(),
          "raw style and script retained");
    CHECK(tree.AttributeOf(1, "id") && *tree.AttributeOf(1, "id") == "kept" &&
              tree.AttributeOf(1, "data-x") && *tree.AttributeOf(1, "data-x") == "bare" &&
              tree.AttributeOf(1, "disabled") && tree.AttributeOf(1, "disabled")->empty(),
          "quoted, unquoted and boolean attributes retain values");
    for (size_t at = 0; at < tree.Nodes().size(); ++at) {
      for (const int child : tree.Nodes()[at].Children) {
        CHECK(tree.Nodes()[static_cast<size_t>(child)].Parent == static_cast<int>(at),
              "every child refers back to its parent");
      }
    }
  }
  CHECK(tree.Read("<div id='kept'></div>", error), "replacement parses");
  CHECK(!tree.CarriesAScript() && tree.StyleText().empty(),
        "replacement clears previous raw state");
  const auto *storage = tree.Nodes().data();
  const auto preserved = [&] {
    CHECK(tree.Nodes().data() == storage && tree.Nodes().size() == 2 && tree.AttributeOf(1, "id") &&
              *tree.AttributeOf(1, "id") == "kept" && !tree.CarriesAScript() &&
              tree.StyleText().empty(),
          "failure preserves previous tree and borrowed storage");
  };
  for (const auto *invalid : {"<div id='lost'></div><style>missing", "<div>bad</br>"}) {
    CHECK(!tree.Read(invalid, error), "invalid markup rejected");
    preserved();
  }
  std::vector<std::string> excessive;
  excessive.emplace_back(1024 * 1024 + 1, 'x');
  excessive.emplace_back();
  for (int at = 0; at < 257; ++at) { excessive.back() += "<div>"; }
  excessive.emplace_back();
  for (int at = 0; at < 65536; ++at) { excessive.back() += "<br>"; }
  excessive.emplace_back("<div ");
  for (int at = 0; at < 65537; ++at) { excessive.back() += "a "; }
  excessive.back() += ">";
  for (const auto &invalid : excessive) {
    CHECK(!tree.Read(invalid, error), "parser budget overflow rejected");
    preserved();
  }
  auto depthBoundary = excessive[1];
  depthBoundary.resize(depthBoundary.size() - 5);
  CHECK(tree.Read(depthBoundary, error) && tree.Nodes().size() == 257,
        "maximum open-element depth remains valid");
  auto nodeBoundary = excessive[2];
  nodeBoundary.resize(nodeBoundary.size() - 4);
  CHECK(tree.Read(nodeBoundary, error) && tree.Nodes().size() == 65536,
        "maximum node count includes the document root");
  auto attributeBoundary = excessive[3];
  attributeBoundary.erase(attributeBoundary.size() - 3, 2);
  CHECK(tree.Read(attributeBoundary, error) && tree.Nodes()[1].Attributes.size() == 65536,
        "maximum attribute count remains valid");
  const std::string boundary(1024 * 1024, 'x');
  CHECK(tree.Read(boundary, error) && tree.Nodes().size() == 2 && tree.Nodes()[1].Text == boundary,
        "maximum source byte count remains valid");
  CHECK(tree.Read({}, error) && tree.Nodes().size() == 1 && !tree.CarriesAScript(),
        "empty document replaces previous state with a document root");
  return Report();
}
