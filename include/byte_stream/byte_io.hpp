/**
 *   ____        _       ____  _                            
 *  | __ ) _   _| |_ ___/ ___|| |_ _ __ ___  __ _ _ __ ___  
 *  |  _ \| | | | __/ _ \___ \| __| '__/ _ \/ _` | '_ ` _ \ 
 *  | |_) | |_| | ||  __/___) | |_| | |  __/ (_| | | | | | |
 *  |____/ \__, |\__\___|____/ \__|_|  \___|\__,_|_| |_| |_|
 *         |___/                                            
 * https://github.com/yang-chengye/byte_stream
 * Version: 0.1.5
 * License: MIT
 */

#ifndef BYTE_STREAM_BYTE_IO_HPP
#define BYTE_STREAM_BYTE_IO_HPP

#include "byte_stream/byte_stream_types.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#if defined(_MSC_VER)
#include <stdlib.h>
#endif

namespace byte_stream {

/**
 * @brief 表示一段不拥有所有权的只读连续字节视图。
 *
 * 本类型不分配、复制或释放内存。调用者必须保证被引用的存储在视图及所有由该视图
 * 构造的 reader 使用期间保持有效；底层存储发生重分配后，已有视图会失效。
 */
class byte_view {
public:
    /**
     * @brief 构造空视图。
     */
    constexpr byte_view() noexcept = default;

    /**
     * @brief 构造引用指定连续字节区间的视图。
     * @param data 首字节地址；当 size 为 0 时可以为空指针。
     * @param size 可访问的字节数。
     */
    constexpr byte_view(const uint8_t* data, std::size_t size) noexcept
        : data_(data), size_(size) {}

    /**
     * @brief 获取被引用区间的首字节地址。
     * @return 底层只读指针；空视图可能返回空指针。
     */
    [[nodiscard]] constexpr const uint8_t* data() const noexcept { return data_; }

    /**
     * @brief 获取视图包含的字节数。
     * @return 字节数。
     */
    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }

    /**
     * @brief 判断视图是否不包含任何字节。
     * @return size() 为 0 时返回 true。
     */
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

private:
    const uint8_t* data_{ nullptr };
    std::size_t size_{ 0 };
};

namespace io_detail {

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
constexpr endian host_endian() noexcept { return endian::little; }
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
constexpr endian host_endian() noexcept { return endian::big; }
#elif defined(_WIN32)
constexpr endian host_endian() noexcept { return endian::little; }
#else
inline endian host_endian() noexcept {
    const uint16_t value = 1;
    uint8_t first = 0;
    std::memcpy(&first, &value, sizeof(first));
    return first == 1 ? endian::little : endian::big;
}
#endif

inline uint16_t byteswap16(uint16_t value) noexcept {
#if defined(_MSC_VER)
    return _byteswap_ushort(value);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(value);
#else
    return static_cast<uint16_t>((value << 8) | (value >> 8));
#endif
}

inline uint32_t byteswap32(uint32_t value) noexcept {
#if defined(_MSC_VER)
    return _byteswap_ulong(value);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(value);
#else
    return ((value & 0x000000FFu) << 24) |
        ((value & 0x0000FF00u) << 8) |
        ((value & 0x00FF0000u) >> 8) |
        ((value & 0xFF000000u) >> 24);
#endif
}

inline uint64_t byteswap64(uint64_t value) noexcept {
#if defined(_MSC_VER)
    return _byteswap_uint64(value);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(value);
#else
    return ((value & 0x00000000000000FFull) << 56) |
        ((value & 0x000000000000FF00ull) << 40) |
        ((value & 0x0000000000FF0000ull) << 24) |
        ((value & 0x00000000FF000000ull) << 8) |
        ((value & 0x000000FF00000000ull) >> 8) |
        ((value & 0x0000FF0000000000ull) >> 24) |
        ((value & 0x00FF000000000000ull) >> 40) |
        ((value & 0xFF00000000000000ull) >> 56);
#endif
}

template <typename UInt>
UInt byteswap(UInt value) noexcept {
    static_assert(std::is_unsigned<UInt>::value, "byteswap requires an unsigned integer");
    if constexpr (sizeof(UInt) == sizeof(uint16_t)) {
        return static_cast<UInt>(byteswap16(static_cast<uint16_t>(value)));
    }
    else if constexpr (sizeof(UInt) == sizeof(uint32_t)) {
        return static_cast<UInt>(byteswap32(static_cast<uint32_t>(value)));
    }
    else if constexpr (sizeof(UInt) == sizeof(uint64_t)) {
        return static_cast<UInt>(byteswap64(static_cast<uint64_t>(value)));
    }
    else {
        return value;
    }
}

template <typename T, bool = std::is_enum<T>::value>
struct wire_type {
    using type = T;
};

template <typename T>
struct wire_type<T, true> {
    using type = typename std::underlying_type<T>::type;
};

template <typename T>
using wire_type_t = typename wire_type<T>::type;

template <typename T>
struct is_scalar
    : std::integral_constant<bool,
          std::is_integral<T>::value || std::is_enum<T>::value ||
              std::is_floating_point<T>::value || std::is_same<T, std::byte>::value> {};

template <typename T>
uint64_t compact_encode(T value) noexcept {
    using wire = wire_type_t<T>;
    using unsigned_wire = typename std::make_unsigned<wire>::type;
    unsigned_wire raw = static_cast<unsigned_wire>(static_cast<wire>(value));
    if constexpr (std::is_signed<wire>::value) {
        constexpr std::size_t bits = sizeof(wire) * 8;
        raw = static_cast<unsigned_wire>((raw << 1) ^
            (unsigned_wire{ 0 } - (raw >> (bits - 1))));
    }
    return static_cast<uint64_t>(raw);
}

template <typename T>
T compact_decode(uint64_t raw_value) noexcept {
    using wire = wire_type_t<T>;
    using unsigned_wire = typename std::make_unsigned<wire>::type;
    unsigned_wire raw = static_cast<unsigned_wire>(raw_value);
    if constexpr (std::is_signed<wire>::value) {
        raw = static_cast<unsigned_wire>((raw >> 1) ^
            (unsigned_wire{ 0 } - (raw & unsigned_wire{ 1 })));
        wire decoded{};
        std::memcpy(&decoded, &raw, sizeof(decoded));
        return static_cast<T>(decoded);
    }
    else {
        return static_cast<T>(static_cast<wire>(raw));
    }
}

inline void store_uint(
    uint8_t* output, uint64_t value, std::size_t byte_count, endian order) noexcept {
    if (byte_count == 1) {
        output[0] = static_cast<uint8_t>(value);
        return;
    }
    if (byte_count == 2) {
        auto narrowed = static_cast<uint16_t>(value);
        if (order != host_endian()) narrowed = byteswap(narrowed);
        std::memcpy(output, &narrowed, sizeof(narrowed));
        return;
    }
    if (byte_count == 4) {
        auto narrowed = static_cast<uint32_t>(value);
        if (order != host_endian()) narrowed = byteswap(narrowed);
        std::memcpy(output, &narrowed, sizeof(narrowed));
        return;
    }
    if (byte_count == 8) {
        auto narrowed = value;
        if (order != host_endian()) narrowed = byteswap(narrowed);
        std::memcpy(output, &narrowed, sizeof(narrowed));
        return;
    }
    for (std::size_t i = 0; i < byte_count; ++i) {
        const std::size_t shift = order == endian::little ? i * 8 : (byte_count - 1 - i) * 8;
        output[i] = static_cast<uint8_t>((value >> shift) & 0xFFu);
    }
}

inline uint64_t load_uint(const uint8_t* input, std::size_t byte_count, endian order) noexcept {
    if (byte_count == 1) return input[0];
    if (byte_count == 2) {
        uint16_t value = 0;
        std::memcpy(&value, input, sizeof(value));
        return order == host_endian() ? value : byteswap(value);
    }
    if (byte_count == 4) {
        uint32_t value = 0;
        std::memcpy(&value, input, sizeof(value));
        return order == host_endian() ? value : byteswap(value);
    }
    if (byte_count == 8) {
        uint64_t value = 0;
        std::memcpy(&value, input, sizeof(value));
        return order == host_endian() ? value : byteswap(value);
    }
    uint64_t value = 0;
    if (order == endian::little) {
        for (std::size_t i = 0; i < byte_count; ++i) value |= uint64_t{ input[i] } << (i * 8);
    }
    else {
        for (std::size_t i = 0; i < byte_count; ++i) value = (value << 8) | input[i];
    }
    return value;
}

} // namespace io_detail

/**
 * @brief 在调用者持有的定长缓冲区上执行零分配顺序写入。
 *
 * writer 不拥有缓冲区，也不会自动扩容。调用者必须保证缓冲区在 writer 使用期间有效。
 * 写入失败时不会写入部分数据，也不会改变当前位置和已写长度；seek 只能移动到已经写入
 * 的区间内。默认线格式为小端序。
 */
class byte_writer {
public:
    /**
     * @brief 表示使用目标类型自然宽度的参数哨兵值。
     */
    static constexpr uint32_t default_size = (std::numeric_limits<uint32_t>::max)();

    /**
     * @brief 构造一个绑定到外部可写缓冲区的 writer。
     * @param data 外部缓冲区首地址；capacity 非零时必须非空。
     * @param capacity 缓冲区可写容量，单位为字节。
     * @param order 多字节标量使用的线字节序。
     */
    byte_writer(void* data, std::size_t capacity, endian order = endian::little) noexcept
        : data_(static_cast<uint8_t*>(data)), capacity_(capacity), endian_(order) {}

    /**
     * @brief 写入一个标量值。
     * @tparam T 整数、枚举、浮点数或 std::byte 类型。
     * @param value 要写入的值。
     * @param n 整数或枚举的写入字节数；浮点数只能使用自然宽度。
     * @return 成功返回 ok；宽度无效、指针为空或剩余空间不足时返回对应错误码。
     */
    template <typename T>
    [[nodiscard]] byte_stream_errc write(T value, uint32_t n = default_size) noexcept {
        using value_type = typename std::remove_cv<T>::type;
        static_assert(io_detail::is_scalar<value_type>::value,
            "byte_writer::write supports integral, enum, floating-point, and std::byte values");

        if constexpr (std::is_same<value_type, std::byte>::value) {
            if (n != default_size && n != 1) return byte_stream_errc::invalid_size;
            return write_uint(static_cast<uint8_t>(value), 1);
        }
        else if constexpr (std::is_floating_point<value_type>::value) {
            if constexpr (sizeof(value_type) != 4 && sizeof(value_type) != 8) {
                static_cast<void>(value);
                return byte_stream_errc::invalid_size;
            }
            else {
                if (n != default_size && n != sizeof(value_type)) {
                    return byte_stream_errc::invalid_size;
                }
                if constexpr (sizeof(value_type) == 4) {
                    uint32_t bits = 0;
                    std::memcpy(&bits, &value, sizeof(bits));
                    return write_uint(bits, sizeof(bits));
                }
                else {
                    uint64_t bits = 0;
                    std::memcpy(&bits, &value, sizeof(bits));
                    return write_uint(bits, sizeof(bits));
                }
            }
        }
        else {
            using wire = typename io_detail::wire_type<value_type>::type;
            if constexpr (std::is_same<wire, bool>::value) {
                if (n != default_size && n != 1) return byte_stream_errc::invalid_size;
                return write_uint(value ? 1u : 0u, 1);
            }
            else {
                const std::size_t byte_count = n == default_size ? sizeof(wire) : n;
                if (byte_count == 0 || byte_count > sizeof(wire)) {
                    return byte_stream_errc::invalid_size;
                }
                using unsigned_wire = typename std::make_unsigned<wire>::type;
                return write_uint(static_cast<uint64_t>(
                    static_cast<unsigned_wire>(static_cast<wire>(value))), byte_count);
            }
        }
    }

    /**
     * @brief 使用 7 位分组变长格式写入整数或枚举。
     *
     * 有符号整数先经过 zigzag 编码，使绝对值较小的负数保持紧凑。
     * @tparam T 除 bool 外的整数类型或枚举类型。
     * @param value 要写入的值。
     * @return 成功返回 ok；缓冲区无效或空间不足时返回对应错误码。
     */
    template <typename T>
    [[nodiscard]] byte_stream_errc write_compact(T value) noexcept {
        using value_type = typename std::remove_cv<T>::type;
        static_assert((std::is_integral<value_type>::value &&
                          !std::is_same<value_type, bool>::value) ||
                std::is_enum<value_type>::value,
            "byte_writer::write_compact supports only integral and enum values");
        uint64_t encoded = io_detail::compact_encode(value);
        uint8_t bytes[10]{};
        std::size_t count = 0;
        do {
            uint8_t byte = static_cast<uint8_t>(encoded & 0x7Fu);
            encoded >>= 7;
            if (encoded != 0) byte |= 0x80u;
            bytes[count++] = byte;
        } while (encoded != 0);
        return write_bytes(bytes, count);
    }

    /**
     * @brief 将一段原始字节追加到当前位置。
     * @param source 源数据地址；size 为 0 时可以为空指针。
     * @param size 要写入的字节数。
     * @return 成功返回 ok；指针为空或剩余空间不足时返回对应错误码。
     */
    [[nodiscard]] byte_stream_errc write_bytes(const void* source, std::size_t size) noexcept {
        if (size == 0) return byte_stream_errc::ok;
        if (source == nullptr || data_ == nullptr) return byte_stream_errc::null_pointer;
        if (size > remaining()) return byte_stream_errc::insufficient_space;
        std::memmove(data_ + position_, source, size);
        position_ += size;
        if (position_ > size_) size_ = position_;
        return byte_stream_errc::ok;
    }

    /**
     * @brief 将写入位置移动到已写数据范围内的指定偏移。
     * @param position 相对于缓冲区起点的目标偏移。
     * @return 目标不超过当前已写长度时返回 true，否则保持原位置并返回 false。
     */
    [[nodiscard]] bool seek(std::size_t position) noexcept {
        if (position > size_) return false;
        position_ = position;
        return true;
    }

    /**
     * @brief 将写入位置复位到起点，但保留当前已写长度和缓冲区内容。
     */
    void reset_position() noexcept { position_ = 0; }

    /**
     * @brief 同时清除逻辑长度并将写入位置复位到起点。
     *
     * 此操作不会擦除底层内存，也不会改变容量或字节序。
     */
    void clear() noexcept {
        position_ = 0;
        size_ = 0;
    }
    /**
     * @brief 设置后续多字节标量写入使用的线字节序。
     * @param order 新字节序。
     */
    void set_endian(endian order) noexcept { endian_ = order; }

    /**
     * @brief 获取当前线字节序。
     * @return 当前字节序。
     */
    [[nodiscard]] endian get_endian() const noexcept { return endian_; }

    /**
     * @brief 获取外部缓冲区的可写首地址。
     * @return 可写指针；未绑定有效缓冲区时可能为空。
     */
    [[nodiscard]] uint8_t* data() noexcept { return data_; }

    /**
     * @brief 获取外部缓冲区的只读首地址。
     * @return 只读指针；未绑定有效缓冲区时可能为空。
     */
    [[nodiscard]] const uint8_t* data() const noexcept { return data_; }

    /**
     * @brief 获取当前写入位置。
     * @return 相对于缓冲区起点的字节偏移。
     */
    [[nodiscard]] std::size_t position() const noexcept { return position_; }

    /**
     * @brief 获取历史最大写入位置形成的逻辑数据长度。
     * @return 已写数据的字节数。
     */
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    /**
     * @brief 获取外部缓冲区总容量。
     * @return 容量字节数。
     */
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    /**
     * @brief 获取从当前位置到缓冲区末尾的可写空间。
     * @return 剩余可写字节数。
     */
    [[nodiscard]] std::size_t remaining() const noexcept { return capacity_ - position_; }

    /**
     * @brief 获取覆盖全部已写数据的只读视图。
     * @return 引用区间 [data(), data() + size()) 的 byte_view。
     */
    [[nodiscard]] byte_view view() const noexcept { return { data_, size_ }; }

private:
    [[nodiscard]] byte_stream_errc write_uint(uint64_t value, std::size_t byte_count) noexcept {
        if (data_ == nullptr) return byte_stream_errc::null_pointer;
        if (byte_count > remaining()) return byte_stream_errc::insufficient_space;
        io_detail::store_uint(data_ + position_, value, byte_count, endian_);
        position_ += byte_count;
        if (position_ > size_) size_ = position_;
        return byte_stream_errc::ok;
    }

    uint8_t* data_{ nullptr };
    std::size_t capacity_{ 0 };
    std::size_t position_{ 0 };
    std::size_t size_{ 0 };
    endian endian_{ endian::little };
};

/**
 * @brief 在调用者持有的只读缓冲区上执行零分配顺序读取。
 *
 * reader 不拥有也不复制输入数据。调用者必须保证输入存储在 reader 使用期间保持有效。
 * 读取失败时不会改变当前位置或目标值；read_view 返回的视图与输入缓冲区具有相同生命期。
 * 默认按小端线格式解析多字节标量。
 */
class byte_reader {
public:
    /**
     * @brief 表示使用目标类型自然宽度的参数哨兵值。
     */
    static constexpr uint32_t default_size = (std::numeric_limits<uint32_t>::max)();

    /**
     * @brief 构造一个绑定到外部只读缓冲区的 reader。
     * @param data 输入缓冲区首地址；size 为 0 时可以为空指针。
     * @param size 可读取的字节数。
     * @param order 多字节标量使用的线字节序。
     */
    byte_reader(const void* data, std::size_t size, endian order = endian::little) noexcept
        : data_(static_cast<const uint8_t*>(data)), size_(size), endian_(order) {}

    /**
     * @brief 从只读字节视图构造 reader。
     * @param view 输入字节视图；其底层存储必须在 reader 使用期间有效。
     * @param order 多字节标量使用的线字节序。
     */
    explicit byte_reader(byte_view view, endian order = endian::little) noexcept
        : byte_reader(view.data(), view.size(), order) {}

    /**
     * @brief 从当前位置读取一个标量值。
     * @tparam T 整数、枚举、浮点数或 std::byte 类型。
     * @param output 接收解码结果；失败时保持原值。
     * @param n 整数或枚举的读取字节数；浮点数只能使用自然宽度。
     * @return 成功返回 ok；宽度、值、指针或剩余数据无效时返回对应错误码。
     */
    template <typename T>
    [[nodiscard]] byte_stream_errc read(T& output, uint32_t n = default_size) noexcept {
        using value_type = typename std::remove_cv<T>::type;
        static_assert(io_detail::is_scalar<value_type>::value,
            "byte_reader::read supports integral, enum, floating-point, and std::byte values");

        if constexpr (std::is_same<value_type, std::byte>::value) {
            if (n != default_size && n != 1) return byte_stream_errc::invalid_size;
            uint64_t raw = 0;
            const auto status = read_uint(raw, 1);
            if (status == byte_stream_errc::ok) output = static_cast<std::byte>(raw);
            return status;
        }
        else if constexpr (std::is_floating_point<value_type>::value) {
            if constexpr (sizeof(value_type) != 4 && sizeof(value_type) != 8) {
                return byte_stream_errc::invalid_size;
            }
            else {
                if (n != default_size && n != sizeof(value_type)) {
                    return byte_stream_errc::invalid_size;
                }
                uint64_t raw = 0;
                const auto status = read_uint(raw, sizeof(value_type));
                if (status != byte_stream_errc::ok) return status;
                if constexpr (sizeof(value_type) == 4) {
                    const auto bits = static_cast<uint32_t>(raw);
                    std::memcpy(&output, &bits, sizeof(output));
                }
                else {
                    std::memcpy(&output, &raw, sizeof(output));
                }
                return byte_stream_errc::ok;
            }
        }
        else {
            using wire = typename io_detail::wire_type<value_type>::type;
            if constexpr (std::is_same<wire, bool>::value) {
                if (n != default_size && n != 1) return byte_stream_errc::invalid_size;
                uint64_t raw = 0;
                const auto old_position = position_;
                const auto status = read_uint(raw, 1);
                if (status != byte_stream_errc::ok) return status;
                if (raw > 1) {
                    position_ = old_position;
                    return byte_stream_errc::invalid_value;
                }
                output = raw != 0;
                return byte_stream_errc::ok;
            }
            else {
                const std::size_t byte_count = n == default_size ? sizeof(wire) : n;
                if (byte_count == 0 || byte_count > sizeof(wire)) {
                    return byte_stream_errc::invalid_size;
                }
                uint64_t raw = 0;
                const auto status = read_uint(raw, byte_count);
                if (status != byte_stream_errc::ok) return status;
                if constexpr (std::is_signed<wire>::value) {
                    const std::size_t bit_count = byte_count * 8;
                    if (bit_count < 64 && (raw & (uint64_t{ 1 } << (bit_count - 1))) != 0) {
                        raw |= (~uint64_t{ 0 }) << bit_count;
                    }
                    using unsigned_wire = typename std::make_unsigned<wire>::type;
                    const auto bits = static_cast<unsigned_wire>(raw);
                    wire decoded{};
                    std::memcpy(&decoded, &bits, sizeof(decoded));
                    output = static_cast<value_type>(decoded);
                }
                else {
                    output = static_cast<value_type>(static_cast<wire>(raw));
                }
                return byte_stream_errc::ok;
            }
        }
    }

    /**
     * @brief 读取一个采用 7 位分组变长格式编码的整数或枚举。
     * @tparam T 除 bool 外的整数类型或枚举类型。
     * @param output 接收解码结果；失败时保持原值。
     * @return 成功返回 ok；数据截断、超宽或编码不规范时返回对应错误码。
     */
    template <typename T>
    [[nodiscard]] byte_stream_errc read_compact(T& output) noexcept {
        using value_type = typename std::remove_cv<T>::type;
        static_assert((std::is_integral<value_type>::value &&
                          !std::is_same<value_type, bool>::value) ||
                std::is_enum<value_type>::value,
            "byte_reader::read_compact supports only integral and enum values");
        using wire = typename io_detail::wire_type<value_type>::type;
        uint64_t raw = 0;
        const auto status = read_compact_uint(raw, sizeof(wire) * 8);
        if (status == byte_stream_errc::ok) output = io_detail::compact_decode<value_type>(raw);
        return status;
    }

    /**
     * @brief 从当前位置复制指定数量的原始字节。
     * @param destination 目标缓冲区；size 为 0 时可以为空指针。
     * @param size 要读取的字节数。
     * @return 成功返回 ok；指针为空或剩余数据不足时返回对应错误码。
     */
    [[nodiscard]] byte_stream_errc read_bytes(void* destination, std::size_t size) noexcept {
        if (size == 0) return byte_stream_errc::ok;
        if (destination == nullptr || data_ == nullptr) return byte_stream_errc::null_pointer;
        if (size > remaining()) return byte_stream_errc::insufficient_data;
        std::memmove(destination, data_ + position_, size);
        position_ += size;
        return byte_stream_errc::ok;
    }

    /**
     * @brief 从当前位置取得一段不复制数据的只读视图。
     * @param size 视图需要包含的字节数。
     * @param output 接收结果视图；失败时保持原值。
     * @return 成功返回 ok；输入指针为空或剩余数据不足时返回对应错误码。
     */
    [[nodiscard]] byte_stream_errc read_view(std::size_t size, byte_view& output) noexcept {
        if (size > remaining()) return byte_stream_errc::insufficient_data;
        if (size != 0 && data_ == nullptr) return byte_stream_errc::null_pointer;
        output = byte_view(data_ == nullptr ? nullptr : data_ + position_, size);
        position_ += size;
        return byte_stream_errc::ok;
    }

    /**
     * @brief 将读取位置移动到输入范围内的指定偏移。
     * @param position 相对于输入起点的目标偏移。
     * @return 目标不超过输入长度时返回 true，否则保持原位置并返回 false。
     */
    [[nodiscard]] bool seek(std::size_t position) noexcept {
        if (position > size_) return false;
        position_ = position;
        return true;
    }

    /**
     * @brief 将读取位置复位到输入起点。
     */
    void reset() noexcept { position_ = 0; }

    /**
     * @brief 设置后续多字节标量读取使用的线字节序。
     * @param order 新字节序。
     */
    void set_endian(endian order) noexcept { endian_ = order; }

    /**
     * @brief 获取当前线字节序。
     * @return 当前字节序。
     */
    [[nodiscard]] endian get_endian() const noexcept { return endian_; }

    /**
     * @brief 获取输入缓冲区首地址。
     * @return 只读指针；空输入可能返回空指针。
     */
    [[nodiscard]] const uint8_t* data() const noexcept { return data_; }

    /**
     * @brief 获取输入总长度。
     * @return 输入字节数。
     */
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    /**
     * @brief 获取当前读取位置。
     * @return 相对于输入起点的字节偏移。
     */
    [[nodiscard]] std::size_t position() const noexcept { return position_; }

    /**
     * @brief 获取尚未读取的字节数。
     * @return 从当前位置到输入末尾的字节数。
     */
    [[nodiscard]] std::size_t remaining() const noexcept { return size_ - position_; }

    /**
     * @brief 判断输入是否不包含任何字节。
     * @return 输入长度为 0 时返回 true。
     */
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    /**
     * @brief 判断读取位置是否已到达输入末尾。
     * @return position() 等于 size() 时返回 true。
     */
    [[nodiscard]] bool eof() const noexcept { return position_ == size_; }

private:
    [[nodiscard]] byte_stream_errc read_uint(uint64_t& output, std::size_t byte_count) noexcept {
        if (data_ == nullptr) return byte_stream_errc::null_pointer;
        if (byte_count > remaining()) return byte_stream_errc::insufficient_data;
        output = io_detail::load_uint(data_ + position_, byte_count, endian_);
        position_ += byte_count;
        return byte_stream_errc::ok;
    }

    [[nodiscard]] byte_stream_errc read_compact_uint(
        uint64_t& output, std::size_t max_bits) noexcept {
        if (data_ == nullptr) return byte_stream_errc::null_pointer;
        uint64_t value = 0;
        std::size_t shift = 0;
        std::size_t cursor = position_;
        for (std::size_t i = 0; i < 10; ++i) {
            if (cursor >= size_) return byte_stream_errc::insufficient_data;
            const uint8_t byte = data_[cursor++];
            const uint64_t payload = byte & 0x7Fu;
            if (shift == 63 && payload > 1) return byte_stream_errc::invalid_size;
            value |= payload << shift;
            if ((byte & 0x80u) == 0) {
                if (i != 0 && payload == 0) return byte_stream_errc::non_canonical_encoding;
                if (max_bits < 64 && (value >> max_bits) != 0) {
                    return byte_stream_errc::invalid_size;
                }
                position_ = cursor;
                output = value;
                return byte_stream_errc::ok;
            }
            shift += 7;
        }
        return byte_stream_errc::invalid_size;
    }

    const uint8_t* data_{ nullptr };
    std::size_t size_{ 0 };
    std::size_t position_{ 0 };
    endian endian_{ endian::little };
};

} // namespace byte_stream

#endif // BYTE_STREAM_BYTE_IO_HPP
