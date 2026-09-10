#include "byte_stream/byte_stream.hpp"

#include <stdint.h>
#include <iomanip>
#include <iostream>

int main() {
    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set<uint8_t>(0x01);
    stream.set<uint16_t>(0x1234);
    stream.set<uint32_t>(0x00A1B2C3, 3);

    std::cout << "encoded: " << stream.debug_string(true) << '\n';

    const auto version = stream.get<uint8_t>();
    const auto sequence = stream.get<uint16_t>();
    const auto area_code = stream.get<uint32_t>(3);

    std::cout << std::hex << std::showbase;
    std::cout << "version=" << static_cast<int>(version)
              << " sequence=" << sequence
              << " area_code=" << area_code << '\n';
}
