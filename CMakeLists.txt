cmake_minimum_required(VERSION 3.24)

project(
    DAVSyncTogether
    VERSION 0.2.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CommonLibSSE REQUIRED)

add_commonlibsse_plugin(${PROJECT_NAME}
    SOURCES
        src/PCH.h
        src/main.cpp
        src/DAVProbe.cpp
        src/DAVProbe.h
        src/DAVStateSnapshot.cpp
        src/DAVStateSnapshot.h
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        DAVST_VERSION_STRING="${PROJECT_VERSION}"
)
