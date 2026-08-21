#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Check.h"

#include "Live.h"
#include "Renderer.h"

using outshine::Clients::Declaration;
using outshine::Clients::Live;
using outshine::Gltf::Assembly;
using outshine::Gltf::Piece;
using outshine::Gltf::Subject;
using outshine::Span;

namespace {

constexpr int kWidePx = 640;
constexpr int kHighPx = 360;
constexpr double kGroundHalfM = 200.0;

Subject Ground(std::vector<float> &positions, std::vector<float> &normals,
               std::vector<uint32_t> &indices) {
  const float h = (float)kGroundHalfM;
  const float corner[4][3] = {{-h, 0, -h}, {h, 0, -h}, {h, 0, h}, {-h, 0, h}};
  for (const auto &at : corner) {
    positions.insert(positions.end(), {at[0], at[1], at[2]});
    normals.insert(normals.end(), {0.0f, 1.0f, 0.0f});
  }
  indices = {0, 2, 1, 0, 3, 2};
  Piece lying;
  lying.PositionsM = Span<const float>(positions.data(), positions.size());
  lying.Normals = Span<const float>(normals.data(), normals.size());
  lying.Indices = Span<const uint32_t>(indices.data(), indices.size());
  Subject built;
  if (!built.Assemble(Assembly{Span<const Piece>(&lying, 1)})) { return Subject{}; }
  return built;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::vector<float> positions, normals;
  std::vector<uint32_t> indices;
  const Subject ground = Ground(positions, normals, indices);
  CHECK(ground.TriangleCount() == 2, "a ground of two triangles assembles without a file");

  outshine::Render::Renderer renderer;
  Declaration declaration;
  declaration.SurfaceWidthPx = kWidePx;
  declaration.SurfaceHeightPx = kHighPx;
  declaration.Built = &ground;
  declaration.KeyLux = 40000.0;
  declaration.KeyElevationDeg = 42.0;
  declaration.KeyBearingDeg = 150.0;
  declaration.DrawsSky = true;

  std::unique_ptr<Live> standing;
  std::string error;
  const bool stood = Live::Open(renderer, std::move(declaration), nullptr, standing, error);
  if (!stood) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(stood, "**A SCENARIO THAT DECLARES THE SKY STANDS UP.** Until board:1549 this refusal was "
               "mute and the picture silently omitted what it was asked for; then it was loud; now "
               "it is satisfied -- the whole medium chain runs before the first frame");
  if (!stood) { return Report(); }

  outshine::Gltf::Placement eye;
  eye.EyeM[0] = 0.0;
  eye.EyeM[1] = 1.6;
  eye.EyeM[2] = 0.0;
  eye.Forward[0] = 0.0;
  eye.Forward[1] = 0.0;
  eye.Forward[2] = -1.0;
  eye.Right[0] = 1.0;
  eye.Right[1] = 0.0;
  eye.Right[2] = 0.0;
  eye.Up[0] = 0.0;
  eye.Up[1] = 1.0;
  eye.Up[2] = 0.0;
  eye.YfovRad = 65.0 * 3.14159265358979 / 180.0;
  standing->Eye(eye);
  if (!standing->Advance(error)) { std::printf("REFUSED %s\n", error.c_str()); }

  std::vector<uint8_t> rgba;
  const bool readBack = standing->ReadPixels(rgba, error);
  if (!readBack) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(readBack, "and the frame reads back");
  if (rgba.size() != (size_t)kWidePx * kHighPx * 4u) { return Report(); }

  size_t skyPixels = 0, written = 0;
  double redUp = 0.0, greenUp = 0.0, blueUp = 0.0;
  const int skyRows = kHighPx / 4;
  for (int row = 0; row < skyRows; ++row) {
    for (int column = 0; column < kWidePx; ++column) {
      const uint8_t *const px = rgba.data() + ((size_t)row * kWidePx + column) * 4u;
      if (px[3] != 0) { ++written; }
      redUp += px[0];
      greenUp += px[1];
      blueUp += px[2];
      ++skyPixels;
    }
  }
  redUp /= (double)skyPixels;
  greenUp /= (double)skyPixels;
  blueUp /= (double)skyPixels;

  Note("pixels in the top quarter", (double)skyPixels, "px");
  Note("of them written", (double)written, "px");
  Note("mean red up there", redUp, "of 255");
  Note("mean green", greenUp, "of 255");
  Note("mean blue", blueUp, "of 255");

  CHECK(written == skyPixels,
        "**EVERY PIXEL OF THE TOP QUARTER IS WRITTEN.** The reviewer measured 51.5 % of a driving "
        "frame as alpha zero -- unwritten bytes composited over whatever the viewer's ground is. "
        "A sky stage's first duty is that the frame has no unwritten pixel above the horizon");
  CHECK(blueUp > redUp * 1.15 && blueUp > 40.0,
        "**AND IT IS BLUE, AND BRIGHT ENOUGH TO READ AS DAYLIGHT.** Blue leads red by at least 15 "
        "percent after the tonemap -- the display transfer compresses the linear ratio of ~3, so "
        "the bound is set where the CURVE puts it, not where the physics does -- and the mean blue "
        "clears 40 of 255, which a black frame or a grey haze does not");

  size_t groundDarker = 0;
  const int groundRow = kHighPx - kHighPx / 8;
  const int skyRow = kHighPx / 8;
  for (int column = 0; column < kWidePx; ++column) {
    const uint8_t *const above = rgba.data() + ((size_t)skyRow * kWidePx + column) * 4u;
    const uint8_t *const below = rgba.data() + ((size_t)groundRow * kWidePx + column) * 4u;
    const int aboveSum = above[0] + above[1] + above[2];
    const int belowSum = below[0] + below[1] + below[2];
    if (belowSum != aboveSum) { ++groundDarker; }
  }
  CHECK(groundDarker > kWidePx / 2,
        "and the ground row differs from the sky row across most of the width, so the horizon is "
        "IN the picture rather than the sky covering everything");

  Covers("I.18.7 a scenario that declares the sky gets one: every pixel above the horizon written, "
         "blue by derivation, standing over the drawn ground in one frame");
  return Report();
}
