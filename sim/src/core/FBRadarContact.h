/* FlightBox — FBRadarContact: one return as an ACTIVE radar reports it, and the deliberate opposite of
 * core/FBDatalinkTrack. A datalink track is a MESSAGE: the sender broadcasts its own callsign, faction
 * and navigation fix, so identity is free. A radar contact is an ECHO: it carries geometry and nothing
 * else — range, bearing, the angles off the nose, closure. There is no callsign field, no team field and
 * no unit id here, and that absence is the model, not an omission (doc/f16/radar-sensors.md: the FCR
 * range/Doppler-processes returns; identification is a separate box).
 *
 * The one identity channel a radar legitimately has is IFF (doc/f16/datalink-iff.md, AN/APX-113): the
 * interrogator challenges the contact and a valid Mode-4 reply proves FRIENDLY. Everything else stays
 * UNKNOWN — an enemy and a friend with a dead transponder produce the identical NoReply, which is why
 * this enum has no "hostile" value at all. Anything above the sensors that wants to shoot has to live
 * with that, exactly as the pilot of the real jet does.
 *
 * TrackNum is the radar's OWN file number (1..), handed out in acquisition order and reused after a
 * drop. It exists so a display or a pilot can follow the same echo across frames without the sensor
 * having to hand out the thing it does not know — who that is.
 *
 * Fixed capacity, no heap: FBState carries the list inline, so rebuilding the picture allocates nothing.
 * Eight matches kMaxDatalinkTracks and comfortably exceeds the APG-68's ten TWS trackfiles' relevance
 * for the close-in fights this simulator flies. */
#ifndef FB_FBRADARCONTACT_H
#define FB_FBRADARCONTACT_H

namespace FlightBox {

constexpr int kMaxRadarContacts = 8;

/* The ONLY identity a radar contact can carry (see the banner). NoReply is not "hostile": it is the
 * absence of proof, and the two must never be collapsed. */
enum class FBIffReply { NotInterrogated, NoReply, Friendly };

struct FBRadarContact {
  int   TrackNum = 0;       /* the radar's own track file number, 1.. — never a unit id */
  float RangeM = 0.0f;      /* SLANT range as of the last look (LookAgeS ago) */
  float BearingDeg = 0.0f;  /* true bearing own -> contact, deg 0..360 */
  float AzDeg = 0.0f;       /* azimuth off the NOSE, deg -180..180 (+ = right), body-referenced */
  float ElDeg = 0.0f;       /* elevation off the boresight plane, deg (+ = above), body-referenced */
  float ClosureMs = 0.0f;   /* range rate, m/s, + = closing */
  float LookAgeS = 0.0f;    /* sim seconds since the beam last actually hit it; > 0 = coasting */
  bool  Coasting = false;   /* held on the last look, not seen this scan frame */
  FBIffReply Iff = FBIffReply::NotInterrogated;
};

} // namespace FlightBox
#endif /* FB_FBRADARCONTACT_H */
