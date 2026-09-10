#include "benchmark_support.hpp"
#include "byte_stream/byte_stream.hpp"

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>

namespace bench {

void benchmark_integral_records() {
    constexpr std::size_t record_count = 1'000'000;
    constexpr std::size_t bytes_per_record = 1 + 2 + 3 + 8;

    byte_stream::stream stream;
    stream.reserve(record_count * bytes_per_record);
    const auto write_result = measure([&] {
        stream.clear();
        for (std::size_t i = 0; i < record_count; ++i) {
            stream.set(static_cast<uint8_t>(i));
            stream.set(static_cast<uint16_t>(i));
            stream.set(static_cast<uint32_t>(i), 3);
            stream.set(static_cast<uint64_t>(0x1020304050607080ull + i));
        }
        return static_cast<uint64_t>(stream.size());
    });
    print_rate("fixed integer record write", record_count, write_result);

    const auto read_result = measure([&] {
        stream.reset_position();
        uint64_t checksum = 0;
        for (std::size_t i = 0; i < record_count; ++i) {
            checksum += stream.get<uint8_t>();
            checksum += stream.get<uint16_t>();
            checksum += stream.get<uint32_t>(3);
            checksum += stream.get<uint64_t>();
        }
        return checksum;
    });
    print_rate("fixed integer record read", record_count, read_result);
}

void benchmark_compact_records() {
    constexpr std::size_t record_count = 1'000'000;

    byte_stream::stream stream;
    stream.reserve(record_count * 4);
    const auto write_result = measure([&] {
        stream.clear();
        for (std::size_t i = 0; i < record_count; ++i) {
            stream.set_compact(static_cast<uint32_t>(i & 0x7Fu));
            stream.set_compact(static_cast<int32_t>((i & 1u) == 0 ? 63 : -63));
            stream.set_compact(static_cast<uint16_t>(i & 0x3FFFu));
        }
        return static_cast<uint64_t>(stream.size());
    });
    print_rate("compact integer record write", record_count, write_result);
    if (!csv_output()) {
        std::cout << std::left << std::setw(34) << "compact record bytes"
                  << std::right << std::setw(10) << stream.size()
                  << " total  " << std::setw(10) << std::fixed << std::setprecision(2)
                  << static_cast<double>(stream.size()) / static_cast<double>(record_count)
                  << " B/record\n";
    }

    const auto read_result = measure([&] {
        stream.reset_position();
        uint64_t checksum = 0;
        for (std::size_t i = 0; i < record_count; ++i) {
            checksum += stream.get_compact<uint32_t>();
            checksum += static_cast<uint64_t>(stream.get_compact<int32_t>() + 64);
            checksum += stream.get_compact<uint16_t>();
        }
        return checksum;
    });
    print_rate("compact integer record read", record_count, read_result);
}

} // namespace bench
