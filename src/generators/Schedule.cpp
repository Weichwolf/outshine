#include "Schedule.h"

#include <algorithm>

namespace outshine::Generators {

Schedule::Schedule(const Ring &ring) : Zoom_(ring.Zoom) {
  const int r = ring.RadiusRegions < 0 ? 0 : ring.RadiusRegions;
  Offsets_.reserve((size_t)(2 * r + 1) * (size_t)(2 * r + 1));
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++) Offsets_.push_back(Offset{x, y});
  std::stable_sort(Offsets_.begin(), Offsets_.end(), [](const Offset &a, const Offset &b) {
    const int da = a.X * a.X + a.Y * a.Y, db = b.X * b.X + b.Y * b.Y;
    if (da != db) return da < db;
    if (a.Y != b.Y) return a.Y < b.Y;
    return a.X < b.X;
  });
}

std::optional<Tile> Schedule::At(size_t i, double lat, double lon) const {
  if (i >= Offsets_.size()) return std::nullopt;
  const Tile centre = Tile::Of(Zoom_, lat, lon);
  const int side = 1 << Zoom_;
  const int y = centre.Y() + Offsets_[i].Y;
  if (y < 0 || y >= side) return std::nullopt;
  return Tile(Zoom_, ((centre.X() + Offsets_[i].X) % side + side) % side, y);
}

Tile Schedule::Broadest() const { return Tile(Zoom_, 0, 1 << (Zoom_ - 1)); }

std::optional<Tile> Schedule::Widest(double lat, double lon) const {
  std::optional<Tile> widest;
  for (size_t i = 0; i < Offsets_.size(); i++) {
    const std::optional<Tile> r = At(i, lat, lon);
    if (!r) continue;
    if (!widest || r->SpanEm() * r->SpanNm() > widest->SpanEm() * widest->SpanNm()) widest = r;
  }
  return widest;
}

}
