#include "FBForcePicture.h"

#include "FBGeodesy.h"

#include <cmath>
#include <cstring>

namespace FlightBox {

const char *FBAffiliationStr(FBAffiliation a) {
  switch (a) {
    case FBAffiliation::Pending: return "pending";
    case FBAffiliation::Unknown: return "unknown";
    case FBAffiliation::AssumedFriend: return "assumed-friend";
    case FBAffiliation::Friend: return "friend";
    case FBAffiliation::Neutral: return "neutral";
    case FBAffiliation::Suspect: return "suspect";
    case FBAffiliation::Hostile: return "hostile";
  }
  return "?";
}

const char *FBForceSourceStr(FBForceSource s) {
  switch (s) {
    case FBForceSource::Self: return "self";
    case FBForceSource::Ppli: return "ppli";
    case FBForceSource::Radar: return "radar";
    case FBForceSource::NetReport: return "net";
    case FBForceSource::Rwr: return "rwr";
    case FBForceSource::Irst: return "irst";
    case FBForceSource::Visual: return "visual";
  }
  return "?";
}

static void CopyLabel(char *dst, const char *src) {
  if (!src) { dst[0] = 0; return; }
  std::strncpy(dst, src, kForceLabelLen - 1);
  dst[kForceLabelLen - 1] = 0;
}

void FBForcePicture::Begin(double nowS, FBUnitTeam ownTeam) {
  Count_ = 0;
  Own_ = Contacts_ = Bearings_ = 0;
  NowS_ = nowS;
  OwnTeam_ = ownTeam;
}

FBForceSymbol *FBForcePicture::Add(FBForceSource src, FBAffiliation aff) {
  if (Count_ >= kMaxSymbols) return nullptr;
  FBForceSymbol &s = Symbols_[Count_++];
  s = FBForceSymbol{};
  s.Src = src;
  s.Aff = aff;
  return &s;
}

bool FBForcePicture::AlreadyHeld(double latDeg, double lonDeg, float ageS) const {
  for (int i = 0; i < Count_; i++) {
    if (!Symbols_[i].HavePoint) continue;
    float older = ageS > Symbols_[i].AgeS ? ageS : Symbols_[i].AgeS;
    double gate = kMergeBaseM + (double)older * kMergeSpeedMs;
    if (FBPlanarDistM(Symbols_[i].LatDeg, Symbols_[i].LonDeg, latDeg, lonDeg) <= gate) return true;
  }
  return false;
}

/* THE ONE FUNCTION, and every line of it reads a block of `s`. The order is the order of certainty, so
 * the merge gate above keeps the BEST datum for a place: own position, then cooperative messages, then
 * own echoes, then reported points, then directions without a range. */
void FBForcePicture::Ingest(const FBState &s, const char *callsign, double latDeg, double lonDeg,
                            double altM, double headingDeg, double speedMs) {
  /* 1. THE OBSERVER HIMSELF. He is not a contact, he is the unit whose picture this is. */
  if (!AlreadyHeld(latDeg, lonDeg, 0.0f)) {
    if (FBForceSymbol *o = Add(FBForceSource::Self, FBAffiliation::Friend)) {
      o->HavePoint = true;
      o->LatDeg = latDeg; o->LonDeg = lonDeg; o->AltM = (float)altM;
      o->ObsLatDeg = latDeg; o->ObsLonDeg = lonDeg;
      o->HeadingDeg = (float)headingDeg; o->SpeedMs = (float)speedMs;
      CopyLabel(o->Label, callsign);
      Own_++;
    }
  }

  /* 2. THE COOPERATIVE MESSAGES. A PPLI is the sender's OWN fix with the sender's OWN identity, so it
   * is the only positive identification this tree hands out for free — and it carries the age the
   * receiver computed, never zero after the tick it arrived. */
  const FBDatalinkBlock &dl = s.Datalink;
  if (dl.H.Readable()) {
    for (int i = 0; i < dl.TrackCount; i++) {
      const FBDatalinkTrack &t = dl.Tracks[i];
      if (t.Team != OwnTeam_) continue;   /* a cooperative net carries one faction */
      if (!AlreadyHeld(t.LatDeg, t.LonDeg, t.AgeS)) {
        if (FBForceSymbol *m = Add(FBForceSource::Ppli, FBAffiliation::Friend)) {
          m->HavePoint = true;
          m->LatDeg = t.LatDeg; m->LonDeg = t.LonDeg; m->AltM = t.AltM;
          m->ObsLatDeg = latDeg; m->ObsLonDeg = lonDeg;
          m->BearingDeg = t.BearingDeg;
          m->RangeM = t.RangeM;
          m->AgeS = t.AgeS;
          m->HeadingDeg = t.HeadingDeg; m->SpeedMs = t.SpeedMs;
          CopyLabel(m->Label, t.Callsign);
          Own_++;
        }
      }
      /* 3. WHAT THAT MEMBER'S OWN SET MEASURED, riding the same message: a POINT, the sender's own look
       * age, and NO identity — there is no id field and no team field on it to read. */
      if (!t.Net.Reporting) continue;
      if (AlreadyHeld(t.Net.LatDeg, t.Net.LonDeg, t.Net.TgtLookAgeS + t.AgeS)) continue;
      if (FBForceSymbol *n = Add(FBForceSource::NetReport, FBAffiliation::Unknown)) {
        n->HavePoint = true;
        n->LatDeg = t.Net.LatDeg; n->LonDeg = t.Net.LonDeg; n->AltM = t.Net.AltM;
        n->ObsLatDeg = latDeg; n->ObsLonDeg = lonDeg;
        n->BearingDeg = (float)FBBearingDeg(latDeg, lonDeg, t.Net.LatDeg, t.Net.LonDeg);
        n->RangeM = (float)FBPlanarDistM(latDeg, lonDeg, t.Net.LatDeg, t.Net.LonDeg);
        /* BOTH staleness terms, summed, because both are real: how old the look was when the sender
         * reported it, plus how long ago the receiver heard the message. */
        n->AgeS = t.Net.TgtLookAgeS + t.AgeS;
        Contacts_++;
      }
    }
  }

  /* 4. THE CONTROLLER'S OWN FEED, if this observer subscribes to one. Same message type, other block —
   * a cue is never a track, and it arrives here as exactly what it is: a place and a staleness. */
  const FBDatalinkBlock &nl = s.NetLink;
  if (nl.H.Readable()) {
    for (int i = 0; i < nl.TrackCount; i++) {
      const FBDatalinkTrack &t = nl.Tracks[i];
      if (!t.Net.Reporting) continue;
      if (AlreadyHeld(t.Net.LatDeg, t.Net.LonDeg, t.Net.TgtLookAgeS + t.AgeS)) continue;
      if (FBForceSymbol *n = Add(FBForceSource::NetReport, FBAffiliation::Unknown)) {
        n->HavePoint = true;
        n->LatDeg = t.Net.LatDeg; n->LonDeg = t.Net.LonDeg; n->AltM = t.Net.AltM;
        n->ObsLatDeg = latDeg; n->ObsLonDeg = lonDeg;
        n->BearingDeg = (float)FBBearingDeg(latDeg, lonDeg, t.Net.LatDeg, t.Net.LonDeg);
        n->RangeM = (float)FBPlanarDistM(latDeg, lonDeg, t.Net.LatDeg, t.Net.LonDeg);
        n->AgeS = t.Net.TgtLookAgeS + t.AgeS;
        Contacts_++;
      }
    }
  }

  /* 5. THIS OBSERVER'S OWN ECHOES. Anonymous by construction; the ONE identity channel is the IFF, and
   * it answers Friendly or it does not answer. NoReply and NotInterrogated both read UNKNOWN — the
   * moment they read anything else, this map has started inventing enemies. */
  const FBRadarBlock &r = s.Radar;
  if (r.H.Readable()) {
    double coslat = std::cos(latDeg * kDeg2Rad);
    for (int i = 0; i < r.ContactCount; i++) {
      const FBRadarContact &c = r.Contacts[i];
      double elv = c.ElevAngleDeg * kDeg2Rad, brg = c.BearingDeg * kDeg2Rad;
      double horiz = c.RangeM * std::cos(elv);
      double cLat = latDeg + horiz * std::cos(brg) / kMPerDeg;
      double cLon = lonDeg + (std::fabs(coslat) > 1e-6 ? horiz * std::sin(brg) / (kMPerDeg * coslat) : 0.0);
      if (AlreadyHeld(cLat, cLon, c.LookAgeS)) continue;
      FBAffiliation aff = c.Iff == FBIffReply::Friendly ? FBAffiliation::Friend : FBAffiliation::Unknown;
      if (FBForceSymbol *e = Add(FBForceSource::Radar, aff)) {
        e->HavePoint = true;
        e->LatDeg = cLat; e->LonDeg = cLon;
        e->AltM = (float)(altM + c.RangeM * std::sin(elv));
        e->ObsLatDeg = latDeg; e->ObsLonDeg = lonDeg;
        e->BearingDeg = c.BearingDeg;
        e->RangeM = c.RangeM;
        e->AgeS = c.LookAgeS;
        e->Coasting = c.Coasting;
        if (aff == FBAffiliation::Friend) Own_++; else Contacts_++;
      }
    }
  }

  /* 6. THE EYE. It earns a TYPE and never an allegiance, and it has no range at all — structurally, in
   * FBVisualContact. So it is a bearing that happens to carry a word. */
  const FBVisualBlock &v = s.Visual;
  if (v.H.Readable()) {
    for (int i = 0; i < v.ContactCount; i++) {
      const FBVisualContact &c = v.Contacts[i];
      if (FBForceSymbol *e = Add(FBForceSource::Visual, FBAffiliation::Unknown)) {
        e->ObsLatDeg = latDeg; e->ObsLonDeg = lonDeg;
        e->BearingDeg = c.BearingDeg;
        e->AgeS = c.LookAgeS;
        e->Coasting = c.Coasting;
        CopyLabel(e->Label, c.TypeName);   /* empty until the angular size earned it */
        Bearings_++;
      }
    }
  }

  /* 7. THE WARNING RECEIVER: a DIRECTION and a class, never a range. It is drawn as a line and its
   * bearing is RELATIVE to the observer's nose, so his own heading is the second half of it. */
  const FBRwrBlock &w = s.Rwr;
  if (w.H.Readable()) {
    for (int i = 0; i < w.ThreatCount; i++) {
      const FBRwrThreat &t = w.Threats[i];
      if (FBForceSymbol *e = Add(FBForceSource::Rwr, FBAffiliation::Unknown)) {
        e->ObsLatDeg = latDeg; e->ObsLonDeg = lonDeg;
        double brg = FBWrap180(headingDeg + t.BearingDeg);
        e->BearingDeg = (float)(brg < 0.0 ? brg + 360.0 : brg);
        e->AgeS = t.AgeS;
        Bearings_++;
      }
    }
  }
}

} // namespace FlightBox
