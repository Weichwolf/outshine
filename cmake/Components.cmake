set(OUTSHINE_COMPONENT_ROOTS
  app
  app/capture
  app/scenario
  audio
  base
  content
  content/assets
  content/shade
  diagnostics
  diagnostics/calibration
  engine
  engine/scenario
  engine/streaming
  engine/streaming/building
  generators
  generators/base
  generators/building
  generators/contracts
  generators/earth
  generators/flora
  generators/osm
  generators/road
  generators/road/meshing
  generators/road/pages
  generators/recipes
  generators/sky
  generators/terrain
  generators/terrain/inputs
  generators/terrain/providers
  generators/transport
  generators/water
  generators/weather
  generators/weather/inputs
  generators/weather/providers
  render
  render/device
  render/device/native
  render/frame
  render/pipelines
  render/plan
  render/temporal
  render/view
  scenario
  scripting
  simulation
  sources
  storage
  ui
  world
  world/contracts
  world/entity
  world/products
)
function(outshine_component name directory)
  file(GLOB_RECURSE files CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/${directory}/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/${directory}/*.mm"
    "${PROJECT_SOURCE_DIR}/src/${directory}/*.h"
    "${PROJECT_SOURCE_DIR}/src/${directory}/*.hpp")
  foreach(child IN LISTS OUTSHINE_COMPONENT_ROOTS)
    if(child MATCHES "^${directory}/")
      list(FILTER files EXCLUDE REGEX "/src/${child}/")
    endif()
  endforeach()
  list(FILTER files EXCLUDE REGEX "/Main\\.cpp$")
  if(APPLE)
    list(FILTER files EXCLUDE REGEX "/native/vulkan/")
  else()
    list(FILTER files EXCLUDE REGEX "\\.mm$|/native/metal/")
  endif()
  set(includes "${PROJECT_SOURCE_DIR}/src/${directory}")
  foreach(file IN LISTS files)
    get_filename_component(parent "${file}" DIRECTORY)
    list(APPEND includes "${parent}")
  endforeach()
  list(REMOVE_DUPLICATES includes)
  add_library(${name}_objects OBJECT ${files})
  target_compile_options(${name}_objects PRIVATE -fno-exceptions)
  target_include_directories(${name}_objects PUBLIC ${includes})
  target_link_libraries(${name}_objects PUBLIC outshine_compiler)
  add_library(${name} STATIC $<TARGET_OBJECTS:${name}_objects>)
  target_link_libraries(${name} INTERFACE ${name}_objects)
  set(OUTSHINE_COMPONENTS ${OUTSHINE_COMPONENTS} ${name} PARENT_SCOPE)
endfunction()
add_library(outshine_ui_paint INTERFACE)
target_include_directories(outshine_ui_paint INTERFACE "${PROJECT_SOURCE_DIR}/src/ui/paint")
add_library(outshine_render_device_api INTERFACE)
target_include_directories(outshine_render_device_api INTERFACE
  "${PROJECT_SOURCE_DIR}/src/render/device/native")
add_library(outshine_world_contracts INTERFACE)
target_include_directories(outshine_world_contracts INTERFACE
  "${PROJECT_SOURCE_DIR}/src/world/contracts")

foreach(root IN LISTS OUTSHINE_COMPONENT_ROOTS)
  if(root STREQUAL "world/contracts")
    continue()
  endif()
  string(REPLACE "/" "_" name "outshine_${root}")
  outshine_component(${name} "${root}")
endforeach()
set_property(TARGET outshine_render_device_native_objects PROPERTY INTERFACE_INCLUDE_DIRECTORIES "")
target_link_libraries(outshine_app_scenario PUBLIC outshine_base outshine_scenario)
target_link_libraries(outshine_app PUBLIC outshine_app_scenario outshine_app_capture outshine_simulation outshine_audio outshine_base outshine_content outshine_diagnostics outshine_diagnostics_calibration outshine_engine outshine_generators outshine_sources outshine_render outshine_scenario outshine_world)
target_link_libraries(outshine_simulation PUBLIC outshine_base outshine_world_entity)
target_link_libraries(outshine_audio PUBLIC outshine_base)
target_link_libraries(outshine_app_capture PUBLIC outshine_app_scenario outshine_base outshine_render outshine_engine
  PRIVATE PkgConfig::sdl3_image)
target_link_libraries(outshine_content PUBLIC outshine_base outshine_content_assets outshine_content_shade outshine_world_products)
target_link_libraries(outshine_content_assets PUBLIC outshine_base outshine_storage
  PRIVATE PkgConfig::libzstd ZLIB::ZLIB)
target_link_libraries(outshine_content_shade PUBLIC outshine_base PRIVATE PkgConfig::sdl3)
target_link_libraries(outshine_diagnostics PUBLIC outshine_base)
target_link_libraries(outshine_diagnostics_calibration PUBLIC outshine_base outshine_render_pipelines
  PRIVATE PkgConfig::sdl3)
target_link_libraries(outshine_engine_scenario PUBLIC outshine_base outshine_scenario outshine_world_entity)
target_link_libraries(outshine_engine PUBLIC outshine_engine_scenario outshine_base outshine_content outshine_world outshine_generators outshine_simulation outshine_render outshine_scenario outshine_scripting outshine_ui outshine_audio outshine_diagnostics outshine_sources outshine_engine_streaming)
target_link_libraries(outshine_engine_streaming PUBLIC outshine_base outshine_content outshine_world outshine_generators outshine_render outshine_sources outshine_engine_streaming_building)
target_link_libraries(outshine_engine_streaming_building PUBLIC outshine_base outshine_generators_building)
target_link_libraries(outshine_generators PUBLIC outshine_base outshine_content outshine_diagnostics outshine_generators_osm outshine_generators_earth outshine_generators_sky outshine_world outshine_generators_road outshine_generators_recipes outshine_generators_terrain outshine_generators_water outshine_generators_flora outshine_generators_weather outshine_generators_weather_inputs outshine_generators_weather_providers outshine_sources)
target_link_libraries(outshine_generators_base PUBLIC outshine_base outshine_content outshine_world outshine_sources)
target_link_libraries(outshine_generators_contracts PUBLIC outshine_base outshine_world_contracts outshine_world_products)
target_link_libraries(outshine_generators_transport PUBLIC outshine_generators_contracts outshine_content_assets)
target_link_libraries(outshine_world_contracts INTERFACE outshine_base)
target_link_libraries(outshine_generators_building PUBLIC outshine_generators_terrain_inputs outshine_generators_base outshine_base outshine_content outshine_world)
target_link_libraries(outshine_generators_earth PUBLIC outshine_base outshine_world outshine_generators_terrain outshine_generators_osm)
target_link_libraries(outshine_generators_flora PUBLIC outshine_generators_base outshine_generators_recipes outshine_base outshine_content outshine_world)
target_link_libraries(outshine_generators_osm PUBLIC outshine_base outshine_content outshine_generators_base outshine_generators_recipes outshine_generators_building outshine_world outshine_sources)
target_link_libraries(outshine_generators_road PUBLIC outshine_generators_road_pages outshine_generators_road_meshing outshine_generators_contracts outshine_base outshine_content outshine_diagnostics outshine_generators_base outshine_generators_recipes outshine_generators_osm outshine_world)
target_link_libraries(outshine_generators_road_pages PUBLIC outshine_generators_road_meshing outshine_content_assets outshine_world_products)
target_link_libraries(outshine_generators_road_meshing PUBLIC outshine_base outshine_generators_contracts)
target_link_libraries(outshine_generators_recipes PUBLIC outshine_base outshine_world)
target_link_libraries(outshine_generators_terrain PUBLIC outshine_generators_terrain_inputs outshine_base outshine_content outshine_generators_base outshine_world outshine_generators_terrain_providers)
target_link_libraries(outshine_generators_terrain_inputs PUBLIC outshine_base outshine_sources outshine_world outshine_generators_terrain_providers)
target_link_libraries(outshine_generators_terrain_providers PUBLIC outshine_base outshine_sources outshine_world_products PRIVATE PkgConfig::libwebp PkgConfig::libtiff_4)
target_link_libraries(outshine_generators_sky PUBLIC outshine_base outshine_sources)
target_link_libraries(outshine_generators_water PUBLIC outshine_base outshine_content outshine_generators_base outshine_world)
target_link_libraries(outshine_generators_weather PUBLIC outshine_base outshine_sources outshine_world)
target_link_libraries(outshine_generators_weather_inputs PUBLIC outshine_base outshine_sources)
target_link_libraries(outshine_generators_weather_providers PUBLIC outshine_base outshine_sources)
target_link_libraries(outshine_scripting PRIVATE qjs Threads::Threads)
target_link_libraries(outshine_sources PUBLIC outshine_base outshine_storage CURL::libcurl)
target_link_libraries(outshine_storage PUBLIC outshine_base PkgConfig::sqlite3)
target_link_libraries(outshine_render PUBLIC outshine_base outshine_content_shade outshine_world outshine_ui_paint outshine_render_device outshine_render_frame outshine_render_pipelines outshine_render_plan outshine_render_view)
target_link_libraries(outshine_render_device PUBLIC outshine_base outshine_render_device_native)
target_link_libraries(outshine_render_device_native PUBLIC outshine_render_device_api
  PRIVATE Threads::Threads PkgConfig::sdl3 PkgConfig::spirv_cross ${SPIRV_CROSS_LIBRARIES})
target_link_libraries(outshine_render_pipelines PUBLIC outshine_base outshine_render_device)
target_link_libraries(outshine_render_plan PUBLIC outshine_base outshine_render_device_api)
target_link_libraries(outshine_render_frame PUBLIC outshine_render_plan)
target_link_libraries(outshine_render_temporal PUBLIC outshine_base)
target_link_libraries(outshine_render_view PUBLIC outshine_base outshine_render_temporal)
target_link_libraries(outshine_scenario PUBLIC outshine_base outshine_sources outshine_world_entity)
target_link_libraries(outshine_ui PUBLIC outshine_base outshine_ui_paint PRIVATE PkgConfig::sdl3_ttf)
target_link_libraries(outshine_world PUBLIC outshine_world_contracts outshine_base outshine_world_products outshine_world_entity)
target_link_libraries(outshine_world_entity PUBLIC outshine_base)
target_link_libraries(outshine_world_products PUBLIC outshine_base)

target_link_libraries(outshine_base PUBLIC PkgConfig::geos ZLIB::ZLIB Threads::Threads)
target_link_libraries(outshine_render PUBLIC PkgConfig::sdl3)
if(APPLE)
  target_link_libraries(outshine_render_device_native PRIVATE "-framework Foundation" "-framework Metal"
    "-framework QuartzCore" "-framework CoreGraphics")
else()
  target_link_libraries(outshine_base PUBLIC OpenSSL::Crypto)
  target_link_libraries(outshine_render_device_native PRIVATE Vulkan::Vulkan)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(outshine_generators_water PUBLIC osqp::osqpstatic)
  target_link_options(outshine_generators_water INTERFACE
    "LINKER:--exclude-libs,$<TARGET_FILE_NAME:osqp::osqpstatic>")
else()
  target_link_libraries(outshine_generators_water PUBLIC osqp::osqp)
endif()
target_link_libraries(outshine_generators_road PUBLIC highs::highs)
target_link_libraries(outshine_engine PUBLIC PkgConfig::sdl3_image)
target_link_libraries(outshine_app PUBLIC PkgConfig::nlohmann_json)

foreach(component IN LISTS OUTSHINE_COMPONENTS)
  get_target_property(dependencies ${component} LINK_LIBRARIES)
  if(dependencies)
    target_link_libraries(${component}_objects PRIVATE ${dependencies})
  endif()
endforeach()
