#include <cstdio>
#include <string>
#include <vector>

#include <Scenario.h>

#include "Check.h"
#include "Rigging.h"

// WHAT A BODY THE DECLARATION CANNOT SATISFY DOES, WHICH IS REFUSE AND SAY WHICH TERM.
//
// JSBSim is the quality bar for this shape and it is worth saying why: an aircraft is declared in
// XML -- mass and balance, an inertia tensor, ground reactions as spring/damper contact points,
// propulsion, aerodynamic coefficient tables -- and the simulator ASSEMBLES it. No aircraft is
// hard-coded, which is how one program flies a Cessna and a 737. Unreal keeps the same shape
// (a wheeled vehicle is a plugin's data asset) and RAGE keeps handling data in the game layer.
// All three agree: the machine is DATA and the engine assembles it.
//
// A declared assembler owes something a hard-coded one does not: when the data cannot be
// assembled, it must say WHICH term made it impossible. A refusal without the term is a bug
// report the author cannot act on, and silence is worse -- a rig that stands with a zero in it
// integrates happily and produces motion nobody can explain.
//
// THE ORACLE IS THE TERM'S OWN NAME appearing in the refusal, which owes nothing to our wording:
// whatever sentence the engine chooses, it has to contain the thing the author must fix. Each
// case below removes exactly ONE term from a body that otherwise stands, so what is being read is
// that term and not a side effect.
//
// WIDTH IS NOT ON THE LIST AND THE REASON IS WORTH KEEPING. `Sim::Stand` accepts a body whose
// width is zero, and that is correct rather than a hole: the STANCE comes from the contacts --
// patches at x = +-0.774 ARE a 1.548 m track (board:1897) -- so nothing in the rig needs a
// separately declared width. What needs it is the DRIVE, which fits a body into a corridor, and
// `DriveAssembly` refuses there with "the body declares no width". Two refusals at two levels,
// each where the number is actually used, and a case that put them in one place would be
// asserting a layering the tree does not have.

namespace {

constexpr double kGravityMs2 = 9.80665;
constexpr double kAirDensityKgM3 = 1.225;

[[nodiscard]] outshine::Contact Standing(double xM, double zM) {
  outshine::Contact one;
  one.At = zM < 0.0 ? "front" : "rear";
  one.AtM[0] = xM;
  one.AtM[1] = 0.333;
  one.AtM[2] = zM;
  one.Strut.ReachM = 0.456;
  one.Strut.StiffnessNPerM = 32000.0;
  one.Strut.DampingNsPerM = 3400.0;
  one.Strut.TravelM = 0.18;
  one.Strut.StopNPerM = 450000.0;
  one.Strut.LimitN = 24000.0;
  one.Touches.Grip = 0.95;
  one.Touches.RadiusM = 0.333;
  one.Touches.CorneringNPerRad = 55000.0;
  one.Touches.RelaxationM = 0.4;
  return one;
}

[[nodiscard]] outshine::Body Whole(void) {
  outshine::Body made;
  made.Name = "one";
  made.MassKg = 1610.0;
  made.WidthM = 1.811;
  made.AssetSpanM = 2.810;
  made.CentreOfMassM[1] = 0.55;
  made.InertiaKgM2[0] = 540.0;
  made.InertiaKgM2[1] = 2400.0;
  made.InertiaKgM2[2] = 2600.0;
  made.DragCoefficient = 0.66;
  made.FrontalM2 = 2.19;
  outshine::Drive turns;
  turns.Does = outshine::Drives::Motion;
  turns.CircleM = 11.3;
  made.Driven.push_back(turns);
  outshine::Drive pushes;
  pushes.Does = outshine::Drives::Effort;
  pushes.PeakNm = 400.0;
  pushes.Ratio = 3.08;
  made.Driven.push_back(pushes);
  outshine::Drive slows;
  slows.Does = outshine::Drives::Effort;
  slows.Opposes = true;
  slows.PeakNm = 5500.0;
  made.Driven.push_back(slows);
  made.Contacts.push_back(Standing(-0.774, -1.405));
  made.Contacts.push_back(Standing(0.774, -1.405));
  made.Contacts.push_back(Standing(-0.774, 1.405));
  made.Contacts.push_back(Standing(0.774, 1.405));
  return made;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const outshine::Sim::Rigged whole = outshine::Sim::Stand(Whole(), kGravityMs2, kAirDensityKgM3);
  std::printf("  a body carrying every term      %s\n",
              whole.Stood ? "STANDS" : whole.Error.c_str());
  CHECK(whole.Stood,
        "**A BODY CARRYING EVERY TERM STANDS**: the refusals below mean nothing unless this one "
        "passes, because a rig that refuses everything refuses correctly for the wrong reason");

  for (const auto &[what, term, bend] :
       std::vector<std::tuple<const char *, const char *, void (*)(outshine::Body &)>>{
           {"no mass", "mass", [](outshine::Body &one) { one.MassKg = 0.0; }},
           {"nothing that opposes", "slow", [](outshine::Body &one) { one.Driven.pop_back(); }},
           {"no frontal area", "frontal", [](outshine::Body &one) { one.FrontalM2 = 0.0; }},
           {"a steering circle inside its own stance",
            "circle",
            [](outshine::Body &one) { one.Driven[0].CircleM = 1.0; }},
           {"no contacts", "contact", [](outshine::Body &one) { one.Contacts.clear(); }}}) {
    outshine::Body bent = Whole();
    bend(bent);
    const outshine::Sim::Rigged stood = outshine::Sim::Stand(bent, kGravityMs2, kAirDensityKgM3);
    const bool named = !stood.Stood && stood.Error.find(term) != std::string::npos;
    std::printf(
        "  %-30s %s\n", what, stood.Stood ? "STOOD ANYWAY" : stood.Error.substr(0, 78).c_str());
    CHECK(named,
          "**A BODY THE DECLARATION CANNOT SATISFY IS REFUSED, AND THE REFUSAL NAMES THE TERM**: "
          "an author reading it has to learn which number to fix, and a rig that stood with a "
          "zero in it would integrate happily and produce motion nobody could explain");
  }

  Covers("assembly: a body declared with every term stands, and one missing its mass, its "
         "braking, its frontal area, its steering circle or its contacts is refused with that "
         "term's own name in the reason -- so the declaration is the specification and the "
         "engine says exactly where it falls short");
  return Report();
}
