#ifndef OUTSHINE_GENERATORS_GENERATORSET_H
#define OUTSHINE_GENERATORS_GENERATORSET_H

#include <vector>

#include "Generator.h"
#include "Rank.h"
#include "Span.h"
#include "Yield.h"

namespace outshine::Generators {

class GeneratorSet {
public:

  [[nodiscard]] bool Add(Rank rank, const Generator &generator);

  [[nodiscard]] size_t Count() const { return Entries_.size(); }
  [[nodiscard]] Rank RankAt(size_t i) const { return Entries_[i].R; }
  [[nodiscard]] const Generator &At(size_t i) const { return *Entries_[i].G; }

  void Occupy(const Ground &ground, Span<Yield> yields) const noexcept;

private:
  struct Entry {
    Rank R;
    const Generator *G;
  };

  std::vector<Entry> Entries_;
};

}
#endif
