#ifndef OUTSHINE_GENERATORS_CONTRACTS_GROUNDPORTSOURCEPROFILE_H
#define OUTSHINE_GENERATORS_CONTRACTS_GROUNDPORTSOURCEPROFILE_H

#include <cstdint>

namespace outshine::Generators {
struct GroundPortSourceProfile {
  uint64_t PathId = 0;
  double MaximumGradient = 0;
};
}
#endif
