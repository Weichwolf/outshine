#include <cstdio>
#include <cstdlib>
#include <string>

#include "Check.h"

#include "Heap.h"
#include "Xml.h"

using outshine::Xml;

// board:1782: the refusal walk's cost is src/core/Xml.cpp behaviour and its proof stood in
// test/unit/scenario/, dragging a whole ReadScenario through its setup to reach it. The unit
// mirror IS the layering proof; a core bound proven through the scenario reader weakens
// exactly what the mirror is for. Measured here against Xml alone, with the tree's own heap
// instrument rather than a second operator new -- src/core/io/Heap.cpp already owns those.
namespace {

const char *kScenario = R"(<?xml version="1.0" encoding="utf-8"?>
<!-- a scenario the size of a studio shot -->
<scenario name="four lines" epoch="2287">
  <frame widthPx="1280" heightPx="720"/>
  <world lat="44.38" lon="4.42" utc="2287-10-23T09:00:00Z"/>
  <generators>
    <generator kind="tree" species="beech" seed="7"/>
    <generator kind="house" storeys="3" ruined="true"/>
  </generators>
  <assets>
    <asset uri="scene.gltf" kind="gltf">the body this scenario stands up</asset>
  </assets>
</scenario>
)";

bool Refuses(const char *text, const char *what) {
  Xml document;
  const bool read = document.Parse(text, std::strlen(text));
  if (!read) { std::printf("NOTE %-22s -> %s\n", what, document.Error().c_str()); }
  return !read;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Xml document;
  const bool read = document.Parse(kScenario, std::strlen(kScenario));
  if (!read) { std::printf("REFUSED %s\n", document.Error().c_str()); }
  CHECK(read, "a scenario with a declaration, a comment, attributes, nesting and text is read");
  if (!read) { return Report(); }

  const Xml::Ref root = document.Root();
  CHECK(root.Name() == "scenario", "the root is the element the document opens with");
  CHECK(root.Attr("name") == "four lines",
        "an attribute's value carries its spaces, so it is a value and not a token");
  CHECK(root.Int("epoch", -1) == 2287, "a number reads as a number");
  CHECK(root.Attr("absent", "declared nowhere") == "declared nowhere",
        "and an attribute the element does not carry answers what the caller declared");

  const Xml::Ref frame = root.Child("frame");
  CHECK(frame.Valid() && frame.Int("widthPx", 0) == 1280 && frame.Int("heightPx", 0) == 720,
        "an empty element carries its attributes and closes itself");

  CHECK(root.Child("world").Num("lat", 0.0) > 44.37 && root.Child("world").Num("lat", 0.0) < 44.39,
        "a fractional number reads as one");

  const Xml::Ref generators = root.Child("generators");
  CHECK(generators.Count("generator") == 2, "a repeated child is counted rather than overwritten");
  CHECK(generators.At("generator", 0).Attr("kind") == "tree" &&
            generators.At("generator", 1).Attr("kind") == "house",
        "and the repetition keeps the document's order");
  CHECK(generators.At("generator", 1).Flag("ruined", false),
        "a flag reads true from 'true'");
  CHECK(!generators.At("generator", 0).Flag("ruined", false),
        "and answers the declared default where the attribute is absent");

  CHECK(root.Child("assets").Child("asset").Text() == "the body this scenario stands up",
        "an element's text is its own, trimmed of the whitespace the indentation put there");
  CHECK(!root.Child("nothing").Valid(),
        "a child that is not there is invalid rather than a default-constructed lie");

  Note("elements read", (double)document.NodeCount(), "elements");

  size_t refused = 0;
  refused += Refuses("<a><b></a></b>", "crossed tags") ? 1u : 0u;
  refused += Refuses("<a>", "never closed") ? 1u : 0u;
  refused += Refuses("<a/><b/>", "two roots") ? 1u : 0u;
  refused += Refuses("<!DOCTYPE a><a/>", "a doctype") ? 1u : 0u;
  refused += Refuses("<svg:rect/>", "a namespace") ? 1u : 0u;
  refused += Refuses("<a x=1/>", "an unquoted value") ? 1u : 0u;
  refused += Refuses("<a x/>", "a valueless attribute") ? 1u : 0u;
  refused += Refuses("</a>", "a close with nothing open") ? 1u : 0u;
  CHECK(refused == 8, "every shape outside the subset is refused, and the refusal says which");

  std::string tooDeep;
  for (size_t at = 0; at < outshine::kXmlMaxDepth + 2; ++at) { tooDeep += "<a>"; }
  CHECK(Refuses(tooDeep.c_str(), "past the depth bound"),
        "a document that nests past the bound is refused with the bound named");

  // board:1770's cost, proven where the code it measures lives. The dead attribute sits on the
  // LAST row: a walk holding a path per node pays for the whole breadth before it reaches the
  // answer, and one carrying a single path does not.
  size_t small = 0, large = 0;
  {
    const auto walkCost = [](int rows, size_t &spent, std::string &named) {
      std::string big = "<rows>";
      for (int at = 0; at < rows; ++at) {
        big += "<r id=\"c" + std::to_string(at) + "\"";
        big += at + 1 == rows ? " dead=\"1\"/>" : "/>";
      }
      big += "</rows>";
      Xml document;
      if (!document.Parse(big.c_str(), big.size())) { return false; }
      // ask every attribute the document carries EXCEPT the dead one, through Xml's own door.
      for (const Xml::Ref row : document.Root().Children("r")) { (void)row.Attr("id"); }
      const size_t before = outshine::Heap::TakenUnder("walk");
      {
        const outshine::Heap::Tagged under("walk");
        const Xml::Unread found = document.FirstUnread();
        named = found.Attribute;
      }
      spent = outshine::Heap::TakenUnder("walk") - before;
      return true;
    };

    std::string smallSaid, largeSaid;
    CHECK(walkCost(100, small, smallSaid) && smallSaid == "dead",
          "a document of 100 rows names its unasked attribute");
    CHECK(walkCost(4000, large, largeSaid) && largeSaid == "dead",
          "and so does one of 4000 -- the same answer, 40x the document");
    Note("bytes the refusal walk takes over 100 rows", (double)small, "bytes");
    Note("bytes it takes over 4000 rows", (double)large, "bytes");
    CHECK(large == small,
          "**THE REFUSAL'S WALK COSTS THE SAME WHATEVER IT WALKS**: naming one unasked "
          "attribute builds ONE path that grows and shrinks along the descent, not one string "
          "per node held all at once (board:1770)");
    CHECK(large > 0 && large < 4096,
          "and that cost is a small constant, not a number that happens to match");
  }

  // board:1782: kXmlMaxDepth bounds OPEN elements. An empty element is a node the parser never
  // pushes, so the deepest CHAIN a document may carry is one longer than the depth bound --
  // and the walk's stack is reserved for the chain, not for the bound.
  {
    std::string deep;
    for (size_t at = 0; at < outshine::kXmlMaxDepth; ++at) { deep += "<n>"; }
    deep += "<n dead=\"1\"/>";
    for (size_t at = 0; at < outshine::kXmlMaxDepth; ++at) { deep += "</n>"; }

    Xml nested;
    const bool parsed = nested.Parse(deep.c_str(), deep.size());
    if (!parsed) { std::printf("REFUSED %s\n", nested.Error().c_str()); }
    CHECK(parsed,
          "**A DOCUMENT AT THE PARSER'S OWN BOUND IS ACCEPTED**: a bound with only its "
          "refusing side tested is one edit away from refusing everything (board:1782)");
    if (parsed) {
      const size_t before = outshine::Heap::TakenUnder("deep walk");
      Xml::Unread found;
      {
        const outshine::Heap::Tagged under("deep walk");
        found = nested.FirstUnread();
      }
      const size_t spent = outshine::Heap::TakenUnder("deep walk") - before;
      size_t steps = 0;
      for (const char c : found.Path) { steps += c == '/' ? 1 : 0; }
      Note("the deepest chain the parser accepts", (double)outshine::kXmlDeepestChain, "nodes");
      Note("bytes the walk takes over it", (double)spent, "bytes");
      Note("slashes in the path the walk built", (double)steps, "steps");
      CHECK(found.Attribute == "dead" && steps == outshine::kXmlMaxDepth,
            "and the walk answers at that depth with the whole chain in the path it hands back");
      // two strings for the Unread it returns, and the path growing from the small-string
      // bound to 65 nodes x 2 chars = 130 bytes, which is three doublings. The stack vector
      // adds NOTHING, because it is reserved for the deepest chain rather than for the depth
      // bound that is one shorter.
      // What the walk may take at this depth, composed rather than guessed: the stack
      // reserved ONCE for the deepest chain (65 x 16 bytes = 1040), the path growing by
      // doubling to 65 nodes x 2 characters = 130 (32 + 64 + 128 + 256 = 480), and the two
      // strings the Unread hands back. A reserve for the depth BOUND -- one shorter -- pushes
      // a 66th entry and the vector doubles, which is another 2080 bytes and puts the walk
      // past this line.
      constexpr size_t kStackReserveBytes = 1040;
      constexpr size_t kPathDoublings = 480;
      constexpr size_t kUnreadStrings = 528;
      Note("what the walk may take here", (double)(kStackReserveBytes + kPathDoublings + kUnreadStrings),
           "bytes");
      CHECK(spent > 0 && spent <= kStackReserveBytes + kPathDoublings + kUnreadStrings,
            "**AND THE STACK IS RESERVED ONCE AND NEVER GROWN**: it holds the deepest chain "
            "the parser accepts, so the last push at that depth fits -- reserve it for the "
            "depth bound, which is one shorter, and the vector doubles on that push "
            "(board:1782)");
      CHECK(spent > small,
            "and the deep walk costs MORE than the wide one, which is what says the cost "
            "follows the DEPTH of the answer and not the size of the document");
    }
  }

  Covers("I.4 a scenario is read from XML: elements, attributes, text and the bounds each declares, "
         "and everything outside that subset is a refusal that names itself");
  return Report();
}
