#ifndef OUTSHINE_GENERATORS_BASE_YIELD_H
#define OUTSHINE_GENERATORS_BASE_YIELD_H

#include <span>
#include <array>
#include <cstddef>
#include <cstdint>

#include "ContactMaterial.h"
#include "Claim.h"
#include "OccupancySink.h"
#include "Rank.h"

namespace outshine::Generators {

class Yield {
public:
  struct Note {
    const char *Name = nullptr;
    uint32_t Times = 0;
    double Peak = 0.0;
    bool Raised = false;
  };

  Yield(OccupancySink &space, std::span<const char *const> names, std::span<Note> notes) noexcept;

  [[nodiscard]] Claim Place(const Body &body) noexcept;

  void Count(size_t name) noexcept { Notes_[name].Times++; }

  void Raise(size_t name, double value) noexcept {
    Note &note = Notes_[name];
    if (!note.Raised || value > note.Peak) { note.Peak = value; }
    note.Raised = true;
    note.Times++;
  }

  [[nodiscard]] BodyRange Placed() const noexcept { return Range_; }

  [[nodiscard]] std::span<const Note> Notes() const noexcept { return Notes_; }

  [[nodiscard]] uint32_t Claims(Claim::Outcome why) const noexcept {
    return Claims_[static_cast<size_t>(why)];
  }

private:
  OccupancySink *Space_;
  std::span<Note> Notes_;
  BodyRange Range_;
  std::array<uint32_t, Claim::kOutcomes> Claims_ = {{0, 0, 0, 0}};
};

} // namespace outshine::Generators
#endif
