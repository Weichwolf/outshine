#ifndef COVER_H
#define COVER_H

#include <cstdint>

namespace outshine::Generators {

class Cover {
public:
  static Cover None() { return Cover(); }
  /* No boundary of this class is near enough for the classifier to have measured one. */
  static Cover Of(int row, int runnerUpRow) {
    Cover c;
    c.Row_ = (int16_t)row;
    c.RunnerUp_ = (int16_t)runnerUpRow;
    return c;
  }
  static Cover Of(int row, float edgeM, int runnerUpRow) {
    Cover c = Of(row, runnerUpRow);
    c.EdgeM_ = edgeM;
    c.HasEdge_ = true;
    return c;
  }

  /* The row of the declared table this place classifies as. False = OSM has no datum here, which
   * is a state and not a default. */
  [[nodiscard]] bool TryRow(int *out) const {
    if (Row_ < 0) return false;
    *out = Row_;
    return true;
  }
  /* What would win if this class were not here — the neighbour a boundary blends towards. */
  [[nodiscard]] bool TryRunnerUp(int *out) const {
    if (RunnerUp_ < 0) return false;
    *out = RunnerUp_;
    return true;
  }
  /* Metres to the nearest edge of this class. False where no edge was near enough to measure: a
   * number for that case would be a sentinel, and one of those cost the browser a forest already. */
  [[nodiscard]] bool TryEdgeM(float *out) const {
    if (!HasEdge_) return false;
    *out = EdgeM_;
    return true;
  }

private:
  float EdgeM_ = 0.0f;
  int16_t Row_ = -1, RunnerUp_ = -1;
  bool HasEdge_ = false;
};

} // namespace outshine::Generators
#endif
