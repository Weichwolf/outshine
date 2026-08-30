#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

// THE VISIBILITY STRUCTURE REFITS ON THE FRAME PATH AND TAKES NOTHING WHILE IT DOES.
//
// Unreal refits a skeletal mesh's bounds and its physics BVH per frame and rebuilds only on a
// topology change; RAGE keeps a bound hierarchy per model and updates it in place. Both agree, and
// the reason is the one CLAUDE.md states as an invariant: the frame path carries BOUNDED terms and
// no allocation. A rebuild is O(triangles) with a fresh allocation; a refit is O(nodes) into
// storage that already stands.
//
// THE MEASUREMENT IS THE POINT AND IT DID NOT EXIST. `Heap::Tagged` has counted bytes per named
// tag all along and nothing outside the library could read the count, so "the frame path allocates
// nothing" was a sentence rather than a number. The engine now publishes every tag that has taken
// anything as `heap taken under <tag>`, cumulative, and the CONSUMER takes the difference across a
// frame -- the engine aggregates and does not compute statistics.
//
// THE SUBJECT MUST MOVE OR THE CASE MEASURES NOTHING. A still one never reaches `SetPose` and so
// never reaches the refit, and eight frames of zero would then prove only that nothing ran. This
// one carries a translation track, which this engine bakes into VERTICES every frame -- so the
// pose path runs, the refit runs with it, and the zero is about the refit rather than about
// silence. The build before it is counted for the same reason: a tag nothing ever wrote also
// reads zero.

namespace {

constexpr int kFramePx = 96;

// 3 positions VEC3 then 3 normals VEC3, 72 bytes.
constexpr const char *kTriangleBase64 =
    "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/"
    "AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/";

// times [0, 1] then a translation to 2 m east, 32 bytes.
constexpr const char *kTrackBase64 = "AAAAAAAAgD8AAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA=";

[[nodiscard]] std::string Walking(void) {
  return std::string("{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
                     "\"nodes\":[{\"mesh\":0}],"
                     "\"meshes\":[{\"primitives\":[{\"attributes\":"
                     "{\"POSITION\":0,\"NORMAL\":1},\"material\":0}]}],"
                     "\"materials\":[{\"pbrMetallicRoughness\":"
                     "{\"baseColorFactor\":[0.8,0.8,0.8,1.0]}}],"
                     "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
                     "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
                     "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
                     "{\"bufferView\":2,\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\","
                     "\"min\":[0],\"max\":[1]},"
                     "{\"bufferView\":3,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"}],"
                     "\"animations\":[{\"samplers\":[{\"input\":2,\"output\":3,"
                     "\"interpolation\":\"LINEAR\"}],"
                     "\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,"
                     "\"path\":\"translation\"}}]}],"
                     "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
                     "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36},"
                     "{\"buffer\":1,\"byteOffset\":0,\"byteLength\":8},"
                     "{\"buffer\":1,\"byteOffset\":8,\"byteLength\":24}],"
                     "\"buffers\":[{\"byteLength\":72,\"uri\":"
                     "\"data:application/octet-stream;base64,") +
         kTriangleBase64 +
         "\"},{\"byteLength\":32,\"uri\":" + "\"data:application/octet-stream;base64," +
         kTrackBase64 + "\"}]}";
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  SDL_IOStream *out = SDL_IOFromFile(path.c_str(), "wb");
  if (out == nullptr) { return false; }
  const bool wrote = SDL_WriteIO(out, held.data(), held.size()) == held.size();
  return SDL_CloseIO(out) && wrote;
}

[[nodiscard]] double Taken(const outshine::Engine &engine, const char *tag) {
  const std::string named = std::string("heap taken under ") + tag;
  for (const outshine::Measure &held : engine.measures()) {
    if (held.What == named) { return held.How; }
  }
  return 0.0;
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
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be drawn");
    return Report();
  }

  const std::string under = nest;
  if (!Wrote(under + "/walks.gltf", Walking())) {
    Unprepared("the subject could not be written into the nest");
    return Report();
  }
  outshine::Engine engine;
  engine.setRoots(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.drawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Render.Fill = 0.6;
  stands.Render.Fps = 24.0;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  outshine::Asset shown;
  shown.Uri = "walks.gltf";
  shown.Kind = "gltf";
  stands.Assets.push_back(shown);
  if (!engine.declare(stands) || !engine.assemble()) {
    Unprepared((std::string("the subject did not stand: ") + engine.error()).c_str());
    return Report();
  }

  // Four steps to get past the first pose, which BUILDS and is allowed to take.
  for (int step = 0; step < 4; ++step) {
    if (!engine.advance() || !engine.renderer().render(outshine::Extent{})) {
      Unprepared((std::string("the picture did not advance: ") + engine.error()).c_str());
      return Report();
    }
  }
  const double bvhBefore = Taken(engine, "mesh-bvh");
  const double allBefore = Taken(engine, "render-frame") + bvhBefore;

  double moved = 0.0;
  for (int step = 0; step < 8; ++step) {
    if (!engine.advance() || !engine.renderer().render(outshine::Extent{})) {
      Unprepared((std::string("the picture did not advance: ") + engine.error()).c_str());
      return Report();
    }
    moved += 1.0;
  }
  const double bvhAfter = Taken(engine, "mesh-bvh");
  const double allAfter = Taken(engine, "render-frame") + bvhAfter;

  std::printf("OVER %.0f POSED FRAMES  mesh-bvh took %.0f byte(s), the frame took %.0f\n",
              moved,
              bvhAfter - bvhBefore,
              allAfter - allBefore);

  CHECK(bvhBefore > 0.0,
        "**THE STRUCTURE IS BUILT AT ALL**: this case reads a DIFFERENCE, and a difference of zero "
        "over a tag nothing ever wrote is what a case measuring nothing looks like. The build is "
        "allowed to take -- it happens once, off the frame path -- and its count is what proves "
        "the tag is the right one to watch");

  CHECK(bvhAfter - bvhBefore == 0.0,
        "**AND IT REFITS RATHER THAN REBUILDS**: eight posed frames of a skinned subject take ZERO "
        "bytes under mesh-bvh. Unreal refits per frame and rebuilds only on a topology change; "
        "RAGE updates its bound hierarchy in place. A rebuild here would be O(triangles) with a "
        "fresh allocation every frame, which is the unbounded term CLAUDE.md's frame-path "
        "invariant names by hand");

  Covers("the frame path: a posed subject's visibility structure REFITS in place and takes no "
         "heap under `mesh-bvh` over eight frames, measured through the door's own tag counters");
  return Report();
}
