#!/bin/sh

ConfigureNativeHardening() {
  nativeMacros=$(${CXX:-c++} -std=c++23 -dM -E -x c++ -include cstddef /dev/null) || return 1
  NATIVE_COMPILE_FLAGS='-fstack-protector-strong -Wformat=2 -Werror=format-security'
  NATIVE_LINK_FLAGS=
  NATIVE_DEBUG_FLAGS=
  case "$nativeMacros" in
    *'define __linux__ '*)
      NATIVE_COMPILE_FLAGS="$NATIVE_COMPILE_FLAGS -fstack-clash-protection -fPIE -D_FORTIFY_SOURCE=3"
      NATIVE_LINK_FLAGS='-pie -Wl,-z,relro,-z,now,-z,noexecstack'
      case "$nativeMacros" in
        *'define __x86_64__ '*|*'define __i386__ '*)
          NATIVE_COMPILE_FLAGS="$NATIVE_COMPILE_FLAGS -fcf-protection=full"
          ;;
      esac
      ;;
    *'define __APPLE__ '*)
      NATIVE_COMPILE_FLAGS="$NATIVE_COMPILE_FLAGS -fPIE -D_FORTIFY_SOURCE=2"
      NATIVE_LINK_FLAGS='-Wl,-pie'
      ;;
    *)
      printf '%s\n' 'native_hardening: unsupported compiler target' >&2
      return 1
      ;;
  esac
  case "$nativeMacros" in
    *'define __GLIBCXX__ '*)
      NATIVE_COMPILE_FLAGS="$NATIVE_COMPILE_FLAGS -D_GLIBCXX_ASSERTIONS"
      ;;
    *'define _LIBCPP_VERSION '*)
      NATIVE_COMPILE_FLAGS="$NATIVE_COMPILE_FLAGS -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST"
      NATIVE_DEBUG_FLAGS='-U_LIBCPP_HARDENING_MODE -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG'
      ;;
  esac
}

if [ "${1:-}" = --compile ] || [ "${1:-}" = --link ]; then
  ConfigureNativeHardening || exit 2
  case "$1" in
    --compile) printf '%s\n' "$NATIVE_COMPILE_FLAGS" ;;
    --link) printf '%s\n' "$NATIVE_LINK_FLAGS" ;;
  esac
fi
