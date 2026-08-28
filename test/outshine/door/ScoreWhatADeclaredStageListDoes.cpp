#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "Check.h"
#include <Outshine.h>
#include <Scenario.h>

// A DECLARED STAGE LIST DECIDES THE PLAN, OR IT IS PARSED FOR NOTHING.
//
// RAGE declares its render phases in the settings files the game ships; Unreal selects passes in
// C++ from ShowFlags and a scene never authors its pass list. This tree takes RAGE, because its
// own invariant already says so: scenarios declare and the engine behaves, and a section NOT
// declared decides nothing -- the engine's default stands in its place, never the zeroes of a
// struct nobody filled in.
//
// MEASURED BEFORE THE REPAIR: `ScenarioRead.cpp` filled `Render::Stages`, `ScenarioLayer.cpp`
// merged it correctly, and `grep -rn '\.Stages' src/` found no consumer at all. The plan came
// from `DeclarePlan` in a function body, out of four booleans. `Compiled::StageByName` -- the
// exact lookup a declared list needs -- stood complete in the tree with ZERO callers. That is
// this tree's commonest defect written down twice: a declaration surface with nothing behind it,
// and a capability no declaration reaches.
//
// This case is the consumer. It declares a plan the default would never build -- no overlay --
// and reads the batch count back, then declares a name the catalogue does not hold and requires
// a refusal that says the name. The second half is what makes the first mean something: a list
// that is accepted whatever it says is not being read either.

namespace {

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
      "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
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

[[nodiscard]] double Measured(const outshine::Engine &engine, const char *what) {
  for (const outshine::Measure &held : engine.measures()) {
    if (held.What == what) { return held.How; }
  }
  return -1.0;
}

[[nodiscard]] outshine::Scenario Naming(const std::vector<std::string> &stages) {
  outshine::Scenario out;
  out.Render.Declared = true;
  out.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  out.Render.Fill = 0.6;
  out.Render.Stages = stages;
  outshine::Asset shown;
  shown.Uri = "one.gltf";
  shown.Kind = "gltf";
  out.Assets.push_back(shown);
  return out;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so no plan can be compiled against a device");
    return Report();
  }

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its subject into the runner's nest and was given none");
    return Report();
  }
  const std::string under = nest;
  if (!Wrote(under + "/one.gltf", Minimal())) {
    Unprepared("the subject could not be written into the nest");
    return Report();
  }

  double drawn[2] = {-1.0, -1.0};
  double staged[2] = {-1.0, -1.0};
  for (int pass = 0; pass < 2; ++pass) {
    outshine::Engine engine;
    engine.setRoots(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
    if (!engine.drawsInto(outshine::Extent{kFramePx, kFramePx})) {
      Unprepared("the device stood no canvas");
      return Report();
    }
    const outshine::Scenario stands =
        pass == 0 ? Naming({}) : Naming({"subjects", "tonemap", "present"});
    if (!engine.declare(stands) || !engine.advance() || !engine.render(outshine::Extent{})) {
      Unprepared(("the declaration would not stand: " + engine.error()).c_str());
      return Report();
    }
    drawn[pass] = Measured(engine, "batches the picture draws");
    staged[pass] = Measured(engine, "stages the compiled plan runs");
  }

  std::printf("THE DEFAULT PLAN RUNS    %.0f stage(s) and draws %.0f batch(es)\n", staged[0],
              drawn[0]);
  std::printf("A DECLARED LIST RUNS     %.0f stage(s) and draws %.0f batch(es)\n", staged[1],
              drawn[1]);

  CHECK(drawn[0] > 0.0 && drawn[1] > 0.0,
        "both plans stand and draw something, so the comparison below is between two pictures "
        "rather than between a picture and a refusal");
  CHECK(staged[1] != staged[0],
        "**THE DECLARED LIST BUILT A PLAN THE DEFAULT WOULD NOT**: the default puts `overlay` in "
        "and this declaration does not, so the compiled stage count must differ. If the two "
        "agreed, the list would be accepted and discarded -- which is what it WAS, and a surface "
        "accepted whatever it says is one nobody reads");

  std::string refused;
  {
    outshine::Engine engine;
    engine.setRoots(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
    (void)engine.drawsInto(outshine::Extent{kFramePx, kFramePx});
    if (!engine.declare(Naming({"subjects", "terrain", "present"}))) { refused = engine.error(); }
  }
  std::printf("AND A NAME THE CATALOGUE DOES NOT HOLD IS REFUSED: %s\n",
              refused.empty() ? "NO" : refused.c_str());

  CHECK(!refused.empty() && refused.find("terrain") != std::string::npos,
        "**AN UNKNOWN STAGE NAME IS REFUSED BY NAME**: the catalogue is the vocabulary a "
        "declaration is checked against, and `Compiled::StageByName` is the check. A typo that "
        "silently drops a pass is a picture nobody can explain, and a list accepted whatever it "
        "says is a list nobody reads -- which is exactly what this surface was before this case");

  Covers("the door: a scenario's declared stage list reaches the plan, and a name the catalogue "
         "does not hold is refused with the name in the reason");
  return Report();
}
