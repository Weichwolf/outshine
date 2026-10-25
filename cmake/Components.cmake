set(OUTSHINE_COMPONENT_ROOTS
  app/capture
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
  generators/contracts
  generators/earth
  generators/flora
  generators/osm
  generators/road
  generators/terrain
  generators/water
  render
  scenario
  sources
  storage
  ui
  world
  world/contracts
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
  add_library(${name}_objects OBJECT ${files})
  target_compile_options(${name}_objects PRIVATE -fno-exceptions)
  target_include_directories(${name}_objects PUBLIC ${includes})
  target_link_libraries(${name}_objects PUBLIC outshine_compiler)
  add_library(${name} STATIC $<TARGET_OBJECTS:${name}_objects>)
  target_link_libraries(${name} INTERFACE ${name}_objects)
  set(OUTSHINE_COMPONENTS ${OUTSHINE_COMPONENTS} ${name} PARENT_SCOPE)
endfunction()
foreach(root IN LISTS OUTSHINE_COMPONENT_ROOTS)
  string(REPLACE "/" "_" name "outshine_${root}")
  outshine_component(${name} "${root}")
endforeach()
target_link_libraries(outshine_actor PUBLIC outshine_base)
target_link_libraries(outshine_audio PUBLIC outshine_base)
target_link_libraries(outshine_app_capture PUBLIC outshine_base outshine_content outshine_render outshine_engine)
target_link_libraries(outshine_client PUBLIC outshine_app_capture outshine_actor outshine_audio outshine_base outshine_content outshine_diagnostics outshine_diagnostics_calibration outshine_engine outshine_generators outshine_sources outshine_render outshine_scenario outshine_world)
target_link_libraries(outshine_content PUBLIC outshine_base outshine_storage)
target_link_libraries(outshine_diagnostics PUBLIC outshine_base)
target_link_libraries(outshine_diagnostics_calibration PUBLIC outshine_base outshine_render)
target_link_libraries(outshine_engine PUBLIC outshine_base outshine_content outshine_world outshine_generators outshine_actor outshine_render outshine_scenario outshine_ui outshine_audio outshine_diagnostics outshine_sources outshine_engine_streaming)
target_link_libraries(outshine_engine_streaming PUBLIC outshine_base outshine_content outshine_world outshine_generators outshine_render outshine_sources)
target_link_libraries(outshine_generators PUBLIC outshine_base outshine_content outshine_diagnostics outshine_generators_osm outshine_generators_earth outshine_world outshine_generators_road outshine_generators_terrain outshine_generators_water outshine_generators_flora outshine_sources)
target_link_libraries(outshine_generators_base PUBLIC outshine_base outshine_content outshine_world)
target_link_libraries(outshine_generators_contracts PUBLIC outshine_base outshine_world_contracts)
target_link_libraries(outshine_world_contracts PUBLIC outshine_base)
target_sources(outshine_world_contracts PRIVATE src/world/contracts/EarthworkStamp.h)
set_target_properties(outshine_world_contracts outshine_world_contracts_objects
  PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(outshine_generators_building PUBLIC outshine_generators_base outshine_base outshine_content outshine_world)
target_link_libraries(outshine_generators_earth PUBLIC outshine_base outshine_world outshine_generators_terrain outshine_generators_osm)
target_link_libraries(outshine_generators_flora PUBLIC outshine_generators_base outshine_base outshine_content outshine_world)
target_link_libraries(outshine_generators_osm PUBLIC outshine_base outshine_content outshine_generators_base outshine_generators_building outshine_world outshine_sources)
target_link_libraries(outshine_generators_road PUBLIC outshine_generators_contracts outshine_base outshine_content outshine_diagnostics outshine_generators_base outshine_generators_osm outshine_world)
target_link_libraries(outshine_generators_terrain PUBLIC outshine_base outshine_content outshine_generators_base outshine_generators_osm outshine_world)
target_link_libraries(outshine_generators_water PUBLIC outshine_base outshine_content outshine_generators_base outshine_world)
target_link_libraries(outshine_sources PUBLIC outshine_base outshine_storage CURL::libcurl)
target_link_libraries(outshine_storage PUBLIC outshine_base PkgConfig::sqlite3)
target_link_libraries(outshine_render PUBLIC outshine_base outshine_content outshine_world)
target_link_libraries(outshine_scenario PUBLIC outshine_actor outshine_audio outshine_base outshine_content outshine_render outshine_world outshine_sources)
target_link_libraries(outshine_ui PUBLIC outshine_base outshine_content)
target_link_libraries(outshine_world PUBLIC outshine_world_contracts outshine_base outshine_content outshine_world_entity outshine_sources)
target_link_libraries(outshine_world_entity PUBLIC outshine_base)

target_link_libraries(outshine_base PUBLIC PkgConfig::geos ZLIB::ZLIB Threads::Threads)
target_link_libraries(outshine_content PUBLIC PkgConfig::sdl3_image PkgConfig::sdl3_ttf
  PkgConfig::sqlite3 PkgConfig::libzstd)
target_link_libraries(outshine_world PUBLIC PkgConfig::libtiff_4 PkgConfig::libwebp)
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

foreach(component IN LISTS OUTSHINE_COMPONENTS)
  get_target_property(dependencies ${component} LINK_LIBRARIES)
  if(dependencies)
    target_link_libraries(${component}_objects PRIVATE ${dependencies})
  endif()
endforeach()
