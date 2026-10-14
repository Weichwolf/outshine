#ifndef OUTSHINE_GENERATORS_FLORA_LEAFOPTICSDEFAULTS_H
#define OUTSHINE_GENERATORS_FLORA_LEAFOPTICSDEFAULTS_H

#include "LeafOpticsTable.h"
#include "math/Vec3.h"

namespace outshine::Generators {
[[nodiscard]] const LeafOpticsTable &DefaultLeafOptics();
[[nodiscard]] Vec3f DefaultLeafReflection();
[[nodiscard]] Vec3f DefaultLeafTransmission();
}
#endif
