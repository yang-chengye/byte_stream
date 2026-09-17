#include "byte_stream/byte_stream.hpp"
#include "byte_stream/byte_stream_utils.hpp"

#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <stdint.h>
#include <list>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace sample {

enum class kind : uint16_t {
    alpha = 0x12,
    beta = 0x1234
};

struct packet {
    uint8_t id{};
    uint32_t device_id{};
    kind type{};
    std::list<uint16_t> values;

    friend void swap(packet& lhs, packet& rhs) noexcept {
        using std::swap;
        swap(lhs.id, rhs.id);
        swap(lhs.device_id, rhs.device_id);
        swap(lhs.type, rhs.type);
        lhs.values.swap(rhs.values);
    }
};

bool operator==(const packet& lhs, const packet& rhs) {
    return lhs.id == rhs.id &&
        lhs.device_id == rhs.device_id &&
        lhs.type == rhs.type &&
        lhs.values == rhs.values;
}

void to_byte_stream(byte_stream::stream& stream, const packet& value) {
    stream.set(value.id);
    stream.set(value.device_id, 3);
    stream.set(value.type, 1);
    stream.set(static_cast<uint8_t>(value.values.size()));
    stream.set(value.values);
}

void from_byte_stream(const byte_stream::stream& stream, packet& value) {
    stream.get_to(value.id);
    stream.get_to(value.device_id, 3);
    stream.get_to(value.type, 1);

    const auto count = stream.get<uint8_t>();
    stream.get_to(value.values, count);
}

struct envelope {
    uint16_t sequence{};
    packet payload;
    std::array<uint8_t, 2> checksum{};
};

bool operator==(const envelope& lhs, const envelope& rhs) {
    return lhs.sequence == rhs.sequence &&
        lhs.payload == rhs.payload &&
        lhs.checksum == rhs.checksum;
}

void to_byte_stream(byte_stream::stream& stream, const envelope& value) {
    stream.set(value.sequence);
    stream.set(value.payload);
    stream.set(value.checksum);
}

void from_byte_stream(const byte_stream::stream& stream, envelope& value) {
    stream.get_to(value.sequence);
    stream.get_to(value.payload);
    stream.get_to(value.checksum);
}

} // namespace sample

namespace macro_sample {

struct public_header {
    uint8_t version{};
    uint16_t sequence{};
    uint32_t device_id{};
    std::array<uint8_t, 3> tag{};
};

bool operator==(const public_header& lhs, const public_header& rhs) {
    return lhs.version == rhs.version &&
        lhs.sequence == rhs.sequence &&
        lhs.device_id == rhs.device_id &&
        lhs.tag == rhs.tag;
}

BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE(public_header, version, sequence, (device_id, 3), tag)

struct public_serialize_only_header {
    uint8_t id{};
    uint32_t device_id{};
};

BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(public_serialize_only_header, id, (device_id, 3))

struct public_deserialize_only_header {
    uint8_t id{};
    uint32_t device_id{};
};

BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE_ONLY_DESERIALIZE(public_deserialize_only_header, id, (device_id, 3))

class private_header {
public:
    private_header() = default;

    private_header(uint8_t channel, uint32_t device_id, std::array<uint16_t, 2> words)
        : channel_(channel),
          device_id_(device_id),
          words_(words) {
    }

    bool operator==(const private_header& other) const {
        return channel_ == other.channel_ &&
            device_id_ == other.device_id_ &&
            words_ == other.words_;
    }

private:
    uint8_t channel_{};
    uint32_t device_id_{};
    std::array<uint16_t, 2> words_{};

    BYTE_STREAM_DEFINE_TYPE_INTRUSIVE(private_header, channel_, (device_id_, 3), words_)
};

class private_serialize_only_header {
public:
    private_serialize_only_header(uint8_t id, uint32_t device_id)
        : id_(id),
          device_id_(device_id) {
    }

private:
    uint8_t id_{};
    uint32_t device_id_{};

    BYTE_STREAM_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(private_serialize_only_header, id_, (device_id_, 3))
};

class private_deserialize_only_header {
public:
    uint8_t id() const noexcept { return id_; }
    uint32_t device_id() const noexcept { return device_id_; }

private:
    uint8_t id_{};
    uint32_t device_id_{};

    BYTE_STREAM_DEFINE_TYPE_INTRUSIVE_ONLY_DESERIALIZE(private_deserialize_only_header, id_, (device_id_, 3))
};

} // namespace macro_sample

namespace codec_sample {

struct frame_id {
    uint8_t channel{};
    uint32_t value{};
};

struct failing_write {
    // Keep failure runtime-dependent: MSVC otherwise diagnoses the assignment
    // operator's return as unreachable after inlining this test codec.
    volatile bool fail{ true };
};

struct oversized_container {
    using value_type = uint8_t;

    const uint8_t* begin() const noexcept { return nullptr; }
    const uint8_t* end() const noexcept { return nullptr; }
    uint8_t* begin() noexcept { return nullptr; }
    uint8_t* end() noexcept { return nullptr; }
    size_t size() const noexcept { return byte_stream::stream::default_size; }
    void clear() noexcept {}
};

bool operator==(const frame_id& lhs, const frame_id& rhs) {
    return lhs.channel == rhs.channel && lhs.value == rhs.value;
}

} // namespace codec_sample

namespace byte_stream {

template <>
struct byte_stream_codec<codec_sample::frame_id> {
    static void write(stream& stream, const codec_sample::frame_id& value, uint32_t) {
        stream.set(value.channel);
        stream.set(value.value, 3);
    }

    static void read(const stream& stream, codec_sample::frame_id& value, uint32_t) {
        stream.get_to(value.channel);
        stream.get_to(value.value, 3);
    }
};

template <>
struct byte_stream_codec<codec_sample::failing_write> {
    static void write(stream& stream, const codec_sample::failing_write& value, uint32_t) {
        stream.set<uint8_t>(0xFF);
        if (value.fail) {
            throw byte_stream_error(byte_stream_errc::invalid_value,
                "byte_stream: deliberate codec failure");
        }
    }
};

} // namespace byte_stream

template <typename Fn>
void expect_byte_stream_error(Fn&& fn, byte_stream::byte_stream_errc expected_code) {
    try {
        fn();
        FAIL() << "expected byte_stream::byte_stream_error";
    }
    catch (const byte_stream::byte_stream_error& err) {
        EXPECT_EQ(err.code(), expected_code);
        EXPECT_NE(std::string(err.what()).find("byte_stream:"), std::string::npos);
    }
}

TEST(ByteStreamUtilsTest, EscapesAndUnescapesConfiguredBytes) {
    const std::array<byte_stream::byte_stream_utils::byte_escape_rule, 3> rules{ {
        { 0xC0, { 0xDB, 0xDC } },
        { 0xDB, { 0xDB, 0xDD } },
        { 0xAA, { 0x10, 0x20, 0x30 } },
    } };
    const std::vector<uint8_t> raw{ 0x01, 0xC0, 0x02, 0xDB, 0xAA, 0x03 };
    const std::vector<uint8_t> escaped{ 0x01, 0xDB, 0xDC, 0x02,
        0xDB, 0xDD, 0x10, 0x20, 0x30, 0x03 };

    EXPECT_EQ(byte_stream::byte_stream_utils::escape_bytes(raw, rules), escaped);
    EXPECT_EQ(byte_stream::byte_stream_utils::unescape_bytes(escaped, rules), raw);
}

TEST(ByteStreamUtilsTest, EscapesAndUnescapesConfiguredStrings) {
    const std::array<byte_stream::byte_stream_utils::byte_escape_rule, 2> rules{ {
        { 0xC0, { 0xDB, 0xDC } },
        { 0xDB, { 0xDB, 0xDD } },
    } };
    const std::string raw{ static_cast<char>(0x01), static_cast<char>(0xC0),
        static_cast<char>(0xDB), static_cast<char>(0x02) };
    const std::string escaped{ static_cast<char>(0x01), static_cast<char>(0xDB),
        static_cast<char>(0xDC), static_cast<char>(0xDB), static_cast<char>(0xDD),
        static_cast<char>(0x02) };

    EXPECT_EQ(byte_stream::byte_stream_utils::escape_string(raw, rules), escaped);
    EXPECT_EQ(byte_stream::byte_stream_utils::unescape_string(escaped, rules), raw);
}

TEST(ByteStreamUtilsTest, EscapesAndUnescapesFramePayloadOnly) {
    const std::array<byte_stream::byte_stream_utils::byte_escape_rule, 2> rules{ {
        { 0xC0, { 0xDB, 0xDC } },
        { 0xDB, { 0xDB, 0xDD } },
    } };
    const std::vector<uint8_t> logical_frame{ 0xC0, 0x01, 0xC0, 0xDB, 0x02, 0xC0 };
    const std::vector<uint8_t> wire_frame{ 0xC0, 0x01, 0xDB, 0xDC, 0xDB, 0xDD, 0x02, 0xC0 };

    EXPECT_EQ(byte_stream::byte_stream_utils::escape_frame_payload(logical_frame, 1, 1, rules), wire_frame);
    EXPECT_EQ(byte_stream::byte_stream_utils::unescape_frame_payload(wire_frame, 1, 1, rules), logical_frame);

    auto stream = byte_stream::byte_stream_utils::make_unescaped_frame_stream(wire_frame, 1, 1, rules);
    EXPECT_EQ(stream.get<uint8_t>(), 0xC0);
    EXPECT_EQ(stream.get<uint8_t>(), 0x01);
    EXPECT_EQ(stream.get<uint8_t>(), 0xC0);
    EXPECT_EQ(stream.get<uint8_t>(), 0xDB);
    EXPECT_EQ(stream.get<uint8_t>(), 0x02);
    EXPECT_EQ(stream.get<uint8_t>(), 0xC0);
    EXPECT_TRUE(stream.eof());
}

TEST(ByteStreamUtilsTest, RejectsInvalidEscapeSequences) {
    const std::array<byte_stream::byte_stream_utils::byte_escape_rule, 2> rules{ {
        { 0xC0, { 0xDB, 0xDC } },
        { 0xDB, { 0xDB, 0xDD } },
    } };

    expect_byte_stream_error(
        [&] { byte_stream::byte_stream_utils::unescape_bytes({ 0x01, 0xDB }, rules); },
        byte_stream::byte_stream_errc::invalid_size);

    expect_byte_stream_error(
        [&] { byte_stream::byte_stream_utils::unescape_bytes({ 0xDB, 0x00 }, rules); },
        byte_stream::byte_stream_errc::invalid_size);

    expect_byte_stream_error(
        [] {
            const std::array<byte_stream::byte_stream_utils::byte_escape_rule, 1> rules{ {
                { 0xC0, {} },
            } };
            byte_stream::byte_stream_utils::escape_bytes({ 0xC0 }, rules);
        },
        byte_stream::byte_stream_errc::invalid_size);

    expect_byte_stream_error(
        [&] { byte_stream::byte_stream_utils::unescape_frame_payload({ 0xC0 }, 1, 1, rules); },
        byte_stream::byte_stream_errc::invalid_size);
}

TEST(ByteStreamTest, ConstructsFilledBuffer) {
    using byte_stream::stream;
    static_assert(!std::is_convertible_v<size_t, stream>);
    const stream zeros(172);
    EXPECT_EQ(zeros.buffer(), std::vector<uint8_t>(172, 0));
    EXPECT_EQ(zeros.position(), 0u);
    EXPECT_EQ(zeros.get_endian(), byte_stream::endian::little);
    const stream filled(172, 0xAB);
    EXPECT_EQ(filled.buffer(), std::vector<uint8_t>(172, 0xAB));
    EXPECT_EQ(filled.position(), 0u);
    EXPECT_EQ(filled.get_endian(), byte_stream::endian::little);
    EXPECT_TRUE(stream(0).empty());
    EXPECT_TRUE(stream(size_t{0}, 0xFF).empty());
    EXPECT_TRUE(stream(nullptr, 0).empty());
    const uint8_t raw[]{0x12, 0x34};
    EXPECT_EQ(stream(raw, sizeof(raw)).buffer(), (std::vector<uint8_t>{0x12, 0x34}));
}

TEST(ByteStreamTest, ResizePreservesPrefixAndOnlyFillsNewBytes) {
    byte_stream::stream stream(byte_stream::endian::big);
    stream.reserve(256);
    stream.set<uint16_t>(0x1234);
    ASSERT_TRUE(stream.seek(1));
    const auto* data = stream.data();
    const auto capacity = stream.buffer().capacity();
    stream.resize(4);
    EXPECT_EQ(stream.buffer(), (std::vector<uint8_t>{0x12, 0x34, 0, 0}));
    stream.resize(6, 0xAB);
    EXPECT_EQ(stream.buffer(), (std::vector<uint8_t>{0x12, 0x34, 0, 0, 0xAB, 0xAB}));
    stream.resize(6, 0xFF);
    EXPECT_EQ(stream.buffer(), (std::vector<uint8_t>{0x12, 0x34, 0, 0, 0xAB, 0xAB}));
    EXPECT_EQ(stream.position(), 1u);
    EXPECT_EQ(stream.get_endian(), byte_stream::endian::big);
    EXPECT_EQ(stream.data(), data);
    EXPECT_EQ(stream.buffer().capacity(), capacity);
    stream.clear();
    stream.resize(172);
    EXPECT_EQ(stream.buffer(), std::vector<uint8_t>(172, 0));
    EXPECT_EQ(stream.position(), 0u);
    EXPECT_EQ(stream.data(), data);
}

TEST(ByteStreamTest, ResizeClampsCursorAndPreservesCapacity) {
    byte_stream::stream stream(8, 0xAB);
    stream.set_endian(byte_stream::endian::big);
    const auto capacity = stream.buffer().capacity();
    ASSERT_TRUE(stream.seek(3));
    stream.resize(5);
    EXPECT_EQ(stream.position(), 3u);
    stream.resize(3);
    EXPECT_EQ(stream.position(), 3u);
    EXPECT_TRUE(stream.eof());
    stream.resize(2);
    EXPECT_EQ(stream.position(), 2u);
    EXPECT_EQ(stream.remaining(), 0u);
    EXPECT_EQ(stream.buffer(), std::vector<uint8_t>(2, 0xAB));
    stream.resize(0);
    EXPECT_TRUE(stream.empty());
    EXPECT_EQ(stream.position(), 0u);
    EXPECT_EQ(stream.buffer().capacity(), capacity);
    stream.resize(8);
    EXPECT_EQ(stream.buffer(), std::vector<uint8_t>(8, 0));
    EXPECT_EQ(stream.get_endian(), byte_stream::endian::big);
}

TEST(ByteStreamTest, ResizeGrowthAndOverflowPreserveState) {
    byte_stream::stream stream(4, 0xAB);
    stream.set_endian(byte_stream::endian::big);
    ASSERT_TRUE(stream.seek(2));
    const auto larger_size = stream.buffer().capacity() + 17;
    stream.resize(larger_size, 0xCD);
    std::vector<uint8_t> expected(larger_size, 0xCD);
    std::fill_n(expected.begin(), 4, 0xAB);
    EXPECT_EQ(stream.buffer(), expected);
    EXPECT_EQ(stream.position(), 2u);
    const auto capacity = stream.buffer().capacity();
    const auto too_large = (std::numeric_limits<size_t>::max)();
    expect_byte_stream_error([&] { stream.resize(too_large); },
        byte_stream::byte_stream_errc::size_overflow);
    EXPECT_EQ(stream.buffer(), expected);
    EXPECT_EQ(stream.position(), 2u);
    EXPECT_EQ(stream.get_endian(), byte_stream::endian::big);
    EXPECT_EQ(stream.buffer().capacity(), capacity);
    expect_byte_stream_error([&] { byte_stream::stream oversized(too_large); },
        byte_stream::byte_stream_errc::size_overflow);
}

TEST(ByteStreamTest, ConstructsEmptyStreamWithExplicitEndian) {
    using byte_stream::endian;
    using byte_stream::stream;
    static_assert(!std::is_convertible_v<endian, stream>);
    static_assert(std::is_nothrow_constructible_v<stream, endian>);
    EXPECT_EQ(stream{}.get_endian(), endian::little);
    for (const auto order : { endian::little, endian::big }) {
        stream bs(order);
        EXPECT_TRUE(bs.empty());
        EXPECT_EQ(bs.position(), 0u);
        EXPECT_EQ(bs.get_endian(), order);
    }
}

TEST(ByteStreamTest, ObjectAssignmentReplacesDataAndReusesCapacity) {
    using byte_stream::endian;
    for (const auto order : { endian::little, endian::big }) {
        byte_stream::stream bs(order);
        bs.reserve(64);
        bs.set<uint32_t>(0xFFFFFFFF);
        ASSERT_TRUE(bs.seek(2));
        const auto capacity = bs.buffer().capacity();
        EXPECT_EQ(&(bs = uint16_t{0x1234}), &bs);
        const std::vector<uint8_t> expected = order == endian::big
            ? std::vector<uint8_t>{0x12, 0x34} : std::vector<uint8_t>{0x34, 0x12};
        EXPECT_EQ(bs.buffer(), expected);
        EXPECT_EQ(bs.buffer().capacity(), capacity);
        EXPECT_EQ(bs.position(), 0u);
        EXPECT_EQ(bs.get_endian(), order);
        EXPECT_EQ(bs.get<uint16_t>(), 0x1234);
        bs.set<uint16_t>(0x5678);
        EXPECT_EQ(bs.position(), 2u);
        uint16_t decoded{};
        bs.get_to(decoded);
        EXPECT_EQ(decoded, 0x5678);
        EXPECT_TRUE(bs.eof());
    }
}

TEST(ByteStreamTest, ObjectAssignmentUsesCustomAndContainerCodecs) {
    using byte_stream::endian;
    byte_stream::stream bs(endian::big);
    const sample::envelope value{0x1234, {7, 0xA1B2C3, sample::kind::alpha, {0x5678}}, {0xAB, 0xCD}};
    bs = value;
    EXPECT_EQ(bs.buffer(), (std::vector<uint8_t>{
        0x12, 0x34, 7, 0xA1, 0xB2, 0xC3, 0x12, 1, 0x56, 0x78, 0xAB, 0xCD}));
    EXPECT_EQ(bs.get<sample::envelope>(), value);
    const std::vector<uint8_t> bytes{1, 2, 3};
    bs = bytes;
    EXPECT_EQ(bs.buffer(), bytes);
    bs = std::vector<uint8_t>{4, 5};
    EXPECT_EQ(bs.buffer(), (std::vector<uint8_t>{4, 5}));
    EXPECT_EQ(bs.get_endian(), endian::big);
    const uint16_t array[]{0x1234, 0x5678};
    bs = array;
    EXPECT_EQ(bs.buffer(), (std::vector<uint8_t>{0x12, 0x34, 0x56, 0x78}));
    bs = std::vector<uint8_t>{};
    EXPECT_TRUE(bs.empty());
    EXPECT_EQ(bs.position(), 0u);
    EXPECT_EQ(bs.get_endian(), endian::big);
}

TEST(ByteStreamTest, ObjectAssignmentFailureLeavesEncodedPrefix) {
    byte_stream::stream bs(byte_stream::endian::big);
    bs.set<uint32_t>(0x12345678);
    ASSERT_TRUE(bs.seek(2));
    expect_byte_stream_error([&] { bs = codec_sample::failing_write{}; },
        byte_stream::byte_stream_errc::invalid_value);
    EXPECT_EQ(bs.buffer(), (std::vector<uint8_t>{0xFF}));
    EXPECT_EQ(bs.position(), 0u);
    EXPECT_EQ(bs.get_endian(), byte_stream::endian::big);
    EXPECT_EQ(&(bs = codec_sample::failing_write{ false }), &bs);
    EXPECT_EQ(bs.buffer(), (std::vector<uint8_t>{0xFF}));
    bs = uint16_t{0x1234};
    EXPECT_EQ(bs.get<uint16_t>(), 0x1234);
}

TEST(ByteStreamTest, EndianSwitchOnlyAffectsSubsequentOperations) {
    using byte_stream::endian;
    byte_stream::stream bs(endian::big);
    bs.set<uint16_t>(0x1234);
    EXPECT_EQ(bs.get<uint16_t>(), 0x1234);
    const auto bytes = bs.buffer();
    bs.set_endian(endian::little);
    EXPECT_EQ(bs.buffer(), bytes);
    EXPECT_EQ(bs.position(), 2u);
    bs.set<uint16_t>(0x5678);
    EXPECT_EQ(bs.buffer(), (std::vector<uint8_t>{0x12, 0x34, 0x78, 0x56}));
    EXPECT_EQ(bs.get<uint16_t>(), 0x5678);
    bs.reset_position();
    EXPECT_EQ(bs.get<uint16_t>(), 0x3412);
}

TEST(ByteStreamTest, StreamTransfersPreserveCompleteState) {
    using byte_stream::endian;
    using byte_stream::stream;
    static_assert(std::is_nothrow_move_constructible_v<stream>);
    static_assert(std::is_nothrow_move_assignable_v<stream>);
    stream source(endian::big);
    source.set<uint32_t>(0x12345678);
    ASSERT_TRUE(source.seek(2));
    const auto bytes = source.buffer();
    const auto check = [&](const stream& bs) {
        EXPECT_EQ(bs.buffer(), bytes);
        EXPECT_EQ(bs.position(), 2u);
        EXPECT_EQ(bs.get_endian(), endian::big);
    };
    stream copied(source);
    check(copied);
    stream assigned;
    assigned = source;
    check(assigned);
    const stream& const_source = source;
    assigned = const_source;
    check(assigned);
    stream moved(std::move(copied));
    check(moved);
    assigned = std::move(moved);
    check(assigned);
    check(source);
    stream& alias = source;
    source = alias;
    check(source);
    struct derived_stream : stream { using stream::stream; };
    derived_stream derived(endian::big);
    derived.set<uint32_t>(0x12345678);
    ASSERT_TRUE(derived.seek(2));
    assigned = derived;
    check(assigned);
}

TEST(ByteStreamTest, WritesAndReadsLittleEndianIntegers) {
    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set<uint16_t>(0x1234);
    stream.set<uint32_t>(0x00A1B2C3, 3);
    stream.set<uint64_t>(0x0001020304050607, 7);

    EXPECT_EQ(stream.to_hex(true), "34 12 c3 b2 a1 07 06 05 04 03 02 01");
    EXPECT_EQ(stream.size(), 12U);
    EXPECT_EQ(stream.position(), 0U);

    EXPECT_EQ(stream.get<uint16_t>(), 0x1234);
    EXPECT_EQ(stream.get<uint32_t>(3), 0x00A1B2C3);
    EXPECT_EQ(stream.get<uint64_t>(7), 0x0001020304050607);
    EXPECT_TRUE(stream.eof());
    EXPECT_EQ(stream.remaining(), 0U);

    stream.reset_position();
    EXPECT_EQ(stream.position(), 0U);
    EXPECT_FALSE(stream.seek(stream.size() + 1));
}

TEST(ByteStreamTest, UsesCanonicalLittleEndianByDefault) {
    byte_stream::stream stream;
    stream.set<uint16_t>(0x1234);
    stream.set<uint32_t>(0x00A1B2C3, 3);
    stream.set<float>(1.5f);

    EXPECT_EQ(stream.get_endian(), byte_stream::endian::little);
    EXPECT_EQ(stream.to_hex(true), "34 12 c3 b2 a1 00 00 c0 3f");
    EXPECT_EQ(stream.get<uint16_t>(), 0x1234);
    EXPECT_EQ(stream.get<uint32_t>(3), 0x00A1B2C3);
    EXPECT_FLOAT_EQ(stream.get<float>(), 1.5f);
}

TEST(ByteStreamTest, WritesAndReadsBigEndianFloatingPointValues) {
    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::big);
    stream.set<uint16_t>(0x1234);
    stream.set<uint32_t>(0x00A1B2C3, 3);
    stream.set<float>(1.5f, 4);

    EXPECT_EQ(stream.to_hex(true), "12 34 a1 b2 c3 3f c0 00 00");
    EXPECT_EQ(stream.get<uint16_t>(), 0x1234);
    EXPECT_EQ(stream.get<uint32_t>(3), 0x00A1B2C3);
    EXPECT_FLOAT_EQ(stream.get<float>(4), 1.5f);
}

TEST(ByteStreamTest, PreservesFloatingPointSpecialValues) {
    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::big);
    stream.set(std::numeric_limits<float>::quiet_NaN());
    stream.set(std::numeric_limits<float>::infinity());
    stream.set(-std::numeric_limits<double>::infinity());

    EXPECT_TRUE(std::isnan(stream.get<float>()));

    const auto positive_inf = stream.get<float>();
    EXPECT_TRUE(std::isinf(positive_inf));
    EXPECT_FALSE(std::signbit(positive_inf));

    const auto negative_inf = stream.get<double>();
    EXPECT_TRUE(std::isinf(negative_inf));
    EXPECT_TRUE(std::signbit(negative_inf));
}

TEST(ByteStreamTest, BulkScalarContainersRespectEndian) {
    const std::vector<uint16_t> words{ 0x1234, 0xABCD };
    const std::array<uint32_t, 2> dwords{ 0x01020304, 0xA1B2C3D4 };

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::big);
    stream.set(words);
    stream.set(dwords);

    EXPECT_EQ(stream.to_hex(true), "12 34 ab cd 01 02 03 04 a1 b2 c3 d4");

    std::vector<uint16_t> decoded_words;
    std::array<uint32_t, 2> decoded_dwords{};
    stream.get_to(decoded_words, static_cast<uint32_t>(words.size()));
    stream.get_to(decoded_dwords);

    EXPECT_EQ(decoded_words, words);
    EXPECT_EQ(decoded_dwords, dwords);
}

TEST(ByteStreamTest, SignExtendsPartialSignedIntegers) {
    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set<int16_t>(-2, 1);
    stream.set<int32_t>(-32768, 2);

    EXPECT_EQ(stream.to_hex(true), "fe 00 80");
    EXPECT_EQ(stream.get<int16_t>(1), -2);
    EXPECT_EQ(stream.get<int32_t>(2), -32768);
}

TEST(ByteStreamTest, SupportsBooleansAndEnums) {
    byte_stream::stream stream;
    stream.set(true);
    stream.set(false);
    stream.set(sample::kind::alpha, 1);
    stream.set(sample::kind::beta);

    EXPECT_EQ(stream.to_hex(true), "01 00 12 34 12");
    EXPECT_TRUE(stream.get<bool>());
    EXPECT_FALSE(stream.get<bool>());
    EXPECT_EQ(stream.get<sample::kind>(1), sample::kind::alpha);
    EXPECT_EQ(stream.get<sample::kind>(), sample::kind::beta);
}

TEST(ByteStreamTest, SupportsFixedWidthIntegersAndStdByte) {
    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set<int8_t>(-1);
    stream.set<uint8_t>(0x80);
    stream.set(std::byte{ 0xAB });
    stream.set<int16_t>(-2);

    EXPECT_EQ(stream.to_hex(true), "ff 80 ab fe ff");
    EXPECT_EQ(stream.get<int8_t>(), -1);
    EXPECT_EQ(stream.get<uint8_t>(), 0x80);
    EXPECT_EQ(stream.get<std::byte>(), std::byte{ 0xAB });
    EXPECT_EQ(stream.get<int16_t>(), -2);
}

TEST(ByteStreamTest, SupportsCompactIntegersAndEnums) {
    byte_stream::stream stream;
    stream.set_compact<uint64_t>(0);
    stream.set_compact<uint64_t>(127);
    stream.set_compact<uint64_t>(128);
    stream.set_compact<uint64_t>(300);
    stream.set_compact<int32_t>(-1);
    stream.set_compact<int32_t>(-64);
    stream.set_compact<int32_t>(64);
    stream.set_compact(sample::kind::beta);

    EXPECT_EQ(stream.to_hex(true), "00 7f 80 01 ac 02 01 7f 80 01 b4 24");
    EXPECT_EQ(stream.get_compact<uint64_t>(), 0U);
    EXPECT_EQ(stream.get_compact<uint64_t>(), 127U);
    EXPECT_EQ(stream.get_compact<uint64_t>(), 128U);
    EXPECT_EQ(stream.get_compact<uint64_t>(), 300U);
    EXPECT_EQ(stream.get_compact<int32_t>(), -1);
    EXPECT_EQ(stream.get_compact<int32_t>(), -64);
    EXPECT_EQ(stream.get_compact<int32_t>(), 64);
    EXPECT_EQ(stream.get_compact<sample::kind>(), sample::kind::beta);
}

TEST(ByteStreamTest, WritesAndReadsContainerElements) {
    const std::vector<uint16_t> values{ 0x1111, 0x2222, 0x3333 };

    byte_stream::stream stream;
    stream.set(values, 2);

    std::vector<uint16_t> decoded;
    stream.get_to(decoded, 2);

    EXPECT_EQ(decoded, (std::vector<uint16_t>{ 0x1111, 0x2222 }));

    byte_stream::stream short_stream;
    expect_byte_stream_error([&] { short_stream.set(values, 4); },
        byte_stream::byte_stream_errc::container_too_small);
}

TEST(ByteStreamTest, HandlesEmptyContainersWithoutConsumingBytes) {
    const std::vector<uint8_t> bytes;
    const std::vector<uint16_t> words;
    const std::list<uint32_t> items;

    byte_stream::stream stream;
    stream.set(bytes);
    stream.set(words);
    stream.set(items);
    EXPECT_TRUE(stream.empty());

    std::vector<uint8_t> decoded_bytes{ 0xAA };
    std::vector<uint16_t> decoded_words{ 0x1234 };
    std::list<uint32_t> decoded_items{ 0xDEADBEEFu };

    stream.get_to(decoded_bytes, 0);
    stream.get_to(decoded_words, 0);
    stream.get_to(decoded_items, 0);

    EXPECT_TRUE(decoded_bytes.empty());
    EXPECT_TRUE(decoded_words.empty());
    EXPECT_TRUE(decoded_items.empty());
    EXPECT_EQ(stream.position(), 0U);
}

TEST(ByteStreamTest, WritesAndReadsCompactSizePrefixedContainers) {
    const std::vector<uint16_t> values{ 0x1234, 0x5678, 0x9ABC };

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set_with_size(values);

    EXPECT_EQ(stream.to_hex(true), "03 34 12 78 56 bc 9a");

    std::vector<uint16_t> decoded;
    stream.get_to_with_size(decoded, 4);
    EXPECT_EQ(decoded, values);

    byte_stream::stream atomic_stream(stream.buffer());
    std::vector<uint16_t> atomic_decoded;
    atomic_stream.get_to_with_size_atomic(atomic_decoded, 4);
    EXPECT_EQ(atomic_decoded, values);

    byte_stream::stream limited(stream.buffer());
    limited.set_endian(byte_stream::endian::little);
    expect_byte_stream_error([&] { limited.get_to_with_size(decoded, 2); },
        byte_stream::byte_stream_errc::count_out_of_range);
}

TEST(ByteStreamTest, RejectsTruncatedLengthPrefixedScalarContainersBeforeResize) {
    byte_stream::stream stream(std::vector<uint8_t>{ 0x02, 0x34 });
    stream.set_endian(byte_stream::endian::little);

    const std::vector<uint16_t> original{ 0xABCD };
    std::vector<uint16_t> decoded = original;

    expect_byte_stream_error([&] { stream.get_to_with_size(decoded, 2); },
        byte_stream::byte_stream_errc::insufficient_data);

    EXPECT_EQ(decoded, original);
    EXPECT_EQ(stream.position(), 0U);
}

TEST(ByteStreamTest, WritesAndReadsRawByteContainers) {
    const std::vector<uint8_t> payload{ 0x00, 0xC0, 0xDB, 0xFF };
    const std::string text{ "wire" };

    byte_stream::stream stream;
    stream.set(payload);
    stream.set(text);

    EXPECT_EQ(stream.to_hex(true), "00 c0 db ff 77 69 72 65");

    std::vector<uint8_t> decoded_payload;
    std::string decoded_text;
    stream.get_to(decoded_payload, static_cast<uint32_t>(payload.size()));
    stream.get_to(decoded_text, static_cast<uint32_t>(text.size()));

    EXPECT_EQ(decoded_payload, payload);
    EXPECT_EQ(decoded_text, text);
}

TEST(ByteStreamTest, WritesAndReadsFixedArrays) {
    const std::array<uint16_t, 3> std_array{ 0x1111, 0x2222, 0x3333 };
    const uint8_t c_array[3]{ 0xAA, 0xBB, 0xCC };

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set(std_array);
    stream.set(c_array, 2);

    EXPECT_EQ(stream.to_hex(true), "11 11 22 22 33 33 aa bb");

    std::array<uint16_t, 3> decoded_std_array{};
    uint8_t decoded_c_array[3]{};
    stream.get_to(decoded_std_array);
    stream.get_to(decoded_c_array, 2);

    EXPECT_EQ(decoded_std_array, std_array);
    EXPECT_EQ(decoded_c_array[0], 0xAA);
    EXPECT_EQ(decoded_c_array[1], 0xBB);
    EXPECT_EQ(decoded_c_array[2], 0x00);

    expect_byte_stream_error([&] { stream.set(std_array, 4); },
        byte_stream::byte_stream_errc::count_out_of_range);
    expect_byte_stream_error([&] { stream.get_to(decoded_c_array, 4); },
        byte_stream::byte_stream_errc::count_out_of_range);
}

TEST(ByteStreamTest, WritesAndReadsOptionalValues) {
    const std::optional<uint16_t> present{ 0x1234 };
    const std::optional<uint16_t> absent;

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set(present);
    stream.set(absent);

    EXPECT_EQ(stream.to_hex(true), "01 34 12 00");

    std::optional<uint16_t> decoded_present;
    std::optional<uint16_t> decoded_absent{ 0xFFFF };
    stream.get_to(decoded_present);
    stream.get_to(decoded_absent);

    ASSERT_TRUE(decoded_present.has_value());
    EXPECT_EQ(*decoded_present, 0x1234);
    EXPECT_FALSE(decoded_absent.has_value());
}

TEST(ByteStreamTest, WritesAndReadsSmartPointers) {
    auto unique_value = std::make_unique<uint16_t>(0x1234);
    auto shared_value = std::make_shared<uint32_t>(0x00A1B2C3);

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set(unique_value);
    stream.set(shared_value, 3);

    EXPECT_EQ(stream.to_hex(true), "34 12 c3 b2 a1");

    std::unique_ptr<uint16_t> decoded_unique;
    std::shared_ptr<uint32_t> decoded_shared;
    stream.get_to(decoded_unique);
    stream.get_to(decoded_shared, 3);

    ASSERT_NE(decoded_unique, nullptr);
    ASSERT_NE(decoded_shared, nullptr);
    EXPECT_EQ(*decoded_unique, 0x1234);
    EXPECT_EQ(*decoded_shared, 0x00A1B2C3);

    const std::unique_ptr<uint16_t> null_unique;
    expect_byte_stream_error([&] { stream.set(null_unique); },
        byte_stream::byte_stream_errc::null_pointer);
}

TEST(ByteStreamTest, WritesAndReadsPairsAndTuples) {
    const std::pair<uint8_t, uint16_t> pair_value{ 0x12, 0x3456 };
    const std::tuple<uint8_t, uint16_t, std::byte> tuple_value{ 0x78, 0x9ABC, std::byte{ 0xDE } };

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set(pair_value);
    stream.set(tuple_value);

    EXPECT_EQ(stream.to_hex(true), "12 56 34 78 bc 9a de");

    std::pair<uint8_t, uint16_t> decoded_pair;
    std::tuple<uint8_t, uint16_t, std::byte> decoded_tuple;
    stream.get_to(decoded_pair);
    stream.get_to(decoded_tuple);

    EXPECT_EQ(decoded_pair, pair_value);
    EXPECT_EQ(decoded_tuple, tuple_value);
}

TEST(ByteStreamTest, WritesAndReadsVariantsWithCanonicalIndexes) {
    using text_type = byte_stream::length_prefixed<std::string, 32>;
    using value_type = std::variant<std::monostate, uint16_t, text_type>;

    byte_stream::stream stream;
    stream.set(value_type{ std::monostate{} });
    stream.set(value_type{ uint16_t{ 0x1234 } });
    stream.set(value_type{ text_type{ std::string{ "wire" } } });

    EXPECT_EQ(stream.to_hex(true), "00 01 34 12 02 04 77 69 72 65");
    EXPECT_TRUE(std::holds_alternative<std::monostate>(stream.get<value_type>()));
    EXPECT_EQ(std::get<uint16_t>(stream.get<value_type>()), 0x1234);
    EXPECT_EQ(std::get<text_type>(stream.get<value_type>()).value, "wire");
}

TEST(ByteStreamTest, RejectsInvalidAndTruncatedVariantsAtomically) {
    using text_type = byte_stream::length_prefixed<std::string, 32>;
    using value_type = std::variant<std::monostate, uint16_t, text_type>;

    byte_stream::stream invalid_index(std::vector<uint8_t>{ 0x03 });
    expect_byte_stream_error([&] { (void)invalid_index.get_atomic<value_type>(); },
        byte_stream::byte_stream_errc::invalid_value);
    EXPECT_EQ(invalid_index.position(), 0U);

    byte_stream::stream truncated(std::vector<uint8_t>{ 0x02, 0x04, 'w' });
    value_type decoded{ uint16_t{ 0xCAFE } };
    expect_byte_stream_error([&] { truncated.get_to_atomic(decoded); },
        byte_stream::byte_stream_errc::insufficient_data);
    EXPECT_EQ(std::get<uint16_t>(decoded), 0xCAFE);
    EXPECT_EQ(truncated.position(), 0U);
}

TEST(ByteStreamTest, LengthPrefixedValuesComposeIdenticallyInsideVariants) {
    using text_type = byte_stream::length_prefixed<std::string, 8>;
    const text_type text{ std::string{ "wire" } };

    byte_stream::stream standalone;
    standalone.set(text);
    EXPECT_EQ(standalone.to_hex(true), "04 77 69 72 65");

    using value_type = std::variant<uint16_t, text_type>;
    byte_stream::stream variant_stream;
    variant_stream.set(value_type{ text });
    ASSERT_EQ(variant_stream.buffer().front(), 0x01);
    EXPECT_TRUE(std::equal(
        standalone.buffer().begin(),
        standalone.buffer().end(),
        variant_stream.buffer().begin() + 1));

    const auto decoded = variant_stream.get<value_type>();
    EXPECT_EQ(std::get<text_type>(decoded), text);

    byte_stream::stream oversized;
    expect_byte_stream_error(
        [&] { oversized.set(text_type{ std::string(9, 'x') }); },
        byte_stream::byte_stream_errc::count_out_of_range);
    EXPECT_TRUE(oversized.empty());
}

TEST(ByteStreamTest, UsesAdlForCustomProtocolTypes) {
    const sample::packet expected{
        7,
        0x00112233,
        sample::kind::alpha,
        { 0x0102, 0x0304 }
    };

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set(expected);

    EXPECT_EQ(stream.to_hex(true), "07 33 22 11 12 02 02 01 04 03");

    sample::packet decoded;
    stream.get_to(decoded);
    EXPECT_EQ(decoded, expected);

    byte_stream::stream default_stream;
    default_stream.set(expected);
    EXPECT_EQ(byte_stream::stream::parse<sample::packet>(default_stream.buffer()), expected);

    byte_stream::stream const_stream(stream.buffer());
    const_stream.set_endian(byte_stream::endian::little);
    sample::packet decoded_from_const_stream;
    const_stream.get_to_atomic(decoded_from_const_stream);
    EXPECT_EQ(decoded_from_const_stream, expected);
}

TEST(ByteStreamTest, ParseConsumesExactlyOneObjectFromVectorsAndStrings) {
    const std::vector<uint8_t> encoded{ 0x34, 0x12 };
    EXPECT_EQ(byte_stream::stream::parse<uint16_t>(encoded), 0x1234);

    const std::string encoded_string{ static_cast<char>(0x34), static_cast<char>(0x12) };
    EXPECT_EQ(byte_stream::stream::parse<uint16_t>(encoded_string), 0x1234);

    expect_byte_stream_error(
        [] { (void)byte_stream::stream::parse<uint16_t>(std::vector<uint8_t>{ 0x34, 0x12, 0x00 }); },
        byte_stream::byte_stream_errc::trailing_data);
    expect_byte_stream_error(
        [] {
            (void)byte_stream::stream::parse<uint16_t>(
                std::string{ static_cast<char>(0x34), static_cast<char>(0x12), '\0' });
        },
        byte_stream::byte_stream_errc::trailing_data);
}

TEST(ByteStreamTest, ParsesRawBytesAndExplicitBigEndian) {
    const std::array<uint8_t, 2> encoded{ 0x12, 0x34 };
    EXPECT_EQ(byte_stream::stream::parse<uint16_t>(
                  encoded.data(), encoded.size(), byte_stream::endian::big),
        0x1234);
    EXPECT_EQ(byte_stream::stream::parse<uint16_t>(
                  std::vector<uint8_t>{ encoded.begin(), encoded.end() }, byte_stream::endian::big),
        0x1234);
}

TEST(ByteStreamTest, WritesAndReadsNestedStructs) {
    const sample::envelope expected{
        0x1234,
        {
            7,
            0x00112233,
            sample::kind::alpha,
            { 0x0102, 0x0304 },
        },
        { 0xAA, 0x55 },
    };

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set(expected);

    EXPECT_EQ(stream.to_hex(true), "34 12 07 33 22 11 12 02 02 01 04 03 aa 55");

    sample::envelope decoded;
    stream.get_to(decoded);
    EXPECT_EQ(decoded, expected);
}

TEST(ByteStreamTest, SupportsExternalCodecSpecialization) {
    const codec_sample::frame_id expected{ 0x12, 0x00A1B2C3 };

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set(expected);

    EXPECT_EQ(stream.to_hex(true), "12 c3 b2 a1");

    codec_sample::frame_id decoded;
    stream.get_to(decoded);
    EXPECT_EQ(decoded, expected);
}

TEST(ByteStreamTest, MacroDefinesNonIntrusiveCustomProtocolTypes) {
    const macro_sample::public_header expected{
        2,
        0x1234,
        0x00112233,
        { 0xAA, 0xBB, 0xCC }
    };

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set(expected);

    EXPECT_EQ(stream.to_hex(true), "02 34 12 33 22 11 aa bb cc");

    macro_sample::public_header decoded;
    stream.get_to(decoded);
    EXPECT_EQ(decoded, expected);
}

TEST(ByteStreamTest, MacroDefinesIntrusiveCustomProtocolTypes) {
    const macro_sample::private_header expected{
        9,
        0x00112233,
        { 0x3344, 0x5566 }
    };

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::little);
    stream.set(expected);

    EXPECT_EQ(stream.to_hex(true), "09 33 22 11 44 33 66 55");

    macro_sample::private_header decoded;
    stream.get_to(decoded);
    EXPECT_EQ(decoded, expected);
}

TEST(ByteStreamTest, MacroDefinesOneWayCustomProtocolTypes) {
    byte_stream::stream public_out;
    public_out.set_endian(byte_stream::endian::little);
    public_out.set(macro_sample::public_serialize_only_header{ 5, 0x00112233 });
    EXPECT_EQ(public_out.to_hex(true), "05 33 22 11");

    byte_stream::stream public_in(std::vector<uint8_t>{ 6, 0x66, 0x55, 0x44 });
    public_in.set_endian(byte_stream::endian::little);
    macro_sample::public_deserialize_only_header public_decoded;
    public_in.get_to(public_decoded);
    EXPECT_EQ(public_decoded.id, 6);
    EXPECT_EQ(public_decoded.device_id, 0x00445566);

    byte_stream::stream private_out;
    private_out.set_endian(byte_stream::endian::little);
    private_out.set(macro_sample::private_serialize_only_header{ 7, 0x00778899 });
    EXPECT_EQ(private_out.to_hex(true), "07 99 88 77");

    byte_stream::stream private_in(std::vector<uint8_t>{ 8, 0xCC, 0xBB, 0xAA });
    private_in.set_endian(byte_stream::endian::little);
    macro_sample::private_deserialize_only_header private_decoded;
    private_in.get_to(private_decoded);
    EXPECT_EQ(private_decoded.id(), 8);
    EXPECT_EQ(private_decoded.device_id(), 0x00AABBCC);
}

TEST(ByteStreamTest, ReportsInvalidInputClearly) {
    byte_stream::stream stream;

    expect_byte_stream_error([&] { (void)stream.get<uint16_t>(); },
        byte_stream::byte_stream_errc::insufficient_data);
    expect_byte_stream_error([&] { (void)stream.get<std::vector<uint8_t>>(); },
        byte_stream::byte_stream_errc::missing_element_count);
    expect_byte_stream_error([&] { stream.set<uint16_t>(0x1234, 0); },
        byte_stream::byte_stream_errc::invalid_size);
    expect_byte_stream_error([&] { stream.set<uint16_t>(0x1234, 3); },
        byte_stream::byte_stream_errc::invalid_size);
    expect_byte_stream_error([&] { stream.set<float>(1.0f, 3); },
        byte_stream::byte_stream_errc::invalid_size);
    expect_byte_stream_error([&] { stream.append(nullptr, 1); },
        byte_stream::byte_stream_errc::null_pointer);
    EXPECT_NO_THROW(stream.append(nullptr, 0));

    byte_stream::stream compact_overflow(std::vector<uint8_t>{ 0x80, 0x02 });
    expect_byte_stream_error([&] { (void)compact_overflow.get_compact<uint8_t>(); },
        byte_stream::byte_stream_errc::invalid_size);
    EXPECT_EQ(compact_overflow.position(), 0U);

    byte_stream::stream compact_unterminated(std::vector<uint8_t>{
        0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80 });
    expect_byte_stream_error([&] { (void)compact_unterminated.get_compact<uint64_t>(); },
        byte_stream::byte_stream_errc::invalid_size);
    EXPECT_EQ(compact_unterminated.position(), 0U);
}

TEST(ByteStreamTest, RejectsNonCanonicalCompactIntegersAndInvalidBooleans) {
    for (const auto& bytes : {
             std::vector<uint8_t>{ 0x80, 0x00 },
             std::vector<uint8_t>{ 0x81, 0x00 } }) {
        byte_stream::stream stream(bytes);
        expect_byte_stream_error([&] { (void)stream.get_compact<uint64_t>(); },
            byte_stream::byte_stream_errc::non_canonical_encoding);
        EXPECT_EQ(stream.position(), 0U);
    }

    byte_stream::stream stream(std::vector<uint8_t>{ 0x02 });
    bool value = true;
    expect_byte_stream_error([&] { stream.get_to(value); },
        byte_stream::byte_stream_errc::invalid_value);
    EXPECT_TRUE(value);
    EXPECT_EQ(stream.position(), 0U);
}

TEST(ByteStreamTest, ReadFailuresKeepPositionAndOutputsStable) {
    byte_stream::stream fixed_width(std::vector<uint8_t>{ 0xAA });
    expect_byte_stream_error([&] { (void)fixed_width.get_atomic<uint16_t>(); },
        byte_stream::byte_stream_errc::insufficient_data);
    EXPECT_EQ(fixed_width.position(), 0U);
    EXPECT_EQ(fixed_width.get<uint8_t>(), 0xAA);

    byte_stream::stream scalar_container(std::vector<uint8_t>{ 0x34 });
    scalar_container.set_endian(byte_stream::endian::little);
    const std::vector<uint16_t> original{ 0xABCD };
    std::vector<uint16_t> decoded = original;

    expect_byte_stream_error([&] { scalar_container.get_to(decoded, 2); },
        byte_stream::byte_stream_errc::insufficient_data);

    EXPECT_EQ(decoded, original);
    EXPECT_EQ(scalar_container.position(), 0U);

    const sample::packet original_packet{
        99, 0x00ABCDEF, sample::kind::beta, { 0x1111, 0x2222 }
    };
    sample::packet decoded_packet = original_packet;
    byte_stream::stream truncated_packet(std::vector<uint8_t>{
        0x07, 0x33, 0x22, 0x11, 0x12, 0x02, 0x02 });
    expect_byte_stream_error([&] { truncated_packet.get_to_atomic(decoded_packet); },
        byte_stream::byte_stream_errc::insufficient_data);
    EXPECT_EQ(decoded_packet, original_packet);
    EXPECT_EQ(truncated_packet.position(), 0U);
}

TEST(ByteStreamTest, WriteFailuresKeepTheExistingBufferUnchanged) {
    byte_stream::stream stream(std::vector<uint8_t>{ 0xAA, 0xBB });
    const auto original = stream.buffer();

    expect_byte_stream_error([&] { stream.set_atomic(codec_sample::failing_write{}); },
        byte_stream::byte_stream_errc::invalid_value);
    EXPECT_EQ(stream.buffer(), original);

    expect_byte_stream_error([&] { stream.set_with_size(codec_sample::oversized_container{}); },
        byte_stream::byte_stream_errc::count_out_of_range);
    EXPECT_EQ(stream.buffer(), original);
}

TEST(ByteStreamTest, RejectsInvalidAliasedAppendRangesAndSizeOverflow) {
    byte_stream::stream stream(std::vector<uint8_t>{ 0x01, 0x02, 0x03 });
    const auto original = stream.buffer();

    expect_byte_stream_error([&] { stream.append(stream.data() + 1, stream.size()); },
        byte_stream::byte_stream_errc::invalid_size);
    EXPECT_EQ(stream.buffer(), original);

    byte_stream::stream cleared;
    cleared.reserve(16);
    const auto* reserved_data = cleared.data();
    expect_byte_stream_error([&] { cleared.append(reserved_data, 1); },
        byte_stream::byte_stream_errc::invalid_size);
    EXPECT_TRUE(cleared.empty());

    const uint8_t byte = 0;
    expect_byte_stream_error(
        [&] { stream.append(&byte, (std::numeric_limits<size_t>::max)()); },
        byte_stream::byte_stream_errc::size_overflow);
    EXPECT_EQ(stream.buffer(), original);
}

TEST(ByteStreamTest, ClearsAndAppendsBuffers) {
    const std::vector<uint8_t> prefix{ 0x01, 0x02 };
    const std::vector<uint8_t> suffix{ 0x03, 0x04 };

    byte_stream::stream stream(prefix);
    stream.reserve(16);
    stream.append(suffix);

    EXPECT_EQ(stream.to_hex(true), "01 02 03 04");
    EXPECT_EQ(stream.data(), stream.buffer().data());

    stream.append(stream);
    EXPECT_EQ(stream.to_hex(true), "01 02 03 04 01 02 03 04");

    stream.set(stream.buffer(), 4);
    EXPECT_EQ(stream.to_hex(true), "01 02 03 04 01 02 03 04 01 02 03 04");

    stream.clear();
    EXPECT_TRUE(stream.empty());
    EXPECT_EQ(stream.position(), 0U);
}

TEST(ByteStreamTest, ConcatenationUsesCompleteBuffersAndPreservesLeftState) {
    byte_stream::stream left(std::vector<uint8_t>{ 0x01, 0x02 });
    byte_stream::stream right(std::vector<uint8_t>{ 0x03, 0x04 });
    left.set_endian(byte_stream::endian::big);
    ASSERT_TRUE(left.seek(1));
    ASSERT_TRUE(right.seek(2));

    const auto combined = left + right;

    EXPECT_EQ(combined.buffer(), (std::vector<uint8_t>{ 0x01, 0x02, 0x03, 0x04 }));
    EXPECT_EQ(combined.position(), 1U);
    EXPECT_EQ(combined.get_endian(), byte_stream::endian::big);
    EXPECT_EQ(right.position(), 2U);

    left += right;
    EXPECT_EQ(left.buffer(), combined.buffer());
    EXPECT_EQ(left.position(), 1U);
    EXPECT_EQ(left.get_endian(), byte_stream::endian::big);

    left += left;
    EXPECT_EQ(left.buffer(),
        (std::vector<uint8_t>{ 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04 }));
    EXPECT_EQ(left.position(), 1U);
}

TEST(ByteStreamTest, ProvidesReadOnlyIndexedAndRangeAccessToCompleteBuffer) {
    byte_stream::stream stream(std::vector<uint8_t>{ 0x10, 0x20, 0x30 });
    ASSERT_TRUE(stream.seek(2));

    static_assert(std::is_same_v<decltype(*stream.begin()), const uint8_t&>,
        "stream iterators must not permit buffer mutation");
    static_assert(std::is_same_v<decltype(stream[0]), const uint8_t&>,
        "indexed access must not permit buffer mutation");

    EXPECT_EQ(stream[0], 0x10);
    EXPECT_EQ(stream.at(2), 0x30);

    std::vector<uint8_t> visited;
    for (uint8_t byte : stream) {
        visited.push_back(byte);
    }
    EXPECT_EQ(visited, stream.buffer());
    EXPECT_EQ(stream.begin(), stream.cbegin());
    EXPECT_EQ(stream.end(), stream.cend());
    EXPECT_EQ(stream.position(), 2U);

    expect_byte_stream_error([&] { (void)stream.at(stream.size()); },
        byte_stream::byte_stream_errc::invalid_size);
    EXPECT_EQ(stream.position(), 2U);

    const byte_stream::stream empty;
    EXPECT_EQ(empty.begin(), empty.end());
}

TEST(ByteStreamTest, TransfersBufferOwnershipWithoutCopying) {
    byte_stream::stream stream;
    stream.set<uint16_t>(0x1234);
    const auto* original_data = stream.data();

    auto bytes = stream.take_buffer();

    EXPECT_EQ(bytes, (std::vector<uint8_t>{ 0x34, 0x12 }));
    EXPECT_EQ(bytes.data(), original_data);
    EXPECT_TRUE(stream.empty());
    EXPECT_EQ(stream.position(), 0U);
    EXPECT_EQ(stream.get_endian(), byte_stream::endian::little);
}

TEST(ByteStreamTest, SeekHandlesEndAndRejectsPastEndWithoutMoving) {
    byte_stream::stream stream(std::vector<uint8_t>{ 0x01, 0x02 });

    EXPECT_TRUE(stream.seek(stream.size()));
    EXPECT_TRUE(stream.eof());
    EXPECT_EQ(stream.position(), 2U);

    EXPECT_FALSE(stream.seek(stream.size() + 1));
    EXPECT_EQ(stream.position(), 2U);

    EXPECT_TRUE(stream.seek(1));
    EXPECT_EQ(stream.get<uint8_t>(), 0x02);
    EXPECT_TRUE(stream.eof());
}

TEST(HexConversion, FormatsCompleteBufferWithoutChangingState) {
    byte_stream::stream stream(std::vector<uint8_t>{0x00, 0x09, 0xab, 0xef, 0xff});
    ASSERT_TRUE(stream.seek(2));
    stream.set_endian(byte_stream::endian::big);
    EXPECT_EQ(stream.to_hex(), "0009abefff");
    EXPECT_EQ(stream.to_hex(true), "00 09 ab ef ff");
    EXPECT_EQ(stream.to_hex(false, true), "0009ABEFFF");
    EXPECT_EQ(stream.to_hex(true, true), "00 09 AB EF FF");
    EXPECT_EQ(stream.position(), 2U);
    EXPECT_EQ(stream.get_endian(), byte_stream::endian::big);
    EXPECT_EQ(byte_stream::stream::to_hex(stream.buffer(), true, true), stream.to_hex(true, true));
    EXPECT_EQ(byte_stream::stream{}.to_hex(), "");
    EXPECT_EQ(byte_stream::stream::to_hex({}, true, true), "");
    EXPECT_EQ(byte_stream::stream::to_hex(std::vector<uint8_t>{0xab}, true), "ab");
}

TEST(HexConversion, AcceptsMixedCaseAndAllAsciiWhitespace) {
    const std::vector<uint8_t> expected{0x00, 0xab, 0xcd, 0xef};
    EXPECT_EQ(byte_stream::stream::from_hex("00aBcDeF"), expected);
    EXPECT_EQ(byte_stream::stream::from_hex(" \t00 aB\ncD\reF\f\v"), expected);
    EXPECT_EQ(byte_stream::stream::from_hex("A B"), (std::vector<uint8_t>{0xab}));
    EXPECT_TRUE(byte_stream::stream::from_hex({}).empty());
    EXPECT_TRUE(byte_stream::stream::from_hex(" \t\n\r\f\v").empty());
    const char text[] = {'x', 'A', 'b', 'y'};
    EXPECT_EQ(byte_stream::stream::from_hex(std::string_view(text + 1, 2)),
        (std::vector<uint8_t>{0xab}));
}

TEST(HexConversion, RejectsMalformedTextWithInvalidValue) {
    for (const auto* text : {"0", " ab c ", "GG", "0x12", "12:34", "12,34", "+1", "-1"}) {
        expect_byte_stream_error([&] { (void)byte_stream::stream::from_hex(text); },
            byte_stream::byte_stream_errc::invalid_value);
    }
    const char nul[] = {'0', '0', '\0', '0'};
    expect_byte_stream_error([&] {
        (void)byte_stream::stream::from_hex(std::string_view(nul, sizeof(nul)));
    }, byte_stream::byte_stream_errc::invalid_value);
    // Exhaust all single-byte characters, including negative char values.
    for (unsigned value = 0; value < 256; ++value) {
        const bool hex = (value >= '0' && value <= '9') ||
            (value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F');
        const bool space = value == ' ' || (value >= '\t' && value <= '\r');
        if (hex || space) continue;
        const std::string text(2, static_cast<char>(value));
        expect_byte_stream_error([&] { (void)byte_stream::stream::from_hex(text); },
            byte_stream::byte_stream_errc::invalid_value);
    }
}

TEST(HexConversion, RoundTripsEveryByteInEveryOutputFormat) {
    std::vector<uint8_t> bytes;
    for (unsigned value = 0; value < 256; ++value) {
        bytes.push_back(static_cast<uint8_t>(value));
    }
    for (bool spaces : {false, true}) {
        for (bool uppercase : {false, true}) {
            const auto text = byte_stream::stream::to_hex(bytes, spaces, uppercase);
            EXPECT_EQ(text.size(), spaces ? 767U : 512U);
            EXPECT_EQ(byte_stream::stream::from_hex(text), bytes);
        }
    }
}
