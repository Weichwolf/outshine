cmake_minimum_required(VERSION 3.24)
project(outshine_water_solver_comparison LANGUAGES CXX)

find_package(highs CONFIG REQUIRED)
find_package(osqp CONFIG REQUIRED)

add_executable(highs-profile-probe highs_profile_probe.cpp)
target_link_libraries(highs-profile-probe PRIVATE highs::highs)
add_executable(osqp-profile-probe osqp_profile_probe.cpp)
target_link_libraries(osqp-profile-probe PRIVATE highs::highs osqp::osqp)

foreach(probe IN ITEMS highs-profile-probe osqp-profile-probe)
  target_compile_features(${probe} PRIVATE cxx_std_23)
  if(MSVC)
    target_compile_options(${probe} PRIVATE /W4 /WX /EHs-c-)
  else()
    target_compile_options(${probe} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror
                                           -fno-exceptions -ffp-contract=off)
  endif()
endforeach()
