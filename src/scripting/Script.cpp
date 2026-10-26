#include "Script.h"
#include "ScriptRuntime.h"
#include <quickjs.h>
#include <chrono>
#include <memory>
#include <cstddef>
#include <utility>
#include <string>
#include <string_view>

namespace outshine::Script {

Program::Program(Budget budget) : Budget_(budget) {}

Program::~Program() = default;
Program::Program(Program &&) noexcept = default;
Program &Program::operator=(Program &&) noexcept = default;

bool Program::Held() const {
  return Runtime_ && Runtime_->OnOwner() && !JS_IsUndefined(Runtime_->Compiled);
}

size_t Program::InterruptChecks() const {
  return Runtime_ && Runtime_->OnOwner() ? Runtime_->Checks : 0;
}

bool Program::Read(std::string_view text, std::string &error) {
  if (Runtime_ && !Runtime_->OnOwner()) {
    error = "JavaScript runtime belongs to another thread";
    return false;
  }
  if (Runtime_ && Runtime_->Bindings != nullptr) {
    error = "JavaScript runtime cannot be replaced during a host call";
    return false;
  }
  Runtime_.reset();
  error.clear();
  if (text.size() > Budget_.InputBytes) {
    error = "JavaScript source exceeds its input budget";
    return false;
  }
  if (text.contains('\0')) {
    error = "JavaScript source contains a NUL byte; use a Unicode escape in string literals";
    return false;
  }
  auto candidate = std::make_unique<Runtime>(Budget_);
  if (!candidate->Open(error)) { return false; }
  const std::string terminated(text);
  candidate->Deadline = std::chrono::steady_clock::now() + Budget_.Time;
  candidate->Compiled = JS_Eval(candidate->Context,
                                terminated.c_str(),
                                terminated.size(),
                                "outshine-action",
                                JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
  if (JS_IsException(candidate->Compiled)) {
    candidate->Compiled = JS_UNDEFINED;
    error = candidate->Exception();
    return false;
  }
  Runtime_ = std::move(candidate);
  return true;
}

bool Program::Run(Host &host, std::string &error) {
  error.clear();
  if (!Held()) {
    error = "no JavaScript program belongs to this thread";
    return false;
  }
  Runtime &state = *Runtime_;
  if (state.BoundHost != nullptr && state.BoundHost != &host) {
    error = "JavaScript program requires its bound host until the next Read";
    return false;
  }
  if (state.Bindings != nullptr) {
    error = "JavaScript runtime cannot be reentered from a host call";
    return false;
  }
  state.Checks = 0;
  state.HostAccesses = 0;
  state.BoundHost = &host;
  state.Deadline = std::chrono::steady_clock::now() + state.Limit.Time;
  state.Bindings = &host;
  const JSValue result = JS_EvalFunction(state.Context, JS_DupValue(state.Context, state.Compiled));
  bool succeeded = !JS_IsException(result);
  JS_FreeValue(state.Context, result);
  while (succeeded && JS_IsJobPending(state.Vm)) {
    if (state.Exhausted()) {
      (void)JS_ThrowInternalError(state.Context, "JavaScript execution budget exhausted");
      succeeded = false;
      break;
    }
    JSContext *context = nullptr;
    succeeded = JS_ExecutePendingJob(state.Vm, &context) >= 0;
  }
  if (succeeded && (state.Rejections != 0 || state.RejectionOverflow)) {
    if (state.Rejections != 0) {
      (void)JS_Throw(state.Context, JS_DupValue(state.Context, state.Rejected[0].Reason));
    } else {
      (void)JS_ThrowInternalError(state.Context, "JavaScript rejected-promise budget exhausted");
    }
    succeeded = false;
  }
  state.Bindings = nullptr;
  if (!succeeded) {
    error = state.Exception();
    JS_FreeValue(state.Context, state.Compiled);
    state.Compiled = JS_UNDEFINED;
  }
  return succeeded;
}

}
