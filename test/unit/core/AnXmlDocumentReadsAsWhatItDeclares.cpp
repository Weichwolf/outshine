#include <cstdio>
#include <string>

#include "Check.h"

#include "Xml.h"

using outshine::Xml;

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

  std::string deep;
  for (size_t at = 0; at < outshine::kXmlMaxDepth + 2; ++at) { deep += "<a>"; }
  CHECK(Refuses(deep.c_str(), "past the depth bound"),
        "a document that nests past the bound is refused with the bound named");

  Covers("I.4 a scenario is read from XML: elements, attributes, text and the bounds each declares, "
         "and everything outside that subset is a refusal that names itself");
  return Report();
}
