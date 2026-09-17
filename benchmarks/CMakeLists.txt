add_executable(byte_stream_performance
    performance.cpp
    performance_bulk.cpp
    performance_custom.cpp
    performance_integers.cpp
    performance_io.cpp
)
target_link_libraries(byte_stream_performance PRIVATE byte_stream::byte_stream)
byte_stream_configure_development_target(byte_stream_performance)
target_compile_definitions(byte_stream_performance
    PRIVATE $<$<CONFIG:Debug>:BYTE_STREAM_BENCHMARK_DEBUG_BUILD>
)

add_executable(byte_stream_coverage_benchmark coverage.cpp coverage_assignment.cpp coverage_macros.cpp)
target_link_libraries(byte_stream_coverage_benchmark PRIVATE byte_stream::byte_stream)
byte_stream_configure_development_target(byte_stream_coverage_benchmark)
target_compile_definitions(byte_stream_coverage_benchmark
    PRIVATE $<$<CONFIG:Debug>:BYTE_STREAM_BENCHMARK_DEBUG_BUILD>
)
