set(OUTSHINE_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -Wshadow -Werror -Wno-unused-parameter
  -ffp-contract=off -fstack-protector-strong -Wformat=2 -Werror=format-security -fPIE)
if(APPLE)
  set(OUTSHINE_COMPILE_DEFINITIONS _FORTIFY_SOURCE=2
    _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST)
  set(OUTSHINE_LINK_OPTIONS -Wl,-pie)
else()
  set(OUTSHINE_COMPILE_DEFINITIONS _FORTIFY_SOURCE=3 _GLIBCXX_ASSERTIONS
    _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST)
  list(APPEND OUTSHINE_COMPILE_OPTIONS -fstack-clash-protection)
  set(OUTSHINE_LINK_OPTIONS -pie -Wl,-z,relro,-z,now,-z,noexecstack)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i.86)$")
    list(APPEND OUTSHINE_COMPILE_OPTIONS -fcf-protection=full)
  endif()
endif()
add_library(outshine_compiler INTERFACE)
target_compile_features(outshine_compiler INTERFACE cxx_std_23)
target_compile_options(outshine_compiler INTERFACE ${OUTSHINE_COMPILE_OPTIONS}
  $<$<CONFIG:Release>:-O2>)
target_compile_definitions(outshine_compiler INTERFACE ${OUTSHINE_COMPILE_DEFINITIONS})
target_link_options(outshine_compiler INTERFACE ${OUTSHINE_LINK_OPTIONS})
if(APPLE)
  target_compile_options(outshine_compiler INTERFACE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
endif()
