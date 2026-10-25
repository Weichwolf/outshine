#ifndef OUTSHINE_SCRIPTING_SCRIPTRUNTIME_H
#define OUTSHINE_SCRIPTING_SCRIPTRUNTIME_H

#include "Script.h"
#include <quickjs.h>
#include <array>
#include <chrono>
#include <expected>
#include <string>
#include <thread>

namespace outshine::Script {

struct Runtime {
  explicit Runtime(Budget budget) : Limit(budget) {}

  ~Runtime();
  Runtime(const Runtime &) = delete;
  Runtime &operator=(const Runtime &) = delete;

  [[nodiscard]] bool Open(std::string &error);
  [[nodiscard]] bool BindHost(std::string &error);

  [[nodiscard]] bool OnOwner() const noexcept { return Owner == std::this_thread::get_id(); }

  [[nodiscard]] std::string Exception() const;
  [[nodiscard]] JSValue ToJs(const Value &value);
  [[nodiscard]] std::expected<Value, std::string> FromJs(JSValueConst value, size_t bytes) const;
  [[nodiscard]] bool UseHost();
  [[nodiscard]] bool Exhausted();
  void Reject(JSContext *context, JSValueConst promise, JSValueConst reason, bool handled);

  Budget Limit;
  JSRuntime *Vm = nullptr;
  JSContext *Context = nullptr;
  JSValue Compiled = JS_UNDEFINED;
  JSClassID ReferenceClass = 0;
  Host *Bindings = nullptr;
  Host *BoundHost = nullptr;
  size_t Checks = 0;
  size_t HostAccesses = 0;
  bool RejectionOverflow = false;
  std::chrono::steady_clock::time_point Deadline;

  struct Rejection {
    JSValue Promise = JS_UNDEFINED;
    JSValue Reason = JS_UNDEFINED;
  };

  std::array<Rejection, 32> Rejected;
  size_t Rejections = 0;

  struct Reference {
    int Id = 0;
    JSValue Object = JS_UNDEFINED;
  };

  std::array<Reference, kMaxNames> References;
  size_t ReferenceCount = 0;

  std::thread::id Owner = std::this_thread::get_id();
};

}
#endif
