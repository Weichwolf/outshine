set(group_scan_directory "${PROJECT_BINARY_DIR}/shaders/grouped-scan")
file(MAKE_DIRECTORY "${group_scan_directory}")
set(group_scan_shaders)
foreach(name IN ITEMS prefix-first prefix-block carry finish)
  set(source "${name}")
  set(defines)
  if(name MATCHES "^prefix-")
    set(source prefix)
    if(name STREQUAL "prefix-first")
      list(APPEND defines -DLIVE_INPUT=1)
    endif()
  endif()
  set(input "${PROJECT_SOURCE_DIR}/test/experiments/render/grouped_scan/${source}.comp")
  set(output "${group_scan_directory}/${name}.spv")
  add_custom_command(OUTPUT "${output}"
    COMMAND "${OUTSHINE_GLSLANG}" -V --target-env vulkan1.0 -S comp ${defines}
      "${input}" -o "${output}"
    DEPENDS "${input}" VERBATIM)
  list(APPEND group_scan_shaders "${output}")
endforeach()
add_custom_target(outshine_experiment_group_scan_shaders DEPENDS ${group_scan_shaders})
add_executable(outshine_experiment_group_scan EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/harness/render/GeometryGroupScanProbe.cpp")
target_include_directories(outshine_experiment_group_scan PRIVATE
  "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/test/harness/shared")
target_compile_options(outshine_experiment_group_scan PRIVATE -fno-exceptions)
target_link_libraries(outshine_experiment_group_scan PRIVATE outshine_render_device SDL3::SDL3)
add_dependencies(outshine_experiment_group_scan outshine_shaders
  outshine_experiment_group_scan_shaders)
