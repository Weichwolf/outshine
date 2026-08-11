#ifndef REGIONPOOL_H
#define REGIONPOOL_H

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "Body.h"
#include "Ground.h"
#include "OccupancySink.h"
#include "Schedule.h"

namespace outshine::Generators {

/* The work buffers a region is generated into, taken once at bring-up and handed round afterwards:
 * with them the hot path allocates nothing, and the fixed heap can be told what they cost. */
class RegionPool {
public:
  /* How much a buffer set holds — never how far it reaches. The extent is the region's own, and a
   * lease takes it from the ground it is opened on. */
  struct Shape {
    int Sinks = 1;
    /* [SET] bodies per region. At the forest's 3.33 m scatter step a z14 region proposes 204 304
     * candidates, so this is a ceiling on what may STAND, not on what is tried; the number that
     * replaces it is the placed count step 6 measures over the reference ring. */
    uint32_t BodyCapacity = 4096;
    /* [SET] metres per occupancy cell. Above the widest contact cylinder a generator declares, so
     * a conflict query reads a 3x3 neighbourhood; below it the query grows and the scan with it. */
    double CellM = 8.0;
  };

  /* Sized for the largest region the schedule can ever hand out, so no lease can be too small. */
  RegionPool(const Schedule &schedule, const Shape &shape);

  class Lease {
  public:
    Lease(Lease &&other) noexcept;
    Lease &operator=(Lease &&other) noexcept;
    Lease(const Lease &) = delete;
    Lease &operator=(const Lease &) = delete;
    ~Lease();

    OccupancySink &Sink() const { return *Sink_; }

  private:
    Lease(RegionPool &pool, size_t slot);
    friend class RegionPool;

    RegionPool *Pool_;
    size_t Slot_;
    OccupancySink *Sink_;
  };

  /* None when every buffer is out — the caller's answer to that is a smaller ring, never a wait.
   * The sink comes back open on this ground and on nothing else. */
  std::optional<Lease> TryAcquire(const Ground &ground);
  size_t Free() const;
  size_t HeapBytes() const { return Bytes_; }

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
