set(OUTSHINE_GENERATED_DIRECTORY "${PROJECT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${OUTSHINE_GENERATED_DIRECTORY}")
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  OUTPUT_VARIABLE OUTSHINE_BUILD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND git status --porcelain WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  OUTPUT_VARIABLE dirty COMMAND_ERROR_IS_FATAL ANY)
if(NOT dirty STREQUAL "")
  string(APPEND OUTSHINE_BUILD_COMMIT "+dirty")
endif()
foreach(reference HEAD index packed-refs)
  execute_process(COMMAND git rev-parse --path-format=absolute --git-path "${reference}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" OUTPUT_VARIABLE path
    OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
endforeach()
execute_process(COMMAND git symbolic-ref -q HEAD WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  OUTPUT_VARIABLE reference OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT reference STREQUAL "")
  execute_process(COMMAND git rev-parse --path-format=absolute --git-path "${reference}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" OUTPUT_VARIABLE path
    OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
endif()
string(TOUPPER "${CMAKE_BUILD_TYPE}" configuration)
string(JOIN " " options ${OUTSHINE_COMPILE_OPTIONS})
set(definitions ${OUTSHINE_COMPILE_DEFINITIONS})
list(TRANSFORM definitions PREPEND "-D")
string(JOIN " " definitions ${definitions})
set(base_flags "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${configuration}} -std=c++${CMAKE_CXX_STANDARD} -fno-exceptions ${options} ${definitions}")
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  string(APPEND base_flags " -O2")
endif()
set(calibration_options)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
  set(calibration_options -mcpu=native)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  set(calibration_options -march=native)
endif()
target_compile_options(outshine_diagnostics_calibration_objects PRIVATE ${calibration_options})
foreach(component App Calibration)
  string(TOUPPER "${component}" component_upper)
  set(OUTSHINE_BUILD_GUARD "OUTSHINE_${component_upper}_BUILD_H")
  if(component STREQUAL "App")
    set(OUTSHINE_BUILD_NAMESPACE "outshine::App")
    set(target outshine_app_objects)
    set(OUTSHINE_BUILD_FLAGS "${base_flags}")
  else()
    set(OUTSHINE_BUILD_NAMESPACE "outshine::Diagnostics::Calibration")
    set(target outshine_diagnostics_calibration_objects)
    set(OUTSHINE_BUILD_FLAGS "${base_flags} ${calibration_options}")
  endif()
  configure_file("${CMAKE_CURRENT_LIST_DIR}/BuildInfo.h.in"
    "${OUTSHINE_GENERATED_DIRECTORY}/${component}Build.h" @ONLY)
  target_include_directories(${target} PRIVATE "${OUTSHINE_GENERATED_DIRECTORY}")
endforeach()
add_custom_command(OUTPUT "${OUTSHINE_GENERATED_DIRECTORY}/LeafOpticsData.inc"
  COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/test/scripts/prepare_leaf_optics.py"
    "${OUTSHINE_GENERATED_DIRECTORY}/LeafOpticsData.inc"
  DEPENDS "${PROJECT_SOURCE_DIR}/test/scripts/prepare_leaf_optics.py"
    "${PROJECT_SOURCE_DIR}/lib/src/outshine_ref/vegetation/data/leaf-optics-table-1.json"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" VERBATIM)
target_sources(outshine_generators_flora_objects PRIVATE "${OUTSHINE_GENERATED_DIRECTORY}/LeafOpticsData.inc")
target_include_directories(outshine_generators_flora_objects PRIVATE "${OUTSHINE_GENERATED_DIRECTORY}")
