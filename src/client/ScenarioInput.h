#ifndef OUTSHINE_CLIENT_SCENARIOINPUT_H
#define OUTSHINE_CLIENT_SCENARIOINPUT_H

#include <scenario/Scenario.h>
#include <expected>
#include <string>
#include <string_view>

namespace outshine {
struct Roots;
}

namespace outshine::Client {

[[nodiscard]] std::expected<Scenario::Document, std::string>
ReadClientScenario(std::string_view source, std::string_view overrides, const Roots &roots);

}

#endif
