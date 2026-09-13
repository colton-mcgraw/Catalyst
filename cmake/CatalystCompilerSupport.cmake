# Catalyst is written against two C++23 features that only recent toolchains implement: deducing
# this (P0847) and std::expected (P0323). A toolchain missing either does not fail with one clear
# diagnostic -- it fails several minutes into the build with a few hundred lines of template errors
# out of <catalyst/math/vector.hpp> or <catalyst/audio/error.hpp>. These checks turn that into a
# sentence at configure time.
#
# This list used to be four. std::forward_like and std::move_only_function were the other two, and
# between them they cost Catalyst every Clang and all of macOS: no Clang can compile libstdc++'s
# forward_like, and libc++ has no move_only_function at all. Both are now small local equivalents
# (math/detail/forward_like.hpp, core/detail/move_only_function.hpp) and neither is required of the
# toolchain any more.
#
# The checks probe features rather than compare version numbers, because no version number answers
# the question on its own -- the standard library decides as much as the compiler does, and the two
# are chosen separately. Clang 18 implements deducing this, for instance, but cannot see libstdc++'s
# <expected>: libstdc++ gates that header on __cpp_concepts >= 202002L, which Clang only began
# reporting in 19. So each check compiles the real construct against the library this build links.

include(CheckCXXSourceCompiles)

function(catalyst_require_cxx23_support)
  # try_compile inherits CMAKE_CXX_STANDARD through CMP0067 (NEW at our cmake_minimum_required), so
  # these compile with the same -std= flag the library does.
  set(CMAKE_REQUIRED_QUIET ON)

  check_cxx_source_compiles("
    struct probe
    {
        int value;
        constexpr int get(this auto &&self) { return self.value; }
    };
    int main() { return probe{0}.get(); }
  " CATALYST_HAS_DEDUCING_THIS)

  check_cxx_source_compiles("
    #include <expected>
    int main()
    {
        std::expected<int, int> ok{0};
        return ok.has_value() ? *ok : 1;
    }
  " CATALYST_HAS_STD_EXPECTED)



  set(_missing "")

  if(NOT CATALYST_HAS_DEDUCING_THIS)
    list(APPEND _missing
      "  - deducing this (P0847)          used by catalyst::math and catalyst::resource::json")
  endif()

  if(NOT CATALYST_HAS_STD_EXPECTED)
    list(APPEND _missing
      "  - std::expected (P0323)          the return type of every fallible Catalyst call")
  endif()



  if(_missing STREQUAL "")
    return()
  endif()

  list(JOIN _missing "\n" _missing_text)

  message(FATAL_ERROR
    "Catalyst needs a C++23 toolchain, and this one is missing:\n"
    "${_missing_text}\n\n"
    "Detected: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} "
    "(${CMAKE_CXX_COMPILER})\n\n"
    "Known-good toolchains:\n"
    "  - GCC 14 or newer\n"
    "  - Clang 19 or newer, against libstdc++ 14 or newer\n"
    "  - MSVC 19.40 (Visual Studio 2022 17.10) or newer\n\n"
    "Clang 18 is not enough: libstdc++ gates <expected> on __cpp_concepts >= 202002L, which\n"
    "Clang only began reporting in 19.\n\n"
    "libc++ does not work yet, at any Clang version. Catalyst carries its own\n"
    "move_only_function and guards the tz-database and <syncstream> paths, but libc++ also\n"
    "leaves the floating-point std::from_chars overloads deleted, which catalyst::resource's\n"
    "JSON and CSV number parsing needs. That one wants a real float parser, not a shim. It is\n"
    "also what stands between Catalyst and macOS, where libc++ is the only option.\n\n"
    "On Ubuntu 24.04 the default g++ is 13 and will not work: install g++-14 and configure with\n"
    "  cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-14\n")
endfunction()
