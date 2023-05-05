file(GLOB_RECURSE HeaderFiles LIST_DIRECTORIES false ${PROJECT_SOURCE_DIR}/DetourTileCache/Include/*.h)
file(GLOB_RECURSE SourceFiles LIST_DIRECTORIES false ${PROJECT_SOURCE_DIR}/DetourTileCache/Source/*.cpp)

target_sources(DetourTileCache 
    PRIVATE ${SourceFiles}
    PUBLIC FILE_SET HEADERS 
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES ${HeaderFiles} )

target_include_directories(DetourTileCache PRIVATE Include)
target_include_directories(DetourTileCache PRIVATE ../Detour/Include)