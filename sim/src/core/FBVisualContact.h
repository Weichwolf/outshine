/* What an EYE may hand on, and it is the least of all five channels. FBRadarContact withholds identity,
 * FBIrstContact withholds range as well; this type withholds the range STRUCTURALLY — there is no
 * HasRange bit, because no path in the tree could ever set one. An eye has no clock on a round trip, no
 * second baseline and no laser.
 *
 * WHAT IT CAN EVENTUALLY SAY IS *WHAT*, NEVER *WHO* AND NEVER *WHOSE*. The type name appears once the
 * observed angular size crosses a Johnson multiple of the detection threshold, and it is the target
 * module's own FBModuleRegistry key — the same string the mission file already spells out. Two MiG-29s
 * on opposite sides produce the identical string; a flight of four produces four contacts each labelled
 * "mig29". doc/sensors.md §9.7. */
#ifndef FBVISUALCONTACT_H
#define FBVISUALCONTACT_H

#include <cstdint>

namespace FlightBox {

constexpr int kMaxVisualContacts = 8;
/* The registry key is the whole vocabulary of this field: "f16", "mig29", "target_hard". */
constexpr int kVisualTypeNameLen = 16;

/* One quantity, three thresholds — see FBVisualSystem's Johnson multiples. The ladder is monotone in
 * angular size, so a contact never skips a rung on the way up. */
enum class FBVisualState : uint8_t { Detected = 1, Recognised = 2, Identified = 3 };

struct FBVisualContact {
  int   TrackNum = 0;         /* sensor-owned file number from 1 up, acquisition order; NO unit id */
  float BearingDeg = 0.0f;    /* true bearing, 0..360 (world-referenced) */
  float ElevAngleDeg = 0.0f;  /* elevation angle, + = above (world-referenced) */
  float AzDeg = 0.0f;         /* body-referenced, + = right of the nose */
  float ElDeg = 0.0f;
  float SizeMrad = 0.0f;      /* the OBSERVED subtense of the largest presented dimension */
  float LookAgeS = 0.0f;
  bool  Coasting = false;
  FBVisualState State = FBVisualState::Detected;
  /* Empty until the size crosses the recognition multiple. NOT a lookup keyed on what the target is —
   * the geometry earns it, and it says only what kind of aeroplane, never whose. */
  char  TypeName[kVisualTypeNameLen] = {};
};

} // namespace FlightBox
#endif
