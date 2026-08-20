/* `KHR_node_visibility`, AND THE HALF THAT IS EASY TO GET WRONG IS INHERITANCE (board:1383).
 *
 * The extension carries one boolean. Its rule is not one boolean's worth: *a node is visible if and
 * only if its own visible property is true and all its parents are visible* -- so a node that says
 * `true` about itself still draws nothing under a parent that says `false`. **A reader that answered
 * each node from its own field would be right on every single-level file and wrong on the first
 * hierarchy**, which is why the chain below is three deep and its leaf is visible.
 *
 * WHAT THE EXTENSION EXCLUDES IS AS NORMATIVE AS WHAT IT HIDES: *visibility affects neither cameras,
 * nor node's interactivity features*. A camera on an invisible node still resolves, and it is checked
 * here rather than assumed, because our flatten implements visibility by NOT DESCENDING and a later
 * round that moved camera resolution into that walk would inherit the skip silently.
 *
 * A LIGHT IS A VISUAL FEATURE AND A CAMERA IS NOT. The specification's list is *meshes, light sources
 * (e.g., attached with KHR_lights_punctual), point clouds, particles, billboards, volumetric
 * effects*, so a hidden node's light leaves the draw list with its geometry.
 *
 * THE SUBJECT IS BUILT IN MEMORY AND NOTHING IS PREPARED. This is a rule about the reader and the
 * flatten; it needs no oracle, no device and no corpus, so a fixture on disk would only add a way for
 * it to be unprepared. */
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

/* Three vertices, no indices, one buffer view. The geometry is irrelevant to what is being decided --
 * what matters is WHICH NODES produced a part -- so it is the smallest thing a mesh can be. */
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

} // namespace

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

  Covers("board:1383 KHR_node_visibility: a hidden node contributes no part and no light, its "
         "descendants inherit that however they answer for themselves, and a camera is exempt");
  return Report();
}
