#include "byte_stream/byte_io.hpp"

#include <array>
#include <cstdint>

int main() {
    std::array<uint8_t, 32> storage{};
    byte_stream::byte_writer writer(storage.data(), storage.size());
    if (writer.write<uint16_t>(0x1234) != byte_stream::byte_stream_errc::ok ||
        writer.write<uint32_t>(0x00A1B2C3, 3) != byte_stream::byte_stream_errc::ok) {
        return 1;
    }

    byte_stream::byte_reader reader(writer.view());
    uint16_t sequence = 0;
    uint32_t device_id = 0;
    if (reader.read(sequence) != byte_stream::byte_stream_errc::ok ||
        reader.read(device_id, 3) != byte_stream::byte_stream_errc::ok) {
        return 2;
    }
    return sequence == 0x1234 && device_id == 0x00A1B2C3 ? 0 : 3;
}
