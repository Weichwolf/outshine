#ifndef OUTSHINE_TEST_SHARED_TESTENGINE_H
#define OUTSHINE_TEST_SHARED_TESTENGINE_H

#include "EngineHeld.h"
#include <memory>

namespace outshine {
class TestEngine {
public:
  [[nodiscard]] static Engine WithGpu(Render::GpuSubmission submission) {
    return Engine(std::make_unique<Engine::State>(submission));
  }
};
}

#endif
