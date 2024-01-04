cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRE ON)

set( CMAKE_CXX_FLAGS " -Wextra " )

project(paper_trail)


set(SOURCES
    src/pdf_parse.h
    src/pdf_parse.cpp
)

add_executable(paper_trail ${SOURCES} src/main.cpp)
# target_link_libraries(paper_trail ${CMAKE_DL_LIBS})
