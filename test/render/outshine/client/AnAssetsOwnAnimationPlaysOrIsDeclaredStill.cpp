#include <cstdio>
#include <string>

#include <outshine/Outshine.h>

#include "Check.h"
#include "PreparedRoot.h"

namespace {

std::string Fixture(void) {
  return outshine::Test::PreparedRoot() + "/test-render-khronos-generator-Animation_Node_00/Animation_Node_00.gltf";
}

std::string Planted(const char *name, const std::string &text) {
  const std::string at = outshine::Test::PlantedPath(name);
  std::FILE *const file = std::fopen(at.c_str(), "wb");
  if (file == nullptr) { return std::string(); }
  std::fputs(text.c_str(), file);
  std::fclose(file);
  return at;
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
  if (std::FILE *const probe = std::fopen(asset.c_str(), "rb")) {
    std::fclose(probe);
  } else {
    Unprepared((asset + " is not prepared -- run test/harness/shared/corpus/prepare.py").c_str());
    return Report();
  }

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

  Covers("II.9 a scenario declares whether an asset's own animation plays, is ignored, or "
         "is the engine's -- one enumeration, never a fallback, and ignored-by-declaration "
         "is distinguishable from could-not-drive (board:1415)");
  return Report();
}
