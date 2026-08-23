#ifndef OUTSHINE_GENERATORS_COVER_H
#define OUTSHINE_GENERATORS_COVER_H

#include <cstdint>

namespace outshine::Generators {

class Cover {
public:
  static Cover None() { return Cover(); }

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

  [[nodiscard]] bool TryRow(int *out) const {
    if (Row_ < 0) return false;
    *out = Row_;
    return true;
  }

  [[nodiscard]] bool TryRunnerUp(int *out) const {
    if (RunnerUp_ < 0) return false;
    *out = RunnerUp_;
    return true;
  }

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

}
#endif
