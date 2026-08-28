#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

// A FILE MAY CARRY SEVERAL ANIMATIONS AND A DECLARATION SAYS WHICH ONE PLAYS.
//
// Unreal selects an animation ASSET by name into a slot; RAGE names a clip. Neither has a spelling
// for "all of them", and neither does glTF: within ONE animation a target must not be used twice,
// and two animations on one property is left undefined by the format. So a viewer plays one, and
// the engine's own default is the FIRST rather than the union.
//
// THE CASE THIS EXISTS FOR IS KHRONOS'S OWN FOX -- three cycles, Survey, Walk and Run, every one
// of them driving the same nodes. Reading `AssetAnimation::Play` as every animation the file
// carries made the poser refuse it by name, so a valid sample asset could not stand through the
// door at all while the corpus scored it green every run (its manifest declares `animations: [0]`
// and says in as many words that a viewer plays one).
//
// The fixture here is that shape in miniature and nothing else: ONE node, TWO animations, both
// driving its translation, one along +X and one along +Y. Two animations on one property is
// exactly what the format leaves undefined, so a reader that took the union would have to refuse
// this file -- and a reader that silently picked one would draw the same picture for clip 0 and
// clip 1. The pixels tell those two apart.

namespace {

constexpr int kFramePx = 64;

// 3 positions VEC3 then 3 normals VEC3, 72 bytes.
constexpr const char *kTriangleBase64 =
    "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/"
    "AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/";

// times [0, 1], then +X to 2 m, then +Y to 2 m. 56 bytes.
constexpr const char *kTracksBase64 =
    "AAAAAAAAgD8AAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAA=";

[[nodiscard]] std::string TwoCycles(void) {
  std::string held =
      std::string("{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
                  "\"nodes\":[{\"mesh\":0}],"
                  "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},"
                  "\"material\":0}]}],"
                  "\"materials\":[{\"pbrMetallicRoughness\":"
                  "{\"baseColorFactor\":[0.8,0.8,0.8,1.0]}}],"
                  "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
                  "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
                  "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
                  "{\"bufferView\":2,\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\","
                  "\"min\":[0],\"max\":[1]},"
                  "{\"bufferView\":3,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"},"
                  "{\"bufferView\":4,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"}],"
                  "\"animations\":["
                  "{\"name\":\"east\",\"samplers\":[{\"input\":2,\"output\":3,"
                  "\"interpolation\":\"LINEAR\"}],"
                  "\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,"
                  "\"path\":\"translation\"}}]},"
                  "{\"name\":\"up\",\"samplers\":[{\"input\":2,\"output\":4,"
                  "\"interpolation\":\"LINEAR\"}],"
                  "\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,"
                  "\"path\":\"translation\"}}]}],"
                  "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
                  "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36},"
                  "{\"buffer\":1,\"byteOffset\":0,\"byteLength\":8},"
                  "{\"buffer\":1,\"byteOffset\":8,\"byteLength\":24},"
                  "{\"buffer\":1,\"byteOffset\":32,\"byteLength\":24}],"
                  "\"buffers\":[{\"byteLength\":72,\"uri\":"
                  "\"data:application/octet-stream;base64,");
  held += kTriangleBase64;
  held += "\"},{\"byteLength\":56,\"uri\":\"data:application/octet-stream;base64,";
  held += kTracksBase64;
  held += "\"}]}";
  return held;
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  SDL_IOStream *out = SDL_IOFromFile(path.c_str(), "wb");
  if (out == nullptr) { return false; }
  const bool wrote = SDL_WriteIO(out, held.data(), held.size()) == held.size();
  return SDL_CloseIO(out) && wrote;
}

[[nodiscard]] outshine::Scenario Playing(const char *uri, int clip, bool name) {
  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Render.Fill = 0.6;
  stands.Render.Fps = 2.0;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  outshine::Asset shown;
  shown.Uri = uri;
  shown.Kind = "gltf";
  if (name) { shown.Clip = clip; }
  stands.Assets.push_back(shown);

  outshine::View watches;
  watches.Id = "station";
  watches.Placed = true;
  watches.Person = "first";
  watches.Stands.AtM[0] = 1.0;
  watches.Stands.AtM[1] = 1.0;
  watches.Stands.AtM[2] = 6.0;
  watches.FovDeg = 60.0;
  stands.Views.push_back(watches);
  return stands;
}

[[nodiscard]] bool Drew(const std::string &under, const char *uri, int clip, bool name, int steps,
                        std::vector<uint8_t> &rgba, std::string &why) {
  outshine::Engine engine;
  engine.Under(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.DrawsInto(outshine::Extent{kFramePx, kFramePx})) {
    why = "the device stood no canvas";
    return false;
  }
  if (!engine.Declare(Playing(uri, clip, name)) || !engine.Assemble()) {
    why = engine.Error();
    return false;
  }
  for (int step = 0; step < steps; ++step) {
    if (!engine.Advance() || !engine.RenderTo(outshine::Extent{})) {
      why = engine.Error();
      return false;
    }
  }
  if (!engine.Pixels(rgba)) {
    why = engine.Error();
    return false;
  }
  return true;
}

[[nodiscard]] size_t Differing(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b) {
  if (a.size() != b.size() || a.empty()) { return 0; }
  size_t apart = 0;
  for (size_t at = 0; at + 3 < a.size(); at += 4) {
    if (a[at] != b[at] || a[at + 1] != b[at + 1] || a[at + 2] != b[at + 2]) { ++apart; }
  }
  return apart;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its subject into the runner's nest and was given none");
    return Report();
  }
  const std::string under = nest;
  const std::string uri = under + "/two-cycles.gltf";
  if (!Wrote(uri, TwoCycles())) {
    Unprepared("the subject could not be written into the nest");
    return Report();
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be drawn");
    return Report();
  }

  std::string why;
  std::vector<uint8_t> byDefault, first, second, again;
  const bool stood = Drew(under, uri.c_str(), 0, false, 6, byDefault, why) &&
                     Drew(under, uri.c_str(), 0, true, 6, first, why) &&
                     Drew(under, uri.c_str(), 1, true, 6, second, why) &&
                     Drew(under, uri.c_str(), 0, true, 6, again, why);
  if (!stood) { std::printf("a declaration did not stand: %s\n", why.c_str()); }

  std::vector<uint8_t> past;
  std::string pastWhy;
  const bool refusedPastTheEnd = !Drew(under, uri.c_str(), 2, true, 2, past, pastWhy);

  CHECK(stood,
        "**A FILE CARRYING TWO ANIMATIONS ON ONE PROPERTY STANDS**: glTF forbids a repeated target "
        "within ONE animation and says nothing about two animations driving the same node, so a "
        "reader that takes the union has to refuse a file the format allows -- which is what "
        "Khronos's own Fox, three cycles over the same nodes, was refused for");

  CHECK(Differing(first, second) > 0,
        "**THE DECLARATION CHOOSES, AND THE PICTURE SHOWS IT**: the two cycles translate the same "
        "node along different axes, so a reader that ignored the choice would draw the same frame "
        "twice. Unreal selects an animation asset into a slot and RAGE names a clip; a door that "
        "cannot name one leaves the engine to guess");

  CHECK(Differing(first, again) == 0 && Differing(byDefault, first) == 0,
        "**THE ENGINE'S OWN DEFAULT IS THE FIRST ANIMATION**: an asset that names no clip draws "
        "what clip 0 draws, and naming clip 0 twice draws it twice -- so the default is a stated "
        "answer rather than an accident, which is what TARGET means by the engine's default "
        "standing where a declaration is silent");

  CHECK(refusedPastTheEnd && pastWhy.find("2 of 2") != std::string::npos,
        "**A CLIP THE FILE DOES NOT CARRY IS REFUSED, WITH THE COUNT IT DOES**: accepting a "
        "declaration and doing nothing with it is worse than refusing it, and a refusal that "
        "does not say how many there are leaves the caller to guess which number is legal");

  Covers("the door: a declared asset names WHICH of a file's animations plays, the engine's own "
         "default is the first, and a clip the file does not carry is refused with its count");
  return Report();
}
