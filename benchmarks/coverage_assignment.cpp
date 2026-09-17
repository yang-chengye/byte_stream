#include "byte_stream/byte_stream.hpp"
#include "benchmark_support.hpp"

#include <array>
#include <vector>

namespace bench {
namespace {

struct assignment_packet {
    uint16_t sequence{};
    std::array<uint32_t, 8> values{};
    std::vector<uint8_t> payload;
};

void to_byte_stream(byte_stream::stream& stream, const assignment_packet& value) {
    stream.set(value.sequence);
    stream.set(value.values);
    stream.set_with_size(value.payload);
}

template <bool Assign>
void benchmark_replacement(byte_stream::endian order) {
    constexpr std::size_t iterations = 300'000;
    assignment_packet value{0, {0x12345678, 0xABCDEF01}, std::vector<uint8_t>(16, 0xAB)};
    byte_stream::stream stream(order);
    stream.reserve(128);
    const auto result = measure([&] {
        uint64_t checksum = 0;
        for (std::size_t i = 0; i < iterations; ++i) {
            value.sequence = static_cast<uint16_t>(i);
            if constexpr (Assign) {
                stream = value;
            } else {
                stream.clear();
                stream.set(value);
            }
            checksum += stream.size() + stream[0] + stream[1];
        }
        return checksum;
    });
    const std::string name = Assign ? "object assign " : "object clear/set ";
    print_rate(name + (order == byte_stream::endian::big ? "big" : "little"),
        iterations, result);
}

} // namespace

void benchmark_object_assignment() {
    for (const auto order : { byte_stream::endian::little, byte_stream::endian::big }) {
        benchmark_replacement<false>(order);
        benchmark_replacement<true>(order);
    }
}

} // namespace bench
