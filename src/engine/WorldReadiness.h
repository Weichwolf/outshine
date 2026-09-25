#ifndef OUTSHINE_ENGINE_WORLDREADINESS_H
#define OUTSHINE_ENGINE_WORLDREADINESS_H

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace outshine {
struct WorldReadiness {
  std::array<std::string_view, 12> Blockers{};

  [[nodiscard]] constexpr bool Ready() const noexcept {
    return std::ranges::all_of(Blockers, [](std::string_view blocker) { return blocker.empty(); });
  }

  [[nodiscard]] std::string Describe() const {
    std::string result;
    for (const auto blocker : Blockers) {
      if (blocker.empty()) { continue; }
      if (!result.empty()) { result += ", "; }
      result += blocker;
    }
    return result;
  }
};
}
#endif
