/* FlightBox — fb-test-nan-divergence: the negative proof for core/FBFlightMonitor's NUMERICAL_DIVERGENCE
 * K.O. Every other check in FBFlightMonitor::Tick is a COMPARISON, and IEEE-754 makes every comparison
 * against NaN false — so before this check existed, a diverged FDM sailed past all of them and the run
 * ended as an unexplained TIMEOUT (exit 3) with no reason in events.log. That silently voided the
 * "unbestechlicher Monitor" contract (CLAUDE.md "Kein Cheaten"): the judge that cannot see the accident.
 *
 * The divergence is injected at the SAMPLE, not into JSBSim: FBFlightMonitorSample is the monitor's whole
 * input (FBFlightMonitor.h — deliberately fdm-decoupled), so poisoning one field of it is exactly the
 * state a diverged integrator would hand over, with no need to corrupt the pinned, read-only model. Both
 * entry points are proven: a non-finite field, and FBFdm::Faulted() (the latched "the integrator raised"
 * flag the exception firewall in FBFdm.cpp sets).
 *
 * `make test-monitor` builds this -> build/fb-test-nan-divergence. Exit 0 = every injected divergence
 * tripped NUMERICAL_DIVERGENCE and the clean control sample did NOT; exit 1 = a case failed. */
#include "FBFlightMonitor.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include <cmath>
#include <limits>
#include <string>

using namespace FlightBox;

namespace {

/* A perfectly ordinary airborne sample: level, fast, high above terrain, nothing near any threshold —
 * so the ONLY thing a trip below can be attributed to is the injected divergence. */
FBFlightMonitorSample CleanSample() {
  FBFlightMonitorSample s;
  s.LatDeg = 46.84335; s.LonDeg = 6.91523;
  s.ElevM = 3000.0; s.GroundAslM = 441.0;
  s.RollDeg = 2.0; s.PitchDeg = 3.0;
  s.PDegS = 1.0; s.QDegS = 0.5; s.RDegS = 0.2;
  s.VsMs = 5.0; s.TasMs = 200.0;
  s.GearPosNorm = 0.0; s.GearForceLbs = 0.0; s.WeightLbs = 20000.0;
  return s;
}

int gFailures = 0;

/* Feeds ONE poisoned sample into a FRESH monitor (the monitor latches, so each case needs its own) and
 * asserts the verdict. */
void Expect(const char *field, const FBFlightMonitorSample &s, bool wantTrip) {
  FBFlightMonitor monitor;
  bool tripped = monitor.Tick(s, 1.0);
  bool ok = tripped == wantTrip &&
            (!wantTrip || monitor.Reason() == FBKoReason::NumericalDivergence);
  if (!ok) gFailures++;
  FBLog::Info("test", ok ? "case_ok" : "case_FAILED",
              {{"field", field}, {"tripped", tripped}, {"wantTrip", wantTrip},
               {"reason", FBKoReasonStr(monitor.Reason())}, {"detail", monitor.Detail()}});
}

void ExpectDiverged(const char *field, const FBFlightMonitorSample &s) { Expect(field, s, true); }

} // namespace

int main() {
  FBStdoutLogSink sink;
  FBLog::SetSink(&sink);
  FBLog::SetLevel(FBLogLevel::Debug);
  FBLog::SetTime(1.0);

  const double kNan = std::numeric_limits<double>::quiet_NaN();
  const double kInf = std::numeric_limits<double>::infinity();

  Expect("none (control)", CleanSample(), false);

  { auto s = CleanSample(); s.LatDeg = kNan; ExpectDiverged("lat", s); }
  { auto s = CleanSample(); s.LonDeg = kNan; ExpectDiverged("lon", s); }
  { auto s = CleanSample(); s.ElevM = kNan; ExpectDiverged("elevM", s); }
  { auto s = CleanSample(); s.GroundAslM = kNan; ExpectDiverged("groundAslM", s); }
  { auto s = CleanSample(); s.RollDeg = kNan; ExpectDiverged("roll", s); }
  { auto s = CleanSample(); s.PitchDeg = kInf; ExpectDiverged("pitch", s); }
  { auto s = CleanSample(); s.PDegS = kNan; ExpectDiverged("p", s); }
  { auto s = CleanSample(); s.QDegS = kNan; ExpectDiverged("q", s); }
  { auto s = CleanSample(); s.RDegS = kNan; ExpectDiverged("r", s); }
  { auto s = CleanSample(); s.VsMs = kNan; ExpectDiverged("vsMs", s); }
  { auto s = CleanSample(); s.TasMs = kInf; ExpectDiverged("tasMs", s); }
  { auto s = CleanSample(); s.GearPosNorm = kNan; ExpectDiverged("gearPos", s); }
  { auto s = CleanSample(); s.GearForceLbs = kNan; ExpectDiverged("gearForceLbs", s); }
  { auto s = CleanSample(); s.WeightLbs = kNan; ExpectDiverged("weightLbs", s); }
  { auto s = CleanSample(); s.FdmFault = true; ExpectDiverged("fdmFault", s); }

  /* The regression this whole check exists for: an all-NaN sample used to trip NOTHING. Proven here by
   * asserting it now trips, and by the pre-fix behaviour recorded in the task report. */
  { auto s = CleanSample();
    s.LatDeg = s.LonDeg = s.ElevM = s.RollDeg = s.PitchDeg = kNan;
    s.PDegS = s.QDegS = s.RDegS = s.VsMs = s.TasMs = kNan;
    ExpectDiverged("all-NaN pose", s); }

  FBLog::Info("test", "RESULT", {{"result", gFailures == 0 ? "PASS" : "FAIL"}, {"failures", gFailures}});
  return gFailures == 0 ? 0 : 1;
}
