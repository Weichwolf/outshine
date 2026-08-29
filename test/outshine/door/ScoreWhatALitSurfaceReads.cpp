#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// THE LAW. A surface's irradiance from a distant key is E * (N . L), and for a HORIZONTAL
// surface under a key declared by elevation and bearing,
//
//   N = (0, 1, 0)
//   L = (cos(elev) sin(bear), sin(elev), cos(elev) cos(bear))
//   N . L = sin(elev)
//
// The bearing CANCELS. It appears in two of L's three components and in neither of the two the
// dot product keeps. So a horizontal surface reads the same under every bearing, and reads
// sin(elev) of the key -- and this holds for any distant source over any horizontal plane,
// whatever a renderer does downstream.
//
// WHY THIS IS THE CASE THAT MATTERS. The key is built in the glTF frame and mapped into the
// engine's by EcefFromGltf (src/engine/GltfStudio.cpp:13), whose image of glTF up is the
// engine's +X. If the light's frame and the geometry's frame were to disagree by a rotation,
// the bearing would sweep partly THROUGH the surface normal and a horizontal surface would
// brighten and darken as the bearing turned. So the cancellation above is exactly the
// measurement that separates one frame from two.
//
// READING IT THROUGH A TONEMAPPER. What the door hands back is tonemapped and transfer-encoded,
// so a pixel is a monotone but unknown function of radiance and no ratio survives it. Two
// things do:
//
//   EQUALITY survives any monotone map. Four bearings that should read alike must read alike.
//   ORDER survives any monotone map. sin(42) = 0.669 against sin(21) = 0.358 must be brighter.
//
// AND THE CONTROL FOR A SPREAD OF ZERO. A spread of zero also reads zero when the bearing is
// dropped on the floor and never reaches the shading at all -- a check that cannot fail proves
// nothing, which is why the same four bearings are also read over a VERTICAL surface, where
//
//   N = (0, 0, 1)   ->   N . L = cos(elev) cos(bear)
//
// keeps the bearing rather than discarding it, and swings from cos(42) = +0.743 at bearing 0
// to -0.743 at bearing 180, which is to say from fully lit to facing away. If the horizontal
// spread is zero while the vertical spread is not, the bearing reaches the shading and a
// horizontal surface is genuinely blind to it.
//
// The case therefore measures the spread across four bearings and compares it against the
// change one elevation step makes. The tolerance is not a taste: the bearing spread must be
// under a TENTH of the elevation step, because a frame mismatch of any consequence would make
// the two comparable -- a bearing sweeping through the normal moves N.L by as much as an
// elevation change does.
constexpr int kFramePx = 96;

constexpr const char *kUprightBase64 =
    "AACAvwAAgL8AAAAAAACAPwAAgL8AAAAAAACAPwAAgD8AAAAAAACAvwAAgL8AAAAAAACAPwAAgD8AAAAAAACAvwAAgD"
    "8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAA"
    "AAAAAAAAAIA/";

constexpr const char *kQuadBase64 =
    "AACAvwAAAAAAAIC/AACAvwAAAAAAAIA/AACAPwAAAAAAAIA/AACAvwAAAAAAAIC/AACAPwAAAAAAAIA/AACAPwAAAA"
    "AAAIC/AAAAAAAAgD8AAAAAAAAAAAAAgD8AAAAAAAAAAAAAgD8AAAAAAAAAAAAAgD8AAAAAAAAAAAAAgD8AAAAAAAAA"
    "AAAAgD8AAAAA";

[[nodiscard]] std::string Sheet(const char *held, const char *least, const char *most) {
  return std::string(
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
      "\"nodes\":[{\"mesh\":0}],"
      "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},\"material\":0}]}],"
      "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,0.8,1.0],"
      "\"metallicFactor\":0.0,\"roughnessFactor\":1.0}}],"
      "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":6,\"type\":\"VEC3\","
      "\"min\":[") + least + "],\"max\":[" + most + "]},"
      "{\"bufferView\":1,\"componentType\":5126,\"count\":6,\"type\":\"VEC3\"}],"
      "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":72},"
      "{\"buffer\":0,\"byteOffset\":72,\"byteLength\":72}],"
      "\"buffers\":[{\"byteLength\":144,\"uri\":\"data:application/octet-stream;base64," +
      held + "\"}]}";
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

[[nodiscard]] double Peak(const std::vector<uint8_t> &rgba) {
  double peak = 0.0;
  for (size_t at = 0; at + 3 < rgba.size(); at += 4) {
    const int r = rgba[at], g = rgba[at + 1], b = rgba[at + 2];
    const double most = r > g ? (r > b ? r : b) : (g > b ? g : b);
    peak = most > peak ? most : peak;
  }
  return peak;
}

[[nodiscard]] outshine::Scenario Under(double elevationDeg, double bearingDeg,
                                       const char *surface) {
  outshine::Scenario made;
  made.Render.Declared = true;
  made.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  made.Lit.Declared = true;
  made.Lit.Key.Lux = 40000.0;
  made.Lit.Key.ElevationDeg = elevationDeg;
  made.Lit.Key.BearingDeg = bearingDeg;
  outshine::Asset shown;
  shown.Uri = surface;
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
  if (!Wrote(under + "/level.gltf", Sheet(kQuadBase64, "-1,0,-1", "1,0,1")) ||
      !Wrote(under + "/upright.gltf", Sheet(kUprightBase64, "-1,-1,0", "1,1,0"))) {
    Unprepared("a surface could not be written into the nest");
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

  double levelPeak = 0.0;
  const auto read = [&](double elevationDeg, double bearingDeg, const char *surface,
                        double &into) {
    const outshine::Scenario stands = Under(elevationDeg, bearingDeg, surface);
    std::vector<uint8_t> rgba;
    if (!engine.declare(stands) || !engine.renderer().readPixels(rgba)) { return false; }
    into = Mean(rgba);
    levelPeak = Peak(rgba);
    return true;
  };

  constexpr double kBearings[4] = {0.0, 90.0, 180.0, 270.0};
  double swept[4] = {0.0, 0.0, 0.0, 0.0};
  double lower = 0.0, dark = 0.0;
  bool came = true;
  double upright[4] = {0.0, 0.0, 0.0, 0.0};
  for (int at = 0; at < 4; ++at) {
    came = came && read(42.0, kBearings[at], "level.gltf", swept[at]) &&
           read(42.0, kBearings[at], "upright.gltf", upright[at]);
  }
  came = came && read(21.0, 0.0, "level.gltf", lower) && read(-80.0, 0.0, "level.gltf", dark);
  if (!came) {
    Unprepared(("a picture did not come back: " + engine.error()).c_str());
    return Report();
  }

  double least = swept[0], most = swept[0], total = 0.0;
  for (int at = 0; at < 4; ++at) {
    if (swept[at] < least) { least = swept[at]; }
    if (swept[at] > most) { most = swept[at]; }
    total += swept[at];
    std::printf("  elevation 42 deg, bearing %3.0f deg   level %7.3f   upright %7.3f\n",
                kBearings[at], swept[at], upright[at]);
  }
  double leastUpright = upright[0], mostUpright = upright[0];
  for (int at = 0; at < 4; ++at) {
    if (upright[at] < leastUpright) { leastUpright = upright[at]; }
    if (upright[at] > mostUpright) { mostUpright = upright[at]; }
  }
  const double uprightSpread = mostUpright - leastUpright;
  const double spread = most - least;
  const double step = total / 4.0 - lower;
  double atFortyTwo = 0.0;
  if (!read(42.0, 0.0, "level.gltf", atFortyTwo)) {
    Unprepared("the level surface would not stand for the value check");
    return Report();
  }
  const double peakAtFortyTwo = levelPeak;
  std::printf("  elevation 42 deg, level surface      PEAK %7.3f of 255\n", peakAtFortyTwo);
  std::printf("  elevation 21 deg, bearing   0 deg   mean max(RGB) %7.3f\n", lower);
  std::printf("  elevation -80 deg (no key)          mean max(RGB) %7.3f\n", dark);
  std::printf("  spread over four bearings %7.3f     one elevation step %7.3f     ratio %6.3f\n",
              spread, step, step > 0.0 ? spread / step : -1.0);
  std::printf("  the same four bearings over an UPRIGHT surface spread %7.3f\n", uprightSpread);

  // THE CLOSED FORM, AND WHAT IT PINS. An ordering is satisfied by any monotone wrongness: a
  // chain off by a factor of two passes every check in this case and looks merely dim. This one
  // states the value.
  //
  //   E_perp    40000 lx                      declared. That `lux` is the illuminance on a
  //                                           surface PERPENDICULAR to the sun and not on a
  //                                           horizontal one is what this number measures -- the
  //                                           horizontal reading would put the peak at 208.
  //   cos(t)    sin(42 deg) = 0.669131        a level surface under a sun at 42 degrees
  //   L         E*cos(t)*albedo/pi
  //             = 40000*0.669131*0.8/pi       = 6814.0 cd/m^2      (baseColorFactor 0.8)
  //   ev100     log2(40000/2.5)               = 13.9658
  //   exposure  1/(1.2*2^ev100)               = 5.20833e-05        published by the engine
  //   x         L*exposure                    = 0.354895
  //   filmic    x(2.51x+0.03)/(x(2.43x+0.59)+0.14) = 0.498562
  //   display   0.498562^(1/2.2) * 255        = 185.9
  //
  // The tolerance is FOUR counts and the reason is stated rather than hidden: the encode's exact
  // transfer is not pinned here. A pure 2.2 power gives 185.9, the sRGB piecewise curve gives
  // 187.3, and the measurement sits between them at 186. Which transfer the frame carries is a
  // separate question; four counts cannot hide a factor of two, which would move this by forty.
  constexpr double kOwed = 185.9;
  constexpr double kWithin = 4.0;
  std::printf("  the closed form owes                %7.3f of 255, measured %7.3f\n", kOwed,
              peakAtFortyTwo);
  CHECK(std::fabs(peakAtFortyTwo - kOwed) <= kWithin,
        "**A LEVEL LAMBERTIAN SURFACE READS WHAT THE CLOSED FORM SAYS**: declared illuminance "
        "through the declared albedo, the engine's own published exposure and the filmic curve, "
        "end to end and to within the encode's own transfer. Every other check in this case is "
        "an ORDERING, and an ordering is satisfied by any monotone wrongness");

  CHECK(step > 0.0,
        "**A KEY'S ELEVATION REACHES A HORIZONTAL SURFACE**: sin(42) = 0.669 against "
        "sin(21) = 0.358 is very nearly twice the irradiance, and a tonemapper may compress "
        "that but it may not reverse it -- order survives any monotone map. This is the control "
        "for the check below: without it a spread of zero would prove only that nothing is lit");

  CHECK(spread < 0.1 * step,
        "**A HORIZONTAL SURFACE DOES NOT SEE THE KEY'S BEARING**: N.L = sin(elevation) for "
        "N = (0,1,0), because the bearing appears only in the two components the dot product "
        "discards. So the light's frame and the geometry's frame are ONE frame. Were they to "
        "disagree by a rotation, a turning bearing would sweep partly through the surface "
        "normal and this spread would be comparable to the elevation step beside it, not a "
        "tenth of it");

  CHECK(uprightSpread > step,
        "and the control for a spread of zero: over an UPRIGHT surface the same four bearings "
        "spread WIDER than one elevation step, because N.L = cos(elev) cos(bear) keeps the "
        "bearing rather than discarding it and swings from fully lit to facing away. So the "
        "bearing does reach the shading, and the zero above is a horizontal surface being blind "
        "to it rather than a declaration going nowhere");

  CHECK(dark < least,
        "and a key 80 degrees BELOW the horizon lights nothing: the surface falls to whatever "
        "floor the declared environment sets, and that floor is below every lit reading");

  return Report();
}
