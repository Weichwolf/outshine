/* FlightBox — FBForcePicture: WHAT ONE FACTION HAS ACTUALLY COLLECTED, and nothing else.
 *
 * The tactical map's single data product. It is built the same way pilot/FBFlightPicture is built and
 * for the same reason: out of PUBLISHED FBState BLOCKS ONLY. It never names units/FBUnitRegistry, so
 * tools/verify_layers.py's PERCEPTION_READERS list stays at SIX entries — a map that read the registry
 * would not be a map, it would be the truth wearing an icon set (doc/player-layer.md §9.1).
 *
 * WHERE IT SITS: at the CONTROL NODE of the faction's net. Ingest() takes one observer's own published
 * state plus where that observer says it is, so the picture is exactly the fusion that node performed:
 * its own sensors, plus every cooperative message that reached it. A contributor that dropped off the
 * link contributes nothing, and its symbols age out — which is the mechanic, not a shortfall.
 *
 * THE AFFILIATION RULE, and it is the one line a careless reader deletes: HOSTILE IS NEVER DERIVED. Our
 * IFF has two answers — a valid Mode 4 reply, or silence — and silence is the absence of proof. A radar
 * echo with no reply is UNKNOWN. Nothing in this file writes Suspect or Hostile; the enum carries them
 * because APP-6 does, and the day something may honestly claim one it will say where it got it from.
 * doc/player-layer.md §9.3, doc/sensors.md §1. */
#ifndef FBFORCEPICTURE_H
#define FBFORCEPICTURE_H

#include <cstdint>

#include "FBState.h"
#include "FBTeam.h"

namespace FlightBox {

/* APP-6 / MIL-STD-2525's graded affiliation alphabet, in the standard's own order. This tree can
 * honestly assert three of the seven; the rest exist so that the vocabulary is the standard's and not
 * an invention, and so that a future claim has a name to land in. */
enum class FBAffiliation : uint8_t { Pending = 0, Unknown, AssumedFriend, Friend, Neutral, Suspect, Hostile };

const char *FBAffiliationStr(FBAffiliation a);

/* THE PROVENANCE KEY of doc/player-layer.md §1 check 2, as a type rather than a promise: every symbol
 * names the block it was read out of, and a symbol cannot be constructed without one. */
enum class FBForceSource : uint8_t { Self = 0, Ppli, Radar, NetReport, Rwr, Irst, Visual };

const char *FBForceSourceStr(FBForceSource s);

constexpr int kForceLabelLen = 25;   /* a callsign (kDatalinkCallsignLen) or a visual TYPE, never both */

/* ONE thing on the map. Two shapes in one struct because the difference IS the content: a source that
 * measured a range produces a POINT, a source that only measured a direction produces a BEARING, and a
 * bearing drawn as a point is a lie the standard has a symbol for avoiding. */
struct FBForceSymbol {
  FBForceSource Src = FBForceSource::Self;
  FBAffiliation Aff = FBAffiliation::Pending;
  bool   HavePoint = false;          /* false = a LINE OF BEARING and nothing more (RWR) */
  double LatDeg = 0.0, LonDeg = 0.0; /* only when HavePoint */
  float  AltM = 0.0f;
  double ObsLatDeg = 0.0, ObsLonDeg = 0.0;   /* who measured it: a bearing has to start somewhere */
  float  BearingDeg = 0.0f;          /* TRUE bearing observer -> datum, always valid */
  float  RangeM = -1.0f;             /* < 0 = no range was ever measured on this datum */
  float  AgeS = 0.0f;                /* seconds since the datum was measured; the map prints it */
  float  HeadingDeg = -1.0f, SpeedMs = -1.0f;   /* < 0 = this source carries no velocity */
  bool   Coasting = false;           /* held on the last look — the block bus's third state, drawn */
  char   Label[kForceLabelLen] = {}; /* a callsign (own force) or an EARNED type (the eye). Never both,
                                      * never invented: empty is the normal case for a contact */
};

class FBForcePicture {
public:
  static constexpr int kMaxSymbols = 40;
  /* WHEN TWO DATA ARE ONE THING. It is doc/formation.md §5.2's correlation gate verbatim, third
   * consumer: a base radius plus what the target could have moved since the OLDER datum was measured.
   * Reused rather than re-chosen, because it answers the identical question -- "is the point somebody
   * reported the echo I am holding?" -- and a map that kept both would draw two aeroplanes where the
   * force has one report and one echo of the same one. */
  static constexpr double kMergeBaseM = 1000.0;
  static constexpr double kMergeSpeedMs = 300.0;

  /* One frame of the picture. The observer's own team is the faction the map belongs to; a track from
   * another team cannot appear, because a cooperative net carries one faction by construction. */
  void Begin(double nowS, FBUnitTeam ownTeam);

  /* ONE CONTRIBUTOR: what this unit published this tick, plus where it says it is. Everything the
   * function reads is an argument or a block of `s` — there is no path from here to the world. */
  void Ingest(const FBState &s, const char *callsign, double latDeg, double lonDeg, double altM,
              double headingDeg, double speedMs);

  int Count() const { return Count_; }
  const FBForceSymbol &At(int i) const { return Symbols_[i]; }
  double NowS() const { return NowS_; }
  FBUnitTeam OwnTeam() const { return OwnTeam_; }

  /* Counts the map's own legend prints, and the numbers the acceptance test reads back. */
  int OwnCount() const { return Own_; }
  int ContactCount() const { return Contacts_; }
  int BearingCount() const { return Bearings_; }

private:
  /* Returns nullptr when the table is full — a picture that silently forgot a symbol would be worse
   * than one that visibly stops growing. */
  FBForceSymbol *Add(FBForceSource src, FBAffiliation aff);
  /* A datum this picture already holds within kMergeM, from a source of equal or better standing.
   * Merging is the only way a symbol may DISAPPEAR here, and it can never invent one. */
  bool AlreadyHeld(double latDeg, double lonDeg, float ageS) const;

  FBForceSymbol Symbols_[kMaxSymbols]{};
  int Count_ = 0;
  int Own_ = 0, Contacts_ = 0, Bearings_ = 0;
  double NowS_ = 0.0;
  FBUnitTeam OwnTeam_ = FBUnitTeam::Friendly;
};

} // namespace FlightBox
#endif
