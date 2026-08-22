#include <cmath>
#include <filesystem>
#include <string>

#include <unistd.h>

#include "Check.h"

#include "Document.h"
#include "Subject.h"

using outshine::LightKind;
using outshine::Gltf::Document;
using outshine::Gltf::Subject;

namespace {

const char *kSceneJson = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_lights_punctual"],
  "extensionsRequired": ["KHR_lights_punctual"],
  "extensions": {"KHR_lights_punctual": {"lights": [
    {"type": "directional", "name": "Key", "color": [0.9, 0.8, 0.1], "intensity": 1.0},
    {"type": "point", "name": "Lamp", "intensity": 4.0, "range": 1.125},
    {"type": "spot", "name": "Torch", "intensity": 2.0,
     "spot": {"innerConeAngle": 0.2, "outerConeAngle": 0.5}}
  ]}},
  "scene": 0,
  "scenes": [{"nodes": [0, 1, 2]}],
  "nodes": [
    {"mesh": 0, "name": "Plate"},
    {"name": "Sun", "rotation": [0.7071067811865476, 0.0, 0.0, 0.7071067811865476],
     "extensions": {"KHR_lights_punctual": {"light": 0}}},
    {"name": "Rig", "translation": [1.0, 0.0, 0.0], "scale": [2.0, 2.0, 2.0],
     "children": [3, 4]},
    {"name": "Bulb", "translation": [0.0, 1.5, 0.0],
     "extensions": {"KHR_lights_punctual": {"light": 1}}},
    {"name": "Beam", "translation": [0.0, 0.0, 0.5],
     "extensions": {"KHR_lights_punctual": {"light": 2}}}
  ],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2}]}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3"},
    {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
    {"bufferView": 2, "componentType": 5123, "count": 6, "type": "SCALAR"}
  ],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 48},
    {"buffer": 0, "byteOffset": 48, "byteLength": 48},
    {"buffer": 0, "byteOffset": 96, "byteLength": 12}
  ],
  "buffers": [{"byteLength": 108, "uri": "quad.bin"}]
})";

void WriteBuffer(const std::string &path) {
  const float vertices[] = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0,
                            0,  0,  1, 0, 0,  1, 0, 0, 1, 0,  0, 1};
  const uint16_t indices[] = {0, 1, 2, 0, 2, 3};
  std::FILE *file = std::fopen(path.c_str(), "wb");
  if (!file) { return; }
  std::fwrite(vertices, sizeof vertices, 1, file);
  std::fwrite(indices, sizeof indices, 1, file);
  std::fclose(file);
}

std::string Written(const std::string &directory, const std::string &name,
                    const std::string &text) {
  const std::string path = directory + "/" + name;
  std::FILE *file = std::fopen(path.c_str(), "wb");
  if (file) {
    std::fwrite(text.data(), 1, text.size(), file);
    std::fclose(file);
  }
  return path;
}

std::string Scratch() {
  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest != nullptr && *nest != 0) { return nest; }
  const char *env = std::getenv("TMPDIR");
  std::string directory = env && *env ? env : "/tmp";
  if (directory.back() == '/') { directory.pop_back(); }
  directory += "/outshine-" + std::to_string(getpid());
  std::filesystem::create_directories(directory);
  return directory;
}

}

int main() {
  using namespace outshine::Test;
  const std::string where = Scratch();
  WriteBuffer(where + "/quad.bin");
  const std::string path = Written(where, "lights.gltf", kSceneJson);

  Document file;
  const bool read = file.ReadFile(path);
  CHECK(read, "a file that REQUIRES KHR_lights_punctual now reads, where it used to be refused by "
              "name -- the honoured list is what changed and it changed in the round the behaviour "
              "was built");
  if (!read) {
    Note("the reader refused", 1, "documents");
    return Report();
  }
  CHECK(file.Lights().size() == 3, "the document-level light table crosses whole");
  CHECK(file.Lights()[0].Light.Kind == LightKind::Directional &&
            file.Lights()[1].Light.Kind == LightKind::Point &&
            file.Lights()[2].Light.Kind == LightKind::Spot,
        "the three types cross as an enumeration, which is what keeps lux and candela apart");
  CHECK(file.Lights()[1].Light.RangeM == 1.125f,
        "the declared range crosses into the engine's own light -- the asset that carries one "
        "depends on it to keep each panel to its own lamp");
  CHECK(file.Lights()[2].Light.InnerConeRad == 0.2f && file.Lights()[2].Light.OuterConeRad == 0.5f,
        "a spot's two cone angles cross as the file states them");

  Subject subject;
  const bool built = subject.Build(file);
  CHECK(built, "the scene flattens with its lights placed");
  if (!built) {
    Note("the subject refused", 1, "subjects");
    return Report();
  }
  CHECK(subject.HasNormal(), "NORMAL reaches the subject, which is what a cosine is measured "
                             "against");
  CHECK(subject.Lights().size() == 3, "every light the scene places arrives placed");

  const auto &lamp = subject.Lights()[1];
  CHECK(lamp.LightName == "Lamp" && lamp.NodeName == "Bulb",
        "a placed light names both the light it is and the node that placed it");
  Note("the lamp's placed height", (double)lamp.Light.Position[1], "m, glTF frame");
  CHECK(std::fabs(lamp.Light.Position[0] - 1.0f) < 1e-6f &&
            std::fabs(lamp.Light.Position[1] - 3.0f) < 1e-6f,
        "the parent's scale reaches the child's translation, so a light inside a scaled rig is "
        "where the rig puts it");

  const auto &sun = subject.Lights()[0];
  Note("the sun's beam, y", (double)sun.Light.Direction[1], "unit, glTF frame");
  CHECK(std::fabs(sun.Light.Direction[1] - 1.0f) < 1e-6f &&
            std::fabs(sun.Light.Direction[0]) < 1e-6f && std::fabs(sun.Light.Direction[2]) < 1e-6f,
        "the beam is the node's -Z turned by the node's rotation, normalised");

  Document broken;
  std::string inverted(kSceneJson);
  const size_t cone = inverted.find("\"innerConeAngle\": 0.2");
  inverted.replace(cone, std::string("\"innerConeAngle\": 0.2").size(),
                   "\"innerConeAngle\": 0.9");
  const std::string brokenPath = Written(where, "lights-inverted.gltf", inverted);
  CHECK(!broken.ReadFile(brokenPath),
        "a spot whose inner cone is wider than its outer one is refused rather than clamped");
  Note("the refusal", (double)broken.Error().size(), "characters, and it names the light");

  Document dangling;
  std::string past(kSceneJson);
  const size_t names = past.find("\"light\": 2");
  past.replace(names, std::string("\"light\": 2").size(), "\"light\": 9");
  const std::string danglingPath = Written(where, "lights-dangling.gltf", past);
  CHECK(!dangling.ReadFile(danglingPath),
        "a node naming a light index the file does not carry is refused by name");

  Covers("II.8 point and spot lights as a list the core lights from -- the reader half: "
         "KHR_lights_punctual crosses the glTF boundary as an enumeration with its own units, and "
         "the node hierarchy is what places it");
  return Report();
}
