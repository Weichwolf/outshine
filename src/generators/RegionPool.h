#ifndef OUTSHINE_GENERATORS_REGIONPOOL_H
#define OUTSHINE_GENERATORS_REGIONPOOL_H

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "ContactMaterial.h"
#include "Ground.h"
#include "OccupancySink.h"
#include "Tile.h"

namespace outshine::Generators {

constexpr double kCellPerRung = 8.0;

class RegionPool {
public:
  struct Shape {
    int Sinks = 1;

    uint32_t BodyCapacity = 4096;

    double CellM = 8.0;
  };

  struct Extent {
    Tile Reached;
    Tile Anywhere;
  };

  RegionPool(const Extent &extent, const Shape &shape);

  class Lease {
  public:
    Lease(Lease &&other) noexcept;
    Lease &operator=(Lease &&other) noexcept;
    Lease(const Lease &) = delete;
    Lease &operator=(const Lease &) = delete;
    ~Lease();

    [[nodiscard]] OccupancySink &Sink() const { return *Sink_; }

  private:
    Lease(RegionPool &pool, size_t slot);
    friend class RegionPool;

    RegionPool *Pool_;
    size_t Slot_;
    OccupancySink *Sink_;
  };

  std::optional<Lease> TryAcquire(const Ground &ground);
  [[nodiscard]] size_t Free() const;

  [[nodiscard]] size_t HeapBytes() const { return Bytes_; }

  [[nodiscard]] size_t SlotBytes() const { return Slots_.empty() ? 0 : Bytes_ / Slots_.size(); }

private:
  void Release(size_t slot);

  struct Slot {
    std::vector<Body> Bodies;
    std::vector<uint32_t> Links, Cells;
    std::unique_ptr<OccupancySink> Sink;
    bool Out = false;
  };

  std::vector<Slot> Slots_;
  size_t Bytes_ = 0;
};

} // namespace outshine::Generators
#endif
