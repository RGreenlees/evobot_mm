file(GLOB_RECURSE HeaderFiles LIST_DIRECTORIES false ${PROJECT_SOURCE_DIR}/Detour/Include/*.h)
file(GLOB_RECURSE SourceFiles LIST_DIRECTORIES false ${PROJECT_SOURCE_DIR}/Detour/Source/*.cpp)

target_sources(Detour 
    PRIVATE ${SourceFiles}
    PUBLIC FILE_SET HEADERS 
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES ${HeaderFiles} )

target_include_directories(Detour PRIVATE Include)