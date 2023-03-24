file(GLOB_RECURSE HeaderFiles LIST_DIRECTORIES false ${PROJECT_SOURCE_DIR}/evobot/src/*.h)
file(GLOB_RECURSE SourceFiles LIST_DIRECTORIES false ${PROJECT_SOURCE_DIR}/evobot/src/*.cpp)

target_sources(evobot 
    PRIVATE ${SourceFiles}
    PUBLIC FILE_SET HEADERS 
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES ${HeaderFiles})

if (MSVC)
    set(RESOURCE_FILES ${PROJECT_SOURCE_DIR}/evobot/fastlz/fastlz.c)
endif()

set_source_files_properties(SourceFiles PROPERTIES COMPILE_OPTIONS "-Wwrite-strings")

target_include_directories(evobot PRIVATE ${PROJECT_SOURCE_DIR}/evobot/metamod)
target_include_directories(evobot PRIVATE ${PROJECT_SOURCE_DIR}/evobot/HLSDK/common)
target_include_directories(evobot PRIVATE ${PROJECT_SOURCE_DIR}/evobot/HLSDK/dlls)
target_include_directories(evobot PRIVATE ${PROJECT_SOURCE_DIR}/evobot/HLSDK/engine)
target_include_directories(evobot PRIVATE ${PROJECT_SOURCE_DIR}/evobot/HLSDK/pm_shared)
target_include_directories(evobot PRIVATE ${PROJECT_SOURCE_DIR}/Detour/Include)
target_include_directories(evobot PRIVATE ${PROJECT_SOURCE_DIR}/DetourTileCache/Include)
target_include_directories(evobot PRIVATE ${PROJECT_SOURCE_DIR}/metamod)
target_include_directories(evobot PRIVATE ${PROJECT_SOURCE_DIR}/evobot/fastlz)
