include(GoogleTest)

set(BUILD_GMOCK OFF CACHE BOOL "Build GoogleMock" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Install GoogleTest" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "Use the shared MSVC runtime" FORCE)
add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/googletest"
    "${CMAKE_CURRENT_BINARY_DIR}/googletest"
    EXCLUDE_FROM_ALL
)

add_executable(byte_stream_tests main.cpp property_tests.cpp byte_io.cpp macro_limits.cpp)
target_link_libraries(byte_stream_tests PRIVATE byte_stream::byte_stream GTest::gtest_main)
byte_stream_configure_development_target(byte_stream_tests)

gtest_discover_tests(byte_stream_tests DISCOVERY_MODE PRE_TEST)

add_executable(byte_stream_header_core header_core.cpp)
target_link_libraries(byte_stream_header_core PRIVATE byte_stream::byte_stream)
byte_stream_configure_development_target(byte_stream_header_core)
add_test(NAME byte_stream_header_core COMMAND byte_stream_header_core)

add_executable(byte_stream_header_utils header_utils.cpp)
target_link_libraries(byte_stream_header_utils PRIVATE byte_stream::byte_stream)
byte_stream_configure_development_target(byte_stream_header_utils)
add_test(NAME byte_stream_header_utils COMMAND byte_stream_header_utils)

add_executable(byte_stream_header_no_exceptions header_no_exceptions.cpp)
target_link_libraries(byte_stream_header_no_exceptions PRIVATE byte_stream::byte_stream)
byte_stream_configure_development_target(byte_stream_header_no_exceptions)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(byte_stream_header_no_exceptions PRIVATE -fno-exceptions)
endif()
add_test(NAME byte_stream_header_no_exceptions COMMAND byte_stream_header_no_exceptions)

add_executable(byte_stream_adjacent_allocations adjacent_allocations.cpp)
target_link_libraries(byte_stream_adjacent_allocations PRIVATE byte_stream::byte_stream)
byte_stream_configure_development_target(byte_stream_adjacent_allocations)
add_test(NAME byte_stream_adjacent_allocations COMMAND byte_stream_adjacent_allocations)
