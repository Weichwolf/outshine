#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// WHAT LIGHTS A SURFACE THE SUN CANNOT REACH.
//
// A surface turned away from the key receives N.L <= 0 from it and nothing at all. What it is
// lit by is the SKY: every patch of the hemisphere above it scatters sunlight toward it, and the
// integral of that over the hemisphere is the irradiance. So a declared sphere with air is not
// only something to look at -- it is the second light in every scene under it, and the one that
// decides whether a shadow, a cabin or a wheel arch is dark or black.
//
// Three consequences, all measurable at the pixel and none of them a property of our design:
//
//   a surface facing AWAY from the sun under a sphere is LIT       the sky is above it
//   it is DARKER as the sun drops                                  less sunlight enters the air
//   with NO sphere declared it is BLACK                            there is no second light
//
// The upright surface is turned to bearing 180 against a key at bearing 0, so N.L = -cos(elev)
// at both elevations compared -- fully away, not merely grazing. Nothing the key does can reach
// it, and every level it reads is the sky's.
//
// WHY THIS CASE EXISTS. `Live::Stand` computes the sky's irradiance from the medium
// (MediumSkyIrradiance) and adds it to the declared environment -- and the whole block is gated
// on `Declared_.DrawsSky`, which was written by nobody and constant false for the life of the
// tree. The capability stood complete and unreachable, which is the shape CLAUDE.md predicts.
// Measured on the driver the day a sphere first reached it, the cabin interior went from mean
// max(RGB) 0.64 with 98.6 % of it below RGB 8, to 34.73 with 29.5 % -- a factor of 54 from a
// declaration, not from a line of shading.
//
// AND THE DECLARED ENVIRONMENT BESIDE IT carries no unit. `<key lux>` is an illuminance and
// `<environment r g b>` is a bare triple, and the two are summed after one exposure divides
// both: 0.06 against a 40000 lx key is 3.1e-06, which is 0.80 of 255. Against the sky's 34 it is
// noise, and this case declares none so that what it measures is the sphere's alone.
constexpr int kFramePx = 96;

constexpr const char *kUprightBase64 =
    "AACAvwAAgL8AAAAAAACAPwAAgL8AAAAAAACAPwAAgD8AAAAAAACAvwAAgL8AAAAAAACAPwAAgD8AAAAAAACAvwAAgD"
    "8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAA"
    "AAAAAAAAAIA/";

[[nodiscard]] std::string Upright(void) {
  return std::string(
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
      "\"nodes\":[{\"mesh\":0}],"
      "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},"
      "\"material\":0}]}],"
      "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,0.8,1.0],"
      "\"metallicFactor\":0.0,\"roughnessFactor\":1.0}}],"
      "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":6,\"type\":\"VEC3\","
      "\"min\":[-1,-1,0],\"max\":[1,1,0]},"
      "{\"bufferView\":1,\"componentType\":5126,\"count\":6,\"type\":\"VEC3\"}],"
      "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":72},"
      "{\"buffer\":0,\"byteOffset\":72,\"byteLength\":72}],"
      "\"buffers\":[{\"byteLength\":144,\"uri\":\"data:application/octet-stream;base64,") +
      kUprightBase64 + "\"}]}";
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

[[nodiscard]] double Mean(const std::vector<uint8_t> &rgba) {
  const size_t pixels = rgba.size() / 4;
  if (pixels == 0) { return 0.0; }
  double sum = 0.0;
  for (size_t at = 0; at < pixels; ++at) {
    const int r = rgba[at * 4], g = rgba[at * 4 + 1], b = rgba[at * 4 + 2];
    sum += r > g ? (r > b ? r : b) : (g > b ? g : b);
  }
  return sum / (double)pixels;
}

[[nodiscard]] outshine::Scenario Turned(bool withASphere, double elevationDeg) {
  outshine::Scenario made;
  made.Render.Declared = true;
  made.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  made.Lit.Declared = true;
  made.Lit.Key.Lux = 40000.0;
  made.Lit.Key.ElevationDeg = elevationDeg;
  made.Lit.Key.BearingDeg = 180.0;
  if (withASphere) {
    made.Ground.Declared = true;
    made.Ground.Lat = 48.1372;
    made.Ground.Lon = 11.5756;
  }
  outshine::Asset shown;
  shown.Uri = "upright.gltf";
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
    Unprepared("this case writes its surface into the runner's nest and was given none");
    return Report();
  }
  const std::string under = nest;
  if (!Wrote(under + "/upright.gltf", Upright())) {
    Unprepared("the surface could not be written into the nest");
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

  const auto read = [&](bool sphere, double elevationDeg, double &into) {
    std::vector<uint8_t> rgba;
    if (!engine.Declare(Turned(sphere, elevationDeg)) || !engine.Pixels(rgba)) { return false; }
    into = Mean(rgba);
    return true;
  };

  double high = 0.0, low = 0.0, under_horizon = 0.0, bare = 0.0, litSide = 0.0;
  if (!read(true, 42.0, high) || !read(true, 8.0, low) || !read(true, -80.0, under_horizon) ||
      !read(false, 42.0, bare)) {
    Unprepared(("a picture did not come back: " + engine.Error()).c_str());
    return Report();
  }
  {
    outshine::Scenario facing = Turned(true, 42.0);
    facing.Lit.Key.BearingDeg = 0.0;
    std::vector<uint8_t> rgba;
    if (!engine.Declare(facing) || !engine.Pixels(rgba)) {
      Unprepared(("the lit side did not come back: " + engine.Error()).c_str());
      return Report();
    }
    litSide = Mean(rgba);
  }

  std::printf("  facing AWAY, sphere, sun  42 deg   mean max(RGB) %7.3f\n", high);
  std::printf("  facing AWAY, sphere, sun   8 deg   mean max(RGB) %7.3f\n", low);
  std::printf("  facing AWAY, sphere, sun -80 deg   mean max(RGB) %7.3f\n", under_horizon);
  std::printf("  facing AWAY, NO sphere,   42 deg   mean max(RGB) %7.3f\n", bare);
  std::printf("  facing the sun, sphere,   42 deg   mean max(RGB) %7.3f\n", litSide);

  CHECK(litSide > high,
        "the key still lights the side turned toward it far more brightly than the sky lights "
        "the side turned away, so what follows is a measurement of the SECOND light and not of "
        "the first leaking around the geometry");

  CHECK(high > 0.0,
        "**A DECLARED SPHERE IS THE SECOND LIGHT**: a surface turned away from the key receives "
        "nothing from it, and under a sphere with air it is still lit -- by the hemisphere of "
        "scattered sunlight above it. Without this a shadow, a cabin and a wheel arch are not "
        "dark, they are BLACK");

  CHECK(high > low,
        "and the sky follows the sun down: less sunlight enters the air at 8 degrees than at 42, "
        "so the irradiance it scatters onto a shaded surface falls with it");

  CHECK(under_horizon == 0.0,
        "and with the sun 80 degrees below the horizon the second light is gone too, exactly -- "
        "no sunlight enters the air at all, so there is nothing for it to scatter");

  CHECK(bare == 0.0,
        "and the CONTROL: with no sphere declared the same surface at the same elevation is "
        "BLACK. The light it had came from the declaration of a medium and from nothing else, "
        "and this case would prove nothing if it were lit anyway");

  Covers("the door: a declared sphere with air is the second light every scene under it "
         "receives, so a surface the key cannot reach is dark rather than black, and the "
         "irradiance it gets follows the sun down and vanishes with it");
  return Report();
}
