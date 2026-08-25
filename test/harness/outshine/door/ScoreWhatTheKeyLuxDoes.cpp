#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// The oracle is the exposure algebra, and it is checked against PIXELS rather than believed.
//
// Live compiles an exposure from the declared key illuminance:
//
//   ev100    = log2(KeyLux / 2.5)
//   Exposure = 1 / (1.2 * 2^ev100) = 2.5 / (1.2 * KeyLux)
//
// and hands the declared environment through as an absolute radiance, unscaled. So what reaches
// the tonemapper is
//
//   from the key          KeyLux * Exposure       = 2.5 / 1.2, independent of KeyLux
//   from the environment  Environment * Exposure  = 2.5 * Environment / (1.2 * KeyLux)
//
// Two consequences follow and both are testable at the pixel:
//
//   scaling KeyLux and Environment TOGETHER changes nothing at all
//   scaling KeyLux alone changes only the fill, and DARKENS it
//
// which is to say KeyLux is not a brightness. It is the reciprocal of the fill ratio, and a
// declaration that carries both numbers carries one truth twice.
constexpr int kFramePx = 64;

constexpr const char *kTriangleBase64 =
    "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAA"
    "AAAIA/";

[[nodiscard]] std::string Minimal(void) {
  return std::string(
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
      "\"nodes\":[{\"mesh\":0}],"
      "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},\"material\":0}]}],"
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

[[nodiscard]] outshine::Scenario Lit(double keyLux, double environment) {
  outshine::Scenario made;
  made.Render.Declared = true;
  made.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  made.Render.Fill = 0.6;
  made.Lit.Declared = true;
  made.Lit.Key.Lux = keyLux;
  made.Lit.Key.ElevationDeg = 42.0;
  made.Lit.Key.BearingDeg = 150.0;
  for (int at = 0; at < 3; ++at) { made.Lit.Environment[at] = environment; }
  outshine::Asset shown;
  shown.Uri = "subject.gltf";
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
  engine.Under(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.DrawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  const auto shot = [&](const outshine::Scenario &stands, const char *named,
                        std::vector<unsigned char> &into) {
    if (!engine.Declare(stands) || !engine.Advance()) { return false; }
    const std::string path = under + "/" + named + ".png";
    if (!engine.Capture(path)) { return false; }
    into = Slurped(path);
    return !into.empty();
  };

  std::vector<unsigned char> plain, scaled, keyed, filled, nightly;
  outshine::Scenario night = Lit(40000.0, 0.20);
  night.Lit.Key.ElevationDeg = -80.0;
  outshine::Scenario other = Lit(40000.0, 0.20);
  other.Lit.Declared = false;
  if (!shot(other, "filled", filled) ||
      !shot(Lit(40000.0, 0.20), "plain", plain) ||
      !shot(Lit(80000.0, 0.40), "scaled", scaled) ||
      !shot(Lit(400.0, 0.20), "keyed", keyed) ||
      !shot(night, "night", nightly)) {
    Unprepared(("a picture did not come back: " + engine.Error()).c_str());
    return Report();
  }

  std::printf("THE SAME SUBJECT WITH NO LIGHTING DECLARED: %zu bytes, %s\n", filled.size(),
              filled == plain ? "IDENTICAL to the lit one -- the declaration changes nothing"
                              : "different, so a declared light does reach it");
  std::printf("40000 lux with 0.20 fill: %zu bytes\n", plain.size());
  std::printf("80000 lux with 0.40 fill: %zu bytes\n", scaled.size());
  std::printf("  400 lux with 0.20 fill: %zu bytes\n", keyed.size());
  std::printf("THE KEY 80 DEG BELOW THE HORIZON: %zu bytes, %s\n", nightly.size(),
              nightly == plain ? "IDENTICAL -- no light reaches this subject at all"
                               : "different, so the key does light it");

  CHECK(plain == scaled,
        "**THE KEY ILLUMINANCE IS NOT A BRIGHTNESS**: doubling the declared key AND the declared "
        "environment together changes not one pixel, because the exposure the key compiles to is "
        "its own reciprocal -- the picture depends on the RATIO alone, and a declaration that "
        "carries both numbers carries one truth twice");
  CHECK(plain != keyed,
        "and the control is a control: dropping the key a hundredfold DOES change the picture, "
        "so this case can tell an invariance from a renderer that ignores its lighting entirely");

  {
    outshine::Scenario bare = Lit(40000.0, 0.20);
    bare.Assets.front().Uri = "bare.gltf";
    std::string flat = Minimal();
    const size_t at = flat.find(",\"NORMAL\":1");
    if (at != std::string::npos) { flat.erase(at, 11); }
    (void)Wrote(under + "/bare.gltf", flat);
    const bool stoodBare = engine.Declare(bare);
    std::printf("A SUBJECT WITH NO NORMAL under the same declared light: %s%s\n",
                stoodBare ? "STOOD" : "REFUSED -- ", stoodBare ? "" : engine.Error().c_str());
  }

  CHECK(plain != nightly,
        "**THE KEY LIGHTS THE SUBJECT FROM WHERE IT IS DECLARED**: a key 80 degrees BELOW the "
        "horizon and a key 42 degrees above it cannot make the same picture of the same subject. "
        "The exposure the key compiles to does reach the frame -- a hundredfold drop moves the "
        "pixels -- but its DIRECTION does not, so the subject is lit by something that has no "
        "direction at all and no surface of it can turn toward or away from the light");

  Covers("the door: the declared key illuminance sets the fill ratio and nothing else, so the "
         "picture is invariant under a common scale of key and environment");
  return Report();
}
