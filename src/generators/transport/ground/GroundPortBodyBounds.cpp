#include "GroundPortBodies.h"
#include "EarthworkDeformation.h"
#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace outshine::Generators {
Box GroundPortBodyBoundsEcef(const GroundPortBodies &bodies) noexcept {
  Box bounds;
  const auto frame = TangentFrame::At(bodies.Anchor);
  for (const auto &body : bodies.Bodies) {
    for (const auto &point : body.Geometry.Points) {
      bounds.Cover(frame.ToEcefPosition({.EastM = point[0], .NorthM = point[1], .UpM = point[2]}));
    }
    for (const auto &contact : body.TerrainContacts) {
      const double relief = std::abs(contact.YieldM);
      const double apron = EarthworkApronWidthM(contact, relief);
      double low = std::numeric_limits<double>::infinity();
      double high = -low;
      for (const double east : std::array{contact.LowE, contact.HighE}) {
        for (const double north : std::array{contact.LowN, contact.HighN}) {
          const double height = contact.WantsAt({.EastM = east, .NorthM = north});
          low = std::min(low, height - relief);
          high = std::max(high, height + relief);
        }
      }
      for (const double east : std::array{contact.LowE - apron, contact.HighE + apron}) {
        for (const double north : std::array{contact.LowN - apron, contact.HighN + apron}) {
          for (const double up : std::array{low, high}) {
            bounds.Cover(frame.ToEcefPosition({.EastM = east, .NorthM = north, .UpM = up}));
          }
        }
      }
    }
  }
  return bounds;
}
}
