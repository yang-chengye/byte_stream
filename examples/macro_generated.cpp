#include "byte_stream/byte_stream.hpp"

#include <array>
#include <stdint.h>
#include <iostream>

namespace protocol {

struct header {
    uint8_t version{};
    uint16_t sequence{};
    uint32_t device_id{};
    std::array<uint8_t, 4> magic{};
};

BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE(header, version, sequence, (device_id, 3), magic)

class private_header {
public:
    private_header() = default;

    private_header(uint8_t flags, uint32_t device_id)
        : flags_(flags),
          device_id_(device_id) {
    }

    uint8_t flags() const noexcept { return flags_; }
    uint32_t device_id() const noexcept { return device_id_; }

private:
    uint8_t flags_{};
    uint32_t device_id_{};

    BYTE_STREAM_DEFINE_TYPE_INTRUSIVE(private_header, flags_, (device_id_, 3))
};

} // namespace protocol

int main() {
    const protocol::header outgoing_header{
        1,
        0x1234,
        0x00112233,
        { 0x42, 0x53, 0x00, 0x01 }
    };
    const protocol::private_header outgoing_private{ 0xA5, 0x00445566 };

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set(outgoing_header);
    stream.set(outgoing_private);

    protocol::header incoming_header;
    protocol::private_header incoming_private;
    stream.get_to(incoming_header);
    stream.get_to(incoming_private);

    std::cout << "encoded: " << stream.to_hex(true) << '\n';
    std::cout << "header version=" << static_cast<int>(incoming_header.version)
              << " sequence=" << incoming_header.sequence << '\n';
    std::cout << "private flags=" << static_cast<int>(incoming_private.flags())
              << " device_id=" << incoming_private.device_id() << '\n';
}
