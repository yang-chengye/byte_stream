#include "byte_stream/byte_io.hpp"

#include <array>
#include <cstdint>

int main() {
    std::array<uint8_t, 8> storage{};
    byte_stream::byte_writer writer(storage.data(), storage.size());
    if (writer.write<uint32_t>(0x12345678u) != byte_stream::byte_stream_errc::ok) return 1;

    byte_stream::byte_reader reader(writer.view());
    uint32_t value = 0;
    if (reader.read(value) != byte_stream::byte_stream_errc::ok) return 2;
    return value == 0x12345678u ? 0 : 3;
}
