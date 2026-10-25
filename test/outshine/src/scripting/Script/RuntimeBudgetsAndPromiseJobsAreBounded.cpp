#include "Script.h"
#include "ScriptHostFixture.h"
#include "Check.h"
#include <chrono>
#include <string>

int main() {
  using namespace outshine::Script;
  using namespace outshine::Test;
  ScriptHostFixture host;
  std::string error;
  Budget limited;
  limited.InterruptChecks = 1;
  limited.Time = std::chrono::seconds(1);
  Program loop(limited);
  CHECK(loop.Read("while(true){}; capture('late');", error), "unbounded loop compiles normally");
  CHECK(!loop.Run(host, error) && !loop.Held() && loop.InterruptChecks() == 1 && host.Calls.empty(),
        "execution interrupts deterministically before a later host action");
  Program memory;
  CHECK(memory.Read("new ArrayBuffer(64*1024*1024)", error), "large allocation compiles");
  CHECK(!memory.Run(host, error) && !memory.Held(), "VM memory limit rejects oversized allocation");
  Program stack;
  CHECK(stack.Read("function recurse(){return recurse()} recurse()", error), "recursion compiles");
  CHECK(!stack.Run(host, error), "stack limit rejects recursion without overflowing the process");
  Program blocking;
  CHECK(blocking.Read("Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0)", error),
        "blocking primitive compiles");
  CHECK(!blocking.Run(host, error), "the runtime cannot block the caller in Atomics.wait");
  Program jobs;
  CHECK(jobs.Read("Promise.resolve(42).then(capture); Promise.reject('handled').catch(capture);",
                  error),
        "ordinary promise jobs compile");
  CHECK(jobs.Run(host, error) && host.Calls.size() == 2,
        "ready jobs finish within the active host scope");
  CHECK(jobs.Read("Promise.reject('broken')", error), "unhandled rejection compiles");
  CHECK(!jobs.Run(host, error) && error.contains("broken") && !jobs.Held(),
        "unhandled rejection is an execution failure");
  CHECK(jobs.Read("function again(){Promise.resolve().then(again)} again()", error),
        "self-scheduling promise compiles");
  CHECK(!jobs.Run(host, error) && !jobs.Held(), "pending-job chains cannot outlive the run budget");
  CHECK(jobs.Read("capture('recovered')", error) && jobs.Run(host, error) && host.Calls.size() == 3,
        "new compilation discards failed pending work and recovers");
  Budget input;
  input.InputBytes = 4;
  Program shortInput(input);
  CHECK(!shortInput.Read("capture('too long')", error) && !shortInput.Held(),
        "input limit rejects before allocating the VM");
  Budget payload;
  payload.HostBytes = 16;
  Program largeArguments(payload);
  const auto calls = host.Calls.size();
  CHECK(largeArguments.Read("capture('123456789','123456789')", error),
        "combined host payload compiles");
  CHECK(!largeArguments.Run(host, error) && host.Calls.size() == calls,
        "aggregate argument bytes are bounded before entering native host code");
  Budget accesses;
  accesses.HostAccesses = 4;
  Program nativeLoop(accesses);
  CHECK(nativeLoop.Read("while(true)capture(1)", error), "native call loop compiles");
  CHECK(!nativeLoop.Run(host, error) && host.Calls.size() - calls <= accesses.HostAccesses,
        "host access budget independently bounds external calls");
  return Report();
}
