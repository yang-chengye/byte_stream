#include "byte_stream/byte_stream.hpp"

#include <array>
#include <stdint.h>
#include <iostream>

namespace protocol {

struct header {
    uint8_t version{};
    uint16_t sequence{};
    uint32_t area_code{};
    std::array<uint8_t, 4> magic{};
};

BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE(header, version, sequence, (area_code, 3), magic)

class private_header {
public:
    private_header() = default;

    private_header(uint8_t flags, uint32_t area_code)
        : flags_(flags),
          area_code_(area_code) {
    }

    uint8_t flags() const noexcept { return flags_; }
    uint32_t area_code() const noexcept { return area_code_; }

private:
    uint8_t flags_{};
    uint32_t area_code_{};

    BYTE_STREAM_DEFINE_TYPE_INTRUSIVE(private_header, flags_, (area_code_, 3))
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

    std::cout << "encoded: " << stream.debug_string(true) << '\n';
    std::cout << "header version=" << static_cast<int>(incoming_header.version)
              << " sequence=" << incoming_header.sequence << '\n';
    std::cout << "private flags=" << static_cast<int>(incoming_private.flags())
              << " area_code=" << incoming_private.area_code() << '\n';
}
