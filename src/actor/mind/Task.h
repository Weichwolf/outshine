#ifndef OUTSHINE_ACTOR_MIND_TASK_H
#define OUTSHINE_ACTOR_MIND_TASK_H

#include <memory>

namespace outshine::Mind {

enum class Doing : uint8_t { Running, Done, Abandoned };

class Task {
public:
  virtual ~Task() = default;
  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;

  [[nodiscard]] Doing Step(double dtS);
  void Abandon();

  [[nodiscard]] bool Holds() const { return Under_ != nullptr; }
  [[nodiscard]] size_t Deep() const;

protected:
  Task() = default;

  [[nodiscard]] virtual Doing Act(double dtS) = 0;

  void Hands(std::unique_ptr<Task> under) { Under_ = std::move(under); }
  void Drops() { Under_.reset(); }

private:
  std::unique_ptr<Task> Under_;
  bool Left_ = false;
};

}

#endif
