#include <cmath>
#include <cstdio>
#include <string>

#include <Outshine.h>

#include "Check.h"

// A WORLD STEPS WITH NO PICTURE, AND THE FALL IS THE ORACLE.
//
// Unreal's LoadMap builds a UWorld with no renderer and its commandlets tick it; RAGE streams a
// map without the draw side. Neither treats headless as a degraded mode, and neither should: with
// no frame to pace against, the simulation runs as fast as it can, which is what a dedicated
// server IS. This case asks for no canvas at all.
//
// THE ORACLE IS THE SCHEME, NOT THE CONTINUUM. Semi-implicit (symplectic) Euler advances velocity
// first and then position with the NEW velocity, so after n steps of h under constant g it has
// fallen g*h^2*n*(n+1)/2 -- which is more than the continuum's g*t^2/2 by exactly g*h*t/2. That
// excess is the scheme's, not an error, and stating it is the difference between a test that
// knows what it integrates and one that fudges a tolerance until it passes.

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Engine engine;
  outshine::Scenario declared;
  declared.Ground.Declared = true;
  declared.Ground.GravityMs2 = 9.80665;
  declared.Motion.Declared = true;
  declared.Motion.StepS = 1.0 / 120.0;

  outshine::Body falling;
  falling.Name = "stone";
  falling.MassKg = 1.0;
  falling.Placed = true;
  falling.AtM[1] = 1000.0;
  declared.Bodies.push_back(falling);

  if (!engine.Declare(declared) || !engine.Assemble()) {
    Unprepared(engine.Error().c_str());
    return Report();
  }

  constexpr int kSteps = 240;
  int taken = 0;
  for (; taken < kSteps && engine.Advance(); ++taken) {}

  CHECK(taken == kSteps,
        ("**A WORLD STEPS WITH NO PICTURE**: this case never asks for a canvas, and neither does a "
         "dedicated server or a commandlet. " + std::to_string(taken) + " of " +
         std::to_string(kSteps) + " steps: " + engine.Error())
            .c_str());
  if (taken != kSteps) { return Report(); }

  double fell = 0.0, fast = 0.0;
  for (const outshine::Measure &held : engine.Numbers()) {
    if (held.What == "the first of them, up") { fell = held.How; }
    if (held.What == "and how fast it falls") { fast = held.How; }
  }

  const double stepS = 1.0 / 120.0;
  const double gravity = 9.80665;
  const double owedM = gravity * stepS * stepS * (double)kSteps * (double)(kSteps + 1) / 2.0;
  const double owedMs = -gravity * stepS * (double)kSteps;

  std::printf("HEADLESS  %d steps of %.5f s   fell %.4f m, the scheme owes %.4f m\n", kSteps,
              stepS, 1000.0 - fell, owedM);
  std::printf("          falling at %.4f m/s, the scheme owes %.4f m/s\n", fast, owedMs);

  CHECK_NEAR(1000.0 - fell, owedM, 1.0e-6, "m",
             "and the fall is SEMI-IMPLICIT EULER's, g*h^2*n*(n+1)/2, which exceeds the "
             "continuum's g*t^2/2 by g*h*t/2 -- the scheme falls further because it advances "
             "velocity before position, and a case that did not say so would be tuning a "
             "tolerance rather than knowing what it integrates");
  CHECK_NEAR(fast, owedMs, 1.0e-9, "m/s",
             "and the speed is exactly n steps of g*h, because constant acceleration is the one "
             "case where the scheme's velocity is the continuum's");

  Covers("the door: a world declares, assembles and STEPS with no graphics device, and the fall "
         "it integrates is the semi-implicit Euler scheme's closed form to a micrometre");
  return Report();
}
