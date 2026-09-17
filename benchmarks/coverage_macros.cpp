#include "byte_stream/byte_stream.hpp"
#include "benchmark_support.hpp"

#include <array>

namespace bench {
namespace {

struct macro_packet {
    std::array<uint32_t, 64> values{};
};

BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE(macro_packet,
    values[0], values[1], values[2], values[3],
    values[4], values[5], values[6], values[7],
    values[8], values[9], values[10], values[11],
    values[12], values[13], values[14], values[15],
    values[16], values[17], values[18], values[19],
    values[20], values[21], values[22], values[23],
    values[24], values[25], values[26], values[27],
    values[28], values[29], values[30], values[31],
    values[32], values[33], values[34], values[35],
    values[36], values[37], values[38], values[39],
    values[40], values[41], values[42], values[43],
    values[44], values[45], values[46], values[47],
    values[48], values[49], values[50], values[51],
    values[52], values[53], values[54], values[55],
    values[56], values[57], values[58], values[59],
    values[60], values[61], values[62], values[63])

} // namespace

void benchmark_macro_fields() {
    constexpr std::size_t iterations = 100'000;
    macro_packet value;
    for (size_t i = 0; i < value.values.size(); ++i) {
        value.values[i] = static_cast<uint32_t>(0x12345600u + i);
    }
    for (const auto order : {byte_stream::endian::little, byte_stream::endian::big}) {
        byte_stream::stream stream(order);
        stream.reserve(256);
        const auto writes = measure([&] {
            uint64_t checksum = 0;
            for (size_t i = 0; i < iterations; ++i) {
                value.values[0] = static_cast<uint32_t>(i);
                stream.clear();
                stream.set(value);
                checksum += stream[i % stream.size()];
            }
            return checksum;
        });
        macro_packet decoded;
        const auto reads = measure([&] {
            uint64_t checksum = 0;
            for (size_t i = 0; i < iterations; ++i) {
                stream.reset_position();
                stream.get_to(decoded);
                checksum += decoded.values[i % decoded.values.size()];
            }
            return checksum;
        });
        const std::string suffix = order == byte_stream::endian::big ? " big" : " little";
        print_rate("macro 64 fields write" + suffix, iterations, writes);
        print_rate("macro 64 fields read" + suffix, iterations, reads);
    }
}

} // namespace bench
