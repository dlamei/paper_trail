cmake_minimum_required(VERSION 2.8.12...3.28)

if(CMAKE_EXECUTABLE_SUFFIX)
  set(CMAKE_EXECUTABLE_SUFFIX_TMP ${CMAKE_EXECUTABLE_SUFFIX})
endif()

project(libjpeg-turbo C)
set(VERSION 3.0.3)
set(COPYRIGHT_YEAR "1991-2024")
string(REPLACE "." ";" VERSION_TRIPLET ${VERSION})
list(GET VERSION_TRIPLET 0 VERSION_MAJOR)
list(GET VERSION_TRIPLET 1 VERSION_MINOR)
list(GET VERSION_TRIPLET 2 VERSION_REVISION)
function(pad_number NUMBER OUTPUT_LEN)
  string(LENGTH "${${NUMBER}}" INPUT_LEN)
  if(INPUT_LEN LESS OUTPUT_LEN)
    math(EXPR ZEROES "${OUTPUT_LEN} - ${INPUT_LEN} - 1")
    set(NUM ${${NUMBER}})
    foreach(C RANGE ${ZEROES})
      set(NUM "0${NUM}")
    endforeach()
    set(${NUMBER} ${NUM} PARENT_SCOPE)
  endif()
endfunction()
pad_number(VERSION_MINOR 3)
pad_number(VERSION_REVISION 3)
set(LIBJPEG_TURBO_VERSION_NUMBER ${VERSION_MAJOR}${VERSION_MINOR}${VERSION_REVISION})

# The libjpeg-turbo build system has never supported and will never support
# being integrated into another build system using add_subdirectory(), because
# doing so would require that we (minimally):
#
# 1. avoid using certain CMake variables, such as CMAKE_SOURCE_DIR,
#    CMAKE_BINARY_DIR, and PROJECT_NAME;
# 2. avoid using implicit include directories and relative paths;
# 3. optionally provide a way to skip the installation of libjpeg-turbo
#    components when the 'install' target is built;
# 4. optionally provide a way to postfix target names, to avoid namespace
#    conflicts;
# 5. restructure the top-level CMakeLists.txt so that it properly sets the
#    PROJECT_VERSION variable; and
# 6. design automated regression tests to ensure that new commits don't break
#    any of the above.
#
# Even if we did all of that, issues would still arise, because it is
# impossible for an upstream build system to anticipate the widely varying
# needs of every downstream build system.  That's why the CMake
# ExternalProject_Add() function exists.  Downstream projects that wish to
# integrate libjpeg-turbo as a subdirectory should either use
# ExternalProject_Add() or make downstream modifications to the libjpeg-turbo
# build system to suit their specific needs.  Please do not file bug reports,
# feature requests, or pull requests regarding this.
#if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
#  message(FATAL_ERROR "The libjpeg-turbo build system cannot be integrated into another build system using add_subdirectory().  Use ExternalProject_Add() instead.")
#endif()

# CMake 3.14 and later sets CMAKE_MACOSX_BUNDLE to TRUE by default when
# CMAKE_SYSTEM_NAME is iOS, tvOS, or watchOS, which breaks the libjpeg-turbo
# build.  (Specifically, when CMAKE_MACOSX_BUNDLE is TRUE, executables for
# Apple platforms are built as application bundles, which causes CMake to
# complain that our install() directives for executables do not specify a
# BUNDLE DESTINATION.  Even if CMake did not complain, building executables as
# application bundles would break our iOS packages.)
set(CMAKE_MACOSX_BUNDLE FALSE)

get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY
  GENERATOR_IS_MULTI_CONFIG)
# If the GENERATOR_IS_MULTI_CONFIG property doesn't exist (CMake < 3.9), then
# set the GENERATOR_IS_MULTI_CONFIG variable manually if the generator is
# Visual Studio or Xcode (the only multi-config generators in CMake < 3.9).
if(NOT GENERATOR_IS_MULTI_CONFIG AND (MSVC_IDE OR XCODE))
  set(GENERATOR_IS_MULTI_CONFIG TRUE)
endif()

string(TIMESTAMP DEFAULT_BUILD "%Y%m%d")
set(BUILD ${DEFAULT_BUILD} CACHE STRING "Build string (default: ${DEFAULT_BUILD})")

# NOTE: On Windows, this does nothing except when using MinGW or Cygwin.
# CMAKE_BUILD_TYPE has no meaning in Visual Studio, and it always defaults to
# Debug when using NMake.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

message(STATUS "VERSION = ${VERSION}, BUILD = ${BUILD}")

set(PKGNAME ${PROJECT_NAME} CACHE STRING
        "Distribution package name (default: ${PROJECT_NAME})")
set(PKGVENDOR "The ${PROJECT_NAME} Project" CACHE STRING
        "Vendor name to be included in distribution package descriptions (default: The ${PROJECT_NAME} Project)")
set(PKGURL "http://www.${PROJECT_NAME}.org" CACHE STRING
        "URL of project web site to be included in distribution package descriptions (default: http://www.${PROJECT_NAME}.org)")
set(PKGEMAIL "information@${PROJECT_NAME}.org" CACHE STRING
        "E-mail of project maintainer to be included in distribution package descriptions (default: information@${PROJECT_NAME}.org")
set(PKGID "com.${PROJECT_NAME}.${PKGNAME}" CACHE STRING
        "Globally unique package identifier (reverse DNS notation) (default: com.${PROJECT_NAME}.${PKGNAME})")

# Detect CPU type and whether we're building 64-bit or 32-bit code
math(EXPR BITS "${CMAKE_SIZEOF_VOID_P} * 8")
string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} CMAKE_SYSTEM_PROCESSOR_LC)
set(COUNT 1)
foreach(ARCH ${CMAKE_OSX_ARCHITECTURES})
  if(COUNT GREATER 1)
    message(FATAL_ERROR "libjpeg-turbo contains assembly code, so it cannot be built with multiple values in CMAKE_OSX_ARCHITECTURES.")
  endif()
  math(EXPR COUNT "${COUNT}+1")
endforeach()
if(CMAKE_SYSTEM_PROCESSOR_LC MATCHES "x86_64" OR
  CMAKE_SYSTEM_PROCESSOR_LC MATCHES "amd64" OR
  CMAKE_SYSTEM_PROCESSOR_LC MATCHES "i[0-9]86" OR
  CMAKE_SYSTEM_PROCESSOR_LC MATCHES "x86" OR
  CMAKE_SYSTEM_PROCESSOR_LC MATCHES "ia32")
  if(BITS EQUAL 64 OR CMAKE_C_COMPILER_ABI MATCHES "ELF X32")
    set(CPU_TYPE x86_64)
  else()
    set(CPU_TYPE i386)
  endif()
  if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL ${CPU_TYPE})
    set(CMAKE_SYSTEM_PROCESSOR ${CPU_TYPE})
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR_LC STREQUAL "aarch64" OR
  CMAKE_SYSTEM_PROCESSOR_LC MATCHES "^arm")
  if(BITS EQUAL 64)
    set(CPU_TYPE arm64)
  else()
    set(CPU_TYPE arm)
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR_LC MATCHES "^ppc" OR
  CMAKE_SYSTEM_PROCESSOR_LC MATCHES "^powerpc")
  set(CPU_TYPE powerpc)
else()
  set(CPU_TYPE ${CMAKE_SYSTEM_PROCESSOR_LC})
endif()
if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64" OR
  CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR
  CMAKE_OSX_ARCHITECTURES MATCHES "i386")
  set(CPU_TYPE ${CMAKE_OSX_ARCHITECTURES})
endif()
if(CMAKE_OSX_ARCHITECTURES MATCHES "ppc")
  set(CPU_TYPE powerpc)
endif()
if(MSVC_IDE AND CMAKE_GENERATOR_PLATFORM MATCHES "arm64")
  set(CPU_TYPE arm64)
endif()

message(STATUS "${BITS}-bit build (${CPU_TYPE})")


###############################################################################
# CONFIGURATION OPTIONS
###############################################################################

set(REQUIRE_SIMD 0)
set(WITH_ARITH_DEC 0)
set(WITH_ARITH_ENC 0)
set(WITH_TURBOJPEG 1)
set(FORCE_INLINE 1)

macro(report_option var desc)
  if(${var})
    message(STATUS "${desc} enabled (${var} = ${${var}})")
  else()
    message(STATUS "${desc} disabled (${var} = ${${var}})")
  endif()
endmacro()

if(WITH_JPEG8 OR WITH_JPEG7)
  set(WITH_ARITH_ENC 1)
  set(WITH_ARITH_DEC 1)
endif()

if(WITH_ARITH_DEC)
  set(D_ARITH_CODING_SUPPORTED 1)
endif()
report_option(WITH_ARITH_DEC "Arithmetic decoding support")

if(WITH_ARITH_ENC)
  set(C_ARITH_CODING_SUPPORTED 1)
endif()
report_option(WITH_ARITH_ENC "Arithmetic encoding support")

report_option(WITH_TURBOJPEG "TurboJPEG API library")

if(NOT WITH_JPEG8)
  set(MEM_SRCDST_FUNCTIONS "global:  jpeg_mem_dest;  jpeg_mem_src;")
endif()

# 0: Original libjpeg v6b/v7/v8 API/ABI
#
# libjpeg v6b/v7 API/ABI emulation:
# 1: + In-memory source/destination managers (libjpeg-turbo 1.3.x)
# 2: + Partial image decompression functions (libjpeg-turbo 1.5.x)
# 3: + ICC functions (libjpeg-turbo 2.0.x)
# 4: + 12-bit-per-component and lossless functions (libjpeg-turbo 2.2.x)
#
# libjpeg v8 API/ABI emulation:
# 1: + Partial image decompression functions (libjpeg-turbo 1.5.x)
# 2: + ICC functions (libjpeg-turbo 2.0.x)
# 3: + 12-bit-per-component and lossless functions (libjpeg-turbo 2.2.x)
set(SO_AGE 3)
if(NOT WITH_JPEG8)
  set(SO_AGE 4)
endif()

if(WITH_JPEG8)
  set(JPEG_LIB_VERSION 80)
elseif(WITH_JPEG7)
  set(JPEG_LIB_VERSION 70)
else()
  set(JPEG_LIB_VERSION 62)
endif()

math(EXPR JPEG_LIB_VERSION_DIV10 "${JPEG_LIB_VERSION} / 10")
math(EXPR JPEG_LIB_VERSION_MOD10 "${JPEG_LIB_VERSION} % 10")
if(JPEG_LIB_VERSION STREQUAL "62")
  set(DEFAULT_SO_MAJOR_VERSION ${JPEG_LIB_VERSION})
else()
  set(DEFAULT_SO_MAJOR_VERSION ${JPEG_LIB_VERSION_DIV10})
endif()
if(JPEG_LIB_VERSION STREQUAL "80")
  set(DEFAULT_SO_MINOR_VERSION 2)
else()
  set(DEFAULT_SO_MINOR_VERSION 0)
endif()

# This causes SO_MAJOR_VERSION/SO_MINOR_VERSION to reset to defaults if
# WITH_JPEG7 or WITH_JPEG8 has changed.
if((DEFINED WITH_JPEG7_INT AND NOT WITH_JPEG7 EQUAL WITH_JPEG7_INT) OR
  (DEFINED WITH_JPEG8_INT AND NOT WITH_JPEG8 EQUAL WITH_JPEG8_INT))
  set(FORCE_SO_VERSION "FORCE")
endif()
set(WITH_JPEG7_INT ${WITH_JPEG7} CACHE INTERNAL "")
set(WITH_JPEG8_INT ${WITH_JPEG8} CACHE INTERNAL "")

set(SO_MAJOR_VERSION ${DEFAULT_SO_MAJOR_VERSION} CACHE STRING
  "Major version of the libjpeg API shared library (default: ${DEFAULT_SO_MAJOR_VERSION})"
  ${FORCE_SO_VERSION})
set(SO_MINOR_VERSION ${DEFAULT_SO_MINOR_VERSION} CACHE STRING
  "Minor version of the libjpeg API shared library (default: ${DEFAULT_SO_MINOR_VERSION})"
  ${FORCE_SO_VERSION})

set(JPEG_LIB_VERSION_DECIMAL "${JPEG_LIB_VERSION_DIV10}.${JPEG_LIB_VERSION_MOD10}")
message(STATUS "Emulating libjpeg API/ABI v${JPEG_LIB_VERSION_DECIMAL} (WITH_JPEG7 = ${WITH_JPEG7}, WITH_JPEG8 = ${WITH_JPEG8})")
message(STATUS "libjpeg API shared library version = ${SO_MAJOR_VERSION}.${SO_AGE}.${SO_MINOR_VERSION}")

# Because the TurboJPEG API library uses versioned symbols and changes the
# names of functions whenever they are modified in a backward-incompatible
# manner, it is always backward-ABI-compatible with itself, so the major and
# minor SO versions don't change.  However, we increase the middle number (the
# SO "age") whenever functions are added to the API, because adding functions
# affects forward API/ABI compatibility.
set(TURBOJPEG_SO_MAJOR_VERSION 0)
# 0: TurboJPEG 1.3.x API
# 1: TurboJPEG 1.4.x API
#    The TurboJPEG 1.5.x API modified some of the function prototypes, adding
#    the const keyword in front of pointers to unmodified buffers, but that did
#    not affect forward API/ABI compatibility.
# 2: TurboJPEG 2.0.x API
#    The TurboJPEG 2.1.x API modified the behavior of the tjDecompressHeader3()
#    function so that it accepts "abbreviated table specification" (AKA
#    "tables-only") datastreams as well as JPEG images, but that did not affect
#    forward API/ABI compatibility.
# 3: TurboJPEG 3 API
set(TURBOJPEG_SO_AGE 3)
set(TURBOJPEG_SO_VERSION 0.${TURBOJPEG_SO_AGE}.0)


###############################################################################
# COMPILER SETTINGS
###############################################################################

string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_UC)

if(MSVC)
  # Use the static C library for all build types
  if(CMAKE_VERSION VERSION_EQUAL "3.15" OR
    CMAKE_VERSION VERSION_GREATER "3.15")
    if(CMAKE_BUILD_TYPE_UC STREQUAL "DEBUG")
      set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDebug)
    elseif(MSVC_IDE)
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
      set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
    endif()
    message(STATUS "Visual C++ run-time library: ${CMAKE_MSVC_RUNTIME_LIBRARY} (WITH_CRT_DLL = ${WITH_CRT_DLL})")
  else()
    foreach(var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
      CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
      if(${var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${var} "${${var}}")
      endif()
    endforeach()
  endif()
  add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  # Use the maximum optimization level for release builds
  foreach(var CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO)
    if(${var} MATCHES "-O2")
      string(REGEX REPLACE "-O2" "-O3" ${var} "${${var}}")
    endif()
  endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  if(CMAKE_C_COMPILER_ID MATCHES "SunPro")
    # Use the maximum optimization level for release builds
    foreach(var CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO)
      if(${var} MATCHES "-xO3")
        string(REGEX REPLACE "-xO3" "-xO5" ${var} "${${var}}")
      endif()
      if(${var} MATCHES "-xO2")
        string(REGEX REPLACE "-xO2" "-xO5" ${var} "${${var}}")
      endif()
    endforeach()
  endif()
endif()

set(EFFECTIVE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UC}}")
message(STATUS "Compiler flags = ${EFFECTIVE_C_FLAGS}")

set(EFFECTIVE_LD_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS_${CMAKE_BUILD_TYPE_UC}}")
message(STATUS "Linker flags = ${EFFECTIVE_LD_FLAGS}")

include(CheckCSourceCompiles)
include(CheckIncludeFiles)
include(CheckTypeSize)

check_type_size("size_t" SIZE_T)
check_type_size("unsigned long" UNSIGNED_LONG)

if(SIZE_T EQUAL UNSIGNED_LONG)
  check_c_source_compiles("int main(int argc, char **argv) { unsigned long a = argc;  return __builtin_ctzl(a); }"
    HAVE_BUILTIN_CTZL)
endif()
if(MSVC)
  check_include_files("intrin.h" HAVE_INTRIN_H)
endif()

if(UNIX)
  if(CMAKE_CROSSCOMPILING)
    set(RIGHT_SHIFT_IS_UNSIGNED 0)
  else()
    include(CheckCSourceRuns)
    check_c_source_runs("
      #include <stdio.h>
      #include <stdlib.h>
      static int is_shifting_signed (long arg) {
        long res = arg >> 4;
        if (res == -0x7F7E80CL)
          return 1; /* right shift is signed */
        /* see if unsigned-shift hack will fix it. */
        /* we can't just test exact value since it depends on width of long... */
        res |= 0xFFFFFFFFL << (32-4);
        if (res == -0x7F7E80CL)
          return 0; /* right shift is unsigned */
        printf(\"Right shift isn't acting as I expect it to.\\\\n\");
        printf(\"I fear the JPEG software will not work at all.\\\\n\\\\n\");
        return 0; /* try it with unsigned anyway */
      }
      int main (void) {
        exit(is_shifting_signed(-0x7F7E80B1L));
      }" RIGHT_SHIFT_IS_UNSIGNED)
  endif()
endif()

if(NOT MSVC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  check_c_source_compiles("extern const int table[1]; const int __attribute__((visibility(\"hidden\"))) table[1] = { 0 }; int main(void) { return table[0]; }"
    HIDDEN_WORKS)
  if(HIDDEN_WORKS)
    set(HIDDEN "__attribute__((visibility(\"hidden\")))")
    message(STATUS "HIDDEN = ${HIDDEN}")
  endif()
endif()

if(MSVC)
  set(INLINE_OPTIONS "__inline;inline")
else()
  set(INLINE_OPTIONS "__inline__;inline")
endif()
if(FORCE_INLINE)
  if(MSVC)
    list(INSERT INLINE_OPTIONS 0 "__forceinline")
  else()
    list(INSERT INLINE_OPTIONS 0 "inline __attribute__((always_inline))")
    list(INSERT INLINE_OPTIONS 0 "__inline__ __attribute__((always_inline))")
  endif()
endif()
foreach(inline ${INLINE_OPTIONS})
  check_c_source_compiles("${inline} static int foo(void) { return 0; } int main(void) { return foo(); }"
    INLINE_WORKS)
  if(INLINE_WORKS)
    set(INLINE ${inline})
    break()
  endif()
endforeach()
if(NOT INLINE_WORKS)
  message(FATAL_ERROR "Could not determine how to inline functions.")
endif()
message(STATUS "INLINE = ${INLINE} (FORCE_INLINE = ${FORCE_INLINE})")

if(MSVC)
  set(THREAD_LOCAL "__declspec(thread)")
else()
  set(THREAD_LOCAL "__thread")
endif()
check_c_source_compiles("static ${THREAD_LOCAL} int i;  int main(void) { i = 0;  return i; }"
  HAVE_THREAD_LOCAL)
if(HAVE_THREAD_LOCAL)
  message(STATUS "THREAD_LOCAL = ${THREAD_LOCAL}")
else()
  message(WARNING "Thread-local storage is not available.  The TurboJPEG API library's global error handler will not be thread-safe.")
  unset(THREAD_LOCAL)
endif()

if(UNIX AND NOT APPLE)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/conftest.map "VERS_1 { global: *; };")
  set(CMAKE_REQUIRED_FLAGS
    "-Wl,--version-script,${CMAKE_CURRENT_BINARY_DIR}/conftest.map")
  check_c_source_compiles("int main(void) { return 0; }" HAVE_VERSION_SCRIPT)
  set(CMAKE_REQUIRED_FLAGS)
  file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/conftest.map)
  if(HAVE_VERSION_SCRIPT)
    message(STATUS "Linker supports GNU-style version scripts")
    set(MAPFLAG "-Wl,--version-script,")
    set(TJMAPFLAG "-Wl,--version-script,")
  else()
    message(STATUS "Linker does not support GNU-style version scripts")
    if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
      # The Solaris linker doesn't like our version script for the libjpeg API
      # library, but the version script for the TurboJPEG API library should
      # still work.
      file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/conftest.map
        "VERS_1 { global: foo;  local: *; }; VERS_2 { global: foo2; } VERS_1;")
      set(CMAKE_REQUIRED_FLAGS "-Wl,-M,${CMAKE_CURRENT_BINARY_DIR}/conftest.map -shared")
      check_c_source_compiles("int foo() { return 0; } int foo2() { return 2; }"
        HAVE_MAPFILE)
      set(CMAKE_REQUIRED_FLAGS)
      file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/conftest.map)
      if(HAVE_MAPFILE)
        message(STATUS "Linker supports mapfiles")
        set(TJMAPFLAG "-Wl,-M,")
      else()
        message(STATUS "Linker does not support mapfiles")
      endif()
    endif()
  endif()
endif()

# Generate files
configure_file(jversion.h.in jversion.h)
if(UNIX)
  configure_file(libjpeg.map.in libjpeg.map)
endif()

# Include directories and compiler definitions
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})


###############################################################################
# TARGETS
###############################################################################

if(CMAKE_EXECUTABLE_SUFFIX_TMP)
  set(CMAKE_EXECUTABLE_SUFFIX ${CMAKE_EXECUTABLE_SUFFIX_TMP})
endif()
message(STATUS "CMAKE_EXECUTABLE_SUFFIX = ${CMAKE_EXECUTABLE_SUFFIX}")

set(JPEG16_SOURCES jcapistd.c jccolor.c jcdiffct.c jclossls.c jcmainct.c
  jcprepct.c jcsample.c jdapistd.c jdcolor.c jddiffct.c jdlossls.c jdmainct.c
  jdpostct.c jdsample.c jutils.c)
set(JPEG12_SOURCES ${JPEG16_SOURCES} jccoefct.c jcdctmgr.c jdcoefct.c
  jddctmgr.c jdmerge.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c jidctint.c
  jidctred.c jquant1.c jquant2.c)
set(JPEG_SOURCES ${JPEG12_SOURCES} jcapimin.c jchuff.c jcicc.c jcinit.c
  jclhuff.c jcmarker.c jcmaster.c jcomapi.c jcparam.c jcphuff.c jctrans.c
  jdapimin.c jdatadst.c jdatasrc.c jdhuff.c jdicc.c jdinput.c jdlhuff.c
  jdmarker.c jdmaster.c jdphuff.c jdtrans.c jerror.c jfdctflt.c jmemmgr.c
  jmemnobs.c jpeg_nbits.c)

if(WITH_ARITH_ENC OR WITH_ARITH_DEC)
  set(JPEG_SOURCES ${JPEG_SOURCES} jaricom.c)
endif()

if(WITH_ARITH_ENC)
  set(JPEG_SOURCES ${JPEG_SOURCES} jcarith.c)
endif()

if(WITH_ARITH_DEC)
  set(JPEG_SOURCES ${JPEG_SOURCES} jdarith.c)
endif()

if(WITH_SIMD)
  add_subdirectory(simd)
  if(NEON_INTRINSICS)
    add_definitions(-DNEON_INTRINSICS)
  endif()
else()
  message(STATUS "SIMD extensions: None (WITH_SIMD = ${WITH_SIMD})")
endif()

# We have to generate these here, because if the build system tries and fails
# to enable the SIMD extensions, the value of WITH_SIMD will have changed.
configure_file(jconfig.h.in jconfig.h)
configure_file(jconfigint.h.in jconfigint.h)

if(WITH_SIMD)
  message(STATUS "SIMD extensions: ${CPU_TYPE} (WITH_SIMD = ${WITH_SIMD})")
  if(MSVC_IDE OR XCODE)
    set_source_files_properties(${SIMD_OBJS} PROPERTIES GENERATED 1)
  endif()
  set(SIMD_TARGET_OBJECTS $<TARGET_OBJECTS:simd>)
endif()

# Compile a separate version of these source files with 12-bit and 16-bit
# data precision.
add_library(jpeg12 OBJECT ${JPEG12_SOURCES})
set_property(TARGET jpeg12 PROPERTY COMPILE_FLAGS
  "-DBITS_IN_JSAMPLE=12")
add_library(jpeg16 OBJECT ${JPEG16_SOURCES})
set_property(TARGET jpeg16 PROPERTY COMPILE_FLAGS
  "-DBITS_IN_JSAMPLE=16")
add_library(jpeg STATIC ${JPEG_SOURCES} ${SIMD_TARGET_OBJECTS}
  ${SIMD_OBJS} $<TARGET_OBJECTS:jpeg12>
  $<TARGET_OBJECTS:jpeg16>)
if(NOT MSVC)
  set_target_properties(jpeg PROPERTIES OUTPUT_NAME jpeg)
endif()

if(WITH_TURBOJPEG)
  add_library(turbojpeg12 OBJECT rdppm.c wrppm.c)
  set_property(TARGET turbojpeg12 PROPERTY COMPILE_FLAGS
    "-DBITS_IN_JSAMPLE=12 -DPPM_SUPPORTED")
  add_library(turbojpeg16 OBJECT rdppm.c wrppm.c)
  set_property(TARGET turbojpeg16 PROPERTY COMPILE_FLAGS
    "-DBITS_IN_JSAMPLE=16 -DPPM_SUPPORTED")
  add_library(turbojpeg STATIC ${JPEG_SOURCES} ${SIMD_TARGET_OBJECTS}
    ${SIMD_OBJS} turbojpeg.c transupp.c jdatadst-tj.c jdatasrc-tj.c rdbmp.c
    rdppm.c wrbmp.c wrppm.c $<TARGET_OBJECTS:jpeg12>
    $<TARGET_OBJECTS:jpeg16> $<TARGET_OBJECTS:turbojpeg12>
    $<TARGET_OBJECTS:turbojpeg16>)
  set_property(TARGET turbojpeg PROPERTY COMPILE_FLAGS
    "-DBMP_SUPPORTED -DPPM_SUPPORTED")
  if(NOT MSVC)
    set_target_properties(turbojpeg PROPERTIES OUTPUT_NAME turbojpeg)
  endif()

  target_include_directories(turbojpeg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(WIN32)
  set(USE_SETMODE "-DUSE_SETMODE")
endif()
set(CDJPEG_COMPILE_FLAGS
  "-DBMP_SUPPORTED -DGIF_SUPPORTED -DPPM_SUPPORTED -DTARGA_SUPPORTED ${USE_SETMODE}")
