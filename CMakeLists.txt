cmake_minimum_required(VERSION 3.23)
project(EvoBot LANGUAGES CXX)

#Necessary for shared library on Windows
if (MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DNOMINMAX)
endif()

if (LINUX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-write-strings -Wno-format-security -fPIC -m32")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ldl -lm -static-libgcc -static-libstdc++ -static")
endif()

set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_STATIC_LIBRARY_PREFIX "")

add_library(Detour STATIC)
target_include_directories(Detour PRIVATE "${PROJECT_SOURCE_DIR}")
add_subdirectory("Detour")

add_library(DetourTileCache STATIC)
target_include_directories(DetourTileCache PRIVATE "${PROJECT_SOURCE_DIR}")
add_subdirectory("DetourTileCache")

add_library(evobot SHARED)
add_dependencies(evobot Detour DetourTileCache)
target_include_directories(evobot PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(evobot PRIVATE Detour DetourTileCache)
add_subdirectory("evobot")
set_target_properties(evobot PROPERTIES OUTPUT_NAME "evobot_mm")

install(TARGETS Detour FILE_SET HEADERS)
install(TARGETS DetourTileCache FILE_SET HEADERS)