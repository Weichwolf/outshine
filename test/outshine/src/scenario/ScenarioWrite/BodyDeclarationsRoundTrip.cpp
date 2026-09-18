#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <limits>
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::string_view input = R"(<scenario>
    <body name="vehicle" asset="mesh&amp;paint" massKg="1250.1234567890123" widthM="2.5">
      <at x="12.345678901234567" y="-2" z="3" qx="0" qy="1" qz="0" qw="0"
        scaleX="2" scaleY="3" scaleZ="4" lat="47.5" lon="11.25" heightM="123.25"
        samplesHeight="yes" bearingDeg="90" pitchDeg="-15"/>
      <centreOfMass x="0.25" y="-0.5" z="1.125"/><inertia ixx="123.45678901234567" iyy="234.5" izz="345.75"/>
      <contact at="wheel&amp;spring" x="1" y="2" z="3" reachM="0.5" stiffnessNPerM="10000"
        dampingNsPerM="1000" travelM="0.2" stopNPerM="20000" limitN="40000" grip="0.8"
        loadFalloff="0.25" radiusM="0.4" corneringNPerRad="15000" relaxationM="0.15"/>
      <actuator does="torque" opposes="1" peakNm="250.125" axisX="1" axisY="2" axisZ="3" ratio="2.25" circleM="12.5"/>
      <actuator does="steer" peakN="100"/><aero dragCoefficient="0.32" frontalM2="2.75"/>
      <slot at="seat&quot;" x="0.1" y="0.2" z="0.3"/>
    </body><body name="template"/></scenario>)";
  Scenario::Document source;
  std::string error;
  CHECK(ReadScenario(input.data(), input.size(), source, error), error.c_str());
  CHECK(source.Bodies.size() == 2, "legacy bodies load");
  if (source.Bodies.size() != 2) { return Report(); }
  CHECK(source.Bodies[0].Placed && !source.Bodies[1].Placed,
        "legacy placement follows at presence");
  CHECK(source.Bodies[0].Driven.size() == 2, "legacy actuators load");
  if (source.Bodies[0].Driven.size() != 2) { return Report(); }
  CHECK(source.Bodies[0].Driven[0].Turns && source.Bodies[0].Driven[0].Opposes &&
            !source.Bodies[0].Driven[1].Turns,
        "legacy torque opposition and linear mode are retained");
  source.Bodies[1] = source.Bodies[0];
  source.Bodies[1].Name = "template\t\n\r<&";
  source.Bodies[1].Placed = false;
  source.Bodies[1].Driven[1].PeakN = 0;
  source.Bodies[1].Driven[1].Opposes = true;
  const auto written = WriteScenario(source);
  CHECK(written.has_value(), "body export succeeds");
  if (!written) { return Report(); }
  Scenario::Document copy;
  CHECK(ReadScenario(written->data(), written->size(), copy, error), error.c_str());
  CHECK(copy.Bodies.size() == 2, "all bodies survive export");
  if (copy.Bodies.size() != 2) { return Report(); }
  for (size_t i = 0; i < source.Bodies.size(); ++i) {
    const auto &a = source.Bodies[i];
    const auto &b = copy.Bodies[i];
    CHECK(a.Name == b.Name && a.Asset == b.Asset && a.Placed == b.Placed,
          "identity and activation survive independently of stored pose");
    CHECK(a.MassKg == b.MassKg && a.WidthM == b.WidthM && a.DragCoefficient == b.DragCoefficient &&
              a.FrontalM2 == b.FrontalM2,
          "body scalar parameters survive");
    for (int axis = 0; axis < 3; ++axis) {
      CHECK(a.CentreOfMassM[axis] == b.CentreOfMassM[axis] &&
                a.InertiaKgM2[axis] == b.InertiaKgM2[axis] &&
                a.Stands.AtM[axis] == b.Stands.AtM[axis] &&
                a.Stands.ScaleXyz[axis] == b.Stands.ScaleXyz[axis],
            "body mass distribution and pose retain exact doubles");
    }
    CHECK(a.Stands.Facing.X == b.Stands.Facing.X && a.Stands.Facing.Y == b.Stands.Facing.Y &&
              a.Stands.Facing.Z == b.Stands.Facing.Z && a.Stands.Facing.W == b.Stands.Facing.W,
          "orientation survives");
    CHECK(a.Stands.GlobeAnchor == b.Stands.GlobeAnchor &&
              a.Stands.SamplesHeight == b.Stands.SamplesHeight &&
              a.Stands.Geodetic.LatitudeDeg == b.Stands.Geodetic.LatitudeDeg &&
              a.Stands.Geodetic.LongitudeDeg == b.Stands.Geodetic.LongitudeDeg &&
              a.Stands.Geodetic.HeightM == b.Stands.Geodetic.HeightM &&
              a.Stands.BearingDeg == b.Stands.BearingDeg && a.Stands.PitchDeg == b.Stands.PitchDeg,
          "geographic metadata survives even on an inactive body");
    CHECK(a.Contacts.size() == b.Contacts.size() && a.Driven.size() == b.Driven.size() &&
              a.Slots.size() == b.Slots.size(),
          "body component counts survive");
    if (b.Contacts.size() != 1 || b.Driven.size() != 2 || b.Slots.size() != 1) { continue; }
    const auto &v = a.Contacts[0];
    const auto &w = b.Contacts[0];
    CHECK(v.At == w.At && v.Strut.ReachM == w.Strut.ReachM &&
              v.Strut.StiffnessNPerM == w.Strut.StiffnessNPerM &&
              v.Strut.DampingNsPerM == w.Strut.DampingNsPerM &&
              v.Strut.TravelM == w.Strut.TravelM &&
              v.Strut.StopStiffnessNPerM == w.Strut.StopStiffnessNPerM &&
              v.Strut.LoadLimitN == w.Strut.LoadLimitN && v.Touches.Grip == w.Touches.Grip &&
              v.Touches.LoadFalloff == w.Touches.LoadFalloff &&
              v.Touches.RadiusM == w.Touches.RadiusM &&
              v.Touches.CorneringNPerRad == w.Touches.CorneringNPerRad &&
              v.Touches.RelaxationM == w.Touches.RelaxationM,
          "contact and suspension parameters survive");
    CHECK(a.Slots[0].At == b.Slots[0].At, "slot name survives");
    for (int axis = 0; axis < 3; ++axis) {
      CHECK(v.AtM[axis] == w.AtM[axis] && a.Slots[0].AtM[axis] == b.Slots[0].AtM[axis],
            "contact and slot positions survive");
    }
    for (size_t j = 0; j < a.Driven.size(); ++j) {
      const auto &d = a.Driven[j];
      const auto &e = b.Driven[j];
      CHECK(d.Does == e.Does && d.Opposes == e.Opposes && d.Turns == e.Turns &&
                d.PeakNm == e.PeakNm && d.PeakN == e.PeakN && d.Ratio == e.Ratio &&
                d.CircleM == e.CircleM,
            "actuator mode is independent of zero magnitude");
      for (int axis = 0; axis < 3; ++axis) {
        CHECK(d.AxisXyz[axis] == e.AxisXyz[axis], "actuator axis survives");
      }
    }
  }
  source.Bodies[0].MassKg = std::numeric_limits<double>::quiet_NaN();
  CHECK(!WriteScenario(source), "export rejects invalid body dynamics");
  source.Bodies[0].MassKg = 1;
  source.Bodies[0].Driven[0].Does = static_cast<Scenario::Drives>(255);
  CHECK(!WriteScenario(source), "export rejects unknown drive category");
  source.Bodies[0].Driven[0].Does = Scenario::Drives::Effort;
  source.Bodies[0].Driven[0].PeakN = 1;
  CHECK(!WriteScenario(source), "export rejects simultaneous force and torque");
  for (const std::string_view attribute :
       {"assetSpanM=\"4\"", "assetGround=\"-1\"", "assetCentreX=\"0\"", "assetCentreZ=\"0\""}) {
    const std::string retired =
        "<scenario><body name=\"legacy\" " + std::string(attribute) + "/></scenario>";
    CHECK(!ReadScenario(retired.data(), retired.size(), source, error),
          "retired asset fitting cannot silently alter a body declaration");
  }
  constexpr std::string_view explicitActivation =
      R"(<scenario><body name="origin" placed="yes"/></scenario>)";
  Scenario::Document activated;
  CHECK(ReadScenario(explicitActivation.data(), explicitActivation.size(), activated, error),
        error.c_str());
  CHECK(activated.Bodies.size() == 1 && activated.Bodies[0].Placed &&
            activated.Bodies[0].Stands.AtM[0] == 0,
        "explicit activation without pose uses the standard origin");
  return Report();
}
