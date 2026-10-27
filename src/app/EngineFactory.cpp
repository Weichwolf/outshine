#include "EngineFactory.h"
#include "Engine.h"
#include "BuiltinSourceProviders.h"
#include <utility>

namespace outshine::App {
Engine CreateEngine() {
  Data::ProviderRegistry providers;
  Generators::RegisterBuiltinSourceProviders(providers);
  return Engine(std::move(providers));
}
}
