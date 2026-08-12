/* THE VIEWER WHO CAN WALK AWAY FROM THE DECLARED STANDPOINT — the one thing the browser adds over
 * the frame oracle. It integrates a stance and nothing else: it owns no world, draws nothing, and
 * ends at Outshine::Look, so render/ never learns that anyone is steering. */
#ifndef WALKER_H
#define WALKER_H

#include "Outshine.h"

namespace outshine::Clients {

class Walker {
public:
  enum class Move { Fwd, Back, Left, Right, Count };

  /* [SET] A relaxed human walk. 1.4 m/s is the pedestrian design speed used for crossing timings,
   * and the ORDER OF MAGNITUDE is all this asks for. */
  static constexpr double kWalkSpeedMs = 1.4;
  /* [SET] by the owner: Shift covers ground, it does not jog. 100 x 1.4 = 140 m/s — a travel speed
   * for reaching a standpoint, not a gait. */
  static constexpr double kRunFactor = 100.0;
  /* [SET] Degrees of turn per pixel of locked pointer travel. Its DERIVED consequence is the number
   * that matters: a 180 deg about-face is 180/0.12 = 1500 px, a little over one 1280 px canvas. */
  static constexpr double kMouseDegPerPx = 0.12;
  /* The basis is built from yaw and pitch alone (no roll), so looking straight up is degenerate: the
   * cross product that makes screen-right vanishes. 89 keeps a whole degree of margin. */
  static constexpr double kMaxPitchDeg = 89.0;
  /* A backgrounded tab hands back one enormous dt on return. A walker must not teleport, so the step
   * is capped at 100 ms = 14 cm at walking pace. */
  static constexpr double kMaxStepS = 0.1;

  void Reset(const Outshine::Stance &s) { At_ = s; }
  void Hold(Move m, bool down) { Held_[(int)m] = down; }
  void ReleaseAll() { for (int i = 0; i < (int)Move::Count; i++) Held_[i] = false; }
  void SetFast(bool on) { Fast_ = on; }
  /* ACCUMULATED and consumed once per step: the browser can deliver several move events between two
   * rAF callbacks, and integrating each one separately would make the turn rate depend on event
   * density rather than on how far the hand moved. */
  void AddLook(double dxPx, double dyPx) { LookX_ += dxPx; LookY_ += dyPx; }

  const Outshine::Stance &Step(double dtS);
  const Outshine::Stance &At() const { return At_; }

private:
  Outshine::Stance At_;
  bool Held_[(int)Move::Count] = {};
  bool Fast_ = false;
  double LookX_ = 0.0, LookY_ = 0.0;
};

} // namespace outshine::Clients
#endif
