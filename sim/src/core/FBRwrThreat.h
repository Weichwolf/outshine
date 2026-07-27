/* FlightBox — FBRwrThreat: one emitter as a WARNING RECEIVER reports it, and the deliberate opposite of
 * both core/FBRadarContact.h and core/FBDatalinkTrack.h. A datalink track is a message and carries a
 * callsign; a radar contact is an echo and carries range; an RWR threat is neither — it is a DIRECTION
 * a signal is arriving from, plus what the receiver made of that signal.
 *
 * THE TWO ABSENCES ARE THE MODEL:
 *   NO RANGE. An RWR measures bearing and received POWER; it cannot measure range, because it never
 *   transmitted anything to time the return of. doc/f16/defence-rwr-cm.md §2.1 is explicit that the
 *   symbol's distance from the centre of the scope is RELATIVE LETHALITY, not physical range — so this
 *   struct carries a lethality figure and no metres, and nothing downstream can accidentally fly a
 *   range solution off a warning receiver.
 *   NO CERTAINTY ABOUT WHO. Kind is what the receiver ESTIMATED from the signal, not what the emitter
 *   published — the same relationship FBRadarContact's IFF field has to the truth. Today's estimate is
 *   perfect (the threat library is one entry deep); the field exists so that the day it is not, no
 *   consumer has to change.
 *
 * MODE IS THE TACTICAL CONTENT (doc/f16/defence-rwr-cm.md §1's symbol table, one to one): a plain symbol
 * for a set that is searching, a boxed one for a set that is tracking you, a flashing one for a set that
 * is guiding a missile at you — information, warning, threat. The receiver derives it from the emission
 * it hears (core/FBEmitter.h) and from what kind of box it thinks is behind the antenna: a missile's own
 * seeker painting you is the third case however it happens to be scanning.
 *
 * Id is the receiver's OWN symbol number (1..), handed out in detection order and never a unit id —
 * exactly the role FBRadarContact::TrackNum plays, and for exactly the same anti-cheat reason. */
#ifndef FBRWRTHREAT_H
#define FBRWRTHREAT_H

#include <cstdint>
#include "FBEmitter.h"

namespace FlightBox {

/* How many emitters the receiver can hold at once. The ALR-56M's OPEN display shows 16 and PRIORITY 5
 * (doc/f16/defence-rwr-cm.md §2.1); those are DISPLAY caps over the detected set, so this is the
 * detection table's own size and matches kMaxRadarContacts/kMaxDatalinkTracks — eight emitters is more
 * than any mission this simulator flies today puts in the air. */
constexpr int kMaxRwrThreats = 8;

/* The three things a warning receiver distinguishes (see the banner). Telemetry-visible ordinals, and
 * the ORDER is the priority order — a higher ordinal outranks a lower one on the display. */
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
  float ElDeg = 0.0f;         /* elevation the signal arrives at, body-referenced (+ = above) — not
                               * displayed on the real azimuth-only scope, but it is what the antenna
                               * coverage limit is decided on, so it is published rather than hidden */
  float LethalityNorm = 0.0f; /* 0..1, the scope's radial position: 1 = centre (most lethal) */
  float SignalNorm = 0.0f;    /* received power, 0..1 of what this receiver can hear at all — the ONE
                               * proximity cue an RWR really has */
  float AgeS = 0.0f;          /* since the last detection; > 0 = held, the emission stopped or the beam
                               * moved off and the symbol has not been dropped yet */
  FBRwrThreatMode Mode = FBRwrThreatMode::Search;
  FBEmitterKind Kind = FBEmitterKind::Unknown;   /* ESTIMATED (see the banner) */
  bool  New = false;          /* inside the new-threat tone window */
};

} // namespace FlightBox
#endif
