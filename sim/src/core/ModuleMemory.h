/* ONE WASM MODULE'S MEMORY, as that module itself sees it. The client is not one linear memory but
 * several — the main module plus the tile workers, each its own address space — and a module can
 * only measure its own. This is what crosses the postMessage channel so the ledger can state the
 * client as main + the workers instead of main alone.
 *
 * All bytes, all double, because the crossing is JavaScript: the receiving side copies words, and
 * the static assertion below is the only place the two languages agree on how many. */
#ifndef MODULEMEMORY_H
#define MODULEMEMORY_H

#include "HeapProbe.h"
#include "StackProbe.h"

namespace outshine {

struct ModuleMemory {
  double HeapBytes = 0;
  double HeapPeakBytes = 0;
  double ReservedBytes = 0;
  double StackPeakBytes = 0;
  double StackFloorBytes = 0;
  double StackLimitBytes = 0;
  double StackCapacityBytes = 0;

  static constexpr int kWordCount = 7;

  /* The one stack this module runs, named by what it is for. A module with several threads publishes
   * them separately and does not travel through here. */
  static ModuleMemory Of(StackProbe::Purpose purpose) {
    HeapProbe::Sample();
    ModuleMemory m;
    m.HeapBytes = (double)HeapProbe::Bytes();
    m.HeapPeakBytes = (double)HeapProbe::PeakBytes();
    m.ReservedBytes = (double)HeapProbe::ReservedBytes();
    m.StackPeakBytes = (double)StackProbe::PeakBytes(purpose);
    m.StackFloorBytes = (double)StackProbe::FloorBytes(purpose);
    m.StackLimitBytes = (double)StackProbe::LimitBytes(purpose);
    m.StackCapacityBytes = (double)StackProbe::CapacityBytes(purpose);
    return m;
  }
};

static_assert(sizeof(ModuleMemory) == ModuleMemory::kWordCount * sizeof(double),
              "the JavaScript side copies kWordCount doubles and knows nothing else about this type");

} // namespace outshine
#endif
