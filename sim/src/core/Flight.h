/* The FLIGHT a unit belongs to and the position it holds in it. In core/ for the same reason
 * Team.h is: it is BOTH mission data (`flight <name> <position>`) and world-entity identity — the
 * cooperative datalink reads it off the registry exactly as it reads the team, and a second notion of
 * "who leads this flight" would let the two disagree. doc/formation.md, section 1. */
#ifndef FLIGHT_H
#define FLIGHT_H

#include <string>

namespace outshine {

/* The name is short because it TRAVELS: a PPLI carries it, so it lives in a fixed-size field beside
 * the callsign (core/DatalinkTrack.h). */
constexpr int kFlightNameLen = 16;
/* One more than a four-ship, because a cooperative track list holds eight and a flight can never be
 * larger than the net that carries it (kMaxDatalinkTracks). */
constexpr int kMaxFlightPosition = 8;

struct FlightId {
  std::string Name;    /* empty = this unit is in no declared flight */
  int Position = 0;    /* 1 = lead, 2..kMaxFlightPosition = wingmen; 0 = not declared */

  bool Declared() const { return Position > 0 && !Name.empty(); }
  bool IsLead() const { return Position == 1; }
};

/* WHAT A MEMBER TELLS ITS FLIGHT, and the whole of it — carried in the same PPLI as the position
 * report, so it costs no channel of its own and inherits its range, its cycle and its age.
 *
 * `Tgt*` is a POSITION and never an identity: this jet's radar does not know whom it sees
 * (core/RadarContact.h), so it cannot tell anybody. A receiver correlates the reported point
 * against its OWN contacts and may fail to — which is the honest failure mode of a shared picture,
 * not a defect. */
struct FlightReport {
  bool   Engaging = false;   /* prosecuting a contact */
  bool   Bound = false;      /* ...and the round in the air still needs THIS jet's illumination */
  double TgtLatDeg = 0.0, TgtLonDeg = 0.0;
  float  TgtAltM = 0.0f;
};

} // namespace outshine
#endif
