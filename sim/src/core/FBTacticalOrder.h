/* FlightBox — the TACTICAL ORDER: what a commander may say to a unit's AI, and the whole of it.
 *
 * An order is an INTENTION, never a state write. It is handed to pilot/FBPilot, which decides in its
 * own decision tick whether it can act on it — and where the act is an avionics input, it is TYPED over
 * core/FBCommandBus at that control's latency and may be turned away there like any other entry. The
 * set of state writers does not grow by one: fdm/FBFdmBoot stays the only one (doc/player-layer.md
 * §9.6 / P10).
 *
 * SO THERE ARE TWO REFUSAL LAYERS AND BOTH ARE VISIBLE: this file's own (the pilot cannot do the thing
 * at all, or has nothing to do it to) and the bus's (the hand is busy, the head is at 5 g, the box is
 * dead). Neither is silent — an order that produced no line would be a commander talking into a void.
 *
 * WHAT IT IS NOT: it names no unit and carries no identity of anything it points at. An order to attack
 * is an order to attack a PLACE, because a place is what the commander's own picture holds
 * (core/FBForcePicture) — the map cannot hand a pilot a target it could not see itself. */
#ifndef FBTACTICALORDER_H
#define FBTACTICALORDER_H

#include <cstdint>

namespace FlightBox {

/* Ordinals are log- and telemetry-visible: append, never reorder. */
enum class FBOrderKind : uint8_t {
  None = 0,
  Waypoint,        /* fly to this point and go on with the route from there */
  Steer,           /* turn onto this point now, holding the present task */
  Attack,          /* engage what you hold at this point — YOUR sensors decide whether you hold it */
  Abort,           /* break off; back to the route */
  Emcon,           /* Value: 1 = go silent, 0 = radiate */
  WeaponsControl,  /* Value: the FBWeaponsControl ordinal the commander transmits */
};

inline const char *FBOrderKindStr(FBOrderKind k) {
  switch (k) {
    case FBOrderKind::None: return "none";
    case FBOrderKind::Waypoint: return "waypoint";
    case FBOrderKind::Steer: return "steer";
    case FBOrderKind::Attack: return "attack";
    case FBOrderKind::Abort: return "abort";
    case FBOrderKind::Emcon: return "emcon";
    case FBOrderKind::WeaponsControl: return "weapons_control";
  }
  return "?";
}

/* WHY AN ORDER DID NOT HAPPEN. Closed catalogue, in the shape FBCommandReason already has, and each
 * entry is a fact about the RECEIVER that the receiver itself knows — never about the world. */
enum class FBOrderReason : uint8_t {
  None = 0,
  NoCapability,   /* this aircraft has no such control at all (no silent radar mode, no fire authority) */
  NothingHeld,    /* nothing of this unit's own is at the place the order names */
  BusRefused,     /* the entry reached the box and the box turned it away — the reason is the bus's */
  QueueFull,      /* more orders outstanding than a crew can carry */
};

inline const char *FBOrderReasonStr(FBOrderReason r) {
  switch (r) {
    case FBOrderReason::None: return "none";
    case FBOrderReason::NoCapability: return "no_capability";
    case FBOrderReason::NothingHeld: return "nothing_held";
    case FBOrderReason::BusRefused: return "bus_refused";
    case FBOrderReason::QueueFull: return "queue_full";
  }
  return "?";
}

struct FBTacticalOrder {
  uint32_t    Seq = 0;
  FBOrderKind Kind = FBOrderKind::None;
  double      LatDeg = 0.0, LonDeg = 0.0, AltM = 0.0;   /* Waypoint / Steer / Attack */
  double      Value = 0.0;                              /* Emcon, WeaponsControl */
  double      IssuedS = 0.0;
};

} // namespace FlightBox
#endif
