/* fb-test-nan-divergence: the negative proof for FBFlightMonitor's NUMERICAL_DIVERGENCE K.O. Every
 * OTHER check in the monitor is a comparison, and IEEE-754 makes every comparison against NaN false — so
 * without this check a diverged FDM sails past all of them and the run ends as an unexplained TIMEOUT
 * with no reason logged: the judge that cannot see the accident.
 * Injected at the SAMPLE, not into JSBSim: the sample IS the monitor's whole input, so poisoning a field
 * of it is exactly the state a diverged integrator hands over. Both entry points are proven: a
 * non-finite field, and the latched Faulted() flag. Exit 0 = every case tripped and the clean control
 * sample did not; 1 = a case failed. */
#include "FBFlightMonitor.h"
#include "FBLog.h"
#include "FBLogSinks.h"
#include <cmath>
#include <limits>
#include <string>

using namespace FlightBox;

namespace {

/* Nothing near any threshold, so the ONLY thing a trip can be attributed to is the injection. */
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

/* A FRESH monitor per case: it latches. */
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

  /* The regression this check exists for: an all-NaN sample used to trip NOTHING. */
  { auto s = CleanSample();
    s.LatDeg = s.LonDeg = s.ElevM = s.RollDeg = s.PitchDeg = kNan;
    s.PDegS = s.QDegS = s.RDegS = s.VsMs = s.TasMs = kNan;
    ExpectDiverged("all-NaN pose", s); }

  FBLog::Info("test", "RESULT", {{"result", gFailures == 0 ? "PASS" : "FAIL"}, {"failures", gFailures}});
  return gFailures == 0 ? 0 : 1;
}
