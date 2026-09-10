#include "Views.h"
#include <expected>
#include <cmath>
#include <utility>
#include <string>
#include <span>
#include <string_view>
#include <cstddef>

namespace outshine {

namespace Says {
constexpr auto InvalidCameraPlacement = " declares an unsupported camera placement";
constexpr auto MissingFollowTarget = " requires a followed body";
constexpr auto InvalidPerson = " requires person first or third when follows is set";
constexpr auto InvalidClockScale = " requires a finite positive timeScale";
}

namespace {
[[nodiscard]] constexpr bool KnownPlacement(Scenario::CameraPlacement placement) noexcept {
  switch (placement) {
    case Scenario::CameraPlacement::FollowEntity:
    case Scenario::CameraPlacement::Local:
    case Scenario::CameraPlacement::Geodetic: return true;
  }
  return false;
}

[[nodiscard]] std::expected<void, std::string> ValidateView(const Scenario::View &view) {
  if (!KnownPlacement(view.Placement)) {
    return std::unexpected("view '" + view.Id + "'" + Says::InvalidCameraPlacement);
  }
  if (view.Follows.empty() && view.Placement == Scenario::CameraPlacement::FollowEntity) {
    return std::unexpected("view '" + view.Id + "'" + Says::MissingFollowTarget);
  }
  if (!view.Follows.empty() && view.Person != "first" && view.Person != "third") {
    return std::unexpected("view '" + view.Id + "'" + Says::InvalidPerson);
  }
  if (!std::isfinite(view.TimeScale) || !(view.TimeScale > 0.0)) {
    return std::unexpected("view '" + view.Id + "'" + Says::InvalidClockScale);
  }
  return {};
}
}

std::expected<ViewBook, std::string> ViewBook::Stand(std::span<const Scenario::View> declared,
                                                     std::string_view starting) {
  ViewBook standing;
  if (declared.empty()) {
    return std::unexpected(
        "a view book stands on 1..N declared views and this scenario declares none");
  }
  for (const Scenario::View &view : declared) {
    if (view.Id.empty()) {
      return std::unexpected(
          "a view without an id cannot be taken, and a view nobody can take is dead weight");
    }
    for (const Scenario::View &held : standing.Held_) {
      if (held.Id == view.Id) {
        return std::unexpected("the view '" + view.Id +
                               "' is declared twice, and taking it would be a coin toss");
      }
    }
    auto valid = ValidateView(view);
    if (!valid) { return std::unexpected(std::move(valid.error())); }
    standing.Held_.push_back(view);
  }
  if (!starting.empty() && !standing.Take(starting)) {
    return std::unexpected("the player starts in view '" + std::string(starting) +
                           "', which no view declares");
  }
  return standing;
}

bool ViewBook::Take(std::string_view id) {
  for (size_t at = 0; at < Held_.size(); ++at) {
    if (Held_[at].Id == id) {
      Active_ = at;
      return true;
    }
  }
  return false;
}

}
