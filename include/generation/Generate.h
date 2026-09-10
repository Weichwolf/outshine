#ifndef OUTSHINE_GENERATION_GENERATE_H
#define OUTSHINE_GENERATION_GENERATE_H

#include <array>
#include <cmath>
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

/// Borrowed terrain-query interface, independent of the renderer and terrain storage.
/// The caller owns the implementation and keeps it alive throughout generation. Calls
/// are synchronous; latency, caching and concurrency depend on the implementation.
/// Missing terrain is distinct from a known zero-metre elevation. Implementations
/// return finite elevations when present and must not retain borrowed call arguments.
class HeightSampler {
public:
  /// Destroy the sampler through its interface; does not own the terrain provider.
  virtual ~HeightSampler() = default;
  /// Terrain-provider identity is not implicitly copied.
  HeightSampler(const HeightSampler &) = delete;
  /// Terrain-provider identity is not implicitly replaced.
  HeightSampler &operator=(const HeightSampler &) = delete;

  /// The source DEM's height above mean sea level (ASL), in metres, or nothing.
  ///
  /// @param at the place; the height it carries is ignored, since that is what is being asked for
  /// @return the height where the ground is KNOWN. An empty answer is not zero: a tile that has not
  ///         arrived and a sea-level plain are different answers, and a generator that cannot tell
  ///         them apart builds a house at zero.
  [[nodiscard]] virtual std::optional<double>
  sampleHeightAslM(const LongitudeLatitudeHeight &at) const = 0;

protected:
  /// Construct the interface subobject without allocation.
  HeightSampler() = default;
};

/// Representation classes a generator can produce. These labels do not prescribe distance
/// thresholds or prove geometric error bounds; the caller must assess the resulting product.
enum class Detail : uint8_t {
  Fine,    ///< Full generator detail.
  Shell,   ///< Exterior shell with secondary surface geometry omitted.
  Massed,  ///< Aggregated groups preserving their coarse volume.
  Skyline, ///< Coarse silhouette representation.
};

/// Map a relative tile rung to a bounded representation class.
/// @param rungsCoarser Rungs relative to the finest tile; nonpositive selects Fine.
/// @return Fine, Shell, Massed, or Skyline, saturating at Skyline from rung three onward.
[[nodiscard]] constexpr Detail DetailAtRung(int rungsCoarser) noexcept {
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

/// Allowed pinhole-projected geometric error, in pixels. One pixel is a quality policy,
/// not a guarantee of perceptual invisibility, silhouette stability or material fidelity.
inline constexpr double kErrorPx = 1.0;

/// Test whether a geometric error meets the projected-error policy.
/// @param errorM Finite nonnegative geometric displacement bound, in metres.
/// @param focalPx Finite positive focal length, in pixels.
/// @param awayM Finite positive distance used by the projection estimate, in metres.
/// @return True if all inputs are valid and errorM * focalPx <= kErrorPx * awayM.
/// Invalid values return false, including invalid projection values with zero error.
/// This function neither measures the displacement bound nor verifies occlusion.
[[nodiscard]] constexpr bool Unseen(double errorM, double focalPx, double awayM) noexcept {
  if (!std::isfinite(errorM) || errorM < 0.0 || !std::isfinite(focalPx) || !(focalPx > 0.0) ||
      !std::isfinite(awayM) || !(awayM > 0.0)) {
    return false;
  }
  return errorM * focalPx <= kErrorPx * awayM;
}

/// Select the coarser representation in constant time, without allocation.
/// @param one Valid representation class.
/// @param two Valid representation class.
/// @return The class with the greater coarseness; does not validate enum values.
[[nodiscard]] constexpr Detail Coarser(Detail one, Detail two) noexcept {
  return static_cast<uint8_t>(one) > static_cast<uint8_t>(two) ? one : two;
}

static_assert(Coarser(Detail::Fine, Detail::Massed) == Detail::Massed);
static_assert(Coarser(Detail::Skyline, Detail::Shell) == Detail::Skyline);

/// Borrowed, case-sensitive generator setting; values are not parsed or normalized.
/// Both views remain valid only during make/stamps. Copy their characters to retain them.
/// The receiving generator defines supported names, units, duplicates and value syntax.
struct Parameter {
  std::string_view Name;  ///< Borrowed setting identifier; no null terminator is promised.
  std::string_view Value; ///< Borrowed textual value interpreted by the receiving generator.
};

/// Value-only generation request with a borrowed terrain provider.
/// Copies do not extend Ground's lifetime. The generator borrows the request for the
/// duration of make/stamps; the caller keeps its values and terrain inputs stable.
/// Coordinates use WGS84 geodetic degrees. Concrete generators define supported
/// windows, missing-data handling and detail levels; this aggregate validates nothing.
struct Request {
  double LatitudeDeg = 0.0;  ///< Window centre latitude, finite and within [-90, 90] degrees.
  double LongitudeDeg = 0.0; ///< Window centre longitude, finite and within [-180, 180] degrees.
  double ExtentM = 0.0;      ///< Window extent in metres; interpretation is generator-specific.
  std::span<const Parameter> Parameters; ///< Borrowed ordered settings; valid only for this call.
  uint64_t Seed = 0; ///< Root seed for reproducible choices with unchanged input data.
  const HeightSampler *Ground = nullptr; ///< Borrowed terrain provider, or nullptr if unavailable.
  Detail Coarseness = Detail::Fine; ///< Requested representation; support is generator-specific.
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

/// Polymorphic CPU-content producer registered by borrowed address.
/// Registration does not transfer ownership. Implementations borrow each request and
/// own their internal dependencies; generated Geometry owns its copied output data.
/// Generation may allocate and perform provider work; it is a preparation operation,
/// not a bounded frame callback. Serialize calls unless an implementation explicitly
/// permits concurrency; const does not guarantee thread safety of its dependencies.
/// Allocation failure currently follows the allocator contract, not the boolean result.
class Generator {
public:
  /// Destroy the implementation through this interface; unregister before destruction.
  virtual ~Generator() = default;
  /// Registered producer identity is not implicitly copied.
  Generator(const Generator &) = delete;
  /// Registered producer identity is not implicitly replaced.
  Generator &operator=(const Generator &) = delete;

  /// @return Borrowed nonempty registration name, readable until registration copies it.
  /// Exact case-sensitive identifier; no ownership is transferred to the caller.
  [[nodiscard]] virtual std::string_view kind() const = 0;
  /// Generate native CPU geometry using implementation-specific request semantics.
  /// @param asked Borrowed request; do not retain it or its Ground pointer beyond the call.
  /// @param into Caller-owned output under exclusive access. Implementations define whether
  /// they append or replace; the built-in structures generator appends parts and materials.
  /// @return True when generation succeeds; false when refused. The current interface
  /// carries no diagnostic and does not guarantee rollback of partially written output.
  [[nodiscard]] virtual bool make(const Request &asked, Geometry &into) const = 0;

  /// Append requested terrain modifications; the default implementation appends nothing.
  /// @param asked Same request used for geometry generation; borrowed only during the call.
  /// @param into Caller-owned accumulation under exclusive access; preserve existing stamps.
  /// @return True if this call appended any stamps; false leaves the accumulation unchanged.
  /// Existing entries do not affect the result. Cost and allocation depend on the producer.
  [[nodiscard]] virtual bool stamps(const Request &asked, std::vector<Stamp> &into) const {
    (void)asked;
    (void)into;
    return false;
  }

protected:
  /// Construct the interface subobject without allocation.
  Generator() = default;
};

/// The generators this engine ships with, and the catalogue is CLOSED: a client registers its own
/// beside them rather than adding a value here.
enum class Shipped : uint8_t {
  /// Built-in square building producer. Optional widthM is its footprint side in metres
  /// (default 12), independent of Request::ExtentM. Accepts one finite positive decimal
  /// value, optionally with exponent, without surrounding whitespace. Unknown/duplicate
  /// settings and invalid values are refused before output mutation. Output uses local
  /// metres at the request centre at zero ASL: X east, Y up, Z south. Terrain anchoring
  /// and per-feature placement remain implementation limitations of this producer.
  Structures,
  kCount ///< Catalogue size sentinel; not a generator.
};

/// The name each shipped kind answers to in a declaration, in the order the enum names them.
inline constexpr std::array<std::string_view, static_cast<size_t>(Shipped::kCount)> kShipped = {
    "structures"};

/// Resolve a built-in generator identifier without allocation.
/// @param which Catalogue value; kCount and unsupported enum values are invalid.
/// @return Static name with process lifetime, or an empty view for an invalid value.
[[nodiscard]] constexpr std::string_view nameOf(Shipped which) noexcept {
  const auto index = static_cast<size_t>(which);
  return index < kShipped.size() ? kShipped[index] : std::string_view{};
}

/// Verify that built-in registration names are nonempty and unique.
/// @return True for a valid catalogue; quadratic in catalogue size, no allocation.
[[nodiscard]] constexpr bool EveryShippedKindIsSpelled() noexcept {
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

/// Catalogue owning registration names and borrowing generator objects.
/// Generators must outlive their registrations and retain stable addresses. Registration
/// does not transfer ownership or invoke generation. No thread affinity; serialize mutations
/// with all access. Concurrent lookups on an unchanged registry are allowed.
/// After move, the source supports only destruction or move assignment.
class Registry {
public:
  /// Register a borrowed generator under a snapshot of its current kind().
  /// @param maker Object retained by address; kind() is called once and its name copied.
  /// The returned name must remain readable for this call; later changes do not rename the entry.
  /// @return False for an empty or already registered name, preserving all registrations.
  /// Success may allocate; allocation failure follows the allocator contract. Setup operation,
  /// linear in the number of registrations and compared name lengths.
  [[nodiscard]] bool offers(const Generator &maker);

  /// Find an exact, case-sensitive registration name without calling generator methods.
  /// @param kind Borrowed lookup key; not retained.
  /// @return Borrowed generator or nullptr; its external owner controls lifetime.
  /// No allocation; linear in registrations and compared name lengths.
  [[nodiscard]] const Generator *named(std::string_view kind) const;
  /// Count registrations in constant time without allocation.
  /// @return Number of registered names, independent of later generator kind() changes.
  [[nodiscard]] size_t count() const;

  /// Create an empty catalogue; allocates private storage.
  Registry();
  /// Release owned names and storage; borrowed generators are not destroyed.
  ~Registry();
  /// Transfer registrations without allocation or moving the borrowed generators.
  /// @param other Source left usable only for destruction or move assignment.
  Registry(Registry &&other) noexcept;
  /// Release previous names and transfer registrations; neither set of generators is destroyed.
  /// @param other Source left usable only for destruction or move assignment.
  /// @return This registry; cost includes releasing previous registration storage.
  Registry &operator=(Registry &&other) noexcept;
  /// Implicit duplication of registrations is prohibited.
  Registry(const Registry &) = delete;
  /// Implicit replacement by copied registrations is prohibited.
  Registry &operator=(const Registry &) = delete;

private:
  struct Kept;
  std::unique_ptr<Kept> Kept_;
};

}

#endif
