#include <string>
#include <vector>

#include "Check.h"

#include "Glb.h"

#include "Document.h"
#include "Subject.h"

using outshine::Gltf::Document;
using outshine::Gltf::Subject;
using outshine::Test::Glb;

namespace {

const char *const kJson = R"({
  "asset": { "version": "2.0" },
  "extensionsUsed": [ "KHR_node_visibility", "KHR_lights_punctual" ],
  "scene": 0,
  "scenes": [ { "nodes": [0, 3, 4, 5, 6, 7] } ],
  "nodes": [
    { "name": "root-shown", "children": [1] },
    { "name": "middle-hidden", "children": [2],
      "extensions": { "KHR_node_visibility": { "visible": false } } },
    { "name": "leaf-shown-under-hidden", "mesh": 0 },
    { "name": "body-shown", "mesh": 0, "translation": [2, 0, 0] },
    { "name": "body-hidden", "mesh": 0, "translation": [4, 0, 0],
      "extensions": { "KHR_node_visibility": { "visible": false } } },
    { "name": "lamp-hidden", "extensions": {
        "KHR_lights_punctual": { "light": 0 },
        "KHR_node_visibility": { "visible": false } } },
    { "name": "lamp-shown", "extensions": { "KHR_lights_punctual": { "light": 0 } } },
    { "name": "eye-hidden", "camera": 0,
      "extensions": { "KHR_node_visibility": { "visible": false } } }
  ],
  "cameras": [ { "type": "perspective",
                 "perspective": { "yfov": 0.7, "znear": 0.1, "zfar": 100.0 } } ],
  "extensions": { "KHR_lights_punctual": { "lights": [
      { "type": "point", "intensity": 1.0, "color": [1.0, 1.0, 1.0] } ] } },
  "meshes": [ { "primitives": [ { "attributes": { "POSITION": 0 } } ] } ],
  "buffers": [ { "byteLength": 36 } ],
  "bufferViews": [ { "buffer": 0, "byteOffset": 0, "byteLength": 36 } ],
  "accessors": [ { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
                   "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0] } ]
})";

std::vector<uint8_t> Triangle() {
  const float xyz[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(xyz);
  return std::vector<uint8_t>(bytes, bytes + sizeof(xyz));
}

bool Drew(const Subject &subject, const char *node) {
  for (const auto &part : subject.Parts()) {
    if (part.NodeName == node) { return true; }
  }
  return false;
}

bool Lit(const Subject &subject, const char *node) {
  for (const auto &light : subject.Lights()) {
    if (light.NodeName == node) { return true; }
  }
  return false;
}

}

int main() {
  using namespace outshine::Test;

  const std::vector<uint8_t> glb = Glb(kJson, Triangle());
  Document document;
  const bool read = document.Read({glb.data(), glb.size()}, "visibility.glb");
  CHECK(read, "a file declaring KHR_node_visibility reads");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }

  Subject subject;
  const bool built = subject.Build(document, {});
  CHECK(built, "the subject flattens");
  if (!built) {
    std::printf("       %s\n", subject.Error().c_str());
    return Report();
  }

  Note("parts", (double)subject.Parts().size(), "parts");
  Note("lights", (double)subject.Lights().size(), "lights");
  for (const auto &part : subject.Parts()) {
    std::printf("NOTE drew '%s'\n", part.NodeName.c_str());
  }

  CHECK(Drew(subject, "body-shown"),
        "a node that says nothing about visibility draws, because the extension's default is true "
        "and absence must cost nothing");
  CHECK(!Drew(subject, "body-hidden"), "a node that says false about itself draws nothing");
  CHECK(!Drew(subject, "leaf-shown-under-hidden"),
        "A NODE THAT SAYS TRUE ABOUT ITSELF DRAWS NOTHING UNDER A PARENT THAT SAYS FALSE -- the "
        "extension's rule is a conjunction over the whole path to the root, not a property of the "
        "node, and this leaf is the case a per-node answer gets wrong");
  CHECK(subject.Parts().size() == 1,
        "exactly one of the three mesh-bearing nodes reached the draw list, so nothing was hidden "
        "that should have drawn and nothing drew that should have been hidden");

  CHECK(Lit(subject, "lamp-shown"), "a visible node's light reaches the draw list");
  CHECK(!Lit(subject, "lamp-hidden"),
        "a hidden node's LIGHT leaves with its geometry -- the extension names light sources among "
        "the visual features it hides, so a reader that hid only meshes would light a scene from a "
        "lamp nobody can see");
  CHECK(subject.Lights().size() == 1, "exactly one of the two lights survived");

  outshine::Gltf::Placement placement;
  std::string why;
  CHECK(outshine::Gltf::DeclaredPlacement(document, 0, placement, why),
        "A CAMERA ON A HIDDEN NODE STILL RESOLVES, which the extension states as plainly as what it "
        "hides: visibility affects neither cameras nor interactivity");
  if (!why.empty()) { std::printf("       %s\n", why.c_str()); }

  Covers("KHR_node_visibility: a hidden node contributes no part and no light, its "
         "descendants inherit that however they answer for themselves, and a camera is exempt");
  return Report();
}
