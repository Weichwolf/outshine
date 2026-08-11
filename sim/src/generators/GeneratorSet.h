#ifndef GENERATORSET_H
#define GENERATORSET_H

#include <vector>

#include "Generator.h"
#include "Rank.h"
#include "Span.h"
#include "Yield.h"

namespace outshine::Generators {

class GeneratorSet {
public:
  /* False on a duplicate rank, which is a load error at the one call site that registers — never a
   * runtime coin toss over who claims first. */
  [[nodiscard]] bool Add(Rank rank, const Generator &generator);

  size_t Count() const { return Entries_.size(); }
  Rank RankAt(size_t i) const { return Entries_[i].R; }
  const Generator &At(size_t i) const { return *Entries_[i].G; }

  /* Runs every generator in rank order. `yields` is the caller's, one per registered generator and
   * in that order; each one already holds the sink they share and its own note storage. */
  void Occupy(const Ground &ground, Span<Yield> yields) const noexcept;

private:
  struct Entry {
    Rank R;
    const Generator *G;
  };

  std::vector<Entry> Entries_;
};

} // namespace outshine::Generators
#endif
