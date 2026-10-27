find_program(OUTSHINE_GLSLANG NAMES glslang glslangValidator
  HINTS "${PROJECT_SOURCE_DIR}/build/deps/install/bin" REQUIRED)
set(shader_directory "${PROJECT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${shader_directory}")
set_property(SOURCE "${PROJECT_SOURCE_DIR}/src/render/device/SpirvFile.cpp" APPEND PROPERTY
  COMPILE_DEFINITIONS "OUTSHINE_SHADER_DIRECTORY=\"${shader_directory}\"")
add_executable(shader-tables EXCLUDE_FROM_ALL "${PROJECT_SOURCE_DIR}/test/scripts/shader-tables.cpp")
target_link_libraries(shader-tables PRIVATE outshine_compiler PkgConfig::sdl3)
target_include_directories(shader-tables PRIVATE "${PROJECT_SOURCE_DIR}/src/base/scene"
  "${PROJECT_SOURCE_DIR}/src/base/math" "${PROJECT_SOURCE_DIR}/src/base/geo"
  "${PROJECT_SOURCE_DIR}/src/base" "${PROJECT_SOURCE_DIR}/src/render/stages")
add_custom_command(OUTPUT "${shader_directory}/brdfTables.glsl"
  COMMAND shader-tables > "${shader_directory}/brdfTables.glsl"
  DEPENDS shader-tables VERBATIM)
function(outshine_shader name source stage)
  set(output "${shader_directory}/${name}.spv")
  add_custom_command(OUTPUT "${output}"
    COMMAND "${OUTSHINE_GLSLANG}" -V --target-env vulkan1.0
      "-I${PROJECT_SOURCE_DIR}/src" "-I${PROJECT_SOURCE_DIR}/src/content/shade"
      "-I${shader_directory}" -S "${stage}" ${ARGN}
      --depfile "${output}.d" "${PROJECT_SOURCE_DIR}/src/render/shaders/${source}" -o "${output}"
    DEPENDS "${PROJECT_SOURCE_DIR}/src/render/shaders/${source}"
      "${shader_directory}/brdfTables.glsl"
    DEPFILE "${output}.d" WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" VERBATIM)
  set(OUTSHINE_SHADER_ARTIFACTS ${OUTSHINE_SHADER_ARTIFACTS} "${output}" PARENT_SCOPE)
endfunction()
file(GLOB shaders CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/render/shaders/*.comp"
  "${PROJECT_SOURCE_DIR}/src/render/shaders/*.vert" "${PROJECT_SOURCE_DIR}/src/render/shaders/*.frag")
foreach(source IN LISTS shaders)
  get_filename_component(name "${source}" NAME)
  get_filename_component(extension "${source}" LAST_EXT)
  string(SUBSTRING "${extension}" 1 -1 stage)
  outshine_shader("${name}" "${name}" "${stage}")
endforeach()
set(outputs)
foreach(v RANGE 0 1)
  outshine_shader("groundLattice-${v}.vert" groundLatticeVertex.glsl vert -DSURFACE_WRITES_VELOCITY=${v})
  outshine_shader("groundPatchLattice-${v}.vert" groundLatticeVertex.glsl vert -DGROUND_PATCHES=1 -DSURFACE_WRITES_VELOCITY=${v})
  foreach(n RANGE 0 3)
    foreach(i RANGE 0 3)
      if((v EQUAL 1 AND (n EQUAL 1 OR i EQUAL 1)) OR (NOT n EQUAL 0 AND n EQUAL i))
        continue()
      endif()
      list(APPEND outputs "${v}${n}${i}")
    endforeach()
  endforeach()
endforeach()
list(APPEND outputs 01234)
foreach(output IN LISTS outputs)
  string(SUBSTRING "${output}" 0 1 v)
  string(SUBSTRING "${output}" 1 1 n)
  string(SUBSTRING "${output}" 2 1 i)
  set(base 0)
  set(metal 0)
  if(output STREQUAL "01234")
    set(base 3)
    set(metal 4)
  endif()
  set(locations -DSURFACE_WRITES_VELOCITY=${v} -DSURFACE_NORMAL_LOCATION=${n}
    -DSURFACE_IDENTITY_LOCATION=${i} -DSURFACE_BASE_LOCATION=${base} -DSURFACE_METALROUGH_LOCATION=${metal})
  outshine_shader("groundLit-${output}.frag" groundLit.glsl frag ${locations})
  outshine_shader("groundPatchLit-${output}.frag" groundLit.glsl frag -DGROUND_PATCHES=1 ${locations})
  outshine_shader("prismLit-${output}.frag" litFragment.glsl frag -DPRISM_GRID=1
    -DLIT_KIND=0 -DLIT_TEXTURED=1 -DLIT_MAPPED=1 ${locations})
  foreach(kind 00 01 10 11 20 21 30)
    string(SUBSTRING "${kind}" 0 1 k)
    string(SUBSTRING "${kind}" 1 1 t)
    outshine_shader("flat-${kind}-${output}.frag" flatFragment.glsl frag -DFLAT_KIND=${k} -DFLAT_TEXTURED=${t} ${locations})
  endforeach()
  foreach(k RANGE 0 3)
    foreach(layout 00 10 11)
      string(SUBSTRING "${layout}" 0 1 t)
      string(SUBSTRING "${layout}" 1 1 m)
      outshine_shader("lit-${k}${layout}-${output}.frag" litFragment.glsl frag
        -DLIT_KIND=${k} -DLIT_TEXTURED=${t} -DLIT_MAPPED=${m} ${locations})
    endforeach()
  endforeach()
endforeach()
foreach(u RANGE 0 2)
  foreach(t RANGE 0 1)
    foreach(v RANGE 0 1)
      outshine_shader("flat-${u}${t}-${v}.vert" flatVertex.glsl vert
        -DFLAT_UVS=${u} -DFLAT_TINTED=${t} -DSURFACE_WRITES_VELOCITY=${v})
    endforeach()
  endforeach()
  foreach(c RANGE 0 1)
    outshine_shader("geometryShadow-${u}${c}.vert" geometryShadowVertex.glsl vert -DSHADOW_UVS=${u} -DSHADOW_TINTED=${c})
  endforeach()
  outshine_shader("geometryShadow-${u}.frag" geometryShadowFragment.glsl frag -DSHADOW_UVS=${u})
endforeach()
foreach(layout 000 010 100 110 200 210 101 111 201 211)
  string(SUBSTRING "${layout}" 0 1 u)
  string(SUBSTRING "${layout}" 1 1 t)
  string(SUBSTRING "${layout}" 2 1 m)
  foreach(v RANGE 0 1)
    outshine_shader("lit-${layout}-${v}.vert" litVertex.glsl vert
      -DLIT_UVS=${u} -DLIT_TINTED=${t} -DLIT_MAPPED=${m} -DSURFACE_WRITES_VELOCITY=${v})
  endforeach()
endforeach()
outshine_shader(prism-colour.vert prismVertex.glsl vert -DPRISM_DEPTH=0)
outshine_shader(prism-depth.vert prismVertex.glsl vert -DPRISM_DEPTH=1)
add_custom_target(outshine_shaders DEPENDS ${OUTSHINE_SHADER_ARTIFACTS})
set(shader_names ${OUTSHINE_SHADER_ARTIFACTS})
list(TRANSFORM shader_names REPLACE "^.*/" "")
file(GENERATE OUTPUT "${PROJECT_BINARY_DIR}/shader-artifacts.txt"
  CONTENT "$<JOIN:${shader_names},\n>\n")
add_executable(compute-shader-contracts EXCLUDE_FROM_ALL "${PROJECT_SOURCE_DIR}/test/scripts/compute-shader-contracts.cpp")
target_link_libraries(compute-shader-contracts PRIVATE outshine_compiler)
target_include_directories(compute-shader-contracts PRIVATE "${PROJECT_SOURCE_DIR}/src/render/pipelines")
add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/compute-shaders.json"
  COMMAND compute-shader-contracts > "${PROJECT_BINARY_DIR}/compute-shaders.json"
  DEPENDS compute-shader-contracts VERBATIM)
add_custom_target(compute_contracts DEPENDS "${PROJECT_BINARY_DIR}/compute-shaders.json")
add_dependencies(outshine_shaders compute_contracts)

set(calibration_shader_artifacts ${OUTSHINE_SHADER_ARTIFACTS})
list(FILTER calibration_shader_artifacts INCLUDE REGEX "/calibration[^/]*\\.spv$")
add_custom_target(outshine_calibration_shaders DEPENDS ${calibration_shader_artifacts})
add_dependencies(outshine_diagnostics_calibration outshine_calibration_shaders)

add_dependencies(outshine_render outshine_shaders)
