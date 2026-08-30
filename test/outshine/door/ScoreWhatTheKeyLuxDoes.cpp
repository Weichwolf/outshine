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
//   from the environment  IndirectLight * Exposure  = 2.5 * IndirectLight / (1.2 * KeyLux)
//
// Two consequences follow and both are testable at the pixel:
//
//   scaling KeyLux and IndirectLight TOGETHER changes nothing at all
//   scaling KeyLux alone changes only the fill, and DARKENS it
//
// which is to say KeyLux is not a brightness. It is the reciprocal of the fill ratio, and a
// declaration that carries both numbers carries one truth twice.
//
// The subject is one triangle whose normal is +Z and the key bears 0 degrees, because that is
// the only arrangement in which the two elevations this case compares are BOTH lit and lit
// differently:
//
//   N.L = cos(elevation) * cos(bearing)
//
//   bearing 150 deg:  +42 deg -> -0.644   -80 deg -> -0.150   both facing AWAY, both unlit
//   bearing   0 deg:  +42 deg -> +0.743   -80 deg -> +0.174   both lit, four times apart
//
// A case that compares two unlit pictures of the same subject compares nothing, and this one
// did until the cosine was worked out rather than assumed.
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

[[nodiscard]] outshine::Scenario Lit(double keyLux, double environment) {
  outshine::Scenario made;
  made.Render.Declared = true;
  made.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  made.Render.Fill = 0.6;
  made.Lit.Declared = true;
  made.Lit.Key.Lux = keyLux;
  made.Lit.Key.ElevationDeg = 42.0;
  made.Lit.Key.BearingDeg = 0.0;
  for (int at = 0; at < 3; ++at) { made.Lit.IndirectLight[at] = environment; }
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

  std::vector<unsigned char> plain, scaled, keyed, filled, nightly;
  outshine::Scenario night = Lit(40000.0, 0.20);
  night.Lit.Key.ElevationDeg = -80.0;
  outshine::Scenario other = Lit(40000.0, 0.20);
  other.Lit.Declared = false;
  other.Lit.Key.ElevationDeg = -80.0;
  if (!shot(other, "filled", filled) || !shot(Lit(40000.0, 0.20), "plain", plain) ||
      !shot(Lit(80000.0, 0.40), "scaled", scaled) || !shot(Lit(400.0, 0.20), "keyed", keyed) ||
      !shot(night, "night", nightly)) {
    Unprepared(("a picture did not come back: " + engine.error()).c_str());
    return Report();
  }

  std::printf("A KEY MOVED TO -80 DEG WITH Lit.Declared LEFT FALSE: %zu bytes, %s\n",
              filled.size(),
              filled == nightly ? "IDENTICAL to the same key DECLARED -- the flag is ignored"
                                : "different, so the flag is read");
  std::printf("40000 lux with 0.20 fill: %zu bytes\n", plain.size());
  std::printf("80000 lux with 0.40 fill: %zu bytes\n", scaled.size());
  std::printf("  400 lux with 0.20 fill: %zu bytes\n", keyed.size());
  std::printf("THE KEY 80 DEG BELOW THE HORIZON: %zu bytes, %s\n",
              nightly.size(),
              nightly == plain ? "IDENTICAL -- no light reaches this subject at all"
                               : "different, so the key does light it");

  CHECK(plain == scaled,
        "**THE KEY ILLUMINANCE IS NOT A BRIGHTNESS**: doubling the declared key AND the declared "
        "environment together changes not one pixel, because the exposure the key compiles to is "
        "its own reciprocal -- the picture depends on the RATIO alone, and a declaration that "
        "carries both numbers carries one truth twice");
  CHECK(plain != keyed,
        "and the control is a control: dropping the key a HUNDREDFOLD alone DOES change the "
        "picture, by the same arithmetic -- the key's own contribution is fixed but the "
        "environment beside it is divided by the key, so a hundredfold smaller key is a "
        "hundredfold brighter fill");

  {
    outshine::Scenario bare = Lit(40000.0, 0.20);
    bare.Assets.front().Uri = "bare.gltf";
    std::string flat = Minimal();
    const size_t at = flat.find(",\"NORMAL\":1");
    if (at != std::string::npos) { flat.erase(at, 11); }
    (void)Wrote(under + "/bare.gltf", flat);
    const bool stoodBare = engine.declare(bare).has_value();
    std::printf("A SUBJECT WITH NO NORMAL under the same declared light: %s%s\n",
                stoodBare ? "STOOD" : "REFUSED -- ",
                stoodBare ? "" : engine.error().c_str());
  }

  CHECK(plain != nightly,
        "**THE KEY LIGHTS THE SUBJECT FROM WHERE IT IS DECLARED**: the same subject under the "
        "same illuminance at +42 deg and at -80 deg differs, because the cosine at those two "
        "elevations differs fourfold and a surface turns toward or away from its light");

  CHECK(filled != nightly,
        "**A LIGHTING THAT IS NOT DECLARED DOES NOT LIGHT**: the same key with Lit.Declared left "
        "false makes the identical picture, so a scenario assembled in code carries lighting it "
        "never declared. Engine::Declare copies KeyLux, the two angles and the environment "
        "unconditionally (Engine.cpp:559-562); only the parser writes the flag and only the "
        "layer merge reads it, so at the door the flag means nothing at all");

  Covers("the door: the declared key illuminance sets the fill ratio and nothing else, so the "
         "picture is invariant under a common scale of key and environment");
  return Report();
}
