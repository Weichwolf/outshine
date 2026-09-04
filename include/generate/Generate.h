#ifndef OUTSHINE_GENERATE_H
#define OUTSHINE_GENERATE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <optional>

#include "Earth.h"
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

  /// The ground's height above the ellipsoid at a place, in metres, or nothing.
  ///
  /// @param at the place; the height it carries is ignored, since that is what is being asked for
  /// @return the height where the ground is KNOWN. An empty answer is not zero: a tile that has not
  ///         arrived and a sea-level plain are different answers, and a generator that cannot tell
  ///         them apart builds a house at zero.
  [[nodiscard]] virtual std::optional<double>
  sampleHeightAslM(const LongitudeLatitudeHeight &at) const = 0;

protected:
  HeightSampler() = default;
};

/// How coarse a generator may build here, decided by the ENGINE and never by the generator.
///
/// A level of detail is the oldest idea in real-time graphics and this engine had no word for it:
/// measured 2026-09-04, `grep -rIn '\bLod\b|\bLOD\b'` over the whole tree returned zero, while
/// Shibuya meshed 575805 buildings at full standing -- 8.15 M triangles, 618 MB, and 14.2 triangles
/// a building, which is a cuboid. The geometry per building was already minimal; what was missing
/// was the decision not to build most of them finely.
///
/// **RAGE IS THE MODEL AND IT DECIDES THREE THINGS.** Its entities carry HD, LOD, SLOD1, SLOD2 and
/// SLOD3, and a coarser entity REPLACES the finer ones under it rather than standing beside them.
/// The choice is made by DISTANCE -- Unreal picks by screen size, RAGE by `lodDistance`, and
/// distance is what this engine already has in its tile cascade. And every level is BAKED when the
/// geometry is built, never in a frame, which is what keeps a frame from meshing.
///
/// So: the ground's tile rungs decide, the generator obeys, and one rung coarser ground means one
/// step coarser everything standing on it -- because it is the same distance away.
enum class Detail : uint8_t {
  /// Every facade, every branch, every kerb. RAGE's HD.
  Fine,
  /// The subject as one closed shell: a building keeps its footprint and height and loses its
  /// facades. RAGE's LOD.
  Shell,
  /// Neighbours MERGED, so a city block is one body rather than thirty. RAGE's SLOD1, and the
  /// reason a distant skyline does not shimmer: the merge is stable because the block is.
  Massed,
  /// A silhouette, and the last thing before the horizon. RAGE's SLOD2 and SLOD3.
  Skyline,
};

/// The detail a tile carries, given how many rungs coarser than the finest it is.
///
/// ONE RULE IN ONE PLACE. Every caller that hands a generator a window has to answer the same
/// question, and two callers answering it separately is how two subsystems come to disagree about
/// the same ground. The rungs are the ground's own cascade: a rung coarser is ground that is
/// further away, so what stands on it is further away by exactly as much.
[[nodiscard]] constexpr Detail DetailAtRung(int rungsCoarser) {
  if (rungsCoarser <= 0) { return Detail::Fine; }
  if (rungsCoarser == 1) { return Detail::Shell; }
  if (rungsCoarser == 2) { return Detail::Massed; }
  return Detail::Skyline;
}

static_assert(DetailAtRung(-1) == Detail::Fine, "a finer rung than the finest is still the finest");
static_assert(DetailAtRung(0) == Detail::Fine);
static_assert(DetailAtRung(1) == Detail::Shell);
static_assert(DetailAtRung(2) == Detail::Massed);
static_assert(DetailAtRung(9) == Detail::Skyline, "every rung beyond is the horizon");

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

  /// How coarse to build. A generator that ignores this builds a city at full detail to the
  /// horizon, which is measurable rather than theoretical: see `Detail`.
  Detail Coarseness = Detail::Fine;
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
