#ifndef OUTSHINE_BASE_CURVE_GAUSS8_H
#define OUTSHINE_BASE_CURVE_GAUSS8_H

#include <array>

namespace outshine::Gauss8 {
inline constexpr std::array Nodes{-0.9602898564975363,
                                  -0.7966664774136267,
                                  -0.5255324099163290,
                                  -0.1834346424956498,
                                  0.1834346424956498,
                                  0.5255324099163290,
                                  0.7966664774136267,
                                  0.9602898564975363};
inline constexpr std::array Weights{0.1012285362903763,
                                    0.2223810344533745,
                                    0.3137066458778873,
                                    0.3626837833783620,
                                    0.3626837833783620,
                                    0.3137066458778873,
                                    0.2223810344533745,
                                    0.1012285362903763};
inline constexpr double RemainderConstant = 0x1.4851476a7a7d3p-76;
}

#endif
