#include "FBMissileUplink.h"
#include "FBGeodesy.h"
#include "FBUnitRegistry.h"
#include "FBUnit.h"
#include "FBUnits.h"
#include <cmath>
#include <cstring>

namespace FlightBox {

void FBMissileUplink::Run(FBState &state, const fb_fdm_state &st, const FBUnitRegistry *net,
                          double simTimeS) {
  FBDatalinkBlock &b = state.Datalink;
  b.Powered = Powered();
  b.Transmitting = false;   /* a missile's terminal is a receiver: it answers nobody */
  b.TrackCount = 0;

  FBWeaponUplink up{};
  if (Powered() && net && LauncherId_ != 0) {
    for (const FBUnit *u : net->Units()) {
      if (!u || u->GetId() != LauncherId_) continue;
      /* By VALUE: what this receiver has heard must outlive the expression that read it. */
      up = u->GetSignature().Uplink;
      if (up.Active && up.LauncherId == LauncherId_ && up.Target.Valid) b.TrackCount = 1;
      break;   /* one launcher, one entry — nothing else in the registry is looked at */
    }
  }
  if (b.TrackCount == 0) {
    /* Nothing received: not an empty picture but NO picture, the state the guidance's timeout runs on. */
    b.H.Invalidate();
    return;
  }

  /* Range/bearing are the RECEIVER's own arithmetic on the reported position. */
  FBDatalinkTrack &t = b.Tracks[0];
  t = FBDatalinkTrack{};
  std::strncpy(t.Callsign, "UPLINK", sizeof t.Callsign - 1);
  t.LatDeg = up.Target.LatDeg;
  t.LonDeg = up.Target.LonDeg;
  t.AltM = (float)up.Target.AltM;
  double speed = std::sqrt(up.Target.VelE * up.Target.VelE + up.Target.VelN * up.Target.VelN);
  t.SpeedMs = (float)speed;
  t.HeadingDeg = (float)(speed > 0.1 ? std::atan2(up.Target.VelE, up.Target.VelN) * kRad2Deg : 0.0);
  t.RangeM = (float)FBPlanarDistM(st.lat, st.lon, t.LatDeg, t.LonDeg);
  t.BearingDeg = (float)FBBearingDeg(st.lat, st.lon, t.LatDeg, t.LonDeg);
  /* The stamp is the LAUNCHER'S look, not the moment this arrived: its age is what the round flies on. */
  t.ReportTimeS = (float)up.Target.StampS;
  t.AgeS = (float)(simTimeS - up.Target.StampS);
  b.H.Publish(simTimeS);
}

} // namespace FlightBox
