#include <cmath>
#include <cstdio>
#include <cstdlib>
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
using outshine::Gltf::Placement;
using outshine::Gltf::Subject;
using outshine::Span;

namespace {

constexpr int kWidePx = 640;
constexpr int kHighPx = 360;

constexpr double kAssetWheelbase = 180.71;
constexpr double kWheelbaseM = 2.810;
constexpr double kAssetGround = -60.939;
constexpr double kAssetCentreX = 60.104;
constexpr double kAssetCentreZ = 22.847;
constexpr double kChaseBackM = 7.0;
constexpr double kChaseUpM = 1.5;

std::string PreparedCar(void) {
  const char *const base = std::getenv("TMPDIR");
  std::string root = base != nullptr ? base : "/tmp";
  if (!root.empty() && root.back() == '/') { root.pop_back(); }
  return root + "/outshine-prepared/tools-driver-f31/scene.gltf";
}

size_t DarkAgainstGround(const std::vector<uint8_t> &rgba) {
  size_t dark = 0;
  for (size_t px = 0; px < rgba.size(); px += 4) {
    const int sum = rgba[px] + rgba[px + 1] + rgba[px + 2];
    if (rgba[px + 3] != 0 && sum < 60) { ++dark; }
  }
  return dark;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string carPath = PreparedCar();
  if (std::FILE *const probe = std::fopen(carPath.c_str(), "rb")) {
    std::fclose(probe);
  } else {
    Unprepared(("the declared F31 is not at " + carPath +
                " -- prepare.py scenario-assets places it").c_str());
    return Report();
  }

  std::vector<float> positions, normals;
  std::vector<uint32_t> indices;
  {
    const float h = 400.0f;
    const int cells = 220;
    for (int row = 0; row <= cells; ++row) {
      for (int column = 0; column <= cells; ++column) {
        positions.insert(positions.end(),
                         {-h + 2.0f * h * (float)column / (float)cells, 0.0f,
                          -h + 2.0f * h * (float)row / (float)cells});
        normals.insert(normals.end(), {0.0f, 1.0f, 0.0f});
      }
    }
    for (int row = 0; row < cells; ++row) {
      for (int column = 0; column < cells; ++column) {
        const uint32_t at = (uint32_t)(row * (cells + 1) + column);
        const uint32_t below = at + (uint32_t)cells + 1u;
        indices.insert(indices.end(), {at, below, at + 1u, at + 1u, below, below + 1u});
      }
    }
  }
  std::vector<float> deckPositions, deckNormals;
  std::vector<uint32_t> deckIndices;
  {
    const float half = 5.0f;
    const float lift = 0.35f;
    const float corner[4][3] = {{-half, lift, -400.0f}, {half, lift, -400.0f},
                                {half, lift, 400.0f}, {-half, lift, 400.0f}};
    for (const auto &at : corner) {
      deckPositions.insert(deckPositions.end(), {at[0], at[1], at[2]});
      deckNormals.insert(deckNormals.end(), {0.0f, 1.0f, 0.0f});
    }
    deckIndices = {0, 2, 1, 0, 3, 2};
  }
  Piece pair[2];
  pair[0].NodeName = "ground";
  pair[0].Material = 0;
  pair[0].PositionsM = Span<const float>(positions.data(), positions.size());
  pair[0].Normals = Span<const float>(normals.data(), normals.size());
  pair[0].Indices = Span<const uint32_t>(indices.data(), indices.size());
  pair[1].NodeName = "deck";
  pair[1].Material = 1;
  pair[1].PositionsM = Span<const float>(deckPositions.data(), deckPositions.size());
  pair[1].Normals = Span<const float>(deckNormals.data(), deckNormals.size());
  pair[1].Indices = Span<const uint32_t>(deckIndices.data(), deckIndices.size());
  Subject ground;
  CHECK(ground.Assemble(Assembly{Span<const Piece>(pair, 2)}),
        "a ground of 96 800 triangles and a carriageway deck assemble -- the drive's own shape, "
        "two pieces and two declared surfaces, at a size in the drive's own class");

  outshine::Render::Renderer renderer;
  Declaration declaration;
  declaration.SurfaceWidthPx = kWidePx;
  declaration.SurfaceHeightPx = kHighPx;
  declaration.Stands = carPath;
  declaration.Built = &ground;
  declaration.Surfacing.resize(2);
  declaration.Surfacing[0].BaseColour[0] = 0.24f;
  declaration.Surfacing[0].BaseColour[1] = 0.30f;
  declaration.Surfacing[0].BaseColour[2] = 0.16f;
  declaration.Surfacing[0].Roughness = 0.98f;
  declaration.Surfacing[1].BaseColour[0] = 0.14f;
  declaration.Surfacing[1].BaseColour[1] = 0.14f;
  declaration.Surfacing[1].BaseColour[2] = 0.15f;
  declaration.Surfacing[1].Roughness = 0.92f;
  declaration.KeyLux = 40000.0;
  declaration.KeyElevationDeg = 42.0;
  declaration.KeyBearingDeg = 150.0;
  declaration.DrawsSky = true;

  std::unique_ptr<Live> standing;
  std::string error;
  const bool stood = Live::Open(renderer, std::move(declaration), nullptr, standing, error);
  if (!stood) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(stood, "the car and the ground stand in one picture");
  if (!stood) { return Report(); }
  Note("parts the picture carries", (double)standing->Shown().Parts().size(), "parts");
  Note("of them the car's", (double)standing->CarriedParts(), "parts");

  const double assetM = kWheelbaseM / kAssetWheelbase;
  double body[16] = {0};
  body[0] = body[5] = body[10] = assetM;
  body[12] = -kAssetCentreX * assetM;
  body[13] = -kAssetGround * assetM;
  body[14] = -kAssetCentreZ * assetM;
  body[15] = 1.0;
  double roadAt[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  CHECK(standing->Carry(body, roadAt, error),
        "and the car takes its measured scale and stands with its tyres on the ground");

  const auto frame = [&](const Placement &eye, std::vector<uint8_t> &rgba) {
    standing->Eye(eye);
    if (!standing->Advance(error)) {
      std::printf("REFUSED %s\n", error.c_str());
      return false;
    }
    return standing->ReadPixels(rgba, error);
  };

  Placement chase;
  chase.EyeM[0] = 0.0;
  chase.EyeM[1] = kChaseUpM;
  chase.EyeM[2] = kChaseBackM;
  chase.Forward[0] = 0.0;
  chase.Forward[1] = 0.0;
  chase.Forward[2] = -1.0;
  chase.Right[0] = 1.0;
  chase.Right[1] = 0.0;
  chase.Right[2] = 0.0;
  chase.Up[0] = 0.0;
  chase.Up[1] = 1.0;
  chase.Up[2] = 0.0;
  chase.YfovRad = 55.0 * 3.14159265358979 / 180.0;
  chase.ZNearM = 0.1;

  std::vector<uint8_t> seen;
  CHECK(frame(chase, seen), "the chase frame renders");
  if (seen.empty()) { return Report(); }
  const size_t dark = DarkAgainstGround(seen);
  Note("pixels darker than RGB-sum 60 in the chase frame", (double)dark, "px");
  Note("what the whole frame holds", (double)(kWidePx * kHighPx), "px");

  CHECK(dark > (size_t)(kWidePx * kHighPx) / 100,
        "**A CHASE CAMERA SEVEN METRES BEHIND THE CAR SEES THE CAR.** The reviewer measured zero "
        "pixels darker than RGB-sum 60 across two chase stills of the drive; a 1.8 m wide, 1.44 m "
        "tall dark car at 7 m under a 55 deg lens must cover more than a hundredth of the frame. "
        "This is board:1551's bisection made permanent: same Live, same Carry, same camera "
        "shape, no corridor and no journey in the way");

  const auto placedAt = [&](double yawDeg, const double at[3], size_t &darkOut) {
    const double yaw = yawDeg * 3.14159265358979 / 180.0;
    const double c = std::cos(yaw), n = std::sin(yaw);
    double turned[16] = {0};
    turned[0] = c * assetM;
    turned[2] = -n * assetM;
    turned[5] = assetM;
    turned[8] = n * assetM;
    turned[10] = c * assetM;
    turned[15] = 1.0;
    const double shift[3] = {-kAssetCentreX * assetM, -kAssetGround * assetM,
                             -kAssetCentreZ * assetM};
    for (int row = 0; row < 3; ++row) {
      turned[12 + row] = at[row];
      for (int k = 0; k < 3; ++k) { turned[12 + row] += turned[k * 4 + row] / assetM * shift[k]; }
    }
    if (!standing->Carry(turned, roadAt, error)) { return false; }
    Placement follows = chase;
    const double ahead[3] = {-n, 0.0, -c};
    for (int axis = 0; axis < 3; ++axis) {
      follows.EyeM[axis] = at[axis] - ahead[axis] * kChaseBackM;
      follows.Forward[axis] = ahead[axis];
    }
    follows.EyeM[1] += kChaseUpM;
    follows.Right[0] = c;
    follows.Right[1] = 0.0;
    follows.Right[2] = -n;
    std::vector<uint8_t> moved;
    if (!frame(follows, moved)) { return false; }
    darkOut = DarkAgainstGround(moved);
    return true;
  };

  {
    const double origin[3] = {0.0, 0.0, 0.0};
    const double out[3] = {-40.9, 0.9, -189.9};
    size_t dark0 = 0, darkYaw = 0, darkOut = 0, darkBoth = 0;
    CHECK(placedAt(0.0, origin, dark0) && placedAt(12.8, origin, darkYaw) &&
              placedAt(0.0, out, darkOut) && placedAt(12.8, out, darkBoth),
          "four placements render: origin, yaw alone, offset alone, both");
    Note("dark pixels at the origin, no yaw", (double)dark0, "px");
    Note("dark pixels with yaw alone", (double)darkYaw, "px");
    Note("dark pixels 190 m out, no yaw", (double)darkOut, "px");
    {
      size_t ignored = 0;
      (void)placedAt(0.0, out, ignored);
      std::string shotError;
      if (!standing->Screenshot("/tmp/chase-offset-probe.png", shotError)) {
        std::printf("REFUSED %s\n", shotError.c_str());
      }
      (void)placedAt(0.0, origin, ignored);
      if (!standing->Screenshot("/tmp/chase-origin-probe.png", shotError)) {
        std::printf("REFUSED %s\n", shotError.c_str());
      }
    }

    Note("dark pixels with both", (double)darkBoth, "px");
    CHECK(darkYaw > (size_t)(kWidePx * kHighPx) / 100,
          "the car survives its own yaw at the origin");
    CHECK(darkOut > (size_t)(kWidePx * kHighPx) / 100,
          "**AND IT SURVIVES STANDING 190 M FROM THE ORIGIN**, which is the drive's own frame");
    CHECK(darkBoth > (size_t)(kWidePx * kHighPx) / 100,
          "and the two together");
  }

  {
    const double yaw = 12.8 * 3.14159265358979 / 180.0;
    const double c = std::cos(yaw), n = std::sin(yaw);
    const double at[3] = {-40.9, 0.9, -189.9};
    double turned[16] = {0};
    turned[0] = c * assetM;
    turned[2] = -n * assetM;
    turned[5] = assetM;
    turned[8] = n * assetM;
    turned[10] = c * assetM;
    turned[15] = 1.0;
    const double shift[3] = {-kAssetCentreX * assetM, -kAssetGround * assetM,
                             -kAssetCentreZ * assetM};
    for (int row = 0; row < 3; ++row) {
      turned[12 + row] = at[row];
      for (int k = 0; k < 3; ++k) { turned[12 + row] += turned[k * 4 + row] / assetM * shift[k]; }
    }
    CHECK(standing->Carry(turned, roadAt, error),
          "the car turns to the drive's own heading and moves 190 m out");

    Placement follows = chase;
    const double ahead[3] = {-n, 0.0, -c};
    for (int axis = 0; axis < 3; ++axis) {
      follows.EyeM[axis] = at[axis] - ahead[axis] * kChaseBackM;
      follows.Forward[axis] = ahead[axis];
    }
    follows.EyeM[1] += kChaseUpM;
    follows.Right[0] = c;
    follows.Right[1] = 0.0;
    follows.Right[2] = -n;

    std::vector<uint8_t> moved;
    CHECK(frame(follows, moved), "and the moved chase frame renders");
    const size_t darkMoved = DarkAgainstGround(moved);
    Note("dark pixels with the drive's own heading and offset", (double)darkMoved, "px");
    CHECK(darkMoved > (size_t)(kWidePx * kHighPx) / 100,
          "**AND THE CAR SURVIVES THE DRIVE'S OWN ROTATION AND OFFSET** -- the placement carries "
          "a yaw and stands 190 m from the origin, exactly the frame the stills tool prints, and "
          "the camera still sees it");
  }

  {
    CHECK(standing->Restand(ground, error) && standing->Carry(body, roadAt, error),
          "the picture re-stands the way the drive re-lays its corridor, and the car is carried "
          "again");
    std::vector<uint8_t> after;
    CHECK(frame(chase, after), "and the frame after the restand renders");
    const size_t darkAfter = DarkAgainstGround(after);
    Note("dark pixels after a restand", (double)darkAfter, "px");
    CHECK(darkAfter > (size_t)(kWidePx * kHighPx) / 100,
          "**AND THE CAR SURVIVES A RESTAND**, which the drive does at every relay");
  }

  Covers("I.4.6 a picture that carries a subject's parts draws them where the placement puts "
         "them, measured from the chase offset the scenario declares");
  return Report();
}
