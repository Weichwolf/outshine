#include <cstdio>
#include <cstdlib>
#include <string>

#include <Event.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// The oracle is what a shadow IS, and it does not depend on our design: a shadow is the absence
// of light behind an occluder, so every piece of geometry the picture draws must also be drawn
// into the depth the light sees. A subject whose batches reach the colour pass but not the
// shadow pass casts a PARTIAL shadow -- a car with a transparent bonnet, lit through a body it
// does not have. Both benchmarks state the same requirement by construction: the shadow pass
// walks the same draw list as the colour pass.
//
// So: batches cast == batches drawn, and both are more than none.
//
// The declaration does not have to carry a shadow radius for this to hold. glTF gives a subject
// an extent, and the smallest sphere that holds it is derivable, so an undeclared shadow leaves
// the engine's own default standing rather than the zero of a struct nobody filled in -- which
// is what stood here until this case was written, and why the shadow cast NOTHING.
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
  for (const outshine::Measure &held : engine.Numbers()) {
    if (held.What == what) { return held.How; }
  }
  return -1.0;
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
  if (!Wrote(under + "/caster.gltf", Minimal())) {
    Unprepared("the subject could not be written into the nest");
    return Report();
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be rendered");
    return Report();
  }

  outshine::Engine engine;
  engine.Under(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.DrawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Render.Fill = 0.6;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  stands.Lit.Key.BearingDeg = 0.0;
  outshine::Asset shown;
  shown.Uri = "caster.gltf";
  shown.Kind = "gltf";
  stands.Assets.push_back(shown);

  if (!engine.Declare(stands) || !engine.Advance()) {
    Unprepared(("the caster did not stand: " + engine.Error()).c_str());
    return Report();
  }

  const double drawn = Measured(engine, "batches the picture draws");
  const double cast = Measured(engine, "batches the shadow casts");
  std::printf("THE PICTURE DRAWS %.0f batch(es)\n", drawn);
  std::printf("THE SHADOW CASTS  %.0f batch(es), with NO shadow radius declared\n", cast);

  CHECK(drawn > 0.0,
        "the picture draws something, so there is an occluder for the light to be stopped by");
  CHECK(cast == drawn,
        "**THE SHADOW CASTS EVERY BATCH THE PICTURE DRAWS**: a shadow is the absence of light "
        "behind an occluder, so geometry that reaches the colour pass and not the depth the "
        "light sees casts a PARTIAL shadow -- a body lit straight through itself");
  CHECK(cast > 0.0,
        "and the control is a control: it casts more than NONE, which is what a declaration "
        "carrying no shadow radius produced until an undeclared radius began deriving itself "
        "from the subject's own extent rather than standing at a struct's zero");

  Covers("the door: a subject casts a shadow over every batch it draws, and a declaration that "
         "names no shadow radius gets one derived from the subject's extent rather than none");
  return Report();
}
