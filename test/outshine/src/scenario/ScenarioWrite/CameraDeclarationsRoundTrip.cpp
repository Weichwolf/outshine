#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <limits>
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::string_view input = R"(<scenario><views>
    <view id="local" fovDeg="65" nearM="0.125" farM="1500"><at x="1" y="2" z="3"/><lookAt x="4" y="5" z="6"/><up x="0" y="0" z="1"/></view>
    <view id="geo" orthographic="yes" xMagM="20" yMagM="15" nearM="0" farM="100"><at lat="47.5" lon="11.25" heightM="120"/></view>
    <view id="follow" follows="actor" person="third" distanceM="4.5" risesBy="0.25"/>
  </views></scenario>)";
  Scenario::Document source;
  std::string error;
  CHECK(ReadScenario(input.data(), input.size(), source, error), error.c_str());
  CHECK(source.Views.size() == 3, "legacy camera declarations load");
  if (source.Views.size() != 3) { return Report(); }
  CHECK(source.Views[0].Placement == Scenario::CameraPlacement::Local &&
            source.Views[0].Sees.LooksAt && source.Views[0].Sees.NearM == 0.125 &&
            source.Views[0].Sees.UpM[2] == 1,
        "legacy local target and clipping retain specified meaning");
  CHECK(source.Views[1].Placement == Scenario::CameraPlacement::Geodetic &&
            source.Views[1].Sees.Orthographic &&
            source.Views[2].Placement == Scenario::CameraPlacement::FollowEntity &&
            source.Views[2].DistanceM == 4.5,
        "legacy geographic and follow modes remain distinct");
  source.Views[0].Sees.FarM = std::numeric_limits<double>::infinity();
  for (auto &view : source.Views) {
    view.In = "scene&<\"'\t\n\r";
    view.Viewport = {
        .LeftFrac = 0.12345678901234567, .TopFrac = 0.2, .WidthFrac = 0.5, .HeightFrac = 0.6};
    view.Sees.PositionM = {1.2345678901234567, 2, 3};
    view.Sees.Orientation = {.X = 0, .Y = 1, .Z = 0, .W = 0};
    view.Sees.ApertureFStops = 2.8;
    view.Sees.ShutterS = 1.0 / 123.0;
    view.Sees.SensitivityIso = 320;
    view.Sees.LookAtM = {4, 5, 6};
    view.Sees.UpM = {0, 1, 0};
    view.OffsetM = {0.25, 0.5, 0.75};
    view.PitchLimitDeg = 78.5;
    view.TimeScale = 1.25;
    view.Geographic.Geodetic = {
        .LongitudeDeg = 11.25, .LatitudeDeg = 47.5, .HeightM = 123.45678901234567};
    view.Geographic.SamplesHeight = true;
    view.Geographic.BearingDeg = 125.5;
    view.Geographic.PitchDeg = -12.25;
  }
  const auto written = WriteScenario(source);
  CHECK(written.has_value(), "camera catalog exports");
  if (!written) { return Report(); }
  Scenario::Document copy;
  CHECK(ReadScenario(written->data(), written->size(), copy, error), error.c_str());
  CHECK(copy.Views.size() == 3, "all views survive");
  if (copy.Views.size() != 3) { return Report(); }
  for (size_t i = 0; i < source.Views.size(); ++i) {
    const auto &a = source.Views[i];
    const auto &b = copy.Views[i];
    CHECK(a.Id == b.Id && a.In == b.In && a.Follows == b.Follows && a.Person == b.Person &&
              a.Placement == b.Placement,
          "identity metadata and mode survive independently of stored pose");
    CHECK(a.DistanceM == b.DistanceM && a.RisesBy == b.RisesBy &&
              a.PitchLimitDeg == b.PitchLimitDeg && a.TimeScale == b.TimeScale,
          "follow and timing parameters survive");
    CHECK(a.Viewport.LeftFrac == b.Viewport.LeftFrac && a.Viewport.TopFrac == b.Viewport.TopFrac &&
              a.Viewport.WidthFrac == b.Viewport.WidthFrac &&
              a.Viewport.HeightFrac == b.Viewport.HeightFrac,
          "viewport metadata survives exactly");
    const auto &v = a.Sees;
    const auto &w = b.Sees;
    CHECK(v.FovDeg == w.FovDeg && v.NearM == w.NearM && v.FarM == w.FarM &&
              v.Orthographic == w.Orthographic && v.XMagM == w.XMagM && v.YMagM == w.YMagM,
          "both projections and infinite perspective retain exact settings");
    CHECK(v.ApertureFStops == w.ApertureFStops && v.ShutterS == w.ShutterS &&
              v.SensitivityIso == w.SensitivityIso,
          "photographic exposure survives exactly");
    CHECK(v.LooksAt == w.LooksAt && v.Orientation.X == w.Orientation.X &&
              v.Orientation.Y == w.Orientation.Y && v.Orientation.Z == w.Orientation.Z &&
              v.Orientation.W == w.Orientation.W,
          "stored target does not activate look-at or discard orientation");
    for (int axis = 0; axis < 3; ++axis) {
      CHECK(v.PositionM[axis] == w.PositionM[axis] && v.LookAtM[axis] == w.LookAtM[axis] &&
                v.UpM[axis] == w.UpM[axis] && a.OffsetM[axis] == b.OffsetM[axis],
            "all camera vectors survive exactly");
    }
    CHECK(a.Geographic.Geodetic.LongitudeDeg == b.Geographic.Geodetic.LongitudeDeg &&
              a.Geographic.Geodetic.LatitudeDeg == b.Geographic.Geodetic.LatitudeDeg &&
              a.Geographic.Geodetic.HeightM == b.Geographic.Geodetic.HeightM &&
              a.Geographic.SamplesHeight == b.Geographic.SamplesHeight &&
              a.Geographic.BearingDeg == b.Geographic.BearingDeg &&
              a.Geographic.PitchDeg == b.Geographic.PitchDeg,
          "geographic metadata survives in every placement mode");
  }
  source.Views[0].Placement = static_cast<Scenario::CameraPlacement>(255);
  CHECK(!WriteScenario(source), "unknown native mode rejects export");
  constexpr std::string_view invalid =
      R"(<scenario><views><view id="bad" placement="unknown"/></views></scenario>)";
  CHECK(!ReadScenario(invalid.data(), invalid.size(), copy, error),
        "unknown XML mode rejects import");
  CHECK(copy.Views.size() == 3 && copy.Views[0].Id == "local",
        "rejected mode preserves previous declaration");
  return Report();
}
