/* The published class grid: constructed once, read through `shared_ptr<const>` and never written
 * again. `board/active/` §1 carries why that is the whole safety argument. */
#ifndef CLASSSTRUCTURE_H
#define CLASSSTRUCTURE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "TangentFrame.h"

namespace outshine {

class ClassStructure {
public:
  /* A rebuild of one grid hands the other on unchanged, which is why a grid is shared and never
   * copied. */
  /* `base` is the winning class of every feature covering the cell WITHOUT a boundary in it, so the
   * common case costs one byte and no geometry; a `seed` carries the winding number at the cell's
   * south-west corner plus that cell's edges of one feature, and a reader walks two axis-aligned legs
   * that cannot leave the cell, so only these edges can cross them and the winding stays exact. */
  struct Grid {
    std::vector<uint32_t> Cells;   /* 2 u32: [base | rank<<8 | seedCount<<16], seedFirst */
    std::vector<uint32_t> Seeds;   /* 3 u32: [tpl | rank<<8 | refCount<<16 | (wind+128)<<24], refFirst, halfWidth */
    std::vector<uint32_t> Refs;    /* edge index */
    std::vector<float> Edges;      /* x0,y0,x1,y1 */
    int W = 0, H = 0;
    double OrgE = 0, OrgN = 0, CellM = 1;
  };

  struct Measures {
    long Edges = 0, Seeds = 0, Probes = 0, NoData = 0;
    int Overflow = 0;
    double BuildMs = 0.0, PackMs = 0.0;
  };

  ClassStructure(const TangentFrame &frame, std::shared_ptr<const Grid> fine,
                 std::shared_ptr<const Grid> coarse, uint64_t version, int unmappedRow,
                 double buildMs, int overflow);

  /* The plane the words' metres are measured in, so a caller holding the structure needs nothing
   * else to ask about a geodetic place. */
  const TangentFrame &Frame() const { return Frame_; }
  const uint32_t *Words() const { return Words_.data(); }
  size_t Bytes() const { return Words_.size() * sizeof(uint32_t); }
  uint64_t Version() const { return Version_; }
  const Measures &Measured() const { return Measures_; }
  double NoDataFraction() const {
    return Measures_.Probes ? (double)Measures_.NoData / (double)Measures_.Probes : 0.0;
  }

  /* No boundary of the winning class lies in the acceleration cell the sample fell in, so the
   * structure knows only that the nearest one is farther than that cell — a state, and every reader
   * of `distM` has to say what it does with it. */
  static constexpr double kNoEdgeM = 1.0e30;

  /* THE ONE EVALUATOR, in C++ — a device half would read the same rule off the same words. -1 = no datum at
   * this place, which is a state and not a default: the caller decides what to do with it. */
  int Evaluate(double e, double n, double *distM, int *runnerUp) const;

private:
  void Pack(int unmappedRow);
  void Probe();

  TangentFrame Frame_;
  std::shared_ptr<const Grid> Fine_, Coarse_;
  std::vector<uint32_t> Words_;
  Measures Measures_;
  uint64_t Version_;
};

} // namespace outshine
#endif
