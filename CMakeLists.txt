cmake_minimum_required(VERSION 3.15)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
# -Wall -Wextra -Wundef -Wcast-align -Wcast-qual -Wswitch-enum -g")

if (LINUX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
endif()

project(papertrail C)

#libraries
include_directories(vendor)

find_package(Vulkan REQUIRED FATAL_ERROR)

# zlib
add_subdirectory(vendor/zlib)
include_directories(vendor/zlib)

# libjpeg-turbo
add_subdirectory(vendor/libjpeg-turbo)
include_directories(${JPEG_SOURCES})



# glfw
if (LINUX)
    set(GLFW_BUILD_WAYLAND OFF)
endif()
add_subdirectory(vendor/glfw)


set(SOURCES
    src/pdf_parse.h
    src/pdf_parse.c
    src/pdf_objects.c
    src/pdf_objects.h
    src/decompress.h
    src/decompress.c

    src/window.h
    src/window.c
    src/vulkan.h
    src/vulkan.c

    src/stb_ds.h
    src/stb_image.h
    src/stb.c
)

add_executable(papertrail ${SOURCES} src/main.c)
set_property(TARGET papertrail PROPERTY C_STANDARD 11)
target_link_libraries(papertrail zlib  glfw Vulkan::Vulkan turbojpeg)

if (UNIX)
    target_link_libraries(papertrail m)
elseif(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endif()
