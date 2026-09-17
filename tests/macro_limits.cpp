#include "byte_stream/byte_stream.hpp"
#include "gtest/gtest.h"

#include <array>
#include <stdint.h>
#include <vector>

namespace macro_limits {

using fields = std::array<uint32_t, 64>;

#define TEST_FIELDS_32 \
    values[0], (values[1], 3), values[2], (values[3], 3), \
    values[4], (values[5], 3), values[6], (values[7], 3), \
    values[8], (values[9], 3), values[10], (values[11], 3), \
    values[12], (values[13], 3), values[14], (values[15], 3), \
    values[16], (values[17], 3), values[18], (values[19], 3), \
    values[20], (values[21], 3), values[22], (values[23], 3), \
    values[24], (values[25], 3), values[26], (values[27], 3), \
    values[28], (values[29], 3), values[30], (values[31], 3)
#define TEST_FIELDS_33 \
    TEST_FIELDS_32, values[32]
#define TEST_FIELDS_63 \
    TEST_FIELDS_33, (values[33], 3), values[34], (values[35], 3), \
    values[36], (values[37], 3), values[38], (values[39], 3), \
    values[40], (values[41], 3), values[42], (values[43], 3), \
    values[44], (values[45], 3), values[46], (values[47], 3), \
    values[48], (values[49], 3), values[50], (values[51], 3), \
    values[52], (values[53], 3), values[54], (values[55], 3), \
    values[56], (values[57], 3), values[58], (values[59], 3), \
    values[60], (values[61], 3), values[62]
#define TEST_FIELDS_64 \
    TEST_FIELDS_63, (values[63], 3)

struct public_32 {
    fields values{};
    const fields& data() const { return values; }
};
BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE(public_32, TEST_FIELDS_32)

struct public_33 {
    fields values{};
    const fields& data() const { return values; }
};
BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE(public_33, TEST_FIELDS_33)

struct public_63 {
    fields values{};
    const fields& data() const { return values; }
};
BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE(public_63, TEST_FIELDS_63)

struct public_64 {
    fields values{};
    const fields& data() const { return values; }
};
BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE(public_64, TEST_FIELDS_64)

struct public_write {
    fields values{};
    const fields& data() const { return values; }
};
BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(public_write, TEST_FIELDS_64)

struct public_read {
    fields values{};
    const fields& data() const { return values; }
};
BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE_ONLY_DESERIALIZE(public_read, TEST_FIELDS_64)

class private_64 {
public:
    private_64() = default;
    explicit private_64(const fields& initial) : values(initial) {}
    const fields& data() const { return values; }
private:
    fields values{};
    BYTE_STREAM_DEFINE_TYPE_INTRUSIVE(private_64, TEST_FIELDS_64)
};

class private_write {
public:
    private_write() = default;
    explicit private_write(const fields& initial) : values(initial) {}
    const fields& data() const { return values; }
private:
    fields values{};
    BYTE_STREAM_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(private_write, TEST_FIELDS_64)
};

class private_read {
public:
    private_read() = default;
    explicit private_read(const fields& initial) : values(initial) {}
    const fields& data() const { return values; }
private:
    fields values{};
    BYTE_STREAM_DEFINE_TYPE_INTRUSIVE_ONLY_DESERIALIZE(private_read, TEST_FIELDS_64)
};

#undef TEST_FIELDS_32
#undef TEST_FIELDS_33
#undef TEST_FIELDS_63
#undef TEST_FIELDS_64

static_assert(byte_stream::detail::has_adl_serializer<public_write>::value);
static_assert(!byte_stream::detail::has_adl_deserializer<public_write>::value);
static_assert(!byte_stream::detail::has_adl_serializer<public_read>::value);
static_assert(byte_stream::detail::has_adl_deserializer<public_read>::value);
static_assert(byte_stream::detail::has_adl_serializer<private_write>::value);
static_assert(!byte_stream::detail::has_adl_deserializer<private_write>::value);
static_assert(!byte_stream::detail::has_adl_serializer<private_read>::value);
static_assert(byte_stream::detail::has_adl_deserializer<private_read>::value);

template <typename Writer, typename Reader = Writer>
void check_fields(size_t count) {
    fields input{};
    for (size_t i = 0; i < count; ++i) {
        input[i] = static_cast<uint32_t>(0x123400u + i);
    }
    for (const auto order : {byte_stream::endian::little, byte_stream::endian::big}) {
        SCOPED_TRACE(order == byte_stream::endian::big ? "big" : "little");
        std::vector<uint8_t> expected;
        for (size_t i = 0; i < count; ++i) {
            const size_t width = i % 2 == 0 ? 4 : 3;
            for (size_t j = 0; j < width; ++j) {
                const size_t shift = 8 * (order == byte_stream::endian::little ? j : width - 1 - j);
                expected.push_back(static_cast<uint8_t>(input[i] >> shift));
            }
        }
        byte_stream::stream stream(order);
        stream.set(Writer{input});
        EXPECT_EQ(stream.buffer(), expected);
        Reader decoded{};
        stream.get_to(decoded);
        EXPECT_EQ(decoded.data(), input);
        EXPECT_TRUE(stream.eof());
    }
}

TEST(ByteStreamMacroTest, Supports32Fields) { check_fields<public_32>(32); }
TEST(ByteStreamMacroTest, Supports33Fields) { check_fields<public_33>(33); }
TEST(ByteStreamMacroTest, Supports63Fields) { check_fields<public_63>(63); }
TEST(ByteStreamMacroTest, Supports64Fields) { check_fields<public_64>(64); }
TEST(ByteStreamMacroTest, Supports64PrivateFields) { check_fields<private_64>(64); }
TEST(ByteStreamMacroTest, Supports64FieldsInOneWayMacros) { check_fields<public_write, public_read>(64); }
TEST(ByteStreamMacroTest, Supports64PrivateFieldsInOneWayMacros) { check_fields<private_write, private_read>(64); }

} // namespace macro_limits
