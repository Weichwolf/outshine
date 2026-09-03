#include "Rig.h"

#include <cmath>
#include <cstddef>

#include "math/Vec3.h"

namespace outshine::Physics {

using outshine::Cross;
using outshine::Dot;
using outshine::Normalise;

Reading Bear(Rig &of,
             const Rigid &body,
             const Footing *under,
             const Controls &with,
             Wrench &into,
             double dtS) {
  Reading out;
  out.Count = of.Count;

  for (size_t which = 0; which < out.Count; ++which) {
    const Mount &mount = of.Mounts[which];
    const Footing &ground = under[which];
    if (!ground.Found) {
      ++out.OffTheSurface;
      ++out.Airborne;
      of.HeldSlipRad[which] = 0.0;
      continue;
    }

    Vec3 normal = {{ground.NormalM[0], ground.NormalM[1], ground.NormalM[2]}};
    if (!Normalise(normal)) { continue; }

    Vec3 worldM;
    Vec3 worldMs;
    Place(body, mount.AtM, worldM);
    Carry(body, mount.AtM, worldMs);

    const double clearanceM = (worldM[1] - ground.HeightM) * normal[1];
    const double closingMs = -Dot(worldMs, normal);
    const Reaction against = Press(mount.Strut, {.ClearanceM = clearanceM, .ClosingMs = closingMs});

    out.Touching[which] = against.Touching;
    out.LoadN[which] = against.LoadN;
    out.PressedM[which] = against.PressedM;
    out.PastTravel = out.PastTravel || against.PastTravel;
    out.PastLimit = out.PastLimit || against.PastLimit;
    out.HeaviestN = std::fmax(out.HeaviestN, against.LoadN);
    if (!against.Touching) {
      ++out.Airborne;
      of.HeldSlipRad[which] = 0.0;
      continue;
    }

    const double steerRad = with.MotionRad * mount.Steering.Applied.Ratio;
    const Vec3 aheadBody = {{-std::sin(steerRad), 0.0, -std::cos(steerRad)}};
    Vec3 ahead;
    Turn(body.OrientationQ, aheadBody, ahead);
    const double onNormal = Dot(ahead, normal);
    for (int axis = 0; axis < 3; ++axis) { ahead[axis] -= onNormal * normal[axis]; }
    if (!Normalise(ahead)) { continue; }
    Vec3 across = Cross(normal, ahead);
    if (!Normalise(across)) { continue; }

    const double alongMs = Dot(worldMs, ahead);
    const double acrossMs = Dot(worldMs, across);
    const double rollingMs = std::fabs(alongMs);
    const double rawSlipRad = rollingMs > 0.0 ? std::atan2(-acrossMs, rollingMs) : 0.0;
    const double rolledM = rollingMs * dtS;
    of.HeldSlipRad[which] =
        Relaxed(mount.Sheds, {.WasRad = of.HeldSlipRad[which], .IsRad = rawSlipRad}, rolledM);

    double askedAlongN = with.AppliedN * mount.Spin.Applied.Ratio;
    const double brakingN = with.ResistingN * mount.Spin.Resisting.Ratio;
    if (brakingN > 0.0) { askedAlongN -= alongMs >= 0.0 ? brakingN : -brakingN; }

    Shearing sheds = mount.Sheds;
    sheds.Grip *= ground.Friction;
    const Shear shed = ShedAt(
        sheds,
        {.LoadN = against.LoadN, .SlipRad = of.HeldSlipRad[which], .AskedAlongN = askedAlongN});
    out.SlipRad[which] = of.HeldSlipRad[which];
    out.RatioOfHold[which] = shed.Ratio;
    out.Sliding = out.Sliding || shed.Sliding;
    out.WorstSlipRad = std::fmax(out.WorstSlipRad, std::fabs(of.HeldSlipRad[which]));
    out.WorstRatio = std::fmax(out.WorstRatio, shed.Ratio);

    Vec3 forceN;
    for (int axis = 0; axis < 3; ++axis) {
      forceN[axis] =
          normal[axis] * against.LoadN + ahead[axis] * shed.AlongN + across[axis] * shed.AcrossN;
    }

    Vec3 normalBody;
    Unturn(body.OrientationQ, normal, normalBody);
    const double standOffM = mount.Strut.ReachM - against.PressedM;
    Vec3 patchM;
    for (int axis = 0; axis < 3; ++axis) {
      patchM[axis] = mount.AtM[axis] - normalBody[axis] * standOffM;
    }
    Push(into, body, {.AtBodyM = patchM, .Newtons = forceN});
  }
  return out;
}

void Resist(Wrench &into, const Rigid &body, double dragArea, double mediumDensity) {
  const double speedMs = std::sqrt(Dot(body.VelocityMs, body.VelocityMs));
  if (!(speedMs > 0.0) || !(dragArea > 0.0) || !(mediumDensity > 0.0)) { return; }
  const double dragN = 0.5 * mediumDensity * dragArea * speedMs * speedMs;
  for (int axis = 0; axis < 3; ++axis) {
    into.ForceN[axis] -= dragN * body.VelocityMs[axis] / speedMs;
  }
}

} // namespace outshine::Physics
