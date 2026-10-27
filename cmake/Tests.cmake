file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/test-output")
file(STRINGS "${PROJECT_SOURCE_DIR}/test/world-gate.txt" gate_scopes)
file(GLOB_RECURSE cases CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/test/outshine/src/*.cpp"
  "${PROJECT_SOURCE_DIR}/test/outshine/integration/*.cpp")
add_custom_target(outshine_tests)
add_custom_target(outshine_gate)
add_dependencies(outshine_gate outshine)
set(capture_cases
  outshine/integration/engine/streaming/ImpostorPreparation/PreparationRetainsGeneratorSurfaces
  outshine/integration/generators/flora/TreePrototype/TreeGeometryUsesNativeMaterials
  outshine/integration/render/material/MaterialResolution/NativeImagesReachTheRenderer
  outshine/integration/render/device/GpuSubmission/WindowFramesCloseAfterDrawing
  outshine/src/engine/Engine/CameraFollowsItsNamedEntity
  outshine/src/engine/Engine/NativeMipImagesReachTheRenderer
  outshine/src/engine/Engine/PerspectiveNativeMipImagesRepeatLinearPixels
  outshine/src/engine/Engine/ScenarioViewsPreserveProjection
  outshine/src/engine/RuntimeScene/PieceInstancesShareTheirGeometry
)
set(metal_validation_cases
  outshine/integration/render/scene/RenderScene/ViewsShareTerrainWithoutRebuilding
  outshine/src/engine/RuntimeScene/SynchronousGeometryCompletesAcceptedUploads
  outshine/src/render/prism/PrismDrawing/ReceiverPlanesPreserveAffineShadowDepth
  outshine/src/render/scene/SceneResources/PrismHandlesSurviveRestoreAndRejectPartialReplacement
  outshine/src/render/scene/SceneResources/UploadPressurePausesPieceRestore
  outshine/src/render/shadow/SunShadowPass/LocalChangesMatchFullAtlas
  outshine/src/render/stages/GeometryRenderer/AcceptedPiecesSurviveMainMeshReplacement
  outshine/src/render/stages/GeometryRenderer/EmissionUploadsFollowUnlitDrawConsumers
  outshine/src/render/stages/GeometryRenderer/InstancesShareTheirPieceClusterSpheres
  outshine/src/render/stages/GeometryRenderer/NativePieceClustersBoundTheirUploadedIndices
  outshine/src/render/stages/GeometryRenderer/OptionalAttributesReserveOnlyTheirConsumers
  outshine/src/render/stages/GeometryRenderer/PassPipelinesExcludeUnreachableMaterials
  outshine/src/render/stages/GeometryRenderer/RigidMainMeshesAvoidDuplicatePreviousPositions
  outshine/src/render/stages/GeometryResidency/DeferredImagesSurviveBackpressureAndRejectedSubmission
  outshine/src/render/stages/GeometryResidency/HalfFloatImagesRetainMipOffsetsAcrossDeferredUpload
  outshine/src/render/stages/GeometryResidency/MipSubmissionFailureRetainsRetry
  outshine/src/render/stages/GeometryResidency/PreparedMipBytesReachTheDevice
  outshine/src/render/stages/GeometryResidency/UnboundMapsShareDeviceResources
  outshine/src/render/terrain/TerrainRenderer/AdaptivePatchesKeepFineNormals
  outshine/src/render/terrain/TerrainRenderer/DrawArgumentsCountAdaptiveInstances
  outshine/src/render/terrain/TerrainRenderer/OuterSkirtsFaceTheGap
  outshine/src/render/terrain/TerrainRenderer/SharedInputsPreserveIndependentGrids
  outshine/src/render/terrain/TerrainRenderer/SurfacePatchesPreserveRasterAndFractions
  outshine/src/render/visibility/DepthPyramidPass/PartialBlocksPreserveUncoveredPixels

)
foreach(source IN LISTS cases)
  file(RELATIVE_PATH name "${PROJECT_SOURCE_DIR}/test" "${source}")
  string(REGEX REPLACE "\\.cpp$" "" name "${name}")
  string(REPLACE "/" "_" target "test_${name}")
  add_executable(${target} EXCLUDE_FROM_ALL "${source}")
  if(name STREQUAL "outshine/src/render/prism/PrismDrawing/ReceiverPlanesPreserveAffineShadowDepth")
    set(receiver_shader "${PROJECT_BINARY_DIR}/test-shaders/prism-receiver.comp.spv")
    file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/test-shaders")
    add_custom_command(OUTPUT "${receiver_shader}"
      COMMAND "${OUTSHINE_GLSLANG}" -V --target-env vulkan1.0
        "-I${PROJECT_SOURCE_DIR}/src" --depfile "${receiver_shader}.d"
        "${PROJECT_SOURCE_DIR}/test/outshine/data/render/prism-receiver.comp"
        -o "${receiver_shader}"
      DEPENDS "${PROJECT_SOURCE_DIR}/test/outshine/data/render/prism-receiver.comp"
      DEPFILE "${receiver_shader}.d" VERBATIM)
    add_custom_target(test_prism_receiver_shader DEPENDS "${receiver_shader}")
    add_dependencies(${target} test_prism_receiver_shader)
    target_compile_definitions(${target} PRIVATE
      "OUTSHINE_PRISM_RECEIVER_SHADER=\"${receiver_shader}\"")
  endif()
  if(name STREQUAL "outshine/src/render/material/MaterialResolution/DenseMaterialBindingsKeepBoundedAllocation"
      OR name STREQUAL "outshine/src/generators/osm/MvtTransportSnapshot/NativeTopologyRetainsItsActualSources"
      OR name STREQUAL "outshine/src/generators/transport/BridgeInputRequirements/FarConditionsRemainSourceBound"
      OR name STREQUAL "outshine/src/generators/transport/TransportCoreSelection/AdmissionAvoidsTemporaryOutput"
      OR name STREQUAL "outshine/src/generators/terrain/inputs/HeightField/LocatedSamplesRetainTheActualBlock"
      OR name STREQUAL "outshine/src/generators/contracts/TransportPlanCodec/DecodingAllocatesOnlyOwnedProductStorage"
      OR name STREQUAL "outshine/src/generators/contracts/TransportPlanCodec/NativeSourceBindingsSurviveOfflineRoundTrip"
      OR name STREQUAL "outshine/src/world/products/navigation/TransportTopology/PreparedValuesRetainTheirOwnedIndex")
    target_sources(${target} PRIVATE test/harness/shared/CppAllocationProbe.cpp)
  endif()
  if(name STREQUAL "outshine/src/app/ProcessBoundary/FatalErrorsAreContained")
    target_compile_options(${target} PRIVATE -fexceptions)
  else()
    target_compile_options(${target} PRIVATE -fno-exceptions)
  endif()
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/test/harness/shared"
    "${PROJECT_SOURCE_DIR}")
  target_compile_definitions(${target} PRIVATE
    "OUTSHINE_TEST_NEST=\"${PROJECT_BINARY_DIR}/test-output\""
    "OUTSHINE_COMPILE=\"${CMAKE_CXX_COMPILER} -std=c++${CMAKE_CXX_STANDARD} -I$<JOIN:$<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>, -I>\"")
  set(owner "")
  if(name MATCHES "^outshine/src/(.+)")
    set(path "${CMAKE_MATCH_1}")
    foreach(root IN LISTS OUTSHINE_COMPONENT_ROOTS)
      if(path MATCHES "^${root}/" AND (owner STREQUAL "" OR root MATCHES "^${owner}/"))
        set(owner "${root}")
      endif()
    endforeach()
  endif()
  if(owner STREQUAL "")
    if(name MATCHES "^outshine/integration/app/")
      set(owner app)
    elseif(name MATCHES "^outshine/integration/render/(TemporalSequence|atmosphere|scene/RenderScene)/")
      set(owner render)
    elseif(name MATCHES "^outshine/integration/generators/osm/(BuildingFootprints|PreparedGroundRegions|BuildingFoundationJob|MvtTransportSnapshot)/")
      set(owner generators/osm)
    elseif(name MATCHES "^outshine/integration/generators/building/StructureBake/")
      set(owner generators/building)
    elseif(name MATCHES "^outshine/integration/generators/water/")
      set(owner generators/water)
    elseif(name MATCHES "^outshine/integration/generators/terrain/providers/")
      set(owner generators/terrain/providers)
    elseif(name MATCHES "^outshine/integration/generators/transport/AppearancePages/")
      set(owner generators/road/pages)
    elseif(name MATCHES "^outshine/integration/generators/transport/PlanRoadPages/")
      set(owner generators/road/meshing)
    elseif(name MATCHES "^outshine/integration/transport/RoadStepBody/")
      set(owner generators/road)
    elseif(name MATCHES "^outshine/integration/generators/terrain/inputs/")
      set(owner generators/terrain/inputs)
    elseif(name MATCHES "^outshine/integration/storage/")
      set(owner content)
    elseif(name MATCHES "^outshine/integration/sources/")
      set(owner sources)
    elseif(name MATCHES "^outshine/integration/content/AssetGeneration/")
      set(owner generators/base)
    else()
      set(owner engine)
    endif()
  endif()
  string(REPLACE "/" "_" component "outshine_${owner}")
  if(name MATCHES "^outshine/src/generators/weather/(LowCloud|inputs/WeatherSnapshotLoader)/")
    target_link_libraries(${target} PRIVATE outshine_generators_weather_providers)
  endif()
  if(name MATCHES "^outshine/src/render/frame/FrameTargets/")
    target_link_libraries(${target} PRIVATE outshine_render_device_native)
  endif()
  if(name IN_LIST capture_cases)
    target_link_libraries(${target} PRIVATE outshine_app_capture)
  endif()
  if(name MATCHES "^outshine/integration/storage/")
    target_link_libraries(${target} PRIVATE outshine_sources)
  endif()
  if(name STREQUAL "outshine/integration/sources/HttpTransport/ByteRangesKeepObjectIdentity")
    target_link_libraries(${target} PRIVATE outshine_generators_terrain_inputs)
  endif()
  if(name MATCHES "/TerrariumRaster/" OR
      name STREQUAL "outshine/src/generators/terrain/inputs/TerrainGrid/TerrariumPreservesCoastalRelief" OR
      name STREQUAL "outshine/integration/generators/terrain/TerrariumSource/CachedWebpBuildsNativeTerrain")
    target_link_libraries(${target} PRIVATE PkgConfig::libwebp)
  endif()
  if(name MATCHES "/CopernicusRaster/" OR
      name MATCHES "/TilePool/(NativeCellsSupplyMercatorTerrain|NativeRangeReceiptsSurviveResidency|LargeOriginalBlocksRemainComplete)$" OR
      name STREQUAL "outshine/integration/sources/HttpTransport/ByteRangesKeepObjectIdentity")
    target_link_libraries(${target} PRIVATE PkgConfig::libtiff_4)
  endif()
  if(name STREQUAL "outshine/integration/generators/terrain/inputs/TilePool/NativeCellsSupplyMercatorTerrain")
    target_link_libraries(${target} PRIVATE outshine_generators_base)
  endif()
  if(name MATCHES "^outshine/integration/generators/osm/MvtTransportSnapshot/")
    target_link_libraries(${target} PRIVATE outshine_generators_transport)
  endif()
  if(name STREQUAL "outshine/integration/generators/water/WaterHeightFit/RoadAndWaterSolversKeepIndependentMatrixStorage"
      OR name STREQUAL "outshine/integration/generators/osm/MvtTransportSnapshot/NativeSourcesReachCoreSelection")
    target_link_libraries(${target} PRIVATE outshine_generators_road)
  endif()
  target_link_libraries(${target} PRIVATE ${component})
  if(name MATCHES "^outshine/integration/generators/transport/PlanRoadPages/")
    target_link_libraries(${target} PRIVATE outshine_generators_transport)
  endif()
  if(name MATCHES "^outshine/integration/places")
    add_dependencies(${target} outshine)
  endif()
  add_test(NAME "${name}" COMMAND ${target})
  set_tests_properties("${name}" PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    TIMEOUT "${OUTSHINE_TEST_TIMEOUT}" LABELS "${owner}"
    ENVIRONMENT "OUTSHINE_TIMEOUT_S=${OUTSHINE_TEST_TIMEOUT};OUTSHINE_CLIENT=$<TARGET_FILE:outshine>"
    FAIL_REGULAR_EXPRESSION "SKIPPED [1-9];UNPREPARED [1-9];PARTIAL [1-9]")
  if(APPLE AND name IN_LIST metal_validation_cases)
    set_property(TEST "${name}" APPEND PROPERTY ENVIRONMENT "MTL_DEBUG_LAYER=1")
  endif()
  add_dependencies(outshine_tests ${target})
  foreach(scope IN LISTS gate_scopes)
    if(name STREQUAL scope OR name MATCHES "^${scope}/")
      set_property(TEST "${name}" APPEND PROPERTY LABELS world-gate)
      add_dependencies(outshine_gate ${target})
      break()
    endif()
  endforeach()
  set(suite "${name}")
  while(NOT suite STREQUAL "")
    string(REPLACE "/" "_" suite_target "test_${suite}")
    if(NOT suite_target STREQUAL target)
      if(NOT TARGET ${suite_target})
        add_custom_target(${suite_target})
      endif()
      add_dependencies(${suite_target} ${target})
    endif()
    get_filename_component(parent "${suite}" DIRECTORY)
    if(parent STREQUAL suite)
      break()
    endif()
    set(suite "${parent}")
  endwhile()
endforeach()

add_executable(test_harness_material_bindings EXCLUDE_FROM_ALL
  test/experiments/render/material_binding_benchmark.cpp
  test/harness/shared/CppAllocationProbe.cpp)
target_include_directories(test_harness_material_bindings PRIVATE
  "${PROJECT_SOURCE_DIR}/test/harness/shared")
target_compile_options(test_harness_material_bindings PRIVATE -fno-exceptions)
target_link_libraries(test_harness_material_bindings PRIVATE outshine_render)

add_executable(test_harness_weather_forecast EXCLUDE_FROM_ALL
  test/experiments/weather/native_forecast.cpp)
target_link_libraries(test_harness_weather_forecast PRIVATE outshine_generators_weather_providers)
add_executable(test_harness_weather_snapshot EXCLUDE_FROM_ALL
  test/experiments/weather/weather_snapshot.cpp)
target_link_libraries(test_harness_weather_snapshot PRIVATE outshine_generators_weather
  outshine_generators_weather_providers)
foreach(target IN ITEMS test_harness_weather_forecast test_harness_weather_snapshot)
  target_compile_options(${target} PRIVATE -fno-exceptions)
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/test/harness/shared")
endforeach()

add_executable(test_harness_transport_source_export EXCLUDE_FROM_ALL
  test/experiments/transport/native_transport_export.cpp
  test/experiments/transport/native_transport_snapshot_json.cpp
  test/experiments/transport/native_transport_facts_json.cpp)
target_compile_options(test_harness_transport_source_export PRIVATE -fno-exceptions)
target_link_libraries(test_harness_transport_source_export PRIVATE outshine_generators_osm)
add_dependencies(outshine_tests test_harness_transport_source_export)
add_test(NAME experiments/transport/NativeSourceFactsExport
  COMMAND "${Python3_EXECUTABLE}" -B
  "${PROJECT_SOURCE_DIR}/test/experiments/transport/test_native_transport_export.py"
  --exporter $<TARGET_FILE:test_harness_transport_source_export>)
set_tests_properties(experiments/transport/NativeSourceFactsExport PROPERTIES
  TIMEOUT "${OUTSHINE_TEST_TIMEOUT}" LABELS "experiments/transport;world-gate")

add_executable(test_harness_building_selection_export EXCLUDE_FROM_ALL
  test/experiments/render/export_building_selection.cpp
  test/experiments/render/building_capture_inputs.cpp)
target_compile_options(test_harness_building_selection_export PRIVATE -fno-exceptions)
target_link_libraries(test_harness_building_selection_export PRIVATE outshine_generators_osm)
add_dependencies(outshine_tests test_harness_building_selection_export)
add_custom_target(test_experiments_render_BuildingCellControl
  DEPENDS test_harness_building_selection_export)
add_test(NAME experiments/render/BuildingCellControl
  COMMAND "${Python3_EXECUTABLE}" -B
  "${PROJECT_SOURCE_DIR}/test/experiments/render/test_building_cell_control.py")
set_tests_properties(experiments/render/BuildingCellControl PROPERTIES
  TIMEOUT "${OUTSHINE_TEST_TIMEOUT}" LABELS "experiments/render")
add_test(NAME experiments/render/NativeBuildingCellExport
  COMMAND "${Python3_EXECUTABLE}" -B
  "${PROJECT_SOURCE_DIR}/test/experiments/render/test_native_building_cell_export.py"
  --exporter $<TARGET_FILE:test_harness_building_selection_export>)
set_tests_properties(experiments/render/NativeBuildingCellExport PROPERTIES
  TIMEOUT "${OUTSHINE_TEST_TIMEOUT}" LABELS "experiments/render")

execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import tempfile; print(tempfile.gettempdir())"
  OUTPUT_VARIABLE system_temp OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
set(OUTSHINE_PREPARED_DIR "${system_temp}/outshine-prepared" CACHE PATH "Prepared pinned corpus inputs")
function(outshine_corpus family component scorer)
  string(REPLACE "/" "_" target "test_harness_${family}")
  add_executable(${target} EXCLUDE_FROM_ALL
    "${PROJECT_SOURCE_DIR}/test/harness/${family}/${scorer}.cpp")
  target_compile_options(${target} PRIVATE -fno-exceptions)
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/test/harness/shared")
  target_link_libraries(${target} PRIVATE ${component})
  add_dependencies(outshine_tests ${target})
  string(REPLACE "/" "_" suite "test_${family}")
  add_custom_target(${suite})
  add_dependencies(${suite} ${target})
  file(GLOB_RECURSE manifests CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/test/${family}/manifest.json")
  foreach(manifest IN LISTS manifests)
    get_filename_component(directory "${manifest}" DIRECTORY)
    file(RELATIVE_PATH name "${PROJECT_SOURCE_DIR}/test" "${directory}")
    string(REPLACE "/" "-" prepared "test/${name}")
    add_test(NAME "${name}" COMMAND ${target} "${OUTSHINE_PREPARED_DIR}/${prepared}")
    set_tests_properties("${name}" PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      TIMEOUT "${OUTSHINE_TEST_TIMEOUT}" LABELS "corpus;${family}"
      FAIL_REGULAR_EXPRESSION "SKIPPED [1-9];UNPREPARED [1-9];PARTIAL [1-9]")
  endforeach()
endfunction()
outshine_corpus(wpt/css outshine_ui ScoreEveryWptCase)
outshine_corpus(test262/js outshine_scripting ScoreEveryScriptCase)
target_link_libraries(test_harness_test262_js PRIVATE outshine_base)
outshine_corpus(geographiclib/geodesic outshine_world ScoreEveryGeodesic)

set_tests_properties(outshine/src/render/stages/StagePipelines/NativePipelinesKeepDeclaredBindings
  PROPERTIES WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
add_test(NAME render/shader-artifacts COMMAND Python3::Interpreter
  "${PROJECT_SOURCE_DIR}/test/scripts/shader_artifacts.py"
  --manifest "${PROJECT_BINARY_DIR}/shader-artifacts.txt"
  --compute-contracts "${PROJECT_BINARY_DIR}/compute-shaders.json")
set_tests_properties(render/shader-artifacts PROPERTIES LABELS "render;world-gate"
  TIMEOUT "${OUTSHINE_TEST_TIMEOUT}")
add_custom_target(test_render_shader_artifacts)
add_dependencies(test_render_shader_artifacts outshine_shaders)

find_program(OUTSHINE_CLANG_FORMAT NAMES clang-format clang-format-22 clang-format-21
  HINTS /opt/homebrew/opt/llvm/bin)
file(GLOB script_tests CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/test/scripts/test_*.py")
add_custom_target(test_tools)
foreach(script IN LISTS script_tests)
  get_filename_component(name "${script}" NAME_WE)
  string(REGEX REPLACE "^test_" "" name "${name}")
  set(target "test_tools_${name}")
  add_custom_target(${target})
  if(name MATCHES "^(cache_command|client_arguments|declared_place_assets|motion_profiles|place_catalog|place_profiles|scenario_benchmark|still_capture)$")
    add_dependencies(${target} outshine)
  endif()
  add_dependencies(test_tools ${target})
  add_dependencies(outshine_tests ${target})
  add_test(NAME "tools/${name}" COMMAND Python3::Interpreter -B "${script}")
  set_tests_properties("tools/${name}" PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    LABELS tools TIMEOUT "${OUTSHINE_TEST_TIMEOUT}"
    ENVIRONMENT "OUTSHINE_BUILD_DIR=${PROJECT_BINARY_DIR};OUTSHINE_CLIENT=$<TARGET_FILE:outshine>;CXX=${CMAKE_CXX_COMPILER};GLSLANG=${OUTSHINE_GLSLANG};CLANG_FORMAT=${OUTSHINE_CLANG_FORMAT}")
endforeach()

add_test(NAME tools/prepare_place_profiles COMMAND Python3::Interpreter -B
  "${PROJECT_SOURCE_DIR}/test/scripts/test_place_profiles.py" --prepare)
get_test_property(tools/place_profiles ENVIRONMENT profile_environment)
set_tests_properties(tools/prepare_place_profiles PROPERTIES
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" TIMEOUT 650 LABELS preparation
  ENVIRONMENT "${profile_environment}" FIXTURES_SETUP place_profiles)
set_tests_properties(tools/place_profiles PROPERTIES FIXTURES_REQUIRED place_profiles)

add_executable(outshine_ggx_fragment_probe EXCLUDE_FROM_ALL
  "${PROJECT_SOURCE_DIR}/test/harness/render/NativeGgxFragmentProbe.cpp")
target_include_directories(outshine_ggx_fragment_probe PRIVATE
  "${PROJECT_SOURCE_DIR}/test/harness/shared" "${PROJECT_SOURCE_DIR}/test/harness/render")
target_compile_options(outshine_ggx_fragment_probe PRIVATE -fno-exceptions)
target_link_libraries(outshine_ggx_fragment_probe PRIVATE outshine_render)
