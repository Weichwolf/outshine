#ifndef DRAWSET_H
#define DRAWSET_H

#include <vector>

#include "DrawSource.h"
#include "GeneratorSet.h"
#include "Rank.h"
#include "Span.h"
#include "Yield.h"

namespace outshine::Generators {

class DrawSet {
public:
  /* False on a duplicate rank, as with the generators. A rank with no generator draws nothing,
   * because there is nothing standing under it. */
  [[nodiscard]] bool Add(Rank rank, const DrawSource &source);

  size_t Count() const { return Entries_.size(); }

  /* Hands every source the bodies its own generator placed — which is why the ranges are read off
   * the yields of the pass that placed them and are stated nowhere else. */
  void Draw(const Ground &ground, const GeneratorSet &generators, Span<const Yield> yields,
            Span<const Body> placed, DrawSink &sink) const noexcept;

private:
  struct Entry {
    Rank R;
    const DrawSource *S;
  };

  std::vector<Entry> Entries_;
};

} // namespace outshine::Generators
#endif
