#ifndef OUTSHINE_AUDIO_SOURCEBINDINGS_H
#define OUTSHINE_AUDIO_SOURCEBINDINGS_H

#include <cstddef>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace outshine::Audio {

class BusGraph;
struct Heard;
struct SoundSource;

struct SourceBindingCost {
  size_t ClearedRows = 0;
  size_t IndexLookups = 0;
};

class SourceBindings {
public:
  void Prepare(std::span<const SoundSource> declared, const BusGraph &routing);

  // Returned pointers borrow the input; the row view lasts until the next Resolve/Prepare.
  [[nodiscard]] std::span<const Heard *const> Resolve(std::span<const Heard> heard) noexcept;

  [[nodiscard]] double Gain(size_t row) const noexcept { return Gains_[row]; }

  [[nodiscard]] SourceBindingCost Cost() const noexcept { return Cost_; }

private:
  std::unordered_map<std::string, size_t> Index_;
  std::vector<const Heard *> Rows_;
  std::vector<double> Gains_;
  SourceBindingCost Cost_;
};

}
#endif
