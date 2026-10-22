#ifndef OUTSHINE_GENERATORS_ROAD_PROFILE_HEIGHTPROGRAMINTERNAL_H
#define OUTSHINE_GENERATORS_ROAD_PROFILE_HEIGHTPROGRAMINTERNAL_H

#include "HeightProgram.h"

namespace outshine::Generators::RoadProfiles {

inline constexpr double kHeightProgramTolerance = 1e-9;

[[nodiscard]] std::expected<void, HeightFitError> Validate(const HeightProgram &program,
                                                           HeightFitBudget budget);

[[nodiscard]] bool Verify(const HeightProgram &program, HeightFit &fit);

}

#endif
