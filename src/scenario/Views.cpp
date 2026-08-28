#include "Views.h"

namespace outshine {

std::expected<ViewBook, std::string> ViewBook::Stand(std::span<const View> declared,
                                                     std::string_view starting) {
  ViewBook standing;
  if (declared.empty()) {
    return std::unexpected(
        "a view book stands on 1..N declared views and this scenario declares none");
  }
  for (const View &view : declared) {
    if (view.Id.empty()) {
      return std::unexpected(
          "a view without an id cannot be taken, and a view nobody can take is dead weight");
    }
    for (const View &held : standing.Held_) {
      if (held.Id == view.Id) {
        return std::unexpected("the view '" + view.Id +
                               "' is declared twice, and taking it would be a coin toss");
      }
    }
    if (view.Follows.empty() && !view.Placed) {
      return std::unexpected("view '" + view.Id +
                             "' neither follows a body nor stands anywhere -- a camera the "
                             "client drives frame by frame is the defect this mechanism "
                             "replaces, and a view that STANDS is still declared");
    }
    if (view.Person != "first" && view.Person != "third") {
      return std::unexpected("view '" + view.Id + "' is '" + view.Person +
                             "'-person, and this engine declares first and third");
    }
    if (!(view.TimeScale > 0.0)) {
      return std::unexpected("view '" + view.Id + "' declares timeScale " +
                             std::to_string(view.TimeScale) +
                             ", and a clock runs forward or the scenario is a still");
    }
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
