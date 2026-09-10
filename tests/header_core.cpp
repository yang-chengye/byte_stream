#include "byte_stream/byte_stream.hpp"

int main() {
    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set<uint8_t>(0x42);
    if (stream.get_endian() != byte_stream::endian::little) return 1;
    return stream.get<uint8_t>() == 0x42 ? 0 : 1;
}
