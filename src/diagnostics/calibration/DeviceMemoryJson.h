#ifndef OUTSHINE_DIAGNOSTICS_CALIBRATION_DEVICEMEMORYJSON_H
#define OUTSHINE_DIAGNOSTICS_CALIBRATION_DEVICEMEMORYJSON_H
#include "MemoryStatus.h"
#include <nlohmann/json_fwd.hpp>
#include <string_view>

namespace outshine::Diagnostics::Calibration {
[[nodiscard]] nlohmann::json DeviceMemoryJson(const Render::Native::MemoryStatus &status,
                                              std::string_view phase);
}
#endif
