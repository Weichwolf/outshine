#include "EngineFactory.h"
#include "Engine.h"
#include "ShippedProviders.h"
#include <utility>

namespace outshine::App {
Engine CreateEngine() {
  Data::ProviderRegistry providers;
  Generators::RegisterShippedProviders(providers);
  return Engine(std::move(providers));
}
}
