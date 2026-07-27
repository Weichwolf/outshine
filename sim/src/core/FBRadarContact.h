/* One return as an ACTIVE radar reports it — the deliberate opposite of FBDatalinkTrack: a track is a
 * MESSAGE and identity is free, an echo is GEOMETRY and nothing else. No callsign, no team, no unit id,
 * and that absence is the MODEL. The one identity channel is IFF, and it is two-valued.
 * doc/flightbox/core.md, Abschnitt 8.1. */
#ifndef FB_FBRADARCONTACT_H
#define FB_FBRADARCONTACT_H

namespace FlightBox {

constexpr int kMaxRadarContacts = 8;

/* NoReply is not "hostile": it is the ABSENCE OF PROOF, and the two must never be collapsed. */
enum class FBIffReply { NotInterrogated, NoReply, Friendly };

struct FBRadarContact {
  int   TrackNum = 0;       /* the radar's own track file number, 1.. — never a unit id */
  float RangeM = 0.0f;      /* SLANT range as of the last look (LookAgeS ago) */
  float BearingDeg = 0.0f;  /* true bearing own -> contact, deg 0..360 */
  float ElevAngleDeg = 0.0f;/* above the local horizontal (+ = above) — BearingDeg's WORLD-referenced
                             * partner, so a consumer places the echo without un-rotating a look-old
                             * body vector through a now-current attitude */
  float AzDeg = 0.0f;       /* azimuth off the NOSE, deg -180..180 (+ = right), body-referenced */
  float ElDeg = 0.0f;       /* elevation off the boresight plane, deg (+ = above), body-referenced */
  float ClosureMs = 0.0f;   /* range rate, m/s, + = closing */
  float LookAgeS = 0.0f;    /* sim seconds since the beam last actually hit it; > 0 = coasting */
  bool  Coasting = false;   /* held on the last look, not seen this scan frame */
  FBIffReply Iff = FBIffReply::NotInterrogated;
};

} // namespace FlightBox
#endif /* FB_FBRADARCONTACT_H */
