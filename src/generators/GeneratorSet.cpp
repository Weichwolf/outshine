#include "GeneratorSet.h"

#include <cassert>

namespace outshine::Generators {

bool GeneratorSet::Add(Rank rank, const Generator &generator) {
  size_t at = 0;
  while (at < Entries_.size() && Entries_[at].R < rank) at++;
  if (at < Entries_.size() && Entries_[at].R == rank) return false;
  Entries_.insert(Entries_.begin() + (long)at, Entry{rank, &generator});
  return true;
}

void GeneratorSet::Occupy(const Ground &ground, Span<Yield> yields) const noexcept {
  assert(yields.Size() == Entries_.size());
  for (size_t i = 0; i < Entries_.size(); i++) Entries_[i].G->Occupy(ground, yields[i]);
}

}
