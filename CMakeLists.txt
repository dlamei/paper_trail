cmake_minimum_required(VERSION 3.15)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
# -Wall -Wextra -Wundef -Wcast-align -Wcast-qual -Wswitch-enum -g")

if (LINUX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
endif()

project(papertrail C CXX)

#libraries
include_directories(./)

find_package(Vulkan REQUIRED FATAL_ERROR)

# zlib
add_subdirectory(ext/zlib)
include_directories(ext/zlib)

# libjpeg-turbo
add_subdirectory(ext/libjpeg-turbo)
include_directories(${JPEG_SOURCES})

add_library(vma
        ext/vk_mem_alloc.h
        ext/vk_mem_alloc.cpp
)
target_link_libraries(vma Vulkan::Vulkan)

# glfw
if (LINUX)
    set(GLFW_BUILD_WAYLAND OFF)
endif()
add_subdirectory(ext/glfw)


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

        ext/vk_mem_alloc.h
        ext/stb_ds.h
        ext/stb_image.h
        ext/stb.c
)

add_executable(papertrail ${SOURCES} src/main.c)
set_property(TARGET papertrail PROPERTY C_STANDARD 11)
target_link_libraries(papertrail zlib  glfw Vulkan::Vulkan turbojpeg vma)

if (UNIX)
    target_link_libraries(papertrail m)
elseif(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endif()
