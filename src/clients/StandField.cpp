#include "StandField.h"

#include <cmath>

namespace outshine::Clients {

void StandField::Reserve(uint32_t stands) {
  Cap_ = stands;
  Stands_.reserve((size_t)stands * kFloats);
}

void StandField::Aim(const Lens &lens, const Generators::TreePrototype::Crown &crown) {
  Lens_ = lens;
  Crown_ = crown;
  Stands_.clear();
  Beyond_ = 0;
  InCrown_ = 0;
  Refused_ = 0;
  Nearest_ = Lens_.ReachM;
  Farthest_ = 0.0;
}

bool StandField::Add(Generators::BodyId, Generators::ClusterId,
                     const Generators::Instance &instance) noexcept {
  if (Full()) {
    Refused_++;
    return false;
  }
  /* Through geodetic coordinates, the same way the class was asked: the region's metres and the
   * lens's metres are anchored differently, and the two agree only where both are written out. */
  double lat = 0.0, lon = 0.0;
  Region_.Geo((double)instance.Em, (double)instance.Nm, &lat, &lon);
  double eastM = 0.0, northM = 0.0;
  Lens_.Frame.Project(lat, lon, &eastM, &northM);
  const double distSq = eastM * eastM + northM * northM;
  if (distSq >= Lens_.ReachM * Lens_.ReachM) {
    Beyond_++;
    return true;
  }
  if (Crown_.HeightM > 0.0f && Crown_.HalfWidth > 0.0f) {
    const double heightM = (double)Crown_.HeightM * (double)instance.Scale;
    const double half = heightM * (double)Crown_.HalfWidth;
    if (Lens_.AslM > (double)instance.AslM + heightM * (double)Crown_.Bottom &&
        Lens_.AslM < (double)instance.AslM + heightM * (double)Crown_.Top && distSq < half * half) {
      InCrown_++;
      return true;
    }
  }
  const double distM = std::sqrt(distSq);
  if (distM < Nearest_) Nearest_ = distM;
  if (distM > Farthest_) Farthest_ = distM;
  Stands_.push_back((float)eastM);
  Stands_.push_back((float)northM);
  Stands_.push_back(instance.AslM);
  Stands_.push_back(instance.YawRad);
  Stands_.push_back(instance.Scale);
  return true;
}

} // namespace outshine::Clients
