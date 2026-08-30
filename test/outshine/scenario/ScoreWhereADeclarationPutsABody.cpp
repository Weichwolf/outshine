#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <Scenario.h>

#include "Check.h"
#include "ScenarioRead.h"

// WHERE A DECLARATION PUTS A BODY, WHICHEVER WAY IT WAS WRITTEN.
//
// Unreal has ONE `FTransform` -- translation, rotation, scale -- and everything that places
// anything takes it: an actor, a component, an instanced-static-mesh element, an asset import.
// RAGE has `Mat34V`, one matrix type, everywhere. TARGET takes Unreal's answer because a named
// type can be validated, composed, and can say which convention it is in; a loose array can do
// none of the three.
//
// The door carried the same fact FOUR ways (board:1980): `Body` had its own `AtM[3]` beside
// `FacingXyzw[4]`, while `Placement` and `Instance` carried a `Standing`. Four spellings of one
// idea is not four bugs -- it is one bug that shows up wherever two of them meet, and the reader
// spelled the read three separate times to match.
//
// THE ORACLE IS AGREEMENT BETWEEN THE TWO ARRIVAL ROUTES, and it owes nothing to our design.
// CLAUDE.md says the same calls and the same refusal text serve a scenario written in XML and a
// scenario written in C++. So the same placement, written both ways, must land on the same nine
// numbers -- and it must do so for the body, the placement and the instance alike, because that
// is what "one type" MEANS. Written as a comparison rather than as constants: a constant would
// let both routes drift together, and this cannot.

namespace {

constexpr const char *kBoth = R"(<?xml version="1.0" encoding="utf-8"?>
<scenario name="two routes" version="1">
  <world lat="48.0" lon="11.0"/>
  <body name="one" asset="a.gltf" massKg="1000">
    <at x="1.5" y="-2.25" z="3.125" qx="0.5" qy="0.5" qz="0.5" qw="0.5"/>
  </body>
  <instances>
    <instance of="k" id="i" x="1.5" y="-2.25" z="3.125" qx="0.5" qy="0.5" qz="0.5" qw="0.5"
              scale="2.0"/>
  </instances>
</scenario>
)";

[[nodiscard]] outshine::Standing Written() {
  outshine::Standing out;
  out.AtM[0] = 1.5;
  out.AtM[1] = -2.25;
  out.AtM[2] = 3.125;
  out.FacingXyzw[0] = 0.5;
  out.FacingXyzw[1] = 0.5;
  out.FacingXyzw[2] = 0.5;
  out.FacingXyzw[3] = 0.5;
  return out;
}

[[nodiscard]] double Apart(const outshine::Standing &left, const outshine::Standing &right) {
  double most = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    most = std::fmax(most, std::fabs(left.AtM[axis] - right.AtM[axis]));
    most = std::fmax(most, std::fabs(left.ScaleXyz[axis] - right.ScaleXyz[axis]));
  }
  for (int part = 0; part < 4; ++part) {
    most = std::fmax(most, std::fabs(left.FacingXyzw[part] - right.FacingXyzw[part]));
  }
  return most;
}

void Say(const char *what, const outshine::Standing &one) {
  std::printf("  %-22s at %.4f %.4f %.4f  facing %.3f %.3f %.3f %.3f  scale %.3f\n",
              what,
              one.AtM[0],
              one.AtM[1],
              one.AtM[2],
              one.FacingXyzw[0],
              one.FacingXyzw[1],
              one.FacingXyzw[2],
              one.FacingXyzw[3],
              one.ScaleXyz[0]);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Scenario read;
  std::string why;
  if (!outshine::ReadScenario(kBoth, std::char_traits<char>::length(kBoth), read, why)) {
    Unprepared(why.c_str());
    return Report();
  }
  if (read.Bodies.empty() || read.Instances.empty()) {
    Unprepared("the declaration read but stood up no body or no instance");
    return Report();
  }

  const outshine::Standing inCode = Written();
  Say("written in C++", inCode);
  Say("body, read from XML", read.Bodies[0].Stands);
  Say("instance, read from XML", read.Instances[0].Stands);

  // ONE TYPE MEANS THE BODY AND THE INSTANCE AGREE. Before board:1980 the body's placement was
  // its own pair of loose arrays and the instance's was a Standing, so this comparison could not
  // be written at all -- there was nothing on the left of it.
  const double bodyApart = Apart(inCode, read.Bodies[0].Stands);
  std::printf("  body against C++            %.3e\n", bodyApart);
  CHECK(bodyApart == 0.0,
        "**A BODY DECLARED IN XML STANDS WHERE THE SAME BODY DECLARED IN C++ STANDS**: both are "
        "one `Standing`, so agreement is exact rather than close. Any difference at all means a "
        "conversion sits between the two routes, and a conversion is where a convention gets "
        "lost");

  outshine::Standing owed = inCode;
  owed.ScaleXyz[0] = owed.ScaleXyz[1] = owed.ScaleXyz[2] = 2.0;
  const double instanceApart = Apart(owed, read.Instances[0].Stands);
  std::printf("  instance against C++        %.3e\n", instanceApart);
  CHECK(instanceApart == 0.0,
        "**AN INSTANCE READS THE SAME PLACEMENT THE SAME WAY**, scale included. The reader used "
        "to spell this read three separate times; one of the three did not read `scale` at all, "
        "so a declaration meant one thing in one element and another thing in the next");

  // AND AN UNDECLARED TERM DECIDES NOTHING. CLAUDE.md: a section not declared leaves the engine's
  // own default standing, never the zeroes of a struct nobody filled in. The body above declares
  // no scale, so it must read as unit -- not as zero, which would collapse it.
  const outshine::Standing bare;
  std::printf("  body scale, undeclared      %.4f (a bare Standing says %.4f)\n",
              read.Bodies[0].Stands.ScaleXyz[0],
              bare.ScaleXyz[0]);
  CHECK(read.Bodies[0].Stands.ScaleXyz[0] == bare.ScaleXyz[0],
        "**AN UNDECLARED SCALE LEAVES THE DEFAULT STANDING**: a body that says nothing about "
        "scale is unit-scaled, and a reader that wrote zero there would collapse every body that "
        "did not mention it");

  Covers("the door: a body, a placement and an instance carry ONE placement type, so the same "
         "declaration written in C++ and in XML lands on the same numbers and an undeclared term "
         "leaves the default standing");
  return Report();
}
