#ifndef OUTSHINE_GENERATORS_BASE_COVER_H
#define OUTSHINE_GENERATORS_BASE_COVER_H

#include <optional>
#include <cstdint>

namespace outshine::Generators {

class Cover {
public:
  static Cover None() { return {}; }

  static Cover Of(int row, int runnerUpRow) {
    Cover c;
    c.Row_ = static_cast<int16_t>(row);
    c.RunnerUp_ = static_cast<int16_t>(runnerUpRow);
    return c;
  }

  static Cover Of(int row, float edgeM, int runnerUpRow) {
    Cover c = Of(row, runnerUpRow);
    c.EdgeM_ = edgeM;
    c.HasEdge_ = true;
    return c;
  }

  [[nodiscard]] std::optional<int> Row() const {
    if (Row_ < 0) { return std::nullopt; }
    return Row_;
  }

private:
  float EdgeM_ = 0.0f;
  int16_t Row_ = -1, RunnerUp_ = -1;
  bool HasEdge_ = false;
};

} // namespace outshine::Generators
#endif
