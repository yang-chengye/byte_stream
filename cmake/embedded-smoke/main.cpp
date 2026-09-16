#include "byte_stream/byte_io.hpp"

#include <array>
#include <cstdint>

namespace {

byte_stream::byte_stream_errc encode(
    byte_stream::byte_writer& writer, uint16_t sequence, uint32_t device_id) noexcept {
    auto status = writer.write(sequence);
    if (status != byte_stream::byte_stream_errc::ok) return status;
    return writer.write(device_id, 3);
}

byte_stream::byte_stream_errc decode(
    byte_stream::byte_reader& reader, uint16_t& sequence, uint32_t& device_id) noexcept {
    auto status = reader.read(sequence);
    if (status != byte_stream::byte_stream_errc::ok) return status;
    return reader.read(device_id, 3);
}

} // namespace

int main() {
    std::array<uint8_t, 8> storage{};
    byte_stream::byte_writer writer(storage.data(), storage.size());
    if (encode(writer, 0x1234, 0x00A1B2C3) != byte_stream::byte_stream_errc::ok) return 1;

    byte_stream::byte_reader reader(writer.view());
    uint16_t sequence = 0;
    uint32_t device_id = 0;
    if (decode(reader, sequence, device_id) != byte_stream::byte_stream_errc::ok) return 2;
    return sequence == 0x1234 && device_id == 0x00A1B2C3 ? 0 : 3;
}
