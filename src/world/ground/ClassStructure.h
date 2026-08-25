#ifndef OUTSHINE_WORLD_GROUND_CLASSSTRUCTURE_H
#define OUTSHINE_WORLD_GROUND_CLASSSTRUCTURE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "TangentFrame.h"

namespace outshine {

class ClassStructure {
public:

  struct Grid {
    std::vector<uint32_t> Cells;
    std::vector<uint32_t> Seeds;
    std::vector<uint32_t> Refs;
    std::vector<float> Edges;
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

  const TangentFrame &Frame() const { return Frame_; }
  const uint32_t *Words() const { return Words_.data(); }
  size_t Bytes() const { return Words_.size() * sizeof(uint32_t); }
  uint64_t Version() const { return Version_; }
  const Measures &Measured() const { return Measures_; }
  double NoDataFraction() const {
    return Measures_.Probes ? (double)Measures_.NoData / (double)Measures_.Probes : 0.0;
  }

  static constexpr double kNoEdgeM = 1.0e30;

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

}
#endif
