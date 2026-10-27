#include "EngineFactory.h"
#include "Engine.h"
#include "BuiltinSourceProviders.h"
#include "Check.h"
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Data::ProviderRegistry factories;
  Generators::RegisterBuiltinSourceProviders(factories);
  const Data::Provider *stars = factories.named("stars");
  CHECK(stars != nullptr, "the builtin catalogue contains the expected star factory");
  if (stars == nullptr) { return Report(); }

  auto engine = App::CreateEngine();
  CHECK(!engine.registerProvider(*stars), "application installs factories before any declaration");
  Scenario::Document document;
  Data::SourceProvider declaration;
  declaration.Kind = "stars";
  document.Providers.push_back(std::move(declaration));
  CHECK(engine.declare(document), "application host accepts its builtin source declarations");
  CHECK(engine.loading().ProviderStarts == 0 && engine.loading().SourceBytes == 0,
        "factory composition and declaration acquire no source bytes");
  return Report();
}
