#include "benchmark_support.hpp"
#include "byte_stream/byte_stream.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace bench {
namespace {

std::vector<uint8_t> make_payload(std::size_t size) {
    std::vector<uint8_t> payload(size);
    for (std::size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>((i * 131u + 17u) & 0xFFu);
    }
    return payload;
}

std::vector<uint32_t> make_u32_values(std::size_t count) {
    std::vector<uint32_t> values(count);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<uint32_t>(0x10000000u + i * 17u);
    }
    return values;
}

} // namespace

void benchmark_raw_payload() {
    constexpr std::size_t payload_size = 16u * 1024u * 1024u;
    constexpr std::size_t rounds = 8;
    const auto payload = make_payload(payload_size);

    byte_stream::stream stream;
    stream.reserve(payload.size());
    const auto write_result = measure([&] {
        uint64_t checksum = 0;
        for (std::size_t i = 0; i < rounds; ++i) {
            stream.clear();
            stream.set(payload);
            checksum += stream.size();
        }
        return checksum;
    });
    print_throughput("raw vector<uint8_t> write", payload_size * rounds, write_result);

    std::vector<uint8_t> decoded;
    decoded.reserve(payload_size);
    const auto read_result = measure([&] {
        uint64_t checksum = 0;
        for (std::size_t i = 0; i < rounds; ++i) {
            stream.reset_position();
            stream.get_to(decoded, static_cast<uint32_t>(payload_size));
            checksum += decoded.front();
            checksum += decoded.back();
        }
        return checksum;
    });
    print_throughput("raw vector<uint8_t> read", payload_size * rounds, read_result);
}

void benchmark_scalar_vector() {
    constexpr std::size_t value_count = 4'000'000;
    constexpr std::size_t rounds = 4;
    const auto values = make_u32_values(value_count);

    byte_stream::stream stream;
    stream.reserve(values.size() * sizeof(uint32_t));
    const auto write_result = measure([&] {
        uint64_t checksum = 0;
        for (std::size_t i = 0; i < rounds; ++i) {
            stream.clear();
            stream.set(values);
            checksum += stream.size();
        }
        return checksum;
    });
    print_throughput("bulk vector<uint32_t> write",
        value_count * sizeof(uint32_t) * rounds,
        write_result);

    std::vector<uint32_t> decoded;
    decoded.reserve(value_count);
    const auto read_result = measure([&] {
        uint64_t checksum = 0;
        for (std::size_t i = 0; i < rounds; ++i) {
            stream.reset_position();
            stream.get_to(decoded, static_cast<uint32_t>(value_count));
            checksum += decoded.front();
            checksum += decoded.back();
        }
        return checksum;
    });
    print_throughput("bulk vector<uint32_t> read",
        value_count * sizeof(uint32_t) * rounds,
        read_result);
}

} // namespace bench
