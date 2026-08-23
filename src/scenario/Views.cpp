#include "Views.h"

namespace outshine {

bool ViewBook::Build(std::span<const View> declared, std::string_view starting,
                     std::string &error) {
  Held_.clear();
  Active_ = 0;
  if (declared.empty()) {
    error = "a view book stands on 1..N declared views and this scenario declares none";
    return false;
  }
  for (const View &view : declared) {
    if (view.Id.empty()) {
      error = "a view without an id cannot be taken, and a view nobody can take is dead "
              "weight";
      return false;
    }
    for (const View &held : Held_) {
      if (held.Id == view.Id) {
        error = "the view '" + view.Id + "' is declared twice, and taking it would be a "
                "coin toss";
        return false;
      }
    }
    if (view.Follows.empty()) {
      error = "view '" + view.Id + "' follows nothing -- a camera the client drives frame "
              "by frame is the defect this mechanism replaces";
      return false;
    }
    if (view.Person != "first" && view.Person != "third") {
      error = "view '" + view.Id + "' is '" + view.Person +
              "'-person, and this engine declares first and third";
      return false;
    }
    if (!(view.TimeScale > 0.0)) {
      error = "view '" + view.Id + "' declares timeScale " + std::to_string(view.TimeScale) +
              ", and a clock runs forward or the scenario is a still";
      return false;
    }
    Held_.push_back(view);
  }
  if (!starting.empty() && !Take(starting)) {
    error = "the player starts in view '" + std::string(starting) +
            "', which no view declares";
    return false;
  }
  return true;
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

} // namespace outshine
