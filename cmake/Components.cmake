set(OUTSHINE_COMPONENT_ROOTS
  actor
  audio
  base
  client
  content
  diagnostics
  diagnostics/calibration
  engine
  engine/streaming
  generators
  generators/base
  generators/building
  generators/flora
  generators/osm
  generators/road
  generators/terrain
  generators/water
  host
  render
  scenario
  ui
  world
  world/entity
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
  add_library(${name} OBJECT ${files})
  target_compile_options(${name} PRIVATE -fno-exceptions)
  target_include_directories(${name} PUBLIC ${includes})
  target_link_libraries(${name} PUBLIC outshine_compiler)
  set(OUTSHINE_COMPONENTS ${OUTSHINE_COMPONENTS} ${name} PARENT_SCOPE)
endfunction()
outshine_component(outshine_actor actor)
outshine_component(outshine_audio audio)
outshine_component(outshine_base base)
outshine_component(outshine_client client)
outshine_component(outshine_content content)
outshine_component(outshine_diagnostics diagnostics)
outshine_component(outshine_diagnostics_calibration diagnostics/calibration)
outshine_component(outshine_engine engine)
outshine_component(outshine_engine_streaming engine/streaming)
outshine_component(outshine_generators generators)
outshine_component(outshine_generators_base generators/base)
outshine_component(outshine_generators_building generators/building)
outshine_component(outshine_generators_flora generators/flora)
outshine_component(outshine_generators_osm generators/osm)
outshine_component(outshine_generators_road generators/road)
outshine_component(outshine_generators_terrain generators/terrain)
outshine_component(outshine_generators_water generators/water)
outshine_component(outshine_host host)
outshine_component(outshine_render render)
outshine_component(outshine_scenario scenario)
outshine_component(outshine_ui ui)
outshine_component(outshine_world world)
outshine_component(outshine_world_entity world/entity)
target_link_libraries(outshine_actor PUBLIC outshine_base)
target_link_libraries(outshine_audio PUBLIC outshine_base)
target_link_libraries(outshine_client PUBLIC outshine_actor outshine_audio outshine_base outshine_content outshine_diagnostics outshine_diagnostics_calibration outshine_engine outshine_generators outshine_host outshine_render outshine_scenario outshine_world)
target_link_libraries(outshine_content PUBLIC outshine_base)
target_link_libraries(outshine_diagnostics PUBLIC outshine_base)
target_link_libraries(outshine_diagnostics_calibration PUBLIC outshine_base outshine_render)
target_link_libraries(outshine_engine PUBLIC outshine_base outshine_content outshine_world outshine_generators outshine_actor outshine_render outshine_scenario outshine_ui outshine_audio outshine_diagnostics outshine_host outshine_engine_streaming)
target_link_libraries(outshine_engine_streaming PUBLIC outshine_base outshine_content outshine_world outshine_generators outshine_render)
target_link_libraries(outshine_generators PUBLIC outshine_base outshine_content outshine_diagnostics outshine_generators_osm outshine_world outshine_generators_road outshine_generators_terrain outshine_generators_water outshine_generators_flora)
target_link_libraries(outshine_generators_base PUBLIC outshine_base outshine_content outshine_world)
target_link_libraries(outshine_generators_building PUBLIC outshine_generators_base outshine_base outshine_content outshine_world)
target_link_libraries(outshine_generators_flora PUBLIC outshine_generators_base outshine_base outshine_content outshine_world)
target_link_libraries(outshine_generators_osm PUBLIC outshine_base outshine_content outshine_generators_base outshine_generators_building outshine_world)
target_link_libraries(outshine_generators_road PUBLIC outshine_base outshine_content outshine_diagnostics outshine_generators_base outshine_generators_osm outshine_world)
target_link_libraries(outshine_generators_terrain PUBLIC outshine_base outshine_content outshine_generators_base outshine_generators_osm outshine_world)
target_link_libraries(outshine_generators_water PUBLIC outshine_base outshine_content outshine_generators_base outshine_world)
target_link_libraries(outshine_host PUBLIC outshine_base outshine_world)
target_link_libraries(outshine_render PUBLIC outshine_base outshine_content outshine_world)
target_link_libraries(outshine_scenario PUBLIC outshine_actor outshine_audio outshine_base outshine_content outshine_render outshine_world)
target_link_libraries(outshine_ui PUBLIC outshine_base outshine_content)
target_link_libraries(outshine_world PUBLIC outshine_base outshine_content outshine_world_entity)
target_link_libraries(outshine_world_entity PUBLIC outshine_base)

target_link_libraries(outshine_base PUBLIC PkgConfig::geos ZLIB::ZLIB Threads::Threads)
target_link_libraries(outshine_content PUBLIC PkgConfig::sdl3_image PkgConfig::sdl3_ttf
  PkgConfig::sqlite3 PkgConfig::libzstd)
target_link_libraries(outshine_world PUBLIC PkgConfig::libtiff_4 PkgConfig::libwebp CURL::libcurl)
target_link_libraries(outshine_render PUBLIC PkgConfig::sdl3 PkgConfig::sdl3_image
  PkgConfig::spirv_cross ${SPIRV_CROSS_LIBRARIES})
if(APPLE)
  target_link_libraries(outshine_render PUBLIC "-framework Foundation" "-framework Metal"
    "-framework QuartzCore" "-framework CoreGraphics")
else()
  target_link_libraries(outshine_base PUBLIC OpenSSL::Crypto)
  target_link_libraries(outshine_render PUBLIC Vulkan::Vulkan)
endif()
target_link_libraries(outshine_generators_water PUBLIC osqp::osqp)
target_link_libraries(outshine_generators_road PUBLIC highs::highs)
target_link_libraries(outshine_generators_terrain PUBLIC PkgConfig::libwebp)
target_link_libraries(outshine_engine PUBLIC PkgConfig::sdl3_image)
target_link_libraries(outshine_client PUBLIC PkgConfig::nlohmann_json)
