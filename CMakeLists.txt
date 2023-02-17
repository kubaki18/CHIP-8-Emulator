cmake_minimum_required(VERSION 3.16)
project(chip-8 C)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

add_executable(chip-8 src/main.c)

if (ENDIAN STREQUAL "small")
    target_compile_definitions(chip-8 PRIVATE SMALL_ENDIAN=1)
else()
    target_compile_definitions(chip-8 PRIVATE BIG_ENDIAN=1)
endif()

if (ARCH STREQUAL "cosmac")
    target_compile_definitions(chip-8 PRIVATE COSMAC=1)
endif()

if (UNIX)
    find_package(X11 REQUIRED)
    include_directories(${X11_INCLUDE_DIR})

    target_link_libraries(chip-8 ${SDL2_LIBRARIES} ${X11_LIBRARIES})
endif (UNIX)

if(WIN32)
    target_link_libraries(chip-8 ${SDL2_LIBRARIES})
endif (WIN32)
