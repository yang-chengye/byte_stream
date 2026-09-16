cmake_minimum_required(VERSION 3.16)

project(
    byte_stream
    VERSION 0.1.1
    DESCRIPTION "Header-only C++17 binary byte stream encoder and decoder"
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(BYTE_STREAM_TOP_LEVEL ON)
else()
    set(BYTE_STREAM_TOP_LEVEL OFF)
endif()

option(BYTE_STREAM_BUILD_TESTS "Build byte_stream tests" ${BYTE_STREAM_TOP_LEVEL})
option(BYTE_STREAM_BUILD_EXAMPLES "Build byte_stream examples" ${BYTE_STREAM_TOP_LEVEL})
option(BYTE_STREAM_BUILD_BENCHMARKS "Build byte_stream performance benchmarks" ${BYTE_STREAM_TOP_LEVEL})
option(BYTE_STREAM_ENABLE_WARNINGS "Enable strict warnings for byte_stream development targets" ${BYTE_STREAM_TOP_LEVEL})
option(BYTE_STREAM_WARNINGS_AS_ERRORS "Treat byte_stream development warnings as errors" OFF)
option(BYTE_STREAM_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(BYTE_STREAM_ENABLE_COVERAGE "Enable GCC/Clang source coverage instrumentation" OFF)

add_library(byte_stream INTERFACE)
add_library(byte_stream::byte_stream ALIAS byte_stream)

target_include_directories(byte_stream INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(byte_stream INTERFACE cxx_std_17)
target_compile_options(byte_stream INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
)

function(byte_stream_configure_development_target target_name)
    set_target_properties(${target_name} PROPERTIES CXX_EXTENSIONS OFF)

    if(BYTE_STREAM_ENABLE_WARNINGS)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /W4 /permissive-)
            if(BYTE_STREAM_WARNINGS_AS_ERRORS)
                target_compile_options(${target_name} PRIVATE /WX)
            endif()
        else()
            target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
            if(BYTE_STREAM_WARNINGS_AS_ERRORS)
                target_compile_options(${target_name} PRIVATE -Werror)
            endif()
        endif()
    endif()

    if(BYTE_STREAM_ENABLE_SANITIZERS)
        if(MSVC)
            message(FATAL_ERROR "BYTE_STREAM_ENABLE_SANITIZERS is supported with GCC or Clang")
        endif()
        target_compile_options(${target_name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
    endif()

    if(BYTE_STREAM_ENABLE_COVERAGE)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            message(FATAL_ERROR "BYTE_STREAM_ENABLE_COVERAGE requires GCC or Clang")
        endif()
        target_compile_options(${target_name} PRIVATE -O0 -g --coverage)
        target_link_options(${target_name} PRIVATE --coverage)
    endif()
endfunction()

if(BYTE_STREAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BYTE_STREAM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BYTE_STREAM_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
