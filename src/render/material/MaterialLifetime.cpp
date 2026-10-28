#include "MaterialLifetime.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

namespace outshine::Render {
void MaterialRetirementQueue::Push(MaterialRetirement event) {
  const std::scoped_lock lock(Mutex_);
  Events_.push_back(event);
}

void MaterialRetirementQueue::Take(std::vector<MaterialRetirement> &events) {
  events.clear();
  const std::scoped_lock lock(Mutex_);
  Events_.swap(events);
}

MaterialLifetime::~MaterialLifetime() {
  for (const auto &subscription : Subscriptions_) {
    if (const auto queue = subscription.Queue.lock()) { queue->Push(subscription.Event); }
  }
}

void MaterialLifetime::Subscribe(const std::shared_ptr<MaterialRetirementQueue> &queue,
                                 MaterialRetirement event) {
  const std::scoped_lock lock(Mutex_);
  std::erase_if(Subscriptions_, [](const auto &held) { return held.Queue.expired(); });
  for (const auto &held : Subscriptions_) {
    if (held.Queue.lock() == queue && held.Event == event) { return; }
  }
  Subscriptions_.push_back({.Queue = queue, .Event = event});
}
}
