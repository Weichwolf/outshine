#ifndef OUTSHINE_GENERATE_H
#define OUTSHINE_GENERATE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "scene/Geometry.h"

namespace outshine::Generators {

/// What a generator may ask about the ground it is standing something on, supplied by whoever owns
/// that ground rather than reached for.
///
/// A generator that read the engine's terrain directly would link the engine, and the generators
/// are a tier that links with none of it -- that separation is what lets a corpus score a
/// derivation without booting a renderer. So the caller passes an answerer and the generator asks.
class HeightSampler {
public:
  virtual ~HeightSampler() = default;
  HeightSampler(const HeightSampler &) = delete;
  HeightSampler &operator=(const HeightSampler &) = delete;

  /// The ground's height above the ellipsoid at a place, or a refusal.
  ///
  /// @param latDeg latitude in degrees
  /// @param lonDeg longitude in degrees
  /// @param into   the height in metres, written only when the answer is yes
  /// @return whether the ground is KNOWN there. False is not zero: a tile that has not arrived and
  ///         a sea-level plain are different answers, and a generator that cannot tell them apart
  ///         builds a house at zero.
  [[nodiscard]] virtual bool sampleHeightAslM(double latDeg, double lonDeg, double &into) const = 0;

protected:
  HeightSampler() = default;
};

/// Where a generator is asked to make something, and what it may ask about that place.
struct Request {
  /// The centre, in DEGREES. A generator that reads a public map has to know where on Earth it is,
  /// and a local metre offset with no origin cannot say. The fields carried metres in their names
  /// and degrees in their values until board:2083 measured it.
  double LatitudeDeg = 0.0;

  /// The centre's longitude, in degrees.
  double LongitudeDeg = 0.0;

  /// How far the window reaches, in metres.
  double ExtentM = 0.0;

  /// The seed every random choice descends from, so one declaration makes one world twice.
  uint64_t Seed = 0;

  /// The ground beneath, or nothing when the caller has none to offer. A generator that needs a
  /// height and is given no answerer refuses rather than assuming a plain at zero.
  const HeightSampler *Ground = nullptr;
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

class Generator {
public:
  virtual ~Generator() = default;
  Generator(const Generator &) = delete;
  Generator &operator=(const Generator &) = delete;

  [[nodiscard]] virtual std::string_view kind() const = 0;
  [[nodiscard]] virtual bool make(const Request &asked, Geometry &into) const = 0;

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
  [[nodiscard]] virtual bool stamps(const Request &asked, std::vector<Stamp> &into) const {
    (void)asked;
    (void)into;
    return false;
  }

protected:
  Generator() = default;
};

/// The generators this engine ships with, and the catalogue is CLOSED: a client registers its own
/// beside them rather than adding a value here.
enum class Shipped : uint8_t { Structures, kCount };

/// The name each shipped kind answers to in a declaration, in the order the enum names them.
inline constexpr std::array<std::string_view, static_cast<size_t>(Shipped::kCount)> kShipped = {
    "structures"};

[[nodiscard]] constexpr std::string_view nameOf(Shipped which) {
  return kShipped[static_cast<size_t>(which)];
}

[[nodiscard]] constexpr bool EveryShippedKindIsSpelled() {
  for (size_t at = 0; at < static_cast<size_t>(Shipped::kCount); ++at) {
    if (kShipped[at].empty()) { return false; }
    for (size_t over = at + 1; over < static_cast<size_t>(Shipped::kCount); ++over) {
      if (kShipped[at] == kShipped[over]) { return false; }
    }
  }
  return true;
}

static_assert(EveryShippedKindIsSpelled(),
              "a shipped kind is spelled once and is never empty -- a catalogue that carries a "
              "blank or a repeat resolves a declaration by whichever entry it reaches first");

[[nodiscard]] bool writeGlb(const Geometry &what, std::vector<uint8_t> &glb, std::string &error);

class Registry {
public:
  [[nodiscard]] bool offers(const Generator &maker);

  [[nodiscard]] const Generator *named(std::string_view kind) const;
  [[nodiscard]] size_t count() const;

  Registry();
  ~Registry();
  Registry(Registry &&) noexcept;
  Registry &operator=(Registry &&) noexcept;
  Registry(const Registry &) = delete;
  Registry &operator=(const Registry &) = delete;

private:
  struct Kept;
  std::unique_ptr<Kept> Kept_;
};

} // namespace outshine::Generators

#endif
