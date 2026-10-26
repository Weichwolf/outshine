#ifndef OUTSHINE_APP_SCENARIOINPUT_H
#define OUTSHINE_APP_SCENARIOINPUT_H

#include <Scenario.h>
#include <expected>
#include <string>
#include <string_view>

namespace outshine::App {

[[nodiscard]] std::expected<Scenario::Document, std::string> ReadScenarioInput(
    std::string_view source, std::string_view overrides, std::string_view resourceDirectory);

}

#endif
