cmake_minimum_required(VERSION 3.15)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
# -Wall -Wextra -Wundef -Wcast-align -Wcast-qual -Wswitch-enum -g")

project(papertrail C)

add_subdirectory(vendor/zlib)
include_directories(vendor/zlib)

set(MOZJPEG_DIR ${CMAKE_SOURCE_DIR}/vendor/mozjpeg)
set(MOZJPEG_OUT_ROOT ${CMAKE_BINARY_DIR}/mozjpeg)
set(MOZJPEG_OUT ${MOZJPEG_OUT_ROOT}/release)


set(SOURCES
    src/pdf_parse.h
    src/pdf_parse.c
    src/pdf_objects.c
    src/pdf_objects.h
    src/decompress.h
    src/decompress.c

    src/stb_ds.h
    src/stb_image.h
    src/stb.c
)

execute_process(
    COMMAND cargo build --release --offline --target-dir ${MOZJPEG_OUT_ROOT} --manifest-path ${MOZJPEG_DIR}/Cargo.toml
    )

add_executable(papertrail ${SOURCES} src/main.c)

# add_custom_target(
#     TARGET papertrail
#     POST_BUILD
#     DEPENDS ${SOURCES} src/main.c # rebuild on change
#     COMMAND cargo build --release --offline --target-dir ${MOZJPEG_OUT_ROOT} --manifest-path ${MOZJPEG_DIR}/Cargo.toml
# )

find_library(MOZJPEG_LIB mozjpeg HINTS ${MOZJPEG_OUT} REQUIRED)
target_link_libraries(papertrail zlib ${MOZJPEG_LIB})

if (UNIX)
    target_link_libraries(papertrail m)
endif()

include_directories(${MOZJPEG_OUT}/build/include) # generated headers
include_directories(${MOZJPEG_DIR}/include) # default headers

