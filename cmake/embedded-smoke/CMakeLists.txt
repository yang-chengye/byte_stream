cmake_minimum_required(VERSION 3.16)

project(byte_stream_embedded_smoke LANGUAGES CXX)

include(CTest)

if(NOT DEFINED BYTE_STREAM_SOURCE_DIR)
    get_filename_component(BYTE_STREAM_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

set(BYTE_STREAM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BYTE_STREAM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BYTE_STREAM_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
add_subdirectory("${BYTE_STREAM_SOURCE_DIR}" byte_stream EXCLUDE_FROM_ALL)

add_executable(byte_stream_embedded_smoke main.cpp)
target_link_libraries(byte_stream_embedded_smoke PRIVATE byte_stream::byte_stream)
set_target_properties(byte_stream_embedded_smoke PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
if(MSVC)
    target_compile_options(byte_stream_embedded_smoke PRIVATE /W4 /WX)
else()
    target_compile_options(byte_stream_embedded_smoke PRIVATE
        -Wall -Wextra -Wpedantic -Werror
        -fno-exceptions -fno-rtti
        -ffunction-sections -fdata-sections
    )
endif()

add_test(NAME byte_stream_embedded_smoke COMMAND byte_stream_embedded_smoke)
