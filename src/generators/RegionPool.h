#ifndef REGIONPOOL_H
#define REGIONPOOL_H

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "Body.h"
#include "Ground.h"
#include "OccupancySink.h"
#include "Region.h"

namespace outshine::Generators {

/* The work buffers a region is generated into, taken once at bring-up and handed round afterwards:
 * with them the hot path allocates nothing, and the fixed heap can be told what they cost. */
class RegionPool {
public:
  /* How much a buffer set holds — never how far it reaches. The extent is the region's own, and a
   * lease takes it from the ground it is opened on. */
  struct Shape {
    int Sinks = 1;
    /* Bodies per region, and it is a ceiling on what may STAND and not on what is tried. Derived
     * from the generators' own declarations (`Generator::Proposes`) over the region below, so a
     * table edited denser moves it without anybody remembering to. */
    uint32_t BodyCapacity = 4096;
    /* [SET] metres per occupancy cell. Above the widest contact cylinder a generator declares, so
     * a conflict query reads a 3x3 neighbourhood; below it the query grows and the scan with it. */
    double CellM = 8.0;
  };

  /* THE TWO REGIONS A SLOT IS SIZED ON, and they are deliberately not the same one.
   *
   * The BODY budget follows `Reached`, the widest region the ring will name at the standpoint: an
   * equatorial z14 region is 2.66 times the area of one at 52 deg N, and sizing every slot for a
   * place the scene does not stand at is 2.66 times the budget spent on nothing. A scenario that
   * travels far enough equatorward outgrows it, and the answer to that is a refused region, not a
   * wider slot — the count travels out in the yield and nothing is truncated.
   *
   * The CELL array follows `Anywhere`, the widest region the ZOOM can name. A lattice index past the
   * end of that array is a write into the next slot, and no refusal can catch it because the index
   * is computed before anything is claimed. Two thirds of the 2.66x saving lives in the body array
   * and is untouched; what this costs is the cells alone. */
  struct Extent {
    Region Reached;
    Region Anywhere;
  };
  RegionPool(const Extent &extent, const Shape &shape);

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
  /* What one lease costs, which is the byte half of a region's own line. */
  size_t SlotBytes() const { return Slots_.empty() ? 0 : Bytes_ / Slots_.size(); }

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
