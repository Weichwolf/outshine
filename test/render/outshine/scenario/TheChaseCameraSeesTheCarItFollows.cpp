#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <unistd.h>

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

std::string Planted(const char *name) { return outshine::Test::PlantedPath(name); }

size_t Different(const std::vector<uint8_t> &with, const std::vector<uint8_t> &without) {
  size_t apart = 0;
  for (size_t px = 0; px + 3 < with.size() && px + 3 < without.size(); px += 4) {
    const int delta = std::abs((int)with[px] - (int)without[px]) +
                      std::abs((int)with[px + 1] - (int)without[px + 1]) +
                      std::abs((int)with[px + 2] - (int)without[px + 2]);
    if (delta > 24) { ++apart; }
  }
  return apart;
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
  declaration.ShadowRadiusM = 60.0;

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
  const auto banish = [&]() {
    double away[16] = {0};
    away[0] = away[5] = away[10] = assetM;
    away[13] = -10000.0;
    away[15] = 1.0;
    return standing->Carry(away, roadAt, error);
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

  std::vector<uint8_t> seen, empty;
  CHECK(frame(chase, seen), "the chase frame renders");
  if (seen.empty()) { return Report(); }
  CHECK(banish() && frame(chase, empty), "and the same frame renders with the car banished");
  CHECK(standing->Carry(body, roadAt, error), "and the car returns");
  const size_t dark = Different(seen, empty);
  Note("pixels the car changes in the chase frame", (double)dark, "px");
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
    std::vector<uint8_t> gone;
    if (!banish() || !frame(follows, gone)) { return false; }
    if (!standing->Carry(turned, roadAt, error)) { return false; }
    std::vector<uint8_t> moved;
    if (!frame(follows, moved)) { return false; }
    darkOut = Different(moved, gone);
    return true;
  };

  {
    const double origin[3] = {0.0, 0.0, 0.0};
    const double out[3] = {-40.9, 0.9, -189.9};
    size_t dark0 = 0, darkYaw = 0, darkOut = 0, darkBoth = 0;
    CHECK(placedAt(0.0, origin, dark0) && placedAt(12.8, origin, darkYaw) &&
              placedAt(0.0, out, darkOut) && placedAt(12.8, out, darkBoth),
          "four placements render: origin, yaw alone, offset alone, both");
    Note("pixels the car changes at the origin, no yaw", (double)dark0, "px");
    Note("pixels the car changes with yaw alone", (double)darkYaw, "px");
    Note("pixels the car changes 190 m out, no yaw", (double)darkOut, "px");
    {
      size_t ignored = 0;
      (void)placedAt(0.0, out, ignored);
      std::string shotError;
      if (!standing->Screenshot(Planted("chase-offset-probe.png").c_str(), shotError)) {
        std::printf("REFUSED %s\n", shotError.c_str());
      }
      (void)placedAt(0.0, origin, ignored);
      if (!standing->Screenshot(Planted("chase-origin-probe.png").c_str(), shotError)) {
        std::printf("REFUSED %s\n", shotError.c_str());
      }
    }

    Note("pixels the car changes with both", (double)darkBoth, "px");
    CHECK(darkYaw > (size_t)(kWidePx * kHighPx) / 100,
          "the car survives its own yaw at the origin");
    CHECK(darkOut > (size_t)(kWidePx * kHighPx) / 100,
          "**AND IT SURVIVES STANDING 190 M FROM THE ORIGIN**, which is the drive's own frame");
    CHECK(darkBoth > (size_t)(kWidePx * kHighPx) / 100,
          "and the two together");
  }

  {
    CHECK(standing->Restand(ground, error), 
          "the picture re-stands the way the drive re-lays its corridor");
    std::vector<uint8_t> gone, after;
    CHECK(banish() && frame(chase, gone) && standing->Carry(body, roadAt, error) &&
              frame(chase, after),
          "and the car is carried again after it");
    const size_t darkAfter = Different(after, gone);
    Note("pixels the car changes after a restand", (double)darkAfter, "px");
    CHECK(darkAfter > (size_t)(kWidePx * kHighPx) / 100,
          "**AND THE CAR SURVIVES A RESTAND**, which the drive does at every relay");
    Note("times the asset was read from disk", (double)outshine::Clients::Live::AssetReads(), "reads");
    CHECK(outshine::Clients::Live::AssetReads() == 1,
          "**AND THE RELAY TOOK NO DISK**: the glTF was read once at stand-up and never again -- "
          "a restand rebuilds what changed (the built geometry) and re-reads nothing that did "
          "not (board:1574), which is RAGE's rule that the frame path takes nothing");
  }

  {
    std::vector<float> atlas;
    CHECK(renderer.ReadShadowAtlas(atlas) == outshine::Render::ReadState::Ready,
          "the shadow atlas reads back");
    size_t written = 0;
    float nearest = 0.0f, median = 0.0f;
    std::vector<float> sample;
    for (size_t at = 0; at < atlas.size(); at += 97) { sample.push_back(atlas[at]); }
    std::sort(sample.begin(), sample.end());
    if (!sample.empty()) { median = sample[sample.size() / 2]; }
    for (const float texel : atlas) {
      if (texel > 0.0f) { ++written; }
      nearest = std::fmax(nearest, texel);
    }
    Note("atlas texels written by the sun's pass", (double)written, "texels");
    Note("of the atlas", (double)written / (double)atlas.size(), "fraction");
    Note("the nearest depth the sun sees", nearest, "");
    Note("the median depth", median, "");
    CHECK(written > atlas.size() * 9 / 10,
          "**THE ATLAS HOLDS THE SCENE FROM THE SUN'S SEAT.** A 60 m orthographic frame centred "
          "on the car over an 800 m ground writes essentially every texel -- an empty atlas is "
          "what board:1575 opened on, six declared-and-unexecuted stages ago");
    CHECK(nearest > median + 0.003f,
          "**AND SOMETHING STANDS PROUD OF THE GROUND IN IT.** The car's roof is 1.44 m above "
          "the deck; along the 42-degree sun that is at least 0.004 of the atlas's depth range "
          "(1.44 * cos(48 deg) / 240 m), so the nearest texel (reverse depth, so the LARGEST) must beat the median by more than 0.003 -- a car-shaped dent in the sun's depth field, measured without rendering a "
          "single shadowed pixel");
  }

  Covers("I.4.6 a picture that carries a subject's parts draws them where the placement puts "
         "them, measured from the chase offset the scenario declares -- and the sun's own depth "
         "atlas carries the same scene");
  return Report();
}
