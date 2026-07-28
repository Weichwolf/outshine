/* One contact as a COOPERATIVE datalink reports it — NOT a sensor return: the sender broadcasts its own
 * fix and identity, so callsign and team come for free and the accuracy is the SENDER's. A receiver
 * adds only WHEN it heard it, which is why a track is never "live".
 * doc/core.md, Abschnitt 8.2. */
#ifndef FB_FBDATALINKTRACK_H
#define FB_FBDATALINKTRACK_H

#include "FBTeam.h"

namespace FlightBox {

constexpr int kMaxDatalinkTracks = 8;
constexpr int kDatalinkCallsignLen = 25;   /* .fbm callsigns are 1..24 chars + NUL (doc/missions/syntax.md) */

struct FBDatalinkTrack {
  int    UnitId = 0;                          /* the sender's unit id — the track's identity key */
  char   Callsign[kDatalinkCallsignLen] = {}; /* the sender's own name, NUL-terminated */
  FBUnitTeam Team = FBUnitTeam::Friendly;
  double LatDeg = 0.0, LonDeg = 0.0;          /* as REPORTED (the sender's own position fix) */
  float  AltM = 0.0f;
  float  HeadingDeg = 0.0f, SpeedMs = 0.0f;   /* the reported velocity vector, polar form */
  float  RangeM = 0.0f;                       /* receiver-computed: own position -> reported position */
  float  BearingDeg = 0.0f;                   /* true bearing to the reported position */
  float  ReportTimeS = 0.0f;                  /* sim time of the message this track still stands on */
  float  AgeS = 0.0f;                         /* now - ReportTimeS; 0 only in the tick it arrived */
};

} // namespace FlightBox
#endif /* FB_FBDATALINKTRACK_H */
