/* WHAT ONE GENERATOR PRODUCED FOR ONE REGION: the space it claimed, and everything it wants said
 * about what it did not claim. The names are the generator's own — the engine stores them by index
 * and never reads one, so nothing here is a taxonomy of content. */
#ifndef YIELD_H
#define YIELD_H

#include <cstddef>
#include <cstdint>

#include "Body.h"
#include "Claim.h"
#include "OccupancySink.h"
#include "Rank.h"
#include "Span.h"

namespace outshine::Generators {

class Yield {
public:
  /* A counter and a high-water under one name, because a generator has both kinds — how often it
   * refused for a reason, and how far the highest thing it placed reached. */
  struct Note {
    const char *Name = nullptr;
    uint32_t Times = 0;
    double Peak = 0.0;
    bool Raised = false;
  };

  /* `notes` is the caller's storage, one entry per declared name. */
  Yield(OccupancySink &space, Span<const char *const> names, Span<Note> notes) noexcept;

  [[nodiscard]] Claim Place(const Body &body) noexcept;
  void Count(size_t name) noexcept { Notes_[name].Times++; }
  void Raise(size_t name, double value) noexcept {
    Note &note = Notes_[name];
    if (!note.Raised || value > note.Peak) note.Peak = value;
    note.Raised = true;
    note.Times++;
  }

  /* What this generator claimed of the sink it shares with the others, in the order it claimed it. */
  BodyRange Placed() const noexcept { return Range_; }
  Span<const Note> Notes() const noexcept { return Notes_; }
  uint32_t Claims(Claim::Outcome why) const noexcept { return Claims_[(size_t)why]; }

private:
  OccupancySink *Space_;
  Span<Note> Notes_;
  BodyRange Range_;
  uint32_t Claims_[Claim::kOutcomes] = {0, 0, 0, 0};
};

} // namespace outshine::Generators
#endif
