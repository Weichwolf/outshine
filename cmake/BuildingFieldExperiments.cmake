file(GLOB building_field_sources CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/test/experiments/render/building_fields/*.cpp"
  "${PROJECT_SOURCE_DIR}/test/experiments/render/building_fields/*.h")
add_library(test_experiments_building_fields STATIC EXCLUDE_FROM_ALL ${building_field_sources})
target_include_directories(test_experiments_building_fields PUBLIC
  "${PROJECT_SOURCE_DIR}/test/experiments/render/building_fields")
target_compile_options(test_experiments_building_fields PRIVATE -fno-exceptions)
target_link_libraries(test_experiments_building_fields PUBLIC
  outshine_content_assets outshine_generators_building outshine_engine_streaming outshine_render)

add_library(test_harness_building_source_capture STATIC EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/BuildingSourceCapture.cpp")
target_include_directories(test_harness_building_source_capture PUBLIC
  "${PROJECT_SOURCE_DIR}/test/harness/render")
target_compile_options(test_harness_building_source_capture PRIVATE -fno-exceptions)
target_link_libraries(test_harness_building_source_capture PUBLIC
  test_experiments_building_fields outshine_core PkgConfig::nlohmann_json)

set(building_field_gpu_cases
  outshine/integration/experiments/building_fields/BuildingFieldRequest/WorldDescriptorsResolveOnlyTheirRequestedField
  outshine/integration/experiments/building_fields/ImpostorViewAssets/CachedViewsPreservePublishedWorlds)
