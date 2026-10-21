#ifndef OUTSHINE_TEST_RENDER_NATIVEOFFLINE_H
#define OUTSHINE_TEST_RENDER_NATIVEOFFLINE_H

#include "src/render/device/native/Device.h"
#include <chrono>
#include <thread>

namespace outshine::Test::Gpu {

template <typename Operation>
auto RetryOffline(Operation &&operation, std::chrono::nanoseconds budget) {
  const auto began = std::chrono::steady_clock::now();
  auto result = operation();
  while (!result && result.error().Kind == Render::Native::ErrorKind::Busy &&
         std::chrono::steady_clock::now() - began < budget) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    result = operation();
  }
  return result;
}

}
#endif
