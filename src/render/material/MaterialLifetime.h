#ifndef OUTSHINE_RENDER_MATERIAL_MATERIALLIFETIME_H
#define OUTSHINE_RENDER_MATERIAL_MATERIALLIFETIME_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace outshine::Render {
struct MaterialRetirement {
  uint32_t Slot = 0;
  uint64_t Generation = 0;
  [[nodiscard]] bool operator==(const MaterialRetirement &) const noexcept = default;
};

class MaterialRetirementQueue {
public:
  void Push(MaterialRetirement event);
  void Take(std::vector<MaterialRetirement> &events);

private:
  std::mutex Mutex_;
  std::vector<MaterialRetirement> Events_;
};

class MaterialLifetime {
public:
  ~MaterialLifetime();
  void Subscribe(const std::shared_ptr<MaterialRetirementQueue> &queue, MaterialRetirement event);

private:
  struct Subscription {
    std::weak_ptr<MaterialRetirementQueue> Queue;
    MaterialRetirement Event;
  };

  std::mutex Mutex_;
  std::vector<Subscription> Subscriptions_;
};
}
#endif
