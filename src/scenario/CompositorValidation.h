#ifndef OUTSHINE_SCENARIO_COMPOSITORVALIDATION_H
#define OUTSHINE_SCENARIO_COMPOSITORVALIDATION_H
#include <scenario/Scenario.h>
#include <cmath>
#include <expected>
#include <span>
#include <string_view>

namespace outshine {
namespace Says {
inline constexpr std::string_view InvalidCompositor =
    "compositor requires a nonempty kind and a finite nonnegative pixel budget";
}

[[nodiscard]] inline std::expected<void, std::string_view>
ValidateCompositors(std::span<const Scenario::Compositor> compositors) noexcept {
  for (const auto &compositor : compositors) {
    if (compositor.Kind.empty() || !std::isfinite(compositor.BudgetPx) ||
        compositor.BudgetPx < 0.0) {
      return std::unexpected(Says::InvalidCompositor);
    }
  }
  return {};
}
}
#endif
