#include "benchmark_support.hpp"
#include "byte_stream/byte_io.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace bench {

void benchmark_fixed_buffer_io() {
    constexpr std::size_t record_count = 1'000'000;
    constexpr std::size_t bytes_per_record = 1 + 2 + 3 + 8;
    std::vector<uint8_t> storage(record_count * bytes_per_record);

    byte_stream::byte_writer writer(storage.data(), storage.size());
    const auto write_result = measure([&] {
        writer.clear();
        for (std::size_t i = 0; i < record_count; ++i) {
            if (writer.write(static_cast<uint8_t>(i)) != byte_stream::byte_stream_errc::ok ||
                writer.write(static_cast<uint16_t>(i)) != byte_stream::byte_stream_errc::ok ||
                writer.write(static_cast<uint32_t>(i), 3) != byte_stream::byte_stream_errc::ok ||
                writer.write(static_cast<uint64_t>(0x1020304050607080ull + i)) !=
                    byte_stream::byte_stream_errc::ok) {
                return (std::numeric_limits<uint64_t>::max)();
            }
        }
        return static_cast<uint64_t>(writer.size());
    });
    print_rate("fixed-buffer record write", record_count, write_result);

    byte_stream::byte_reader reader(writer.view());
    const auto read_result = measure([&] {
        reader.reset();
        uint64_t checksum = 0;
        for (std::size_t i = 0; i < record_count; ++i) {
            uint8_t first = 0;
            uint16_t second = 0;
            uint32_t third = 0;
            uint64_t fourth = 0;
            if (reader.read(first) != byte_stream::byte_stream_errc::ok ||
                reader.read(second) != byte_stream::byte_stream_errc::ok ||
                reader.read(third, 3) != byte_stream::byte_stream_errc::ok ||
                reader.read(fourth) != byte_stream::byte_stream_errc::ok) {
                return (std::numeric_limits<uint64_t>::max)();
            }
            checksum += first + second + third + fourth;
        }
        return checksum;
    });
    print_rate("fixed-buffer record read", record_count, read_result);
}

} // namespace bench
