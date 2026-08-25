#include <cstdio>
#include <string>

#include <outshine/Outshine.h>

#include "Check.h"
#include "PreparedRoot.h"

namespace {

std::string Planted(const char *name, const std::string &text) {
  const std::string at = outshine::Test::PlantedPath(name);
  std::FILE *const file = std::fopen(at.c_str(), "wb");
  if (file == nullptr) { return std::string(); }
  std::fputs(text.c_str(), file);
  std::fclose(file);
  return at;
}

// the fixture is OURS, planted in the nest -- a corpus case's directory belongs to the
// prune, which reclaims sources its own oracle can regenerate
std::string Fixture(void) {
  const std::string bin = outshine::Test::PlantedPath("anim-fixture.bin");
  {
    std::FILE *const file = std::fopen(bin.c_str(), "wb");
    if (file == nullptr) { return std::string(); }
    const float positions[9] = {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    const uint16_t indices[4] = {0, 1, 2, 0};
    const float times[2] = {0.f, 1.f};
    const float rotations[8] = {0.f, 0.f, 0.f, 1.f, 0.f, 0.7071068f, 0.f, 0.7071068f};
    std::fwrite(positions, 1, sizeof positions, file);
    std::fwrite(indices, 1, sizeof indices, file);
    std::fwrite(times, 1, sizeof times, file);
    std::fwrite(rotations, 1, sizeof rotations, file);
    std::fclose(file);
  }
  const std::string doc = std::string("{\"asset\":{\"version\":\"2.0\"},") +
      "\"buffers\":[{\"uri\":\"anim-fixture.bin\",\"byteLength\":84}]," +
      "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}," +
      "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}," +
      "{\"buffer\":0,\"byteOffset\":44,\"byteLength\":8}," +
      "{\"buffer\":0,\"byteOffset\":52,\"byteLength\":32}]," +
      "\"accessors\":[" +
      "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"," +
      "\"min\":[0,0,0],\"max\":[1,1,0]}," +
      "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}," +
      "{\"bufferView\":2,\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\"," +
      "\"min\":[0],\"max\":[1]}," +
      "{\"bufferView\":3,\"componentType\":5126,\"count\":2,\"type\":\"VEC4\"}]," +
      "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}]," +
      "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0," +
      "\"animations\":[{\"channels\":[{\"sampler\":0," +
      "\"target\":{\"node\":0,\"path\":\"rotation\"}}]," +
      "\"samplers\":[{\"input\":2,\"output\":3,\"interpolation\":\"LINEAR\"}]}]}";
  return Planted("anim-fixture.gltf", doc);
}



std::string Declared(const std::string &asset, const char *animation) {
  return "<scenario name=\"still or moving\">"
         "<render widthPx=\"64\" heightPx=\"36\" fps=\"24\"/>"
         "<assets><asset uri=\"" + asset + "\" kind=\"gltf\" animation=\"" +
         std::string(animation) + "\"/></assets></scenario>";
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string asset = Fixture();
  CHECK(!asset.empty(), "the animated fixture plants itself in the nest -- no corpus "
                        "directory borrowed, nothing for the prune to reclaim");

  {
    outshine::Engine plays;
    const std::string path = Planted("anim-play.scenario", Declared(asset, "play"));
    const bool stood = plays.Load(path);
    if (!stood) { std::printf("REFUSED %s\n", plays.Error().c_str()); }
    Note("frames the play declaration stands", (double)plays.Frames(), "frames");
    CHECK(stood && plays.Frames() > 1,
          "**PLAY IT**: the file's own animation drives the subject -- the sequence has the "
          "clip's frames, which is what an animated prop ships with");
  }
  {
    outshine::Engine still;
    const std::string path = Planted("anim-ignore.scenario", Declared(asset, "ignore"));
    const bool stood = still.Load(path);
    if (!stood) { std::printf("REFUSED %s\n", still.Error().c_str()); }
    CHECK(stood && still.Frames() == 1,
          "**IGNORE IT**: the same asset stands in its rest pose because the DECLARATION "
          "asked for a still -- never because the engine fell back to one");
    bool spoken = false;
    for (const std::string &row : still.Carried()) {
      if (row.find("IGNORED by declaration") != std::string::npos) { spoken = true; }
    }
    CHECK(spoken, "and the capability SAYS so -- 'ignored by declaration' is a different "
                  "answer from 'could not drive', and a consumer can tell them apart");
  }
  {
    outshine::Engine driven;
    const std::string path = Planted("anim-driven.scenario", Declared(asset, "driven"));
    CHECK(driven.Load(path) && driven.Frames() == 1,
          "**TAKE IT OVER**: driven means the engine's pose supplies the motion -- the "
          "file's clips wait, and the declared answer is on Carried");
  }
  {
    outshine::Engine wrong;
    const std::string path = Planted("anim-wrong.scenario", Declared(asset, "bounce"));
    CHECK(!wrong.Load(path) && wrong.Error().find("bounce") != std::string::npos,
          "a fourth answer does not exist, and the refusal names the three that do");
  }

  Covers("II.21 a scenario declares whether an asset's own animation plays, is ignored, or "
         "is the engine's -- one enumeration, never a fallback, and ignored-by-declaration "
         "is distinguishable from could-not-drive (board:1415)");
  return Report();
}
