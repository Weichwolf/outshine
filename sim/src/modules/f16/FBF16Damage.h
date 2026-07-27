/* FlightBox — the F-16's DAMAGE LAYOUT: where this airframe's systems sit, and how much energy each of
 * them takes to lose. Module data, exactly like the SMS's pylon geometry next door (FBF16Sms) — the
 * core resolves a burst (core/FBDamageModel: geometry, energy, thresholds), but WHICH box is in the
 * radome and which is in the tail is knowledge only the aircraft has.
 *
 * THE AXIS IS THE MODEL'S OWN. Every zone boundary below is read out of the pinned f16.xml's structural
 * frame (x positive AFT, CG at FS -193 in) and converted to metres forward of the CG — no dimension is
 * taken from a drawing or a manual:
 *   RADOME contact  FS -486.6 in  -> +7.46 m   (the nose tip; with the tail below, 14.9 m of airframe,
 *                                               which is the F-16's own 15.03 m length)
 *   EYEPOINT        FS -336.2 in  -> +3.64 m   (the cockpit — the front of the avionics/pilot section)
 *   NOSE_LG         FS -299.6 in  -> +2.71 m
 *   CG              FS -193.0 in  ->  0.00 m
 *   MLG             FS -158.6 in  -> -0.87 m
 *   wingtips (WT)   FS -121.3 in  -> -1.82 m
 *   ventral fins    FS  -97.6 in  -> -2.42 m   (where the engine bay begins)
 *   thruster        FS    0.0 in  -> -4.90 m   (the nozzle)
 *   arrester hook   FS +100.7 in  -> -7.46 m   (the aft extremity)
 *
 * WHICH SYSTEM SITS WHERE is the placement any F-16 photograph shows and no measurement is claimed for
 * it: the APG-68's antenna and transmitter behind the radome; the pitot/AoA probes on the nose cone; the
 * INS, the fire-control computer, the CARA and the MIDS terminal in the avionics bays around the
 * cockpit; the SMS and its station wiring in the centre fuselage with the wing roots; the ALR-56M's
 * receivers and the ALE-47 dispensers on the aft fuselage, next to the engine.
 *
 * THE FOUR FRAGILITY CLASSES are the model's actual settings (values in FBF16Damage.cpp). They are
 * SET numbers, chosen once so that the ladder they produce against this airframe's own geometry reads
 * the way a blast-fragmentation warhead behaves — a burst inside a couple of metres destroys the jet, a
 * burst at the AIM-120's own 10 m fuze radius costs it some avionics — and every intermediate case then
 * follows from the 1/r^2 energy law rather than from another number. */
#ifndef FBF16DAMAGE_H
#define FBF16DAMAGE_H

#include "FBDamageModel.h"

namespace FlightBox {

const FBDamageLayout &FBF16DamageLayout();

} // namespace FlightBox
#endif
