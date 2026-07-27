/* One emitter as a WARNING RECEIVER reports it: a DIRECTION a signal arrives from, plus what the
 * receiver made of it — the deliberate opposite of both FBRadarContact and FBDatalinkTrack.
 * THE TWO ABSENCES ARE THE MODEL: no RANGE (an RWR never transmitted anything to time a return, so the
 * scope's radial position is LETHALITY), and no certainty about WHO (Kind is ESTIMATED).
 * doc/flightbox/core.md, Abschnitt 8.4. */
#ifndef FBRWRTHREAT_H
#define FBRWRTHREAT_H

#include <cstdint>
#include "FBEmitter.h"

namespace FlightBox {

/* The DETECTION table's own size (the ALR-56M's 16/5 are DISPLAY caps over it), matching
 * kMaxRadarContacts/kMaxDatalinkTracks. */
constexpr int kMaxRwrThreats = 8;

/* Telemetry-visible ordinals, and the ORDER is the PRIORITY order on the display. */
enum class FBRwrThreatMode : uint8_t { Search = 0, Track, Missile };

inline const char *FBRwrThreatModeStr(FBRwrThreatMode m) {
  switch (m) {
    case FBRwrThreatMode::Search: return "search";
    case FBRwrThreatMode::Track: return "track";
    case FBRwrThreatMode::Missile: return "missile";
  }
  return "?";
}

struct FBRwrThreat {
  int   Id = 0;               /* the receiver's own symbol number, 1.. — never a unit id */
  float BearingDeg = 0.0f;    /* RELATIVE to own nose, -180..180 (+ = right): the TWA is a relative-
                               * bearing display, own nose at the top (doc/f16/defence-rwr-cm.md §1) */
  float ElDeg = 0.0f;         /* body-referenced (+ = above) — not on the real azimuth-only scope, but
                               * the antenna coverage limit is decided on it, so it is published */
  float LethalityNorm = 0.0f; /* 0..1, the scope's radial position: 1 = centre (most lethal) */
  float SignalNorm = 0.0f;    /* received power, 0..1 — the ONE proximity cue an RWR really has */
  float AgeS = 0.0f;          /* since the last detection; > 0 = held, not yet dropped */
  FBRwrThreatMode Mode = FBRwrThreatMode::Search;
  FBEmitterKind Kind = FBEmitterKind::Unknown;   /* ESTIMATED, never the emitter's own claim */
  bool  New = false;          /* inside the new-threat tone window */
};

} // namespace FlightBox
#endif
