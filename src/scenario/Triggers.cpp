#include "Triggers.h"
#include <world/Entity.h>
#include "math/Vec3.h"

#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>
#include <cstddef>
#include <expected>
#include <string>
#include <span>
#include <cstdint>
#include <vector>
#include <string_view>

namespace outshine {

namespace Says {
constexpr auto InvalidVolumeGeometry =
    "volume center and extents must be finite; extents must be nonnegative";
constexpr auto InvalidDwell = "dwell duration must be finite and positive";
constexpr auto ExcessEvents = "event catalog exceeds the 16-bit index capacity";
constexpr auto InvalidEventName = "event names must be nonempty and unique";
}

namespace {

constexpr size_t kMostVolumes = 256;
constexpr size_t kMostEvents = static_cast<size_t>(std::numeric_limits<uint16_t>::max()) + 1;
static_assert(kMostEvents > std::numeric_limits<uint16_t>::max());
using EventIndex = std::unordered_map<std::string_view, uint16_t>;
constexpr size_t kMostOccupantsPerVolume = 256;
constexpr size_t kMostFired = 256;

[[nodiscard]] std::expected<EventIndex, std::string>
BuildEventIndex(std::span<const Scenario::Event> events) {
  if (events.size() > kMostEvents) { return std::unexpected(Says::ExcessEvents); }
  EventIndex index;
  index.reserve(events.size());
  for (size_t at = 0; at < events.size(); ++at) {
    if (events[at].Name.empty() ||
        !index.emplace(events[at].Name, static_cast<uint16_t>(at)).second) {
      return std::unexpected(Says::InvalidEventName);
    }
  }
  return index;
}

}

std::expected<TriggerField::PreparedVolume, std::string>
TriggerField::PrepareVolume(const Scenario::Volume &volume, uint16_t event) {
  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(volume.AtM[axis]) || !std::isfinite(volume.ExtentM[axis]) ||
        volume.ExtentM[axis] < 0.0) {
      return std::unexpected(Says::InvalidVolumeGeometry);
    }
  }
  PreparedVolume prepared;
  if (volume.When == "enter") {
    prepared.Opens = When::Enter;
  } else if (volume.When == "exit") {
    prepared.Opens = When::Exit;
  } else if (volume.When == "dwell") {
    prepared.Opens = When::Dwell;
  } else {
    return std::unexpected("volume '" + volume.Id + "' fires when '" + volume.When +
                           "', and a volume fires on enter, exit or dwell -- the engine "
                           "spells no fourth");
  }
  if (volume.Shape == "sphere") {
    prepared.Sphere = 1;
  } else if (volume.Shape == "box" || volume.Shape.empty()) {
    prepared.Sphere = 0;
  } else {
    return std::unexpected("volume '" + volume.Id + "' is a '" + volume.Shape +
                           "', and a volume is a box or a sphere");
  }
  if (prepared.Opens == When::Dwell && (!std::isfinite(volume.DwellS) || volume.DwellS <= 0.0)) {
    return std::unexpected(Says::InvalidDwell);
  }
  prepared.Event = event;
  prepared.AtM = volume.AtM;
  prepared.ExtentM = volume.ExtentM;
  prepared.DwellS = volume.DwellS;
  return prepared;
}

std::expected<TriggerField, std::string>
TriggerField::Stand(std::span<const Scenario::Volume> volumes,
                    std::span<const Scenario::Event> events) {
  if (volumes.size() > kMostVolumes) {
    return std::unexpected("the scenario declares " + std::to_string(volumes.size()) +
                           " volumes over the pool's " + std::to_string(kMostVolumes));
  }
  auto index = BuildEventIndex(events);
  if (!index) { return std::unexpected(std::move(index.error())); }
  TriggerField standing;
  standing.Events_.reserve(events.size());
  standing.Carries_.reserve(events.size());
  standing.Heard_.assign(events.size(), 0);
  standing.Unheard_.assign(events.size(), 0);
  for (const auto &event : events) {
    standing.Events_.push_back(event.Name);
    standing.Carries_.emplace_back(event.Carries.begin(), event.Carries.end());
  }
  standing.Volumes_.reserve(volumes.size());
  for (const auto &volume : volumes) {
    const auto named = index->find(volume.Fires);
    if (named == index->end()) {
      return std::unexpected("volume '" + volume.Id + "' refers to undeclared event '" +
                             volume.Fires + "'");
    }
    auto prepared = PrepareVolume(volume, named->second);
    if (!prepared) { return std::unexpected(std::move(prepared.error())); }
    standing.Volumes_.push_back(*prepared);
  }
  standing.Occupants_.resize(standing.Volumes_.size());
  for (auto &occupants : standing.Occupants_) { occupants.reserve(kMostOccupantsPerVolume); }
  standing.Ring_.reserve(kMostFired);
  standing.Drained_.reserve(kMostFired);
  return standing;
}

bool TriggerField::Listen(std::string_view event,
                          std::span<const std::string_view> reads,
                          std::string &error) {
  for (size_t at = 0; at < Events_.size(); ++at) {
    if (Events_[at] != event) { continue; }
    for (const std::string_view read : reads) {
      bool carried = false;
      for (const std::string &field : Carries_[at]) {
        if (field == read) { carried = true; }
      }
      if (!carried) {
        error = "the listener reads '" + std::string(read) + "' from '" + std::string(event) +
                "', which carries none -- a field is declared or it "
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

bool TriggerField::Inside(const PreparedVolume &door, const Vec3 &atM) {
  if (door.Sphere != 0) {
    return std::hypot(atM[0] - door.AtM[0], atM[1] - door.AtM[1], atM[2] - door.AtM[2]) <=
           door.ExtentM[0];
  }
  for (int axis = 0; axis < 3; ++axis) {
    if (std::fabs(atM[axis] - door.AtM[axis]) > door.ExtentM[axis]) { return false; }
  }
  return true;
}

void TriggerField::Probe(Entity body, const Vec3 &atM, double nowS) {
  const auto fire = [&](uint16_t event) {
    if (Heard_[event] == 0) { ++Unheard_[event]; }
    if (Ring_.size() >= kMostFired) {
      ++Overflowed_;
      return;
    }
    Ring_.push_back(Fired{.Event = event, .Body = body});
  };
  for (uint32_t which = 0; which < static_cast<uint32_t>(Volumes_.size()); ++which) {
    const PreparedVolume &door = Volumes_[which];
    const bool in = Inside(door, atM);
    std::vector<Occupant> &seated = Occupants_[which];
    size_t standing = seated.size();
    for (size_t at = 0; at < seated.size(); ++at) {
      if (seated[at].Body == body) {
        standing = at;
        break;
      }
    }
    if (in && standing == seated.size()) {
      if (seated.size() >= kMostOccupantsPerVolume) {
        ++Unseated_;
        continue;
      }
      seated.push_back(Occupant{.Body = body, .SinceS = nowS, .Dwelt = false});
      if (door.Opens == When::Enter) { fire(door.Event); }
      continue;
    }
    if (in && standing < seated.size() && door.Opens == When::Dwell && !seated[standing].Dwelt &&
        nowS - seated[standing].SinceS >= door.DwellS) {
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

}
