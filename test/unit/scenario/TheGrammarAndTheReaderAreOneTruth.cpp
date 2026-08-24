#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "Check.h"

#include "ScenarioRead.h"

namespace {
size_t gAllocations = 0;
}
void *operator new(size_t bytes) {
  ++gAllocations;
  void *held = std::malloc(bytes == 0 ? 1 : bytes);
  if (held == nullptr) { std::abort(); }
  return held;
}
void *operator new[](size_t bytes) { return operator new(bytes); }
void operator delete(void *held) noexcept { std::free(held); }
void operator delete[](void *held) noexcept { std::free(held); }
void operator delete(void *held, size_t) noexcept { std::free(held); }
void operator delete[](void *held, size_t) noexcept { std::free(held); }

using outshine::ReadScenario;
using outshine::Scenario;

namespace {

[[nodiscard]] bool Reads(const char *text, std::string &error) {
  Scenario declared;
  return ReadScenario(text, std::strlen(text), declared, error);
}

[[nodiscard]] bool RefusedNaming(const char *text, const char *naming, std::string &error) {
  if (Reads(text, error)) { return false; }
  const bool named = error.find(naming) != std::string::npos;
  if (!named) { std::printf("FOUND the refusal reads: %s\n", error.c_str()); }
  return named;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string error;

  // board:1760 drift one: the grammar permitted node= on <contact> and <seat>, Contact has
  // no field for it, and the value evaporated on a document that loaded clean. There is now
  // no attribute column to drift from: the reader is the only truth about attributes, and
  // an attribute nobody asked for is the refusal.
  CHECK(RefusedNaming("<scenario name=\"v\"><vehicle name=\"car\">"
                      "<contact at=\"fl\" node=\"wheel_fl\" x=\"1\" y=\"0\" z=\"1\"/>"
                      "</vehicle></scenario>",
                      "node", error),
        "**AN ATTRIBUTE NOBODY READS IS REFUSED**, naming it -- a value that evaporates is a "
        "declaration the author believes and the engine ignores");
  CHECK(error.find("scenario/vehicle/contact") != std::string::npos,
        "and the refusal names the PATH it stands at, so the author can find it");

  CHECK(RefusedNaming("<scenario name=\"v\"><vehicle name=\"car\">"
                      "<seat at=\"driver\" node=\"seat_fl\"/></vehicle></scenario>",
                      "node", error),
        "the same holds at <seat>, which carried the same dead attribute");

  CHECK(RefusedNaming("<scenario name=\"t\"><world lat=\"52\" latitude=\"52\"/></scenario>",
                      "latitude", error),
        "a near-miss spelling is refused rather than silently dropped");

  CHECK(Reads("<scenario name=\"v\"><vehicle name=\"car\">"
              "<contact at=\"fl\" x=\"1\" y=\"0\" z=\"1\"/></vehicle></scenario>", error),
        "and the same element without the dead attribute reads");

  // board:1760 drift two: the grammar's Required column stood in front of the stand-ups'
  // refusals as a second, weaker truth. Every attribute a stand-up refuses the absence of
  // now arrives at the grammar, which names the element rather than four layers later.
  CHECK(RefusedNaming("<scenario name=\"t\"><views><view follows=\"car\" person=\"third\"/>"
                      "</views></scenario>", "id", error),
        "**THE GRAMMAR REFUSES WHAT THE STAND-UP WOULD REFUSE**: a view without an id is "
        "refused at the door, not by ViewBook four layers later");
  CHECK(RefusedNaming("<scenario name=\"t\"><views><view id=\"chase\" follows=\"car\"/>"
                      "</views></scenario>", "person", error),
        "and a view without a person, which ViewBook has no default for");
  CHECK(RefusedNaming("<scenario name=\"t\"><views><view id=\"chase\" person=\"third\"/>"
                      "</views></scenario>", "follows", error),
        "and a view that follows nothing");
  CHECK(RefusedNaming("<scenario name=\"t\"><events><event name=\"e\"/></events>"
                      "<volumes><volume id=\"v\" when=\"enter\"/></volumes></scenario>",
                      "fires", error),
        "a volume that fires nothing is refused at the door");
  CHECK(RefusedNaming("<scenario name=\"t\"><audio><bus gainDb=\"0\"/></audio></scenario>",
                      "id", error),
        "and a bus without an id, which BusGraph refuses because nothing can route into it");

  CHECK(Reads("<scenario name=\"t\">"
              "<views><view id=\"chase\" follows=\"car\" person=\"third\"/></views>"
              "<audio><bus id=\"master\"/></audio></scenario>", error),
        "and the rows that spell what their stand-up needs read");

  // board:1770: the refusal walk built a path string for EVERY node it visited, holding
  // breadth-many strings at once, to name ONE attribute. The cost of finding the answer
  // must not grow with the size of the document that carries it.
  {
    const auto walkCost = [](int rows, size_t &spent, std::string &named) {
      // the dead attribute sits on the LAST row, not beside the root: a walk that holds a
      // path per node pays for the whole breadth before it reaches the answer, and one that
      // carries a single path does not.
      std::string big = "<scenario name=\"crowd\"><instances>";
      for (int at = 0; at < rows; ++at) {
        big += "<instance of=\"car\" id=\"c" + std::to_string(at) + "\" x=\"" +
               std::to_string(at) + "\"";
        big += at + 1 == rows ? " latitude=\"52\"/>" : "/>";
      }
      big += "</instances></scenario>";
      outshine::Xml document;
      if (!document.Parse(big.c_str(), big.size())) { return false; }
      Scenario declared;
      std::string why;
      if (ReadScenario(document, declared, why)) { return false; }
      // the reader has now asked every attribute it reads; this second call walks the same
      // document for the same answer, and ONLY its cost is counted.
      const size_t before = gAllocations;
      const outshine::Xml::Unread found = document.FirstUnread();
      spent = gAllocations - before;
      named = found.Attribute;
      return true;
    };

    size_t small = 0, large = 0;
    std::string smallSaid, largeSaid;
    CHECK(walkCost(100, small, smallSaid) && smallSaid == "latitude",
          "a document of 100 rows names its dead attribute");
    CHECK(walkCost(4000, large, largeSaid) && largeSaid == "latitude",
          "and so does one of 4000 -- the same answer, 40x the document");
    Note("allocations the refusal walk spends over 100 rows", (double)small, "allocations");
    Note("allocations it spends over 4000 rows", (double)large, "allocations");

    CHECK(large == small,
          "**THE REFUSAL'S WALK COSTS THE SAME WHATEVER IT WALKS**: naming one unread "
          "attribute builds ONE path that grows and shrinks along the descent, not one "
          "string per node held all at once (board:1770)");
    CHECK(large <= 4,
          "and that cost is a small constant, not a number that happens to match");

    // board:1782: kXmlMaxDepth bounds OPEN elements -- an empty element is a node the parser
    // never pushes, so the deepest chain a document may carry is one longer than the depth
    // bound. The walk's stack is reserved for that chain, and the deepest document the
    // parser ACCEPTS must be walked without the reserve being exceeded.
    // only the innermost element carries an attribute, so the walk must reach the bottom of
    // the chain to answer -- and the answer it builds is the whole path.
    std::string deep;
    for (size_t at = 0; at < outshine::kXmlMaxDepth; ++at) { deep += "<n>"; }
    deep += "<n latitude=\"52\"/>";
    for (size_t at = 0; at < outshine::kXmlMaxDepth; ++at) { deep += "</n>"; }

    outshine::Xml nested;
    const bool parsed = nested.Parse(deep.c_str(), deep.size());
    if (!parsed) { std::printf("REFUSED %s\n", nested.Error().c_str()); }
    CHECK(parsed, "a document nested to the parser's own bound is accepted");
    if (parsed) {
      const size_t before = gAllocations;
      const outshine::Xml::Unread found = nested.FirstUnread();
      const size_t spent = gAllocations - before;
      Note("the deepest chain the parser accepts", (double)outshine::kXmlDeepestChain, "nodes");
      Note("allocations the walk spends over it", (double)spent, "allocations");
      size_t steps = 0;
      for (const char c : found.Path) { steps += c == '/' ? 1 : 0; }
      Note("slashes in the path the walk built", (double)steps, "steps");
      CHECK(found.Attribute == "latitude" && steps == outshine::kXmlMaxDepth,
            "**AND THE WALK ANSWERS AT THE PARSER'S OWN DEPTH**: the deepest document it "
            "accepts is one the refusal walk can still name an attribute in, with the whole "
            "chain in the path it hands back (board:1782)");
      // the walk's own allocations at this depth are countable: two strings for the Unread
      // it returns, and the path growing from the small-string bound to 65 nodes x 2 chars
      // = 130 bytes, which is three doublings. The stack vector adds NOTHING, because it is
      // reserved for the deepest chain -- reserve it for the depth bound instead, which is
      // one shorter, and it reallocates for a sixth.
      constexpr size_t kUnreadStrings = 2;
      constexpr size_t kPathDoublings = 3;
      CHECK(spent <= kUnreadStrings + kPathDoublings,
            "and the stack itself allocates NOTHING at that depth -- it is reserved for the "
            "deepest chain the parser accepts, not for the depth bound that is one shorter "
            "(board:1782)");
    }
  }

  Covers("III.10 the XML door has ONE truth about attributes -- the reader -- and an "
         "attribute nobody asks for is refused by its path, the way the JSON door already "
         "refuses an unread property; every absence a stand-up refuses is refused at the "
         "grammar instead (board:1760)");
  return Report();
}
