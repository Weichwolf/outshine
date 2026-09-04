#ifndef OUTSHINE_GENERATORS_SCHEDULE_H
#define OUTSHINE_GENERATORS_SCHEDULE_H

#include <optional>
#include <vector>

#include "Tile.h"

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

  [[nodiscard]] std::optional<Tile> At(size_t i, LongitudeLatitude over) const;

  [[nodiscard]] std::optional<Tile> Widest(LongitudeLatitude over) const;

  [[nodiscard]] Tile Broadest() const;

private:
  struct Offset {
    int X, Y;
  };

  int Zoom_;
  std::vector<Offset> Offsets_;
};

} // namespace outshine::Generators
#endif
