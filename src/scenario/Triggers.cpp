#include "Triggers.h"

#include <cmath>

namespace outshine {

namespace {

// [SET] the pool bounds: a scenario's doors are a declared few, the concurrent
// body-inside-door pairs and the per-drain firings are the frame path's budget
constexpr size_t kMostDoors = 256;
// [SET] the bodies one door remembers: a door is a place, and more than 256 bodies
// standing in ONE door at once is a crowd no scenario declares -- the bound is per door
// now, so the probe's cost is its own door's few and never every pair (board:1759)
constexpr size_t kMostStandings = 256;
constexpr size_t kMostFired = 256;

} // namespace

bool TriggerField::Build(std::span<const Volume> volumes, std::span<const Event> events,
                         std::string &error) {
  Doors_.clear();
  Events_.clear();
  Carries_.clear();
  Heard_.clear();
  Unheard_.clear();
  InsideDoor_.clear();
  Ring_.clear();
  Overflowed_ = 0;
  Unseated_ = 0;

  if (volumes.size() > kMostDoors) {
    error = "the scenario declares " + std::to_string(volumes.size()) +
            " volumes over the pool's " + std::to_string(kMostDoors);
    return false;
  }
  for (const Event &event : events) {
    Events_.push_back(event.Name);
    Carries_.emplace_back(event.Carries.begin(), event.Carries.end());
    Heard_.push_back(0);
    Unheard_.push_back(0);
  }
  for (const Volume &volume : volumes) {
    Door door;
    if (volume.When == "enter") {
      door.Opens = When::Enter;
    } else if (volume.When == "exit") {
      door.Opens = When::Exit;
    } else if (volume.When == "dwell") {
      door.Opens = When::Dwell;
    } else {
      error = "volume '" + volume.Id + "' fires when '" + volume.When +
              "', and a volume fires on enter, exit or dwell -- the engine spells no fourth";
      return false;
    }
    if (volume.Shape == "sphere") {
      door.Sphere = 1;
    } else if (volume.Shape == "box" || volume.Shape.empty()) {
      door.Sphere = 0;
    } else {
      error = "volume '" + volume.Id + "' is a '" + volume.Shape +
              "', and a volume is a box or a sphere";
      return false;
    }
    if (door.Opens == When::Dwell && !(volume.DwellS > 0.0)) {
      error = "volume '" + volume.Id +
              "' fires on dwell and declares no dwellS -- a dwell without a duration is "
              "an enter wearing a costume";
      return false;
    }
    uint16_t named = (uint16_t)Events_.size();
    for (size_t at = 0; at < Events_.size(); ++at) {
      if (Events_[at] == volume.Fires) { named = (uint16_t)at; }
    }
    if (named == (uint16_t)Events_.size()) {
      std::string all;
      for (const std::string &event : Events_) {
        if (!all.empty()) { all += ' '; }
        all += event;
      }
      error = "volume '" + volume.Id + "' fires '" + volume.Fires +
              "', which no event declares -- the scenario declares: " +
              (all.empty() ? "nothing" : all);
      return false;
    }
    door.Event = named;
    for (int axis = 0; axis < 3; ++axis) {
      door.AtM[axis] = volume.AtM[axis];
      door.ExtentM[axis] = volume.ExtentM[axis];
    }
    door.DwellS = volume.DwellS;
    Doors_.push_back(door);
  }
  InsideDoor_.assign(Doors_.size(), {});
  for (std::vector<Standing> &seated : InsideDoor_) { seated.reserve(kMostStandings); }
  Ring_.reserve(kMostFired);
  Drained_.reserve(kMostFired);
  return true;
}

bool TriggerField::Listen(std::string_view event, std::span<const std::string_view> reads,
                          std::string &error) {
  for (size_t at = 0; at < Events_.size(); ++at) {
    if (Events_[at] != event) { continue; }
    for (const std::string_view read : reads) {
      bool carried = false;
      for (const std::string &field : Carries_[at]) {
        if (field == read) { carried = true; }
      }
      if (!carried) {
        error = "the listener reads '" + std::string(read) + "' from '" +
                std::string(event) + "', which carries none -- a field is declared or it "
                "is a null at run time, and this engine refuses the null here";
        return false;
      }
    }
    Heard_[at] = 1;
    return true;
  }
  error = "the listener asks for '" + std::string(event) + "', which no event declares";
  return false;
}

bool TriggerField::Inside(const Door &door, const double atM[3]) const {
  if (door.Sphere != 0) {
    double away = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      const double gap = atM[axis] - door.AtM[axis];
      away += gap * gap;
    }
    return away <= door.ExtentM[0] * door.ExtentM[0];
  }
  for (int axis = 0; axis < 3; ++axis) {
    if (std::fabs(atM[axis] - door.AtM[axis]) > door.ExtentM[axis]) { return false; }
  }
  return true;
}

void TriggerField::Probe(uint32_t body, const double atM[3], double nowS) {
  const auto fire = [&](uint16_t event) {
    if (Heard_[event] == 0) { ++Unheard_[event]; }
    if (Ring_.size() >= kMostFired) {
      ++Overflowed_;
      return;
    }
    Ring_.push_back(Fired{event, body});
  };
  for (uint32_t which = 0; which < (uint32_t)Doors_.size(); ++which) {
    const Door &door = Doors_[which];
    const bool in = Inside(door, atM);
    std::vector<Standing> &seated = InsideDoor_[which];
    size_t standing = seated.size();
    for (size_t at = 0; at < seated.size(); ++at) {
      if (seated[at].Body == body) {
        standing = at;
        break;
      }
    }
    if (in && standing == seated.size()) {
      if (seated.size() >= kMostStandings) {
        // a body that cannot be seated would fire Enter every tick and never Exit --
        // counted, and NOT fired, because a door that cannot remember is not a door
        ++Unseated_;
        continue;
      }
      seated.push_back(Standing{body, which, nowS, false});
      if (door.Opens == When::Enter) { fire(door.Event); }
      continue;
    }
    if (in && standing < seated.size() && door.Opens == When::Dwell &&
        !seated[standing].Dwelt && nowS - seated[standing].SinceS >= door.DwellS) {
      seated[standing].Dwelt = true;
      fire(door.Event);
      continue;
    }
    if (!in && standing < seated.size()) {
      if (door.Opens == When::Exit) { fire(door.Event); }
      seated[standing] = seated.back();
      seated.pop_back();
    }
  }
}

std::span<const TriggerField::Fired> TriggerField::Drain() {
  Drained_.assign(Ring_.begin(), Ring_.end());
  Ring_.clear();
  return {Drained_.data(), Drained_.size()};
}

const std::string *TriggerField::EventNamed(uint16_t event) const {
  return event < Events_.size() ? &Events_[event] : nullptr;
}

size_t TriggerField::Unheard(std::string_view event) const {
  for (size_t at = 0; at < Events_.size(); ++at) {
    if (Events_[at] == event) { return Unheard_[at]; }
  }
  return 0;
}

} // namespace outshine
