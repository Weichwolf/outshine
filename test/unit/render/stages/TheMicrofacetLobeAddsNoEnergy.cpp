#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Check.h"

#include "MetalRoughBrdf.h"

using outshine::Render::BrdfGeometry;
using outshine::Render::BrdfTerms;
using outshine::Render::kBrdfPi;
using outshine::Render::kDielectricF0;
using outshine::Render::MetalRoughBrdf;

namespace {

struct Vector {
  double X = 0, Y = 0, Z = 0;
};

double Dot(const Vector &left, const Vector &right) {
  return left.X * right.X + left.Y * right.Y + left.Z * right.Z;
}

Vector Normalised(const Vector &of) {
  const double length = std::sqrt(Dot(of, of));
  return {of.X / length, of.Y / length, of.Z / length};
}

struct SamplePair {
  double First = 0, Second = 0;
};

SamplePair Hammersley(uint32_t index, uint32_t count) {
  uint32_t bits = index;
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return {(index + 0.5) / count, bits * 2.3283064365386963e-10};
}

Vector SampleVisibleNormal(const Vector &view, double alpha, const SamplePair &at) {
  const Vector stretched = Normalised({alpha * view.X, alpha * view.Y, view.Z});
  const double planar = stretched.X * stretched.X + stretched.Y * stretched.Y;
  const Vector first = planar > 0 ? Vector{-stretched.Y / std::sqrt(planar),
                                           stretched.X / std::sqrt(planar), 0}
                                  : Vector{1, 0, 0};
  const Vector second{stretched.Y * first.Z - stretched.Z * first.Y,
                      stretched.Z * first.X - stretched.X * first.Z,
                      stretched.X * first.Y - stretched.Y * first.X};
  const double radius = std::sqrt(at.First);
  const double angle = 2.0 * kBrdfPi * at.Second;
  const double across = radius * std::cos(angle);
  double along = radius * std::sin(angle);
  const double lean = 0.5 * (1.0 + stretched.Z);
  along = (1.0 - lean) * std::sqrt(std::fmax(0.0, 1.0 - across * across)) + lean * along;
  const double up = std::sqrt(std::fmax(0.0, 1.0 - across * across - along * along));
  const Vector unstretched{across * first.X + along * second.X + up * stretched.X,
                           across * first.Y + along * second.Y + up * stretched.Y,
                           across * first.Z + along * second.Z + up * stretched.Z};
  return Normalised({alpha * unstretched.X, alpha * unstretched.Y, std::fmax(0.0, unstretched.Z)});
}

double VisibleNormalDensity(double alpha, double nv, double nh) {
  const double alphaSquared = alpha * alpha;
  const double denominator = nh * nh * (alphaSquared - 1.0) + 1.0;
  const double distribution = alphaSquared / (kBrdfPi * denominator * denominator);
  const double masking =
      2.0 * nv / (nv + std::sqrt(alphaSquared + (1.0 - alphaSquared) * nv * nv));
  return masking * distribution / (4.0 * nv);
}

struct DirectionalAlbedo {
  double Specular = 0;
  double Diffuse = 0;
  double Total() const { return Specular + Diffuse; }
};

DirectionalAlbedo Integrate(double roughness, double nv, double metalness, uint32_t samples) {
  const double alpha = roughness * roughness;
  const double alphaSquared = alpha * alpha;

  const std::array<double, 3> f0{metalness > 0.5 ? 1.0 : kDielectricF0,
                                 metalness > 0.5 ? 1.0 : kDielectricF0,
                                 metalness > 0.5 ? 1.0 : kDielectricF0};
  const std::array<double, 3> diffuseColour{1.0 - metalness, 1.0 - metalness, 1.0 - metalness};
  const Vector view{std::sqrt(std::fmax(0.0, 1.0 - nv * nv)), 0, nv};
  DirectionalAlbedo albedo;
  for (uint32_t index = 0; index < samples; ++index) {
    const SamplePair at = Hammersley(index, samples);
    if (alphaSquared > 0.0) {
      const Vector half = SampleVisibleNormal(view, alpha, at);
      const double vh = Dot(view, half);
      const Vector light{2.0 * vh * half.X - view.X, 2.0 * vh * half.Y - view.Y,
                         2.0 * vh * half.Z - view.Z};
      const double density = VisibleNormalDensity(alpha, nv, std::fmax(half.Z, 0.0));
      if (light.Z > 0.0 && vh > 0.0 && density > 0.0) {
        const BrdfGeometry geometry{light.Z, nv, std::fmax(half.Z, 0.0), vh};
        const BrdfTerms terms = MetalRoughBrdf(diffuseColour, f0, 1.0, alphaSquared, geometry);
        albedo.Specular += terms.Specular[0] * light.Z / density;
      }
    }

    const double radius = std::sqrt(at.First);
    const double angle = 2.0 * kBrdfPi * at.Second;
    const Vector light{radius * std::cos(angle), radius * std::sin(angle),
                       std::sqrt(std::fmax(0.0, 1.0 - at.First))};
    const Vector half = Normalised({light.X + view.X, light.Y + view.Y, light.Z + view.Z});
    const BrdfGeometry geometry{light.Z, nv, std::fmax(half.Z, 0.0),
                                std::fmax(Dot(view, half), 0.0)};
    const BrdfTerms terms = MetalRoughBrdf(diffuseColour, f0, 1.0, alphaSquared, geometry);
    albedo.Diffuse += terms.Diffuse[0] * kBrdfPi;
  }
  albedo.Specular /= samples;
  albedo.Diffuse /= samples;
  return albedo;
}

constexpr double kRoughnessSweep[] = {0.0, 0.05, 0.1, 0.16, 0.25, 0.33, 0.5, 0.7, 0.85, 1.0};

constexpr double kViewCosineSweep[] = {1.0, 0.7, 0.4, 0.15};

constexpr uint32_t kSamples = 1u << 18;

constexpr uint32_t kCoarseSamples = 1u << 16;

struct Extremum {
  double Value = 0;
  double Roughness = 0;
  double ViewCosine = 0;

  void Offer(double value, double roughness, double nv) {
    if (value <= Value) { return; }
    Value = value;
    Roughness = roughness;
    ViewCosine = nv;
  }

  void Report(const char *what) const {
    outshine::Test::Note(what, Value, "dimensionless");
    outshine::Test::Note("at roughness", Roughness, "dimensionless");
    outshine::Test::Note("at N.V", ViewCosine, "dimensionless");
  }
};

struct Sweep {
  Extremum Microfacet;
  Extremum DielectricTotal;
  double EstimatorFloor = 0;
};

Sweep RunSweep(void) {
  Sweep found;
  for (const double roughness : kRoughnessSweep) {
    for (const double nv : kViewCosineSweep) {
      const DirectionalAlbedo metal = Integrate(roughness, nv, 1.0, kSamples);
      const DirectionalAlbedo coarse = Integrate(roughness, nv, 1.0, kCoarseSamples);
      const DirectionalAlbedo dielectric = Integrate(roughness, nv, 0.0, kSamples);
      std::printf("SWEEP roughness = %.2f, N.V = %.2f, metal E = %.9g, dielectric E = %.9g "
                  "(specular %.9g + diffuse %.9g)\n",
                  roughness, nv, metal.Specular, dielectric.Total(), dielectric.Specular,
                  dielectric.Diffuse);
      found.EstimatorFloor =
          std::fmax(found.EstimatorFloor, std::fabs(metal.Specular - coarse.Specular));
      found.Microfacet.Offer(metal.Specular, roughness, nv);
      found.DielectricTotal.Offer(dielectric.Total(), roughness, nv);
      CHECK(metal.Specular <= 1.0,
            "the microfacet lobe of a perfect reflector returns no more than it receives");
    }
  }
  return found;
}

}

int main() {
  const Sweep found = RunSweep();
  found.Microfacet.Report("worst microfacet directional albedo");
  outshine::Test::Note("estimator floor, 2^18 against 2^16 samples", found.EstimatorFloor,
                       "dimensionless");

  found.DielectricTotal.Report(
      "worst dielectric directional albedo, diffuse and specular together");

  outshine::Test::Covers("I.26.12 DirectionalLight's energy half: the directional albedo of the "
                         "microfacet lobe, integrated over the hemisphere at a roughness sweep, "
                         "with no asset, no oracle and no punctual light");
  return outshine::Test::Report();
}
