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

  [[nodiscard]] bool Add(Rank rank, const DrawSource &source);

  size_t Count() const { return Entries_.size(); }

  void Draw(const Ground &ground, const GeneratorSet &generators, Span<const Yield> yields,
            Span<const Body> placed, DrawSink &sink) const noexcept;

private:
  struct Entry {
    Rank R;
    const DrawSource *S;
  };

  std::vector<Entry> Entries_;
};

}
#endif
