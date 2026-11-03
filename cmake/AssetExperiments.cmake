add_executable(outshine_experiment_urban_bake EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/urban_fields/UrbanAreaBake.cpp")
target_include_directories(outshine_experiment_urban_bake PRIVATE
  "${PROJECT_SOURCE_DIR}/test/harness/shared")
target_link_libraries(outshine_experiment_urban_bake PRIVATE outshine_generators_osm)

add_executable(outshine_experiment_plant_prepare EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/NativePlantWorldPrepare.cpp")
target_link_libraries(outshine_experiment_plant_prepare PRIVATE outshine_generators_flora)

add_executable(outshine_experiment_urban_control EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/urban_fields/NativeUrbanFieldControl.cpp")
target_include_directories(outshine_experiment_urban_control PRIVATE
  "${PROJECT_SOURCE_DIR}/test/harness/shared")
target_link_libraries(outshine_experiment_urban_control PRIVATE outshine_app_capture outshine_engine)

foreach(probe IN ITEMS outshine_experiment_urban_bake outshine_experiment_plant_prepare
    outshine_experiment_urban_control)
  target_compile_options(${probe} PRIVATE -fno-exceptions)
endforeach()
