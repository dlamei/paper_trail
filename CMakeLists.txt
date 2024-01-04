cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRE ON)

set( CMAKE_CXX_FLAGS " -pthread " )

project(paper_trail)


set(SOURCES
    src/gb.h
)

add_executable(paper_trail ${SOURCES} src/main.cpp)
target_link_libraries(paper_trail ${CMAKE_DL_LIBS})

add_custom_target(run
    COMMAND echo "running paper_trail..." && echo "" && ./paper_trail
    DEPENDS paper_trail
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)