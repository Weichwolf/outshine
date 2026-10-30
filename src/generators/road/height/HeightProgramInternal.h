#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_HEIGHTPROGRAMINTERNAL_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_HEIGHTPROGRAMINTERNAL_H

#include "HeightProgram.h"

namespace outshine::Generators::RoadProfiles {

inline constexpr double kHeightProgramTolerance = 1e-9;
inline constexpr int kHeightSolverThreads = 1;
inline constexpr int kHeightSolverSimplexStrategy = 1;
inline constexpr int kHeightSolverRandomSeed = 0;
inline constexpr const char *kHeightSolverParallel = "off";
inline constexpr const char *kHeightSolverName = "simplex";
inline constexpr const char *kHeightSolverPresolve = "off";

[[nodiscard]] std::expected<void, HeightFitError> Validate(const HeightProgram &program,
                                                           HeightFitBudget budget);

[[nodiscard]] bool Verify(const HeightProgram &program, HeightFit &fit);

}

#endif
