cmake_minimum_required(VERSION 3.15)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c17")
# -Wall -Wextra -Wundef -Wcast-align -Wcast-qual -Wswitch-enum -g")

project(papertrail C)

set(SOURCES
    src/pdf_parse.h
    src/pdf_parse.c
    src/pdf_objects.c
    src/pdf_objects.h
    src/stb_ds.h
    src/stb_ds.c
)

add_executable(papertrail ${SOURCES} src/main.c)
# target_link_libraries(paper_trail ${CMAKE_DL_LIBS})
