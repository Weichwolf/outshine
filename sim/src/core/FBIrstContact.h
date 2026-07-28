/* What a PASSIVE INFRARED sensor may hand on, and it is even less than a radar echo. FBRadarContact
 * withholds identity; this type withholds RANGE as well, because an IRST measures an ANGLE and nothing
 * else — no round trip was ever transmitted whose return could be timed.
 *
 * The one exception is the collimated LASER RANGEFINDER (the "L" in KOLS): a short, deliberate active
 * measurement on command, inside its own much shorter reach. It is therefore not a field that is
 * always filled but a field with its own validity bit — a consumer that wants metres has to ask
 * whether anybody measured any.
 *
 * NO IDENTITY, NO IFF, and here that is not only the anti-cheat rule but the aircraft's own doctrine:
 * "the IFF interrogator does not operate with the IRST — be absolutely sure that the target is an
 * enemy aircraft before attacking" (doc/modules/mig29/radar-sensors.md §6.4). Going passive buys
 * stealth and costs identity. doc/sensors.md, Abschnitt 6. */
#ifndef FBIRSTCONTACT_H
#define FBIRSTCONTACT_H

namespace FlightBox {

constexpr int kMaxIrstContacts = 8;

struct FBIrstContact {
  int   TrackNum = 0;         /* sensor-owned file number from 1 up, acquisition order; NO unit id */
  float BearingDeg = 0.0f;    /* true bearing to the source, 0..360 (world-referenced) */
  float ElevAngleDeg = 0.0f;  /* elevation angle to it, + = above (world-referenced) */
  float AzDeg = 0.0f;         /* body-referenced, + = right — the head's own quantity */
  float ElDeg = 0.0f;
  float LookAgeS = 0.0f;      /* since the last real look; the picture is never "live" */
  bool  Coasting = false;
  /* The laser's answer, and only ever the laser's. False means "nobody measured a range", NOT "zero
   * metres" — the same three-state discipline the block heads use, applied to one field. */
  bool  HasRange = false;
  float RangeM = 0.0f;
};

} // namespace FlightBox
#endif
