#ifndef OUTSHINE_CLIENT_SCENARIOINPUT_H
#define OUTSHINE_CLIENT_SCENARIOINPUT_H

#include <Scenario.h>
#include <expected>
#include <string>
#include <string_view>

namespace outshine::Client {

[[nodiscard]] std::expected<Scenario::Document, std::string> ReadClientScenario(
    std::string_view source, std::string_view overrides, std::string_view resourceDirectory);

}

#endif
