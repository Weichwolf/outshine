#ifndef OUTSHINE_WORLD_WEATHER_CLOUDLAYER_H
#define OUTSHINE_WORLD_WEATHER_CLOUDLAYER_H

#include "math/Vec3.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace outshine {

inline constexpr double kCloudReferenceRadiusM = 6360000.0;
inline constexpr double kCloudLayerThicknessM = 600.0;
inline constexpr double kCloudExtinctionPerM = 0.012;

enum class CloudProfile : uint32_t { Stratus = 0, CumulusV1 = 1 };

static_assert(static_cast<uint32_t>(CloudProfile::Stratus) == 0 &&
              static_cast<uint32_t>(CloudProfile::CumulusV1) == 1);

struct CloudLayer {
  double RadiusM = kCloudReferenceRadiusM;
  double BaseM = 0.0;
  double ThicknessM = kCloudLayerThicknessM;
  double Cover = 0.0;
  double ExtinctionPerM = kCloudExtinctionPerM;
  Vec3 WindMs{};
  CloudProfile Profile = CloudProfile::Stratus;
};

struct CloudSegment {
  double BeginM = 0.0;
  double EndM = 0.0;
};

using CloudSegments = std::array<CloudSegment, 2>;

inline constexpr size_t kCloudIntegrationSamples = 16;

struct CloudWind {
  double FromDeg = 0.0;
  double SpeedMs = 0.0;
};

struct CloudTrace {
  double Seconds = 0.0;
  double MaximumM = std::numeric_limits<double>::infinity();
};

[[nodiscard]] Vec3 CloudWindFrom(const CloudWind &wind) noexcept;

[[nodiscard]] double CloudAltitude(const Vec3 &positionM, double radiusM) noexcept;

[[nodiscard]] CloudSegments
CloudShell(const CloudLayer &layer, const Vec3 &originM, const Vec3 &direction) noexcept;

[[nodiscard]] double
CloudExtinction(const CloudLayer &layer, const Vec3 &positionM, double seconds) noexcept;

[[nodiscard]] double CloudOpticalDepth(const CloudLayer &layer,
                                       const Vec3 &originM,
                                       const Vec3 &direction,
                                       const CloudTrace &trace = {}) noexcept;

}
#endif
