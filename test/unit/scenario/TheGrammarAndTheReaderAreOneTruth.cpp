#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "Check.h"

#include "ScenarioRead.h"

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

  // board:1782: what the refusal walk COSTS is src/core/Xml.cpp behaviour and its proof moved
  // to the mirror of the file that owns it, test/unit/core/AnXmlDocumentReadsAsWhatItDeclares.
  // What stays here is what src/scenario/ owns: that the reader asks for every attribute the
  // grammar declares, so an attribute nobody asks for is a finding rather than a habit.
  {
    std::string big = "<scenario name=\"crowd\"><instances>";
    big += "<instance of=\"car\" id=\"c0\" x=\"0\" latitude=\"52\"/>";
    big += "</instances></scenario>";
    outshine::Xml document;
    CHECK(document.Parse(big.c_str(), big.size()), "a scenario with a stranger attribute parses");
    Scenario declared;
    std::string why;
    CHECK(!ReadScenario(document, declared, why),
          "and the reader refuses it rather than ignoring what it does not know");
    const outshine::Xml::Unread found = document.FirstUnread();
    Note("the attribute the reader never asked for", 1.0, found.Attribute.c_str());
    CHECK(found.Attribute == "latitude",
          "**AND THE ATTRIBUTE NOBODY ASKED FOR IS NAMED**: the reader is the one truth about "
          "what a scenario may spell, so a stranger in the file is found by asking the reader "
          "what it read and not by a second list beside it (board:1770)");
  }

  Covers("III.10 the XML door has ONE truth about attributes -- the reader -- and an "
         "attribute nobody asks for is refused by its path, the way the JSON door already "
         "refuses an unread property; every absence a stand-up refuses is refused at the "
         "grammar instead (board:1760)");
  return Report();
}
