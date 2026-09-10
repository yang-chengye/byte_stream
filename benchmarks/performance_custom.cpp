#include "benchmark_support.hpp"
#include "byte_stream/byte_stream.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace bench {
namespace custom {

struct packet {
    uint16_t sequence{};
    uint32_t area_code{};
    uint8_t type{};
    std::array<uint16_t, 8> values{};
    std::optional<uint16_t> quality;
    std::vector<uint8_t> payload;
};

void to_byte_stream(byte_stream::stream& stream, const packet& value) {
    stream.set(value.sequence);
    stream.set(value.area_code, 3);
    stream.set(value.type);
    stream.set(value.values);
    stream.set(value.quality);
    stream.set(static_cast<uint16_t>(value.payload.size()));
    stream.set(value.payload);
}

void from_byte_stream(const byte_stream::stream& stream, packet& value) {
    value.sequence = stream.get<uint16_t>();
    value.area_code = stream.get<uint32_t>(3);
    value.type = stream.get<uint8_t>();
    stream.get_to(value.values);
    stream.get_to(value.quality);
    const auto payload_size = stream.get<uint16_t>();
    stream.get_to(value.payload, payload_size);
}

} // namespace custom
namespace {

std::vector<uint8_t> make_payload(std::size_t size) {
    std::vector<uint8_t> payload(size);
    for (std::size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>((i * 131u + 17u) & 0xFFu);
    }
    return payload;
}

std::vector<custom::packet> make_packets(std::size_t count) {
    std::vector<custom::packet> packets;
    packets.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        custom::packet value;
        value.sequence = static_cast<uint16_t>(i);
        value.area_code = 0x00110000u | static_cast<uint32_t>(i & 0xFFFFu);
        value.type = static_cast<uint8_t>(i & 0x7Fu);
        for (std::size_t j = 0; j < value.values.size(); ++j) {
            value.values[j] = static_cast<uint16_t>(i + j * 3u);
        }
        if ((i & 1u) == 0) {
            value.quality = static_cast<uint16_t>(i & 0xFFFFu);
        }
        value.payload = make_payload(32);
        packets.push_back(std::move(value));
    }
    return packets;
}

} // namespace

void benchmark_complex_protocol() {
    constexpr std::size_t packet_count = 200'000;
    const auto packets = make_packets(packet_count);

    byte_stream::stream stream;
    stream.reserve(packet_count * 64);
    const auto write_result = measure([&] {
        stream.clear();
        for (const auto& packet : packets) {
            stream.set(packet);
        }
        return static_cast<uint64_t>(stream.size());
    });
    print_rate("custom packet write", packet_count, write_result);
    print_throughput("custom packet write bytes", stream.size(), write_result);

    custom::packet decoded;
    const auto read_result = measure([&] {
        stream.reset_position();
        uint64_t checksum = 0;
        for (std::size_t i = 0; i < packet_count; ++i) {
            stream.get_to(decoded);
            checksum += decoded.sequence;
            checksum += decoded.payload.size();
        }
        return checksum;
    });
    print_rate("custom packet read", packet_count, read_result);
    print_throughput("custom packet read bytes", stream.size(), read_result);
}

} // namespace bench
