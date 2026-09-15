#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <cmath>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::string_view input = R"(<scenario><placements>
    <place asset="bridge&amp;road" x="1.25" y="-2.5" z="3.75" qx="0" qy="1" qz="0" qw="0" scale="4" scaleY="-2" scaleZ="0"/>
    <place asset="geo" lat="47.5" lon="11.25" heightM="123.5" samplesHeight="yes" bearingDeg="90" pitchDeg="-15" x="6" y="7" z="8" qx="1" qw="0" scale="2"/>
    <place asset="default"/>
  </placements></scenario>)";
  Scenario::Document source;
  std::string error;
  CHECK(ReadScenario(input.data(), input.size(), source, error),
        "independent placement XML imports");
  CHECK(source.Placements.size() == 3, "all placements retained");
  if (source.Placements.size() != 3) { return Report(); }
  CHECK(source.Placements[0].Stands.ScaleXyz[0] == 4 &&
            source.Placements[0].Stands.ScaleXyz[1] == -2 &&
            source.Placements[0].Stands.ScaleXyz[2] == 0,
        "axis scale overrides legacy uniform scale including zero and reflection");
  CHECK(source.Placements[1].Stands.GlobeAnchor && source.Placements[1].Stands.AtM[0] == 6 &&
            source.Placements[1].Stands.Facing.X == 1,
        "geographic intent retains independent local pose");
  CHECK(source.Placements[2].Stands.ScaleXyz[0] == 1 && !source.Placements[2].Stands.GlobeAnchor,
        "default placement remains local and unit-scaled");
  source.Placements[0].Stands.AtM[0] = std::nextafter(6000000.0, 7000000.0);
  source.Placements[1].Stands.Geodetic.HeightM = std::nextafter(123.5, 124.0);
  source.Placements[2].Stands.ScaleXyz[0] = std::nextafter(1.0, 2.0);
  const auto written = WriteScenario(source);
  CHECK(written.has_value(), "placement export succeeds");
  if (!written) { return Report(); }
  Scenario::Document copy;
  CHECK(ReadScenario(written->data(), written->size(), copy, error), "exported placements parse");
  CHECK(copy.Placements.size() == source.Placements.size(),
        "placement count retained through export");
  if (copy.Placements.size() != source.Placements.size()) { return Report(); }
  for (size_t i = 0; i < source.Placements.size(); ++i) {
    const auto &a = source.Placements[i];
    const auto &b = copy.Placements[i];
    CHECK(a.Asset == b.Asset, "asset references and order preserved");
    for (int axis = 0; axis < 3; ++axis) {
      CHECK(a.Stands.AtM[axis] == b.Stands.AtM[axis], "exact local position survives");
      CHECK(a.Stands.ScaleXyz[axis] == b.Stands.ScaleXyz[axis], "exact per-axis scale survives");
    }
    CHECK(a.Stands.Facing.X == b.Stands.Facing.X && a.Stands.Facing.Y == b.Stands.Facing.Y &&
              a.Stands.Facing.Z == b.Stands.Facing.Z && a.Stands.Facing.W == b.Stands.Facing.W,
          "quaternion components survive without normalization");
    CHECK(a.Stands.GlobeAnchor == b.Stands.GlobeAnchor &&
              a.Stands.SamplesHeight == b.Stands.SamplesHeight,
          "geographic placement intent survives");
    CHECK(a.Stands.Geodetic.LongitudeDeg == b.Stands.Geodetic.LongitudeDeg &&
              a.Stands.Geodetic.LatitudeDeg == b.Stands.Geodetic.LatitudeDeg &&
              a.Stands.Geodetic.HeightM == b.Stands.Geodetic.HeightM &&
              a.Stands.BearingDeg == b.Stands.BearingDeg && a.Stands.PitchDeg == b.Stands.PitchDeg,
          "geographic coordinates and angles survive");
  }
  const auto empty = WriteScenario({});
  CHECK(empty && empty->find("<placements>") == std::string::npos,
        "empty placement section remains absent");
  return Report();
}
