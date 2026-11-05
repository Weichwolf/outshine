set(serial_geometry_scan "${PROJECT_BINARY_DIR}/shaders/geometryRootScan.comp.spv")
add_custom_command(OUTPUT "${serial_geometry_scan}"
  COMMAND "${OUTSHINE_GLSLANG}" -V --target-env vulkan1.0 -S comp
    "-I${PROJECT_SOURCE_DIR}/src/render/shaders"
    "${PROJECT_SOURCE_DIR}/test/experiments/render/grouped_scan/serial.comp"
    -o "${serial_geometry_scan}"
  DEPENDS "${PROJECT_SOURCE_DIR}/test/experiments/render/grouped_scan/serial.comp"
    "${PROJECT_SOURCE_DIR}/src/render/shaders/geometryHierarchy.glsl" VERBATIM)
add_custom_target(outshine_experiment_serial_geometry_scan DEPENDS "${serial_geometry_scan}")
add_executable(outshine_experiment_group_scan EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/harness/render/GeometryGroupScanProbe.cpp")
target_include_directories(outshine_experiment_group_scan PRIVATE
  "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/test/harness/shared")
target_compile_options(outshine_experiment_group_scan PRIVATE -fno-exceptions)
target_link_libraries(outshine_experiment_group_scan PRIVATE outshine_render SDL3::SDL3)
add_dependencies(outshine_experiment_group_scan outshine_shaders outshine_experiment_serial_geometry_scan)
