#include "UiAction.h"
#include "Event.h"
#include "Check.h"
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace {
struct Receiver final : outshine::Host {
  size_t Calls = 0;

  bool calls(std::string_view name,
             [[maybe_unused]] std::span<const outshine::Argument> args) override {
    ++Calls;
    return name == "clicked";
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Receiver receiver;
  const auto executed =
      RunUiAction(&receiver, "const yes=()=>[1,2].includes(2)", "if(yes())clicked()");
  CHECK(executed && *executed && receiver.Calls == 1,
        "normal UI JavaScript reaches its native action");
  const auto silent = RunUiAction(&receiver, "", "1+2");
  CHECK(silent && !*silent && receiver.Calls == 1,
        "pure computation does not invent a handled action");
  CHECK(!RunUiAction(nullptr, "", "clicked()"), "missing application host is an error");
  CHECK(!RunUiAction(&receiver, "const bytes=new ArrayBuffer(1024*1024)", "clicked()") &&
            receiver.Calls == 1,
        "the Main/UI heap profile rejects bulk allocations before native actions");
  constexpr size_t kOverBudgetBytes = size_t{65} * 1024;
  const std::string large(kOverBudgetBytes, ' ');
  CHECK(!RunUiAction(&receiver, large, "clicked()") && receiver.Calls == 1,
        "oversized composed source is rejected before a second large copy or execution");
  CHECK(!RunUiAction(&receiver, "Promise.reject('broken')", "") && receiver.Calls == 1,
        "unhandled UI promise errors reach the caller");
  return Report();
}
