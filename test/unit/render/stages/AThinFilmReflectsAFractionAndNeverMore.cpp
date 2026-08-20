/* `KHR_materials_iridescence` AS AN INSTRUMENT RATHER THAN A PICTURE (board:1389). The corpus decides
 * whether the film LOOKS right; this decides whether the quantity is a reflectance at all, over a
 * population no case reaches -- every index the schema allows, every thickness, and the base F0 of a
 * white metal, which is where the extension's own text comes apart.
 *
 * FOUR CLAIMS, AND THE FIRST TWO ARE THE ONES THAT COST SOMETHING.
 *
 * THE INVERSE FRESNEL MAP HAS A POLE AT F0 = 1 AND THE EXTENSION WALKS INTO IT. Its implementation
 * note reads `Fresnel0ToIor(baseF0 + 0.0001)`, commented *guard against 1.0*, and at F0 = 1 that
 * addition carries the argument PAST the pole rather than away from it: the square root exceeds one,
 * the denominator turns negative and the index comes back as -40002. A metal whose `baseColorFactor`
 * is 1.0 is ordinary glTF -- `IridescenceMetallicSpheres` is 346 of them -- so this is reached by an
 * asset and not by a fuzzer.
 *
 * A REFLECTANCE IS A FRACTION IN BOTH DIRECTIONS AND THE EXTENSION CLAMPS ONE. `max(I, 0.0)` is its
 * only bound, and the truncated Airy sum over a near-mirror base overshoots one. It is not free to
 * leave: the layering the same document specifies weights the base by `1 - max(F)`, so a component
 * above one turns the diffuse term negative and the surface EMITS.
 *
 * TOTAL INTERNAL REFLECTION IS CHECKED AT ITS ANGLE AND NOT AT ITS EXISTENCE. A branch that returned
 * white everywhere would pass a test that only asked whether white ever came back, so the critical
 * angle is computed from Snell independently and the two sides of it are asked separately.
 *
 * AND THE FILM MUST ACTUALLY DO SOMETHING. A function returning a constant satisfies every bound
 * above, so the hue is required to MOVE across the thickness range the extension's own defaults
 * bracket. */
#include <algorithm>
#include <array>
#include <cmath>

#include "Check.h"

#include "IridescenceLobe.h"

using outshine::Render::Fresnel0ToIor;
using outshine::Render::IorToFresnel0;
using outshine::Render::IridescenceFresnel;
using outshine::Render::IridescenceSchlick;
using outshine::Render::kOutsideIor;

namespace {

/* The film indices the schema admits, the base reflectances a glTF material can produce -- 0 through
 * a white metal's 1 -- and thicknesses from none to well past the visible band. */
constexpr std::array<double, 6> kFilmIors = {1.0, 1.1, 1.3, 1.8, 2.4, 3.0};
constexpr std::array<double, 7> kBaseF0s = {0.0, 0.02, 0.04, 0.2, 0.766, 0.98, 1.0};

} // namespace

int main() {
  double worstHigh = 0.0;
  double worstLow = 1.0;
  long long samples = 0;
  for (const double filmIor : kFilmIors) {
    for (const double f0 : kBaseF0s) {
      const std::array<double, 3> base = {f0, f0 * 0.8, f0 * 0.6};
      for (double thickness = 0.0; thickness <= 2000.0; thickness += 25.0) {
        for (double cosTheta = 0.02; cosTheta <= 1.0; cosTheta += 0.02) {
          std::array<double, 3> f{};
          IridescenceFresnel(cosTheta, thickness, filmIor, base, f);
          for (const double channel : f) {
            worstHigh = std::fmax(worstHigh, channel);
            worstLow = std::fmin(worstLow, channel);
            ++samples;
          }
        }
      }
    }
  }
  outshine::Test::Note("population", (double)samples, "channels");
  CHECK(worstHigh <= 1.0, "a thin film reflects no more than arrives, on every index and thickness");
  CHECK(worstLow >= 0.0, "a thin film reflects no less than nothing");

  /* THE POLE, APPROACHED FROM BELOW AND STOOD ON. Monotone, finite and positive is the whole of what
   * the inverse map owes; the extension's own expression fails the last of the three at F0 = 1. */
  double previous = 0.0;
  for (const double f0 : {0.0, 0.1, 0.5, 0.9, 0.99, 0.9999, 1.0}) {
    const double ior = Fresnel0ToIor(f0 + 0.0001);
    CHECK(std::isfinite(ior) && ior > 0.0,
          "the inverse of Fresnel's normal-incidence case answers a real index at every reachable F0");
    CHECK(ior >= previous, "and it rises with the reflectance it inverts");
    previous = ior;
  }
  /* The map is an inverse and not merely finite: a round trip through both directions returns what
   * went in, wherever the ceiling is not the answer. */
  for (const double f0 : {0.0, 0.04, 0.2, 0.5, 0.9}) {
    CHECK_NEAR(IorToFresnel0(Fresnel0ToIor(f0), kOutsideIor), f0, 1.0e-12, "dimensionless",
               "F0 -> index -> F0 is the identity below the pole");
  }

  /* TOTAL INTERNAL REFLECTION, AT SNELL'S OWN ANGLE. Below the critical cosine the refracted ray does
   * not exist and the film is a mirror; above it, some light gets in and the answer is not white. */
  const double filmIor = 0.7;
  const double criticalCos = std::sqrt(1.0 - (filmIor / kOutsideIor) * (filmIor / kOutsideIor));
  const std::array<double, 3> base = {0.04, 0.04, 0.04};
  std::array<double, 3> inside{};
  std::array<double, 3> outside{};
  IridescenceFresnel(criticalCos * 0.95, 400.0, filmIor, base, inside);
  IridescenceFresnel(criticalCos * 1.05, 400.0, filmIor, base, outside);
  CHECK(inside[0] == 1.0 && inside[1] == 1.0 && inside[2] == 1.0,
        "past the critical angle the film reflects everything, in every channel");
  CHECK(outside[0] < 1.0, "and short of it, it does not");

  /* THE EFFECT EXISTS. Across the range `iridescenceThicknessMinimum` and `iridescenceThicknessMaximum`
   * default to, the three channels must separate -- a film that returned grey would satisfy every
   * bound above and show nothing. */
  double widestSpread = 0.0;
  for (double thickness = 100.0; thickness <= 400.0; thickness += 10.0) {
    std::array<double, 3> f{};
    IridescenceFresnel(0.7, thickness, 1.3, base, f);
    widestSpread = std::fmax(widestSpread, *std::max_element(f.begin(), f.end()) -
                                               *std::min_element(f.begin(), f.end()));
  }
  outshine::Test::Note("widest channel separation over the default thickness range", widestSpread,
                       "reflectance");
  CHECK(widestSpread > 0.01, "the film tints, and a constant would not");

  /* THE DC TERM IS THE GEOMETRIC SERIES IT CLAIMS TO BE. `R12 + T121^2 R23 / (1 - R12 R23)` is the sum
   * over every round trip inside the film, and the sum is spelled here independently so a typo in the
   * closed form is a difference rather than a picture nobody questions. */
  const double cosTheta = 0.6;
  const double r12 = IridescenceSchlick(IorToFresnel0(1.3, kOutsideIor), cosTheta);
  const double cosTheta2 = std::sqrt(1.0 - (kOutsideIor / 1.3) * (kOutsideIor / 1.3) *
                                               (1.0 - cosTheta * cosTheta));
  const double r23 = IridescenceSchlick(IorToFresnel0(Fresnel0ToIor(0.04 + 0.0001), 1.3), cosTheta2);
  double series = r12;
  double carried = (1.0 - r12) * (1.0 - r12) * r23;
  for (int bounce = 0; bounce < 200; ++bounce) {
    series += carried;
    carried *= r12 * r23;
  }
  CHECK_NEAR(r12 + (1.0 - r12) * (1.0 - r12) * r23 / (1.0 - r12 * r23), series, 1.0e-12,
             "dimensionless", "the closed form is the sum over every round trip inside the film");

  outshine::Test::Covers("board:1389 the thin-film Fresnel is a reflectance in both directions, at "
                         "every index the schema admits and at a white metal's F0 -- where the "
                         "extension's own guard produces a negative index");
  return outshine::Test::Report();
}
