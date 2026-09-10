#include "benchmark_support.hpp"

#include <cstring>
#include <iostream>

namespace bench {

volatile uint64_t sink = 0;

void benchmark_raw_payload();
void benchmark_scalar_vector();
void benchmark_integral_records();
void benchmark_compact_records();
void benchmark_complex_protocol();
void benchmark_fixed_buffer_io();

} // namespace bench

int main(int argc, char** argv) {
    bool csv = false;
    if (argc == 2 && std::strcmp(argv[1], "--csv") == 0) {
        csv = true;
    }
    else if (argc != 1) {
        std::cerr << "usage: byte_stream_performance [--csv]\n";
        return 2;
    }

#ifdef BYTE_STREAM_BENCHMARK_DEBUG_BUILD
    if (!csv) {
        std::cout << "warning: benchmark is running in a debug build\n";
    }
#endif

    bench::set_csv_output(csv);
    bench::print_header("byte_stream performance benchmark");
    bench::benchmark_raw_payload();
    bench::benchmark_scalar_vector();
    bench::benchmark_integral_records();
    bench::benchmark_compact_records();
    bench::benchmark_complex_protocol();
    bench::benchmark_fixed_buffer_io();
    if (!csv) {
        std::cout << "sink=" << bench::sink << '\n';
    }
}
