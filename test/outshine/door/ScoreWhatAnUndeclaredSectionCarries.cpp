#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// The oracle is the meaning of the word DECLARATIVE, and it does not depend on our design: a
// section a scenario did not declare cannot decide anything, and a flag that says so must be
// read where the decision is made. Unreal reads its own: a UWorld with no PostProcessVolume
// does not silently inherit the volume struct's zeroed fields, it stands at the engine default.
// RAGE the same, through gameSkeleton's per-system init.
//
// The instrument is the picture itself. A scenario is assembled TWICE, identical in every field
// including the fields of the section under test, and differing only in that section's Declared
// flag. If the two pictures differ, the flag decides. If they are byte for byte the same, the
// door read a section nobody declared -- and the case says which section.
constexpr int kFramePx = 64;

constexpr const char *kTriangleBase64 =
    "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAA"
    "AAAIA/";

[[nodiscard]] std::string Minimal(void) {
  return std::string(
             "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
             "\"nodes\":[{\"mesh\":0}],"
             "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},"
             "\"material\":0}]}],"
             "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,0.8,1.0]}}],"
             "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":"
             "\"VEC3\","
             "\"min\":[0,0,0],\"max\":[1,1,0]},"
             "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
             "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
             "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
             "\"buffers\":[{\"byteLength\":72,\"uri\":\"data:application/octet-stream;base64,") +
         kTriangleBase64 + "\"}]}";
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

[[nodiscard]] std::vector<unsigned char> Slurped(const std::string &path) {
  std::vector<unsigned char> held;
  std::FILE *const file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) { return held; }
  unsigned char block[4096];
  for (size_t many = std::fread(block, 1, sizeof block, file); many > 0;
       many = std::fread(block, 1, sizeof block, file)) {
    held.insert(held.end(), block, block + many);
  }
  (void)std::fclose(file);
  return held;
}

[[nodiscard]] outshine::Scenario Filled(bool declaresRender) {
  outshine::Scenario made;
  made.Render.Declared = declaresRender;
  made.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  made.Render.Fill = 0.9;
  made.Lit.Declared = true;
  made.Lit.Key.Lux = 40000.0;
  made.Lit.Key.ElevationDeg = 42.0;
  made.Lit.Key.BearingDeg = 0.0;
  for (int at = 0; at < 3; ++at) { made.Lit.IndirectLight[at] = 0.20; }
  outshine::Asset shown;
  shown.Uri = "subject.gltf";
  shown.Kind = "gltf";
  made.Assets.push_back(shown);
  return made;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its subject into the runner's nest and was given none");
    return Report();
  }
  const std::string under = nest;
  if (!Wrote(under + "/subject.gltf", Minimal())) {
    Unprepared("the subject could not be written into the nest");
    return Report();
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be rendered");
    return Report();
  }

  outshine::Engine engine;
  engine.setRoots(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.drawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  const auto shot =
      [&](const outshine::Scenario &stands, const char *named, std::vector<unsigned char> &into) {
        if (!engine.declare(stands) || !engine.advance()) { return false; }
        const std::string path = under + "/" + named + ".png";
        if (!engine.renderer().saveScreenshot(path)) { return false; }
        into = Slurped(path);
        return !into.empty();
      };

  std::vector<unsigned char> declared, undeclared;
  if (!shot(Filled(true), "render-declared", declared) ||
      !shot(Filled(false), "render-undeclared", undeclared)) {
    Unprepared(("a picture did not come back: " + engine.error()).c_str());
    return Report();
  }

  std::printf("A 0.9 FILL, RENDER DECLARED:   %zu bytes\n", declared.size());
  std::printf("THE SAME 0.9 FILL, UNDECLARED: %zu bytes, %s\n",
              undeclared.size(),
              declared == undeclared ? "IDENTICAL -- the flag decides nothing"
                                     : "different, so the flag decides");

  CHECK(declared != undeclared,
        "**A RENDER SECTION THAT IS NOT DECLARED DOES NOT RENDER**: the same fill with "
        "Render.Declared left false makes a different picture, because an undeclared render "
        "leaves the engine's own default standing rather than reading a struct nobody filled in "
        "on purpose");

  CHECK(!undeclared.empty() && undeclared.size() > 32,
        "and an undeclared render still stands a PICTURE: the frame is the one the client handed "
        "in through DrawsInto and the picture is the whole of it, because a scenario that "
        "declares no render has not declared a smaller one either -- a zeroed Patch would draw "
        "nothing at all and that is the failure this default exists to refuse");

  Covers("the door: a section a scenario did not declare decides nothing, and what stands in its "
         "place is the engine's own default rather than the zeroes of an unfilled struct");
  return Report();
}
