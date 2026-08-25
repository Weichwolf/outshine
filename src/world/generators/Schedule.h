#ifndef OUTSHINE_WORLD_GENERATORS_SCHEDULE_H
#define OUTSHINE_WORLD_GENERATORS_SCHEDULE_H

#include <optional>
#include <vector>

#include "Region.h"

namespace outshine::Generators {

class Schedule {
public:

  struct Ring {

    int Zoom = 14;

    int RadiusRegions = 1;
  };

  explicit Schedule(const Ring &ring);

  [[nodiscard]] int Zoom() const { return Zoom_; }
  [[nodiscard]] size_t Count() const { return Offsets_.size(); }

  [[nodiscard]] std::optional<Region> At(size_t i, double lat, double lon) const;

  [[nodiscard]] std::optional<Region> Widest(double lat, double lon) const;

  [[nodiscard]] Region Broadest() const;

private:
  struct Offset {
    int X, Y;
  };

  int Zoom_;
  std::vector<Offset> Offsets_;
};

}
#endif
