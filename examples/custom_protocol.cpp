#include "byte_stream/byte_stream.hpp"

#include <stdint.h>
#include <iostream>
#include <vector>

namespace protocol {

enum class packet_type : uint8_t {
    heartbeat = 1,
    telemetry = 2
};

struct packet {
    uint8_t id{};
    uint32_t area_code{};
    packet_type type{};
    std::vector<uint16_t> values;
};

void to_byte_stream(byte_stream::stream& stream, const packet& value) {
    stream.set(value.id);
    stream.set(value.area_code, 3);
    stream.set(value.type);
    stream.set(static_cast<uint8_t>(value.values.size()));
    stream.set(value.values);
}

void from_byte_stream(const byte_stream::stream& stream, packet& value) {
    value.id = stream.get<uint8_t>();
    value.area_code = stream.get<uint32_t>(3);
    value.type = stream.get<packet_type>();

    const auto value_count = stream.get<uint8_t>();
    stream.get_to(value.values, value_count);
}

} // namespace protocol

int main() {
    const protocol::packet outgoing{
        7,
        0x00112233,
        protocol::packet_type::telemetry,
        { 0x0102, 0x0304 }
    };

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set(outgoing);

    byte_stream::stream input(stream.buffer());
    input.set_endian(byte_stream::endian::little);
    protocol::packet incoming;
    input.get_to(incoming);

    std::cout << "encoded: " << stream.debug_string(true) << '\n';
    std::cout << "decoded id=" << static_cast<int>(incoming.id)
              << " values=" << incoming.values.size() << '\n';
}
