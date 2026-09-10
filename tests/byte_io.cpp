#include "byte_stream/byte_io.hpp"
#include "byte_stream/byte_stream.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {

enum class message_type : uint16_t {
    data = 0x1234
};

TEST(ByteIoTest, WritesAndReadsScalarsWithoutOwningStorage) {
    std::array<uint8_t, 64> storage{};
    byte_stream::byte_writer writer(storage.data(), storage.size());

    EXPECT_EQ(writer.write<uint16_t>(0x1234), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(writer.write<int32_t>(-2, 3), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(writer.write(message_type::data), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(writer.write(1.25f), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(writer.write(true), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(writer.write(std::byte{ 0xA5 }), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(writer.write_compact<int32_t>(-63), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(writer.write_compact<uint32_t>(300), byte_stream::byte_stream_errc::ok);

    byte_stream::byte_reader reader(writer.view());
    uint16_t word = 0;
    int32_t partial = 0;
    message_type type{};
    float number = 0.0f;
    bool flag = false;
    std::byte byte{};
    int32_t compact_signed = 0;
    uint32_t compact_unsigned = 0;

    EXPECT_EQ(reader.read(word), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(reader.read(partial, 3), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(reader.read(type), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(reader.read(number), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(reader.read(flag), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(reader.read(byte), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(reader.read_compact(compact_signed), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(reader.read_compact(compact_unsigned), byte_stream::byte_stream_errc::ok);

    EXPECT_EQ(word, 0x1234);
    EXPECT_EQ(partial, -2);
    EXPECT_EQ(type, message_type::data);
    EXPECT_EQ(number, 1.25f);
    EXPECT_TRUE(flag);
    EXPECT_EQ(byte, std::byte{ 0xA5 });
    EXPECT_EQ(compact_signed, -63);
    EXPECT_EQ(compact_unsigned, 300u);
    EXPECT_TRUE(reader.eof());
}

TEST(ByteIoTest, UsesCanonicalBigEndianBytes) {
    std::array<uint8_t, 16> storage{};
    byte_stream::byte_writer writer(storage.data(), storage.size(), byte_stream::endian::big);

    ASSERT_EQ(writer.write<uint32_t>(0x00123456u, 3), byte_stream::byte_stream_errc::ok);
    ASSERT_EQ(writer.write<uint16_t>(0xABCDu), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(writer.size(), 5u);
    EXPECT_EQ(storage[0], 0x12);
    EXPECT_EQ(storage[1], 0x34);
    EXPECT_EQ(storage[2], 0x56);
    EXPECT_EQ(storage[3], 0xAB);
    EXPECT_EQ(storage[4], 0xCD);

    byte_stream::byte_reader reader(writer.view(), byte_stream::endian::big);
    uint32_t first = 0;
    uint16_t second = 0;
    EXPECT_EQ(reader.read(first, 3), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(reader.read(second), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(first, 0x00123456u);
    EXPECT_EQ(second, 0xABCDu);
}

TEST(ByteIoTest, PreservesStateWhenStorageIsInsufficient) {
    std::array<uint8_t, 2> storage{{ 0xA5, 0xA5 }};
    byte_stream::byte_writer writer(storage.data(), storage.size());

    EXPECT_EQ(writer.write<uint32_t>(0x12345678u), byte_stream::byte_stream_errc::insufficient_space);
    EXPECT_EQ(writer.size(), 0u);
    EXPECT_EQ(storage[0], 0xA5);
    EXPECT_EQ(storage[1], 0xA5);

    ASSERT_EQ(writer.write<uint8_t>(0x11), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(writer.write<uint16_t>(0x2233), byte_stream::byte_stream_errc::insufficient_space);
    EXPECT_EQ(writer.size(), 1u);
    EXPECT_EQ(storage[1], 0xA5);

    const std::array<uint8_t, 1> input{{ 0x44 }};
    byte_stream::byte_reader reader(input.data(), input.size());
    uint16_t output = 0xBEEF;
    EXPECT_EQ(reader.read(output), byte_stream::byte_stream_errc::insufficient_data);
    EXPECT_EQ(reader.position(), 0u);
    EXPECT_EQ(output, 0xBEEF);
}

TEST(ByteIoTest, PatchesWrittenBytesWithoutTruncatingTheLogicalBuffer) {
    std::array<uint8_t, 8> storage{};
    byte_stream::byte_writer writer(storage.data(), storage.size());
    ASSERT_EQ(writer.write<uint32_t>(0xAABBCCDDu), byte_stream::byte_stream_errc::ok);
    ASSERT_EQ(writer.write<uint16_t>(0x1122u), byte_stream::byte_stream_errc::ok);
    ASSERT_EQ(writer.size(), 6u);

    ASSERT_TRUE(writer.seek(1));
    ASSERT_EQ(writer.write<uint8_t>(0x7Fu), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(writer.position(), 2u);
    EXPECT_EQ(writer.size(), 6u);
    EXPECT_EQ(writer.view().size(), 6u);
    EXPECT_FALSE(writer.seek(7));

    writer.clear();
    EXPECT_EQ(writer.position(), 0u);
    EXPECT_EQ(writer.size(), 0u);
}

TEST(ByteIoTest, RejectsInvalidValuesAndCompactEncodingsAtomically) {
    {
        const std::array<uint8_t, 1> input{{ 0x02 }};
        byte_stream::byte_reader reader(input.data(), input.size());
        bool output = true;
        EXPECT_EQ(reader.read(output), byte_stream::byte_stream_errc::invalid_value);
        EXPECT_EQ(reader.position(), 0u);
        EXPECT_TRUE(output);
    }
    {
        const std::array<uint8_t, 2> input{{ 0x80, 0x00 }};
        byte_stream::byte_reader reader(input.data(), input.size());
        uint32_t output = 7;
        EXPECT_EQ(reader.read_compact(output), byte_stream::byte_stream_errc::non_canonical_encoding);
        EXPECT_EQ(reader.position(), 0u);
        EXPECT_EQ(output, 7u);
    }
    {
        const std::array<uint8_t, 1> input{{ 0x80 }};
        byte_stream::byte_reader reader(input.data(), input.size());
        uint32_t output = 7;
        EXPECT_EQ(reader.read_compact(output), byte_stream::byte_stream_errc::insufficient_data);
        EXPECT_EQ(reader.position(), 0u);
        EXPECT_EQ(output, 7u);
    }
}

TEST(ByteIoTest, ReturnsViewsIntoTheOriginalInput) {
    std::array<uint8_t, 4> input{{ 1, 2, 3, 4 }};
    byte_stream::byte_reader reader(input.data(), input.size());
    uint8_t prefix = 0;
    ASSERT_EQ(reader.read(prefix), byte_stream::byte_stream_errc::ok);

    byte_stream::byte_view payload;
    ASSERT_EQ(reader.read_view(2, payload), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(payload.data(), input.data() + 1);
    EXPECT_EQ(payload.size(), 2u);
    input[1] = 0xAA;
    EXPECT_EQ(payload.data()[0], 0xAA);
    EXPECT_EQ(reader.position(), 3u);
}

TEST(ByteIoTest, HandlesNullAndEmptyRangesPredictably) {
    byte_stream::byte_writer writer(nullptr, 0);
    EXPECT_EQ(writer.write_bytes(nullptr, 0), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(writer.write<uint8_t>(1), byte_stream::byte_stream_errc::null_pointer);

    byte_stream::byte_reader reader(nullptr, 0);
    byte_stream::byte_view view(reinterpret_cast<const uint8_t*>(1), 1);
    EXPECT_EQ(reader.read_view(0, view), byte_stream::byte_stream_errc::ok);
    EXPECT_EQ(view.data(), nullptr);
    EXPECT_EQ(view.size(), 0u);
    uint8_t value = 9;
    EXPECT_EQ(reader.read(value), byte_stream::byte_stream_errc::null_pointer);
    EXPECT_EQ(value, 9);
}

TEST(ByteIoTest, ExposesStableStatusNames) {
    EXPECT_STREQ(byte_stream::to_string(byte_stream::byte_stream_errc::ok), "ok");
    EXPECT_STREQ(byte_stream::to_string(byte_stream::byte_stream_errc::insufficient_space), "insufficient_space");
}

TEST(ByteIoPropertyTest, MatchesOwningIntegerWireFormatAcrossEndianAndWidths) {
    uint64_t state = 0x9E3779B97F4A7C15ull;
    for (const auto byte_order : { byte_stream::endian::little, byte_stream::endian::big }) {
        for (uint32_t width = 1; width <= sizeof(uint64_t); ++width) {
            for (std::size_t iteration = 0; iteration < 1000; ++iteration) {
                state ^= state << 7;
                state ^= state >> 9;
                state ^= state << 8;

                std::array<uint8_t, sizeof(uint64_t)> storage{};
                byte_stream::byte_writer writer(storage.data(), storage.size(), byte_order);
                ASSERT_EQ(writer.write(state, width), byte_stream::byte_stream_errc::ok);

                byte_stream::stream owning;
                owning.set_endian(byte_order);
                owning.set(state, width);
                ASSERT_EQ(writer.size(), owning.size());
                EXPECT_TRUE(std::equal(
                    storage.begin(), storage.begin() + writer.size(), owning.buffer().begin()));

                byte_stream::byte_reader reader(writer.view(), byte_order);
                uint64_t decoded = 0;
                ASSERT_EQ(reader.read(decoded, width), byte_stream::byte_stream_errc::ok);
                EXPECT_EQ(decoded, owning.get<uint64_t>(width));
            }
        }
    }
}

TEST(ByteIoPropertyTest, PreservesFloatingPointBitPatterns) {
    uint64_t state = 0xD1B54A32D192ED03ull;
    for (const auto byte_order : { byte_stream::endian::little, byte_stream::endian::big }) {
        for (std::size_t iteration = 0; iteration < 4000; ++iteration) {
            state ^= state << 7;
            state ^= state >> 9;
            state ^= state << 8;
            const uint32_t bits = static_cast<uint32_t>(state);
            float value = 0.0f;
            std::memcpy(&value, &bits, sizeof(value));

            std::array<uint8_t, sizeof(float)> storage{};
            byte_stream::byte_writer writer(storage.data(), storage.size(), byte_order);
            ASSERT_EQ(writer.write(value), byte_stream::byte_stream_errc::ok);
            byte_stream::byte_reader reader(writer.view(), byte_order);
            float decoded = 0.0f;
            ASSERT_EQ(reader.read(decoded), byte_stream::byte_stream_errc::ok);
            uint32_t decoded_bits = 0;
            std::memcpy(&decoded_bits, &decoded, sizeof(decoded_bits));
            EXPECT_EQ(decoded_bits, bits);
        }
    }
}

TEST(ByteIoPropertyTest, MatchesOwningCompactWireFormat) {
    uint64_t state = 0xA0761D6478BD642Full;
    for (std::size_t iteration = 0; iteration < 20000; ++iteration) {
        state ^= state << 7;
        state ^= state >> 9;
        state ^= state << 8;
        int64_t value = 0;
        std::memcpy(&value, &state, sizeof(value));

        std::array<uint8_t, 10> storage{};
        byte_stream::byte_writer writer(storage.data(), storage.size());
        ASSERT_EQ(writer.write_compact(value), byte_stream::byte_stream_errc::ok);

        byte_stream::stream owning;
        owning.set_compact(value);
        ASSERT_EQ(writer.size(), owning.size());
        EXPECT_TRUE(std::equal(
            storage.begin(), storage.begin() + writer.size(), owning.buffer().begin()));

        byte_stream::byte_reader reader(writer.view());
        int64_t decoded = 0;
        ASSERT_EQ(reader.read_compact(decoded), byte_stream::byte_stream_errc::ok);
        EXPECT_EQ(decoded, value);
    }
}

TEST(ByteIoTest, RejectsUnsupportedScalarWidthsWithoutChangingState) {
    constexpr bool long_double_has_supported_width =
        sizeof(long double) == 4 || sizeof(long double) == 8;

    std::array<uint8_t, 16> storage{};
    byte_stream::byte_writer writer(storage.data(), storage.size());
    EXPECT_EQ(writer.write<uint16_t>(1, 0), byte_stream::byte_stream_errc::invalid_size);
    EXPECT_EQ(writer.write<uint16_t>(1, 3), byte_stream::byte_stream_errc::invalid_size);
    EXPECT_EQ(writer.write(1.0f, 3), byte_stream::byte_stream_errc::invalid_size);
    if constexpr (!long_double_has_supported_width) {
        EXPECT_EQ(writer.write(static_cast<long double>(1.0)),
            byte_stream::byte_stream_errc::invalid_size);
    }
    EXPECT_EQ(writer.position(), 0u);

    byte_stream::byte_reader reader(storage.data(), storage.size());
    uint16_t integer = 9;
    float number = 2.0f;
    long double extended = 3.0L;
    EXPECT_EQ(reader.read(integer, 3), byte_stream::byte_stream_errc::invalid_size);
    EXPECT_EQ(reader.read(number, 3), byte_stream::byte_stream_errc::invalid_size);
    if constexpr (!long_double_has_supported_width) {
        EXPECT_EQ(reader.read(extended), byte_stream::byte_stream_errc::invalid_size);
    }
    EXPECT_EQ(reader.position(), 0u);
    EXPECT_EQ(integer, 9);
    EXPECT_EQ(number, 2.0f);
    if constexpr (!long_double_has_supported_width) {
        EXPECT_EQ(extended, 3.0L);
    }
}

} // namespace
