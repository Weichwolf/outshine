#ifndef OUTSHINE_GENERATE_H
#define OUTSHINE_GENERATE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Geometry.h"

namespace outshine {

struct Ask {
  double EastM = 0.0;
  double NorthM = 0.0;
  double ExtentM = 0.0;
  uint64_t Seed = 0;
};

/// A generator's request that the ground become FLAT under what it made, and OPTIONAL by design: a
/// generator standing on level ground returns none, and the terrain is then untouched byte for
/// byte.
///
/// The generator DECLARES and the ground APPLIES, because a generator does not own the ground and a
/// second writer of one field is what makes two subsystems disagree about the same place. It is
/// also what makes the operation orderable: two stamps that overlap disagree, and a ground that
/// applied them in completion order would render different bytes twice from one declaration.
struct Stamp {
  /// The footprint as east/north pairs in world metres, left open -- the ring closes at its first
  /// point rather than repeating it, so a reader cannot disagree with a writer about whether the
  /// last pair is the first.
  std::vector<double> RingEastNorthM;

  /// What the ground becomes inside the ring: the MEAN height over the footprint rather than its
  /// highest point, because a site balances cut against fill before it builds. A pad seated at the
  /// highest corner would bury the low side of every sloping plot.
  double PlateauAslM = 0.0;

  /// How far outside the ring the ground blends back to what it was. The blend is cosine-weighted,
  /// so the pad is left at zero slope and the terrain rejoined at zero slope -- a linear ramp
  /// leaves two creases where a viewer's eye goes first.
  double FalloffM = 0.0;
};

class Generates {
public:
  virtual ~Generates() = default;
  Generates(const Generates &) = delete;
  Generates &operator=(const Generates &) = delete;

  [[nodiscard]] virtual std::string_view kind() const = 0;
  [[nodiscard]] virtual bool make(const Ask &ask, Geometry &into) const = 0;

  /// What the ground must BECOME for this to stand, or nothing at all. The default answers nothing,
  /// so a generator that needs no plateau says so by saying nothing and never has to know this verb
  /// exists.
  ///
  /// @param ask  the same window `make` was asked for, so a stamp and the geometry it carries
  ///             cannot disagree about where they are
  /// @param into the stamps are APPENDED, because one generator may plateau several sites in one
  ///             window and the caller collects across generators
  /// @return whether anything was appended. False and an empty `into` mean the same thing; the
  ///         return exists so a caller need not compare sizes to find out.
  [[nodiscard]] virtual bool stamps(const Ask &ask, std::vector<Stamp> &into) const {
    (void)ask;
    (void)into;
    return false;
  }

protected:
  Generates() = default;
};

enum class Ships { Structures, kCount };

inline constexpr std::string_view kShipped[] = {"structures"};

static_assert(sizeof kShipped / sizeof kShipped[0] == (size_t)Ships::kCount,
              "every shipped generator the catalogue enumerates carries a name");

[[nodiscard]] constexpr std::string_view nameOf(Ships which) {
  return kShipped[(size_t)which];
}

[[nodiscard]] constexpr bool EveryShippedKindIsSpelled() {
  for (size_t at = 0; at < (size_t)Ships::kCount; ++at) {
    if (kShipped[at].empty()) { return false; }
    for (size_t over = at + 1; over < (size_t)Ships::kCount; ++over) {
      if (kShipped[at] == kShipped[over]) { return false; }
    }
  }
  return true;
}

static_assert(EveryShippedKindIsSpelled(),
              "a shipped kind is spelled once and is never empty -- a catalogue that carries a "
              "blank or a repeat resolves a declaration by whichever entry it reaches first");

[[nodiscard]] bool writeGlb(const Geometry &what, std::vector<uint8_t> &glb, std::string &error);

class Makers {
public:
  [[nodiscard]] bool offers(const Generates &maker);

  [[nodiscard]] const Generates *named(std::string_view kind) const;
  [[nodiscard]] size_t count() const;

  Makers();
  ~Makers();
  Makers(Makers &&) noexcept;
  Makers &operator=(Makers &&) noexcept;
  Makers(const Makers &) = delete;
  Makers &operator=(const Makers &) = delete;

private:
  struct Kept;
  std::unique_ptr<Kept> Kept_;
};

} // namespace outshine

#endif
