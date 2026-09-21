#include "Check.h"
#include "TerrainPress.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {
constexpr int kSide = 3;
constexpr int kHalo = 1;
constexpr int kZoom = 20;

outshine::Patchwork FlatCandidate() {
  constexpr uint32_t centre = 1u << (kZoom - 1);
  outshine::Patchwork candidate;
  candidate.Sheets.push_back(
      {.Tile = {.Zoom = kZoom, .X = centre, .Y = centre},
       .Nodes = std::vector<float>((kSide + 2 * kHalo) * (kSide + 2 * kHalo), 0.0f),
       .Side = kSide,
       .Postings = kSide,
       .Virtual = true});
  return candidate;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;

  Patchwork candidate = FlatCandidate();
  Yields pad;
  pad.RingEastNorthM = {-100.0, -100.0, 100.0, -100.0, 100.0, 100.0, -100.0, 100.0};
  pad.LowE = -100.0;
  pad.HighE = 100.0;
  pad.LowN = -100.0;
  pad.HighN = 100.0;
  pad.PlateauM = 5.0;
  pad.Fills = true;
  pad.Kind = Stamp::Pad;
  CHECK(pad.HeapBytes() >= pad.RingEastNorthM.size() * sizeof(double),
        "earthwork stamp reports its owned ring capacity");

  const auto pressed = PressTerrain(
      std::span{&pad, 1u}, candidate, TangentFrame::At({}), {.Side = kSide, .Halo = kHalo}, 30.0);
  CHECK(pressed.Nodes == candidate.Sheets[0].Nodes.size() && pressed.Structures == 0 &&
            pressed.Held == 0,
        "every candidate height including its halo is pressed without a renderer");
  CHECK(pressed.Pads.Stamps == 1 && pressed.Pads.Nodes == pressed.Nodes &&
            pressed.Pads.Unreached == 0 && pressed.Corridors.Stamps == 0,
        "pad contact diagnostics describe the same native product");
  CHECK(std::ranges::all_of(candidate.Sheets[0].Nodes,
                            [](float heightM) { return std::abs(heightM - 5.0f) < 0.001f; }),
        "the flat five metre result is written into the private candidate");

  Patchwork slopedCandidate = FlatCandidate();
  Yields corridor = pad;
  corridor.PlateauM = 0.0;
  corridor.SlopeE = 0.05;
  corridor.Kind = Stamp::Corridor;
  const auto sloped = PressTerrain(std::span{&corridor, 1u},
                                   slopedCandidate,
                                   TangentFrame::At({}),
                                   {.Side = kSide, .Halo = kHalo},
                                   30.0);
  CHECK(sloped.Nodes > 0 && sloped.DeepestM > 0.0 && sloped.RaisedM > 0.0,
        "a sloped corridor cuts and fills the flat candidate");
  CHECK(sloped.Corridors.Stamps == 1 &&
            sloped.Corridors.Nodes == slopedCandidate.Sheets[0].Nodes.size() &&
            sloped.Pads.Stamps == 0,
        "corridor diagnostics cover moved and already-level candidate nodes");
  CHECK(sloped.Corridors.AboveM < 0.001 && sloped.Corridors.BelowM < 0.001,
        "corridor contact diagnostics measure the sloped native result");

  const auto retained = candidate.Sheets[0].Nodes;
  const auto invalid = PressTerrain(std::span{&pad, 1u},
                                    candidate,
                                    TangentFrame::At({}),
                                    {.Side = std::numeric_limits<int>::max(), .Halo = 1},
                                    30.0);
  CHECK(invalid.Nodes == 0 && candidate.Sheets[0].Nodes == retained,
        "an unrepresentable page layout leaves the candidate unchanged");
  return Report();
}
