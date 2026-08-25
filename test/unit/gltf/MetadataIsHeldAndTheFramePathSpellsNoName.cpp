#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Document.h"
#include "Subject.h"

using outshine::Gltf::Document;
using outshine::Gltf::Subject;

namespace {

// KHR_xmp_json_ld, fetched from the Khronos registry rather than recalled: packets live at
// extensions.KHR_xmp_json_ld.packets on the document root, and an object points at one by
// index through extensions.KHR_xmp_json_ld.packet.
constexpr const char *kTagged = R"({
  "asset":{"version":"2.0",
           "extensions":{"KHR_xmp_json_ld":{"packet":0}}},
  "extensionsUsed":["KHR_xmp_json_ld"],
  "extensions":{"KHR_xmp_json_ld":{"packets":[
    {"@context":{"dc":"http://purl.org/dc/elements/1.1/"},
     "@id":"",
     "dc:title":"a cube nobody will ever see the title of",
     "dc:creator":"the corpus"}
  ]}},
  "scene":0,
  "scenes":[{"nodes":[0]}],
  "nodes":[{"mesh":0}],
  "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],
  "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3",
                "min":[0.0,0.0,0.0],"max":[1.0,1.0,0.0]}],
  "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36}],
  "buffers":[{"byteLength":36,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA"}]
})";

constexpr const char *kOutOfRange = R"({
  "asset":{"version":"2.0","extensions":{"KHR_xmp_json_ld":{"packet":3}}},
  "extensions":{"KHR_xmp_json_ld":{"packets":[{"@id":""}]}}
})";

[[nodiscard]] bool Reads(const char *text, Document &into) {
  const std::string held = text;
  return into.Read({(const uint8_t *)held.data(), held.size()}, "tagged.gltf");
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Document document;
  const bool read = Reads(kTagged, document);
  if (!read) { std::printf("REFUSED %s\n", document.Error().c_str()); }
  CHECK(read, "a document carrying an XMP packet reads");
  if (!read) { return Report(); }

  Note("packets the document holds", (double)document.Metadata().size(), "packets");
  Note("the packet the asset names", (double)document.MetadataOfAsset(), "index");

  CHECK(document.Metadata().size() == 1 && document.MetadataOfAsset() == 0,
        "**METADATA IS HELD WHERE A NAME IS HELD**: the packets array is read at load and the "
        "asset points into it by index, which is what KHR_xmp_json_ld declares (board:1395)");
  const auto &packet = document.Metadata().front();
  CHECK(packet.Of("dc:title") == "a cube nobody will ever see the title of",
        "and a property is reachable by its prefixed key, the way the packet spells it");

  // board:1849: Of() answered "" for a key that is absent AND for one whose value the reader
  // could not keep -- the fixture's own @context is a JSON object -- so a claim about the first
  // was proven by a predicate that could not tell it from the second.
  Note("the packet carries @context", packet.Carries("@context") ? 1.0 : 0.0, "yes");
  Note("and Of answers a string for it", packet.Of("@context").has_value() ? 1.0 : 0.0, "yes");
  CHECK(!packet.Of("dc:nothing").has_value() && !packet.Carries("dc:nothing"),
        "**A KEY THE PACKET DOES NOT CARRY IS ABSENT**, and the packet says so rather than "
        "answering an empty string");
  CHECK(packet.Carries("@context") && !packet.Of("@context").has_value(),
        "**AND A KEY WHOSE VALUE IS A STRUCTURE IS PRESENT WITHOUT BEING TEXT**: an XMP "
        "property is a JSON-LD value, and this reader keeps strings -- so 'carried, and not as "
        "text' is a third answer, not the same empty one absence gives (board:1849)");

  Document beyond;
  const bool refused = !Reads(kOutOfRange, beyond);
  std::printf("NOTE a packet index past the array says: '%.90s'\n", beyond.Error().c_str());
  CHECK(refused && beyond.Error().find("outside the array") != std::string::npos,
        "**AND AN INDEX OUTSIDE THE ARRAY IT INDEXES IS A REFUSAL**, not a silently dropped "
        "packet -- the same rule every other index in this reader carries");

  Subject subject;
  const bool built = subject.Build(document);
  if (!built) { std::printf("REFUSED %s\n", subject.Error().c_str()); }
  CHECK(built, "the tagged document builds as a subject");
  if (!built) { return Report(); }

  size_t named = 0;
  for (const auto &part : subject.Parts()) {
    named += part.NodeName.find("cube nobody") != std::string::npos ? 1u : 0u;
    named += part.NodeName.find("the corpus") != std::string::npos ? 1u : 0u;
  }
  Note("parts the subject carries", (double)subject.Parts().size(), "parts");
  Note("parts whose name carries a metadata string", (double)named, "parts");

  CHECK(named == 0,
        "**AND THE FRAME PATH SPELLS NO NAME FROM IT**: metadata carries no picture, and that "
        "is precisely the requirement -- it must parse, be reachable, and never put a string "
        "where the draw path can reach one (board:1395)");

  Covers("I.2.10 KHR_xmp_json_ld packets are read at load, reachable by their prefixed keys, "
         "refused when an index leaves its array, and absent from everything the frame path "
         "touches (board:1395)");
  return Report();
}
