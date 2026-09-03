#ifndef OUTSHINE_GENERATORS_BASE_COVER_H
#define OUTSHINE_GENERATORS_BASE_COVER_H

#include <optional>
#include <cstdint>

namespace outshine::Generators {

class Cover {
public:
  static Cover None() { return {}; }

  struct Covering {
    int Row = -1;
    int RunnerUpRow = -1;
  };

  static Cover Of(Covering by) {
    Cover c;
    c.Row_ = static_cast<int16_t>(by.Row);
    c.RunnerUp_ = static_cast<int16_t>(by.RunnerUpRow);
    return c;
  }

  static Cover Of(Covering by, float edgeM) {
    Cover c = Of(by);
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
