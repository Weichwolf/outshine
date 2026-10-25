#include "SourceBindings.h"
#include "Mixer.h"
#include "BusGraph.h"
#include <algorithm>
#include <cstddef>
#include <span>

namespace outshine::Audio {

void SourceBindings::Prepare(std::span<const SoundSource> declared, const BusGraph &routing) {
  Index_.clear();
  Index_.reserve(declared.size());
  Rows_.assign(declared.size(), nullptr);
  Gains_.resize(declared.size());
  Cost_ = {};
  for (size_t row = 0; row < declared.size(); ++row) {
    Index_.emplace(declared[row].Id, row);
    Gains_[row] = routing.GainOf(declared[row].Id);
  }
}

std::span<const Heard *const> SourceBindings::Resolve(std::span<const Heard> heard) noexcept {
  std::ranges::fill(Rows_, nullptr);
  Cost_ = {.ClearedRows = Rows_.size()};
  for (const Heard &source : heard) {
    if (!source.Standing) { continue; }
    ++Cost_.IndexLookups;
    const auto row = Index_.find(source.Id);
    if (row != Index_.end()) { Rows_[row->second] = &source; }
  }
  return Rows_;
}

}
