find_program(OUTSHINE_LINUX_CXX NAMES clang++-22 clang++
  HINTS "$ENV{HOME}/.local/bin" REQUIRED)
set(CMAKE_CXX_COMPILER "${OUTSHINE_LINUX_CXX}" CACHE FILEPATH "Linux C++23 compiler")
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libstdc++")
set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "-lc++abi")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}/../build/deps/install")
