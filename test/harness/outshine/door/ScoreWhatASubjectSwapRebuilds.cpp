#include <cstdio>
#include <cstdlib>
#include <string>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"
#include "Live.h"

namespace {

// The oracle is the same cost bound the sibling case states, applied to the case that actually
// costs something: a subject that CHANGES. Unreal swaps a streaming level into a persistent
// world without rebuilding the renderer behind it; RAGE swaps an IMAP group against a map data
// store the same way. Neither throws away its pipelines to show a different model.
//
// So: swapping the subject must read the NEW subject -- there is no way around that, the bytes
// are different -- and must initialise NO further render plan, because the plan describes the
// passes and resources of the picture and the picture did not change. Two instruments, and the
// case needs both: AssetReads() must rise by exactly one per swap, or nothing was swapped and
// the test is measuring nothing; PlanInits() must not rise at all.
constexpr int kFramePx = 64;

// A triangle, three vec3 of float32, base64 of the 36 bytes: (0,0,0) (1,0,0) (0,1,0).
constexpr const char *kTriangleBase64 =
    "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA";

[[nodiscard]] std::string Minimal(double red) {
  return std::string(
             "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
             "\"nodes\":[{\"mesh\":0}],"
             "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0}]}],"
             "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[") +
         std::to_string(red) +
         ",0.5,0.5,1.0]}}],"
         "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
         "\"min\":[0,0,0],\"max\":[1,1,0]}],"
         "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
         "\"buffers\":[{\"byteLength\":36,\"uri\":\"data:application/octet-stream;base64," +
         std::string(kTriangleBase64) + "\"}]}";
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

[[nodiscard]] outshine::Scenario Showing(const std::string &uri) {
  outshine::Scenario made;
  made.Render.Declared = true;
  made.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  made.Lit.Declared = true;
  made.Lit.Key.Lux = 40000.0;
  made.Lit.Key.ElevationDeg = 42.0;
  made.Lit.Key.BearingDeg = 150.0;
  outshine::Asset shown;
  shown.Uri = uri;
  shown.Kind = "gltf";
  made.Assets.push_back(shown);
  return made;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its two subjects into the runner's nest and was given none");
    return Report();
  }
  const std::string under = nest;
  if (!Wrote(under + "/red.gltf", Minimal(0.9)) ||
      !Wrote(under + "/blue.gltf", Minimal(0.1))) {
    Unprepared("the two subjects could not be written into the nest");
    return Report();
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so no stand can be judged");
    return Report();
  }

  outshine::Engine engine;
  engine.Under(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.DrawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  const size_t readsBefore = outshine::Clients::Live::AssetReads();
  const size_t plansBefore = outshine::Clients::Live::PlanInits();
  if (!engine.Declare(Showing("red.gltf"))) {
    Unprepared(("the first subject did not stand: " + engine.Error()).c_str());
    return Report();
  }
  const size_t readsFirst = outshine::Clients::Live::AssetReads();
  const size_t plansFirst = outshine::Clients::Live::PlanInits();
  std::printf("FIRST SUBJECT read %zu asset(s), initialised %zu plan(s)\n",
              readsFirst - readsBefore, plansFirst - plansBefore);
  CHECK(readsFirst == readsBefore + 1, "the first subject is read exactly once");
  CHECK(plansFirst == plansBefore + 1, "and it builds the one plan the picture needs");

  CHECK(engine.Declare(Showing("blue.gltf")), "a different subject in the same picture stands");
  const size_t readsSwapped = outshine::Clients::Live::AssetReads();
  const size_t plansSwapped = outshine::Clients::Live::PlanInits();
  std::printf("SWAPPED SUBJECT read %zu further asset(s), initialised %zu further plan(s)\n",
              readsSwapped - readsFirst, plansSwapped - plansFirst);
  CHECK(readsSwapped == readsFirst + 1,
        "the swapped subject IS read -- different bytes, and a swap that read nothing would "
        "mean this case measures nothing");
  CHECK(plansSwapped == plansFirst,
        "**SWAPPING THE SUBJECT REBUILDS NO PLAN**: the plan describes the passes and resources "
        "of the picture, and the picture did not change -- an engine that re-initialises its "
        "device to show a different model cannot stream a world, because every part entering "
        "would cost every pipeline behind it");

  CHECK(engine.Declare(Showing("red.gltf")), "and back again");
  const size_t plansBack = outshine::Clients::Live::PlanInits();
  std::printf("SWAPPED BACK initialised %zu further plan(s)\n", plansBack - plansSwapped);
  CHECK(plansBack == plansSwapped, "and swapping back rebuilds nothing either");

  Note("plans initialised over three subjects in one picture",
       (double)(plansBack - plansBefore), "inits");
  Note("assets read over the same three", (double)(readsSwapped + 1 - readsBefore), "reads");

  Covers("the door: a subject that changes costs the reading of that subject and nothing else, "
         "so a scenario streams its parts rather than being rebuilt around them");
  return Report();
}
