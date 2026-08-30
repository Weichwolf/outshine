#include "DrawSet.h"

#include <cassert>

namespace outshine::Generators {

bool DrawSet::Add(Rank rank, const DrawSource &source) {
  for (const Entry &e : Entries_) {
    if (e.R == rank) { return false; }
  }
  Entries_.push_back(Entry{rank, &source});
  return true;
}

void DrawSet::Draw(const Ground &ground,
                   const GeneratorSet &generators,
                   Span<const Yield> yields,
                   Span<const Body> placed,
                   DrawSink &sink) const noexcept {
  assert(yields.Size() == generators.Count());
  for (size_t i = 0; i < generators.Count(); i++) {
    const BodyRange range = yields[i].Placed();
    for (const Entry &e : Entries_) {
      if (e.R != generators.RankAt(i)) { continue; }
      e.S->Draw(ground, placed.Sub(range.First, range.Count), sink);
    }
  }
}

}
