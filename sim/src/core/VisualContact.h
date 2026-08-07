/* What an EYE may hand on, and it is the least of all five channels. RadarContact withholds identity,
 * IrstContact withholds range as well; this type withholds the range STRUCTURALLY — there is no
 * HasRange bit, because no path in the tree could ever set one. An eye has no clock on a round trip, no
 * second baseline and no laser.
 *
 * WHAT IT CAN EVENTUALLY SAY IS *WHAT*, NEVER *WHO* AND NEVER *WHOSE*. The type name appears once the
 * observed angular size crosses a Johnson multiple of the detection threshold, and it is the target's
 * own registry key. Two bodies of the same kind on opposite sides produce the identical string. */
#ifndef VISUALCONTACT_H
#define VISUALCONTACT_H

#include <cstdint>

namespace outshine {

constexpr int kMaxVisualContacts = 8;
constexpr int kVisualTypeNameLen = 16;

/* One quantity, three thresholds — see VisualSystem's Johnson multiples. The ladder is monotone in
 * angular size, so a contact never skips a rung on the way up. */
enum class VisualState : uint8_t { Detected = 1, Recognised = 2, Identified = 3 };

struct VisualContact {
  int   TrackNum = 0;         /* sensor-owned file number from 1 up, acquisition order; NO unit id */
  float BearingDeg = 0.0f;    /* true bearing, 0..360 (world-referenced) */
  float ElevAngleDeg = 0.0f;  /* elevation angle, + = above (world-referenced) */
  float AzDeg = 0.0f;         /* body-referenced, + = right of the nose */
  float ElDeg = 0.0f;
  float SizeMrad = 0.0f;      /* the OBSERVED subtense of the largest presented dimension */
  float LookAgeS = 0.0f;
  bool  Coasting = false;
  VisualState State = VisualState::Detected;
  /* Empty until the size crosses the recognition multiple. NOT a lookup keyed on what the target is —
   * the geometry earns it, and it says only what kind of aeroplane, never whose. */
  char  TypeName[kVisualTypeNameLen] = {};
};

} // namespace outshine
#endif
