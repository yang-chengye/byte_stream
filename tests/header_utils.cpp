#include "byte_stream/byte_stream_utils.hpp"

#include <array>

int main() {
    const std::array<byte_stream::byte_stream_utils::byte_escape_rule, 1> rules{ {
        { 0xC0, { 0xDB, 0xDC } },
    } };
    const auto escaped = byte_stream::byte_stream_utils::escape_bytes({ 0xC0 }, rules);
    return escaped == std::vector<uint8_t>({ 0xDB, 0xDC }) ? 0 : 1;
}
