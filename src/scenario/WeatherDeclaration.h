#ifndef OUTSHINE_SCENARIO_WEATHERDECLARATION_H
#define OUTSHINE_SCENARIO_WEATHERDECLARATION_H

#include <world/weather/WeatherSnapshot.h>

namespace outshine::Scenario {

struct WeatherDeclaration {
  WeatherPosition At{};
  WeatherValues Values{};
};

}
#endif
