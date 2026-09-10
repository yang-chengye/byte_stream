#include "byte_stream/byte_stream.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <stdint.h>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace property_sample {

struct packet {
    uint32_t sequence{};
    int16_t temperature{};
    std::vector<uint8_t> payload;
};

bool operator==(const packet& lhs, const packet& rhs) {
    return lhs.sequence == rhs.sequence &&
        lhs.temperature == rhs.temperature &&
        lhs.payload == rhs.payload;
}

void to_byte_stream(byte_stream::stream& stream, const packet& value) {
    stream.set(value.sequence);
    stream.set(value.temperature);
    stream.set_with_size(value.payload);
}

void from_byte_stream(const byte_stream::stream& stream, packet& value) {
    value.sequence = stream.get<uint32_t>();
    value.temperature = stream.get<int16_t>();
    stream.get_to_with_size(value.payload, 4096);
}

uint64_t low_byte_mask(size_t byte_count) {
    return byte_count == sizeof(uint64_t)
        ? (std::numeric_limits<uint64_t>::max)()
        : (uint64_t{ 1 } << (byte_count * 8)) - 1;
}

int64_t expected_signed_value(uint64_t raw, size_t byte_count) {
    const uint64_t mask = low_byte_mask(byte_count);
    uint64_t extended = raw & mask;
    if (byte_count != sizeof(uint64_t)) {
        const uint64_t sign_bit = uint64_t{ 1 } << (byte_count * 8 - 1);
        if ((extended & sign_bit) != 0) {
            extended |= ~mask;
        }
    }

    int64_t result = 0;
    std::memcpy(&result, &extended, sizeof(result));
    return result;
}

} // namespace property_sample

TEST(ByteStreamPropertyTest, RandomFixedWidthIntegersRoundTripAcrossWidthsAndEndian) {
    std::mt19937_64 random{ 0x627974655f737472ull };

    for (const auto byte_order : { byte_stream::endian::little, byte_stream::endian::big }) {
        byte_stream::stream stream;
        stream.reserve(sizeof(uint64_t));

        for (size_t iteration = 0; iteration < 20'000; ++iteration) {
            const uint64_t raw = random();
            const auto byte_count = static_cast<uint32_t>(iteration % sizeof(uint64_t) + 1);

            stream.clear();
            stream.set_endian(byte_order);
            stream.set(raw, byte_count);
            EXPECT_EQ(stream.get<uint64_t>(byte_count),
                raw & property_sample::low_byte_mask(byte_count));

            stream.clear();
            stream.set_endian(byte_order);
            int64_t signed_value = 0;
            std::memcpy(&signed_value, &raw, sizeof(signed_value));
            stream.set(signed_value, byte_count);
            EXPECT_EQ(stream.get<int64_t>(byte_count),
                property_sample::expected_signed_value(raw, byte_count));
        }
    }
}

TEST(ByteStreamPropertyTest, CompactInt16RoundTripsExhaustively) {
    byte_stream::stream stream;
    stream.reserve(400'000);

    for (uint32_t raw = 0; raw <= (std::numeric_limits<uint16_t>::max)(); ++raw) {
        stream.set_compact(static_cast<uint16_t>(raw));
    }
    for (int32_t value = (std::numeric_limits<int16_t>::min)();
         value <= (std::numeric_limits<int16_t>::max)(); ++value) {
        stream.set_compact(static_cast<int16_t>(value));
    }

    for (uint32_t raw = 0; raw <= (std::numeric_limits<uint16_t>::max)(); ++raw) {
        ASSERT_EQ(stream.get_compact<uint16_t>(), static_cast<uint16_t>(raw));
    }
    for (int32_t value = (std::numeric_limits<int16_t>::min)();
         value <= (std::numeric_limits<int16_t>::max)(); ++value) {
        ASSERT_EQ(stream.get_compact<int16_t>(), static_cast<int16_t>(value));
    }
    EXPECT_TRUE(stream.eof());
}

TEST(ByteStreamPropertyTest, FloatingPointBitPatternsRoundTripExactly) {
    std::mt19937 random{ 0x42595445u };

    for (const auto byte_order : { byte_stream::endian::little, byte_stream::endian::big }) {
        byte_stream::stream stream;
        stream.reserve(sizeof(float));

        for (size_t iteration = 0; iteration < 20'000; ++iteration) {
            const uint32_t input_bits = random();
            float input = 0.0f;
            std::memcpy(&input, &input_bits, sizeof(input));

            stream.clear();
            stream.set_endian(byte_order);
            stream.set(input);
            const float output = stream.get<float>();

            uint32_t output_bits = 0;
            std::memcpy(&output_bits, &output, sizeof(output));
            EXPECT_EQ(output_bits, input_bits);
        }
    }
}

TEST(ByteStreamPropertyTest, EveryTruncatedPacketPrefixFailsAtomically) {
    const property_sample::packet expected{
        0x10203040u,
        -273,
        { 0x00, 0x01, 0x7F, 0x80, 0xC0, 0xDB, 0xFF }
    };
    const property_sample::packet sentinel{
        0xAABBCCDDu,
        123,
        { 0x55, 0xAA }
    };

    byte_stream::stream encoded;
    encoded.set(expected);

    for (size_t prefix_size = 0; prefix_size < encoded.size(); ++prefix_size) {
        byte_stream::stream truncated(encoded.data(), prefix_size);
        auto decoded = sentinel;
        EXPECT_THROW(truncated.get_to_atomic(decoded), byte_stream::byte_stream_error);
        EXPECT_EQ(decoded, sentinel);
        EXPECT_EQ(truncated.position(), 0U);
    }

    auto decoded = sentinel;
    encoded.get_to_atomic(decoded);
    EXPECT_EQ(decoded, expected);
    EXPECT_TRUE(encoded.eof());
}

TEST(ByteStreamPropertyTest, RandomSizePrefixedByteVectorsRoundTripWithinLimit) {
    std::mt19937 random{ 0x5354524Du };
    std::uniform_int_distribution<size_t> length_distribution(0, 4096);
    std::uniform_int_distribution<unsigned int> byte_distribution(0, 255);

    for (size_t iteration = 0; iteration < 1'000; ++iteration) {
        std::vector<uint8_t> input(length_distribution(random));
        for (auto& byte : input) {
            byte = static_cast<uint8_t>(byte_distribution(random));
        }

        byte_stream::stream stream;
        stream.reserve(input.size() + 2);
        stream.set_with_size(input);

        std::vector<uint8_t> output;
        stream.get_to_with_size(output, 4096);
        EXPECT_EQ(output, input);
        EXPECT_TRUE(stream.eof());
    }
}
