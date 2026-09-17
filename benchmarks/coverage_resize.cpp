#include "byte_stream/byte_stream.hpp"
#include "benchmark_support.hpp"

namespace bench {

void benchmark_buffer_resize() {
    for (const std::size_t size : {std::size_t{172}, std::size_t{4096}, std::size_t{262144}}) {
        const std::size_t iterations = size > 4096 ? 1000 : 100'000;
        byte_stream::stream stream;
        stream.reserve(size);
        const auto result = measure([&] {
            uint64_t checksum = 0;
            for (std::size_t i = 0; i < iterations; ++i) {
                stream.clear();
                stream.resize(size, static_cast<uint8_t>(i));
                checksum += stream.size() + stream[i % size];
            }
            return checksum;
        });
        print_rate("resize reused " + std::to_string(size), iterations, result);
    }

    // This workload explicitly measures construction and allocation together.
    constexpr std::size_t iterations = 100'000;
    const auto construction = measure([&] {
        uint64_t checksum = 0;
        for (std::size_t i = 0; i < iterations; ++i) {
            byte_stream::stream stream(172, static_cast<uint8_t>(i));
            checksum += stream.size() + stream[i % stream.size()];
        }
        return checksum;
    });
    print_rate("construct filled 172", iterations, construction);
}

} // namespace bench
