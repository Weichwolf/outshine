#ifndef OUTSHINE_GENERATORS_DRAW_DRAWSET_H
#define OUTSHINE_GENERATORS_DRAW_DRAWSET_H

#include <span>
#include <vector>

#include "DrawSource.h"
#include "GeneratorSet.h"
#include "Rank.h"
#include "Yield.h"

namespace outshine::Generators {

class DrawSet {
public:
  [[nodiscard]] bool Add(Rank rank, const DrawSource &source);

  [[nodiscard]] size_t Count() const { return Entries_.size(); }

  void Draw(const Ground &ground,
            const GeneratorSet &generators,
            std::span<const Yield> yields,
            std::span<const Body> placed,
            DrawSink &sink) const noexcept;

private:
  struct Entry {
    Rank R;
    const DrawSource *S;
  };

  std::vector<Entry> Entries_;
};

} // namespace outshine::Generators
#endif
