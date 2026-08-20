#ifndef SCHEDULE_H
#define SCHEDULE_H

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

  int Zoom() const { return Zoom_; }
  size_t Count() const { return Offsets_.size(); }

  std::optional<Region> At(size_t i, double lat, double lon) const;

  std::optional<Region> Widest(double lat, double lon) const;

  Region Broadest() const;

private:
  struct Offset {
    int X, Y;
  };

  int Zoom_;
  std::vector<Offset> Offsets_;
};

}
#endif
