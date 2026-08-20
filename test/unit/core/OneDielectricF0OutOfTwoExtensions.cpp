/* `KHR_materials_ior` AND `KHR_materials_specular` ARE TWO SPELLINGS OF ONE NUMBER (board:1205).
 *
 * The renderer needs the dielectric's normal-incidence reflectance and nothing else; the file states
 * it as an index of refraction, a scalar factor and a colour. `DielectricF0` is where the three become
 * one, and it is the ONLY place -- the shader used to carry a constant 0.04 while the row carried an
 * `Ior` nothing read, which is the same quantity answered twice and the defect this test pins shut.
 *
 * THE THREE CASES THAT ARE NOT ARITHMETIC:
 *   the DEFAULT   a material declaring neither extension must land on glTF's own 0.04, or every
 *                 existing picture in the corpus moves
 *   the CAP       the format caps `((ior-1)/(ior+1))^2 * specularFactor` at 1 BEFORE the colour tints
 *                 it, so a large factor cannot brighten past white and a tint can still darken
 *   `ior = 0`     legal, and it means NO Fresnel. It is not "absent", and the naive expression gives
 *                 ((0-1)/(0+1))^2 = 1 -- a mirror -- which is the opposite of what the file says
 */
#include <cmath>
#include <string>

#include "Check.h"

#include "Material.h"

using outshine::DielectricF0;
using outshine::Material;

namespace {

/* glTF's default dielectric: ((1.5 - 1)/(1.5 + 1))^2 = 0.2^2. */
constexpr double kDefaultF0 = 0.04;

} // namespace

int main() {
  using namespace outshine::Test;

  float f0[3] = {-1.0f, -1.0f, -1.0f};

  const Material plain;
  DielectricF0(plain, f0);
  for (int channel = 0; channel < 3; ++channel) {
    CHECK_NEAR(f0[channel], kDefaultF0, 1e-7, "dimensionless",
               "a material declaring neither extension is glTF's own 0.04, so every picture already "
               "in the corpus is unmoved by this arriving");
  }

  /* IORTestGrid's own four values, and the arithmetic is hand-checkable: 1.0 -> 0, 1.33 -> 0.0201,
   * 1.76 -> 0.0757, 2.42 -> 0.1730. The first is the one that matters -- an ior of exactly 1 is a
   * surface with the same index as the air around it, and it must reflect NOTHING. */
  const double grid[4] = {1.0, 1.33, 1.76, 2.42};
  for (double ior : grid) {
    Material material;
    material.Ior = (float)ior;
    DielectricF0(material, f0);
    const double edge = (ior - 1.0) / (ior + 1.0);
    CHECK_NEAR(f0[0], edge * edge, 1e-7, "dimensionless",
               "the ior alone gives ((ior-1)/(ior+1))^2, which is the formula and not a fit");
  }

  Material matched;
  matched.Ior = 1.0f;
  DielectricF0(matched, f0);
  CHECK(f0[0] == 0.0f && f0[1] == 0.0f && f0[2] == 0.0f,
        "an ior of 1 reflects nothing at all, exactly and not to a tolerance -- the surface has the "
        "index of the air in front of it");

  /* SpecularTest's own five factors, at the default ior, which is the row the corpus asset varies. */
  const double factors[5] = {0.0, 0.051269, 0.212231, 0.520996, 1.0};
  for (double factor : factors) {
    Material material;
    material.SpecularFactor = (float)factor;
    DielectricF0(material, f0);
    CHECK_NEAR(f0[0], kDefaultF0 * factor, 1e-7, "dimensionless",
               "the specular factor scales the default F0, which is what SpecularTest's black "
               "roughness-zero panels isolate");
  }

  Material tinted;
  tinted.SpecularColour[0] = 1.0f;
  tinted.SpecularColour[1] = 0.5f;
  tinted.SpecularColour[2] = 0.0f;
  DielectricF0(tinted, f0);
  CHECK_NEAR(f0[0], kDefaultF0, 1e-7, "dimensionless", "the tint's red channel passes F0 unchanged");
  CHECK_NEAR(f0[1], kDefaultF0 * 0.5, 1e-7, "dimensionless", "the tint's green channel halves it");
  CHECK(f0[2] == 0.0f, "the tint's blue channel removes the lobe from that channel entirely");

  /* THE CAP, AND THE ORDER IT IS APPLIED IN IS THE CLAIM. A factor of 100 at the default ior gives 4,
   * which the format caps to 1; the tint then darkens that 1. Capping AFTER the tint would give 1 in
   * the green channel too, and the two orders differ by a whole stop on this input. */
  Material capped;
  capped.SpecularFactor = 100.0f;
  capped.SpecularColour[1] = 0.5f;
  DielectricF0(capped, f0);
  CHECK(f0[0] == 1.0f, "the product is capped at 1, so a specular factor above unity cannot brighten "
                       "past a perfect mirror");
  CHECK_NEAR(f0[1], 0.5, 1e-7, "dimensionless",
             "the tint darkens the CAPPED value, which is the format's order -- capping after the "
             "tint would leave this channel at 1 and it is a whole stop apart");

  Material fresnelFree;
  fresnelFree.Ior = 0.0f;
  fresnelFree.SpecularFactor = 1.0f;
  DielectricF0(fresnelFree, f0);
  CHECK(f0[0] == 0.0f && f0[1] == 0.0f && f0[2] == 0.0f,
        "an ior of 0 is legal and means no Fresnel at all -- the naive expression would make it a "
        "MIRROR, which is the opposite of what the file states");

  Note("ior values checked", 4.0, "from IORTestGrid");
  Note("specular factors checked", 5.0, "from SpecularTest");
  Covers("I.26.6 KHR_materials_ior and KHR_materials_specular, combined into one F0");
  return Report();
}
