#include <span>
#include "DrawSet.h"

#include <cassert>
#include <cstddef>

namespace outshine::Generators {

bool DrawSet::Add(Rank rank, const DrawSource &source) {
  for (const Entry &e : Entries_) {
    if (e.R == rank) { return false; }
  }
  Entries_.push_back(Entry{.R = rank, .S = &source});
  return true;
}

void DrawSet::Draw(const Ground &ground,
                   const GeneratorSet &generators,
                   std::span<const Yield> yields,
                   std::span<const Body> placed,
                   DrawSink &sink) const noexcept {
  assert(yields.size() == generators.Count());
  for (size_t i = 0; i < generators.Count(); i++) {
    const BodyRange range = yields[i].Placed();
    for (const Entry &e : Entries_) {
      if (e.R != generators.RankAt(i)) { continue; }
      e.S->Draw(ground, placed.subspan(range.First, range.Count), range, sink);
    }
  }
}

} // namespace outshine::Generators
