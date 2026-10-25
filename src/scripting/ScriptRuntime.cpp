#include "ScriptRuntime.h"
#include <quickjs.h>
#include <chrono>
#include <cstddef>
#include <string>

namespace outshine::Script {
namespace {
int Interrupt(JSRuntime *vm, void *opaque) {
  return JS_GetRuntimeOpaque(vm) != opaque || static_cast<Runtime *>(opaque)->Exhausted() ? 1 : 0;
}

void TrackRejection(
    JSContext *context, JSValueConst promise, JSValueConst reason, bool handled, void *opaque) {
  static_cast<Runtime *>(opaque)->Reject(context, promise, reason, handled);
}
}

Runtime::~Runtime() {
  if (Context != nullptr) {
    JS_FreeValue(Context, Compiled);
    for (size_t row = 0; row < ReferenceCount; ++row) {
      JS_FreeValue(Context, References[row].Object);
    }
    for (size_t row = 0; row < Rejections; ++row) {
      JS_FreeValue(Context, Rejected[row].Promise);
      JS_FreeValue(Context, Rejected[row].Reason);
    }
    JS_FreeContext(Context);
  }
  if (Vm != nullptr) { JS_FreeRuntime(Vm); }
}

bool Runtime::Open(std::string &error) {
  if (Limit.MemoryBytes == 0 || Limit.StackBytes == 0 || Limit.InterruptChecks == 0 ||
      Limit.Time <= std::chrono::microseconds::zero()) {
    error = "JavaScript requires positive memory, stack and execution budgets";
    return false;
  }
  Vm = JS_NewRuntime();
  if (Vm == nullptr) {
    error = "JavaScript runtime allocation failed";
    return false;
  }
  JS_SetMemoryLimit(Vm, Limit.MemoryBytes);
  JS_SetMaxStackSize(Vm, Limit.StackBytes);
  JS_SetGCThreshold(Vm, Limit.MemoryBytes / 2);
  JS_SetCanBlock(Vm, false);
  Context = JS_NewContext(Vm);
  if (Context == nullptr) {
    error = "JavaScript context exceeds its memory budget";
    return false;
  }
  JS_SetContextOpaque(Context, this);
  JS_SetRuntimeOpaque(Vm, this);
  JS_SetInterruptHandler(Vm, Interrupt, this);
  JS_SetHostPromiseRejectionTracker(Vm, TrackRejection, this);
  return BindHost(error);
}

bool Runtime::Exhausted() {
  ++Checks;
  return Checks >= Limit.InterruptChecks || std::chrono::steady_clock::now() >= Deadline;
}

std::string Runtime::Exception() const {
  const JSValue exception = JS_GetException(Context);
  const char *text = JS_ToCString(Context, exception);
  const std::string error = text != nullptr ? text : "JavaScript allocation or conversion failed";
  JS_FreeCString(Context, text);
  JS_FreeValue(Context, exception);
  return error;
}

bool Runtime::UseHost() {
  if (HostAccesses >= Limit.HostAccesses || std::chrono::steady_clock::now() >= Deadline) {
    (void)JS_ThrowInternalError(Context, "JavaScript host access budget exhausted");
    return false;
  }
  ++HostAccesses;
  return true;
}

void Runtime::Reject(JSContext *context, JSValueConst promise, JSValueConst reason, bool handled) {
  for (size_t row = 0; row < Rejections; ++row) {
    if (!JS_IsStrictEqual(context, promise, Rejected[row].Promise)) { continue; }
    if (handled) {
      JS_FreeValue(context, Rejected[row].Promise);
      JS_FreeValue(context, Rejected[row].Reason);
      Rejected[row] = Rejected[--Rejections];
      Rejected[Rejections] = {};
    }
    return;
  }
  if (handled) { return; }
  if (Rejections == Rejected.size()) {
    RejectionOverflow = true;
    return;
  }
  Rejected[Rejections++] = {.Promise = JS_DupValue(context, promise),
                            .Reason = JS_DupValue(context, reason)};
}

}
