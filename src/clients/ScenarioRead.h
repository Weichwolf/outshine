#ifndef OUTSHINE_CLIENTS_SCENARIOREAD_H
#define OUTSHINE_CLIENTS_SCENARIOREAD_H

#include <cstddef>
#include <string>

#include <outshine/Scenario.h>

namespace outshine {

[[nodiscard]] bool ReadScenario(const char *text, size_t length, Scenario &into,
                                std::string &error);

}

#endif
