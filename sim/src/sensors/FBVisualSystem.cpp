#include "FBVisualSystem.h"
#include "FBCamera.h"
#include "FBEphemeris.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBRadarSystem.h"
#include "FBUnit.h"
#include "FBUnitRegistry.h"
#include <cmath>
#include <cstring>

namespace FlightBox::Sensors {

namespace {

void CopyTypeName(char *dst, const char *src) {
  std::size_t n = 0;
  while (n + 1 < (std::size_t)kVisualTypeNameLen && src[n]) { dst[n] = src[n]; n++; }
  dst[n] = '\0';
}

} // namespace

void FBVisualSystem::SetCone(double azHalfDeg, double elHalfDeg) {
  Field_.AzHalfDeg = azHalfDeg > 0.0 ? azHalfDeg : 0.0;
  Field_.ElHalfDeg = elHalfDeg > 0.0 ? elHalfDeg : 0.0;
}

/* THE MARCH — the price doc/sensors.md §6.5 named and the IRST declined, paid here because the whole
 * point of this channel is that an eye does not see through cloud, and a LID over-reports blindness at
 * exactly the coverage a broken deck has. core/FBCloudDensity is a closed-form field; walking it gives
 * a real transmittance, so a 40 % deck is mostly hole and the holes are where the field says they are.
 *
 * Bounded by construction, which is why the price is affordable here and was not for a 50 nm radar:
 * only the segment each deck's own altitude band cuts out of the line of sight is walked (at most three
 * such segments), the line of sight is a few km long, and the look runs at 1 Hz over <= 8 contacts. */
double FBVisualSystem::CloudTransmittance(const Fdm::fb_fdm_state &own,
                                          const Units::FBUnitPose &tgt) const {
  if (!Sky_.Any()) return 1.0;
  double e0 = 0.0, n0 = 0.0, e1 = 0.0, n1 = 0.0;
  FBEnuOffsetM(Sky_.AnchorLatDeg, Sky_.AnchorLonDeg, own.lat, own.lon, e0, n0);
  FBEnuOffsetM(Sky_.AnchorLatDeg, Sky_.AnchorLonDeg, tgt.LatDeg, tgt.LonDeg, e1, n1);
  const double a0 = own.elev, a1 = tgt.ElevM;
  const double lenM = std::sqrt((e1 - e0) * (e1 - e0) + (n1 - n0) * (n1 - n0) + (a1 - a0) * (a1 - a0));
  if (lenM < 1.0) return 1.0;

  double od = 0.0;
  for (int i = 0; i < 3; i++) {
    const FBCloudDeckParams &d = Sky_.Deck[i];
    if (d.Cover <= 0.0f || d.SigmaPerM <= 0.0f || d.ThicknessM() <= 0.0f) continue;
    /* The parameter window in which the straight segment's ALTITUDE lies inside this deck. */
    double t0 = 0.0, t1 = 1.0;
    if (std::fabs(a1 - a0) < 1e-6) {
      if (a0 < (double)d.BaseM || a0 > (double)d.TopM) continue;
    } else {
      double tb = ((double)d.BaseM - a0) / (a1 - a0);
      double tt = ((double)d.TopM - a0) / (a1 - a0);
      t0 = tb < tt ? tb : tt;
      t1 = tb < tt ? tt : tb;
      if (t0 < 0.0) t0 = 0.0;
      if (t1 > 1.0) t1 = 1.0;
      if (t1 <= t0) continue;
    }
    const double ds = (t1 - t0) * lenM / (double)kCloudMarchSteps;
    double dens = 0.0;
    for (int k = 0; k < kCloudMarchSteps; k++) {
      const double t = t0 + (t1 - t0) * ((double)k + 0.5) / (double)kCloudMarchSteps;
      const double alt = a0 + t * (a1 - a0);
      const float h = (float)((alt - (double)d.BaseM) / (double)d.ThicknessM());
      dens += (double)FBCloudDensity(d, (float)(e0 + t * (e1 - e0)), (float)(n0 + t * (n1 - n0)), h);
    }
    od += (double)d.SigmaPerM * dens * ds;
  }
  return std::exp(-od);
}

/* DISABILITY GLARE, and it is its own term rather than a multiplier on the general daylight one because
 * it is a different mechanism: intraocular scatter raises the BACKGROUND, and contrast is measured
 * against the background. Stiles-Holladay with the two unknown luminances collapsed into one readable
 * setting — see kGlareHalfAngleDeg. */
double FBVisualSystem::GlareFactor(const FBState &state, double bearingDeg,
                                   double elevAngleDeg) const {
  if (state.Env.H.Status == FBBlockStatus::Invalid) return 1.0;   /* no clock -> no sun to look into */
  if (state.Env.SunElDeg <= 0.0f) return 1.0;                     /* below the horizon: nothing to dazzle */
  const double e1 = elevAngleDeg * kDeg2Rad, e2 = (double)state.Env.SunElDeg * kDeg2Rad;
  const double dAz = (bearingDeg - (double)state.Env.SunAzDeg) * kDeg2Rad;
  double c = std::sin(e1) * std::sin(e2) + std::cos(e1) * std::cos(e2) * std::cos(dAz);
  if (c > 1.0) c = 1.0;
  if (c < -1.0) c = -1.0;
  const double thetaDeg = std::acos(c) * kRad2Deg;
  if (thetaDeg <= kSunBlindConeDeg) return 0.0;
  return 1.0 / (1.0 + kGlareHalfAngleDeg * kGlareHalfAngleDeg / (thetaDeg * thetaDeg));
}

void FBVisualSystem::ScanFrame(const FBState &state, const Fdm::fb_fdm_state &st,
                               const Units::FBUnitRegistry &net, double simTimeS) {
  bool seen[kMaxVisualContacts] = {};
  int maskedIds[kMaxVisualContacts] = {};
  int maskedCount = 0;
  double glareMin = 1.0;

  /* The horizon does not sit at zero elevation aloft: it drops by acos(R/(R+h)), and whether a contact
   * is against SKY or against terrain clutter is decided against that line. core/FBCamera's dip, the
   * same one the conformal HUD draws with — one horizon in the tree, not two. */
  const double dipDeg = (double)FBHorizonDipRad((float)st.elev) * kRad2Deg;
  const double dayF = state.Env.H.Status == FBBlockStatus::Invalid
                          ? 1.0
                          : FBDaylightFactor((double)state.Env.SunElDeg);
  const double visM = Sky_.VisibilityM > 1.0f ? (double)Sky_.VisibilityM : 1.0;
  const double thetaDetectRad = kDetectThresholdDeg * kDeg2Rad;
  const double contrastFloor = kContrastLookUp / kContrastPenaltyMax;

  for (const Units::FBUnit *u : net.Units()) {
    if (!u || u->GetId() == SelfId_) continue;
    /* KEIN ARTFILTER. Ein AUGE hat keinen Modus — es sieht, was da ist, und die Frage, OB etwas zu
     * sehen ist, beantwortet die naechste Zeile bereits besser: wer keine Ausdehnung deklariert, ist
     * unsichtbar. Bis 2026-08-05 stand hier `GetKind() != Aircraft`, mit der Begruendung des Radars
     * uebernommen — beim RADAR traegt sie (ein Luft-Luft-FCR gibt wirklich keine Bodenrueckstreuung
     * als Kontakt aus), beim Auge nicht. Der Unterschied kostete nichts und schloss alles Nicht-
     * Fliegende von der Wahrnehmung aus. [MESS] die Loeschung bewegt heute NICHTS: jedes Bodenziel
     * fuehrt FrontalExtentM = 0 und faellt an der naechsten Zeile heraus; sichtbar wird eine Einheit
     * erst, wenn ihr Katalog Masze DEKLARIERT. Damit ist Sichtbarkeit eine Aussage des Objekts ueber
     * sich, nicht eine Aussage des Sensors ueber Arten. */
    const Units::FBUnitSignature sig = u->GetSignature();
    const Units::FBVisualSignature &vs = sig.Visual;
    if (vs.FrontalM <= 0.0f && vs.LateralM <= 0.0f && vs.PlanM <= 0.0f) continue;   /* nothing to see */

    const Units::FBUnitPose p = u->GetPose();
    double rangeM = 0.0, bearingDeg = 0.0, elevAngleDeg = 0.0, azDeg = 0.0, elDeg = 0.0;
    FBRadarSystem::RelativeLos(st, p.LatDeg, p.LonDeg, p.ElevM, rangeM, bearingDeg, elevAngleDeg,
                               azDeg, elDeg);

    int slot = -1;
    for (int i = 0; i < TrackCount_; i++)
      if (Tracks_[i].UnitId == u->GetId()) { slot = i; break; }

    /* THE CONE, body-referenced and fixed: no slew, no auto-acquire, and nothing is ever seen outside
     * it at any range. Uniform inside — a real detection probability falls with eccentricity, which is
     * §9.8's one place where this model is generous, named rather than approximated. */
    const bool inCone = std::fabs(azDeg) <= Field_.AzHalfDeg && std::fabs(elDeg) <= Field_.ElHalfDeg;

    bool detected = false;
    bool cloudRejected = false;
    double thetaObs = 0.0, thetaReq = 0.0, cEff = 0.0, maskedT = 1.0;
    if (inCone && rangeM > 1.0) {
      /* THE PRESENTED EXTENT. The line of sight expressed in the TARGET's body frame is what decides
       * which silhouette it shows — a fighter head-on is worth a fraction of the same fighter side-on,
       * and here that is a consequence of the geometry rather than a coefficient. */
      double te = 0.0, tn = 0.0;
      FBEnuOffsetM(p.LatDeg, p.LonDeg, st.lat, st.lon, te, tn);
      double fwd = 0.0, right = 0.0, down = 0.0;
      FBEnuToBodyVec(p.RollDeg, p.PitchDeg, p.YawDeg, te, tn, st.elev - p.ElevM, fwd, right, down);
      const double extentM = (double)FBPresentedDimensionM(vs, fwd, right, down);
      thetaObs = extentM / rangeM;

      const double c0 = elevAngleDeg > -dipDeg ? kContrastLookUp : kContrastLookDown;
      const double zEff = 0.5 * (st.elev + p.ElevM);
      const double sigma = kKoschmieder / visM * std::exp(-(zEff > 0.0 ? zEff : 0.0) / kHazeScaleHM);
      const double tHaze = std::exp(-sigma * rangeM);
      const double tCloud = CloudTransmittance(st, p);
      const double glare = GlareFactor(state, bearingDeg, elevAngleDeg);
      if (glare < glareMin) glareMin = glare;
      /* The clear-line-of-sight contrast is carried alongside rather than divided back out of cEff:
       * an optically closed deck drives the transmittance to zero, and the MASKED counterfactual below
       * must not have to divide by it. */
      const double cClear = c0 * tHaze * dayF * glare;
      cEff = cClear * tCloud;

      if (cEff > contrastFloor) {
        thetaReq = thetaDetectRad * (kContrastLookUp / cEff);
        detected = thetaObs >= thetaReq;
      }
      /* MASKED is the counterfactual, and it is only counted where the cloud is what DECIDED the
       * outcome: the same air without the deck would have shown this target. Anything else would tally
       * sources that were never going to be seen — the rule FBIrstSystem's mask counter follows. */
      if (!detected && tCloud < 1.0) {
        cloudRejected = cClear > contrastFloor &&
                        thetaObs >= thetaDetectRad * (kContrastLookUp / cClear);
        maskedT = tCloud;
      }
    }
    if (cloudRejected && maskedCount < kMaxVisualContacts) {
      bool wasMasked = false;
      for (int i = 0; i < MaskedIdCount_; i++)
        if (MaskedIds_[i] == u->GetId()) { wasMasked = true; break; }
      if (!wasMasked)
        FBLog::Info("vis", "MASKED", {{"sizeMrad", thetaObs * 1000.0}, {"transmittance", maskedT},
            {"azDeg", azDeg}, {"elDeg", elDeg}});
      maskedIds[maskedCount++] = u->GetId();
    }

    if (!detected) {
      if (slot >= 0) {
        Track &t = Tracks_[slot];
        t.Masked = cloudRejected;
        if (!t.Firm) {   /* an unconfirmed file that stops being seen never existed */
          for (int j = slot; j < TrackCount_ - 1; j++) Tracks_[j] = Tracks_[j + 1];
          TrackCount_--;
          for (int j = slot; j < TrackCount_; j++) seen[j] = seen[j + 1];
        }
      }
      continue;
    }

    if (slot < 0) {
      if (TrackCount_ >= kMaxVisualContacts) continue;
      slot = TrackCount_++;
      Tracks_[slot] = Track{};
      Tracks_[slot].UnitId = u->GetId();
    }
    Track &t = Tracks_[slot];
    t.BearingDeg = bearingDeg;
    t.ElevAngleDeg = elevAngleDeg;
    t.AzDeg = azDeg;
    t.ElDeg = elDeg;
    t.SizeRad = thetaObs;
    t.LastLookS = simTimeS;
    t.Masked = false;
    seen[slot] = true;
    if (t.Hits < kHitsToFirm) t.Hits++;

    if (!t.Firm && t.Hits >= kHitsToFirm) {
      t.Firm = true;
      t.TrackNum = NextTrackNum_++;
      /* No range in the event either: what is not published is not logged, or a debriefing would carry
       * knowledge the pilot never had. */
      FBLog::Info("vis", "CONTACT", {{"track", t.TrackNum}, {"sizeMrad", thetaObs * 1000.0},
          {"contrast", cEff}, {"azDeg", azDeg}, {"elDeg", elDeg}});
    }

    /* RECOGNITION IS THE SAME MEASUREMENT AT A HIGHER THRESHOLD — one quantity, three rungs. The name
     * that appears is the target module's own registry key, so it says WHAT and can never say WHO or
     * WHOSE: two MiG-29s on opposite sides produce the identical string. */
    FBVisualState want = FBVisualState::Detected;
    if (thetaObs >= kIdentifyCycles * thetaReq) want = FBVisualState::Identified;
    else if (thetaObs >= kRecogniseCycles * thetaReq) want = FBVisualState::Recognised;
    if (t.Firm && want != t.State) {
      if (want >= FBVisualState::Recognised) CopyTypeName(t.TypeName, vs.TypeName);
      else t.TypeName[0] = '\0';
      if (want > t.State)
        FBLog::Info("vis", want == FBVisualState::Identified ? "IDENTIFIED" : "RECOGNISED",
            {{"track", t.TrackNum}, {"sizeMrad", thetaObs * 1000.0}, {"type", t.TypeName}});
      t.State = want;
    }
  }

  const double coastS = kCoastFrames * Field_.FrameS > kMinCoastS ? kCoastFrames * Field_.FrameS
                                                                 : kMinCoastS;
  for (int i = 0; i < TrackCount_;) {
    Track &t = Tracks_[i];
    if (seen[i] || simTimeS - t.LastLookS < coastS) {
      if (!seen[i]) t.Hits = 0;
      i++;
      continue;
    }
    FBLog::Info("vis", "DROP", {{"track", t.TrackNum}, {"coastS", simTimeS - t.LastLookS}});
    for (int j = i; j < TrackCount_ - 1; j++) Tracks_[j] = Tracks_[j + 1];
    for (int j = i; j < TrackCount_ - 1; j++) seen[j] = seen[j + 1];
    TrackCount_--;
  }
  for (int i = 0; i < maskedCount; i++) MaskedIds_[i] = maskedIds[i];
  MaskedIdCount_ = maskedCount;
  MaskedCount_ = maskedCount;
  GlareMin_ = (float)glareMin;
}

void FBVisualSystem::DropAllTracks(const char *reason) {
  for (int i = 0; i < TrackCount_; i++)
    if (Tracks_[i].Firm)
      FBLog::Info("vis", "DROP", {{"track", Tracks_[i].TrackNum}, {"reason", reason}});
  TrackCount_ = 0;
}

void FBVisualSystem::Run(FBState &state, const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry *net,
                         double simTimeS) {
  FBVisualBlock &b = state.Visual;
  b.Powered = Powered_;

  if (!Powered_ || !net) {
    if (TrackCount_ > 0) DropAllTracks("eyes closed");
    MaskedIdCount_ = 0;
    b.ContactCount = 0;
    b.CloudMaskedCount = 0;
    b.GlareFactor = 1.0f;
    b.H.Invalidate();   /* eyes shut are NO picture, not an empty one */
    ContactCount_ = MaskedCount_ = BestState_ = 0;
    BestMrad_ = BestAzDeg_ = BestElDeg_ = 0.0f;
    GlareMin_ = 1.0f;
    BlockStatus_ = (int)b.H.Status;
    return;
  }

  int guard = 0;
  bool swept = false;
  while (NextScanS_ <= simTimeS && guard++ < 64) {
    swept = true;
    ScanFrame(state, st, *net, simTimeS);
    NextScanS_ += Field_.FrameS;
  }
  if (NextScanS_ <= simTimeS) NextScanS_ = simTimeS + Field_.FrameS;

  int n = 0;
  BestMrad_ = BestAzDeg_ = BestElDeg_ = 0.0f;
  BestState_ = 0;
  for (int i = 0; i < TrackCount_ && n < kMaxVisualContacts; i++) {
    const Track &t = Tracks_[i];
    if (!t.Firm) continue;
    FBVisualContact &c = b.Contacts[n];
    c.TrackNum = t.TrackNum;
    c.BearingDeg = (float)t.BearingDeg;
    c.ElevAngleDeg = (float)t.ElevAngleDeg;
    c.AzDeg = (float)t.AzDeg;
    c.ElDeg = (float)t.ElDeg;
    c.SizeMrad = (float)(t.SizeRad * 1000.0);
    c.LookAgeS = (float)(simTimeS - t.LastLookS > 0.0 ? simTimeS - t.LastLookS : 0.0);
    c.Coasting = c.LookAgeS > (float)Field_.FrameS;
    c.State = t.State;
    CopyTypeName(c.TypeName, t.TypeName);
    if (c.SizeMrad > BestMrad_) {
      BestMrad_ = c.SizeMrad;
      BestAzDeg_ = c.AzDeg;
      BestElDeg_ = c.ElDeg;
      BestState_ = (int)t.State;
    }
    n++;
  }
  b.ContactCount = n;
  b.CloudMaskedCount = MaskedCount_;
  b.GlareFactor = GlareMin_;
  ContactCount_ = n;
  if (swept) b.H.Publish(simTimeS);
  else b.H.Hold();
  BlockStatus_ = (int)b.H.Status;
}

void FBVisualSystem::DeclareTelemetry(FBTelemetrySchema &schema) const {
  /* FIRST column is this block's validity, declared HERE and not in FBStateBusTelemetry's list — that
   * list sits in the middle of every telemetry.csv ever measured. The same rule the RWR and the IRST
   * state. THERE IS NO RANGE COLUMN, and there is no path that could fill one. */
  schema.Add("blk_vis");
  schema.Add("vis_on");
  schema.Add("vis_contacts");
  schema.Add("vis_best_mrad", "mrad");
  schema.Add("vis_best_az", "deg");
  schema.Add("vis_best_el", "deg");
  schema.Add("vis_best_state");   /* 0 none / 1 detected / 2 recognised / 3 identified */
  schema.Add("vis_masked");
  schema.Add("vis_glare");
}

void FBVisualSystem::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(BlockStatus_);
  row.Push(Powered_);
  row.Push(ContactCount_);
  row.Push((double)BestMrad_);
  row.Push((double)BestAzDeg_);
  row.Push((double)BestElDeg_);
  row.Push(BestState_);
  row.Push(MaskedCount_);
  row.Push((double)GlareMin_);
}

} // namespace FlightBox::Sensors
