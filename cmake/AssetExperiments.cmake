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

add_executable(outshine_experiment_source_geometry EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/source_geometry_control.cpp")
target_link_libraries(outshine_experiment_source_geometry PRIVATE outshine_engine)

add_executable(outshine_experiment_source_inputs EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/source_product_inputs.cpp")
target_link_libraries(outshine_experiment_source_inputs PRIVATE outshine_generators_building)

add_executable(outshine_experiment_building_geometry_cache EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/building_geometry_cache.cpp")
target_link_libraries(outshine_experiment_building_geometry_cache PRIVATE outshine_engine)

add_executable(outshine_experiment_source_group_bake EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/source_group_bake.cpp")
target_link_libraries(outshine_experiment_source_group_bake PRIVATE outshine_generators_building)

add_executable(outshine_experiment_building_geometry_asset EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/building_geometry_asset.cpp")
target_link_libraries(outshine_experiment_building_geometry_asset PRIVATE outshine_generators_building)

add_executable(outshine_experiment_surface_ground EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/NativeSurfaceGround.cpp")
target_link_libraries(outshine_experiment_surface_ground PRIVATE outshine_world outshine_world_products)

add_executable(outshine_experiment_surface_buildings EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/NativeSurfaceBuildings.cpp")
target_link_libraries(outshine_experiment_surface_buildings PRIVATE
  outshine_generators_building PkgConfig::nlohmann_json)

add_executable(outshine_experiment_group_impostors EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/NativeGroupImpostors.cpp")
target_link_libraries(outshine_experiment_group_impostors PRIVATE
  outshine_generators_flora outshine_render SDL3::SDL3 PkgConfig::nlohmann_json)

add_executable(outshine_experiment_tree_prototype EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/NativeTreePrototype.cpp")
target_link_libraries(outshine_experiment_tree_prototype PRIVATE
  outshine_generators_flora PkgConfig::nlohmann_json)
target_compile_options(outshine_experiment_tree_prototype PRIVATE -fno-exceptions)

add_executable(outshine_experiment_compose_groups EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/render/ComposeGroupImpostors.cpp")
target_link_libraries(outshine_experiment_compose_groups PRIVATE
  outshine_render PkgConfig::nlohmann_json)
target_compile_options(outshine_experiment_compose_groups PRIVATE -fno-exceptions)

if(APPLE)
  add_executable(outshine_experiment_metal_groups EXCLUDE_FROM_ALL
    "${PROJECT_SOURCE_DIR}/test/experiments/render/MetalGroupImpostorControl.cpp")
  target_link_libraries(outshine_experiment_metal_groups PRIVATE
    outshine_render_products outshine_app_capture PkgConfig::nlohmann_json)
  target_compile_options(outshine_experiment_metal_groups PRIVATE -fno-exceptions)
endif()

add_executable(outshine_experiment_road_display EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/streaming/tile-selection/RoadDisplayCache.cpp")
target_link_libraries(outshine_experiment_road_display PRIVATE outshine_generators_road_pages)

add_executable(outshine_experiment_building_shell_metadata EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/streaming/tile-selection/BuildingShellMetadata.cpp")
target_link_libraries(outshine_experiment_building_shell_metadata PRIVATE outshine_generators_building)

add_executable(outshine_experiment_prepared_roof_forms EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/streaming/tile-selection/PreparedRoofForms.cpp")
target_link_libraries(outshine_experiment_prepared_roof_forms PRIVATE outshine_generators_building)

add_executable(outshine_experiment_native_roof_prefix EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/streaming/tile-selection/NativeRoofPrefix.cpp")
target_link_libraries(outshine_experiment_native_roof_prefix PRIVATE outshine_engine_streaming)

add_executable(outshine_experiment_footprint_cards EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/streaming/tile-selection/NativeFootprintCards.cpp")
target_link_libraries(outshine_experiment_footprint_cards PRIVATE outshine_app_capture outshine_engine)

add_executable(outshine_experiment_prepared_terraces EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/experiments/streaming/tile-selection/NativePreparedTerraces.cpp")
target_link_libraries(outshine_experiment_prepared_terraces PRIVATE outshine_generators_osm)

foreach(probe IN ITEMS outshine_experiment_urban_bake outshine_experiment_plant_prepare
    outshine_experiment_urban_control outshine_experiment_source_geometry outshine_experiment_building_geometry_cache
    outshine_experiment_source_group_bake outshine_experiment_building_geometry_asset
    outshine_experiment_surface_ground outshine_experiment_surface_buildings
    outshine_experiment_group_impostors
    outshine_experiment_source_inputs outshine_experiment_road_display
    outshine_experiment_building_shell_metadata outshine_experiment_prepared_roof_forms
    outshine_experiment_native_roof_prefix outshine_experiment_footprint_cards
    outshine_experiment_prepared_terraces)
  target_compile_options(${probe} PRIVATE -fno-exceptions)
endforeach()
