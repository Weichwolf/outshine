/* FlightBox — the STORE catalogue: what an external store IS, as data. One entry per carriable item,
 * and every number in it is either the store's OWN pinned JSBSim model (vendor/jsbsim/aircraft/<model>,
 * read-only per CLAUDE.md Prinzip 1) or derived from it by a stated formula — nothing about a weapon is
 * invented here, because a released store flies as its own FDM instance of exactly that model and the
 * carriage figures must describe the same object.
 *
 * WHY A CATALOGUE AND NOT A CLASS. A store has no behaviour while it hangs on a pylon: it is mass,
 * drag and a model name. Its behaviour IS the JSBSim model it becomes the moment it is released
 * (modules/stores/FBStoreModule + fdm/FBFdm), so the thing that stays behind on the aircraft is a value
 * type. Lives in core/ for the same reason FBRunway/FBSpawn do: the mission parser, the module's SMS
 * and the app-side spawn path all name it, and none of them may include the others. */
#ifndef FBSTORE_H
#define FBSTORE_H

#include <cstdint>
#include <cstring>

namespace FlightBox {

/* Append only — the ordinal is telemetry-visible (the per-station store column). None is the empty
 * station and must stay 0 so a zeroed block reads as "nothing loaded". */
enum class FBStoreKind : uint8_t { None = 0, Mk82 };

struct FBStoreSpec {
  FBStoreKind Kind = FBStoreKind::None;
  const char *Key = "";        /* the mission-file / FBModuleRegistry name of this store */
  const char *FdmModel = "";   /* its JSBSim model directory under the aircraft root */
  double MassLbs = 0.0;        /* carriage mass */
  double DragAreaFt2 = 0.0;    /* CdA: carriage drag = this * qbar (lbf), see kMk82 below */
  double MaxFlightS = 0.0;     /* lifetime cap after release (see the runner's retire rule) */
};

/* Mk-82, 500 lb general-purpose bomb (doc/f16/weapons.md §3).
 *   MassLbs      = the model's own <emptywt> (mk82.xml: 500 LBS). One object, one mass — the figure the
 *                  carrier loses at release is the same one the released FDM instance then flies with.
 *   DragAreaFt2  = the model's own zero-lift drag at carriage Mach, expressed as an area so it can be
 *                  multiplied by the CARRIER's dynamic pressure: mk82.xml's CDmin table gives
 *                  Cd = 0.144 at M 0.8 over <wingarea> 2.54 ft^2 -> CdA = 0.366 ft^2. Deliberately the
 *                  store's own coefficient and NOT a "drag index" out of a loading manual: no T1/T2
 *                  source for one exists (doc/f16/weapons.md §4.5 marks the station/loading figures as
 *                  T4, cross-check only), and interference/pylon drag is a real effect nobody here can
 *                  quantify — so the carriage drag is exactly the store's own body drag, no invented
 *                  installation factor on top.
 *   MaxFlightS   = a leak guard, not physics: a released store that has neither hit anything nor
 *                  diverged after this long is retired so a run cannot accumulate zombie actors. Fall
 *                  times for this class are tens of seconds (§4.2), so 300 s never truncates a real
 *                  trajectory. */
inline constexpr FBStoreSpec kMk82{FBStoreKind::Mk82, "mk82", "mk82", 500.0, 0.366, 300.0};

inline constexpr const FBStoreSpec *kStoreCatalogue[] = {&kMk82};

inline const FBStoreSpec *FBFindStore(const char *key) {
  if (!key) return nullptr;
  for (const FBStoreSpec *s : kStoreCatalogue)
    if (std::strcmp(s->Key, key) == 0) return s;
  return nullptr;
}

inline const FBStoreSpec *FBStoreSpecOf(FBStoreKind kind) {
  for (const FBStoreSpec *s : kStoreCatalogue)
    if (s->Kind == kind) return s;
  return nullptr;
}

/* ONE released store, as the SMS hands it over: which station let go of what, when, and WHERE that
 * station sits relative to the carrier's CG (body axes, metres: +fwd/+right/+down). The offset travels
 * with the release because the SMS is the only thing that knows its own pylon geometry, and the app-side
 * spawn — the only code allowed to produce an FDM (fdm/FBFdmBoot.h) — must place the new unit at the
 * pylon, not at the carrier's centre of gravity. */
struct FBStoreRelease {
  int    Station = 0;
  FBStoreKind Kind = FBStoreKind::None;
  double MassLbs = 0.0;
  double SimTimeS = 0.0;
  double OffFwdM = 0.0, OffRightM = 0.0, OffDownM = 0.0;
};

} // namespace FlightBox
#endif
